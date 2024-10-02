
// -----------------------------------------------------------------------------------------------

class TemperatureManager: public Component {

public:
    typedef struct {
        int PROBE_NUMBER, PROBE_ENVIRONMENT;
        float MINIMAL, WARNING, CRITICAL;
    } Config;

protected:
    const Config &config;
    TemperatureInterface& _temperature;

public:
    TemperatureManager (const Config& cfg, TemperatureInterface& temperature) : config (cfg), _temperature (temperature) {};
};

// -----------------------------------------------------------------------------------------------

// XXX probably should be template class according to probe numbers
class TemperatureManagerBatterypack: public TemperatureManager, public Alarmable {

    static inline constexpr int PROBE_COUNT = 15; // config.temperature.PROBE_NUMBER - 1

    typedef std::array <float, PROBE_COUNT> Values;
    float _min, _max, _avg;
    Values _values;
    std::array <MovingAverage <float, 16>, PROBE_COUNT> _filters;

public:
    TemperatureManagerBatterypack (const TemperatureManager::Config& cfg, TemperatureInterface& temperature) : TemperatureManager (cfg, temperature) {};
    void process () override {
        float sum = 0.0;
        _min = 1000.0;
        _max = -1000.0;
        int cnt = 0;
        DEBUG_PRINTF ("TemperatureManagerBatterypack::process: temps=[");
        for (int num = 0; num < config.PROBE_NUMBER; num ++) {
            if (num != config.PROBE_ENVIRONMENT) {
                const float val = std::round (_temperature.get (num) * 100.0) / 100.0;
                const float tmp = std::round (_filters [cnt].update (val) * 100.0) / 100.0;
                _values [cnt ++] = tmp;
                sum += tmp;
                if (tmp < _min) _min = tmp;
                if (tmp > _max) _max = tmp;
                DEBUG_PRINTF ("%.2f/%.2f%s", val, tmp, (num + 1 != config.PROBE_NUMBER) ? ", " : "");
            }
        }
        _avg = std::round ((sum / (1.0 * cnt)) * 100.0) / 100.0;
        DEBUG_PRINTF ("], avg=%.2f, min=%.2f, max=%.2f\n", _avg, _min, _max);
    }
    inline float min () const { return _min; }
    inline float max () const { return _max; }
    inline float avg () const { return _avg; }
    inline const Values& getTemperatures () const { return _values; }
    inline float setpoint () const { return (config.WARNING + config.CRITICAL) / 2.0f; }
    inline float current () const { return _max; }

protected:
    void collectAlarms (AlarmSet& alarms) const override {
      if (_min <= config.MINIMAL) alarms += ALARM_TEMPERATURE_MINIMAL;
      if (_max >= config.CRITICAL) alarms += ALARM_TEMPERATURE_MAXIMAL;
    }
};

// -----------------------------------------------------------------------------------------------

class TemperatureManagerEnvironment : public TemperatureManager, public Alarmable {
    float _value;
    MovingAverage <float, 16> _filter;

public:
    TemperatureManagerEnvironment (const TemperatureManager::Config& cfg, TemperatureInterface& temperature) : TemperatureManager (cfg, temperature) {};
    void process () override {
        const float val = std::round (_temperature.get (config.PROBE_ENVIRONMENT) * 100.0) / 100.0;
        _value = std::round (_filter.update (val) * 100.0) / 100.0;
        DEBUG_PRINTF ("TemperatureManagerEnvironment::process: temp=%.2f\n", _value);
    }
    inline float getTemperature () const { return _value; }

protected:
    void collectAlarms (AlarmSet& alarms) const override {
      if (_value <= config.MINIMAL) alarms += ALARM_TEMPERATURE_MINIMAL;
      if (_value >= config.CRITICAL) alarms += ALARM_TEMPERATURE_MAXIMAL;
    }
};

// -----------------------------------------------------------------------------------------------

class FanManager : public Component {

public:
    typedef struct {
        uint8_t MIN_SPEED, MAX_SPEED;
    } Config;

private:
    const Config &config;
    FanInterface &_fan;
    PidController &_controllerAlgorithm;
    AlphaSmoothing &_smootherAlgorithm;
    const TemperatureManagerBatterypack& _temperatures;

public:
    FanManager (const Config& cfg, FanInterface& fan, const TemperatureManagerBatterypack& temperatures, PidController& controller, AlphaSmoothing& smoother) : config (cfg), _fan (fan), _controllerAlgorithm (controller), _smootherAlgorithm (smoother), _temperatures (temperatures) {}
    void process () override {
        const float setpoint = _temperatures.setpoint (), current = _temperatures.current ();
        const float speedCalculated = _controllerAlgorithm.apply (setpoint, current);
        const float speedConstrained = std::clamp (map  <float> (speedCalculated, -100.0, 100.0, (float) config.MIN_SPEED, (float) config.MAX_SPEED), (float) config.MIN_SPEED, (float) config.MAX_SPEED);
        const float speedSmoothed = _smootherAlgorithm.apply (speedConstrained);
        _fan.setSpeed (speedSmoothed);
        DEBUG_PRINTF ("FanManager::process: setpoint=%.2f, current=%.2f, calculated=%.2f, constrained=%.2f, smoothed=%.2f\n", setpoint, current, speedCalculated, speedConstrained, speedSmoothed);
    }
};

// -----------------------------------------------------------------------------------------------
