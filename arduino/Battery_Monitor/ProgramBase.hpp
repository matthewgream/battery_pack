
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
    virtual void collectDiagnostics (JsonDocument &) const = 0;
};

class DiagnosticManager : public Component {

public:
    typedef struct {
    } Config;

private:
    const Config &config;

    const Diagnosticable::List _diagnosticables;

public:
    DiagnosticManager (const Config& cfg, const Diagnosticable::List diagnosticables) : config (cfg), _diagnosticables (diagnosticables) {}
    void collect (JsonDocument &doc) const {
        for (const auto& diagnosticable : _diagnosticables)
            diagnosticable->collectDiagnostics (doc);
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
#define ALARM_DELIVER_SIZE          _ALARM_NUMB (9)
#define ALARM_UPDATE_VERS           _ALARM_NUMB (10)
#define _ALARM_COUNT                 (11)
static const char * _ALARM_NAMES [_ALARM_COUNT] = { "TIME_SYNC", "TIME_DRIFT", "TEMP_FAIL", "TEMP_MIN", "TEMP_WARN", "TEMP_MAX", "STORE_FAIL", "STORE_SIZE", "PUBLISH_FAIL", "DELIVER_SIZE", "UPDATE_VERS" };
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
    String toStringBitmap () const {
        String s;
        for (int number = 0; number < _ALARM_COUNT; number ++)
            s += (_alarms & _ALARM_NUMB (number)) ? '1' : '0';
        return s;
    }
    String toStringNames () const {
        String s;
        for (int number = 0; number < _ALARM_COUNT; number ++)
            s += (_alarms & _ALARM_NUMB (number)) ? (String (s.isEmpty () ? "" : ",") + String (_ALARM_NAME (number))) : "";
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
    AlarmManager (const Config& cfg, AlarmInterface_SinglePIN &interface, const Alarmable::List alarmables) : config (cfg), _interface (interface), _alarmables (alarmables) {}
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
            DEBUG_PRINTF ("AlarmManager::process: alarms: %s <-- %s\n", alarms.toStringBitmap ().c_str (), _alarms.toStringBitmap ().c_str ());
            _alarms = alarms;
        }
    }
    inline String getAlarmsAsBitmap () const { return _alarms.toStringBitmap (); }
    inline String getAlarmsAsString () const { return _alarms.toStringNames (); }

protected:
    void collectDiagnostics (JsonDocument &obj) const override { // XXX this is too large and needs reduction
        JsonObject alarms = obj ["alarms"].to <JsonObject> ();
        for (int number = 0; number < _alarms.size (); number ++) {
            JsonObject alarm = alarms [_alarms.name (number)].to <JsonObject> ();
            alarm ["cnt"] = _activations [number].number ();
            alarm ["now"] = _alarms.isSet (number) ? 1 : 0;
            alarm ["for"] = _alarms.isSet (number) ? _activations [number].seconds () : _deactivations [number].seconds ();
        }
    }
};

// -----------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------

#include <functional>

extern String ota_image_check (const String& json, const String& type, const String& vers);

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
    
    Intervalable _interval;

    bool _available = false;

public:
    UpdateManager (const Config& cfg, const BooleanFunc networkIsAvailable): config (cfg), _networkIsAvailable (std::move (networkIsAvailable)),
        _persistent_data ("updates"), _persistent_data_previous (_persistent_data, "previous", 0), _persistent_data_version (_persistent_data, "version", String ("")),
        _interval (config.intervalUpdate) {}
    void process () override {
        if (static_cast <bool> (_interval)) {
            if (_networkIsAvailable ()) {
                time_t previous = (time_t) static_cast <uint32_t> (_persistent_data_previous), current = time (NULL);
                if ((current > 0 && (current - previous) > (config.intervalCheck / 1000)) || (previous > current)) {
                    _persistent_data_version = ota_image_check (config.json, config.type, config.vers);
                    _persistent_data_previous = current;
                    _available = true;
                }
            }
        }
    }

protected:
    void collectAlarms (AlarmSet& alarms) const override {
        if (_available) alarms += ALARM_UPDATE_VERS;
    }
    void collectDiagnostics (JsonDocument &obj) const override {
        JsonObject updates = obj ["updates"].to <JsonObject> ();
        updates ["current"] = config.vers;
        updates ["available"] = static_cast <String> (_persistent_data_version);
        const time_t time = (time_t) static_cast <uint32_t> (_persistent_data_previous);
        if (time > 0)
            updates ["checked"] = getTimeString (time);
    }        
};

// -----------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------
