
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
    std::array <MovingAverage <float, 16>, PROBE_COUNT> _values;
    using AggregateValue = Stats <float>;
    AggregateValue _value; // change to compute something more sophisticated
    //std::array <Stats <float>, PROBE_COUNT> _statsValues;
    Stats <float> _statsValueAvg, _statsValueMin, _statsValueMax;

public:
    TemperatureManagerBatterypackTemplate (const Config& cfg, TemperatureInterface& interface): Alarmable ({
            AlarmCondition (ALARM_TEMPERATURE_FAILURE, [this] () { return _value.min () <= config.FAILURE; }),
            AlarmCondition (ALARM_TEMPERATURE_MINIMAL, [this] () { return _value.min () > config.FAILURE && _value.min () <= config.MINIMAL; }),
            AlarmCondition (ALARM_TEMPERATURE_WARNING, [this] () { return _value.max () >= config.WARNING && _value.max () < config.MAXIMAL; }),
            AlarmCondition (ALARM_TEMPERATURE_MAXIMAL, [this] () { return _value.max () >= config.MAXIMAL; }),
        }), config (cfg), _interface (interface), _values () {
        assert (config.FAILURE < config.MINIMAL && config.MINIMAL < config.WARNING && config.WARNING < config.MAXIMAL && "Bad configuration values");
        _values.fill (MovingAverage <float, 16> (round2places));
    };
    void process () override {
        _value.reset ();
        int cnt = 0;
        DEBUG_PRINTF ("TemperatureManagerBatterypack::process: temps=[");
        for (const auto channel : config.channels) {
              const float tmp = _interface.getTemperature (channel), val = (_values [cnt] = tmp);
              //_statsValues [cnt] += val;
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
    inline TemperatureArray getTemperatures () const {
        TemperatureArray result;
        auto it = result.begin ();
        for (const auto& value : _values)
            *it ++ = static_cast <float> (value);
        return result;
    }
    inline float setpoint () const { return config.SETPOINT; }
    inline float current () const { return _value.max (); } // XXX think about this ... max, average, etc

protected:
    void collectDiagnostics (JsonVariant &obj) const override {
        JsonObject sub = obj ["bat"].to <JsonObject> ();
            JsonObject tmp = sub ["tmp"].to <JsonObject> ();
                tmp ["avg"] = _statsValueAvg;
                tmp ["min"] = _statsValueMin;
                tmp ["max"] = _statsValueMax;
                // JsonArray val = tmp ["val"].to <JsonArray> ();
                // for (const auto& stats : _statsValues)
                //     val.add (ArithmeticToString (stats.avg ()) + "," + ArithmeticToString (stats.min ()) + "," + ArithmeticToString (stats.max ()));
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

    MovingAverage <float, 16> _value;
    Stats <float> _statsValue;

public:
    TemperatureManagerEnvironmentTemplate (const Config& cfg, TemperatureInterface& interface): Alarmable ({
            AlarmCondition (ALARM_TEMPERATURE_FAILURE, [this] () { return _value <= config.FAILURE; })
        }), config (cfg), _interface (interface), _value (round2places) {};
    void process () override {
        _value = _interface.getTemperature (config.channel);
        _statsValue += _value;
        DEBUG_PRINTF ("TemperatureManagerEnvironment::process: temp=%.2f\n", static_cast <float> (_value));
    }
    inline float getTemperature () const { return _value; }

protected:
    void collectDiagnostics (JsonVariant &obj) const override {
        JsonObject sub = obj ["env"].to <JsonObject> ();
            sub ["tmp"] = _statsValue;
    }
};

// -----------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------

class FanManager : public Component, public Diagnosticable {
public:
    typedef struct {
    } Config;

    using TargetSet = std::pair <float, float>;
    using TargetSetFunc = std::function <TargetSet ()>;

private:
    const Config &config;

    FanInterface &_fan;
    PidController <double> &_controllerAlgorithm; // XXX should be an abstract interface
    AlphaSmoothing <double> &_smootherAlgorithm; // XXX should be an abstract interface

    const TargetSetFunc _targetValues;
    float _value = 0.0f;
    Stats <float> _statsValue;

public:
    FanManager (const Config& cfg, FanInterface& fan, PidController <double>& controller, AlphaSmoothing <double>& smoother, const TargetSetFunc targetValues): config (cfg), _fan (fan), _controllerAlgorithm (controller), _smootherAlgorithm (smoother), _targetValues (targetValues) {
    }
    void process () override {
        const TargetSet targets (_targetValues ());
        const float &setpoint = targets.first, &current = targets.second;
        if (current < setpoint) {
            DEBUG_PRINTF ("FanManager::process: setpoint=%.2f, current=%.2f\n", setpoint, current);
            _fan.setSpeed (0.0f);
        } else {
            const double speedCalculated = _controllerAlgorithm.apply (setpoint, current);
            const double speedConstrained = std::clamp (map <double> (speedCalculated, -100.0, 100.0, 0.0, 100.0), 0.0, 100.0); // XXX is this correct?
            const double speedSmoothed = _smootherAlgorithm.apply (speedConstrained);
            DEBUG_PRINTF ("FanManager::process: setpoint=%.2f, current=%.2f --> calculated=%.2e, constrained=%.2e, smoothed=%.2e\n", setpoint, current, speedCalculated, speedConstrained, speedSmoothed);
            _value = static_cast <float> (speedSmoothed);
            _fan.setSpeed (_value); // percentage: 0% -> 100%
            _statsValue += _value;
        }
    }

protected:
    void collectDiagnostics (JsonVariant &obj) const override {
        JsonObject sub = obj ["fan"].to <JsonObject> ();
            sub ["pid"] = _controllerAlgorithm;
            sub ["speed"] = _statsValue;
    }
};

// -----------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------
