
// -----------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------

#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>

class WebSocket : private Singleton<WebSocket>, public ConnectionReceiver<WebSocket>::Insertable, public JsonSerializable {
public:
    typedef struct {
        bool enabled;
        uint16_t port;
        String root;
    } Config;

private:
    //

    void events (AsyncWebSocketClient *client, const AwsEventType type, const void *arg, const uint8_t *data, const size_t len) {
        if (type == WS_EVT_CONNECT) {
            DEBUG_PRINTF ("WebSocket[%u]::events: CONNECT address=%s\n", client->id (), client->remoteIP ().toString ().c_str ());
            _connected (client);
        } else if (type == WS_EVT_DISCONNECT) {
            DEBUG_PRINTF ("WebSocket[%u]::events: DISCONNECT\n", client->id ());
            _disconnected ();
        } else if (type == WS_EVT_ERROR) {
            DEBUG_PRINTF ("WebSocket[%u]::events: ERROR: (%u) %s\n", client->id (), *reinterpret_cast<const uint16_t *> (arg), reinterpret_cast<const char *> (data));
            _connected_error (reinterpret_cast<const char *> (data));
        } else if (type == WS_EVT_PONG) {
            DEBUG_PRINTF ("WebSocket[%u]::events: PONG: (%u) %s\n", client->id (), len, len ? reinterpret_cast<const char *> (data) : "");
        } else if (type == WS_EVT_DATA) {
            const AwsFrameInfo *info = reinterpret_cast<const AwsFrameInfo *> (arg);
            if (info->opcode == WS_TEXT && info->final && info->index == 0 && info->len == len) {
                DEBUG_PRINTF ("WebSocket[%u]::events: message [%lu]: %.*s\n", client->id (), len, len, reinterpret_cast<const char *> (data));
                _connected_writeReceived (String (data, len));
                // client->text ("I got your text message");
            }
        }
    }
    static void __webSocketEventHandler (AsyncWebSocket *, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len) {
        WebSocket *websocket = Singleton<WebSocket>::instance ();
        if (websocket != nullptr)
            websocket->events (client, type, arg, data, len);
    }

    //

    const Config &config;

    AsyncWebServer _server;
    AsyncWebSocket _socket;
    Initialisable _started;

    ConnectionReceiver<WebSocket> _connectionReceiver;
    ConnectionSender<WebSocket> _connectionSender;
    ActivationTracker _connections, _disconnections;
    ActivationTrackerWithDetail _errors;
    bool _connectionActive = false;
    AsyncWebSocketClient *_connectionClient = nullptr;

    void _connected (AsyncWebSocketClient *client) {
        _disconnect ();    // only one connection
        if (! _connectionActive) {
            _connectionClient = client;
            _connections++;
            _connectionActive = true;
        }
    }
    void _connected_error (const String &error) {
        if (_connectionActive) {
            _errors += error;
        }
    }
    bool _connected_send (const String &data) {
        if (_connectionActive) {
            return _connectionSender.send (data);
        } else {
            DEBUG_PRINTF ("WebSocket::send: not connected");
            return false;
        }
    }
    void _connected_writeReceived (const String &str) {
        if (_connectionActive) {
            _connectionReceiver.insert (str);
        }
    }
    void _disconnect () {
        if (_connectionActive) {
            _connectionActive = false;
            _disconnections++;
            if (_connectionClient) {
                _connectionClient->close ();
                _connectionClient = nullptr;
            }
            _connectionReceiver.drain ();
        }
    }
    void _disconnected () {
        if (_connectionActive) {
            _connectionActive = false;
            _disconnections++;
            _connectionClient = nullptr;
            _connectionReceiver.drain ();
        }
    }
    void _connection_init () {
        _socket.onEvent (__webSocketEventHandler);
        _server.addHandler (&_socket);
        _server.begin ();
        DEBUG_PRINTF ("WebSocket::start: active, port=%u, root=%s\n", config.port, config.root.c_str ());
    }
    void _connection_process () {
        if (_connectionActive) {
            _connectionReceiver.process ();
        }
    }

public:
    explicit WebSocket (const Config &cfg) :
        Singleton<WebSocket> (this),
        ConnectionReceiver<WebSocket>::Insertable (&_connectionReceiver),
        config (cfg),
        _server (config.port),
        _socket (config.root),
        _connectionReceiver (this),
        _connectionSender ([&] (const String &str) {
            if (_connectionClient)
                _connectionClient->text (str);    // XXX a bit worried about this
        }) { }

    void begin () {
    }
    void process () {
        if (config.enabled && ! _started)    // only when connected or panic()
            _connection_init ();
        _connection_process ();
    }
    bool available () const {
        return _connectionActive;
    }
    //
    // json only
    bool send (const String &data) {
        return _connected_send (data);
    }
    //
    void serialize (JsonVariant &obj) const override {
        if ((obj ["connected"] = _connectionActive)) {
            obj ["address"] = _connectionClient->remoteIP ();
        }
        if (_connectionReceiver._failures)
            obj ["receiveFailures"] = _connectionReceiver._failures;
        if (_errors)
            obj ["errors"] = _errors;
        if (_connections)
            obj ["connects"] = _connections;
        if (_disconnections)
            obj ["disconnects"] = _disconnections;
    }
};

// -----------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------
