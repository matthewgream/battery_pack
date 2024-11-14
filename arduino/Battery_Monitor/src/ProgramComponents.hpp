
// -----------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------

class ProgramComponents : public Component, public Diagnosticable {
public:
    typedef struct {
        BluetoothServer::Config blue;
        MulticastDNSPublisher::Config mdns;
        MQTTClient::Config mqtt;
        WebServer::Config webserver;
        WebSocket::Config websocket;
        WiFiNetworkClient::Config wifi;

    } Config;

    using BooleanFunc = std::function<bool ()>;

private:
    const Config &config;
    const BooleanFunc _networkIsAvailable;

    BluetoothServer _blue;
    MulticastDNSPublisher _mdns;
    MQTTClient _mqtt;
    WebServer _webserver;
    WebSocket _websocket;
    WiFiNetworkClient _wifi;

public:
    explicit ProgramComponents (const Config &cfg, const BooleanFunc networkIsAvailable) :
        config (cfg),
        _networkIsAvailable (networkIsAvailable),
        _blue (config.blue),
        _mdns (config.mdns),
        _mqtt (config.mqtt),
        _wifi (config.wifi, _mdns),
        _webserver (config.webserver),
        _websocket (config.websocket) { }

    void begin () override {
        _wifi.begin ();
        _mdns.begin ();
        _blue.begin ();
        _mqtt.begin ();
        _webserver.begin ();
        _websocket.begin ();
    }
    void process () override {
        _wifi.process ();
        _blue.process ();
        if (_networkIsAvailable ()) {
            _mdns.process ();
            _mqtt.process ();
            _webserver.process ();
            _websocket.process ();
        }
    }
    //
    MulticastDNSPublisher &mdns () {
        return _mdns;
    }
    BluetoothServer &blue () {
        return _blue;
    }
    MQTTClient &mqtt () {
        return _mqtt;
    }
    WebServer &webserver () {
        return _webserver;
    }
    WebSocket &websocket () {
        return _websocket;
    }
    WiFiNetworkClient &wifi () {
        return _wifi;
    }

protected:
    void collectDiagnostics (JsonVariant &obj) const override {
        JsonObject sub = obj ["components"].to<JsonObject> ();
        sub ["blue"] = _blue;
        sub ["mdns"] = _mdns;
        sub ["mqtt"] = _mqtt;
        sub ["webserver"] = _webserver;
        sub ["websocket"] = _websocket;
        sub ["wifi"] = _wifi;
    }
};

// -----------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------
