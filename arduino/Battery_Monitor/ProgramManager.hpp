
// -----------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------

class DeviceManager: public Component, public Diagnosticable {
public:
    typedef struct {
        BluetoothDevice::Config blue;
        MQTTPublisher::Config mqtt;
    } Config;

    using BooleanFunc = std::function <bool ()>;

private:
    const Config &config;
    const BooleanFunc _networkIsAvailable;

    BluetoothDevice _blue;
    MQTTPublisher _mqtt;

public:
    explicit DeviceManager (const Config& cfg, const BooleanFunc networkIsAvailable): config (cfg), _networkIsAvailable (networkIsAvailable), _blue (config.blue), _mqtt (config.mqtt) {}
    void begin () override {
        _blue.begin ();
        _mqtt.setup ();
    }
    void process () override {
        _blue.process ();
        if (_networkIsAvailable ())
            _mqtt.process ();
    }

    BluetoothDevice& blue () { return _blue; }
    MQTTPublisher& mqtt () { return _mqtt; }

protected:
    void collectDiagnostics (JsonVariant &obj) const override {
        JsonObject sub = obj ["devices"].to <JsonObject> ();
            sub ["blue"] = _blue;
            sub ["mqtt"] = _mqtt;
    }
};

// -----------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------

#ifdef DEBUG
#include <mutex>
#endif

class LoggingHandler: private Singleton <LoggingHandler> {
public:
    typedef struct {
        bool enableSerial, enableMqtt;
        String mqttTopic;
    } Config;

#ifdef DEBUG
private:
    const Config& config;

    bool _enableSerial = false, _enableMqtt = false;
    __DebugLoggerFunc __debugLoggerPrevious = nullptr;
    char _mqttTopic [64+1]; // arbitrary
    MQTTPublisher *_mqttClient;

public:
    explicit LoggingHandler (const Config& cfg, MQTTPublisher *mqttClient = nullptr): Singleton <LoggingHandler> (this), config (cfg), _mqttClient (mqttClient) { init (); }
    ~LoggingHandler () { term (); }

protected:
    void init () {
        if (config.enableMqtt && _mqttClient != nullptr) {
            snprintf (_mqttTopic, sizeof (_mqttTopic) - 1, "%s/logs", config.mqttTopic.c_str ());
            __debugLoggerPrevious = __debugLoggerSet (__debugLoggerMQTT);
            if (config.enableSerial) _enableSerial = true;
            _enableMqtt = true;
            DEBUG_PRINTF ("LoggingHandler::init: logging directed to Serial and MQTT (as topic '%s' when online)\n", _mqttTopic);
        } else if (config.enableSerial) {
            __debugLoggerPrevious = __debugLoggerSet (__debugLoggerSerial);
            _enableSerial = true;
            DEBUG_PRINTF ("LoggingHandler::init: logging directed to Serial\n");
        } else {
            DEBUG_PRINTF ("LoggingHandler::init: logging not directed\n");
        }
    }
    void term () { // not entirely thread safe
      __debugLoggerSet (__debugLoggerPrevious); __debugLoggerPrevious = nullptr;
      _enableSerial = false;
      _enableMqtt = false;
      _mqttTopic [0] = '\0';
      _mqttClient = nullptr;
      DEBUG_PRINTF ("LoggingHandler::term: logging reverted to previous\n");
    }

private:
    static std::mutex _bufferMutex;
    static int constexpr _bufferLength = DEFAULT_DEBUG_LOGGING_BUFFER;
    static char _bufferContent [_bufferLength];
    static int _bufferOffset;

    static void __debugLoggerMQTT (const char* format, ...) {

        auto logging = Singleton <LoggingHandler>::instance ();
        if (!logging)
            return;

        std::lock_guard <std::mutex> guard (_bufferMutex);

        va_list args;
        va_start (args, format);
        int printed = vsnprintf (_bufferContent + _bufferOffset, (_bufferLength - _bufferOffset), format, args);
        va_end (args);
        if (printed < 0)
            return;

        _bufferOffset = (printed >= (_bufferLength - _bufferOffset)) ? (_bufferLength - 1) : (_bufferOffset + printed);
        if (_bufferOffset == (_bufferLength - 1) || (_bufferOffset > 0 && _bufferContent [_bufferOffset - 1] == '\n')) {
            while (_bufferOffset > 0 && _bufferContent [_bufferOffset - 1] == '\n')
                _bufferContent [-- _bufferOffset] = '\0';
            _bufferContent [_bufferOffset] = '\0';
            _bufferOffset = 0;

            if (logging->_enableSerial)
                Serial.println (_bufferContent);

            if (logging->_enableMqtt && _bufferContent [0] != '\0') {
#ifdef DEFAULT_SCRUB_SENSITIVE_CONTENT_FROM_NETWORK_LOGGING
                for (char *location = _bufferContent; (location = strstr (location, "pass=")) != NULL; )
                    for (location += sizeof ("pass=") - 1; *location != '\0' && *location != ',' && *location != ' '; )
                        *location ++ = '*';
                for (char *location = _bufferContent; (location = strstr (location, "pin=")) != NULL; )
                    for (location += sizeof ("pin=") - 1; *location != '\0' && *location != ',' && *location != ' '; )
                        *location ++ = '*';
#endif
                logging->_mqttClient->publish__native (logging->_mqttTopic, _bufferContent);
            }
        }
    }
#else
    explicit LoggingHandler (const Config&, MQTTPublisher *): Singleton <LoggingHandler> (this) {}
#endif
};

#ifdef DEBUG
std::mutex LoggingHandler::_bufferMutex;
char LoggingHandler::_bufferContent [_bufferLength];
int LoggingHandler::_bufferOffset = 0;
#endif

// -----------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------

class ControlManager: public Component, public Diagnosticable {
public:
    typedef struct {
        LoggingHandler::Config logging;
    } Config;

private:
    const Config &config;

    DeviceManager& _devices;
    LoggingHandler _logging;

    class BluetoothWriteHandler_TypeCtrl: public BluetoothWriteHandler_TypeSpecific {
    public:
        BluetoothWriteHandler_TypeCtrl (): BluetoothWriteHandler_TypeSpecific ("ctrl") {};
        bool process (BluetoothDevice& device, const String& time, const String& ctrl, JsonDocument& doc) override {
            DEBUG_PRINTF ("BluetoothWriteHandler_TypeCtrl:: type=ctrl, time=%s, ctrl='%s'\n", time.c_str (), ctrl.c_str ());
            // XXX
            // request controllables
            // process controllables
                // force reboot
                // turn on/off diag/logging
                // wipe spiffs data file
                // disable/enable wifi
                // change wifi user/pass
                // suppress specific alarms
            return true;
        }
    };
    class BluetoothReadHandler_TypeCtrl: public BluetoothReadHandler_TypeSpecific {
    public:
        BluetoothReadHandler_TypeCtrl (): BluetoothReadHandler_TypeSpecific ("ctrl") {};
        bool process (BluetoothDevice& device, const String& time, const String& ctrl, JsonDocument& doc) override {
            DEBUG_PRINTF ("BluetoothReadHandler_TypeCtrl:: type=ctrl\n");
            // XXX
            // send controllables
            return false;
        }
    };

public:
    explicit ControlManager (const Config& cfg, DeviceManager& devices): config (cfg), _devices (devices), _logging (config.logging, &_devices.mqtt ()) {
        _devices.blue ().insert ({ { String ("ctrl"), std::make_shared <BluetoothWriteHandler_TypeCtrl> () } });
        _devices.blue ().insert ({ { std::make_shared <BluetoothReadHandler_TypeCtrl> () } });
    }

protected:
    void collectDiagnostics (JsonVariant &) const override {
    }
};

// -----------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------

class IntervalableByPersistentTime {
    interval_t _interval;
    PersistentValue <uint32_t>& _previous;

public:
    explicit IntervalableByPersistentTime (const interval_t interval, PersistentValue <uint32_t>& previous) : _interval (interval), _previous (previous) {}
    operator bool () {
        const uint32_t timet = static_cast <uint32_t> (time (NULL));
        if (timet > 0 && ((timet - _previous) > (_interval / 1000))) {
            _previous = timet;
            return true;
        }
        return false;
    }
    interval_t interval () const {
        const uint32_t timet = static_cast <uint32_t> (time (NULL));
        return timet > 0 && _previous > 0 ? (timet - _previous) * 1000 : 0;
    }
};

#include <functional>

extern String ota_image_check (const String& json, const String& type, const String& vers, const String& addr);

class UpdateManager: public Component, public Alarmable, public Diagnosticable {
public:
    typedef struct {
        interval_t intervalCheck, intervalLong;
        String json, type, vers, addr;
    } Config;

    using BooleanFunc = std::function <bool ()>;

private:
    const Config& config;
    const BooleanFunc _networkIsAvailable;

    PersistentData _persistent_data;
    PersistentValue <uint32_t> _persistent_data_previous;
    PersistentValue <String> _persistent_data_version;
    IntervalableByPersistentTime _interval;
    bool _available;

public:
    UpdateManager (const Config& cfg, const BooleanFunc networkIsAvailable): Alarmable ({
            AlarmCondition (ALARM_UPDATE_VERS, [this] () { return _available; }),
            AlarmCondition (ALARM_UPDATE_LONG, [this] () { return istoolong (); }),
        }), config (cfg), _networkIsAvailable (networkIsAvailable),
        _persistent_data ("updates"), _persistent_data_previous (_persistent_data, "previous", 0), _persistent_data_version (_persistent_data, "version", String ()),
        _interval (config.intervalCheck, _persistent_data_previous), _available (!static_cast <String> (_persistent_data_version).isEmpty ()) {}
    void process () override {
        if (_networkIsAvailable () && _interval) {
            const String version = ota_image_check (config.json, config.type, config.vers, config.addr);
            if (_persistent_data_version != version) {
                _persistent_data_version = version;
                _available = !version.isEmpty ();
            }
        }
    }
    bool istoolong () const {
        return _interval.interval () > config.intervalLong;
    }

protected:
    void collectDiagnostics (JsonVariant &obj) const override {
        JsonObject sub = obj ["updates"].to <JsonObject> ();
            sub ["current"] = config.vers;
            if (_available)
                sub ["available"] = static_cast <String> (_persistent_data_version);
            if (_persistent_data_previous)
                sub ["checked"] = getTimeString (static_cast <time_t> (_persistent_data_previous));
    }
};

// -----------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------
