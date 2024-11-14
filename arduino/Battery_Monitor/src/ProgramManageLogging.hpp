
// -----------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------

#ifdef DEBUG
#include <mutex>
#endif

class ProgramLoggingManager : private Singleton<ProgramLoggingManager> {
public:
    typedef struct {
        bool enableSerial, enableMqtt;
        String mqttTopic;
    } Config;

#ifdef DEBUG
private:
    const Config &config;

    bool _enableSerial = false, _enableMqtt = false;
    __DebugLoggerFunc __debugLoggerPrevious = nullptr;
    MQTTPublisher *_mqttClient;
    const String _mqttTopic;

public:
    explicit ProgramLoggingManager (const Config &cfg, const String &id, MQTTPublisher *mqttClient = nullptr) :
        Singleton<ProgramLoggingManager> (this),
        config (cfg),
        _mqttClient (mqttClient),
        _mqttTopic (config.mqttTopic + "/logs/" + id) {
        init ();
    }
    ~ProgramLoggingManager () {
        term ();
    }

protected:
    void init () {
        if (config.enableMqtt && _mqttClient != nullptr) {
            __debugLoggerPrevious = __debugLoggerSet (__debugLoggerMQTT);
            if (config.enableSerial)
                _enableSerial = true;
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
    void term () {    // not entirely thread safe
        __debugLoggerSet (__debugLoggerPrevious);
        __debugLoggerPrevious = nullptr;
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

    static void __debugLoggerMQTT (const char *format, ...) {

        auto logging = Singleton<ProgramLoggingManager>::instance ();
        if (! logging)
            return;

        std::lock_guard<std::mutex> guard (_bufferMutex);

        va_list args;
        va_start (args, format);
        int printed = vsnprintf (_bufferContent + _bufferOffset, (_bufferLength - _bufferOffset), format, args);
        va_end (args);
        if (printed < 0)
            return;

        _bufferOffset = (printed >= (_bufferLength - _bufferOffset)) ? (_bufferLength - 1) : (_bufferOffset + printed);
        if (_bufferOffset == (_bufferLength - 1) || (_bufferOffset > 0 && _bufferContent [_bufferOffset - 1] == '\n')) {
            while (_bufferOffset > 0 && _bufferContent [_bufferOffset - 1] == '\n')
                _bufferContent [--_bufferOffset] = '\0';
            _bufferContent [_bufferOffset] = '\0';
            _bufferOffset = 0;

            if (logging->_enableSerial)
                Serial.println (_bufferContent);

            if (logging->_enableMqtt && _bufferContent [0] != '\0') {
#ifdef DEFAULT_SCRUB_SENSITIVE_CONTENT_FROM_NETWORK_LOGGING
                for (char *location = _bufferContent; (location = strstr (location, "pass=")) != NULL;)
                    for (location += sizeof ("pass=") - 1; *location != '\0' && *location != ',' && *location != ' ';)
                        *location++ = '*';
                for (char *location = _bufferContent; (location = strstr (location, "pin=")) != NULL;)
                    for (location += sizeof ("pin=") - 1; *location != '\0' && *location != ',' && *location != ' ';)
                        *location++ = '*';
#endif
                logging->_mqttClient->publish__native (logging->_mqttTopic.c_str (), _bufferContent);
            }
        }
    }
#else
    explicit LoggingHandler (const Config &, MQTTPublisher *) :
        Singleton<LoggingHandler> (this) { }
#endif
};

#ifdef DEBUG
std::mutex ProgramLoggingManager::_bufferMutex;
char ProgramLoggingManager::_bufferContent [_bufferLength];
int ProgramLoggingManager::_bufferOffset = 0;
#endif

// -----------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------
