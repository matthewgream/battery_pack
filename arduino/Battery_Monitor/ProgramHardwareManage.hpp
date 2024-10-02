
// -----------------------------------------------------------------------------------------------

class TemperatureManager: public Component, public Alarmable {

public:
    typedef struct {
        int PROBE_NUMBER, PROBE_ENVIRONMENT;
        float SETPOINT;
        float FAILURE, MINIMAL, WARNING, MAXIMAL;
    } Config;

protected:
    const Config &config;
    TemperatureInterface& _temperature;

    virtual float min () const = 0;
    virtual float max () const = 0;

    void collectAlarms (AlarmSet& alarms) const override {
      const float _min = min (), _max = max ();
      if (_min <= config.FAILURE) alarms += ALARM_TEMPERATURE_FAILURE;
      else if (_min <= config.MINIMAL) alarms += ALARM_TEMPERATURE_MINIMAL;
      if (_max >= config.MAXIMAL) alarms += ALARM_TEMPERATURE_MAXIMAL;
      else if (_max >= config.WARNING) alarms += ALARM_TEMPERATURE_WARNING;
    }

public:
    TemperatureManager (const Config& cfg, TemperatureInterface& temperature): config (cfg), _temperature (temperature) {};
};

// -----------------------------------------------------------------------------------------------

// XXX
// XXX probably should be template class according to probe numbers
class TemperatureManagerBatterypack: public TemperatureManager {

    static inline constexpr int PROBE_COUNT = 15; // config.temperature.PROBE_NUMBER - 1

    float _min, _max, _avg;
    std::array <MovingAverageValue <float, 16>, PROBE_COUNT> _values;

public:
    TemperatureManagerBatterypack (const TemperatureManager::Config& cfg, TemperatureInterface& temperature): TemperatureManager (cfg, temperature), _values () {
        _values.fill (MovingAverageValue <float, 16> (round2places));      
    };
    void process () override {
        float sum = 0.0f;
        _min = 1000.0f;
        _max = -1000.0f;
        int cnt = 0;
        DEBUG_PRINTF ("TemperatureManagerBatterypack::process: temps=[");
        for (int num = 0; num < config.PROBE_NUMBER; num ++) {
            if (num != config.PROBE_ENVIRONMENT) {
                const float val = _temperature.getTemperature (num);
                const float tmp = (_values [cnt ++] = val);
                sum += tmp;
                if (tmp < _min) _min = tmp;
                if (tmp > _max) _max = tmp;
                DEBUG_PRINTF ("%.2f/%.2f%s", val, tmp, (num + 1 != config.PROBE_NUMBER) ? ", " : "");
            }
        }
        _avg = round2places (sum / static_cast <float> (cnt));
        DEBUG_PRINTF ("], avg=%.2f, min=%.2f, max=%.2f\n", _avg, _min, _max);
    }
    inline float min () const override { return _min; }
    inline float max () const override { return _max; }
    inline float avg () const { return _avg; }
    inline const std::array <float, PROBE_COUNT> getTemperatures () const { 
        std::array <float, PROBE_COUNT> result;
        auto it = result.begin ();
        for (const auto& value : _values)
            *it ++ = static_cast <float> (value);
        return result;        
    }
    inline float setpoint () const { return config.SETPOINT; }
    inline float current () const { return _max; } // XXX think about this ...
};

// -----------------------------------------------------------------------------------------------

class TemperatureManagerEnvironment : public TemperatureManager {
    MovingAverageValue <float, 16> _value;

public:
    TemperatureManagerEnvironment (const TemperatureManager::Config& cfg, TemperatureInterface& temperature): TemperatureManager (cfg, temperature), _value (round2places) {};
    void process () override {
        _value = _temperature.getTemperature (config.PROBE_ENVIRONMENT);
        DEBUG_PRINTF ("TemperatureManagerEnvironment::process: temp=%.2f\n", static_cast <float> (_value));
    }
    inline float min () const override { return _value; }
    inline float max () const override { return _value; }
    inline float getTemperature () const { return _value; }
};

// -----------------------------------------------------------------------------------------------

class FanManager : public Component {

public:
    typedef struct {
        uint8_t NO_SPEED, MIN_SPEED, MAX_SPEED;
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
