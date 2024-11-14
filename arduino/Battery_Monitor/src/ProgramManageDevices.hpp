
// -----------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------

class ProgramDeviceManager : public Component, public Diagnosticable {
public:
    struct Config_i2c {
        int PIN_SDA, PIN_SCL;
    };
    typedef struct {
        Config_i2c i2c0, i2c1;
        BluetoothDevice::Config blue;
        MulticastDNS::Config mdns;
        MQTTPublisher::Config mqtt;
        WebServer::Config webserver;
        WebSocket::Config websocket;
        ProgramLoggingManager::Config logging;

        RealtimeClock_DS3231::Config timeHardware;
        ProgramTimeManager::Config timeNetwork;
    } Config;

    using BooleanFunc = std::function<bool ()>;

private:
    const Config &config;
    const BooleanFunc _networkIsAvailable;

    TwoWire i2c_bus0, i2c_bus1;

    BluetoothDevice _blue;
    MulticastDNS _mdns;
    MQTTPublisher _mqtt;
    WebServer _webserver;
    WebSocket _websocket;
    ProgramLoggingManager _logging;

    RealtimeClock_DS3231 _timeHardware;
    ProgramTimeManager _timeNetwork;

public:
    explicit ProgramDeviceManager (const Config &cfg, const BooleanFunc networkIsAvailable) :
        config (cfg),
        _networkIsAvailable (networkIsAvailable),
        i2c_bus0 (0),
        i2c_bus1 (1),
        _blue (config.blue),
        _mdns (config.mdns),
        _mqtt (config.mqtt),
        _webserver (config.webserver),
        _websocket (config.websocket),
        _logging (config.logging, getMacAddressBase (""), &_mqtt),
        _timeHardware (config.timeHardware, i2c_bus0),
        _timeNetwork (config.timeNetwork)
    {
        i2c_bus0.setPins (config.i2c0.PIN_SDA, config.i2c0.PIN_SCL);
        i2c_bus1.setPins (config.i2c1.PIN_SDA, config.i2c1.PIN_SCL);
    }

    void begin () override {
        _blue.begin ();
        _mdns.begin ();
        _mqtt.begin ();
        _webserver.begin ();
        _websocket.begin ();
        _timeHardware.begin ();
        _timeNetwork.begin ();
    }
    void process () override {
        _blue.process ();
        if (_networkIsAvailable ()) {
            _mdns.process ();
            _mqtt.process ();
            _webserver.process ();
            _websocket.process ();
            _timeNetwork.process ();
        }
        _timeHardware.process ();
    }
    //
    MulticastDNS &mdns () {
        return _mdns;
    }
    BluetoothDevice &blue () {
        return _blue;
    }
    MQTTPublisher &mqtt () {
        return _mqtt;
    }
    WebServer &webserver () {
        return _webserver;
    }
    WebSocket &websocket () {
        return _websocket;
    }
    RealtimeClock_DS3231 &timeHardware () {
        return _timeHardware;
    }
    ProgramTimeManager &timeNetwork () {
        return _timeNetwork;
    }

protected:
    void collectDiagnostics (JsonVariant &obj) const override {
        JsonObject sub = obj ["devices"].to<JsonObject> ();
        sub ["blue"] = _blue;
        sub ["mdns"] = _mdns;
        sub ["mqtt"] = _mqtt;
        sub ["webserver"] = _webserver;
        sub ["websocket"] = _websocket;
        JsonObject time = obj ["time"].to<JsonObject> ();
//        time ["hardware"] = _timeHardware;
        time ["network"] = _timeNetwork;
    }
};

// -----------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------
