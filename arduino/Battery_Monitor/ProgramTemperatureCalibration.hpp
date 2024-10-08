
// -----------------------------------------------------------------------------------------------

template <size_t SENSOR_SIZE, float TEMP_START, float TEMP_END, float TEMP_STEP>
class TemperatureCalibrationDefinitions {
public:
    static constexpr size_t TEMP_SIZE = static_cast <size_t> ((TEMP_END - TEMP_START) / TEMP_STEP) + 1;
    using Temperatures = std::array <float, TEMP_SIZE>;
    using Resistances = std::array <uint16_t, TEMP_SIZE>;
    struct Collection {
        Temperatures temperatures;
        std::array <Resistances, SENSOR_SIZE> resistances;
    };
};

// -----------------------------------------------------------------------------------------------

template <size_t SENSOR_SIZE, float TEMP_START, float TEMP_END, float TEMP_STEP>
class TemperatureCalibrationCollector {
public:
    using Definitions = TemperatureCalibrationDefinitions <SENSOR_SIZE, TEMP_START, TEMP_END, TEMP_STEP>;
    using Collection = typename Definitions::Collection;
    using TemperatureReadFunc = std::function <float ()>;
    using ResistanceReadFunc = std::function <uint16_t (size_t)>;

    bool collect (Collection &collection, const TemperatureReadFunc readTemperature, const ResistanceReadFunc readResistance) {
        static constexpr int DELAY = 100, COUNT = 5*1000/DELAY;

        if (readTemperature () > (TEMP_START - TEMP_STEP)) {
            float temperatureActual = readTemperature ();
            DEBUG_PRINTF ("TemperatureCalibrationCollector: waiting for DS18B20 temperature to reach %.2f°C .. (at %.2f°C) ", TEMP_START - TEMP_STEP, temperatureActual);
            int where = 0;
            while (temperatureActual > (TEMP_START - TEMP_STEP)) {
                if (++ where > COUNT)
                    DEBUG_PRINTF ("(at %.2f°C) ", temperatureActual), where = 0;
                delay (DELAY);
                temperatureActual = readTemperature ();
            }
            DEBUG_PRINTF ("done\n");
        }

        DEBUG_PRINTF ("TemperatureCalibrationCollector: collecting from %.2f°C to %.2f°C in %.2f°C steps (%d total) for %d NTC resistances\n", TEMP_START, TEMP_END, TEMP_STEP, Definitions::TEMP_SIZE, SENSOR_SIZE);
        for (size_t step = 0; step < Definitions::TEMP_SIZE; step ++) {
            float temperatureTarget = TEMP_START + (step * TEMP_STEP), temperatureActual = readTemperature ();
            DEBUG_PRINTF ("TemperatureCalibrationCollector: waiting for DS18B20 temperature to reach %.2f°C ... (at %.2f°C) ", temperatureTarget, temperatureActual);
            int where = 0;
            while (temperatureActual < temperatureTarget) {
                if (++ where > COUNT)
                    DEBUG_PRINTF ("(at %.2f°C) ", temperatureActual), where = 0;
                delay (DELAY);
                temperatureActual = readTemperature ();
            }
            DEBUG_PRINTF ("reached %.2f°C, collecting %d NTC resistances ... ", temperatureActual, SENSOR_SIZE);
            for (size_t sensor = 0; sensor < SENSOR_SIZE; sensor ++)
                collection.resistances [sensor] [step] = readResistance (sensor);
            collection.temperatures [step] = temperatureActual;
            DEBUG_PRINTF ("done\n");
        }
        return true;
    }
};

// -----------------------------------------------------------------------------------------------

template <size_t SENSOR_SIZE, float TEMP_START, float TEMP_END, float TEMP_STEP>
class TemperatureCalibrationAdjustmentStrategy {
public:
    using Definitions = TemperatureCalibrationDefinitions <SENSOR_SIZE, TEMP_START, TEMP_END, TEMP_STEP>;
    using StatsErrors = Stats <float>;

    virtual ~TemperatureCalibrationAdjustmentStrategy () = default;
    virtual bool calibrate (const Definitions::Temperatures& temperatures, const Definitions::Resistances& resistances) = 0;
    virtual bool calculate (float& temperature, const uint16_t resistance) const = 0;
    virtual void serialize (JsonObject& obj) const = 0;
    virtual void deserialize (JsonObject& obj) = 0;
    virtual String getName () const = 0;
    virtual String getDetails () const = 0;

    StatsErrors calculateStatsErrors (const Definitions::Temperatures& t, const Definitions::Resistances& r) const {
        StatsErrors stats;
        for (size_t index = 0; index < Definitions::TEMP_SIZE; index ++) { 
            float temperature;
            if (calculate (temperature, r [index]))
                stats += std::abs (temperature - t [index]);
        }
        return stats;
    }
};

template <size_t SENSOR_SIZE, float TEMP_START, float TEMP_END, float TEMP_STEP>
class TemperatureCalibrationAdjustmentStrategy_Lookup : public TemperatureCalibrationAdjustmentStrategy <SENSOR_SIZE, TEMP_START, TEMP_END, TEMP_STEP> {
public:
    using Definitions = TemperatureCalibrationDefinitions <SENSOR_SIZE, TEMP_START, TEMP_END, TEMP_STEP>;

private:    
    Definitions::Temperatures temperatures;
    Definitions::Resistances resistances;

public:
    bool calibrate (const Definitions::Temperatures& t, const Definitions::Resistances& r) override {
        temperatures = t;
        resistances = r;
        return true;
    }
    bool calculate (float& temperature, const uint16_t resistance) const override {
        const auto it = std::lower_bound (resistances.begin (), resistances.end (), resistance, std::greater <uint16_t> ());
        if (it == resistances.end () || it == resistances.begin ()) return false;
        const size_t index = std::distance (resistances.begin (), it);
        temperature = temperatures [index - 1] + (temperatures [index] - temperatures [index - 1]) * (resistance - resistances [index - 1]) / (resistances [index] - resistances [index - 1]);
        return true;
    }
    //
    void serialize (JsonObject& obj) const override {
        JsonArray t = obj ["temperatures"].to <JsonArray> (), r = obj ["resistances"].to <JsonArray> ();
        for (size_t i = 0; i < Definitions::TEMP_SIZE; i ++)
            t.add (temperatures [i]), r.add (resistances [i]);
    }
    void deserialize (JsonObject& obj) override {
        JsonArray t = obj ["temperatures"], r = obj ["resistances"];
        for (size_t i = 0; i < Definitions::TEMP_SIZE; i ++)
            temperatures [i] = t [i], resistances [i] = r [i];
    }
    String getName () const override {
        return "Lookup"; 
    }
    String getDetails () const override {
        return "Lookup (N=" + IntToString (temperatures.size ()) + ")";
    }
};

template <size_t SENSOR_SIZE, float TEMP_START, float TEMP_END, float TEMP_STEP>
class TemperatureCalibrationAdjustmentStrategy_Steinhart : public TemperatureCalibrationAdjustmentStrategy <SENSOR_SIZE, TEMP_START, TEMP_END, TEMP_STEP> {
public:
    using Definitions = TemperatureCalibrationDefinitions <SENSOR_SIZE, TEMP_START, TEMP_END, TEMP_STEP>;

private:
    float A, B, C;

public:
    TemperatureCalibrationAdjustmentStrategy_Steinhart (const float a = 0.0, const float b = 0.0, const float c = 0.0): A (a), B (b), C (c) {}

    bool calibrate (const Definitions::Temperatures& t, const Definitions::Resistances& r) override {
        if (t.size () < 3 || r.size () < 3 || t.size () != r.size ()) {
            DEBUG_PRINTF ("calibrateSteinhartHart: at least 3 matching temperature-resistance pairs are required for calibration.\n");
            return false;
        }

        std::array <double, Definitions::TEMP_SIZE> T, Y, L;
        for (size_t i = 0; i < t.size (); i ++) {
            T [i] = t [i] + 273.15;
            Y [i] = 1.0 / T [i];
            L [i] = log (r [i]);
        }

        double sum_L = 0, sum_Y = 0, sum_L2 = 0, sum_L3 = 0, sum_LY = 0, sum_L2Y = 0;
        for (size_t i = 0; i < L.size (); i ++) {
            sum_L   += L [i];
            sum_Y   += Y [i];
            sum_L2  += L [i] * L [i];
            sum_L3  += L [i] * L [i] * L [i];
            sum_LY  += L [i] * Y [i];
            sum_L2Y += L [i] * L [i] * Y [i];
        }

        const double num = static_cast <double> (L.size ());
        const double det   = num   * (sum_L2 * sum_L2  - sum_L  * sum_L3)  - sum_L * (sum_L  * sum_L2  - sum_L  * sum_L3)  + sum_L2 * (sum_L  * sum_L  - num    * sum_L2);
        const double det_A = sum_Y * (sum_L2 * sum_L2  - sum_L  * sum_L3)  - sum_L * (sum_LY * sum_L2  - sum_L  * sum_L2Y) + sum_L2 * (sum_LY * sum_L  - sum_Y  * sum_L2);
        const double det_B = num   * (sum_LY * sum_L2  - sum_L  * sum_L2Y) - sum_Y * (sum_L  * sum_L2  - sum_L  * sum_L3)  + sum_L2 * (sum_L  * sum_LY - num    * sum_L2Y);
        const double det_C = num   * (sum_L2 * sum_L2Y - sum_LY * sum_L3)  - sum_L * (sum_L  * sum_L2Y - sum_LY * sum_L2)  + sum_Y  * (sum_L  * sum_L3 - sum_L2 * sum_L2);

        constexpr double epsilon = 1e-10;
        if (std::abs (det) < epsilon) {
            DEBUG_PRINTF ("calibrateSteinhartHart: determinant is too close to zero, solution may be unstable.\n");
            return false;
        }
        A = static_cast <float> (det_A / det);
        B = static_cast <float> (det_B / det);
        C = static_cast <float> (det_C / det);
        return true;
    }
    bool calculate (float& temperature, const uint16_t resistance) const override {
        const float logR = log (resistance);
        temperature = 1.0f / (A + B * logR + C * logR * logR * logR) - 273.15f;
        return true;
    }
    //
    void serialize (JsonObject& obj) const override {
        obj ["A"] = A, obj ["B"] = B, obj ["C"] = C;
    }
    void deserialize (JsonObject& obj) override {
        A = obj ["A"], B = obj ["B"], C = obj ["C"];
    }
    String getName () const override {
        return "Steinhart";
    }
    String getDetails () const override {
        return "Steinhart (A=" + FloatToString (A) + ", B=" + FloatToString (B) + ", C=" + FloatToString (C) + ")";
    }
};

template <size_t SENSOR_SIZE, float TEMP_START, float TEMP_END, float TEMP_STEP>
class TemperatureCalibrationAdjustmentStrategy_SteinhartAverage: public TemperatureCalibrationAdjustmentStrategy_Steinhart <SENSOR_SIZE, TEMP_START, TEMP_END, TEMP_STEP> {
public:
    using Definitions = TemperatureCalibrationDefinitions <SENSOR_SIZE, TEMP_START, TEMP_END, TEMP_STEP>;
    using Base = TemperatureCalibrationAdjustmentStrategy_Steinhart <SENSOR_SIZE, TEMP_START, TEMP_END, TEMP_STEP>;

public:
    TemperatureCalibrationAdjustmentStrategy_SteinhartAverage (const float a = 0.0, const float b = 0.0, const float c = 0.0): Base (a, b, c) {}

    bool calibrate (const Definitions::Collection &c) {
        typename Definitions::Resistances resistances;
        for (size_t index = 0; index < Definitions::TEMP_SIZE; index ++) {
            uint32_t average = 0;
            for (size_t sensor = 0; sensor < SENSOR_SIZE; sensor ++)
                average += static_cast <uint32_t> (c.resistances [sensor] [index]);
            resistances [index] = static_cast <uint16_t> (average / SENSOR_SIZE);
        }
        return Base::calibrate (c.temperatures, resistances);
    }
};

// -----------------------------------------------------------------------------------------------

template <size_t SENSOR_SIZE, float TEMP_START, float TEMP_END, float TEMP_STEP>
class TemperatureCalibrationCalculator {
public:
    using Definitions = TemperatureCalibrationDefinitions <SENSOR_SIZE, TEMP_START, TEMP_END, TEMP_STEP>;
    using StrategyType = TemperatureCalibrationAdjustmentStrategy <SENSOR_SIZE, TEMP_START, TEMP_END, TEMP_STEP>;
    using StrategyDefault = TemperatureCalibrationAdjustmentStrategy_SteinhartAverage <SENSOR_SIZE, TEMP_START, TEMP_END, TEMP_STEP>;
    using StrategyFactory = std::function <std::shared_ptr <StrategyType> ()>;
    using StrategyFactories = std::map <String, StrategyFactory>;

    using __CalibrationStrategySet = std::vector <std::shared_ptr <StrategyType>>;
    using CalibrationStrategies = std::array <__CalibrationStrategySet, SENSOR_SIZE>;

    bool compute (CalibrationStrategies& calibrations, const Definitions::Collection &collection, const StrategyFactories &factories) {
        DEBUG_PRINTF ("TemperatureCalibrationCollector: calculating from %.2f°C to %.2f°C in %.2f°C steps (%d total) for %d NTC resistances\n", TEMP_START, TEMP_END, TEMP_STEP, Definitions::TEMP_SIZE, SENSOR_SIZE);
        for (size_t sensor = 0; sensor < SENSOR_SIZE; sensor ++) {
            DEBUG_PRINTF ("[%d/%d]: ", sensor + 1, SENSOR_SIZE);
            for (const auto& factory : factories) {
                auto name = factory.first, strategy = factory.second ();
                if (strategy->calibrate (collection.temperatures, collection.resistances [sensor])) {
                    calibrations [sensor].push_back (strategy);
                    auto stats = strategy->calculateStatsErrors (collection.temperatures, collection.resistances [sensor]);
                    DEBUG_PRINTF ("%s%s (avg_error=%.2f,max_error=%.2f,min_error=%.2f)", calibrations [sensor].size () > 0 ? ", ": "", name.c_str (), stats.avg (), stats.max (), stats.min ());
                } else {
                    DEBUG_PRINTF ("%s%s (failed)", calibrations [sensor].size () > 0 ? ", ": "", name.c_str ());
                    //return false;
                }
            }
            DEBUG_PRINTF ("\n");
        }
        return true;
    }
    bool computeDefault (StrategyDefault& strategyDefault, const Definitions::Collection& collection) {
        return strategyDefault.calibrate (collection);
    }
};

// -----------------------------------------------------------------------------------------------

template <size_t SENSOR_SIZE, float TEMP_START, float TEMP_END, float TEMP_STEP>
class TemperatureCalibrationStorage {
public:
    using Definitions = TemperatureCalibrationDefinitions <SENSOR_SIZE, TEMP_START, TEMP_END, TEMP_STEP>;
    using Calculator = TemperatureCalibrationCalculator <SENSOR_SIZE, TEMP_START, TEMP_END, TEMP_STEP>;

    using StrategyDefault = typename Calculator::StrategyDefault;
    using StrategyFactory = typename Calculator::StrategyFactory;
    using StrategyFactories = typename Calculator::StrategyFactories;
    using CalibrationStrategies = typename Calculator::CalibrationStrategies;

    static bool serialize (const String& filename, const StrategyDefault& defaultStrategy, const CalibrationStrategies& calibrationStrategies) {
        JsonDocument doc;
        for (size_t i = 0; i < SENSOR_SIZE; i ++) {
            JsonObject sensor = doc ["sensor" + IntToString (i)].to <JsonObject> ();
            JsonObject strategies = sensor ["strategies"].to <JsonObject> ();
            for (const auto& strategy : calibrationStrategies [i]) {
                JsonObject strategyDetails = strategies [String (strategy->getName ())].to <JsonObject> ();
                strategy->serialize (strategyDetails);
            }
        }
        JsonObject strategyDefault = doc ["default"].to <JsonObject> ();
        defaultStrategy.serialize (strategyDefault);
        
        //
        if (!SPIFFS.begin (true)) {
            Serial.println ("SPIFFS: failed to mount");
            return false;
        }
        File file = SPIFFS.open (filename, FILE_WRITE);
        if (!file) {
            Serial.printf ("SPIFFS: Failed to open file (%s) for writing\n", filename.c_str ());
            return false;
        }
        serializeJson (doc, file);
        serializeJson (doc, Serial);
        file.close ();
        //
     
        return true;
    }

    static bool deserialize (const String& filename, StrategyDefault& defaultStrategy, CalibrationStrategies& calibrationStrategies, const StrategyFactories& strategyFactories) {

        //
        if (!SPIFFS.begin (true)) {
            Serial.println ("SPIFFS: failed to mount");
            return false;
        }
        File file = SPIFFS.open (filename, FILE_READ);
        if (!file) {
            Serial.printf ("SPIFFS: Failed to open file (%s) for reading\n", filename.c_str ());
            return false;
        }
        JsonDocument doc;
        deserializeJson (doc, file);
        file.close ();
        //

        int count = 0;
        for (size_t i = 0; i < SENSOR_SIZE; i ++) {
            JsonObject sensor = doc ["sensor" + IntToString (i)].as <JsonObject> ();
            if (sensor) {
                for (JsonPair kv : sensor ["strategies"].as <JsonObject> ()) {
                    auto factoryIt = strategyFactories.find (String (kv.key ().c_str ()));
                    if (factoryIt != strategyFactories.end ()) {
                        auto strategy = factoryIt->second ();
                        JsonObject details = kv.value ().as <JsonObject> ();
                        strategy->deserialize (details);
                        calibrationStrategies [i].push_back (strategy);
                        count ++;
                    }
                }
            }
        }
        JsonObject details = doc ["default"].as <JsonObject> ();
        if (details)
            defaultStrategy.deserialize (details), count ++;

        return count > 0;
    }
};

// -----------------------------------------------------------------------------------------------

template <size_t SENSOR_SIZE, float TEMP_START, float TEMP_END, float TEMP_STEP>
class TemperatureCalibrationRuntime {
public:
    using Definitions = TemperatureCalibrationDefinitions <SENSOR_SIZE, TEMP_START, TEMP_END, TEMP_STEP>;
    using Calculator = TemperatureCalibrationCalculator <SENSOR_SIZE, TEMP_START, TEMP_END, TEMP_STEP>;

    using StrategyDefault = typename Calculator::StrategyDefault;
    using StrategyFactory = typename Calculator::StrategyFactory;
    using CalibrationStrategies = typename Calculator::CalibrationStrategies;

private:
    StrategyDefault defaultStrategy;
    CalibrationStrategies calibrationStrategies;

public:
    TemperatureCalibrationRuntime (const StrategyDefault& defaultStrategy, const CalibrationStrategies& calibrationStrategies): defaultStrategy (defaultStrategy), calibrationStrategies (calibrationStrategies) {}

    float calculateTemperature (const size_t index, const uint16_t resistance) const {
        float temperature;
        for (const auto& strategy : calibrationStrategies [index])
            if (strategy->calculate (temperature, resistance))
                return temperature;
        if (defaultStrategy.calculate (temperature, resistance))
            return temperature;
        DEBUG_PRINTF ("TemperatureCalibrationRuntime::calculateTemperature: failed");
        return NAN;
    }
};

// -----------------------------------------------------------------------------------------------

template <size_t SENSOR_SIZE, float TEMP_START, float TEMP_END, float TEMP_STEP>
class TemperatureCalibrationManager: public Component {
public:
    using Definitions = TemperatureCalibrationDefinitions <SENSOR_SIZE, TEMP_START, TEMP_END, TEMP_STEP>;
    using Collector = TemperatureCalibrationCollector <SENSOR_SIZE, TEMP_START, TEMP_END, TEMP_STEP>;
    using Calculator = TemperatureCalibrationCalculator <SENSOR_SIZE, TEMP_START, TEMP_END, TEMP_STEP>;
    using Storage = TemperatureCalibrationStorage <SENSOR_SIZE, TEMP_START, TEMP_END, TEMP_STEP>;
    using Runtime = TemperatureCalibrationRuntime <SENSOR_SIZE, TEMP_START, TEMP_END, TEMP_STEP>;

    using StrategyLookup = TemperatureCalibrationAdjustmentStrategy_Lookup <SENSOR_SIZE, TEMP_START, TEMP_END, TEMP_STEP>;
    using StrategySteinhart = TemperatureCalibrationAdjustmentStrategy_Steinhart <SENSOR_SIZE, TEMP_START, TEMP_END, TEMP_STEP>;
    using StrategyDefault = typename Calculator::StrategyDefault;
    using StrategyFactories = typename Calculator::StrategyFactories;
    using CalibrationStrategies = typename Calculator::CalibrationStrategies;

    struct Config {
        String filename;
        struct { float A, B, C; } steinhartDefault;
    };

private:
    const Config &config;
    std::shared_ptr <Runtime> runtime;

    StrategyFactories createStrategyFactories () const {
        return StrategyFactories {
            { "Lookup", [] { return std::make_shared <StrategyLookup> (); } },
            { "Steinhart", [] { return std::make_shared <StrategySteinhart> (); } }
        };
    }

public:
    TemperatureCalibrationManager (const Config& cfg) : config (cfg) {}

    void begin () override {
        CalibrationStrategies calibrationStrategies;
        StrategyDefault defaultStrategy (config.steinhartDefault.A, config.steinhartDefault.B, config.steinhartDefault.C);
        if (!Storage::deserialize (config.filename, defaultStrategy, calibrationStrategies, createStrategyFactories ()))
            DEBUG_PRINTF ("TemperatureCalibrationManager:: no stored calibrations (filename = %s), will rely upon default\n", config.filename.c_str ());
        runtime = std::make_shared <Runtime> (defaultStrategy, calibrationStrategies);
    }

    float calculateTemperature (const size_t index, const float resistance) const {
        if (!runtime) return -273.15f;
        return runtime->calculateTemperature (index, resistance);
    }

    bool calibrateTemperatures (const Collector::TemperatureReadFunc readTemperature, const Collector::ResistanceReadFunc readResistance) {
        Collector collector;
        typename Collector::Collection calibrationData;
        if (!collector.collect (calibrationData, readTemperature, readResistance)) {
            DEBUG_PRINTF ("TemperatureCalibrationManager::calibateTemperatures - collector failed\n");
            return false;
        }

        Calculator calculator;
        typename Calculator::CalibrationStrategies calibrationStrategies;
        StrategyDefault defaultStrategy;
        if (!calculator.compute (calibrationStrategies, calibrationData, createStrategyFactories ()) || !calculator.computeDefault (defaultStrategy, calibrationData)) {
            DEBUG_PRINTF ("TemperatureCalibrationManager::calibateTemperatures - calculator failed\n");
            return false;
        }
        DEBUG_PRINTF ("defaultStrategy: %s\n", defaultStrategy.getDetails ().c_str ());

        Storage::serialize (config.filename, defaultStrategy, calibrationStrategies);
        runtime = std::make_shared <Runtime> (defaultStrategy, calibrationStrategies);
        return true;
    }
};

// -----------------------------------------------------------------------------------------------
