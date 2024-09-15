
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

#include <Arduino.h>
#include <WiFi.h>
#include <SPIFFS.h>
#include <HTTPClient.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

#include <PubSubClient.h>
#include <ArduinoJson.h>

#include <ctime>
#include <array>
#include <vector>

#include "Debug.hpp"
#include "Utility.hpp"
#include "Config.hpp"

// -----------------------------------------------------------------------------------------------

class Component {
public:
    virtual ~Component () {}
    virtual void begin () {}
    virtual void process () {}
};

class ComponentManager {
protected:  
    std::vector <Component*> components;
public:
    virtual ~ComponentManager () {}
    virtual void setup () {
        for (const auto& component : components) component->begin ();
    }
    virtual void loop () {
        for (const auto& component : components) component->process ();
    }
};

// -----------------------------------------------------------------------------------------------

typedef enum {
  ALARM_NONE                  = 0,
  ALARM_TEMPERATURE_MAXIMAL   = 1 << 0,
  ALARM_TEMPERATURE_MINIMAL   = 1 << 1,
  ALARM_STORAGE_FAIL          = 1 << 2,
  ALARM_STORAGE_SIZE          = 1 << 3,
  ALARM_NETTIME               = 1 << 4,
} AlarmSet;

class Alarmable {
public:
    virtual ~Alarmable () {}
    virtual AlarmSet alarm () const = 0;
};

class AlarmManager : public Component {
    const Config::AlarmConfig& config;
    std::vector <Alarmable*> _alarmables;
    AlarmSet _alarms = ALARM_NONE;
public:
    AlarmManager (const Config::AlarmConfig& cfg, const std::vector <Alarmable*> alarmables): config (cfg), _alarmables (alarmables) {}
    void begin () override {
        pinMode (config.PIN_ALARM, OUTPUT);
    }
    void process () override {
        AlarmSet alarms = ALARM_NONE;
        for (const auto& alarmable : _alarmables)
            alarms = (AlarmSet) ((int) alarms | (int) alarmable->alarm ());
        if (alarms != _alarms)
            digitalWrite (config.PIN_ALARM, ((_alarms = alarms) != ALARM_NONE) ? HIGH : LOW);
    }
    //
    AlarmSet getAlarms () const { return _alarms; }
};

// -----------------------------------------------------------------------------------------------

class MuxInterface : public Component {
    const Config::TemperatureInterfaceConfig::MuxInterfaceConfig& config;
public:
    MuxInterface (const Config::TemperatureInterfaceConfig::MuxInterfaceConfig& cfg) : config (cfg) {}
    void begin () override {
        pinMode (config.PIN_S0, OUTPUT);
        pinMode (config.PIN_S1, OUTPUT);
        pinMode (config.PIN_S2, OUTPUT);
        pinMode (config.PIN_S3, OUTPUT);
        pinMode (config.PIN_SIG, INPUT);
    }
    //
    uint16_t get (int channel) const {
        digitalWrite (config.PIN_S0, channel & 1);
        digitalWrite (config.PIN_S1, (channel >> 1) & 1);
        digitalWrite (config.PIN_S2, (channel >> 2) & 1);
        digitalWrite (config.PIN_S3, (channel >> 3) & 1);
        delay (10);
        return analogRead (config.PIN_SIG);
    }
};
class TemperatureInterface : public Component {
    const Config::TemperatureInterfaceConfig& config;
    MuxInterface _muxInterface;
public:
    TemperatureInterface (const Config::TemperatureInterfaceConfig& cfg) : config (cfg), _muxInterface (cfg.mux) {}
    void begin () override {
        analogReadResolution (12);
        _muxInterface.begin ();
    }
    //
    float get (int channel) const {
        return calculateTemp (_muxInterface.get (channel));
    }
private:
    float calculateTemp (uint16_t value) const {
        float steinhart = (log ((config.REFERENCE_RESISTANCE / ((4095.0 / value) - 1.0)) / config.NOMINAL_RESISTANCE) / config.BETA) + (1.0 / (config.NOMINAL_TEMPERATURE + 273.15));
        return (1.0 / steinhart) - 273.15;
    }
};

// -----------------------------------------------------------------------------------------------

class FanInterface : public Component {
    const Config::FanInterfaceConfig& config;
    int _speed = 0;
public:
    FanInterface (const Config::FanInterfaceConfig& cfg) : config (cfg) {}
    void begin () override {
        pinMode (config.PIN_PWM, OUTPUT);
        analogWrite (config.PIN_PWM, 0);
    }
    //  
    void setSpeed (int speed) {
        analogWrite (config.PIN_PWM, _speed = speed);
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
        WiFi.persistent (false);
        WiFi.setHostname (config.host.c_str ());
        WiFi.setAutoReconnect (true);
        WiFi.mode (WIFI_STA);
        WiFi.begin (config.ssid.c_str (), config.pass.c_str ());      
    }
};

// -----------------------------------------------------------------------------------------------

class NetworkTimeFetcher {
    const String _server; 
public:
    NetworkTimeFetcher (const String& server) : _server (server) {}
    time_t fetch () {
        HTTPClient client;
        client.begin (_server);
        if (client.GET () > 0) {
            String header = client.header ("Date");
            client.end ();
            if (!header.isEmpty ()) {
                struct tm timeinfo;
                if (strptime (header.c_str (), "%a, %d %b %Y %H:%M:%S GMT", &timeinfo) != NULL)
                    return mktime (&timeinfo);
            }
        }
        client.end ();
        return (time_t) 0;
    }
};

RTC_DATA_ATTR long _persistentDriftMs = 0;

class TimeDriftCalculator {
    long _driftMs;
    static constexpr long MAX_DRIFT_MS = 60000;
public:
    TimeDriftCalculator () : _driftMs (_persistentDriftMs) {}
    void updateDrift (time_t periodSecs, unsigned long periodMs) {
        long driftMs = (((periodSecs * 1000) - periodMs) * (60 * 60 * 1000)) / periodMs; // ms per hour
        _persistentDriftMs = _driftMs = std::clamp ((_driftMs * 3 + driftMs) / 4, -MAX_DRIFT_MS, MAX_DRIFT_MS); // 75% old value, 25% new value
    }    
    long applyDrift (struct timeval &currentTime, unsigned long periodMs) {
        long adjustMs = (_driftMs * periodMs) / (60 * 60 * 1000);
        currentTime.tv_sec += adjustMs / 1000;
        currentTime.tv_usec += (adjustMs % 1000) * 1000;
        if (currentTime.tv_usec >= 1000000) {
            currentTime.tv_sec += currentTime.tv_usec / 1000000;
            currentTime.tv_usec %= 1000000;
        } else if (currentTime.tv_usec < 0) {
            currentTime.tv_sec -= 1 + (-currentTime.tv_usec / 1000000);
            currentTime.tv_usec = 1000000 - (-currentTime.tv_usec % 1000000);
        }
        return adjustMs;
    }
};

RTC_DATA_ATTR struct timeval _persistentTime = { .tv_sec = 0, .tv_usec = 0 };

class NettimeManager : public Component, public Alarmable {
    const Config::NettimeConfig& config;
    NetworkTimeFetcher _fetcher;
    TimeDriftCalculator _drifter;
    unsigned long _previousTimeUpdate = 0, _previousTimeAdjust = 0;
    time_t _previousTime = 0;
    int _failures = 0;
public:
    NettimeManager (const Config::NettimeConfig& cfg) : config (cfg), _fetcher (cfg.server) {
      if (_persistentTime.tv_sec > 0)
          settimeofday (&_persistentTime, nullptr);
    }
    void process () override {
        unsigned long currentTime = millis ();
        if (WiFi.isConnected ()) {
          if (currentTime - _previousTimeUpdate >= config.intervalUpdate) {
              time_t fetchedTime = _fetcher.fetch ();
              if (fetchedTime > 0) {
                  _persistentTime.tv_sec = fetchedTime; _persistentTime.tv_usec = 0;
                  settimeofday (&_persistentTime, nullptr);
                  if (_previousTime > 0)
                      _drifter.updateDrift (fetchedTime - _previousTime, currentTime - _previousTimeUpdate);
                  _previousTime = fetchedTime;
                  _previousTimeUpdate = currentTime;
                  _failures = 0;
              } else _failures ++;
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
    AlarmSet alarm () const override { return (_failures > config.failureLimit) ? ALARM_NETTIME : ALARM_NONE; }
    String getTimeString () {
        struct tm timeinfo;
        char timeString [20] = { '\0' };
        if (getLocalTime (&timeinfo))
            strftime (timeString, sizeof (timeString), "%Y-%m-%d %H:%M:%S", &timeinfo);
        return String (timeString);
    }
};

// -----------------------------------------------------------------------------------------------

class BluetoothNotifier : protected BLEServerCallbacks {
    BLEServer *_server = nullptr;
    BLECharacteristic *_characteristic = nullptr;
    bool _connected = false;
protected:
    void onConnect (BLEServer*) override { _connected = true; }
    void onDisconnect (BLEServer*) override { _connected = false; }
public:
    void advertise (const String& name, const String& serviceUUID, const String& characteristicUUID)  {
        BLEDevice::init (name);
        _server = BLEDevice::createServer ();
        _server->setCallbacks (this);
        BLEService *service = _server->createService (serviceUUID);
        _characteristic = service->createCharacteristic (characteristicUUID, BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY);
        _characteristic->addDescriptor (new BLE2902 ());
        service->start ();
        BLEAdvertising *advertising = BLEDevice::getAdvertising ();
        advertising->addServiceUUID (serviceUUID);
        advertising->setScanResponse (true);
        advertising->setMinPreferred (0x06);
        advertising->setMinPreferred (0x12);
        BLEDevice::startAdvertising ();
    }
    void notify (const String& data) {
        _characteristic->setValue (data.c_str ());
        _characteristic->notify ();
    }
    bool connected (void) const { return _connected; }
};

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

class MQTTPublisher {
    const String _client, _host;
    const uint16_t _port;
    const String _user, _pass;
    WiFiClient _wifiClient;
    PubSubClient _mqttClient;
public:
    MQTTPublisher (const String& client, const String& host, const uint16_t port, const String& user, const String& pass) : _client (client), _host (host), _port (port), _user (user), _pass (pass), _mqttClient (_wifiClient) {}
    void connect () {
        _mqttClient.setServer (_host.c_str (), _port);
    }
    bool publish (const String& topic, const String& data) {
      if (!connected ())
          return false;
      _mqttClient.loop ();
      return _mqttClient.publish (topic.c_str (), data.c_str ());
    }
    bool connected () {
        if (!WiFi.isConnected ())
            return false;
        if (!_mqttClient.connected ())
            _mqttClient.connect (_client.c_str (), _user.c_str (), _pass.c_str ());
        return _mqttClient.connected ();
    }
};

class PublishManager : public Component {
    const Config::PublishConfig& config;
    MQTTPublisher _mqtt;
public:
    PublishManager (const Config::PublishConfig& cfg) : config (cfg), _mqtt (config.client, config.host, config.port, config.user, config.pass) {}
    void begin () override {
        _mqtt.connect ();
    }
    bool publish (const String& data) {
        return _mqtt.connected () ? _mqtt.publish (config.topic, data) : false;
    }
    //
    bool connected () { return _mqtt.connected (); }
};

// -----------------------------------------------------------------------------------------------

class StorageManager : public Component, public Alarmable {
    const Config::StorageConfig& config;
    size_t _size = 0;
    int _failures = 0;
public:
    StorageManager (const Config::StorageConfig& cfg) : config (cfg) {}
    void begin () override {
        if (!SPIFFS.begin (true)) {
            DEBUG_PRINTLN ("Error mounting SPIFFS");
            _failures ++;
        } else {
            File file = SPIFFS.open (config.filename, FILE_READ);
            if (file) {
                _size = file.size ();
                file.close ();
            }
            _failures = 0;
        }
    }
    //
    AlarmSet alarm () const override { 
      return (AlarmSet) ((int) ((_size >= config.lengthCritical) ? ALARM_STORAGE_SIZE : ALARM_NONE) | (int) (_failures > config.failureLimit ? ALARM_STORAGE_FAIL : ALARM_NONE)); 
    }     
    size_t size () const { return _size; }
    void writeData (const String& data) {
        if (_size + data.length () > config.lengthMaximum)
            clearData ();
        File file = SPIFFS.open (config.filename, FILE_APPEND);
        if (file) {
            file.println (data);
            _size = file.size ();
            file.close ();
            _failures = 0;
        } else _failures ++;
    }
    class LineCallback {
    public:
        virtual bool process (const String& line) = 0;
    };
    bool readData (LineCallback& callback) {
        File file = SPIFFS.open (config.filename, FILE_READ);
        if (file) {
            while (file.available ()) {
                if (!callback.process (file.readStringUntil ('\n'))) {
                    file.close ();
                    return false;
                }
            }
            file.close ();
        }
        return true;
    }
    void clearData () {
        SPIFFS.remove (config.filename);
        _size = 0;
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
      return (AlarmSet) ((int) (_max >= config.CRITICAL ? ALARM_TEMPERATURE_MAXIMAL : ALARM_NONE) | (int) (_min <= config.MINIMAL ? ALARM_TEMPERATURE_MINIMAL : ALARM_NONE));
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
        float setpoint = _temperatures.setpoint (), current = _temperatures.current ();
        float value = _controllerAlgorithm.process (setpoint, current);
        int speed = _smootherAlgorithm.process (std::clamp ((int) map<float> (value, -100, 100, config.MIN_SPEED, config.MAX_SPEED), config.MIN_SPEED, config.MAX_SPEED));
        _fan.setSpeed (speed);
    }
};

// -----------------------------------------------------------------------------------------------

class Program : public Component, public ComponentManager {
    const Config& config;

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

    unsigned long previousDeliver = 0, previousCapture = 0;

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
        class StorageLineProcessor: public StorageManager::LineCallback {
            PublishManager& _publish;
        public:
            StorageLineProcessor (PublishManager& publish): _publish (publish) {}
            bool process (const String& line) override {
                return line.isEmpty () ? true : _publish.publish (line);
            }
        };
        if (publish.connected ()) {
            if (storage.size () > 0) {
                StorageLineProcessor processor (publish);
                if (storage.readData (processor))
                    storage.clearData ();
            }
            if (!publish.publish (data))
              storage.writeData (data);
        } else
            storage.writeData (data);
    }

    void process () {
        String data = dataCollect ();
        unsigned long current = millis ();
        if (current - previousDeliver > config.intervalDeliver) {
            previousDeliver = current;
            dataDeliver (data);
        }
        if (current - previousCapture > config.intervalCapture) {
            previousCapture = current;
            dataCapture (data);
        }
        delay (config.intervalProcess);
    }

public:
    Program (const Config& cfg) : config (cfg),
          fanControllingAlgorithm (10.0f, 0.1f, 1.0f), fanSmoothingAlgorithm (0.1f), 
          temperatureInterface (config.temperature), temperatureManagerBatterypack (config.temperature, temperatureInterface), temperatureManagerEnvironment (config.temperature, temperatureInterface),
          fanInterface (config.fan), fanManager (config.fan, fanInterface, temperatureManagerBatterypack, fanControllingAlgorithm, fanSmoothingAlgorithm),
          network (config.network), nettime (config.nettime),
          deliver (config.deliver), publish (config.publish), storage (config.storage),
          alarms (config.alarm, { &temperatureManagerBatterypack, &storage, &nettime }) {
        components.push_back (&temperatureInterface);
        components.push_back (&fanInterface);
        components.push_back (&temperatureManagerBatterypack);
        components.push_back (&temperatureManagerEnvironment);
        components.push_back (&fanManager);
        components.push_back (&alarms);
        components.push_back (&network);
        components.push_back (&nettime);
        components.push_back (&deliver);
        components.push_back (&publish);
        components.push_back (&storage);
        components.push_back (this);
    };
};

// -----------------------------------------------------------------------------------------------

static const Config config;
static Program program (config);

void setup () {
    DEBUG_START ();
    program.setup ();
}

void loop () {
    program.loop ();
}

// -----------------------------------------------------------------------------------------------
