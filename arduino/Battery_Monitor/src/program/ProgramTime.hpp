
// -----------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------

class ProgramTime : public Component, public Alarmable, public Diagnosticable {
public:
    typedef struct {
        Hardware_AnalogDevicesDS3231::Config hardware;
        String useragent, server;
        interval_t intervalUpdate, intervalAdjust;
        int failureLimit;
    } Config;

    using BooleanFunc = std::function<bool ()>;

private:
    const Config &config;
    const BooleanFunc _networkIsAvailable;

    Hardware_AnalogDevicesDS3231 _hardware;

    NetworkTimeFetcher _fetcher;
    ActivationTrackerWithDetail _fetches;
    ActivationTracker _failures;

    PersistentData _persistentData;
    PersistentValue<long> _persistentDrift;
    TimeDriftCalculator _drifter;

    time_t _fetchedTime = 0;
    PersistentValue<uint32_t> _persistentTime;
    Intervalable _intervalUpdate, _intervalAdjust;

public:
    explicit ProgramTime (const Config &cfg, TwoWire& wire, const BooleanFunc networkIsAvailable) :
        Alarmable ({ AlarmCondition (ALARM_TIME_SYNC, [this] () { return _failures > config.failureLimit; }),
                     AlarmCondition (ALARM_TIME_DRIFT, [this] () { return _drifter.highDrift () != 0; }) }),
        config (cfg),
        _networkIsAvailable (networkIsAvailable),
        _hardware (cfg.hardware, wire),
        _fetcher (cfg.useragent, cfg.server),
        _persistentData ("nettime"),
        _persistentDrift (_persistentData, "drift", 0),
        _drifter (_persistentDrift),
        _persistentTime (_persistentData, "time", 0),
        _intervalUpdate (config.intervalUpdate),
        _intervalAdjust (config.intervalAdjust) {
        if (_persistentTime > 0UL) {
            struct timeval tv = { .tv_sec = _persistentTime, .tv_usec = 0 };
            settimeofday (&tv, nullptr);
        }
        DEBUG_PRINTF ("NettimeManager::constructor: persistentTime=%lu, persistentDrift=%ld, time=%s\n", (unsigned long) _persistentTime, (long) _persistentDrift, getTimeString ().c_str ());
    }

    void process () override {
        interval_t interval;

        if (_networkIsAvailable () && _intervalUpdate.passed (&interval, true)) {
            const time_t fetchedTime = _fetcher.fetch ();
            if (fetchedTime > 0) {
                _fetches += ArithmeticToString (fetchedTime);
                const struct timeval tv = { .tv_sec = fetchedTime, .tv_usec = 0 };
                settimeofday (&tv, nullptr);
                if (_fetchedTime > 0)
                    _persistentDrift = _drifter.updateDrift (fetchedTime - _fetchedTime, interval);
                _fetchedTime = fetchedTime;
                _persistentTime = (uint32_t) fetchedTime;
                _failures = 0;
                DEBUG_PRINTF ("NettimeManager::process: time=%s\n", getTimeString ().c_str ());
            } else
                _failures++;
        }

        if (_intervalAdjust.passed (&interval)) {
            struct timeval tv;
            gettimeofday (&tv, nullptr);
            if (_drifter.applyDrift (tv, interval) > 0) {
                settimeofday (&tv, nullptr);
                _persistentTime = tv.tv_sec;
            }
        }
    }
    //
    void collectDiagnostics (JsonVariant &obj) const override {
        JsonObject sub = obj ["time"].to<JsonObject> ();
        sub ["now"] = getTimeString ();
        sub ["drift"] = _drifter.drift ();
        if (_drifter.highDrift () != 0)
            sub ["highdrift"] = _drifter.highDrift ();
        if (_fetches)
            sub ["fetches"] = _fetches;
        if (_failures)
            sub ["failures"] = _fetches;
    }
};

// -----------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------
