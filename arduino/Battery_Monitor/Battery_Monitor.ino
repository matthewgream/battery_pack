
// -----------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------

#include "Program.hpp"

// -----------------------------------------------------------------------------------------------

#define BUILD_Y ((__DATE__[7] - '0') * 1000 + (__DATE__[8] - '0') * 100 + (__DATE__[9] - '0') * 10 + (__DATE__[10] - '0'))
#define BUILD_M ((__DATE__[0] == 'J') ? ((__DATE__[1] == 'a') ? 1 : ((__DATE__[2] == 'n') ? 6 : 7)) : (__DATE__[0] == 'F') ? 2 : (__DATE__[0] == 'M') ? ((__DATE__[2] == 'r') ? 3 : 5) \
    : (__DATE__[0] == 'A') ? ((__DATE__[2] == 'p') ? 4 : 8) : (__DATE__[0] == 'S') ? 9 : (__DATE__[0] == 'O') ? 10 : (__DATE__[0] == 'N') ? 11 : (__DATE__[0] == 'D') ? 12 : 0)
#define BUILD_D ((__DATE__[4] == ' ') ? (__DATE__[5] - '0') : (__DATE__[4] - '0') * 10 + (__DATE__[5] - '0'))
#define BUILD_T __TIME__

static inline constexpr const char __build_name [] = DEFAULT_NAME;
static inline constexpr const char __build_vers [] = DEFAULT_VERS;
static inline constexpr const char __build_time [] = {
    BUILD_Y/1000 + '0', (BUILD_Y%1000)/100 + '0', (BUILD_Y%100)/10 + '0', BUILD_Y%10 + '0',  BUILD_M/10 + '0', BUILD_M%10 + '0',  BUILD_D/10 + '0', BUILD_D%10 + '0',
    BUILD_T [0], BUILD_T [1], BUILD_T [3], BUILD_T [4], BUILD_T [6], BUILD_T [7],
    '\0'
};  

const String build_info (String (__build_name) + " V" + String (__build_vers) + "-" + String (__build_time));

// -----------------------------------------------------------------------------------------------

Program *program;

#ifdef TESTING
static void test_fanInterface () {
    FanInterface::Config config = { .hardware = { .I2C_ADDR = OpenSmart_QuadMotorDriver::I2cAddress, .PIN_I2C_SDA = 1, .PIN_I2C_SCL = 0, .PIN_PWMS = { 2, 3, 4, 5 }, .frequency = 20000 }, .MIN_SPEED = 192, .MAX_SPEED = 255 };
    FanInterfaceStrategy_motorAll strategy;
    FanInterface driver (config, strategy);
    driver.begin ();
    FanInterface::FanSpeedType speed = 0;
    while (1) {
        DEBUG_PRINTF ("speed=%d\n", speed);
        driver.setSpeed (speed);
        delay (15*1000);
        speed += 16;
        if (speed > 255) speed = 0;
   }
}
static void test_muxInterface () {
    typedef uint16_t AdcValueType;
    MuxInterface_CD74HC4067 <AdcValueType>::Config config = { .PIN_EN = 20, .PIN_SIG = 6, .PIN_ADDR = { 10, 9, 8, 7 } };
    MuxInterface_CD74HC4067 <AdcValueType> interface (config);

    interface.enable (true);
    while (1) {
        AdcValueType value = interface.get (15);
        DEBUG_PRINTF ("value=%x\n", value);
        delay (5*1000);
    }
}
#endif





template <size_t SENSOR_COUNT, float TEMP_START, float TEMP_END, float TEMP_STEP>
class TemperatureCalibrationDefinitions {
public:
    static constexpr size_t TEMP_COUNT = static_cast <size_t> ((TEMP_END - TEMP_START) / TEMP_STEP) + 1;
    using TemperatureArray = std::array <float, TEMP_COUNT>;
    using ResistanceArray = std::array <uint16_t, TEMP_COUNT>;
    using ResistanceSet = std::array <ResistanceArray, SENSOR_COUNT>;
    struct TemperatureResistanceCollection {
        TemperatureArray temperatures;
        ResistanceSet resistances;
    };
};

template <size_t SENSOR_COUNT, float TEMP_START, float TEMP_END, float TEMP_STEP>
class TemperatureCalibrationDataCollector {
public:
    using Definitions = TemperatureCalibrationDefinitions <SENSOR_COUNT, TEMP_START, TEMP_END, TEMP_STEP>;
    using Collection = typename Definitions::TemperatureResistanceCollection;
    using TemperatureReadFunc = std::function <float ()>;
    using ResistanceReadFunc = std::function <uint16_t (size_t)>;

    static Collection collect (const TemperatureReadFunc readTemperature, const ResistanceReadFunc readResistance) {
        Collection collection;
        for (size_t i = 0; i < Definitions::TEMP_COUNT; i ++) {
            float temperatureTarget = TEMP_START + i * TEMP_STEP, temperatureActual;
            DEBUG_PRINTF ("TemperatureCalibrationDataCollector: waiting for temperature to reach %.2f ...", temperatureTarget);
            while ((temperatureActual = readTemperature ()) < temperatureTarget)
                delay (100);
            DEBUG_PRINTF ("reached %.2f, collecting %d resistances ... ", temperatureActual, SENSOR_COUNT);
            for (size_t sensor = 0; sensor < SENSOR_COUNT; sensor ++)
                collection.resistances [sensor] [i] = readResistance (sensor);
            collection.temperatures [i] = temperatureActual;
            DEBUG_PRINTF ("done\n");
        }
        return collection;
    }
};

template <size_t SENSOR_COUNT, float TEMP_START, float TEMP_END, float TEMP_STEP>
class TemperatureCalibrationAdjustmentStrategy {
public:
    using Definitions = TemperatureCalibrationDefinitions <SENSOR_COUNT, TEMP_START, TEMP_END, TEMP_STEP>;

    virtual ~TemperatureCalibrationAdjustmentStrategy () = default;
    virtual float calibrate (const Definitions::TemperatureArray& temperatures, const Definitions::ResistanceArray& resistances) = 0;
    virtual float calculateTemperature (const uint16_t resistance) const = 0;
    virtual void serialize (JsonObject& obj) const = 0;
    virtual void deserialize (JsonObject& obj) = 0;
    virtual const char* getName () const = 0;

protected:
    float calculateError (const Definitions::TemperatureArray& t, const Definitions::ResistanceArray& r) const override {
        float maxError = 0.0f;
        for (size_t i = 0; i < Definitions::TEMP_COUNT; i ++)
            maxError = std::max (maxError, std::abs (calculateTemperature (r [i]) - t [i]));
        return maxError;
    }
};

template <size_t SENSOR_COUNT, float TEMP_START, float TEMP_END, float TEMP_STEP>
class TemperatureCalibrationAdjustmentStrategy_Lookup : public TemperatureCalibrationAdjustmentStrategy <SENSOR_COUNT, TEMP_START, TEMP_END, TEMP_STEP> {
private:
    using Definitions = TemperatureCalibrationDefinitions <SENSOR_COUNT, TEMP_START, TEMP_END, TEMP_STEP>;
    Definitions::TemperatureArray temperatures;
    Definitions::ResistanceArray resistances;

public:
    float calibrate (const Definitions::TemperatureArray& t, const Definitions::ResistanceArray& r) override {
        temperatures = t;
        resistances = r;
        return calculateError (t, r);
    }
    float calculateTemperature (const uint16_t resistance) const override {
        const auto it = std::lower_bound (resistances.begin (), resistances.end (), resistance, std::greater <uint16_t> ());
        if (it == resistances.end ()) return temperatures.back ();
        if (it == resistances.begin ()) return temperatures.front ();
        const size_t index = std::distance (resistances.begin (), it);
        return temperatures [index - 1] + (temperatures [index] - temperatures [index - 1]) * (resistance - resistances [index - 1]) / (resistances [index] - resistances [index - 1]);
    }

    void serialize (JsonObject& obj) const override {
        obj ["name"] = getName ();
        JsonArray t = obj.createNestedArray ("temperatures"), r = obj.createNestedArray ("resistances");
        for (size_t i = 0; i < Definitions::TEMP_COUNT; i ++)
            t.add (temperatures [i]), r.add (resistances [i]);
    }
    void deserialize (JsonObject& obj) override {
        JsonArray t = obj ["temperatures"], r = obj ["resistances"];
        for (size_t i = 0; i < Definitions::TEMP_COUNT; i ++)
            temperatures [i] = t [i], resistances [i] = r [i];
    }

    const char* getName () const override { return "Lookup"; }
};

template <size_t SENSOR_COUNT, float TEMP_START, float TEMP_END, float TEMP_STEP>
class TemperatureCalibrationAdjustmentStrategy_Steinhart : public TemperatureCalibrationAdjustmentStrategy <SENSOR_COUNT, TEMP_START, TEMP_END, TEMP_STEP> {
private:
    using Definitions = TemperatureCalibrationDefinitions <SENSOR_COUNT, TEMP_START, TEMP_END, TEMP_STEP>;
    using TemperatureArray = typename Definitions::TemperatureArray;
    float A, B, C;

public:
    float calibrate (const Definitions::TemperatureArray& t, const Definitions::ResistanceArray& r) override {
        calibrateSteinhartHart (t, r);
        return calculateError (t, r);
    }
    float calculateTemperature (const uint16_t resistance) const override {
        const float logR = log (resistance);
        return 1.0f / (A + B * logR + C * logR * logR * logR) - 273.15f;
    }

    void serialize (JsonObject& obj) const override {
        obj ["name"] = getName ();
        obj ["A"] = A, obj ["B"] = B, obj ["C"] = C;
    }
    void deserialize (JsonObject& obj) override {
        A = obj ["A"], B = obj ["B"], C = obj ["C"];
    }

    const char* getName () const override { return "Steinhart"; }

private:
    void calibrateSteinhartHart (const Definitions::TemperatureArray& t, const Definitions::ResistanceArray& r) {

        if (t.size () < 3 || r.size () < 3 || t.size () != r.size ()) {
            DEBUG_PRINTF ("calibrateSteinhartHart: at least 3 matching temperature-resistance pairs are required for calibration.");
            return;
        }

        TemperatureArray T, Y, L;
        for (size_t i = 0; i < t.size (); i ++) {
            T [i] = t [i] + 273.15f;
            Y [i] = 1.0f / T [i];
            L [i] = log (r [i]);
        }

        float sum_L = 0, sum_Y = 0, sum_L2 = 0, sum_L3 = 0, sum_LY = 0, sum_L2Y = 0;
        for (size_t i = 0; i < L.size (); i ++) {
            sum_L += L [i];
            sum_Y += Y [i];
            sum_L2 += L [i] * L [i];
            sum_L3 += L [i] * L [i] * L [i];
            sum_LY += L [i] * Y [i];
            sum_L2Y += L [i] * L [i] * Y [i];
        }

        const float n = static_cast <float> (L.size ());
        const float det = n * (sum_L2 * sum_L2 - sum_L * sum_L3) - sum_L * (sum_L * sum_L2 - sum_L * sum_L3) + sum_L2 * (sum_L * sum_L - n * sum_L2);
        const float det_A = sum_Y * (sum_L2 * sum_L2 - sum_L * sum_L3) - sum_L * (sum_LY * sum_L2 - sum_L * sum_L2Y) + sum_L2 * (sum_LY * sum_L - sum_Y * sum_L2);
        const float det_B = n * (sum_LY * sum_L2 - sum_L * sum_L2Y) - sum_Y * (sum_L * sum_L2 - sum_L * sum_L3) + sum_L2 * (sum_L * sum_LY - n * sum_L2Y);
        const float det_C = n * (sum_L2 * sum_L2Y - sum_LY * sum_L3) - sum_L * (sum_L * sum_L2Y - sum_LY * sum_L2) + sum_Y * (sum_L * sum_L3 - sum_L2 * sum_L2);
        A = det_A / det;
        B = det_B / det;
        C = det_C / det;
    }
};

template <size_t SENSOR_COUNT, float TEMP_START, float TEMP_END, float TEMP_STEP>
class TemperatureCalibrationAdjustmentStrategy_MasterSteinhart : public TemperatureCalibrationAdjustmentStrategy_Steinhart <SENSOR_COUNT, TEMP_START, TEMP_END, TEMP_STEP> {
private:
    using Definitions = TemperatureCalibrationDefinitions<SENSOR_COUNT, TEMP_START, TEMP_END, TEMP_STEP>;
    using Collection = typename Definitions::TemperatureResistanceCollection;
    using ResistanceArray = typename Definitions::ResistanceArray;

public:
    float calibrate (const Collection &c) override {
        ResistanceArray resistances;
        
        for (size_t i = 0; i < Definitions::TEMP_COUNT; i ++) {
            uint32_t average = 0;
            for (size_t sensor = 0; sensor < SENSOR_COUNT; sensor ++)
                average += static_cast <uint32_t> (c.resistances [sensor] [i]);
            resistances [i] = static_cast <uint16_t> (average / SENSOR_COUNT);
        }

        TemperatureCalibrationAdjustmentStrategy_Steinhart <SENSOR_COUNT, TEMP_START, TEMP_END, TEMP_STEP>::calibrateSteinhartHart (c.temperatures, resistances);
        return calculateError (c.temperatures, resistances);
    }

    const char* getName () const override { return "MasterSteinhart"; }
};

template <size_t SENSOR_COUNT, float TEMP_START, float TEMP_END, float TEMP_STEP>
class TemperatureCalibrationCalculator {
public:
    using Definitions = TemperatureCalibrationDefinitions <SENSOR_COUNT, TEMP_START, TEMP_END, TEMP_STEP>;
    using Collection = typename Definitions::TemperatureResistanceCollection;
    using StrategyType = TemperatureCalibrationAdjustmentStrategy <SENSOR_COUNT, TEMP_START, TEMP_END, TEMP_STEP>;
    using StrategyDefault = TemperatureCalibrationAdjustmentStrategy_MasterSteinhart <SENSOR_COUNT, TEMP_START, TEMP_END, TEMP_STEP>;

    struct SensorCalibration {
        std::unique_ptr <StrategyType> strategy;
        float error;
    };
    using SensorCalibrationArray = std::array <SensorCalibration, SENSOR_COUNT>;

    static SensorCalibrationArray compute (const Collection& collection, const std::vector <std::function <std::unique_ptr <StrategyType>()>>& factories, const float errorThreshold) {
        SensorCalibrationArray calibrations;

        for (size_t i = 0; i < SENSOR_COUNT; i ++) {
            float bestError = std::numeric_limits <float>::max ();
            for (const auto& factory : factories) {
                auto strategy = factory ();
                const float error = strategy->calibrate (collection.temperatures, collection.resistances [i]);
                if (error < errorThreshold && error < bestError) {
                    bestError = error;
                    calibrations [i].strategy = std::move (strategy);
                    calibrations [i].error = error;
                }
            }
        }
        return calibrations;
    }

    static std::unique_ptr <StrategyDefault> computeMaster (const Collection& collection) {
        std::unique_ptr <StrategyDefault> master = std::make_unique <StrategyDefault> ();
        master.calibrate (collection);
        return master;
    }
};

template <size_t SENSOR_COUNT, float TEMP_START, float TEMP_END, float TEMP_STEP>
class CalibrationStorage {
public:
    using Definitions = TemperatureCalibrationDefinitions <SENSOR_COUNT, TEMP_START, TEMP_END, TEMP_STEP>;
    using SensorCalibration = typename TemperatureCalibrationCalculator <SENSOR_COUNT, TEMP_START, TEMP_END, TEMP_STEP>::SensorCalibration;
    using SensorCalibrationArray = std::array <SensorCalibration, SENSOR_COUNT>;
    using StrategyType = TemperatureCalibrationAdjustmentStrategy <SENSOR_COUNT, TEMP_START, TEMP_END, TEMP_STEP>;
    using StrategyLookup = TemperatureCalibrationAdjustmentStrategy_Lookup <SENSOR_COUNT, TEMP_START, TEMP_END, TEMP_STEP>;
    using StrategySteinhart = TemperatureCalibrationAdjustmentStrategy_Steinhart <SENSOR_COUNT, TEMP_START, TEMP_END, TEMP_STEP>;

    static bool serialize (const String& filename, const SensorCalibrationArray& calibrations) {
        JsonDocument doc;
        for (size_t i = 0; i < SENSOR_COUNT; i ++)
            calibrations [i].strategy->serialize (doc ["sensor" + IntToString (i)].to <JsonObject> ());
        
        if (!SPIFFS.begin (true)) {
            Serial.println ("An error occurred while mounting SPIFFS");
            return false;
        }
        File file = SPIFFS.open (filename, FILE_WRITE);
        if (!file) {
            Serial.println ("Failed to open file for writing");
            return false;
        }
        serializeJson (doc, file);
        file.close ();
        return true;
    }

    static bool deserialize (const String& filename, const std::vector <std::function <std::unique_ptr <StrategyType>()>>& factories, SensorCalibrationArray& calibrations) {
        if (!SPIFFS.begin(true)) {
            Serial.println("An error occurred while mounting SPIFFS");
            return false;
        }
        File file = SPIFFS.open(filename, FILE_READ);
        if (!file) {
            Serial.println("Failed to open file for reading");
            return false;
        }
        JsonDocument doc;
        deserializeJson (doc, file);
        file.close ();

        for (size_t i = 0; i < SENSOR_COUNT; i ++) {
            JsonObject sensor = doc ["sensor" + IntToString (i)].to <JsonObject> ();
            if (sensor) {
                if (sensor ["name"] == String ("Lookup"))
                    calibrations [i].strategy = std::make_unique <StrategyLookup> ();
                else if (sensor ["name"] == String ("Steinhart"))
                    calibrations [i].strategy = std::make_unique <StrategySteinhart> ();
                calibrations [i].strategy->deserialize (sensor);
            }
        }
        return true;
    }
};

template <size_t SENSOR_COUNT, float TEMP_START, float TEMP_END, float TEMP_STEP>
class CalibrationRuntime {
public:
    using Definitions = TemperatureCalibrationDefinitions <SENSOR_COUNT, TEMP_START, TEMP_END, TEMP_STEP>;
    using SensorCalibration = typename TemperatureCalibrationCalculator <SENSOR_COUNT, TEMP_START, TEMP_END, TEMP_STEP>::SensorCalibration;
    using SensorCalibrationArray = std::array <SensorCalibration, SENSOR_COUNT>;
    using StrategyMaster = TemperatureCalibrationAdjustmentStrategy_MasterSteinhart <SENSOR_COUNT, TEMP_START, TEMP_END, TEMP_STEP>;

private:
    const bool master;
    const SensorCalibrationArray calibrations;
    const StrategyMaster masterStrategy;

public:
    CalibrationRuntime (const SensorCalibrationArray& calibrations, const StrategyMaster& masterStrategy)
        : master (false), calibrations (calibrations), masterStrategy (masterStrategy) {}
    CalibrationRuntime (const StrategyMaster& masterStrategy, const float errorThreshold)
        : master (true), masterStrategy (masterStrategy) {}

    float calculateTemperature (uint16_t resistance, size_t index) const {
        if (index >= SENSOR_COUNT) {
            Serial.println ("Invalid sensor index");
            return NAN;
        }
        return master ? masterStrategy.calculateTemperature (resistance) : calibrations [index].strategy->calculateTemperature (resistance);
    }
};


#if 0



void setup() {
    // ... (initialize Serial, SPIFFS, etc.)

    constexpr size_t NUM_SENSORS = 16;
    constexpr float ERROR_THRESHOLD = 0.5f;

    auto [calibrations, masterStrategy] = CalibrationStorage<NUM_SENSORS>::deserialize("/calibration.json", strategyFactories);
    CalibrationRuntime<NUM_SENSORS> runtime(calibrations, masterStrategy, ERROR_THRESHOLD);

    // Now you can use the runtime in your main loop
}

void loop() {
    float resistance = readResistance(0);  // Read resistance from first sensor
    float temperature = runtime.calculateTemperature(resistance, 0);
    Serial.print("Temperature: ");
    Serial.println(temperature);

    delay(1000);
}
template<size_t NUM_SENSORS, float START_TEMP, float END_TEMP, float STEP>
class TemperatureCalibrationManager {
public:
    struct Config {
        String filename;
        float masterA;
        float masterB;
        float masterC;
        float errorThreshold;
    };

private:
    Config config;
    std::unique_ptr<CalibrationRuntime<NUM_SENSORS>> runtime;
    std::vector<std::function<std::unique_ptr<AdjustmentStrategy>()>> strategyFactories;

public:
    TemperatureCalibrationManager(const Config& cfg) : config(cfg) {
        strategyFactories = {
            []() { return std::make_unique<SteinhartHartStrategy>(); },
            []() { return std::make_unique<LookupTableStrategy>(); },
            [this]() { return std::make_unique<MasterSteinhartStrategy>(config.masterA, config.masterB, config.masterC); }
        };
    }

    bool begin() {
        if (!SPIFFS.begin(true)) {
            Serial.println("An error occurred while mounting SPIFFS");
            return false;
        }

        auto [calibrations, masterStrategy] = CalibrationStorage<NUM_SENSORS>::deserialize(config.filename, strategyFactories);
        
        if (calibrations[0].strategy == nullptr) {
            Serial.println("Failed to load calibration data");
            return false;
        }

        runtime = std::make_unique<CalibrationRuntime<NUM_SENSORS>>(calibrations, masterStrategy, config.errorThreshold);
        return true;
    }

    float adjustTemperature(size_t ntcIndex, float resistance) {
        if (!runtime) {
            Serial.println("Runtime not initialized");
            return NAN;
        }
        return runtime->calculateTemperature(resistance, ntcIndex);
    }

#ifdef CALIBRATION_MODE
    bool calibrate(std::function<float()> tempReader, std::function<float(size_t)> resistanceReader) {
        DataCollector<NUM_SENSORS, START_TEMP, END_TEMP, STEP> collector;
        auto calibrationData = collector.collect(tempReader, resistanceReader);

        CalibrationCalculator<NUM_SENSORS> calculator;
        auto calibrations = calculator.compute(calibrationData.temps, calibrationData.resistances, strategyFactories);
        auto masterStrategy = calculator.calculateMasterCoefficients(calibrationData.temps, calibrationData.resistances);

        bool success = CalibrationStorage<NUM_SENSORS>::serialize(calibrations, masterStrategy, config.filename);
        
        if (success) {
            runtime = std::make_unique<CalibrationRuntime<NUM_SENSORS>>(calibrations, masterStrategy, config.errorThreshold);
            
            // Update master coefficients in config
            config.masterA = masterStrategy.getA();
            config.masterB = masterStrategy.getB();
            config.masterC = masterStrategy.getC();

            // Print master coefficients for manual update
            Serial.println("Update the following master coefficients in your code:");
            Serial.print("masterA = "); Serial.println(config.masterA, 9);
            Serial.print("masterB = "); Serial.println(config.masterB, 9);
            Serial.print("masterC = "); Serial.println(config.masterC, 9);
        }

        return success;
    }
#endif

    void setErrorThreshold(float threshold) {
        config.errorThreshold = threshold;
        if (runtime) {
            runtime->setErrorThreshold(threshold);
        }
    }

    float getErrorThreshold() const {
        return config.errorThreshold;
    }

    float getSensorError(size_t sensorIndex) const {
        if (!runtime) {
            Serial.println("Runtime not initialized");
            return NAN;
        }
        return runtime->getSensorError(sensorIndex);
    }

    const Config& getConfig() const {
        return config;
    }
};

strategyFactories = {
    []() { return std::make_unique<SteinhartHartStrategy>(); },
    []() { return std::make_unique<LookupTableStrategy>(); },
    []() { return std::make_unique<MasterSteinhartStrategy>(); }
};

#endif


void setup () {
    DEBUG_START ();
    DEBUG_PRINTF ("\n*** %s ***\n\n", build_info.c_str ());
    exception_catcher ([&] () { 
        program = new Program ();
        program->setup (); 
    });
}

void loop () {
    exception_catcher ([&] () { 
        program->loop ();
        program->sleep ();
    });
}

// -----------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------
