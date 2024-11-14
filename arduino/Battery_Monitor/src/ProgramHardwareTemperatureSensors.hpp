
// -----------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------

class TemperatureInterface : public Component, public Diagnosticable {
public:
    using AdcValueType = uint16_t;
    static inline constexpr int AdcResolution = 12;
    static inline constexpr AdcValueType AdcValueMin = 0, AdcValueMax = (1 << AdcResolution) - 1;
    using AdcHardware = MuxInterface_CD74HC4067<AdcValueType>;
    using TemperatureCalculationFunc = std::function<float (const int channel, const AdcValueType resistance)>;
    static inline constexpr int CHANNELS = MuxInterface_CD74HC4067<AdcValueType>::CHANNELS;

    typedef struct {
        AdcHardware::Config hardware;
#ifdef TEMPERATURE_INTERFACE_DONTUSECALIBRATION
        struct Thermister {
            float REFERENCE_RESISTANCE, NOMINAL_RESISTANCE, NOMINAL_TEMPERATURE;
        } thermister;
#endif
    } Config;

private:
    const Config &config;

    AdcHardware _hardware;
    const TemperatureCalculationFunc _calculator;

    std::array<StatsWithValue<float>, AdcHardware::CHANNELS> _stats;
    inline void updateStats (const int channel, const float temperature) {
        _stats [channel] += temperature;
    }

public:
    TemperatureInterface (const Config &cfg, const TemperatureCalculationFunc calculator) :
        config (cfg),
        _hardware (cfg.hardware),
        _calculator (calculator) { }
    void begin () override {
        analogReadResolution (AdcResolution);
        _hardware.enable ();
    }
    inline bool isResistanceReasonable (const uint16_t resistance) const {
        return resistance > 0 && resistance < 10 * 1000;
    }
    inline bool isTemperatureReasonable (const float temperature) const {
        return temperature > -100.0f && temperature < 150.0f;
    }
    bool getTemperature (const int channel, float *temperature) const {
        assert (channel >= 0 && channel < AdcHardware::CHANNELS && "Channel out of range");
        AdcValueType resistance = _hardware.get (channel);
        if (! isResistanceReasonable (resistance))
            return false;
#ifdef TEMPERATURE_INTERFACE_DONTUSECALIBRATION
        *temperature = steinharthart_calculator (resistance, AdcValueMax, config.thermister.REFERENCE_RESISTANCE, config.thermister.NOMINAL_RESISTANCE, config.thermister.NOMINAL_TEMPERATURE);
#else
        *temperature = _calculator (channel, resistance);
#endif
        if (! isTemperatureReasonable (*temperature))
            return false;
        const_cast<TemperatureInterface *> (this)->updateStats (channel, *temperature);
        return true;
    }

protected:
    void collectDiagnostics (JsonVariant &obj) const override {
        JsonArray sub = obj ["tmp"].to<JsonArray> ();
        for (const auto &stats : _stats)
            sub.add (ArithmeticToString (stats.val ()) + "," + ArithmeticToString (stats.avg ()) + "," + ArithmeticToString (stats.min ()) + "," + ArithmeticToString (stats.max ()));
    }
};

// -----------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------
