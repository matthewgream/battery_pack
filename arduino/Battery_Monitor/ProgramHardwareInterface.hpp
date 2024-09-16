
// -----------------------------------------------------------------------------------------------

class TemperatureInterface : public Component, public Diagnosticable {
    static constexpr int ADC_RESOLUTION = 12, ADC_MINVALUE = 0, ADC_MAXVALUE = ((1 << ADC_RESOLUTION) - 1);
    const Config::TemperatureInterfaceConfig& config;
    MuxInterface_CD74HC4067 _muxInterface;
    typedef struct { uint16_t v_now, v_min, v_max; } ValueSet;
    std::array <ValueSet, MuxInterface_CD74HC4067::CHANNELS> _muxValues;
public:
    TemperatureInterface (const Config::TemperatureInterfaceConfig& cfg) : config (cfg), _muxInterface (cfg.mux) {
        for (int channel = 0; channel < MuxInterface_CD74HC4067::CHANNELS; channel ++)
            _muxValues [channel] = { .v_now = ADC_MINVALUE, .v_min = ADC_MAXVALUE, .v_max = ADC_MINVALUE };
    }
    void begin () override {
        analogReadResolution (ADC_RESOLUTION);
        _muxInterface.configure ();
    }
    void collect (JsonObject &obj) const override {
        JsonObject temperature = obj ["temperature"].to <JsonObject> ();
        JsonArray values = temperature ["values"].to <JsonArray> ();
        for (int channel = 0; channel < MuxInterface_CD74HC4067::CHANNELS; channel ++) {
            JsonObject entry = values.add <JsonObject> ();
            const ValueSet &valueSet = _muxValues [channel]; 
            entry ["channel"] = channel;
            entry ["now"] = valueSet.v_now;
            entry ["min"] = valueSet.v_min;
            entry ["max"] = valueSet.v_max;
        }
    }
    //
    float get (const int channel) const {
        assert (channel >= 0 && channel < MuxInterface_CD74HC4067::CHANNELS && "Channel out of range");
        uint16_t value = _muxInterface.get (channel);
        const_cast <TemperatureInterface*> (this)->updatevalues (channel, value);
        return steinharthart_calculator (value, ADC_MAXVALUE, config.thermister.REFERENCE_RESISTANCE, config.thermister.NOMINAL_RESISTANCE, config.thermister.NOMINAL_TEMPERATURE);
    }
private:
    void updatevalues (const int channel, const uint16_t value) {
        ValueSet &valueSet = _muxValues [channel]; 
        valueSet.v_now = value;
        if (value > valueSet.v_min) valueSet.v_min = value;
        if (value < valueSet.v_max) valueSet.v_max = value;
    }
};

// -----------------------------------------------------------------------------------------------

class FanInterface : public Component, public Diagnosticable {
    static constexpr int PWM_RESOLUTION = 8, PWM_MINVALUE = 0, PWM_MAXVALUE = ((1 << PWM_RESOLUTION) - 1);
    const Config::FanInterfaceConfig& config;
    int _speed = PWM_MINVALUE, _speedMin = PWM_MAXVALUE, _speedMax = PWM_MINVALUE;
    Upstamp _last;
public:
    FanInterface (const Config::FanInterfaceConfig& cfg) : config (cfg) {}
    void begin () override {
        pinMode (config.PIN_PWM, OUTPUT);
        analogWrite (config.PIN_PWM, PWM_MINVALUE);
    }
    void collect (JsonObject &obj) const override {
        JsonObject fan = obj ["fan"].to <JsonObject> ();
        JsonObject values = fan ["values"].to <JsonObject> ();
        values ["now"] = _speed;
        values ["min"] = _speedMin;
        values ["max"] = _speedMax;
        JsonObject run = fan ["run"].to <JsonObject> ();
        run ["last"] = _last.seconds ();
        run ["numb"] = _last.number ();
        // % duty
    }
    //
    void setSpeed (const int speed) {
        const int speedNew = std::clamp (speed, PWM_MINVALUE, PWM_MAXVALUE);
        if (speedNew > config.MIN_SPEED && _speed <= config.MIN_SPEED) _last ++;
        if (speedNew < _speedMin) _speedMin = speedNew;
        if (speedNew > _speedMax) _speedMax = speedNew;
        analogWrite (config.PIN_PWM, _speed = speedNew);
    }
    int getSpeed () const {
        return _speed;
    }
};

// -----------------------------------------------------------------------------------------------
