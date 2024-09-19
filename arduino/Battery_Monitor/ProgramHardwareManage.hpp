
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
    static constexpr int _num = 15; // config.temperature.PROBE_NUMBER - 1
    typedef std::array <float, _num> Values;
    float _min, _max, _avg;
    Values _values;
    std::array <MovingAverage <float>, _num> _filters;

public:
    TemperatureManagerBatterypack (const Config::TemperatureInterfaceConfig& cfg, TemperatureInterface& temperature) : TemperatureManager (cfg, temperature) {};
    void process () override {
        DEBUG_PRINT ("TemperatureManagerBatterypack::process: ");
        float sum = 0.0;
        _min = 1000.0;
        _max = -1000.0;
        int cnt = 0;
        DEBUG_PRINT ("temps=[");
        for (int num = 0; num < config.PROBE_NUMBER; num ++) {
            if (num != config.PROBE_ENVIRONMENT) {
                float val = _temperature.get (num);
                float tmp = _filters [cnt].update (val);
                _values [cnt ++] = tmp;
                sum += tmp;
                if (tmp < _min) _min = tmp;
                if (tmp > _max) _max = tmp;
                DEBUG_PRINT (val);
                DEBUG_PRINT ("/");
                DEBUG_PRINT (tmp);
                if (num + 1 != config.PROBE_NUMBER) {
                    DEBUG_PRINT (", ");
                }
            }
        }
        _avg = sum / (1.0 * cnt);
        DEBUG_PRINT ("], avg=");
        DEBUG_PRINT (_avg);
        DEBUG_PRINT (", min=");
        DEBUG_PRINT (_min);
        DEBUG_PRINT (", max=");
        DEBUG_PRINTLN (_max);
    }
    float min () const { return _min; }
    float max () const { return _max; }
    float avg () const { return _avg; }
    const Values& getTemperatures () const { return _values; }
    float setpoint () const { return (config.WARNING + config.CRITICAL) / 2.0f; }
    float current () const { return _max; }

protected:
    void collectAlarms (AlarmSet& alarms) const override {
      if (_min <= config.MINIMAL) alarms += ALARM_TEMPERATURE_MINIMAL;
      if (_max >= config.CRITICAL) alarms += ALARM_TEMPERATURE_MAXIMAL;
    }
};

// -----------------------------------------------------------------------------------------------

class TemperatureManagerEnvironment : public TemperatureManager, public Alarmable {
    float _value;
    MovingAverage <float> _filter;

public:
    TemperatureManagerEnvironment (const Config::TemperatureInterfaceConfig& cfg, TemperatureInterface& temperature) : TemperatureManager (cfg, temperature) {};
    void process () override {
        DEBUG_PRINT ("TemperatureManagerEnvironment::process: ");
        _value = _filter.update (_temperature.get (config.PROBE_ENVIRONMENT));
        DEBUG_PRINT ("temp=");
        DEBUG_PRINTLN (_value);
    }
    float getTemperature () const { return _value; }

protected:
    void collectAlarms (AlarmSet& alarms) const override {
      if (_value <= config.MINIMAL) alarms += ALARM_TEMPERATURE_MINIMAL;
      if (_value >= config.CRITICAL) alarms += ALARM_TEMPERATURE_MAXIMAL;
    }
};

// -----------------------------------------------------------------------------------------------

class FanManager : public Component {
    const Config::FanInterfaceConfig &config;
    FanInterface &_fan;
    PidController &_controllerAlgorithm;
    AlphaSmoothing &_smootherAlgorithm;
    const TemperatureManagerBatterypack& _temperatures;

public:
    FanManager (const Config::FanInterfaceConfig& cfg, FanInterface& fan, const TemperatureManagerBatterypack& temperatures, PidController& controller, AlphaSmoothing& smoother) : config (cfg), _fan (fan), _controllerAlgorithm (controller), _smootherAlgorithm (smoother), _temperatures (temperatures) {}
    void process () override {
        DEBUG_PRINT ("FanManager::process: ");
        const float setpoint = _temperatures.setpoint (), current = _temperatures.current ();
        const float speedCalculated = _controllerAlgorithm.apply (setpoint, current);
        const float speedConstrained = std::clamp (map  <float> (speedCalculated, -100.0, 100.0, (float) config.MIN_SPEED, (float) config.MAX_SPEED), (float) config.MIN_SPEED, (float) config.MAX_SPEED);
        const float speedSmoothed = _smootherAlgorithm.apply (speedConstrained);
        _fan.setSpeed (speedSmoothed);
        DEBUG_PRINT ("setpoint=");
        DEBUG_PRINT (setpoint);
        DEBUG_PRINT (", current=");
        DEBUG_PRINT (current);
        DEBUG_PRINT (", calculated=");
        DEBUG_PRINT (speedCalculated);
        DEBUG_PRINT (", constrained=");
        DEBUG_PRINT (speedConstrained);
        DEBUG_PRINT (", smoothed=");
        DEBUG_PRINTLN (speedSmoothed);
    }
};

// -----------------------------------------------------------------------------------------------
