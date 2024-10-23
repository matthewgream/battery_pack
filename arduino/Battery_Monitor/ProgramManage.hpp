
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
    MQTTPublisher *_mqttClient;
    const String _mqttTopic;

public:
    explicit LoggingHandler (const Config& cfg, const String& id, MQTTPublisher *mqttClient = nullptr): Singleton <LoggingHandler> (this), config (cfg), _mqttClient (mqttClient), _mqttTopic (config.mqttTopic + "/" + id + "/logs") { init (); }
    ~LoggingHandler () { term (); }

protected:
    void init () {
        if (config.enableMqtt && _mqttClient != nullptr) {
            __debugLoggerPrevious = __debugLoggerSet (__debugLoggerMQTT);
            if (config.enableSerial) _enableSerial = true;
            _enableMqtt = true;
            DEBUG_PRINTF ("LoggingHandler::init: logging directed to Serial and MQTT (as topic '%s' when online)\n", _mqttTopic.c_str ());
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
                logging->_mqttClient->publish__native (logging->_mqttTopic.c_str (), _bufferContent);
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

class DeviceManager: public Component, public Diagnosticable {
public:
    typedef struct {
        BluetoothDevice::Config blue;
        MQTTPublisher::Config mqtt;
        WebServer::Config webserver;
        WebSocket::Config websocket;
        LoggingHandler::Config logging;
    } Config;

    using BooleanFunc = std::function <bool ()>;

private:
    const Config &config;
    const BooleanFunc _networkIsAvailable;

    BluetoothDevice _blue;
    MQTTPublisher _mqtt;
    WebServer _webserver;
    WebSocket _websocket;
    LoggingHandler _logging;

public:
    explicit DeviceManager (const Config& cfg, const BooleanFunc networkIsAvailable): config (cfg), _networkIsAvailable (networkIsAvailable), _blue (config.blue), _mqtt (config.mqtt), _webserver (config.webserver), _websocket (config.websocket), _logging (config.logging, getMacAddressBase (""), &_mqtt) {}
    void begin () override {
        _blue.begin ();
        _mqtt.begin ();
        _webserver.begin ();
        _websocket.begin ();
    }
    void process () override {
        _blue.process ();
        if (_networkIsAvailable ()) {
            _mqtt.process ();
            _webserver.process ();
            _websocket.process ();
        }
    }

    BluetoothDevice& blue () { return _blue; }
    MQTTPublisher& mqtt () { return _mqtt; }
    WebServer& webserver () { return _webserver; }
    WebSocket& websocket () { return _websocket; }

protected:
    void collectDiagnostics (JsonVariant &obj) const override {
        JsonObject sub = obj ["devices"].to <JsonObject> ();
            sub ["blue"] = _blue;
            sub ["mqtt"] = _mqtt;
            sub ["webserver"] = _webserver;
            sub ["websocket"] = _websocket;
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
        return timet > 0 && _previous > static_cast <uint32_t> (0) ? (timet - _previous) * 1000 : 0;
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
