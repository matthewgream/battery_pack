
// -----------------------------------------------------------------------------------------------

class DeliverManager : public Component, public Diagnosticable {
    const Config::DeliverConfig& config;
    BluetoothNotifier _blue;
    ActivationTracker _activations;

public:
    DeliverManager (const Config::DeliverConfig& cfg) : config (cfg), _blue (cfg.blue) {}
    void begin () override {
        _blue.advertise ();
    }
    void deliver (const String& data) {
        if (_blue.connected ()) {
            _activations ++;
            _blue.notify (data);
        }
    }

protected:
    void collectDiagnostics (JsonObject &obj) const override {
        JsonObject deliver = obj ["deliver"].to <JsonObject> ();
        _blue.serialize (deliver);
        _activations.serialize (deliver ["delivered"].to <JsonObject> ());
    }
};

// -----------------------------------------------------------------------------------------------

class PublishManager : public Component, public Alarmable, public Diagnosticable {
    const Config::PublishConfig& config;
    ConnectManager &_network;
    MQTTPublisher _mqtt;
    ActivationTracker _activations;
    counter_t _failures = 0;

public:
    PublishManager (const Config::PublishConfig& cfg, ConnectManager &network) : config (cfg), _network (network), _mqtt (cfg.mqtt) {}
    void begin () override {
        _mqtt.setup ();
    }
    void process () override {
        if (_network.isAvailable ())
            _mqtt.process ();
    }
    bool publish (const String& data) {
        if (_network.isAvailable ()) {
          if (_mqtt.connected () && _mqtt.publish (config.mqtt.topic, data)) {
              _activations ++;
              _failures = 0;
              return true;
          } else _failures ++;
        }
        return false;
    }
    bool connected () { return _network.isAvailable () && _mqtt.connected (); }

protected:
    void collectAlarms (AlarmSet& alarms) const override {
        if (_failures > config.failureLimit) alarms += ALARM_PUBLISH_FAIL;
    }
    void collectDiagnostics (JsonObject &obj) const override {
        JsonObject publish = obj ["publish"].to <JsonObject> ();
        _mqtt.serialize (publish);
        _activations.serialize (publish ["published"].to <JsonObject> ());
    }
};

// -----------------------------------------------------------------------------------------------

class StorageManager : public Component, public Alarmable, public Diagnosticable {
    const Config::StorageConfig& config;
    SPIFFSFile _file;
    ActivationTracker _activations;
    counter_t _failures = 0, _erasures = 0;

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
    void collectAlarms (AlarmSet& alarms) const override {
        if (_failures > config.failureLimit) alarms += ALARM_STORAGE_FAIL;
        if (_file.size () > config.lengthCritical) alarms += ALARM_STORAGE_SIZE;
    }
    void collectDiagnostics (JsonObject &obj) const override {
        JsonObject storage = obj ["storage"].to <JsonObject> ();
        JsonObject size = storage ["size"].to <JsonObject> ();
        size ["current"] = _file.size ();
        size ["maximum"] = config.lengthMaximum;
        size ["critical"] = config.lengthCritical;
        size ["erasures"] = _erasures;
        _activations.serialize (storage ["appended"].to <JsonObject> ());
    }
};

// -----------------------------------------------------------------------------------------------
