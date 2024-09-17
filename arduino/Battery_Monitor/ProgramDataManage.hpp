
// -----------------------------------------------------------------------------------------------

class DeliverManager : public Component, public Diagnosticable {
    const Config::DeliverConfig& config;
    BluetoothNotifier _bluetooth;
    ActivationTracker _activations;
public:
    DeliverManager (const Config::DeliverConfig& cfg) : config (cfg) {}
    void begin () override {
        _bluetooth.advertise (config.name, config.service, config.characteristic);
    }
    void collect (JsonObject &obj) const override {
        JsonObject deliver = obj ["deliver"].to <JsonObject> ();
        _bluetooth.serialize (deliver);
        _activations.serialize (deliver);
    }
    void deliver (const String& data) {
        if (_bluetooth.connected ()) {
            _activations ++;
            _bluetooth.notify (data);
        }
    }
};

// -----------------------------------------------------------------------------------------------

class PublishManager : public Component, public Diagnosticable {
    const Config::PublishConfig& config;
    MQTTPublisher _mqtt;
    ActivationTracker _activations;
public:
    PublishManager (const Config::PublishConfig& cfg) : config (cfg), _mqtt (cfg.mqtt) {}
    void begin () override {
        _mqtt.connect ();
    }
    void process () override {
        _mqtt.process ();
    }
    bool publish (const String& data) {
        if (!_mqtt.connected () || !_mqtt.publish (config.mqtt.topic, data))
            return false;
        _activations ++;
        return true;
    }

    void collect (JsonObject &obj) const override {
        JsonObject publish = obj ["publish"].to <JsonObject> ();
        _mqtt.serialize (publish);
        _activations.serialize (publish);
    }
    //
    bool connected () { return _mqtt.connected (); }
};

// -----------------------------------------------------------------------------------------------

class StorageManager : public Component, public Alarmable, public Diagnosticable {
    const Config::StorageConfig& config;
    SPIFFSFile _file;
    unsigned long _failures = 0;
    unsigned long _erasures = 0;
    ActivationTracker _activations;
public:
    typedef SPIFFSFile::LineCallback LineCallback;
    StorageManager (const Config::StorageConfig& cfg) : config (cfg), _file (config.filename, config.lengthMaximum) {}
    void begin () override {
        _failures = _file.begin () ? 0 : _failures + 1;
    }
    void collect (JsonObject &obj) const override {
        JsonObject storage = obj ["storage"].to <JsonObject> ();
        JsonObject size = storage ["size"].to <JsonObject> ();
        size ["current"] = _file.size ();
        size ["maximum"] = config.lengthMaximum;
        size ["critical"] = config.lengthCritical;
        size ["erasures"] = _erasures;
        _activations.serialize (storage);
    }
    //
    AlarmSet alarm () const override {
        AlarmSet alarms;
        if (_failures > config.failureLimit) alarms += ALARM_STORAGE_FAIL;
        if (_file.size () > config.lengthCritical) alarms += ALARM_STORAGE_SIZE;
        return alarms;
    }
    size_t size () const {
        return _file.size ();
    }
    void append (const String& data) {
        if (!_file.append (data))
            _failures ++;
        else {
            _failures = 0;
            _activations ++;
        }
    }
    bool retrieve (LineCallback& callback) const {
        return _file.read (callback);
    }
    void erase () {
        _erasures ++;
        _file.erase ();
    }
};

// -----------------------------------------------------------------------------------------------
