
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

typedef uint32_t _AlarmType;
#define _ALARM_NUMB(x)              ((_AlarmType) (1UL < (x)))
#define ALARM_NONE                  (0UL)
#define ALARM_TEMPERATURE_MINIMAL   _ALARM_NUMB (0)
#define ALARM_TEMPERATURE_MAXIMAL   _ALARM_NUMB (1)
#define ALARM_STORAGE_FAIL          _ALARM_NUMB (2)
#define ALARM_STORAGE_SIZE          _ALARM_NUMB (3)
#define ALARM_TIME_DRIFT            _ALARM_NUMB (4)
#define ALARM_TIME_NETWORK          _ALARM_NUMB (5)
#define _ALARM_COUNT                 (6)
static const char * _ALARM_NAMES [] = { "TEMP_MIN", "TEMP_MAX", "STORE_FAIL", "STORE_SIZE", "TIME_DRIFT", "TIME_NETWORK" };
#define _ALARM_NAME(x)               (_ALARM_NAMES [x])

class AlarmSet {
protected:
    _AlarmType _alarmSet = ALARM_NONE;
public:
    inline const char * name (const int number) const { return _ALARM_NAME (number); }
    inline size_t size () const { return _ALARM_COUNT; }
    inline bool isNone () const { return (_alarmSet == ALARM_NONE); }
    inline bool isActive (const int number) const { return (_alarmSet & _ALARM_NUMB (number)); }
    inline operator _AlarmType () const { return _alarmSet; }
    inline AlarmSet& operator+= (const _AlarmType alarm) { _alarmSet |= alarm; return *this; }
    inline AlarmSet& operator+= (const AlarmSet& alarmset) { _alarmSet |= alarmset._alarmSet; return *this; }
    String toStringBitmap () const {
        String s; 
        for (int number = 0; number < _ALARM_COUNT; number ++)
            s += (_alarmSet & _ALARM_NUMB (number)) ? '1' : '0';
        return s;
    }
};

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
    AlarmSet _alarms;
    std::array <ActivationTracker, _ALARM_COUNT> _activations, _deactivations;
public:
    AlarmManager (const Config::AlarmConfig& cfg, const Alarmable::List alarmables) : config (cfg), _alarmables (alarmables) {}
    void begin () override {
        pinMode (config.PIN_ALARM, OUTPUT);
    }
    void process () override {
        AlarmSet alarms;
        for (const auto& alarmable : _alarmables)
            alarms += alarmable->alarm ();
        if (alarms != _alarms) {
            for (int number = 0; number < alarms.size (); number ++)
                if (alarms.isActive (number) && !_alarms.isActive (number))
                    _activations [number] ++;
                else if (!alarms.isActive (number) && _alarms.isActive (number))
                    _deactivations [number] ++;
            digitalWrite (config.PIN_ALARM, alarms.isNone () ? LOW : HIGH);
            _alarms = alarms;
        }
    }
    void collect (JsonObject &obj) const override {
        JsonObject alarm = obj ["alarm"].to <JsonObject> ();
        JsonArray alarms = alarm ["alarms"].to <JsonArray> ();
        for (int number = 0; number < _alarms.size (); number ++) {
            JsonObject entry = alarms.add <JsonObject> ();
            entry ["name"] = _alarms.name (number);
            entry ["active"] = _alarms.isActive (number);
            JsonObject activations = entry ["activations"].to <JsonObject> ();            
            _activations [number].serialize (activations);
            JsonObject deactivations = entry ["deactivations"].to <JsonObject> ();            
            _deactivations [number].serialize (deactivations);
        }
    }
    //
    String getAlarms () const { return _alarms.toStringBitmap (); }
};

// -----------------------------------------------------------------------------------------------
