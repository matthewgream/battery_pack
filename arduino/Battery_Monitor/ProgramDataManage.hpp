
// -----------------------------------------------------------------------------------------------

class DeliverManager : public Component, public Diagnosticable {
    const Config::DeliverConfig& config;
    BluetoothNotifier _blue;
    ActivationTracker _activations;

public:
    DeliverManager (const Config::DeliverConfig& cfg) : config (cfg) {}
    void begin () override {
        _blue.advertise (config.blue.name, config.blue.service, config.blue.characteristic);
    }
    void deliver (const String& data) {
        if (_blue.connected ()) {
            _activations ++;
            _blue.notify (data);
        }
    }

protected:
    void serializeDiagnostics (JsonObject &obj) const override {
        JsonObject deliver = obj ["deliver"].to <JsonObject> ();
        _blue.serialize (deliver);
        _activations.serialize (deliver);
    }
};

// -----------------------------------------------------------------------------------------------

class PublishManager : public Component, public Alarmable, public Diagnosticable {
    const Config::PublishConfig& config;
    MQTTPublisher _mqtt;
    ActivationTracker _activations;
    unsigned long _failures = 0;

public:
    PublishManager (const Config::PublishConfig& cfg) : config (cfg), _mqtt (cfg.mqtt) {}
    void begin () override {
        _mqtt.setup ();
    }
    void process () override {
        _mqtt.process ();
    }
    bool publish (const String& data) {
        if (_mqtt.connected () && _mqtt.publish (config.mqtt.topic, data)) {
            _activations ++;
            _failures = 0;
            return true;
        } else _failures ++;
        return false;
    }
    bool connected () { return _mqtt.connected (); }

protected:
    AlarmSet collectAlarms () const override {
        AlarmSet alarms;
        if (_failures > config.failureLimit) alarms += ALARM_PUBLISH_FAIL;
        return alarms;
    }
    void serializeDiagnostics (JsonObject &obj) const override {
        JsonObject publish = obj ["publish"].to <JsonObject> ();
        _mqtt.serialize (publish);
        _activations.serialize (publish);
    }
};

// -----------------------------------------------------------------------------------------------

class StorageManager : public Component, public Alarmable, public Diagnosticable {
    const Config::StorageConfig& config;
    SPIFFSFile _file;
    ActivationTracker _activations;
    unsigned long _failures = 0;
    unsigned long _erasures = 0;

public:
    typedef SPIFFSFile::LineCallback LineCallback;
    StorageManager (const Config::StorageConfig& cfg) : config (cfg), _file (config.filename, config.lengthMaximum) {}
    void begin () override {
        _failures = _file.begin () ? 0 : _failures + 1;
    }
    size_t size () const {
        return _file.size ();
    }
    void append (const String& data) {
        if (_file.append (data)) {
            _activations ++;
            _failures = 0;
        } else _failures ++;
    }
    bool retrieve (LineCallback& callback) const {
        return _file.read (callback);
    }
    void erase () {
        _erasures ++;
        _file.erase ();
    }

protected:
    AlarmSet collectAlarms () const override {
        AlarmSet alarms;
        if (_failures > config.failureLimit) alarms += ALARM_STORAGE_FAIL;
        if (_file.size () > config.lengthCritical) alarms += ALARM_STORAGE_SIZE;
        return alarms;
    }
    void serializeDiagnostics (JsonObject &obj) const override {
        JsonObject storage = obj ["storage"].to <JsonObject> ();
        JsonObject size = storage ["size"].to <JsonObject> ();
        size ["current"] = _file.size ();
        size ["maximum"] = config.lengthMaximum;
        size ["critical"] = config.lengthCritical;
        size ["erasures"] = _erasures;
        _activations.serialize (storage);
    }
};

// -----------------------------------------------------------------------------------------------
