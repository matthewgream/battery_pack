
// -----------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------

template <typename C>
class ConnectionReceiver_TypeSpecific : public ConnectionReceiver<C>::Handler {
protected:
    const String _type;

public:
    ConnectionReceiver_TypeSpecific (const String &type) :
        _type (type) { }
    virtual bool process (C &, const String &, JsonDocument &) = 0;
    bool process (C &c, JsonDocument &doc) override {
        const char *type = doc [_type.c_str ()], *time = doc ["time"];
        return (type != nullptr && time != nullptr) ? process (c, String (time), doc) : false;
    }
};

// -----------------------------------------------------------------------------------------------

class ProgramDataControl : public Component, public Diagnosticable {
public:
    typedef struct {
        String url_version;
    } Config;

private:
    const Config &config;

    const String _id;
    ModuleConnectivity &_components;

    class WebSocketReceiver_TypeInfo : public ConnectionReceiver_TypeSpecific<WebSocket> {
    public:
        WebSocketReceiver_TypeInfo () :
            ConnectionReceiver_TypeSpecific<WebSocket> ("info") {};
        bool process (WebSocket &device, const String &time, JsonDocument &doc) override {
            String content = doc ["info"] | "(not provided)";
            DEBUG_PRINTF ("WebSocketReceiver_TypeInfo:: type=info, time=%s, info='%s'\n", time.c_str (), content.c_str ());
            return true;
        }
    };

    class BluetoothReceiver_TypeInfo : public ConnectionReceiver_TypeSpecific<BluetoothServer> {
    public:
        BluetoothReceiver_TypeInfo () :
            ConnectionReceiver_TypeSpecific<BluetoothServer> ("info") {};
        bool process (BluetoothServer &device, const String &time, JsonDocument &doc) override {
            String content = doc ["info"] | "(not provided)";
            DEBUG_PRINTF ("BluetoothReceiver_TypeInfo:: type=info, time=%s, info='%s'\n", time.c_str (), content.c_str ());
            return true;
        }
    };
    class BluetoothReceiver_TypeCtrl : public ConnectionReceiver_TypeSpecific<BluetoothServer> {
    public:
        BluetoothReceiver_TypeCtrl () :
            ConnectionReceiver_TypeSpecific<BluetoothServer> ("ctrl") {};
        bool process (BluetoothServer &device, const String &time, JsonDocument &doc) override {
            String content = doc ["ctrl"] | "(not provided)";
            DEBUG_PRINTF ("BluetoothReceiver_TypeCtrl:: type=ctrl, time=%s, ctrl='%s'\n", time.c_str (), content.c_str ());
            // XXX
            // request controllables
            // process controllables
            // force reboot
            // turn on/off diag/logging
            // wipe spiffs data file
            // disable/enable wifi
            // change wifi user/pass
            // suppress specific alarms
            return true;
        }
    };

public:
    explicit ProgramDataControl (const Config &cfg, const String &id, ModuleConnectivity &components) :
        config (cfg),
        _id (id),
        _components (components) { }

    void begin () override {
        extern const String build;
        const String addr = getMacAddressBase ("");
        _components.webserver ().__implementation ().on (config.url_version.c_str (), HTTP_GET, [] (AsyncWebServerRequest *request) {
            request->send (200, "text/plain", build);
        });
        // XXX for now ...
        _components.blue ().insertReceivers ({
            { String ("ctrl"), std::make_shared<BluetoothReceiver_TypeCtrl> () },
            { String ("info"), std::make_shared<BluetoothReceiver_TypeInfo> () }
        });
        _components.websocket ().insertReceivers ({
            { String ("info"), std::make_shared<WebSocketReceiver_TypeInfo> () }
        });

        auto text = MDNS::Service::TXT::Builder ()
                        .add ("name", DEFAULT_NAME)
                        .add ("addr", addr)
                        .add ("build", build)
                        .build ();
        _components.mdns ().__implementation ().serviceInsert (
            MDNS::Service::Builder ()
                .withName ("webserver._http")
                .withPort (80)
                .withProtocol (MDNS::Service::Protocol::TCP)
                .withTXT (text)
                .build ());
        _components.mdns ().__implementation ().serviceInsert (
            MDNS::Service::Builder ()
                .withName ("webserver._ws")
                .withPort (81)
                .withProtocol (MDNS::Service::Protocol::TCP)
                .withTXT (text)
                .build ());
    }
    //

protected:
    void collectDiagnostics (JsonVariant &) const override {
    }
};

// -----------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------
