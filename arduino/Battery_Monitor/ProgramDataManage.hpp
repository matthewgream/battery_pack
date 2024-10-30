
// XXX this is all a bit of a mess at the moment ...

// -----------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------

template<typename C>
class ConnectionReceiver_TypeSpecific : public ConnectionReceiver<C>::Handler {
protected:
    const String _type;
public:
    ConnectionReceiver_TypeSpecific(const String& type)
        : _type(type) {}
    virtual bool process(C&, const String&, JsonDocument&) = 0;
    bool process(C& c, JsonDocument& doc) override {
        const char *type = doc[_type.c_str()], *time = doc["time"];
        return (type != nullptr && time != nullptr) ? process(c, String(time), doc) : false;
    }
};

// -----------------------------------------------------------------------------------------------

class ControlManager : public Component, public Diagnosticable {
public:
    typedef struct {
        String url_version;
    } Config;

private:
    const Config& config;

    const String _id;
    DeviceManager& _devices;

    class WebSocketReceiver_TypeInfo : public ConnectionReceiver_TypeSpecific<WebSocket> {
    public:
        WebSocketReceiver_TypeInfo()
            : ConnectionReceiver_TypeSpecific<WebSocket>("info"){};
        bool process(WebSocket& device, const String& time, JsonDocument& doc) override {
            String content = doc["info"] | "(not provided)";
            DEBUG_PRINTF("WebSocketReceiver_TypeInfo:: type=info, time=%s, info='%s'\n", time.c_str(), content.c_str());
            return true;
        }
    };

    class BluetoothReceiver_TypeInfo : public ConnectionReceiver_TypeSpecific<BluetoothDevice> {
    public:
        BluetoothReceiver_TypeInfo()
            : ConnectionReceiver_TypeSpecific<BluetoothDevice>("info"){};
        bool process(BluetoothDevice& device, const String& time, JsonDocument& doc) override {
            String content = doc["info"] | "(not provided)";
            DEBUG_PRINTF("BluetoothReceiver_TypeInfo:: type=info, time=%s, info='%s'\n", time.c_str(), content.c_str());
            return true;
        }
    };
    class BluetoothReceiver_TypeCtrl : public ConnectionReceiver_TypeSpecific<BluetoothDevice> {
    public:
        BluetoothReceiver_TypeCtrl()
            : ConnectionReceiver_TypeSpecific<BluetoothDevice>("ctrl"){};
        bool process(BluetoothDevice& device, const String& time, JsonDocument& doc) override {
            String content = doc["ctrl"] | "(not provided)";
            DEBUG_PRINTF("BluetoothReceiver_TypeCtrl:: type=ctrl, time=%s, ctrl='%s'\n", time.c_str(), content.c_str());
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
    explicit ControlManager(const Config& cfg, const String& id, DeviceManager& devices)
        : config(cfg), _id(id), _devices(devices) {}

    void begin() override {
        extern const String build;
        const String addr = getMacAddressBase("");
        _devices.webserver().__implementation().on(config.url_version.c_str(), HTTP_GET, [](AsyncWebServerRequest* request) {
            request->send(200, "text/plain", build);
        });
        // XXX for now ...
        _devices.blue().insertReceivers({ { String("ctrl"), std::make_shared<BluetoothReceiver_TypeCtrl>() }, { String("info"), std::make_shared<BluetoothReceiver_TypeInfo>() } });
        _devices.websocket().insertReceivers({ { String("info"), std::make_shared<WebSocketReceiver_TypeInfo>() } });
        _devices.mdns().__implementation().addServiceRecord(MDNSServiceTCP, 80, "webserver._http", { "build=" + build, "type=BatteryMonitor" });
        _devices.mdns().__implementation().addServiceRecord(MDNSServiceTCP, 81, "BatteryMonitor._ws", { "addr=" + addr, "type=BatteryMonitor" });
    }
    //

protected:
    void collectDiagnostics(JsonVariant&) const override {
    }
};

// -----------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------

class DeliverManager : public Component, public Alarmable, public Diagnosticable {
public:
    typedef struct {
        String topic;
        counter_t failureLimit;
    } Config;

private:
    const Config& config;

    const String _id;
    BluetoothDevice& _blue;
    MQTTPublisher& _mqtt;
    WebSocket& _webs;
    ActivationTrackerWithDetail _delivers;
    ActivationTracker _failures;

public:
    explicit DeliverManager(const Config& cfg, const String& id, BluetoothDevice& blue, MQTTPublisher& mqtt, WebSocket& webs)
        : Alarmable({ AlarmCondition(ALARM_DELIVER_FAIL, [this]() {
                          return _failures > config.failureLimit;
                      }),
                      AlarmCondition(ALARM_DELIVER_SIZE, [this]() {
                          return _blue.payloadExceeded();
                      }) }),
          config(cfg), _id(id), _blue(blue), _mqtt(mqtt), _webs(webs) {}

    bool available() {
        return _blue.available() || _webs.available() || _mqtt.available();
    }
    //
    bool deliver(const String& data, const String& type, bool willPublishToMqtt) {
        if ((_blue.available() && _blue.send(data)) || (_webs.available() && _webs.send(data)) || (willPublishToMqtt || (_mqtt.available() && _mqtt.publish(config.topic + "/" + _id + "/" + type, data)))) {
            _delivers += ArithmeticToString(data.length());
            _failures = 0;
            return true;
        } else _failures++;
        return false;
    }

protected:
    void collectDiagnostics(JsonVariant& obj) const override {
        JsonObject sub = obj["deliver"].to<JsonObject>();
        if (_delivers)
            sub["delivers"] = _delivers;
        if (_failures) {
            sub["failures"] = _failures;
            JsonObject failures = sub["failures"].as<JsonObject>();
            failures["limit"] = config.failureLimit;
        }
    }
};

// -----------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------

class PublishManager : public Component, public Alarmable, public Diagnosticable {
public:
    typedef struct {
        String topic;
        counter_t failureLimit;
    } Config;

    using BooleanFunc = std::function<bool()>;

private:
    const Config& config;

    const String _id;
    MQTTPublisher& _mqtt;
    ActivationTrackerWithDetail _publishes;
    ActivationTracker _failures;

public:
    explicit PublishManager(const Config& cfg, const String& id, MQTTPublisher& mqtt)
        : Alarmable({ AlarmCondition(ALARM_PUBLISH_FAIL, [this]() {
                          return _failures > config.failureLimit;
                      }),
                      AlarmCondition(ALARM_PUBLISH_SIZE, [this]() {
                          return _mqtt.bufferExceeded();
                      }) }),
          config(cfg), _id(id), _mqtt(mqtt) {}

    bool available() {
        return _mqtt.available();
    }
    //
    bool publish(const String& data, const String& type) {
        if (_mqtt.publish(config.topic + "/" + _id + "/" + type, data)) {
            _publishes += ArithmeticToString(data.length());
            _failures = 0;
            return true;
        } else _failures++;
        return false;
    }

protected:
    void collectDiagnostics(JsonVariant& obj) const override {
        JsonObject sub = obj["publish"].to<JsonObject>();
        if (_publishes)
            sub["publishes"] = _publishes;
        if (_failures) {
            sub["failures"] = _failures;
            JsonObject failures = sub["failures"].as<JsonObject>();
            failures["limit"] = config.failureLimit;
        }
    }
};

// -----------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------

class StorageManager : public Component, public Alarmable, public Diagnosticable {
public:
    typedef struct {
        String filename;
        float remainLimit;
        counter_t failureLimit;
    } Config;
    typedef SPIFFSFile::LineCallback LineCallback;

private:
    const Config& config;

    SPIFFSFile _file;
    ActivationTrackerWithDetail _appends;
    ActivationTracker _failures, _erasures;

public:
    explicit StorageManager(const Config& cfg)
        : Alarmable({ AlarmCondition(ALARM_STORAGE_FAIL, [this]() {
                          return _failures > config.failureLimit;
                      }),
                      AlarmCondition(ALARM_STORAGE_SIZE, [this]() {
                          return _file.remains() < config.remainLimit;
                      }) }),
          config(cfg), _file(config.filename) {}

    void begin() override {
        if (!_file.begin()) _failures++;
    }
    bool available() const {
        return _file.available();
    }
    //
    long size() const {
        return _file.size();
    }
    bool append(const String& data) {
        if (_file.append(data)) {
            _appends += ArithmeticToString(data.length());
            _failures = 0;
            return true;
        } else _failures++;
        return false;
    }
    bool retrieve(LineCallback& callback) {    // XXX const
        return _file.read(callback);
    }
    void erase() {
        _file.erase();
        _erasures++;
    }

protected:
    void collectDiagnostics(JsonVariant& obj) const override {
        JsonObject sub = obj["storage"].to<JsonObject>();
        sub["critical"] = config.remainLimit;
        if (_appends)
            sub["appends"] = _appends;
        if (_failures) {
            sub["failures"] = _failures;
            JsonObject failures = sub["failures"].as<JsonObject>();
            failures["limit"] = config.failureLimit;
        }
        if (_erasures)
            sub["erasures"] = _erasures;
        sub["file"] = _file;
    }
};

// -----------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------
