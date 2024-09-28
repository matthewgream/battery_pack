
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
    virtual void collectDiagnostics (JsonDocument &) const = 0;
};

class DiagnosticManager : public Component {
    const Config::DiagnosticConfig& config;
    Diagnosticable::List _diagnosticables;

public:
    DiagnosticManager (const Config::DiagnosticConfig& cfg, const Diagnosticable::List diagnosticables) : config (cfg), _diagnosticables (diagnosticables) {}
    void collect (JsonDocument &doc) const {
        for (const auto& diagnosticable : _diagnosticables)
            diagnosticable->collectDiagnostics (doc);
    }
};

// -----------------------------------------------------------------------------------------------

typedef uint32_t _AlarmType;
#define _ALARM_NUMB(x)              ((_AlarmType) (1UL << (x)))
#define ALARM_NONE                  (0UL)
#define ALARM_TEMPERATURE_MINIMAL   _ALARM_NUMB (0)
#define ALARM_TEMPERATURE_MAXIMAL   _ALARM_NUMB (1)
#define ALARM_STORAGE_FAIL          _ALARM_NUMB (2)
#define ALARM_STORAGE_SIZE          _ALARM_NUMB (3)
#define ALARM_PUBLISH_FAIL          _ALARM_NUMB (4)
#define ALARM_TIME_DRIFT            _ALARM_NUMB (5)
#define ALARM_TIME_NETWORK          _ALARM_NUMB (6)
#define _ALARM_COUNT                 (7)
static const char * _ALARM_NAMES [_ALARM_COUNT] = { "TEMP_MIN", "TEMP_MAX", "STORE_FAIL", "STORE_SIZE", "PUBLISH_FAIL", "TIME_DRIFT", "TIME_NETWORK" };
#define _ALARM_NAME(x)               (_ALARM_NAMES [x])

class AlarmSet {
    _AlarmType _alarmSet = ALARM_NONE;

public:
    inline const char * name (const int number) const { return _ALARM_NAME (number); }
    inline size_t size () const { return _ALARM_COUNT; }
    inline bool isNone () const { return (_alarmSet == ALARM_NONE); }
    inline bool isAny (const int number) const { return (_alarmSet & _ALARM_NUMB (number)); }
    inline operator _AlarmType () const { return _alarmSet; }
    inline AlarmSet& operator+= (const _AlarmType alarm) { _alarmSet |= alarm; return *this; }
    inline AlarmSet& operator+= (const AlarmSet& alarmset) { _alarmSet |= alarmset._alarmSet; return *this; }
    String toStringBitmap () const {
        String s; 
        for (int number = 0; number < _ALARM_COUNT; number ++)
            s += (_alarmSet & _ALARM_NUMB (number)) ? '1' : '0';
        return s;
    }
    String toStringNames () const {
        String s; 
        for (int number = 0; number < _ALARM_COUNT; number ++)
            s += (_alarmSet & _ALARM_NUMB (number)) ? (String (s.isEmpty () ? "" : ",") + String (_ALARM_NAME (number))) : "";
        return s;
    }
};
inline bool operator== (const AlarmSet &a, const AlarmSet &b) { return (_AlarmType) a == (_AlarmType) b; }
inline bool operator!= (const AlarmSet &a, const AlarmSet &b) { return (_AlarmType) a != (_AlarmType) b; }

// -----------------------------------------------------------------------------------------------

class Alarmable {
protected:
    ~Alarmable () {};

public:
    typedef std::vector <Alarmable*> List;
    virtual void collectAlarms (AlarmSet& alarms) const = 0;
};

class AlarmManager : public Component, public Diagnosticable {
    const Config::AlarmConfig& config;
    Alarmable::List _alarmables;
    AlarmSet _alarms;
    std::array <ActivationTracker, _ALARM_COUNT> _activations, _deactivations;

public:
    AlarmManager (const Config::AlarmConfig& cfg, const Alarmable::List alarmables) : config (cfg), _alarmables (alarmables) {}
    void begin () override {
        if (config.PIN_ALARM > 0)
            pinMode (config.PIN_ALARM, OUTPUT);
    }
    void process () override {
        AlarmSet alarms;
        for (const auto& alarmable : _alarmables)
            alarmable->collectAlarms (alarms);
        if (alarms != _alarms) {
            for (int number = 0; number < alarms.size (); number ++)
                if (alarms.isAny (number) && !_alarms.isAny (number))
                    _activations [number] ++;
                else if (!alarms.isAny (number) && _alarms.isAny (number))
                    _deactivations [number] ++;
            if (config.PIN_ALARM > 0)
                digitalWrite (config.PIN_ALARM, alarms.isNone () ? LOW : HIGH);
            DEBUG_PRINTF ("AlarmManager::process: alarms: %s <-- %s\n", alarms.toStringBitmap ().c_str (), _alarms.toStringBitmap ().c_str ());
            _alarms = alarms;
        }
    }
    String getAlarmsAsBitmap () const { return _alarms.toStringBitmap (); }
    String getAlarmsAsString () const { return _alarms.toStringNames (); }

protected:
    void collectDiagnostics (JsonDocument &obj) const override { // XXX too large
        JsonArray alarms = obj ["alarms"].to <JsonArray> ();
        for (int number = 0; number < _alarms.size (); number ++) {
            JsonObject alarm = alarms.add <JsonObject> ();
            alarm ["@"] = _alarms.name (number);
            alarm ["="] = _alarms.isAny (number) ? 1 : 0;
            alarm ["#"] = _activations [number].number ();
            alarm ["+"] = _activations [number].seconds ();
            alarm ["-"] = _deactivations [number].seconds ();
        }
    }
};

// -----------------------------------------------------------------------------------------------
