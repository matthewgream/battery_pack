
// -----------------------------------------------------------------------------------------------

class DeliverManager : public Component, public Alarmable, public Diagnosticable {
    const Config::DeliverConfig& config;
    BluetoothNotifier _blue;
    ActivationTrackerWithDetail _delivers;

public:
    DeliverManager (const Config::DeliverConfig& cfg) : config (cfg), _blue (cfg.blue) {}
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
    const Config::PublishConfig& config;
    ConnectManager &_network;
    MQTTPublisher _mqtt;
    ActivationTrackerWithDetail _publishes;
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
              _publishes += IntToString (data.length ()), _failures = 0;
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
    void collectDiagnostics (JsonDocument &obj) const override {
        JsonObject publish = obj ["publish"].to <JsonObject> ();
        _publishes.serialize (publish ["publishes"].to <JsonObject> ());
        _mqtt.serialize (publish);
    }
};

// -----------------------------------------------------------------------------------------------

class StorageManager : public Component, public Alarmable, public Diagnosticable {
    const Config::StorageConfig& config;
    SPIFFSFile _file;
    ActivationTrackerWithDetail _appends;
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
    bool append (const String& data) {
        if (_file.append (data)) {
            _appends += IntToString (data.length ()), _failures = 0;
            return true;
        } else _failures ++;
        return false;
    }
    bool retrieve (LineCallback& callback) { // XXX const
        return _file.read (callback);
    }
    void erase () {
        _erasures ++;
        _file.erase ();
    }

protected:
    void collectAlarms (AlarmSet& alarms) const override {
        if (_failures > config.failureLimit) alarms += ALARM_STORAGE_FAIL;
        if ((long) _file.size () > (long) config.lengthCritical) alarms += ALARM_STORAGE_SIZE;
    }
    void collectDiagnostics (JsonDocument &obj) const override {
        JsonObject storage = obj ["storage"].to <JsonObject> ();
        JsonObject size = storage ["size"].to <JsonObject> ();
        size ["current"] = _file.size ();
        size ["maximum"] = config.lengthMaximum;
        size ["critical"] = config.lengthCritical;
        size ["erasures"] = _erasures;
        _appends.serialize (storage ["appends"].to <JsonObject> ());
        _file.serialize (storage);
    }
};

// -----------------------------------------------------------------------------------------------
