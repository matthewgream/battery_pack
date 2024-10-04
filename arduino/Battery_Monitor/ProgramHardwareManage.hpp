
// -----------------------------------------------------------------------------------------------

class TemperatureManagerBatterypack: public Component, public Alarmable, public Diagnosticable {

public:
    static inline constexpr int PROBE_COUNT = 15; // config.temperature.PROBE_NUMBER - 1

    using ChannelList = std::array <int, PROBE_COUNT>;
    typedef struct {
        ChannelList channels;
        float SETPOINT;
        float FAILURE, MINIMAL, WARNING, MAXIMAL;
    } Config;

private:
    const Config& config;

    TemperatureInterface& _interface;
    std::array <MovingAverageWithValue <float, 16>, PROBE_COUNT> _values;
    using AggregateValue = Stats <float>;
    AggregateValue _value; // change to compute something more sophisticated
    std::array <Stats <float>, PROBE_COUNT> _statsValues;
    Stats <float> _statsValueAvg, _statsValueMin, _statsValueMax;

public:
    TemperatureManagerBatterypack (const Config& cfg, TemperatureInterface& interface): config (cfg), _interface (interface), _values () {
        _values.fill (MovingAverageWithValue <float, 16> (round2places));      
    };
    void process () override {
        _value.reset ();
        int cnt = 0;
        DEBUG_PRINTF ("TemperatureManagerBatterypack::process: temps=[");
        for (const auto channel : config.channels) {
              const float tmp = _interface.getTemperature (channel), val = (_values [cnt] = tmp);
              _statsValues [cnt] += val;
              _value += val;
              DEBUG_PRINTF ("%s%.2f<%.2f", (cnt > 0) ? ", " : "", val, tmp);
              cnt ++;
        }
        const float avg = round2places (_value.avg ()), min = _value.min (), max = _value.max ();
        DEBUG_PRINTF ("], avg=%.2f, min=%.2f, max=%.2f\n", avg, min, max);
        _statsValueAvg += avg; _statsValueMin += min; _statsValueMax += max;
    }
    inline float min () const { return _value.min (); }
    inline float max () const { return _value.max (); }
    inline float avg () const { return round2places (_value.avg ()); }
    using TemperatureArray = std::array <float, PROBE_COUNT>;
    inline const TemperatureArray getTemperatures () const { 
        TemperatureArray result;
        auto it = result.begin ();
        for (const auto& value : _values)
            *it ++ = static_cast <float> (value);
        return result;        
    }
    inline float setpoint () const { return config.SETPOINT; }
    inline float current () const { return _value.max (); } // XXX think about this ... max, average, etc

protected:
    void collectAlarms (AlarmSet& alarms) const override {
        const float _min = _value.min (), _max = _value.max ();
        if (_min <= config.FAILURE) alarms += ALARM_TEMPERATURE_FAILURE;
        else if (_min <= config.MINIMAL) alarms += ALARM_TEMPERATURE_MINIMAL;
        if (_max >= config.MAXIMAL) alarms += ALARM_TEMPERATURE_MAXIMAL;
        else if (_max >= config.WARNING) alarms += ALARM_TEMPERATURE_WARNING;
    }
    void collectDiagnostics (JsonDocument &obj) const override {
//        JsonObject bat = obj ["bat"].to <JsonObject> ();
//        _statsValues, _statsValueAvg, _statsValueMin, _statsValueMax
    }
};

// -----------------------------------------------------------------------------------------------

class TemperatureManagerEnvironment : public Component, public Alarmable, public Diagnosticable {

public:
    static inline constexpr int PROBE_COUNT = 1;

    typedef struct {
        int channel;
        float FAILURE;
    } Config;

private:
    const Config &config;

    TemperatureInterface& _interface;
    MovingAverageWithValue <float, 16> _value;
    Stats <float> _stats;

public:
    TemperatureManagerEnvironment (const Config& cfg, TemperatureInterface& interface): config (cfg), _interface (interface), _value (round2places) {};
    void process () override {
        _value = _interface.getTemperature (config.channel);
        _stats += _value;
        DEBUG_PRINTF ("TemperatureManagerEnvironment::process: temp=%.2f\n", static_cast <float> (_value));
    }
    inline float getTemperature () const { return _value; }

protected:
    void collectAlarms (AlarmSet& alarms) const override {
        if (_value <= config.FAILURE) alarms += ALARM_TEMPERATURE_FAILURE;
    }
    void collectDiagnostics (JsonDocument &obj) const override {
//        JsonObject env = obj ["env"].to <JsonObject> ();    
//        _stats
    }
};

// -----------------------------------------------------------------------------------------------

class FanManager : public Component {

public:
    typedef struct {
        FanInterface::FanSpeedType NO_SPEED, MIN_SPEED, MAX_SPEED;
    } Config;
    using TargetSet = std::pair <float, float>;
    using TargetSetFunc = std::function <TargetSet ()>;

private:
    const Config &config;

    FanInterface &_fan;
    PidController &_controllerAlgorithm;
    AlphaSmoothing &_smootherAlgorithm;

    const TargetSetFunc _targetValues;

public:
    FanManager (const Config& cfg, FanInterface& fan, PidController& controller, AlphaSmoothing& smoother, const TargetSetFunc targetValues): config (cfg), _fan (fan), _controllerAlgorithm (controller), _smootherAlgorithm (smoother), _targetValues (std::move (targetValues)) {}
    void process () override {
        const TargetSet targets (_targetValues ());
        const float &setpoint = targets.first, &current = targets.second;
        if (current < setpoint) {
            _fan.setSpeed (config.NO_SPEED);
            DEBUG_PRINTF ("FanManager::process: setpoint=%.2f, current=%.2f\n", setpoint, current);
        } else {
            const float speedCalculated = _controllerAlgorithm.apply (setpoint, current);
            const float speedConstrained = std::clamp (map  <float> (speedCalculated, -100.0, 100.0, (float) config.MIN_SPEED, (float) config.MAX_SPEED), (float) config.MIN_SPEED, (float) config.MAX_SPEED);
            const float speedSmoothed = _smootherAlgorithm.apply (speedConstrained);
            _fan.setSpeed (speedSmoothed);
            DEBUG_PRINTF ("FanManager::process: setpoint=%.2f, current=%.2f --> calculated=%.2f, constrained=%.2f, smoothed=%.2f\n", setpoint, current, speedCalculated, speedConstrained, speedSmoothed);
        }
    }
};

// -----------------------------------------------------------------------------------------------
