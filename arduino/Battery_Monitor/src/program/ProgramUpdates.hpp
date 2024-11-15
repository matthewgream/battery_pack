
// -----------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------

#include <functional>

extern bool ota_image_update (const String &json, const String &type, const String &vers, const String &addr, const std::function<void ()> &func = nullptr);
extern bool ota_image_check (const String &json, const String &type, const String &vers, const String &addr, String *newr);

class ProgramUpdates : public Component, public Alarmable, public Diagnosticable {
public:
    typedef struct {
        bool startupCheck, updateImmmediately;
        interval_t intervalCheck, intervalLong;
        String json, type, vers, addr;
    } Config;

    using BooleanFunc = std::function<bool ()>;

private:
    const Config &config;
    const BooleanFunc _networkIsAvailable;

    PersistentData _persistent_data;
    PersistentValue<uint32_t> _persistent_data_previous;
    PersistentValue<String> _persistent_data_version;
    IntervalableByPersistentTime _interval;
    Enableable _startupCheck;
    bool _available;

public:
    ProgramUpdates (const Config &cfg, const BooleanFunc networkIsAvailable) :
        Alarmable ({
            AlarmCondition (ALARM_UPDATE_VERS, [this] () { return _available; }),
            AlarmCondition (ALARM_UPDATE_LONG, [this] () { return istoolong (); }),
        }),
        config (cfg),
        _networkIsAvailable (networkIsAvailable),
        _persistent_data ("updates"),
        _persistent_data_previous (_persistent_data, "previous", 0),
        _persistent_data_version (_persistent_data, "version", String ()),
        _interval (config.intervalCheck, _persistent_data_previous),
        _startupCheck (! config.startupCheck),
        _available (! static_cast<String> (_persistent_data_version).isEmpty ()) { }

    void begin () override {
        if (_networkIsAvailable () && ! _startupCheck)
            checkForAndMaybeUpdate ();
    }
    void process () override {
        if (_networkIsAvailable () && (! _startupCheck || _interval))
            checkForAndMaybeUpdate ();
    }
    //
    bool istoolong () const {
        return _interval.interval () > config.intervalLong;
    }

protected:
    void checkForAndMaybeUpdate () {
        _startupCheck = true;
        _interval.reset ();
        if (config.updateImmmediately) {
            _available = ota_image_update (config.json, config.type, config.vers, config.addr, [&] () {
                _persistent_data_version = "";
            });
            // only _available == false
            _persistent_data_version = "";
        } else {
            String version;
            _available = ota_image_check (config.json, config.type, config.vers, config.addr, &version);
            _persistent_data_version = _available ? version : "";
        }
    }
    void collectDiagnostics (JsonVariant &obj) const override {
        JsonObject sub = obj ["updates"].to<JsonObject> ();
        sub ["current"] = config.vers;
        if (_available)
            sub ["available"] = static_cast<String> (_persistent_data_version);
        if (_persistent_data_previous)
            sub ["checked"] = getTimeString (static_cast<time_t> (_persistent_data_previous));
    }
};

// -----------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------
