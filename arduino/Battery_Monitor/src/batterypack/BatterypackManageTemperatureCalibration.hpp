
// -----------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------

template <size_t SENSOR_SIZE, float TEMP_START, float TEMP_END, float TEMP_STEP>
class ProgramManageTemperatureCalibrationTemplate : public Component, public Diagnosticable {
public:
    using Definitions = TemperatureCalibrationDefinitions<SENSOR_SIZE, TEMP_START, TEMP_END, TEMP_STEP>;
    using Collector = TemperatureCalibrationCollector<SENSOR_SIZE, TEMP_START, TEMP_END, TEMP_STEP>;
    using Calculator = TemperatureCalibrationCalculator<SENSOR_SIZE, TEMP_START, TEMP_END, TEMP_STEP>;
    using Storage = TemperatureCalibrationStorage<SENSOR_SIZE, TEMP_START, TEMP_END, TEMP_STEP>;
    using Runtime = TemperatureCalibrationRuntime<SENSOR_SIZE, TEMP_START, TEMP_END, TEMP_STEP>;

    using StrategyLookup = TemperatureCalibrationAdjustmentStrategy_Lookup<SENSOR_SIZE, TEMP_START, TEMP_END, TEMP_STEP>;
    using StrategySteinhart = TemperatureCalibrationAdjustmentStrategy_Steinhart<SENSOR_SIZE, TEMP_START, TEMP_END, TEMP_STEP>;
    using StrategyDefault = typename Calculator::StrategyDefault;
    using StrategyFactories = typename Calculator::StrategyFactories;
    using CalibrationStrategies = typename Calculator::CalibrationStrategies;

    struct Config {
        String filename;
        StrategyDefault::Config strategyDefault;
    };

private:
    const Config &config;
    std::shared_ptr<Runtime> runtime;
    size_t _loaded = 0;

    StrategyFactories createStrategyFactoriesForCalibration () const {    // store the lookup tables, but don't use them
        return StrategyFactories {
            { StrategyLookup::NAME, [] { return std::make_shared<StrategyLookup> (); } },
            { StrategySteinhart::NAME, [] { return std::make_shared<StrategySteinhart> (); } }
        };
    }
    StrategyFactories createStrategyFactoriesForOperation () const {
        return StrategyFactories {
            { StrategySteinhart::NAME, [] { return std::make_shared<StrategySteinhart> (); } }
        };
    }

public:
    explicit ProgramManageTemperatureCalibrationTemplate (const Config &cfg) :
        config (cfg) { }

    void begin () override {
        std::shared_ptr<typename Calculator::CalibrationStrategies> calibrationStrategies = std::make_shared<typename Calculator::CalibrationStrategies> ();
        StrategyDefault defaultStrategy (config.strategyDefault);
        if (! (_loaded = Storage::deserialize (config.filename, defaultStrategy, *calibrationStrategies, createStrategyFactoriesForOperation ())))
            DEBUG_PRINTF ("TemperatureCalibrationManager:: no stored calibrations (filename = %s), will rely upon default\n", config.filename.c_str ());
        runtime = std::make_shared<Runtime> (defaultStrategy, *calibrationStrategies);
    }

    float calculateTemperature (const size_t index, const float resistance) const {
        if (! runtime)
            return -273.15f;
        return runtime->calculateTemperature (index, resistance);
    }

    bool calibrateTemperatures () {
        std::shared_ptr<typename Collector::Collection> calibrationData = std::make_shared<typename Collector::Collection> ();
        if (! TemperatureCalibrationStaticDataLoader<SENSOR_SIZE, TEMP_START, TEMP_END, TEMP_STEP> ().load (*calibrationData)) {
            DEBUG_PRINTF ("TemperatureCalibrationManager::calibateTemperatures - static load failed\n");
            return false;
        }
        return calibrateTemperaturesFromData (*calibrationData);
    }
    bool calibrateTemperatures (const Collector::TemperatureReadFunc readTemperature, const Collector::ResistanceReadFunc readResistance) {
        std::shared_ptr<typename Collector::Collection> calibrationData = std::make_shared<typename Collector::Collection> ();
        if (! Collector ().collect (*calibrationData, readTemperature, readResistance)) {
            DEBUG_PRINTF ("TemperatureCalibrationManager::calibateTemperatures - collector failed\n");
            return false;
        }
        for (int index = 0; index < calibrationData->temperatures.size (); index++) {
            DEBUG_PRINTF ("= %d,%.2f", index, calibrationData->temperatures [index]);
            for (size_t sensor = 0; sensor < calibrationData->resistances.size (); sensor++)
                DEBUG_PRINTF (",%u", calibrationData->resistances [sensor][index]);
            DEBUG_PRINTF ("\n");
        }
        return calibrateTemperaturesFromData (*calibrationData);
    }

private:
    bool calibrateTemperaturesFromData (const Collector::Collection &calibrationData) {
        Calculator calculator;
        std::shared_ptr<typename Calculator::CalibrationStrategies> calibrationStrategies = std::make_shared<typename Calculator::CalibrationStrategies> ();
        StrategyDefault defaultStrategy;
        if (! calculator.compute (*calibrationStrategies, calibrationData, createStrategyFactoriesForCalibration ()) || ! calculator.computeDefault (defaultStrategy, calibrationData)) {
            DEBUG_PRINTF ("TemperatureCalibrationManager::calibateTemperatures - calculator failed\n");
            return false;
        }
        Storage::serialize (config.filename, defaultStrategy, *calibrationStrategies);
        runtime = std::make_shared<Runtime> (defaultStrategy, *calibrationStrategies);
        return true;
    }

protected:
    void collectDiagnostics (JsonVariant &obj) const override {
        JsonObject sub = obj ["cal"].to<JsonObject> ();
        sub ["loaded"] = _loaded;
    }
};

// -----------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------
