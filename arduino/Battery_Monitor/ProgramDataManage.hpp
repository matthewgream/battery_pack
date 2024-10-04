
// -----------------------------------------------------------------------------------------------

class DeliverManager : public Component, public Alarmable, public Diagnosticable {

public:
    typedef struct {
        BluetoothNotifier::Config blue;
    } Config;

private:
    const Config &config;

    BluetoothNotifier _blue;

    ActivationTrackerWithDetail _delivers;

public:
    DeliverManager (const Config& cfg) : config (cfg), _blue (cfg.blue) {}
    void begin () override {
        _blue.advertise ();
    }
    void process () override {
        _blue.check ();
    }
    bool deliver (const String& data) {
        if (_blue.connected ()) {
            _delivers += IntToString (data.length ()), _blue.notify (data);
            return true;
        }
        return false;
    }

protected:
    void collectAlarms (AlarmSet& alarms) const override {
        if (_blue.maxpacket () > config.blue.mtu) alarms += ALARM_DELIVER_SIZE;
    }
    void collectDiagnostics (JsonDocument &obj) const override {
        JsonObject deliver = obj ["deliver"].to <JsonObject> ();
        _delivers.serialize (deliver ["delivers"].to <JsonObject> ());
        _blue.serialize (deliver);
    }
};

// -----------------------------------------------------------------------------------------------
    
class PublishManager : public Component, public Alarmable, public Diagnosticable {

public:
    typedef struct {
        MQTTPublisher::Config mqtt;
        int failureLimit;
    } Config;
    using BooleanFunc = std::function <bool ()>;

private:
    const Config &config;

    const BooleanFunc _networkIsAvailable;

    MQTTPublisher _mqtt;

    ActivationTrackerWithDetail _publishes;
    counter_t _failures = 0;

public:
    PublishManager (const Config& cfg, const BooleanFunc networkIsAvailable) : config (cfg), _networkIsAvailable (std::move (networkIsAvailable)), _mqtt (cfg.mqtt) {}
    void begin () override {
        _mqtt.setup ();
    }
    void process () override {
        if (_networkIsAvailable ())
            _mqtt.process ();
    }
    bool publish (const String& data) {
        if (_networkIsAvailable ()) {
          if (_mqtt.connected () && _mqtt.publish (config.mqtt.topic, data)) {
              _publishes += IntToString (data.length ()), _failures = 0;
              return true;
          } else _failures ++;
        }
        return false;
    }
    inline bool connected () { return _networkIsAvailable () && _mqtt.connected (); }

protected:
    void collectAlarms (AlarmSet& alarms) const override {
        if (_failures > config.failureLimit) alarms += ALARM_PUBLISH_FAIL;
    }
    void collectDiagnostics (JsonDocument &obj) const override {
        JsonObject publish = obj ["publish"].to <JsonObject> ();
        JsonObject failures = publish ["failures"].to <JsonObject> ();
        failures ["count"] = _failures;
        failures ["limit"] = config.failureLimit;
        _publishes.serialize (publish ["publishes"].to <JsonObject> ());
        _mqtt.serialize (publish);
    }
};

// -----------------------------------------------------------------------------------------------

class StorageManager : public Component, public Alarmable, public Diagnosticable {

public:
    typedef struct {
        String filename ;
        float remainLimit;
        int failureLimit;
    } Config;

private:
    const Config &config;

    SPIFFSFile _file;

    ActivationTrackerWithDetail _appends;
    counter_t _failures = 0, _erasures = 0;

public:
    typedef SPIFFSFile::LineCallback LineCallback;
    StorageManager (const Config& cfg) : config (cfg), _file (config.filename) {}
    void begin () override {
        _failures = _file.begin () ? 0 : _failures + 1;
    }
    inline long size () const {
        return _file.size ();
    }
    bool append (const String& data) {
        if (_file.append (data)) {
            _appends += IntToString (data.length ()), _failures = 0;
            return true;
        } else _failures ++;
        return false;
    }
    inline bool retrieve (LineCallback& callback) { // XXX const
        return _file.read (callback);
    }
    void erase () {
        _erasures ++;
        _file.erase ();
    }

protected:
    void collectAlarms (AlarmSet& alarms) const override {
        if (_failures > config.failureLimit) alarms += ALARM_STORAGE_FAIL;
        if (_file.remains () < config.remainLimit) alarms += ALARM_STORAGE_SIZE;
    }
    void collectDiagnostics (JsonDocument &obj) const override {
        JsonObject storage = obj ["storage"].to <JsonObject> ();
        storage ["critical"] = config.remainLimit;
        storage ["erasures"] = _erasures;
        JsonObject failures = storage ["failures"].to <JsonObject> ();
        failures ["count"] = _failures;
        failures ["limit"] = config.failureLimit;
        _appends.serialize (storage ["appends"].to <JsonObject> ());
        _file.serialize (storage);
    }
};

// -----------------------------------------------------------------------------------------------
