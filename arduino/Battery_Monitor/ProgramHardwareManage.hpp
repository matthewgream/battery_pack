
// -----------------------------------------------------------------------------------------------

class TemperatureManager: public Component {
protected:
    const Config::TemperatureInterfaceConfig& config;
    TemperatureInterface& _temperature;
public:
    TemperatureManager (const Config::TemperatureInterfaceConfig& cfg, TemperatureInterface& temperature) : config (cfg), _temperature (temperature) {};
};

// -----------------------------------------------------------------------------------------------

class TemperatureManagerBatterypack: public TemperatureManager, public Alarmable {
    float _min, _max, _avg;
    static constexpr int _num = 15; // config.temperature.PROBE_NUMBER - 1
    typedef std::array <float, _num> Values;
    Values _values;
    std::array <MovingAverage <float>, _num> _filters;
public:
    TemperatureManagerBatterypack (const Config::TemperatureInterfaceConfig& cfg, TemperatureInterface& temperature) : TemperatureManager (cfg, temperature) {};
    void process () override {
        float sum = 0.0;
        _min = 1000.0;
        _max = -1000.0;
        int cnt = 0;
        for (int num = 0; num < config.PROBE_NUMBER; num ++) {
            if (num != config.PROBE_ENVIRONMENT) {
                float tmp = _filters [cnt].update (_temperature.get (num));
                _values [cnt ++] = tmp;
                sum += tmp;
                if (tmp < _min) _min = tmp;
                if (tmp > _max) _max = tmp;
            }
        }
        _avg = sum / (1.0 * cnt);
    }
    //
    AlarmSet alarm () const override {
      AlarmSet alarms;
      if (_min <= config.MINIMAL) alarms += ALARM_TEMPERATURE_MINIMAL;
      if (_max >= config.CRITICAL) alarms += ALARM_TEMPERATURE_MAXIMAL;
      return alarms;
    }
    float min () const { return _min; }
    float max () const { return _max; }
    float avg () const { return _avg; }
    const Values& getTemperatures () const { return _values; }
    float setpoint () const { return (config.WARNING + config.CRITICAL) / 2.0f; }
    float current () const { return _max; }
};

// -----------------------------------------------------------------------------------------------

class TemperatureManagerEnvironment : public TemperatureManager, public Alarmable {
    float _value;
    MovingAverage <float> _filter;
public:
    TemperatureManagerEnvironment (const Config::TemperatureInterfaceConfig& cfg, TemperatureInterface& temperature) : TemperatureManager (cfg, temperature) {};
    void process () override {
        _value = _filter.update (_temperature.get (config.PROBE_ENVIRONMENT));
    }
    AlarmSet alarm () const override {
      AlarmSet alarms;
      if (_value <= config.MINIMAL) alarms += ALARM_TEMPERATURE_MINIMAL;
      if (_value >= config.CRITICAL) alarms += ALARM_TEMPERATURE_MAXIMAL;
      return alarms;
    }
    //
    float getTemperature () const { return _value; }
};

// -----------------------------------------------------------------------------------------------

class FanManager : public Component {
    const Config::FanInterfaceConfig &config;
    FanInterface &_fan;
    const TemperatureManagerBatterypack& _temperatures;
    PidController &_controllerAlgorithm;
    AlphaSmoothing &_smootherAlgorithm;

public:
    FanManager (const Config::FanInterfaceConfig& cfg, FanInterface& fan, const TemperatureManagerBatterypack& temperatures, PidController& controller, AlphaSmoothing& smoother) : config (cfg), _fan (fan), _temperatures (temperatures), _controllerAlgorithm (controller), _smootherAlgorithm (smoother) {}
    void process () override {
        const float setpoint = _temperatures.setpoint (), current = _temperatures.current ();
        const float speedCalculated = _controllerAlgorithm.apply (setpoint, current);
        const int speedSmoothed = _smootherAlgorithm.apply (std::clamp ((int) map  <float> (speedCalculated, -100, 100, config.MIN_SPEED, config.MAX_SPEED), config.MIN_SPEED, config.MAX_SPEED));
        _fan.setSpeed (speedSmoothed);
    }
};

// -----------------------------------------------------------------------------------------------
