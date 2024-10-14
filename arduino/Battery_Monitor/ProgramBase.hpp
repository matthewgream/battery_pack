
// -----------------------------------------------------------------------------------------------
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
// -----------------------------------------------------------------------------------------------

class Diagnosticable {
protected:
    ~Diagnosticable () {};

public:
    typedef std::vector <Diagnosticable*> List;
    virtual void collectDiagnostics (JsonVariant &) const = 0;
};

class DiagnosticManager : public Component {
public:
    typedef struct {
    } Config;

private:
    const Config &config;

    const Diagnosticable::List _diagnosticables;

public:
    DiagnosticManager (const Config& cfg, const Diagnosticable::List& diagnosticables) : config (cfg), _diagnosticables (diagnosticables) {}
    void collect (JsonVariant &obj) const {
        for (const auto& diagnosticable : _diagnosticables)
            diagnosticable->collectDiagnostics (obj);
    }
};

// -----------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------

typedef uint32_t _AlarmType;
#define _ALARM_NUMB(x)              ((_AlarmType) (1UL << (x)))
#define ALARM_NONE                  (0UL)
#define ALARM_TIME_SYNC             _ALARM_NUMB (0)
#define ALARM_TIME_DRIFT            _ALARM_NUMB (1)
#define ALARM_TEMPERATURE_FAILURE   _ALARM_NUMB (2)
#define ALARM_TEMPERATURE_MINIMAL   _ALARM_NUMB (3)
#define ALARM_TEMPERATURE_WARNING   _ALARM_NUMB (4)
#define ALARM_TEMPERATURE_MAXIMAL   _ALARM_NUMB (5)
#define ALARM_STORAGE_FAIL          _ALARM_NUMB (6)
#define ALARM_STORAGE_SIZE          _ALARM_NUMB (7)
#define ALARM_PUBLISH_FAIL          _ALARM_NUMB (8)
#define ALARM_PUBLISH_SIZE          _ALARM_NUMB (9)
#define ALARM_DELIVER_FAIL          _ALARM_NUMB (10)
#define ALARM_DELIVER_SIZE          _ALARM_NUMB (11)
#define ALARM_UPDATE_VERS           _ALARM_NUMB (12)
#define ALARM_SYSTEM_MEMORYLOW      _ALARM_NUMB (13)
#define ALARM_SYSTEM_BADRESET       _ALARM_NUMB (14)
#define _ALARM_COUNT                 (15)
static const char * _ALARM_NAMES [_ALARM_COUNT] = { "TIME_SYNC", "TIME_DRIFT", "TEMP_FAIL", "TEMP_MIN", "TEMP_WARN", "TEMP_MAX", "STORE_FAIL", "STORE_SIZE", "PUBLISH_FAIL", "PUBLISH_SIZE", "DELIVER_FAIL", "DELIVER_SIZE", "UPDATE_VERS", "SYSTEM_MEMLOW", "SYSTEM_BADRESET" };
#define _ALARM_NAME(x)               (_ALARM_NAMES [x])

class AlarmSet {
    _AlarmType _alarms = ALARM_NONE;

public:
    inline const char * name (const int number) const { return _ALARM_NAME (number); }
    inline size_t size () const { return _ALARM_COUNT; }
    inline bool isAny () const { return (_alarms != ALARM_NONE); }
    inline bool isSet (const int number) const { return (_alarms & _ALARM_NUMB (number)); }
    inline operator const _AlarmType& () const { return _alarms; }
    inline AlarmSet& operator+= (const _AlarmType alarm) { _alarms |= alarm; return *this; }
    inline AlarmSet& operator+= (const AlarmSet& other) { _alarms |= other._alarms; return *this; }
    inline AlarmSet operator^ (const AlarmSet& other) const { AlarmSet result; result._alarms = _alarms ^ other._alarms; return result; }
    String toString () const {
        String s;
        for (int number = 0; number < _ALARM_COUNT; number ++)
            s += (_alarms & _ALARM_NUMB (number)) ? (String (s.isEmpty () ? "" : ",") + String (_ALARM_NAME (number))) : "";
        return s;
    }
};
inline bool operator== (const AlarmSet &a, const AlarmSet &b) { return (_AlarmType) a == (_AlarmType) b; }
inline bool operator!= (const AlarmSet &a, const AlarmSet &b) { return (_AlarmType) a != (_AlarmType) b; }

// -----------------------------------------------------------------------------------------------

class AlarmCondition {
public:
    using CheckFunction = std::function <bool ()>;

private:
    _AlarmType _type;
    const CheckFunction _checkFunc;

public:
    AlarmCondition (_AlarmType type, const CheckFunction checkFunc): _type (type), _checkFunc (checkFunc) {}
    inline bool check () const { return _checkFunc (); }
    inline _AlarmType getType () const { return _type; }
};

class Alarmable {
protected:
    const std::vector <AlarmCondition> _conditions;

public:
    typedef std::vector <Alarmable*> List;
    Alarmable (const std::initializer_list <AlarmCondition>& conditions): _conditions (conditions) {}
    virtual ~Alarmable() {};
    void collectAlarms (AlarmSet& alarms) const {
        for (const auto& condition : _conditions)
            if (condition.check ())
                alarms += condition.getType ();
    }
};

class AlarmManager : public Component, public Diagnosticable {
public:
    typedef struct {
    } Config;

private:
    const Config &config;

    AlarmInterface_SinglePIN &_interface;
    const Alarmable::List _alarmables;

    AlarmSet _alarms;
    std::array <ActivationTracker, _ALARM_COUNT> _activations, _deactivations;

public:
    AlarmManager (const Config& cfg, AlarmInterface_SinglePIN &interface, const Alarmable::List& alarmables) : config (cfg), _interface (interface), _alarmables (alarmables) {}
    void process () override {
        AlarmSet alarms;
        for (const auto& alarmable : _alarmables)
            alarmable->collectAlarms (alarms);
        AlarmSet changes = alarms ^ _alarms;
        if (changes.isAny ()) {
            for (int number = 0; number < alarms.size (); number ++)
                if (changes.isSet (number))
                    (alarms.isSet (number) ? _activations [number] : _deactivations [number]) ++;
            _interface.set (alarms.isAny ());
            DEBUG_PRINTF ("AlarmManager::process: alarms: %s <-- %s\n", alarms.toString ().c_str (), _alarms.toString ().c_str ());
            _alarms = alarms;
        }
    }
    inline String toString () const { return _alarms.toString (); }

protected:
    void collectDiagnostics (JsonVariant &obj) const override { // XXX this is too large and needs reduction
        JsonObject sub = obj ["alarms"].to <JsonObject> ();
            for (int number = 0; number < _alarms.size (); number ++) {
                const int _now = _alarms.isSet (number) ? 1 : 0;
                const counter_t _cnt = _activations [number];
                if (_now || _cnt > 0) {
                    JsonObject alarm = sub [_alarms.name (number)].to <JsonObject> ();
                    alarm ["now"] = _now;
                    alarm ["for"] = _now ? _activations [number].seconds () : _deactivations [number].seconds ();
                    if (_cnt > 0) alarm ["cnt"] = _cnt;
                }
            }
    }
};

// -----------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------

#include <functional>

extern String ota_image_check (const String& json, const String& type, const String& vers, const String& addr);

class UpdateManager: public Component, public Alarmable, public Diagnosticable {
public:
    typedef struct {
        interval_t intervalUpdate;
        time_t intervalCheck;
        String json, type, vers;
    } Config;

    using BooleanFunc = std::function <bool ()>;

private:
    const Config& config;
    const BooleanFunc _networkIsAvailable;

    PersistentData _persistent_data;
    PersistentValue <uint32_t> _persistent_data_previous;
    PersistentValue <String> _persistent_data_version;
    bool _available;

    Intervalable _interval;

public:
    UpdateManager (const Config& cfg, const BooleanFunc networkIsAvailable): Alarmable ({
            AlarmCondition (ALARM_UPDATE_VERS, [this] () { return _available; })
        }), config (cfg), _networkIsAvailable (networkIsAvailable),
        _persistent_data ("updates"), _persistent_data_previous (_persistent_data, "previous", 0), _persistent_data_version (_persistent_data, "version", String ("")),
        _available (!static_cast <String> (_persistent_data_version).isEmpty ()),
        _interval (config.intervalUpdate) {}
    void process () override {
        if (static_cast <bool> (_interval)) {
            if (_networkIsAvailable ()) {
                time_t previous = (time_t) static_cast <uint32_t> (_persistent_data_previous), current = time (nullptr);
                if ((current > 0 && (current - previous) > (config.intervalCheck / 1000)) || (previous > current)) {
                    String version = ota_image_check (config.json, config.type, config.vers, getMacAddress ());
                    if (_persistent_data_version != version) {
                        _persistent_data_version = version;
                        _available = !version.isEmpty ();
                    }
                    _persistent_data_previous = current;
                }
            }
        }
    }

protected:
    void collectDiagnostics (JsonVariant &obj) const override {
        JsonObject sub = obj ["updates"].to <JsonObject> ();
            sub ["current"] = config.vers;
            if (_available)
                sub ["available"] = static_cast <String> (_persistent_data_version);
            const time_t time = (time_t) static_cast <uint32_t> (_persistent_data_previous);
            if (time)
                sub ["checked"] = getTimeString (time);
    }
};

// -----------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------
