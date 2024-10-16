
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
        bool debugLoggerSerial, debugLoggerMQTT;
        String debugLoggerMQTTTopic;
    } Config;

#ifdef DEBUG
private:
    const Config& config;

    bool __debugLoggerEnableSerial = false, __debugLoggerEnableMQTT = false;
    __DebugLoggerFunc __debugLoggerPrevious = nullptr;
    char __debugLoggerMQTTTopic [64+1];
    MQTTPublisher *__debugLoggerMQTTClient;

public:
    explicit LoggingHandler (const Config& cfg, MQTTPublisher *loggingMQTTClient = nullptr): Singleton <LoggingHandler> (this), config (cfg), __debugLoggerMQTTClient (loggingMQTTClient) { init (); }
    ~LoggingHandler () { term (); }

protected:
    void init () {
        if (config.debugLoggerMQTT && __debugLoggerMQTTClient != nullptr) {
            snprintf (__debugLoggerMQTTTopic, sizeof (__debugLoggerMQTTTopic) - 1, "%s/logs", config.debugLoggerMQTTTopic.c_str ());
            __debugLoggerPrevious = __debugLoggerSet (__debugLoggerMQTT);
            if (config.debugLoggerSerial) __debugLoggerEnableSerial = true;
            __debugLoggerEnableMQTT = true;
            DEBUG_PRINTF ("LoggingHandler::init: logging directed to Serial and MQTT (as topic '%s' when online)\n", __debugLoggerMQTTTopic);
        } else if (config.debugLoggerSerial) {
            __debugLoggerPrevious = __debugLoggerSet (__debugLoggerSerial);
            __debugLoggerEnableSerial = true;
            DEBUG_PRINTF ("LoggingHandler::init: logging directed to Serial\n");
        } else {
            DEBUG_PRINTF ("LoggingHandler::init: logging not directed\n");
        }
    }
    void term () { // not entirely thread safe
      __debugLoggerSet (__debugLoggerPrevious);
      __debugLoggerPrevious = nullptr;
      __debugLoggerEnableSerial = false;
      __debugLoggerEnableMQTT = false;
      __debugLoggerMQTTTopic [0] = '\0';
      __debugLoggerMQTTClient = nullptr;
      DEBUG_PRINTF ("LoggingHandler::term: logging reverted to previous\n");
    }
    void send (const char *line) {
        if (__debugLoggerEnableSerial)
            Serial.println (line);
        if (__debugLoggerEnableMQTT && line [0] != '\0')
            __debugLoggerMQTTClient->publish__native (__debugLoggerMQTTTopic, line);
    }

private:
    static std::mutex __debugLoggerBuffer_Mutex;
    #define __debugPrintBuffer_Length (1024+1)
    static char __debugLoggerBuffer_Content [__debugPrintBuffer_Length];
    static int __debugLoggerBuffer_Offset;
    static void __debugLoggerMQTT (const char* format, ...) {

        std::lock_guard <std::mutex> guard (__debugLoggerBuffer_Mutex);

        va_list args;
        va_start (args, format);
        int printed = vsnprintf (__debugLoggerBuffer_Content + __debugLoggerBuffer_Offset, (__debugPrintBuffer_Length - __debugLoggerBuffer_Offset), format, args);
        va_end (args);
        if (printed < 0)
            return;

        __debugLoggerBuffer_Offset = (printed >= (__debugPrintBuffer_Length - __debugLoggerBuffer_Offset)) ? (__debugPrintBuffer_Length - 1) : (__debugLoggerBuffer_Offset + printed);
        if (__debugLoggerBuffer_Offset == (__debugPrintBuffer_Length - 1) || (__debugLoggerBuffer_Offset > 0 && __debugLoggerBuffer_Content [__debugLoggerBuffer_Offset - 1] == '\n')) {
            while (__debugLoggerBuffer_Offset > 0 && __debugLoggerBuffer_Content [__debugLoggerBuffer_Offset - 1] == '\n')
                __debugLoggerBuffer_Content [-- __debugLoggerBuffer_Offset] = '\0';
            __debugLoggerBuffer_Content [__debugLoggerBuffer_Offset] = '\0';
            __debugLoggerBuffer_Offset = 0;

            auto logging = Singleton <LoggingHandler>::instance ();
            if (logging) logging->send (__debugLoggerBuffer_Content);
        }
    }
#else
    explicit LoggingHandler (const Config&, MQTTPublisher *): Singleton <LoggingHandler> (this) {}
#endif
};

#ifdef DEBUG
std::mutex LoggingHandler::__debugLoggerBuffer_Mutex;
char LoggingHandler::__debugLoggerBuffer_Content [__debugPrintBuffer_Length];
int LoggingHandler::__debugLoggerBuffer_Offset = 0;
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
            // turn on/off diag
            // force reboot
            // wipe spiffs data file
            // changw wifi
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
            // send all of the variables that can be controlled
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

#include <functional>

extern String ota_image_check (const String& json, const String& type, const String& vers, const String& addr);

class UpdateManager: public Component, public Alarmable, public Diagnosticable {
public:
    typedef struct {
        interval_t intervalUpdate;
        time_t intervalCheck;
        String json, type, vers;
    } Config;

    using BooleanFunc = std::function <bool ()>;

private:
    const Config& config;
    const BooleanFunc _networkIsAvailable;

    PersistentData _persistent_data;
    PersistentValue <uint32_t> _persistent_data_previous;
    PersistentValue <String> _persistent_data_version;
    bool _available;

    Intervalable _interval;

public:
    UpdateManager (const Config& cfg, const BooleanFunc networkIsAvailable): Alarmable ({
            AlarmCondition (ALARM_UPDATE_VERS, [this] () { return _available; })
        }), config (cfg), _networkIsAvailable (networkIsAvailable),
        _persistent_data ("updates"), _persistent_data_previous (_persistent_data, "previous", 0), _persistent_data_version (_persistent_data, "version", String ("")),
        _available (!static_cast <String> (_persistent_data_version).isEmpty ()),
        _interval (config.intervalUpdate) {}
    void process () override {
        if (static_cast <bool> (_interval)) {
            if (_networkIsAvailable ()) {
                time_t previous = (time_t) static_cast <uint32_t> (_persistent_data_previous), current = time (nullptr);
                if ((current > 0 && (current - previous) > (config.intervalCheck / 1000)) || (previous > current)) {
                    String version = ota_image_check (config.json, config.type, config.vers, getMacAddress ());
                    if (_persistent_data_version != version) {
                        _persistent_data_version = version;
                        _available = !version.isEmpty ();
                    }
                    _persistent_data_previous = current;
                }
            }
        }
    }

protected:
    void collectDiagnostics (JsonVariant &obj) const override {
        JsonObject sub = obj ["updates"].to <JsonObject> ();
            sub ["current"] = config.vers;
            if (_available)
                sub ["available"] = static_cast <String> (_persistent_data_version);
            const time_t time = (time_t) static_cast <uint32_t> (_persistent_data_previous);
            if (time)
                sub ["checked"] = getTimeString (time);
    }
};

// -----------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------
