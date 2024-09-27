
// -----------------------------------------------------------------------------------------------

class TemperatureInterface : public Component, public Diagnosticable {
    static constexpr int ADC_RESOLUTION = 12, ADC_MINVALUE = 0, ADC_MAXVALUE = ((1 << ADC_RESOLUTION) - 1);
    typedef struct { uint16_t v_now, v_min, v_max; } ValueSet;
    const Config::TemperatureInterfaceConfig& config;
    MuxInterface_CD74HC4067 _muxInterface;
    std::array <ValueSet, MuxInterface_CD74HC4067::CHANNELS> _muxValues;
    void updatevalues (const int channel, const uint16_t value) {
        ValueSet &valueSet = _muxValues [channel]; 
        valueSet.v_now = value;
        if (value > valueSet.v_min) valueSet.v_min = value;
        if (value < valueSet.v_max) valueSet.v_max = value;
    }

public:
    TemperatureInterface (const Config::TemperatureInterfaceConfig& cfg) : config (cfg), _muxInterface (cfg.mux) {
        for (int channel = 0; channel < MuxInterface_CD74HC4067::CHANNELS; channel ++)
            _muxValues [channel] = { .v_now = ADC_MINVALUE, .v_min = ADC_MAXVALUE, .v_max = ADC_MINVALUE };
    }
    void begin () override {
        analogReadResolution (ADC_RESOLUTION);
        _muxInterface.configure ();
    }
    float get (const int channel) const {
        assert (channel >= 0 && channel < MuxInterface_CD74HC4067::CHANNELS && "Channel out of range");
        uint16_t value = _muxInterface.get (channel);
        const_cast <TemperatureInterface*> (this)->updatevalues (channel, value);
        return steinharthart_calculator (value, ADC_MAXVALUE, config.thermister.REFERENCE_RESISTANCE, config.thermister.NOMINAL_RESISTANCE, config.thermister.NOMINAL_TEMPERATURE);
    }

protected:
    void collectDiagnostics (JsonObject &obj) const override {
        JsonArray temps = obj ["temps"].to <JsonArray> ();
        for (int channel = 0; channel < MuxInterface_CD74HC4067::CHANNELS; channel ++) {
            JsonObject entry = temps.add <JsonObject> ();
            const ValueSet &valueSet = _muxValues [channel]; 
            entry ["channel"] = channel;
            entry ["now"] = valueSet.v_now;
            entry ["min"] = valueSet.v_min;
            entry ["max"] = valueSet.v_max;
        }
    }
};

// -----------------------------------------------------------------------------------------------

class FanInterface : public Component, public Diagnosticable {
    static constexpr int PWM_RESOLUTION = 8;
    static constexpr uint8_t PWM_MINVALUE = 0, PWM_MAXVALUE = ((1 << PWM_RESOLUTION) - 1);
    const Config::FanInterfaceConfig& config;
    uint8_t _speed = PWM_MINVALUE, _speedMin = PWM_MAXVALUE, _speedMax = PWM_MINVALUE;
    OpenSmart_QuadMotorDriver _driver;
    ActivationTracker _activations;

public:
    FanInterface (const Config::FanInterfaceConfig& cfg) : config (cfg), _driver (config.I2C, { config.PIN_PWMA, config.PIN_PWMB, config.PIN_PWMC, config.PIN_PWMD}) {}
    void begin () override {
        _driver.setDirection (OpenSmart_QuadMotorDriver::MOTOR_CLOCKWISE); // all 4 fans, for now
    }
    void setSpeed (const uint8_t speed) {
        const uint8_t speedNew = std::clamp (speed, PWM_MINVALUE, PWM_MAXVALUE);
        if (speedNew > config.MIN_SPEED && _speed <= config.MIN_SPEED) _activations ++;
        _driver.setSpeed (_speed = speed); // all 4 fans, for now
    }
    uint8_t getSpeed () const {
        return _speed;
    }

protected:
    void collectDiagnostics (JsonObject &obj) const override {
        JsonObject fan = obj ["fan"].to <JsonObject> ();
        fan ["now"] = _speed;
        fan ["min"] = _speedMin;
        fan ["max"] = _speedMax;
        _activations.serialize (fan ["activated"].to <JsonObject> ());
        // % duty
    }
};

// -----------------------------------------------------------------------------------------------
