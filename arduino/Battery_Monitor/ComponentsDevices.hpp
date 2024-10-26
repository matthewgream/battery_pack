
// -----------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------

#include <map>

template <typename C>
class ConnectionReceiver {
public:
    class Handler {
    public:
        virtual bool process (C&, JsonDocument&) = 0;
        virtual ~Handler () {};
    };
    using Handlers = std::map <String, std::shared_ptr <Handler>>;
    class Insertable {
        ConnectionReceiver <C>* _manager;
    public:
        explicit Insertable (ConnectionReceiver <C>* manager): _manager (manager) {}
        void insertReceivers (const ConnectionReceiver <C>::Handlers &handlers) { (*_manager) += handlers; }
    };
private:
    using Queue = QueueSimpleConcurrentSafe <String>;
    C* const _c;
    Queue _queue;
    Handlers _handlers;

    void processJson (const String& str) {
        JsonDocument doc;
        DeserializationError error;
        if ((error = deserializeJson (doc, str)) != DeserializationError::Ok) {
            DEBUG_PRINTF ("Receiver::process: deserializeJson fault: %s\n", error.c_str ());
            _failures += String ("write failed to deserialize Json: ") + String (error.c_str ());
            return;
        }
        const char *type = doc ["type"];
        if (type != nullptr) {
            auto handler = _handlers.find (String (type));
            if (handler != _handlers.end ()) {
                if (!handler->second->process (*_c, doc))
                    _failures += String ("write failed in handler for 'type': ") + String (type);
            } else _failures += String ("write failed to find handler for 'type': ") + String (type);
        } else _failures += String ("write failed to contain 'type'");
    }
public:
    ActivationTrackerWithDetail _failures;
    explicit ConnectionReceiver (C* c): _c (c) {}
    ConnectionReceiver& operator += (const Handlers& handlers) {
        _handlers.insert (handlers.begin (), handlers.end ());
        return *this;
    }
    void insert (const String& str) {
        _queue.push (str);
    }
    void process () {
        String str;
        while (_queue.pull (str)) {
            DEBUG_PRINTF ("Receiver::process: content=<<<%s>>>\n", str.c_str ());
            if (str.startsWith ("{\"type\""))
                processJson (str);
            else _failures += String ("write failed to identify as Json or have leading 'type'");
        }
    }
    void drain () {
        _queue.drain ();
    }
};

template <typename C>
class ConnectionSender {
    using Function = std::function <void (const String&)>;
    const Function _func;
public:
    ActivationTrackerWithDetail _payloadExceeded;
    explicit ConnectionSender (Function func): _func (func) {}
    bool send (const String& data, const int maxPayloadSize = -1) {
        bool result = true;
        if (maxPayloadSize == -1 || data.length () <= maxPayloadSize) {
            _func (data);
            DEBUG_PRINTF ("Sender::send: length=%u\n", data.length ());
        } else {
            JsonSplitter splitter (maxPayloadSize, { "type", "time", "addr" });
            splitter.splitJson (data, [&] (const String& part, const int elements) {
                const size_t length = part.length ();
                _func (part);
                DEBUG_PRINTF ("Sender::send: data=%u, part=%u, elements=%d\n", data.length (), length, elements);
                if (length > maxPayloadSize)
                    _payloadExceeded += ArithmeticToString (length), result = false;
                delay (20);
            });
        }
        return result;
    }
};

// -----------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------

#include <WiFiUdp.h>
// announce+response, not lookup, see https://gist.github.com/matthewgream/1c535fa86fd006ae794f4f245216b1a0
#include <ArduinoLightMDNS.h>

class MulticastDNS: public JsonSerializable {
public:
    typedef struct {
    } Config;
    using __implementation_t = MDNS;

private:
    const Config &config;

    WiFiUDP _udp;
    MDNS _mdns;

public:
    explicit MulticastDNS (const Config& cfg): config (cfg), _mdns (_udp) {}

    void begin () {
        MDNSStatus_t status = _mdns.begin ();
        if (status != MDNSSuccess)
            DEBUG_PRINTF ("NetworkManager::begin: mdns begin error=%d\n", status);
    }
    void process () {
        MDNSStatus_t status = _mdns.process ();
        if (status != MDNSSuccess)
            DEBUG_PRINTF ("NetworkManager::process: mdns process error=%d\n", status);
    }
    __implementation_t& __implementation () {
        return _mdns;
    }
    //
    void start (const IPAddress& address, const String& host) {
        _mdns.start (address, host.c_str ());
    }
    void stop () {
        _mdns.stop ();
    }
    //
    void serialize (JsonVariant &obj) const override {
    }
};

// -----------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------

#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>

class WebServer: public JsonSerializable  {
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
    explicit WebServer (const Config& cfg): config (cfg), _server (config.port) {}

    void begin () {
    }
    void process () {
        if (config.enabled && !_started) // only when connected or panic()
            _connection_init ();
    }
    __implementation_t& __implementation () {
        return _server;
    }
    //
    void serialize (JsonVariant &obj) const override {
    }
};

// -----------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------

class WebSocket: private Singleton <WebSocket>, public ConnectionReceiver <WebSocket>::Insertable, public JsonSerializable  {
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
            DEBUG_PRINTF ("WebSocket[%u]::events: ERROR: (%u) %s\n", client->id (), *reinterpret_cast <const uint16_t *> (arg), reinterpret_cast <const char*> (data));
            _connected_error (reinterpret_cast <const char*> (data));
        } else if (type == WS_EVT_PONG) {
            DEBUG_PRINTF ("WebSocket[%u]::events: PONG: (%u) %s\n", client->id (), len, len ? reinterpret_cast <const char*> (data) : "");
        } else if (type == WS_EVT_DATA) {
            const AwsFrameInfo *info = reinterpret_cast <const AwsFrameInfo *> (arg);
            if (info->opcode == WS_TEXT && info->final && info->index == 0 && info->len == len) {
                DEBUG_PRINTF ("WebSocket[%u]::events: message [%lu]: %.*s\n", client->id (), len, len, reinterpret_cast <const char*> (data));
                _connected_writeReceived (String (data, len));
                //client->text ("I got your text message");
            }
        }
    }
    static void __webSocketEventHandler (AsyncWebSocket *, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len) {
        WebSocket *websocket = Singleton <WebSocket>::instance ();
        if (websocket != nullptr) websocket->events (client, type, arg, data, len);
    }

    //

    const Config &config;

    AsyncWebServer _server;
    AsyncWebSocket _socket;
    Initialisable _started;

    ConnectionReceiver <WebSocket> _connectionReceiver;
    ConnectionSender <WebSocket> _connectionSender;
    ActivationTracker _connections, _disconnections;
    ActivationTrackerWithDetail _errors;
    bool _connectionActive = false;
    AsyncWebSocketClient *_connectionClient = nullptr;

    void _connected (AsyncWebSocketClient *client) {
        _disconnect (); // only one connection
        if (!_connectionActive) {
            _connectionClient = client;
            _connections ++; _connectionActive = true;
        }
    }
    void _connected_error (const String& error) {
        if (_connectionActive) {
            _errors += error;
        }
    }
    bool _connected_send (const String& data) {
        if (_connectionActive) {
            return _connectionSender.send (data);
        } else {
            DEBUG_PRINTF ("WebSocket::send: not connected");
            return false;
        }
    }
    void _connected_writeReceived (const String& str) {
        if (_connectionActive) {
            _connectionReceiver.insert (str);
        }
    }
    void _disconnect () {
        if (_connectionActive) {
            _connectionActive = false; _disconnections ++;
            if (_connectionClient) {
                _connectionClient->close ();
                _connectionClient = nullptr;
            }
            _connectionReceiver.drain ();
        }
    }
    void _disconnected () {
        if (_connectionActive) {
            _connectionActive = false; _disconnections ++;
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
    explicit WebSocket (const Config& cfg): Singleton <WebSocket> (this), ConnectionReceiver <WebSocket>::Insertable (&_connectionReceiver), config (cfg), _server (config.port), _socket (config.root),
        _connectionReceiver (this),
        _connectionSender ([&] (const String& str) {
            if (_connectionClient)
                _connectionClient->text (str); // XXX a bit worried about this
        }) {}

    void begin () {
    }
    void process () {
        if (config.enabled && !_started) // only when connected or panic()
            _connection_init ();
        _connection_process ();
    }
    //
    // json only
    bool send (const String& data) {
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

#include <ArduinoJson.h>
#include <WiFi.h>
#include <PubSubClient.h>

class MQTTPublisher: public JsonSerializable {
public:
    typedef struct {
        String client, host, user, pass;
        uint16_t port;
        uint16_t bufferSize;
    } Config;
    using __implementation_t = PubSubClient;

private:
    const Config &config;

    WiFiClient _wifiClient;
    PubSubClient _mqttClient;
    ActivationTrackerWithDetail _bufferExceeded;

    bool connect () {
        const bool result = _mqttClient.connect (config.client.c_str (), config.user.c_str (), config.pass.c_str ());
        DEBUG_PRINTF ("MQTTPublisher::connect: host=%s, port=%u, client=%s, user=%s, pass=%s, bufferSize=%u, result=%d\n", config.host.c_str (), config.port, config.client.c_str (), config.user.c_str (), config.pass.c_str (), config.bufferSize, result);
        return result;
    }

public:
    explicit MQTTPublisher (const Config& cfg) : config (cfg), _mqttClient (_wifiClient) {}

    void begin () {
        _mqttClient.setServer (config.host.c_str (), config.port)
            .setBufferSize (config.bufferSize);
    }
    void process () {
        _mqttClient.loop ();
        if (!_mqttClient.connected ())
            connect ();
    }
    inline bool connected () {
        return _mqttClient.connected ();
    }
    __implementation_t& __implementation () {
        return _mqttClient;
    }
    //
    bool publish (const String& topic, const String& data) {
        if (data.length () > config.bufferSize) {
            _bufferExceeded += ArithmeticToString (data.length ());
            DEBUG_PRINTF ("MQTTPublisher::publish: failed, length %u would exceed buffer size %u\n", data.length (), config.bufferSize);
            return false;
        }
        const bool result = _mqttClient.publish (topic.c_str (), data.c_str ());
        DEBUG_PRINTF ("MQTTPublisher::publish: length=%u, result=%d\n", data.length (), result);
        return result;
    }
    void publish__native (const char *topic, const char *data) { // no logging, silent dropping
        if (_mqttClient.connected ())
            _mqttClient.publish (topic, data);
    }
    inline bool bufferExceeded () const {
        return _bufferExceeded;
    }
    //
    void serialize (JsonVariant &obj) const override {
        PubSubClient& mqttClient = const_cast <MQTTPublisher *> (this)->_mqttClient;
        if ((obj ["connected"] = mqttClient.connected ())) {
            //
        }
        obj ["state"] = _state_to_string (mqttClient.state ());
        if (_bufferExceeded)
            obj ["bufferExceeded"] = _bufferExceeded;
    }

private:
    static String _state_to_string (const int state) {
        switch (state) {
          case MQTT_CONNECTION_TIMEOUT: return "CONNECTION_TIMEOUT";
          case MQTT_CONNECTION_LOST: return "CONNECTION_LOST";
          case MQTT_CONNECT_FAILED: return "CONNECT_FAILED";
          case MQTT_CONNECTED: return "CONNECTED";
          case MQTT_CONNECT_BAD_PROTOCOL: return "CONNECT_BAD_PROTOCOL";
          case MQTT_CONNECT_BAD_CLIENT_ID: return "CONNECT_BAD_CLIENT_ID";
          case MQTT_CONNECT_UNAVAILABLE: return "CONNECT_UNAVAILABLE";
          case MQTT_CONNECT_BAD_CREDENTIALS: return "CONNECT_BAD_CREDENTIALS";
          case MQTT_CONNECT_UNAUTHORIZED: return "CONNECT_UNAUTHORIZED";
          default: return "UNDEFINED_(" + ArithmeticToString (state) + ")";
        }
    }
};

// -----------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------
