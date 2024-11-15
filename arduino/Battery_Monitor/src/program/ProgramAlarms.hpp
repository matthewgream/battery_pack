// -----------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------

typedef uint32_t _AlarmType;
#define _ALARM_NUMB(x)            ((_AlarmType) (1UL << (x)))
#define ALARM_NONE                (0UL)
#define ALARM_TIME_SYNC           _ALARM_NUMB (0)
#define ALARM_TIME_DRIFT          _ALARM_NUMB (1)
#define ALARM_TEMPERATURE_FAILURE _ALARM_NUMB (2)
#define ALARM_TEMPERATURE_MINIMAL _ALARM_NUMB (3)
#define ALARM_TEMPERATURE_WARNING _ALARM_NUMB (4)
#define ALARM_TEMPERATURE_MAXIMAL _ALARM_NUMB (5)
#define ALARM_STORAGE_FAIL        _ALARM_NUMB (6)
#define ALARM_STORAGE_SIZE        _ALARM_NUMB (7)
#define ALARM_PUBLISH_FAIL        _ALARM_NUMB (8)
#define ALARM_PUBLISH_SIZE        _ALARM_NUMB (9)
#define ALARM_DELIVER_FAIL        _ALARM_NUMB (10)
#define ALARM_DELIVER_SIZE        _ALARM_NUMB (11)
#define ALARM_UPDATE_VERS         _ALARM_NUMB (12)
#define ALARM_UPDATE_LONG         _ALARM_NUMB (13)
#define ALARM_SYSTEM_MEMORYLOW    _ALARM_NUMB (14)
#define ALARM_SYSTEM_BADRESET     _ALARM_NUMB (15)
#define _ALARM_COUNT              (16)
static const char *_ALARM_NAMES [_ALARM_COUNT] = { "TIME_SYNC", "TIME_DRIFT", "TEMP_FAIL", "TEMP_MIN", "TEMP_WARN", "TEMP_MAX", "STORE_FAIL", "STORE_SIZE", "PUBLISH_FAIL", "PUBLISH_SIZE", "DELIVER_FAIL", "DELIVER_SIZE", "UPDATE_VERS", "UPDATE_LONG", "SYSTEM_MEMLOW", "SYSTEM_BADRESET" };
#define _ALARM_NAME(x) (_ALARM_NAMES [x])

class AlarmSet {
    _AlarmType _alarms = ALARM_NONE;

public:
    inline const char *name (const int number) const {
        return _ALARM_NAME (number);
    }
    inline size_t size () const {
        return _ALARM_COUNT;
    }
    inline bool isAny () const {
        return (_alarms != ALARM_NONE);
    }
    inline bool isSet (const int number) const {
        return (_alarms & _ALARM_NUMB (number));
    }
    inline operator const _AlarmType & () const {
        return _alarms;
    }
    inline AlarmSet &operator+= (const _AlarmType alarm) {
        _alarms |= alarm;
        return *this;
    }
    inline AlarmSet &operator+= (const AlarmSet &other) {
        _alarms |= other._alarms;
        return *this;
    }
    inline AlarmSet operator^ (const AlarmSet &other) const {
        AlarmSet result;
        result._alarms = _alarms ^ other._alarms;
        return result;
    }
    String toString () const {
        String s;
        for (int number = 0; number < _ALARM_COUNT; number++)
            s += (_alarms & _ALARM_NUMB (number)) ? (String (s.isEmpty () ? "" : ",") + String (_ALARM_NAME (number))) : "";
        return s;
    }
};
inline bool operator== (const AlarmSet &a, const AlarmSet &b) {
    return (_AlarmType) a == (_AlarmType) b;
}
inline bool operator!= (const AlarmSet &a, const AlarmSet &b) {
    return (_AlarmType) a != (_AlarmType) b;
}

// -----------------------------------------------------------------------------------------------

class AlarmCondition {
public:
    using CheckFunction = std::function<bool ()>;

private:
    _AlarmType _type;
    const CheckFunction _checkFunc;

public:
    AlarmCondition (_AlarmType type, const CheckFunction checkFunc) :
        _type (type),
        _checkFunc (checkFunc) { }
    inline bool check () const {
        return _checkFunc ();
    }
    inline _AlarmType getType () const {
        return _type;
    }
};

class Alarmable {
protected:
    const std::vector<AlarmCondition> _conditions;

public:
    Alarmable (const std::initializer_list<AlarmCondition> &conditions) :
        _conditions (conditions) { }
    virtual ~Alarmable () {};
    void collectAlarms (AlarmSet &alarms) const {
        for (const auto &condition : _conditions)
            if (condition.check ())
                alarms += condition.getType ();
    }
};

class ProgramAlarms : public Component, public Diagnosticable {
public:
    typedef struct {
    } Config;

    using List = std::vector<const Alarmable *>;

private:
    const Config &config;

    ProgramAlarmsInterface &_interface;
    const List _alarmables;

    AlarmSet _alarms;
    std::array<ActivationTracker, _ALARM_COUNT> _activations, _deactivations;

public:
    ProgramAlarms (const Config &cfg, ProgramAlarmsInterface &interface, const List &alarmables) :
        config (cfg),
        _interface (interface),
        _alarmables (alarmables) { }
    void process () override {
        AlarmSet alarms;
        for (const auto &alarmable : _alarmables)
            alarmable->collectAlarms (alarms);
        AlarmSet changes = alarms ^ _alarms;
        if (changes.isAny ()) {
            for (int number = 0; number < alarms.size (); number++)
                if (changes.isSet (number))
                    (alarms.isSet (number) ? _activations [number] : _deactivations [number])++;
            _interface.set (alarms.isAny ());
            DEBUG_PRINTF ("AlarmManager::process: alarms: %s <-- %s\n", alarms.toString ().c_str (), _alarms.toString ().c_str ());
            _alarms = alarms;
        }
    }
    inline String toString () const {
        return _alarms.toString ();
    }

protected:
    void collectDiagnostics (JsonVariant &obj) const override {    // XXX this is too large and needs reduction
        JsonObject sub = obj ["alarms"].to<JsonObject> ();
        for (int number = 0; number < _alarms.size (); number++) {
            const int _now = _alarms.isSet (number) ? 1 : 0;
            const counter_t _cnt = _activations [number];
            if (_now || _cnt > 0) {
                JsonObject alarm = sub [_alarms.name (number)].to<JsonObject> ();
                alarm ["now"] = _now;
                alarm ["for"] = _now ? _activations [number].seconds () : _deactivations [number].seconds ();
                if (_cnt > 0)
                    alarm ["cnt"] = _cnt;
            }
        }
    }
};

// -----------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------
