
// -----------------------------------------------------------------------------------------------

class Component {
protected:
    ~Component () {};
public:
    typedef std::vector <Component*> List;
    virtual void begin () {}
    virtual void process () {}
};

// -----------------------------------------------------------------------------------------------

class Diagnosticable {
protected:
    ~Diagnosticable () {};
public:
    typedef std::vector <Diagnosticable*> List;
    virtual void collect (JsonObject &obj) const = 0;
};

class DiagnosticManager : public Component {
    const Config::DiagnosticConfig& config;
    Diagnosticable::List _diagnosticables;
public:
    DiagnosticManager (const Config::DiagnosticConfig& cfg, const Diagnosticable::List diagnosticables) : config (cfg), _diagnosticables (diagnosticables) {}
    void collect (JsonDocument &doc) const {
        JsonObject obj = doc.to <JsonObject> ();
        for (const auto& diagnosticable : _diagnosticables)
            diagnosticable->collect (obj);
    }
};

// -----------------------------------------------------------------------------------------------

typedef uint32_t AlarmSet;
#define _ALARM_NUMB(x)               (1UL < (x))
#define ALARM_NONE                  (0UL)
#define ALARM_TEMPERATURE_MINIMAL   _ALARM_NUMB (0)
#define ALARM_TEMPERATURE_MAXIMAL   _ALARM_NUMB (1)
#define ALARM_STORAGE_FAIL          _ALARM_NUMB (2)
#define ALARM_STORAGE_SIZE          _ALARM_NUMB (3)
#define ALARM_TIME_DRIFT            _ALARM_NUMB (4)
#define ALARM_TIME_NETWORK          _ALARM_NUMB (5)
#define ALARM_COUNT                 (6)
#define ALARM_ACTIVE(a,x)           ((a) & _ALARM_NUMB (x))
static const char * _ALARM_NAME [] = { "TEMP_MIN", "TEMP_MAX", "STORE_FAIL", "STORE_SIZE", "TIME_DRIFT", "TIME_NETWORK" };
#define ALARM_NAME(x)               (_ALARM_NAME [x])

class Alarmable {
protected:
    ~Alarmable () {};
public:
    typedef std::vector <Alarmable*> List;
    virtual AlarmSet alarm () const = 0;
};

class AlarmManager : public Component, public Diagnosticable {
    const Config::AlarmConfig& config;
    Alarmable::List _alarmables;
    AlarmSet _alarms = ALARM_NONE;
    std::array <Upstamp, ALARM_COUNT> _counts;
public:
    AlarmManager (const Config::AlarmConfig& cfg, const Alarmable::List alarmables) : config (cfg), _alarmables (alarmables) {}
    void begin () override {
        pinMode (config.PIN_ALARM, OUTPUT);
    }
    void process () override {
        AlarmSet alarms = ALARM_NONE;
        for (const auto& alarmable : _alarmables)
            alarms |= alarmable->alarm ();
        if (alarms != _alarms) {
            for (int i = 0; i < ALARM_COUNT; i ++)
                if (ALARM_ACTIVE (alarms, i) && !ALARM_ACTIVE (_alarms, i))
                    _counts [i] ++;
            digitalWrite (config.PIN_ALARM, (alarms != ALARM_NONE) ? HIGH : LOW);
            _alarms = alarms;
        }
    }
    void collect (JsonObject &obj) const override {
        JsonObject alarm = obj ["alarm"].to <JsonObject> ();
        JsonArray alarms = alarm ["alarms"].to <JsonArray> ();
        for (int i = 0; i < ALARM_COUNT; i ++) {
            JsonObject entry = alarms.add <JsonObject> ();
            entry ["name"] = ALARM_NAME (i);
            entry ["active"] = ALARM_ACTIVE (_alarms, i);
            entry ["last"] = _counts [i].seconds ();
            entry ["numb"] = _counts [i].number ();
        }
    }
    //
    AlarmSet getAlarms () const { return _alarms; }
};

// -----------------------------------------------------------------------------------------------
