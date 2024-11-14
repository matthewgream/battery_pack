
// XXX this is all a bit of a mess at the moment ...

// -----------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------

class ProgramDataDeliver : public Component, public Alarmable, public Diagnosticable {
public:
    typedef struct {
        String topic;
        counter_t failureLimit;
    } Config;

private:
    const Config &config;

    const String _id;
    BluetoothServer &_blue;
    MQTTClient &_mqtt;
    WebSocket &_webs;
    ActivationTrackerWithDetail _delivers;
    ActivationTracker _failures;

public:
    explicit ProgramDataDeliver (const Config &cfg, const String &id, BluetoothServer &blue, MQTTClient &mqtt, WebSocket &webs) :
        Alarmable ({ AlarmCondition (ALARM_DELIVER_FAIL, [this] () { return _failures > config.failureLimit; }),
                     AlarmCondition (ALARM_DELIVER_SIZE, [this] () { return _blue.payloadExceeded (); }) }),
        config (cfg),
        _id (id),
        _blue (blue),
        _mqtt (mqtt),
        _webs (webs) { }

    bool available () {
        return _blue.available () || _webs.available () || _mqtt.available ();
    }
    //
    bool deliver (const String &data, const String &type, bool willPublishToMqtt) {
        if ((_blue.available () && _blue.send (data)) || (_webs.available () && _webs.send (data)) || (willPublishToMqtt || (_mqtt.available () && _mqtt.publish (config.topic + "/" + _id + "/" + type, data)))) {
            _delivers += ArithmeticToString (data.length ());
            _failures = 0;
            return true;
        } else
            _failures++;
        return false;
    }

protected:
    void collectDiagnostics (JsonVariant &obj) const override {
        JsonObject sub = obj ["deliver"].to<JsonObject> ();
        if (_delivers)
            sub ["delivers"] = _delivers;
        if (_failures) {
            sub ["failures"] = _failures;
            JsonObject failures = sub ["failures"].as<JsonObject> ();
            failures ["limit"] = config.failureLimit;
        }
    }
};

// -----------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------

class ProgramDataPublish : public Component, public Alarmable, public Diagnosticable {
public:
    typedef struct {
        String topic;
        counter_t failureLimit;
    } Config;

    using BooleanFunc = std::function<bool ()>;

private:
    const Config &config;

    const String _id;
    MQTTClient &_mqtt;
    ActivationTrackerWithDetail _publishes;
    ActivationTracker _failures;

public:
    explicit ProgramDataPublish (const Config &cfg, const String &id, MQTTClient &mqtt) :
        Alarmable ({ AlarmCondition (ALARM_PUBLISH_FAIL, [this] () { return _failures > config.failureLimit; }),
                     AlarmCondition (ALARM_PUBLISH_SIZE, [this] () { return _mqtt.bufferExceeded (); }) }),
        config (cfg),
        _id (id),
        _mqtt (mqtt) { }

    bool available () {
        return _mqtt.available ();
    }
    //
    bool publish (const String &data, const String &type) {
        if (_mqtt.publish (config.topic + "/" + _id + "/" + type, data)) {
            _publishes += ArithmeticToString (data.length ());
            _failures = 0;
            return true;
        } else
            _failures++;
        return false;
    }

protected:
    void collectDiagnostics (JsonVariant &obj) const override {
        JsonObject sub = obj ["publish"].to<JsonObject> ();
        if (_publishes)
            sub ["publishes"] = _publishes;
        if (_failures) {
            sub ["failures"] = _failures;
            JsonObject failures = sub ["failures"].as<JsonObject> ();
            failures ["limit"] = config.failureLimit;
        }
    }
};

// -----------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------

class ProgramDataStorage : public Component, public Alarmable, public Diagnosticable {
public:
    typedef struct {
        String filename;
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
    explicit ProgramDataStorage (const Config &cfg) :
        Alarmable ({ AlarmCondition (ALARM_STORAGE_FAIL, [this] () { return _failures > config.failureLimit; }),
                     AlarmCondition (ALARM_STORAGE_SIZE, [this] () { return _file.remains () < config.remainLimit; }) }),
        config (cfg),
        _file (config.filename) { }

    void begin () override {
        if (! _file.begin ())
            _failures++;
    }
    bool available () const {
        return _file.available ();
    }
    //
    long size () const {
        return _file.size ();
    }
    bool append (const String &data) {
        if (_file.append (data)) {
            _appends += ArithmeticToString (data.length ());
            _failures = 0;
            return true;
        } else
            _failures++;
        return false;
    }
    bool retrieve (LineCallback &callback) {    // XXX const
        return _file.read (callback);
    }
    void erase () {
        _file.erase ();
        _erasures++;
    }

protected:
    void collectDiagnostics (JsonVariant &obj) const override {
        JsonObject sub = obj ["storage"].to<JsonObject> ();
        sub ["critical"] = config.remainLimit;
        if (_appends)
            sub ["appends"] = _appends;
        if (_failures) {
            sub ["failures"] = _failures;
            JsonObject failures = sub ["failures"].as<JsonObject> ();
            failures ["limit"] = config.failureLimit;
        }
        if (_erasures)
            sub ["erasures"] = _erasures;
        sub ["file"] = _file;
    }
};

// -----------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------
