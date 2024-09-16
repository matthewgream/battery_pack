
/*
    https://www.waveshare.com/esp32-c3-zero.htm
    https://www.waveshare.com/wiki/ESP32-C3-Zero
    https://github.com/mikedotalmond/Arduino-MuxInterface-CD74HC4067
    https://github.com/ugurakas/Esp32-C3-LP-Project
    https://github.com/ClaudeMarais/ContinousAnalogRead_ESP32-C3
    https://docs.espressif.com/projects/esp-idf/en/release-v4.4/esp32/api-reference/peripherals/adc.html
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
protected:  
    ~Component () {};
public:
    typedef std::vector <Component*> List;
    virtual void begin () {}
    virtual void process () {}
};

// -----------------------------------------------------------------------------------------------

class Diagnosticable {
protected:  
    ~Diagnosticable () {};
public:
    typedef std::vector <Diagnosticable*> List; 
    virtual void collect (JsonObject &obj) const = 0;
};

class DiagnosticManager : public Component {
    const Config::DiagnosticConfig& config;
    Diagnosticable::List _diagnosticables;
public:
    DiagnosticManager (const Config::DiagnosticConfig& cfg, const Diagnosticable::List diagnosticables) : config (cfg), _diagnosticables (diagnosticables) {}
    JsonDocument assemble () {
        JsonDocument doc;
        JsonObject obj = doc.to <JsonObject> ();
        for (const auto& diagnosticable : _diagnosticables)
            diagnosticable->collect (obj);
        return doc;
    }
};

// -----------------------------------------------------------------------------------------------

typedef uint32_t AlarmSet;
#define _ALARM_NUMB(x)               (1UL < (x))
#define ALARM_NONE                  (0UL)
#define ALARM_TEMPERATURE_MINIMAL   _ALARM_NUMB (0)
#define ALARM_TEMPERATURE_MAXIMAL   _ALARM_NUMB (1)
#define ALARM_STORAGE_FAIL          _ALARM_NUMB (2)
#define ALARM_STORAGE_SIZE          _ALARM_NUMB (3)
#define ALARM_TIME_DRIFT            _ALARM_NUMB (4)
#define ALARM_TIME_NETWORK          _ALARM_NUMB (5)
#define ALARM_COUNT                 (6)
#define ALARM_ACTIVE(a,x)           ((a) & _ALARM_NUMB (x))
static const char * _ALARM_NAME [] = { "TEMP_MIN", "TEMP_MAX", "STORE_FAIL", "STORE_SIZE", "TIME_DRIFT", "TIME_NETWORK" };
#define ALARM_NAME(x)               (_ALARM_NAME [x])

class Alarmable {
protected:  
    ~Alarmable () {};
public:
    typedef std::vector <Alarmable*> List; 
    virtual AlarmSet alarm () const = 0;
};

class AlarmManager : public Component, public Diagnosticable {
    const Config::AlarmConfig& config;
    Alarmable::List _alarmables;
    AlarmSet _alarms = ALARM_NONE;
    std::array <Upstamp, ALARM_COUNT> _counts;
public:
    AlarmManager (const Config::AlarmConfig& cfg, const Alarmable::List alarmables) : config (cfg), _alarmables (alarmables) {}
    void begin () override {
        pinMode (config.PIN_ALARM, OUTPUT);
    }
    void process () override {
        AlarmSet alarms = ALARM_NONE;
        for (const auto& alarmable : _alarmables)
            alarms |= alarmable->alarm ();
        if (alarms != _alarms) {
            for (int i = 0; i < ALARM_COUNT; i ++)
                if (ALARM_ACTIVE (alarms, i) && !ALARM_ACTIVE (_alarms, i))
                    _counts [i] ++;
            digitalWrite (config.PIN_ALARM, (alarms != ALARM_NONE) ? HIGH : LOW);
            _alarms = alarms;
        }
    }
    void collect (JsonObject &obj) const override {
        JsonObject alarm = obj ["alarm"].to <JsonObject> ();      
        JsonArray name = alarm ["name"].to <JsonArray> (), active = alarm ["active"].to <JsonArray> (), numb = alarm ["numb"].to <JsonArray> (), time = alarm ["time"].to <JsonArray> ();
        for (int i = 0; i < ALARM_COUNT; i ++)
            name.add (ALARM_NAME (i)), active.add (ALARM_ACTIVE (_alarms, i)), time.add (_counts [i].seconds ()), numb.add (_counts [i].number ());
    }
    //
    AlarmSet getAlarms () const { return _alarms; }
};

// -----------------------------------------------------------------------------------------------

class TemperatureInterface : public Component, public Diagnosticable {
    static constexpr int ADC_RESOLUTION = 12, ADC_MINVALUE = 0, ADC_MAXVALUE = ((1 << ADC_RESOLUTION) - 1);
    const Config::TemperatureInterfaceConfig& config;
    MuxInterface_CD74HC4067 _muxInterface;
    std::array <uint16_t, MuxInterface_CD74HC4067::CHANNELS> _muxValues, _muxValuesMin, _muxValuesMax;
public:
    TemperatureInterface (const Config::TemperatureInterfaceConfig& cfg) : config (cfg), _muxInterface (cfg.mux) {
        for (int i = 0; i < MuxInterface_CD74HC4067::CHANNELS; i ++)
            _muxValues [i] = ADC_MINVALUE, _muxValuesMin [i] = ADC_MAXVALUE, _muxValuesMax [i] = ADC_MINVALUE;
    }
    void begin () override {
        analogReadResolution (ADC_RESOLUTION);
        _muxInterface.configure ();
    }
    void collect (JsonObject &obj) const override {
        JsonObject temperature = obj ["temperature"].to <JsonObject> ();
        JsonObject values = temperature ["values"].to <JsonObject> ();
        JsonArray valuesNow = values ["now"].to <JsonArray> (), valuesMin = values ["min"].to <JsonArray> (), valuesMax = values ["max"].to <JsonArray> ();
        for (int i = 0; i < MuxInterface_CD74HC4067::CHANNELS; i ++)
            valuesNow.add (_muxValues [i]), valuesMin.add (_muxValuesMin [i]), valuesMax.add (_muxValuesMax [i]);
    }
    //
    float get (const int channel) const {
        assert (channel >= 0 && channel < MuxInterface_CD74HC4067::CHANNELS && "Channel out of range");
        uint16_t value = _muxInterface.get (channel);
        const_cast <TemperatureInterface*> (this)->updatevalues (channel, value);
        return steinharthart_calculator (value, ADC_MAXVALUE, config.thermister.REFERENCE_RESISTANCE, config.thermister.NOMINAL_RESISTANCE, config.thermister.NOMINAL_TEMPERATURE);
    }
private:
    void updatevalues (const int channel, const uint16_t value) {
        _muxValues [channel] = value;
        if (value > _muxValuesMin [channel]) _muxValuesMin [channel] = value;
        if (value < _muxValuesMax [channel]) _muxValuesMax [channel] = value;
    } 
};

// -----------------------------------------------------------------------------------------------

class FanInterface : public Component, public Diagnosticable {
    static constexpr int PWM_RESOLUTION = 8, PWM_MINVALUE = 0, PWM_MAXVALUE = ((1 << PWM_RESOLUTION) - 1);
    const Config::FanInterfaceConfig& config;
    int _speed = PWM_MINVALUE, _speedMin = PWM_MAXVALUE, _speedMax = PWM_MINVALUE;
    Upstamp _last;
public:
    FanInterface (const Config::FanInterfaceConfig& cfg) : config (cfg) {}
    void begin () override {
        pinMode (config.PIN_PWM, OUTPUT);
        analogWrite (config.PIN_PWM, PWM_MINVALUE);
    }
    void collect (JsonObject &obj) const override {
        JsonObject fan = obj ["fan"].to <JsonObject> ();
        JsonObject values = fan ["values"].to <JsonObject> ();
        values ["now"] = _speed;
        values ["min"] = _speedMin;
        values ["max"] = _speedMax;
        fan ["last-run"] = _last.seconds ();
        fan ["numb-run"] = _last.number ();
        // % duty
    }
    //  
    void setSpeed (const int speed) {
        int speedNew = std::clamp (speed, PWM_MINVALUE, PWM_MAXVALUE);
        if (speedNew > config.MIN_SPEED && _speed <= config.MIN_SPEED) _last ++;
        if (speedNew < _speedMin) _speedMin = speedNew;
        if (speedNew > _speedMax) _speedMax = speedNew;
        analogWrite (config.PIN_PWM, _speed = speedNew);
    }
    int getSpeed () const {
        return _speed;
    }
};

// -----------------------------------------------------------------------------------------------

static Upstamp __ConnectManager_WiFiEvents_connected;
static Upstamp __ConnectManager_WiFiEvents_disconnected;
static void __ConnectManager_WiFiEvents (WiFiEvent_t event) {
    if (event == ARDUINO_EVENT_WIFI_STA_CONNECTED)
        __ConnectManager_WiFiEvents_connected ++;
    else if (event == ARDUINO_EVENT_WIFI_STA_DISCONNECTED)
        __ConnectManager_WiFiEvents_disconnected ++;
}

class ConnectManager : public Component, public Diagnosticable  {
    const Config::ConnectConfig& config;
public:
    ConnectManager (const Config::ConnectConfig& cfg) : config (cfg) {}
    void begin () override {
        WiFi.onEvent (__ConnectManager_WiFiEvents);
        WiFi.setHostname (config.host.c_str ());
        WiFi.setAutoReconnect (true);
        WiFi.mode (WIFI_STA);
        WiFi.begin (config.ssid.c_str (), config.pass.c_str ());
    }
    void collect (JsonObject &obj) const override {
        JsonObject wifi = obj ["wifi"].to <JsonObject> ();
        wifi ["mac"] = mac_address ();
        if ((wifi ["connected"] = WiFi.isConnected ())) {
            wifi ["address"] = WiFi.localIP ();
            wifi ["rssi"] = WiFi.RSSI ();
        }
        wifi ["last-connect"] = __ConnectManager_WiFiEvents_connected.seconds ();
        wifi ["numb-connect"] = __ConnectManager_WiFiEvents_connected.number ();
        wifi ["last-disconnect"] = __ConnectManager_WiFiEvents_disconnected.seconds ();
        wifi ["numb-disconnect"] = __ConnectManager_WiFiEvents_disconnected.number ();
    }
};

// -----------------------------------------------------------------------------------------------

RTC_DATA_ATTR long _persistentDriftMs = 0;
RTC_DATA_ATTR struct timeval _persistentTime = { .tv_sec = 0, .tv_usec = 0 };

class NettimeManager : public Component, public Alarmable, public Diagnosticable {
    const Config::NettimeConfig& config;
    NetworkTimeFetcher _fetcher;
    TimeDriftCalculator _drifter;
    unsigned long _previousTimeUpdate = 0, _previousTimeAdjust = 0;
    time_t _previousTime = 0;
    Upstamp _last;
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
                  _last ++;
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
    void collect (JsonObject &obj) const override {
        JsonObject clock = obj ["clock"].to <JsonObject> ();
        clock ["time"] = getTimeString ();
        clock ["drift"] = _drifter.drift ();
        clock ["last"] = _last.seconds ();
        clock ["numb"] = _last.number ();
    }
    //
    AlarmSet alarm () const override {
        AlarmSet alarms = ALARM_NONE;
        if (_fetcher.failures () > config.failureLimit) alarms |= ALARM_TIME_NETWORK;
        if (_drifter.highDrift ()) alarms |= ALARM_TIME_DRIFT;
        return alarms;
    }
    String getTimeString () const {
        struct tm timeinfo;
        char timeString [sizeof ("yyyy-mm-ddThh:mm:ssZ") + 1] = { '\0' };
        if (getLocalTime (&timeinfo))
            strftime (timeString, sizeof (timeString), "%Y-%m-%dT%H:%M:%SZ", &timeinfo);
        return String (timeString);
    }
};

// -----------------------------------------------------------------------------------------------

class DeliverManager : public Component, public Diagnosticable {
    const Config::DeliverConfig& config;
    BluetoothNotifier _bluetooth;
    Upstamp _last;
public:
    DeliverManager (const Config::DeliverConfig& cfg) : config (cfg) {}
    void begin () override {
        _bluetooth.advertise (config.name, config.service, config.characteristic);
    }
    void collect (JsonObject &obj) const override {
        JsonObject blue = obj ["blue"].to <JsonObject> ();
        if ((blue ["connected"] = _bluetooth.connected ())) {
            blue ["address"] = _bluetooth.address ();          
            blue ["devices"] = _bluetooth.devices ();          
        }
        blue ["last"] = _last.seconds ();
        blue ["numb"] = _last.number ();
    }
    void deliver (const String& data) {
        if (_bluetooth.connected ()) {
            _last ++;
            _bluetooth.notify (data);
        }
    }
};

// -----------------------------------------------------------------------------------------------

class PublishManager : public Component, public Diagnosticable {
    const Config::PublishConfig& config;
    MQTTPublisher _mqtt;
    Upstamp _last;
public:
    PublishManager (const Config::PublishConfig& cfg) : config (cfg), _mqtt (cfg.mqtt) {}
    void begin () override {
        _mqtt.connect ();
    }
    void process () override {
        _mqtt.process ();
    }
    bool publish (const String& data) {
        if (!_mqtt.connected () || !_mqtt.publish (config.mqtt.topic, data))
            return false;
        _last ++;
        return true;
    }

    void collect (JsonObject &obj) const override {
        JsonObject mqtt = obj ["mqtt"].to <JsonObject> ();      
        if ((mqtt ["connected"] = const_cast <MQTTPublisher *> (&_mqtt)->connected ())) {
        }
        mqtt ["state"] = const_cast <MQTTPublisher *> (&_mqtt)->state ();
        mqtt ["last"] = _last.seconds ();
        mqtt ["numb"] = _last.number ();
    }
    //
    bool connected () { return _mqtt.connected (); }
};

// -----------------------------------------------------------------------------------------------

class StorageManager : public Component, public Alarmable, public Diagnosticable {
    const Config::StorageConfig& config;
    SPIFFSFile _file;
    unsigned long _failures = 0;
    unsigned long _erasures = 0;
    Upstamp _last;
public:
    typedef SPIFFSFile::LineCallback LineCallback; 
    StorageManager (const Config::StorageConfig& cfg) : config (cfg), _file (config.filename, config.lengthMaximum) {}
    void begin () override {
        _failures = _file.begin () ? 0 : _failures + 1;
    }
    void collect (JsonObject &obj) const override {
        JsonObject storage = obj ["storage"].to <JsonObject> ();
        JsonObject size = storage ["size"].to <JsonObject> ();
        size ["current"] = _file.size ();
        size ["maximum"] = config.lengthMaximum;
        size ["critical"] = config.lengthCritical;
        size ["erasures"] = _erasures;
        storage ["last"] = _last.seconds ();
        storage ["numb"] = _last.number ();
    }
    //
    AlarmSet alarm () const override { 
        AlarmSet alarms = ALARM_NONE;
        if (_failures > config.failureLimit) alarms |= ALARM_STORAGE_FAIL;
        if (_file.size () > config.lengthCritical) alarms |= ALARM_STORAGE_SIZE;
        return alarms;
    }     
    size_t size () const { 
        return _file.size ();
    }
    void append (const String& data) {
        if (!_file.append (data))
            _failures ++;
        else {
            _failures = 0;
            _last ++;
        }
    }
    bool retrieve (LineCallback& callback) const {
        return _file.read (callback);
    }
    void erase () {
        _erasures ++; 
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
    std::array <MovingAverage <float>, _num> _filters;
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
      if (_min <= config.MINIMAL) alarms |= ALARM_TEMPERATURE_MINIMAL;
      if (_max >= config.CRITICAL) alarms |= ALARM_TEMPERATURE_MAXIMAL;
      return alarms;
    }     
    float min () const { return _min; }
    float max () const { return _max; }
    float avg () const { return _avg; }
    const Values& getTemperatures () const { return _values; }
    float setpoint () const { return (config.WARNING + config.CRITICAL) / 2.0f; }
    float current () const { return _max; }
};

class TemperatureManagerEnvironment : public TemperatureManager, public Alarmable {
    float _value;
    MovingAverage <float> _filter;
public:    
    TemperatureManagerEnvironment (const Config::TemperatureInterfaceConfig& cfg, TemperatureInterface& temperature) : TemperatureManager (cfg, temperature) {};
    void process () override {
        _value = _filter.update (_temperature.get (config.PROBE_ENVIRONMENT));
    }
    AlarmSet alarm () const override { 
      AlarmSet alarms = ALARM_NONE;
      if (_value <= config.MINIMAL) alarms |= ALARM_TEMPERATURE_MINIMAL;
      if (_value >= config.CRITICAL) alarms |= ALARM_TEMPERATURE_MAXIMAL;
      return alarms;
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
        const float speedCalculated = _controllerAlgorithm.apply (setpoint, current);
        const int speedSmoothed = _smootherAlgorithm.apply (std::clamp ((int) map  <float> (speedCalculated, -100, 100, config.MIN_SPEED, config.MAX_SPEED), config.MIN_SPEED, config.MAX_SPEED));
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

    Uptime uptime;
    Upstamp iterations;

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
                if (storage.retrieve (handler))
                    storage.erase ();
            }
            if (!publish.publish (data))
              storage.append (data);
        } else
            storage.append (data);
    }

    //

    void diagnose () {
        String diag;
        if (serializeJson (diagnostics.assemble (), diag) && !diag.isEmpty ())
            dataDeliver (diag);
    }    
    void collect (JsonObject &obj) const override {
        JsonObject program = obj ["program"].to <JsonObject> ();
        program ["uptime"] = uptime.seconds ();      
        program ["iterations"] = iterations.number ();
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
        iterations ++;            
    }

public:
    Program () :
        fanControllingAlgorithm (10.0f, 0.1f, 1.0f), fanSmoothingAlgorithm (0.1f), 
        temperatureInterface (config.temperature), temperatureManagerBatterypack (config.temperature, temperatureInterface), temperatureManagerEnvironment (config.temperature, temperatureInterface),
        fanInterface (config.fan), fanManager (config.fan, fanInterface, temperatureManagerBatterypack, fanControllingAlgorithm, fanSmoothingAlgorithm),
        network (config.network), nettime (config.nettime),
        deliver (config.deliver), publish (config.publish), storage (config.storage),
        alarms (config.alarm, { &temperatureManagerEnvironment, &temperatureManagerBatterypack, &storage, &nettime }),
        diagnostics (config.diagnostic, { &temperatureInterface, &fanInterface, &network, &nettime, &deliver, &publish, &storage, &alarms, this }),
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
