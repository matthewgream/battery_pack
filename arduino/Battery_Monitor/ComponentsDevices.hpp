
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

public:
    explicit WebServer (const Config& cfg): config (cfg), _server (config.port) {}

    void begin () {
        _server.onNotFound ([] (AsyncWebServerRequest *request) {
            request->send (404, "text/plain", STRING_404_NOT_FOUND);
        });
    }
    void process () {
        // only when connected or panic()
        if (config.enabled && !_started) {
            _server.begin ();
            DEBUG_PRINTF ("WebServer::start: active, port=%u\n", config.port);
        }
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

class WebSocket: public JsonSerializable  {
public:
    typedef struct {
        bool enabled;
        uint16_t port;
        String root;
    } Config;

private:
    const Config &config;

    AsyncWebServer _server;
    AsyncWebSocket _socket;
    Initialisable _started;

    static void webSocket_onEvent (AsyncWebSocket * server, AsyncWebSocketClient * client, AwsEventType type, void * arg, uint8_t *data, size_t len) {
        if (type == WS_EVT_CONNECT){
            DEBUG_PRINTF ("ws[%s][%u] connect\n", server->url (), client->id ());
            //client->printf("Hello Client %u :)", client->id());
            client->ping();

          } else if(type == WS_EVT_DISCONNECT){
            DEBUG_PRINTF ("ws[%s][%u] disconnect\n", server->url(), client->id());

          } else if(type == WS_EVT_ERROR){
            DEBUG_PRINTF ("ws[%s][%u] error(%u): %s\n", server->url(), client->id(), *((uint16_t*)arg), (char*)data);

          } else if(type == WS_EVT_PONG){
            DEBUG_PRINTF ("ws[%s][%u] pong[%u]: %s\n", server->url(), client->id(), len, (len)?(char*)data:"");

          } else if(type == WS_EVT_DATA){
            AwsFrameInfo * info = (AwsFrameInfo*)arg;
            if(info->final && info->index == 0 && info->len == len){
              DEBUG_PRINTF ("ws[%s][%u] %s-message[%llu]: ", server->url(), client->id(), (info->opcode == WS_TEXT)?"text":"binary", info->len);
              if(info->opcode == WS_TEXT){
                data[len] = 0;
                DEBUG_PRINTF ("%s\n", (char*)data);
              } else {
                for(size_t i=0; i < info->len; i++)
                  DEBUG_PRINTF ("%02x ", data[i]);
                DEBUG_PRINTF ("\n");
              }
              // if(info->opcode == WS_TEXT)
              //   client->text ("I got your text message");
              // else
              //   client->binary ("I got your binary message");
            } else {
              if (info->index == 0){
                if (info->num == 0)
                  DEBUG_PRINTF ("ws[%s][%u] %s-message start\n", server->url(), client->id(), (info->message_opcode == WS_TEXT)?"text":"binary");
                DEBUG_PRINTF ("ws[%s][%u] frame[%u] start[%llu]\n", server->url(), client->id(), info->num, info->len);
              }
              DEBUG_PRINTF ("ws[%s][%u] frame[%u] %s[%llu - %llu]: ", server->url(), client->id(), info->num, (info->message_opcode == WS_TEXT)?"text":"binary", info->index, info->index + len);
              if(info->message_opcode == WS_TEXT){
                data[len] = 0;
                DEBUG_PRINTF ("%s\n", (char*)data);
              } else {
                for(size_t i=0; i < len; i++)
                  DEBUG_PRINTF ("%02x ", data[i]);
                DEBUG_PRINTF ("\n");
              }

              if((info->index + len) == info->len){
                DEBUG_PRINTF ("ws[%s][%u] frame[%u] end[%llu]\n", server->url(), client->id(), info->num, info->len);
                if(info->final){
                  DEBUG_PRINTF ("ws[%s][%u] %s-message end\n", server->url(), client->id(), (info->message_opcode == WS_TEXT)?"text":"binary");
                  // if(info->message_opcode == WS_TEXT)
                  //   client->text("I got your text message");
                  // else
                  //   client->binary("I got your binary message");
                }
              }
            }
          }
    }

    void start () {
    }

public:
    explicit WebSocket (const Config& cfg): config (cfg), _server (config.port), _socket (config.root) {}

    void begin () {
    }
    void process () {
        // only when connected or panic()
        if (config.enabled && !_started) {
            _socket.onEvent (webSocket_onEvent);
            _server.addHandler (&_socket);
            _server.begin ();
            DEBUG_PRINTF ("WebSocket::start: active, port=%u, root=%s\n", config.port, config.root.c_str ());
        }
    }
    //
    void serialize (JsonVariant &obj) const override {
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
