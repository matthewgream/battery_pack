
// -----------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------

#include <ArduinoJson.h>
#include <WiFi.h>
#include <PubSubClient.h>

class MQTTClient : public JsonSerializable {
public:
    static inline constexpr uint16_t DEFAULT_PORT = 1883;
    struct Peer {
        String name;
        uint16_t port;
        String user;
        String pass;
    };
    using PeersManager = ConnectionPeers<Peer>;

    typedef struct {
        String client;
        PeersManager::Config peers;
        uint16_t bufferSize;
    } Config;
    using __implementation_t = PubSubClient;

private:
    const Config &config;

    PeersManager _peers;
    WiFiClient _wifiClient;
    PubSubClient _mqttClient;
    ActivationTrackerWithDetail _bufferExceeded;

    bool connect () {
        const Peer peer = _peers.select ();
        _mqttClient.setServer (peer.name.c_str (), peer.port);
        const bool result = _mqttClient.connect (config.client.c_str (), peer.user.c_str (), peer.pass.c_str ());
        DEBUG_PRINTF ("MQTTPublisher::connect: host=%s, port=%u, client=%s, user=%s, pass=%s, bufferSize=%u, result=%d\n", peer.name.c_str (), peer.port, config.client.c_str (), peer.user.c_str (), peer.pass.c_str (), config.bufferSize, result);
        _peers.update (result);
        return result;
    }

public:
    explicit MQTTClient (const Config &cfg) :
        config (cfg),
        _peers (config.peers, [] (const String &details) {    // host:port/user@pass
            Peer peer { .port = DEFAULT_PORT };
            const int positionSlash = details.indexOf ('/');
            if (details.indexOf ('/') != -1) {
                const int positionAt = details.indexOf ('@', positionSlash);
                if (positionAt != -1)
                    peer.user = details.substring (positionSlash + 1, positionAt), peer.pass = details.substring (positionAt + 1);
            }
            const int positionColon = details.indexOf (':'), positionEnd = (positionSlash != -1) ? positionSlash : details.length ();
            if (positionColon != -1)
                peer.name = details.substring (0, positionColon), peer.port = details.substring (positionColon + 1, positionEnd).toInt ();
            else
                peer.name = details.substring (0, positionEnd);
            return peer;
        }),
        _mqttClient (_wifiClient) { }

    void begin () {
        _mqttClient.setBufferSize (config.bufferSize);
    }
    void process () {
        _mqttClient.loop ();
        if (! _mqttClient.connected () && _peers.available ())
            connect ();
    }
    inline bool available () {
        return _mqttClient.connected ();
    }
    __implementation_t &__implementation () {
        return _mqttClient;
    }
    //
    bool publish (const String &topic, const String &data) {
        if (data.length () > config.bufferSize) {
            _bufferExceeded += ArithmeticToString (data.length ());
            DEBUG_PRINTF ("MQTTPublisher::publish: failed, length %u would exceed buffer size %u\n", data.length (), config.bufferSize);
            return false;
        }
        const bool result = _mqttClient.publish (topic.c_str (), data.c_str ());
        DEBUG_PRINTF ("MQTTPublisher::publish: length=%u, result=%d\n", data.length (), result);
        return result;
    }
    void publish__native (const char *topic, const char *data) {    // no logging, silent dropping
        if (_mqttClient.connected ())
            _mqttClient.publish (topic, data);
    }
    inline bool bufferExceeded () const {
        return _bufferExceeded;
    }
    //
    void serialize (JsonVariant &obj) const override {
        PubSubClient &mqttClient = const_cast<MQTTClient *> (this)->_mqttClient;
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
        case MQTT_CONNECTION_TIMEOUT :
            return "CONNECTION_TIMEOUT";
        case MQTT_CONNECTION_LOST :
            return "CONNECTION_LOST";
        case MQTT_CONNECT_FAILED :
            return "CONNECT_FAILED";
        case MQTT_CONNECTED :
            return "CONNECTED";
        case MQTT_CONNECT_BAD_PROTOCOL :
            return "CONNECT_BAD_PROTOCOL";
        case MQTT_CONNECT_BAD_CLIENT_ID :
            return "CONNECT_BAD_CLIENT_ID";
        case MQTT_CONNECT_UNAVAILABLE :
            return "CONNECT_UNAVAILABLE";
        case MQTT_CONNECT_BAD_CREDENTIALS :
            return "CONNECT_BAD_CREDENTIALS";
        case MQTT_CONNECT_UNAUTHORIZED :
            return "CONNECT_UNAUTHORIZED";
        default :
            return "UNDEFINED_(" + ArithmeticToString (state) + ")";
        }
    }
};

// -----------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------
