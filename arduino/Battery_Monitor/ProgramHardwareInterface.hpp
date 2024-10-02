
// -----------------------------------------------------------------------------------------------

class TemperatureInterface : public Component, public Diagnosticable {

    static inline constexpr int ADC_RESOLUTION = 12, ADC_MINVALUE = 0, ADC_MAXVALUE = ((1 << ADC_RESOLUTION) - 1);

public:
    typedef struct {
        MuxInterface_CD74HC4067::Config mux;
        struct Thermister {
            float REFERENCE_RESISTANCE, NOMINAL_RESISTANCE, NOMINAL_TEMPERATURE;
        } thermister;
    } Config;

private:
    const Config &config;
    typedef struct { uint16_t v_now, v_min, v_max; } ValueSet;
    MuxInterface_CD74HC4067 _muxInterface;
    std::array <ValueSet, MuxInterface_CD74HC4067::CHANNELS> _muxValues;
    void updateValues (const int channel, const uint16_t value) {
        ValueSet &valueSet = _muxValues [channel];
        valueSet.v_now = value;
        if (value > valueSet.v_min) valueSet.v_min = value;
        if (value < valueSet.v_max) valueSet.v_max = value;
    }

public:
    TemperatureInterface (const Config cfg) : config (cfg), _muxInterface (cfg.mux) {
        for (int channel = 0; channel < MuxInterface_CD74HC4067::CHANNELS; channel ++)
            _muxValues [channel] = { .v_now = ADC_MINVALUE, .v_min = ADC_MAXVALUE, .v_max = ADC_MINVALUE };
    }
    void begin () override {
        analogReadResolution (ADC_RESOLUTION);
        _muxInterface.enable ();
    }
    float get (const int channel) const {
        assert (channel >= 0 && channel < MuxInterface_CD74HC4067::CHANNELS && "Channel out of range");
        uint16_t value = _muxInterface.get (channel);
        const_cast <TemperatureInterface*> (this)->updateValues (channel, value);
        return steinharthart_calculator (value, ADC_MAXVALUE, config.thermister.REFERENCE_RESISTANCE, config.thermister.NOMINAL_RESISTANCE, config.thermister.NOMINAL_TEMPERATURE);
    }

protected:
    void collectDiagnostics (JsonDocument &obj) const override { // XXX this is too large and needs reduction
        JsonObject tmp = obj ["tmp"].to <JsonObject> ();
        for (int channel = 0; channel < MuxInterface_CD74HC4067::CHANNELS; channel ++) {
            JsonObject entry = tmp [IntToString (channel)].to <JsonObject> ();
            entry ["#"] = _muxValues [channel].v_now;
            entry ["<"] = _muxValues [channel].v_min;
            entry [">"] = _muxValues [channel].v_max;
        }
    }
};

// -----------------------------------------------------------------------------------------------

class FanInterface : public Component, public Diagnosticable {

    static inline constexpr int PWM_RESOLUTION = 8;
    static inline constexpr uint8_t PWM_MINVALUE = 0, PWM_MAXVALUE = ((1 << PWM_RESOLUTION) - 1);

public:
    typedef struct  {
        OpenSmart_QuadMotorDriver::Config qmd;
        uint8_t MIN_SPEED, MAX_SPEED;
    } Config;

private:
    const Config &config;
    uint8_t _speed = PWM_MINVALUE, _speedMin = PWM_MAXVALUE, _speedMax = PWM_MINVALUE;
    OpenSmart_QuadMotorDriver _driver;
    ActivationTracker _sets;

public:
    FanInterface (const Config& cfg) : config (cfg), _driver (config.qmd) {}
    void begin () override {
        _driver.setDirection (OpenSmart_QuadMotorDriver::MOTOR_CLOCKWISE); // all 4 fans, for now
        _driver.setSpeed (_speed);
    }

    void setSpeed (const uint8_t speed) {
        const uint8_t speedNew = std::clamp (speed, PWM_MINVALUE, PWM_MAXVALUE);
        if (speedNew != _speed) {
            DEBUG_PRINTF ("FanInterface::setSpeed: %d\n", speedNew);
            if (speedNew > config.MIN_SPEED && _speed <= config.MIN_SPEED) _sets ++;
            _driver.setSpeed (_speed = speedNew); // all 4 fans, for now
            if (_speed < _speedMin) _speedMin = _speed;
            if (_speed > _speedMax) _speedMax = _speed;
        }
    }
   inline uint8_t getSpeed () const {
        return _speed;
    }

protected:
    void collectDiagnostics (JsonDocument &obj) const override {
        JsonObject fan = obj ["fan"].to <JsonObject> ();
        fan ["now"] = _speed;
        fan ["min"] = _speedMin;
        fan ["max"] = _speedMax;
        _sets.serialize (fan);
        // % duty
    }
};

// -----------------------------------------------------------------------------------------------
