
// -----------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------

template <size_t PROBE_COUNT>
class TemperatureManagerBatterypackTemplate: public Component, public Alarmable, public Diagnosticable {

public:
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
    TemperatureManagerBatterypackTemplate (const Config& cfg, TemperatureInterface& interface): Alarmable ({
            AlarmCondition (ALARM_TEMPERATURE_FAILURE, [this] () { return _value.min () <= config.FAILURE; }),
            AlarmCondition (ALARM_TEMPERATURE_MINIMAL, [this] () { return _value.min () > config.FAILURE && _value.min () <= config.MINIMAL; }),
            AlarmCondition (ALARM_TEMPERATURE_MAXIMAL, [this] () { return _value.max () >= config.MAXIMAL; }),
            AlarmCondition (ALARM_TEMPERATURE_WARNING, [this] () { return _value.max () < config.MAXIMAL && _value.max () >= config.WARNING; })
        }), config (cfg), _interface (interface), _values () {
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
        const float _avg = round2places (_value.avg ()), _min = _value.min (), _max = _value.max ();
        DEBUG_PRINTF ("], avg=%.2f, min=%.2f, max=%.2f\n", _avg, _min, _max);
        _statsValueAvg += _avg; _statsValueMin += _min; _statsValueMax += _max;
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
    void collectDiagnostics (JsonDocument &obj) const override {
//        JsonObject bat = obj ["bat"].to <JsonObject> ();
//        _statsValues, _statsValueAvg, _statsValueMin, _statsValueMax
    }
};

// -----------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------

template <size_t PROBE_COUNT>
class TemperatureManagerEnvironmentTemplate : public Component, public Alarmable, public Diagnosticable {

public:
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
    TemperatureManagerEnvironmentTemplate (const Config& cfg, TemperatureInterface& interface): Alarmable ({
            AlarmCondition (ALARM_TEMPERATURE_FAILURE, [this] () { return _value <= config.FAILURE; })
        }), config (cfg), _interface (interface), _value (round2places) {};
    void process () override {
        _value = _interface.getTemperature (config.channel);
        _stats += _value;
        DEBUG_PRINTF ("TemperatureManagerEnvironment::process: temp=%.2f\n", static_cast <float> (_value));
    }
    inline float getTemperature () const { return _value; }

protected:
   void collectDiagnostics (JsonDocument &obj) const override {
//        JsonObject env = obj ["env"].to <JsonObject> ();
//        _stats
    }
};

// -----------------------------------------------------------------------------------------------
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
    PidController <double> &_controllerAlgorithm;
    AlphaSmoothing <double> &_smootherAlgorithm;

    const TargetSetFunc _targetValues;

public:
    FanManager (const Config& cfg, FanInterface& fan, PidController <double>& controller, AlphaSmoothing <double>& smoother, const TargetSetFunc targetValues): config (cfg), _fan (fan), _controllerAlgorithm (controller), _smootherAlgorithm (smoother), _targetValues (std::move (targetValues)) {}
    void process () override {
        const TargetSet targets (_targetValues ());
        const float &setpoint = targets.first, &current = targets.second;
        if (current < setpoint) {
            _fan.setSpeed (config.NO_SPEED);
            DEBUG_PRINTF ("FanManager::process: setpoint=%.2f, current=%.2f\n", setpoint, current);
        } else {
            const double speedCalculated = _controllerAlgorithm.apply (setpoint, current);
            const double speedConstrained = std::clamp (map  <double> (speedCalculated, -100.0, 100.0, static_cast <double> (config.MIN_SPEED), static_cast <double> (config.MAX_SPEED)), static_cast <double> (config.MIN_SPEED), static_cast <double> (config.MAX_SPEED));
            const double speedSmoothed = _smootherAlgorithm.apply (speedConstrained);
            _fan.setSpeed (static_cast <FanInterface::FanSpeedType> (speedSmoothed));
            DEBUG_PRINTF ("FanManager::process: setpoint=%.2f, current=%.2f --> calculated=%.2e, constrained=%.2e, smoothed=%.2e\n", setpoint, current, speedCalculated, speedConstrained, speedSmoothed);
        }
    }
};

// -----------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------
