
// -----------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------

class ControlManager: public Component, public Diagnosticable {
public:
    typedef struct {
    } Config;

private:
    const Config &config;

    const String _id;
    DeviceManager& _devices;

    class BluetoothWriteHandler_TypeCtrl: public BluetoothWriteHandler_TypeSpecific {
    public:
        BluetoothWriteHandler_TypeCtrl (): BluetoothWriteHandler_TypeSpecific ("ctrl") {};
        bool process (BluetoothDevice& device, const String& time, const String& ctrl, JsonDocument& doc) override {
            DEBUG_PRINTF ("BluetoothWriteHandler_TypeCtrl:: type=ctrl, time=%s, ctrl='%s'\n", time.c_str (), ctrl.c_str ());
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
    class BluetoothReadHandler_TypeCtrl: public BluetoothReadHandler_TypeSpecific {
    public:
        BluetoothReadHandler_TypeCtrl (): BluetoothReadHandler_TypeSpecific ("ctrl") {};
        bool process (BluetoothDevice& device, const String& time, const String& ctrl, JsonDocument& doc) override {
            DEBUG_PRINTF ("BluetoothReadHandler_TypeCtrl:: type=ctrl\n");
            // XXX
            // send controllables
            return false;
        }
    };

public:
    explicit ControlManager (const Config& cfg, const String& id, DeviceManager& devices): config (cfg), _id (id), _devices (devices) {
        _devices.blue ().insert ({ { String ("ctrl"), std::make_shared <BluetoothWriteHandler_TypeCtrl> () } });
        _devices.blue ().insert ({ { std::make_shared <BluetoothReadHandler_TypeCtrl> () } });
    }

protected:
    void collectDiagnostics (JsonVariant &) const override {
    }
};

// -----------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------

// XXX wifi, mqtt, blue
// connect to delivery peer
// listen for request, switch/use that peer
// alternate if peer disconnected
// xxx should make it an interface: DeliverChannel
class DeliverManager: public Component, public Alarmable, public Diagnosticable {
public:
    typedef struct {
        counter_t failureLimit;
    } Config;

private:
    const Config &config;

    const String _id;
    BluetoothDevice& _blue;
    MQTTPublisher& _mqtt;
    WebServer& _webs;

    ActivationTrackerWithDetail _delivers;
    ActivationTracker _failures;

public:
    explicit DeliverManager (const Config& cfg, const String& id, BluetoothDevice& blue, MQTTPublisher& mqtt, WebServer& webs): Alarmable ({
            AlarmCondition (ALARM_DELIVER_FAIL, [this] () { return _failures > config.failureLimit; }),
            AlarmCondition (ALARM_DELIVER_SIZE, [this] () { return _blue.payloadExceeded (); })
        }), config (cfg), _id (id), _blue (blue), _mqtt (mqtt), _webs (webs) {}
    bool deliver (const String& data) {
        if (_blue.available ()) {
            if (_blue.notify (data)) {
                _delivers += ArithmeticToString (data.length ());
                _failures = 0;
                return true;
            } else _failures ++;
        }
        return false;
    }
    bool available () { return _blue.available (); }

protected:
    void collectDiagnostics (JsonVariant &obj) const override {
        JsonObject sub = obj ["deliver"].to <JsonObject> ();
            if (_delivers)
                sub ["delivers"] = _delivers;
            if (_failures) {
                sub ["failures"] = _failures;
                JsonObject failures = sub ["failures"].as <JsonObject> ();
                failures ["limit"] = config.failureLimit;
            }
    }
};

// -----------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------

class PublishManager: public Component, public Alarmable, public Diagnosticable {
public:
    typedef struct {
        String topic;
        counter_t failureLimit;
    } Config;

    using BooleanFunc = std::function <bool ()>;

private:
    const Config &config;
    const BooleanFunc _networkIsAvailable;

    const String _id;
    MQTTPublisher& _mqtt;
    ActivationTrackerWithDetail _publishes;
    ActivationTracker _failures;

public:
    explicit PublishManager (const Config& cfg, const String& id, MQTTPublisher& mqtt, const BooleanFunc networkIsAvailable): Alarmable ({
            AlarmCondition (ALARM_PUBLISH_FAIL, [this] () { return _failures > config.failureLimit; }),
            AlarmCondition (ALARM_PUBLISH_SIZE, [this] () { return _mqtt.bufferExceeded (); })
        }), config (cfg), _networkIsAvailable (networkIsAvailable), _id (id), _mqtt (mqtt) {}
    bool publish (const String& data, const String& subtopic) {
        if (_networkIsAvailable ()) {
          if (_mqtt.connected () && _mqtt.publish (config.topic + "/" + subtopic, data)) { // XXX address
              _publishes += ArithmeticToString (data.length ());
              _failures = 0;
              return true;
          } else _failures ++;
        }
        return false;
    }
    bool available () { return _networkIsAvailable () && _mqtt.connected (); }

protected:
    void collectDiagnostics (JsonVariant &obj) const override {
        JsonObject sub = obj ["publish"].to <JsonObject> ();
            if (_publishes)
                sub ["publishes"] = _publishes;
            if (_failures) {
                sub ["failures"] = _failures;
                JsonObject failures = sub ["failures"].as <JsonObject> ();
                failures ["limit"] = config.failureLimit;
            }
    }
};

// -----------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------

class StorageManager: public Component, public Alarmable, public Diagnosticable {
public:
    typedef struct {
        String filename ;
        float remainLimit;
        counter_t failureLimit;
    } Config;
    typedef SPIFFSFile::LineCallback LineCallback;

private:
    const Config &config;

    SPIFFSFile _file;
    ActivationTrackerWithDetail _appends;
    ActivationTracker _failures, _erasures;

public:
    explicit StorageManager (const Config& cfg): Alarmable ({
            AlarmCondition (ALARM_STORAGE_FAIL, [this] () { return _failures > config.failureLimit; }),
            AlarmCondition (ALARM_STORAGE_SIZE, [this] () { return _file.remains () < config.remainLimit; })
        }), config (cfg), _file (config.filename) {}
    void begin () override {
        if (!_file.begin ()) _failures ++;
    }
    long size () const {
        return _file.size ();
    }
    bool append (const String& data) {
        if (_file.append (data)) {
            _appends += ArithmeticToString (data.length ());
            _failures = 0;
            return true;
        } else _failures ++;
        return false;
    }
    bool retrieve (LineCallback& callback) { // XXX const
        return _file.read (callback);
    }
    void erase () {
        _file.erase ();
        _erasures ++;
    }
    bool available () const { return _file.available (); }

protected:
    void collectDiagnostics (JsonVariant &obj) const override {
        JsonObject sub = obj ["storage"].to <JsonObject> ();
            sub ["critical"] = config.remainLimit;
            if (_appends)
                sub ["appends"] = _appends;
            if (_failures) {
                sub ["failures"] = _failures;
                JsonObject failures = sub ["failures"].as <JsonObject> ();
                failures ["limit"] = config.failureLimit;
            }
            if (_erasures)
                sub ["erasures"] = _erasures;
            sub ["file"] = _file;
    }
};

// -----------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------
