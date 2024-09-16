
// -----------------------------------------------------------------------------------------------

class DeliverManager : public Component, public Diagnosticable {
    const Config::DeliverConfig& config;
    BluetoothNotifier _bluetooth;
    Upstamp _last;
public:
    DeliverManager (const Config::DeliverConfig& cfg) : config (cfg) {}
    void begin () override {
        _bluetooth.advertise (config.name, config.service, config.characteristic);
    }
    void collect (JsonObject &obj) const override {
        JsonObject deliver = obj ["deliver"].to <JsonObject> ();
        JsonObject blue = deliver ["blue"].to <JsonObject> ();
        if ((blue ["connected"] = _bluetooth.connected ())) {
            blue ["address"] = _bluetooth.address ();
            blue ["devices"] = _bluetooth.devices ();
        }
        JsonObject deliver2 = deliver ["deliver"].to <JsonObject> ();
        deliver2 ["last"] = _last.seconds ();
        deliver2 ["numb"] = _last.number ();
    }
    void deliver (const String& data) {
        if (_bluetooth.connected ()) {
            _last ++;
            _bluetooth.notify (data);
        }
    }
};

// -----------------------------------------------------------------------------------------------

class PublishManager : public Component, public Diagnosticable {
    const Config::PublishConfig& config;
    MQTTPublisher _mqtt;
    Upstamp _last;
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
        _last ++;
        return true;
    }

    void collect (JsonObject &obj) const override {
        JsonObject publish = obj ["publish"].to <JsonObject> ();
        JsonObject mqtt = publish ["mqtt"].to <JsonObject> ();
        if ((mqtt ["connected"] = const_cast <MQTTPublisher *> (&_mqtt)->connected ())) {
        }
        mqtt ["state"] = const_cast <MQTTPublisher *> (&_mqtt)->state ();
        JsonObject publish2 = publish ["publish"].to <JsonObject> ();
        publish2 ["last"] = _last.seconds ();
        publish2 ["numb"] = _last.number ();
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
    Upstamp _last;
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
        JsonObject storage2 = storage ["storage"].to <JsonObject> ();
        storage2 ["last"] = _last.seconds ();
        storage2 ["numb"] = _last.number ();
    }
    //
    AlarmSet alarm () const override {
        AlarmSet alarms = ALARM_NONE;
        if (_failures > config.failureLimit) alarms |= ALARM_STORAGE_FAIL;
        if (_file.size () > config.lengthCritical) alarms |= ALARM_STORAGE_SIZE;
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
            _last ++;
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
