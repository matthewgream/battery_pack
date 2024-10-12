
// -----------------------------------------------------------------------------------------------
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
    explicit DeliverManager (const Config& cfg): Alarmable ({
            AlarmCondition (ALARM_DELIVER_SIZE, [this] () { return _blue.mtuexceeded (); })
        }), config (cfg), _blue (cfg.blue) {}
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
    void collectDiagnostics (JsonDocument &obj) const override {
        JsonObject sub = obj ["deliver"].to <JsonObject> ();
        if (_delivers.number () > 0)
            _delivers.serialize (sub ["delivers"].to <JsonObject> ());
        _blue.serialize (sub);
    }
};

// -----------------------------------------------------------------------------------------------
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
    explicit PublishManager (const Config& cfg, const BooleanFunc networkIsAvailable): Alarmable ({
            AlarmCondition (ALARM_PUBLISH_FAIL, [this] () { return _failures > config.failureLimit; })
        }), config (cfg), _networkIsAvailable (std::move (networkIsAvailable)), _mqtt (cfg.mqtt) {}
    void begin () override {
        _mqtt.setup ();
    }
    void process () override {
        if (_networkIsAvailable ())
            _mqtt.process ();
    }
    bool publish (const String& data, const String& subtopic) {
        if (_networkIsAvailable ()) {
          if (_mqtt.connected () && _mqtt.publish (config.mqtt.topic + "/" + subtopic, data)) {
              _publishes += IntToString (data.length ()), _failures = 0;
              return true;
          } else _failures ++;
        }
        return false;
    }
    inline bool connected () { return _networkIsAvailable () && _mqtt.connected (); }

protected:
    void collectDiagnostics (JsonDocument &obj) const override {
        JsonObject sub = obj ["publish"].to <JsonObject> ();
        if (_publishes.number () > 0)
            _publishes.serialize (sub ["publishes"].to <JsonObject> ());
        if (_failures > 0) {
            JsonObject failures = sub ["failures"].to <JsonObject> ();
            failures ["count"] = _failures;
            failures ["limit"] = config.failureLimit;
        }
        _mqtt.serialize (sub);
    }
};

// -----------------------------------------------------------------------------------------------
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
    explicit StorageManager (const Config& cfg): Alarmable ({
            AlarmCondition (ALARM_STORAGE_FAIL, [this] () { return _failures > config.failureLimit; }),
            AlarmCondition (ALARM_STORAGE_SIZE, [this] () { return _file.remains () < config.remainLimit; })
        }), config (cfg), _file (config.filename) {}
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
    void collectDiagnostics (JsonDocument &obj) const override {
        JsonObject sub = obj ["storage"].to <JsonObject> ();
        sub ["critical"] = config.remainLimit;
        if (_erasures > 0)
            sub ["erasures"] = _erasures;
        if (_failures > 0) {
            JsonObject failures = sub ["failures"].to <JsonObject> ();
            failures ["count"] = _failures;
            failures ["limit"] = config.failureLimit;
        }
        if (_appends.number () > 0)
            _appends.serialize (sub ["appends"].to <JsonObject> ());
        _file.serialize (sub);
    }
};

// -----------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------
