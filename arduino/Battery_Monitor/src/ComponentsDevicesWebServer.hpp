
// -----------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------

#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>

class WebServer : public JsonSerializable {
public:
    typedef struct {
        bool enabled;
        uint16_t port;
    } Config;
    using __implementation_t = AsyncWebServer;

    static inline constexpr const char *STRING_404_NOT_FOUND = "Not Found";

private:
    const Config &config;

    AsyncWebServer _server;
    Initialisable _started;

    void _connection_init () {
        _server.onNotFound ([] (AsyncWebServerRequest *request) {
            request->send (404, "text/plain", STRING_404_NOT_FOUND);
        });
        _server.begin ();
        DEBUG_PRINTF ("WebServer::start: active, port=%u\n", config.port);
    }

public:
    explicit WebServer (const Config &cfg) :
        config (cfg),
        _server (config.port) { }

    void begin () {
    }
    void process () {
        if (config.enabled && ! _started)    // only when connected or panic()
            _connection_init ();
    }
    __implementation_t &__implementation () {
        return _server;
    }
    //
    void serialize (JsonVariant &obj) const override {
    }
};

// -----------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------
