
/*
  https://www.waveshare.com/esp32-c3-zero.htm
  https://www.waveshare.com/wiki/ESP32-C3-Zero

  initialise CD74HC4067
    https://github.com/mikedotalmond/Arduino-MuxInterface-CD74HC4067
  initialise ADC
    https://github.com/ugurakas/Esp32-C3-LP-Project
    https://github.com/ClaudeMarais/ContinousAnalogRead_ESP32-C3
    https://docs.espressif.com/projects/esp-idf/en/release-v4.4/esp32/api-reference/peripherals/adc.html
  initialise PWM
    https://github.com/khoih-prog/ESP32_FastPWM
*/

// -----------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------

#include <Arduino.h>
#include <WiFi.h>

#include <ArduinoJson.h>

#include <ctime>
#include <array>
#include <vector>

#include "Debug.hpp"
#include "Utility.hpp"
#include "Helpers.hpp"
#include "Config.hpp"

// -----------------------------------------------------------------------------------------------

class Component {
public:
    typedef std::vector <Component*> List;
    virtual ~Component () = default;
    virtual void begin () {}
    virtual void process () {}
};

// -----------------------------------------------------------------------------------------------

class Diagnosticable {
public:
    typedef std::vector <Diagnosticable*> List; 
    virtual ~Diagnosticable () = default;
    virtual void collect (JsonDocument &doc) = 0;
};

class DiagnosticManager : public Component {
    const Config::DiagnosticConfig& config;
    Diagnosticable::List _diagnosticables;
public:
    DiagnosticManager (const Config::DiagnosticConfig& cfg, const Diagnosticable::List diagnosticables) : config (cfg), _diagnosticables (diagnosticables) {}
    JsonDocument assemble () {
        JsonDocument doc;
        for (const auto& diagnosticable : _diagnosticables)
            diagnosticable->collect (doc);
        return doc;
    }
};

// -----------------------------------------------------------------------------------------------

typedef uint32_t AlarmSet;
#define ALARM_NONE                  (0UL)
#define ALARM_TEMPERATURE_MAXIMAL   (1UL << 0)
#define ALARM_TEMPERATURE_MINIMAL   (1UL << 1)
#define ALARM_STORAGE_FAIL          (1UL << 2)
#define ALARM_STORAGE_SIZE          (1UL << 3)
#define ALARM_TIME_DRIFT            (1UL << 4)
#define ALARM_TIME_NETWORK          (1UL << 5)

class Alarmable {
public:
    typedef std::vector <Alarmable*> List; 
    virtual ~Alarmable () = default;
    virtual AlarmSet alarm () const = 0;
};

class AlarmManager : public Component {
    const Config::AlarmConfig& config;
    Alarmable::List _alarmables;
    AlarmSet _alarms = ALARM_NONE;
public:
    AlarmManager (const Config::AlarmConfig& cfg, const Alarmable::List alarmables) : config (cfg), _alarmables (alarmables) {}
    void begin () override {
        pinMode (config.PIN_ALARM, OUTPUT);
    }
    void process () override {
        AlarmSet alarms = ALARM_NONE;
        for (const auto& alarmable : _alarmables)
            alarms |= alarmable->alarm ();
        if (alarms != _alarms)
            digitalWrite (config.PIN_ALARM, ((_alarms = alarms) == ALARM_NONE) ? LOW : HIGH);
    }
    //
    AlarmSet getAlarms () const { return _alarms; }
};

// -----------------------------------------------------------------------------------------------

class TemperatureInterface : public Component {
    const Config::TemperatureInterfaceConfig& config;
    MuxInterface_CD74HC4067 _muxInterface;
    static constexpr int ADC_RESOLUTION = 12, ADC_MINVALUE = 0, ADC_MAXVALUE = ((1 << ADC_RESOLUTION) - 1);
public:
    TemperatureInterface (const Config::TemperatureInterfaceConfig& cfg) : config (cfg), _muxInterface (cfg.mux) {}
    void begin () override {
        analogReadResolution (ADC_RESOLUTION);
        _muxInterface.configure ();
    }
    //
    float get (const int channel) const {
        return calculateTemp (_muxInterface.get (channel));
    }
private:
    float calculateTemp (const uint16_t value) const {
        static constexpr float BETA = 3950.0;
        const float steinhart = (log ((config.thermister.REFERENCE_RESISTANCE / (((float) ADC_MAXVALUE / (float) value) - 1.0)) / config.thermister.NOMINAL_RESISTANCE) / BETA) + (1.0 / (config.thermister.NOMINAL_TEMPERATURE + 273.15));
        return (1.0 / steinhart) - 273.15;
    }
};

// -----------------------------------------------------------------------------------------------

class FanInterface : public Component {
    const Config::FanInterfaceConfig& config;
    int _speed = 0;
    static constexpr int PWM_RESOLUTION = 8, PWM_MINVALUE = 0, PWM_MAXVALUE = ((1 << PWM_RESOLUTION) - 1);
public:
    FanInterface (const Config::FanInterfaceConfig& cfg) : config (cfg) {}
    void begin () override {
        pinMode (config.PIN_PWM, OUTPUT);
        analogWrite (config.PIN_PWM, 0);
    }
    //  
    void setSpeed (const int speed) {
        analogWrite (config.PIN_PWM, _speed = std::clamp (speed, PWM_MINVALUE, PWM_MAXVALUE));
    }
    int getSpeed () const {
        return _speed;
    }
};

// -----------------------------------------------------------------------------------------------

class ConnectManager : public Component {
    const Config::ConnectConfig& config;
public:
    ConnectManager (const Config::ConnectConfig& cfg) : config (cfg) {}
    void begin () override {
        WiFi.setHostname (config.host.c_str ());
        WiFi.setAutoReconnect (true);
        WiFi.mode (WIFI_STA);
        WiFi.begin (config.ssid.c_str (), config.pass.c_str ());      
    }
};

// -----------------------------------------------------------------------------------------------

RTC_DATA_ATTR long _persistentDriftMs = 0;
RTC_DATA_ATTR struct timeval _persistentTime = { .tv_sec = 0, .tv_usec = 0 };

class NettimeManager : public Component, public Alarmable {
    const Config::NettimeConfig& config;
    NetworkTimeFetcher _fetcher;
    TimeDriftCalculator _drifter;
    unsigned long _previousTimeUpdate = 0, _previousTimeAdjust = 0;
    time_t _previousTime = 0;
public:
    NettimeManager (const Config::NettimeConfig& cfg) : config (cfg), _fetcher (cfg.server), _drifter (_persistentDriftMs) {
      if (_persistentTime.tv_sec > 0)
          settimeofday (&_persistentTime, nullptr);
    }
    void process () override {
        unsigned long currentTime = millis ();
        if (WiFi.isConnected ()) {
          if (currentTime - _previousTimeUpdate >= config.intervalUpdate) {
              const time_t fetchedTime = _fetcher.fetch ();
              if (fetchedTime > 0) {
                  _persistentTime.tv_sec = fetchedTime; _persistentTime.tv_usec = 0;
                  settimeofday (&_persistentTime, nullptr);
                  if (_previousTime > 0)
                      _persistentDriftMs = _drifter.updateDrift (fetchedTime - _previousTime, currentTime - _previousTimeUpdate);
                  _previousTime = fetchedTime;
                  _previousTimeUpdate = currentTime;
              }
          }
        }
        if (currentTime - _previousTimeAdjust >= config.intervalAdjust) {
            gettimeofday (&_persistentTime, nullptr);
            if (_drifter.applyDrift (_persistentTime, currentTime - _previousTimeAdjust) > 0)
                settimeofday (&_persistentTime, nullptr);
            _previousTimeAdjust = currentTime;
        }
    }
    //
    AlarmSet alarm () const override {
      AlarmSet alarms = ALARM_NONE;
      if (_drifter.highDrift ()) alarms |= ALARM_TIME_DRIFT;
      if (_fetcher.failures () > config.failureLimit) alarms |= ALARM_TIME_NETWORK;
      return alarms;
    }
    String getTimeString () {
        struct tm timeinfo;
        char timeString [20] = { '\0' };
        if (getLocalTime (&timeinfo))
            strftime (timeString, sizeof (timeString), "%Y-%m-%d %H:%M:%S", &timeinfo);
        return String (timeString);
    }
};

// -----------------------------------------------------------------------------------------------

class DeliverManager : public Component {
    const Config::DeliverConfig& config;
    BluetoothNotifier _bluetooth;
public:
    DeliverManager (const Config::DeliverConfig& cfg) : config (cfg) {}
    void begin () override {
        _bluetooth.advertise (config.name, config.service, config.characteristic);
    }
    void deliver (const String& data) {
        if (_bluetooth.connected ())
            _bluetooth.notify (data);
    }
};

// -----------------------------------------------------------------------------------------------

class PublishManager : public Component {
    const Config::PublishConfig& config;
    MQTTPublisher _mqtt;
public:
    PublishManager (const Config::PublishConfig& cfg) : config (cfg), _mqtt (cfg.mqtt) {}
    void begin () override {
        _mqtt.connect ();
    }
    bool publish (const String& data) {
        return _mqtt.connected () ? _mqtt.publish (config.mqtt.topic, data) : false;
    }
    //
    bool connected () { return _mqtt.connected (); }
};

// -----------------------------------------------------------------------------------------------

class StorageManager : public Component, public Alarmable {
    const Config::StorageConfig& config;
    SPIFFSFile _file;
    int _failures = 0;
public:
    // XXX last write
    // XXX capacity used
    typedef SPIFFSFile::LineCallback LineCallback; 
    StorageManager (const Config::StorageConfig& cfg) : config (cfg), _file (config.filename, config.lengthMaximum) {}
    void begin () override {
        _failures = _file.begin () ? 0 : _failures + 1;
    }
    //
    AlarmSet alarm () const override { 
      AlarmSet alarms = ALARM_NONE;
      if (_file.size () >= config.lengthCritical) alarms |= ALARM_STORAGE_SIZE;
      if (_failures > config.failureLimit) alarms |= ALARM_STORAGE_FAIL;
      return alarms;
    }     
    size_t size () const { 
        return _file.size ();
    }
    void writeData (const String& data) {
        _failures = _file.append (data) ? 0 : _failures + 1;
    }
    bool readData (LineCallback& callback) const {
        return _file.read (callback);
    }
    void clearData () { 
        _file.erase ();
    }
};

// -----------------------------------------------------------------------------------------------

class TemperatureManager: public Component {
protected: 
    const Config::TemperatureInterfaceConfig& config;
    TemperatureInterface& _temperature;
public:    
    TemperatureManager (const Config::TemperatureInterfaceConfig& cfg, TemperatureInterface& temperature) : config (cfg), _temperature (temperature) {};
};

class TemperatureManagerBatterypack: public TemperatureManager, public Alarmable {
    float _min, _max, _avg;
    static constexpr int _num = 15; // config.temperature.PROBE_NUMBER - 1
    typedef std::array <float, _num> Values;
    Values _values; 
    std::array <MovingAverage, _num> _filters;
public:    
    TemperatureManagerBatterypack (const Config::TemperatureInterfaceConfig& cfg, TemperatureInterface& temperature) : TemperatureManager (cfg, temperature) {};
    void process () override {
        float sum = 0.0;
        _min = 1000.0;
        _max = -1000.0;
        int cnt = 0;
        for (int num = 0; num < config.PROBE_NUMBER; num ++) {
            if (num != config.PROBE_ENVIRONMENT) {
                float tmp = _filters [cnt].update (_temperature.get (num));
                _values [cnt ++] = tmp;
                sum += tmp;
                if (tmp < _min) _min = tmp;
                if (tmp > _max) _max = tmp;
            }
        }
        _avg = sum / (1.0 * cnt);
    }
    //
    AlarmSet alarm () const override { 
      AlarmSet alarms = ALARM_NONE;
      if (_max >= config.CRITICAL) alarms |= ALARM_TEMPERATURE_MAXIMAL;
      if (_min <= config.MINIMAL) alarms |= ALARM_TEMPERATURE_MINIMAL;
      return alarms;
    }     
    float min () const { return _min; }
    float max () const { return _max; }
    float avg () const { return _avg; }
    const Values& getTemperatures () const { return _values; }
    float setpoint () const { return (config.WARNING + config.CRITICAL) / 2.0f; }
    float current () const { return _max; }
};

class TemperatureManagerEnvironment : public TemperatureManager {
    float _value;
    MovingAverage _filter;
public:    
    TemperatureManagerEnvironment (const Config::TemperatureInterfaceConfig& cfg, TemperatureInterface& temperature) : TemperatureManager (cfg, temperature) {};
    void process () override {
        _value = _filter.update (_temperature.get (config.PROBE_ENVIRONMENT));
    }
    //
    float getTemperature () const { return _value; }
};

// -----------------------------------------------------------------------------------------------

class FanManager : public Component {
    const Config::FanInterfaceConfig &config;
    FanInterface &_fan;
    const TemperatureManagerBatterypack& _temperatures;
    PidController &_controllerAlgorithm;
    AlphaSmoothing &_smootherAlgorithm;

public:    
    FanManager (const Config::FanInterfaceConfig& cfg, FanInterface& fan, const TemperatureManagerBatterypack& temperatures, PidController& controller, AlphaSmoothing& smoother) : config (cfg), _fan (fan), _temperatures (temperatures), _controllerAlgorithm (controller), _smootherAlgorithm (smoother) {}
    void process () override {
        const float setpoint = _temperatures.setpoint (), current = _temperatures.current ();
        const float speedCalculated = _controllerAlgorithm.process (setpoint, current);
        const int speedSmoothed = _smootherAlgorithm.process (std::clamp ((int) map<float> (speedCalculated, -100, 100, config.MIN_SPEED, config.MAX_SPEED), config.MIN_SPEED, config.MAX_SPEED));
        _fan.setSpeed (speedSmoothed);
    }
};

// -----------------------------------------------------------------------------------------------

class Program : public Component, public Diagnosticable {
    const Config config;

    PidController fanControllingAlgorithm;
    AlphaSmoothing fanSmoothingAlgorithm;

    TemperatureInterface temperatureInterface;
    TemperatureManagerBatterypack temperatureManagerBatterypack;
    TemperatureManagerEnvironment temperatureManagerEnvironment;
    FanInterface fanInterface;
    FanManager fanManager;

    ConnectManager network;
    NettimeManager nettime;
    DeliverManager deliver;
    PublishManager publish;
    StorageManager storage;

    AlarmManager alarms;
    DiagnosticManager diagnostics;

    //

    String dataCollect () {
        JsonDocument doc;

        doc ["time"] = nettime.getTimeString ();

        JsonObject temperatures = doc ["temperatures"].to <JsonObject> ();
        temperatures ["environment"] = temperatureManagerEnvironment.getTemperature ();
        JsonObject batterypack = temperatures ["batterypack"].to <JsonObject> ();
        batterypack ["avg"] = temperatureManagerBatterypack.avg ();
        batterypack ["min"] = temperatureManagerBatterypack.min ();
        batterypack ["max"] = temperatureManagerBatterypack.max ();
        JsonArray values = batterypack ["values"].to <JsonArray> ();
        for (const auto& temperature : temperatureManagerBatterypack.getTemperatures ())
            values.add (temperature);

        JsonObject fan = doc ["fan"].to <JsonObject> ();
        fan ["speed"] = fanInterface.getSpeed ();

        doc ["alarms"] = alarms.getAlarms ();

        String output;
        serializeJson (doc, output);
        return output;
    }

    void dataDeliver (const String& data) {
        deliver.deliver (data);
    }
    void dataCapture (const String& data) {
        class StorageLineHandler: public StorageManager::LineCallback {
            PublishManager& _publish;
        public:
            StorageLineHandler (PublishManager& publish): _publish (publish) {}
            bool process (const String& line) override {
                return line.isEmpty () ? true : _publish.publish (line);
            }
        };
        if (publish.connected ()) {
            if (storage.size () > 0) {
                StorageLineHandler handler (publish);
                if (storage.readData (handler))
                    storage.clearData ();
            }
            if (!publish.publish (data))
              storage.writeData (data);
        } else
            storage.writeData (data);
    }

    //

    void diagnose () {
        String diag;
        if (serializeJson (diagnostics.assemble (), diag) && !diag.isEmpty ())
            dataDeliver (diag);
    }    

    //

    void collect (JsonDocument &doc) override {
        // XXX
    }

    //

    Intervalable intervalDeliver, intervalCapture, intervalDiagnose;
    void process () override {
        const String data = dataCollect ();
        if (intervalDeliver)
            dataDeliver (data);
        if (intervalCapture)
            dataCapture (data);
        if (intervalDiagnose)
            diagnose ();
    }

public:
    Program () :
        fanControllingAlgorithm (10.0f, 0.1f, 1.0f), fanSmoothingAlgorithm (0.1f), 
        temperatureInterface (config.temperature), temperatureManagerBatterypack (config.temperature, temperatureInterface), temperatureManagerEnvironment (config.temperature, temperatureInterface),
        fanInterface (config.fan), fanManager (config.fan, fanInterface, temperatureManagerBatterypack, fanControllingAlgorithm, fanSmoothingAlgorithm),
        network (config.network), nettime (config.nettime),
        deliver (config.deliver), publish (config.publish), storage (config.storage),
        alarms (config.alarm, { &temperatureManagerBatterypack, &storage, &nettime }),
        diagnostics (config.diagnostic, { this }),
        intervalDeliver (config.intervalDeliver), intervalCapture (config.intervalCapture), intervalDiagnose (config.intervalDiagnose),
        components ({ &temperatureInterface, &fanInterface, &temperatureManagerBatterypack, &temperatureManagerEnvironment, &fanManager, 
            &alarms, &network, &nettime, &deliver, &publish, &storage, &diagnostics, this }) {
    };

    Component::List components;
    void setup () { for (const auto& component : components) component->begin (); }
    void loop () { for (const auto& component : components) component->process (); }
    void sleep () { delay (config.intervalProcess); }
};

// -----------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------
