
// -----------------------------------------------------------------------------------------------
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
// -----------------------------------------------------------------------------------------------

template <size_t SENSOR_SIZE, float TEMP_START, float TEMP_END, float TEMP_STEP>
class TemperatureCalibrationCollector {
public:
    using Definitions = TemperatureCalibrationDefinitions <SENSOR_SIZE, TEMP_START, TEMP_END, TEMP_STEP>;
    using Collection = typename Definitions::Collection;
    using TemperatureReadFunc = std::function <float ()>;
    using ResistanceReadFunc = std::function <uint16_t (size_t)>;

    bool collect (Collection &collection, const TemperatureReadFunc readTemperature, const ResistanceReadFunc readResistance) {
        static constexpr int DELAY = 100, COUNT = 5*1000/DELAY, AVG_MASTER = 12, AVG_SAMPLE = 6;
        MovingAverageWithValue <float, AVG_MASTER> temperature;

        static constexpr float temperatureBegin = (TEMP_START - TEMP_STEP);
        if ((temperature = readTemperature ()) > temperatureBegin) {
            DEBUG_PRINTF ("TemperatureCalibrationCollector: waiting for DS18B20 temperature to reduce to %.2f°C ... \n", temperatureBegin);
            int where = 0;
            while (temperature > temperatureBegin) {
                if (-- where < 0) {
                    DEBUG_PRINTF ("(at %.2f°C)\n", static_cast <float> (temperature));
                    where = COUNT;
                }
                delay (DELAY);
                temperature = readTemperature ();
            }
            DEBUG_PRINTF ("done\n");
        }

        DEBUG_PRINTF ("TemperatureCalibrationCollector: collecting from %.2f°C to %.2f°C in %.2f°C steps (%d total) for %d NTC resistances [master_avg=%d, sample_avg=%d]\n", TEMP_START, TEMP_END, TEMP_STEP, Definitions::TEMP_SIZE, SENSOR_SIZE, AVG_MASTER, AVG_SAMPLE);
        for (size_t step = 0; step < Definitions::TEMP_SIZE; step ++) {

            const float temperatureTarget = TEMP_START + (step * TEMP_STEP);
            DEBUG_PRINTF ("TemperatureCalibrationCollector: waiting for DS18B20 temperature to increase to %.2f°C ... \n", temperatureTarget);
            int where = 0;
            while (temperature < temperatureTarget) {
                if (-- where < 0) {
                    DEBUG_PRINTF ("(at %.2f°C)\n", static_cast <float> (temperature));
                    where = COUNT;
                }
                delay (DELAY);
                temperature = readTemperature ();
            }

            DEBUG_PRINTF ("... reached %.2f°C, collecting %d NTC resistances ...\n", static_cast <float> (temperature), SENSOR_SIZE);
            collection.temperatures [step] = temperature;
            std::array <uint32_t, SENSOR_SIZE> resistances;
            resistances.fill (static_cast <uint32_t> (0));
            for (int count = 0; count < AVG_SAMPLE; count ++)
              for (size_t sensor = 0; sensor < resistances.size (); sensor ++)
                  resistances [sensor] += static_cast <uint32_t> (readResistance (sensor));
            for (size_t sensor = 0; sensor < resistances.size (); sensor ++)                  
                collection.resistances [sensor] [step] = static_cast <uint16_t> (resistances [sensor] / static_cast <uint32_t> (AVG_SAMPLE));
            DEBUG_PRINTF ("done\n");
            
            //
            DEBUG_PRINTF ("= %d,%.2f", step, collection.temperatures [step]);
            for (size_t sensor = 0; sensor < collection.resistances.size (); sensor ++)
                DEBUG_PRINTF (",%u", collection.resistances [sensor] [step]);
            DEBUG_PRINTF ("\n");
        }
        return true;
    }
};

// -----------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------

template <size_t SENSOR_SIZE, float TEMP_START, float TEMP_END, float TEMP_STEP>
class TemperatureCalibrationAdjustmentStrategy {
public:
    using Definitions = TemperatureCalibrationDefinitions <SENSOR_SIZE, TEMP_START, TEMP_END, TEMP_STEP>;
    using StatsErrors = Stats <float>;

    virtual ~TemperatureCalibrationAdjustmentStrategy () = default;
    virtual String calibrate (const Definitions::Temperatures& temperatures, const Definitions::Resistances& resistances) = 0;
    virtual bool calculate (float& temperature, const uint16_t resistance) const = 0;
    virtual void serialize (JsonObject& obj) const = 0;
    virtual void deserialize (JsonObject& obj) = 0;
    virtual String getName () const = 0;
    virtual String getDetails () const = 0;

    StatsErrors calculateStatsErrors (const Definitions::Temperatures& temperatures, const Definitions::Resistances& resistances) const {
        StatsErrors stats;
        assert (temperatures.size () == resistances.size ());
        for (size_t index = 0; index < temperatures.size (); index ++) { 
            float temperature;
            if (calculate (temperature, resistances [index]))
                stats += std::abs (temperature - temperatures [index]);
        }
        return stats;
    }
};

// -----------------------------------------------------------------------------------------------

template <size_t SENSOR_SIZE, float TEMP_START, float TEMP_END, float TEMP_STEP>
class TemperatureCalibrationAdjustmentStrategy_Lookup : public TemperatureCalibrationAdjustmentStrategy <SENSOR_SIZE, TEMP_START, TEMP_END, TEMP_STEP> {
public:
    using Definitions = TemperatureCalibrationDefinitions <SENSOR_SIZE, TEMP_START, TEMP_END, TEMP_STEP>;
    static constexpr const char * NAME = "lookup";

private:    
    Definitions::Temperatures temperatures;
    Definitions::Resistances resistances;

public:
    String calibrate (const Definitions::Temperatures& t, const Definitions::Resistances& r) override {
        temperatures = t;
        resistances = r;
        return String ();
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
        JsonArray t = obj ["T"].to <JsonArray> (), r = obj ["R"].to <JsonArray> ();
        for (size_t index = 0; index < temperatures.size (); index ++)
            t.add (temperatures [index]), r.add (resistances [index]);
    }
    void deserialize (JsonObject& obj) override {
        JsonArray t = obj ["T"], r = obj ["R"];
        for (size_t index = 0; index < temperatures.size (); index ++)
            temperatures [index] = t [index], resistances [index] = r [index];
    }
    String getName () const override { return NAME; }
    String getDetails () const override { return "lookup (N=" + IntToString (temperatures.size ()) + ")"; }
};

// -----------------------------------------------------------------------------------------------

template <size_t SENSOR_SIZE, float TEMP_START, float TEMP_END, float TEMP_STEP>
class TemperatureCalibrationAdjustmentStrategy_Steinhart : public TemperatureCalibrationAdjustmentStrategy <SENSOR_SIZE, TEMP_START, TEMP_END, TEMP_STEP> {
public:
    using Definitions = TemperatureCalibrationDefinitions <SENSOR_SIZE, TEMP_START, TEMP_END, TEMP_STEP>;
    static constexpr const char * NAME = "steinhart";
    typedef struct {
        double A, B, C, D;
    } Config;

private:
    double A, B, C, D;

    inline bool isResistanceReasonable (const uint16_t resistance) const { return resistance > 0 && resistance < 100*1000; }
    inline bool isTemperatureReasonable (const float temperature) const { return temperature > -273.15f && temperature < 200.0f; }

public:
    TemperatureCalibrationAdjustmentStrategy_Steinhart (const double a = 0.0, const double b = 0.0, const double c = 0.0, const double d = 0.0): A (a), B (b), C (c), D (d) {}
    TemperatureCalibrationAdjustmentStrategy_Steinhart (const Config& config): A (config.A), B (config.B), C (config.C), D (config.D) {}

    String calibrate (const Definitions::Temperatures& temperatures, const Definitions::Resistances& resistances) override {
        assert (temperatures.size () >= 4 && temperatures.size () == resistances.size ());

        // build matrices
        double siz = static_cast <double> (temperatures.size ());
        double sum_Y = 0;
        double sum_L1 = 0, sum_L2 = 0, sum_L3 = 0, sum_L4 = 0, sum_L5 = 0, sum_L6 = 0;
        double sum_YL1 = 0, sum_YL2 = 0, sum_YL3 = 0;
        for (size_t index = 0; index < temperatures.size (); index ++) {
              if (!isTemperatureReasonable (temperatures [index]))
                  return String ("invalid temperature at index ") + IntToString (index);
              if (!isResistanceReasonable (resistances [index]))
                  return String ("invalid resistance at index ") + IntToString (index);
              const double Y = 1.0 / (temperatures [index] + 273.15);
              const double L1 = std::log (static_cast <double> (resistances [index])), L2 = L1 * L1, L3 = L2 * L1;
              sum_Y   +=  Y;
              sum_L1  += L1;
              sum_L2  += L2;
              sum_L3  += L3;
              sum_L4  += L2 * L2;
              sum_L5  += L2 * L3;
              sum_L6  += L3 * L3;
              sum_YL1 +=  Y * L1;
              sum_YL2 +=  Y * L2;
              sum_YL3 +=  Y * L3;
        }
        gaussian::matrix4 <double> matrix = {{
            { siz,    sum_L1, sum_L2, sum_L3 },
            { sum_L1, sum_L2, sum_L3, sum_L4 },
            { sum_L2, sum_L3, sum_L4, sum_L5 },
            { sum_L3, sum_L4, sum_L5, sum_L6 }
        }};
        // solve matrices
        gaussian::vector4 <double> result = { sum_Y, sum_YL1, sum_YL2, sum_YL3 };
        const String faults = gaussian::solve (matrix, result);
        if (!faults.isEmpty ())
            return faults;
        A = result [0];
        B = result [1];
        C = result [2];
        D = result [3];

        // check result and errors
        for (size_t index = 0; index < temperatures.size (); index ++) {
            float temperature;
            if (!calculate (temperature, resistances [index]) || std::abs (temperature - temperatures [index]) > 5.0f)  // Allow 5 degrees of error
                return String ("unreliable result, error = ") + FloatToString (std::abs (temperature - temperatures [index]));
        }

        return String ();
    }
    String calibrate (const Definitions::Collection& collection) {
        assert (collection.temperatures.size () >= 4);

        // build matrices
        gaussian::matrix4 <double> XtX = { 0 };
        gaussian::vector4 <double> XtY = { 0 };
        for (size_t index = 0; index < collection.temperatures.size (); index ++) {
            if (!isTemperatureReasonable (collection.temperatures [index]))
                return String ("invalid temperature at index ") + IntToString (index);
            const double T = 1.0 / (collection.temperatures [index] + 273.15);
            for (size_t sensor = 0; sensor < collection.resistances.size (); sensor ++) {
                if (!isResistanceReasonable (collection.resistances [sensor][index]))
                    return String ("invalid resistance at index ") + IntToString (index);
                const double lnR = std::log (static_cast <double> (collection.resistances [sensor][index]));
                const std::array <double, 4> row = { 1, lnR, lnR * lnR, lnR * lnR * lnR };
                for (int j = 0; j < 4; j ++) {
                    for (int k = 0; k < 4; k ++)
                        XtX [j][k] += row [j] * row [k];
                    XtY [j] += row [j] * T;
                }
            }
        }
        // solve matrices
        gaussian::vector4 <double> result;
        const String faults = gaussian::solve (XtX, XtY, result);
        if (!faults.isEmpty ())
            return faults;
        A = result [0];
        B = result [1];
        C = result [2];
        D = result [3];

        // check result and errors
        for (size_t index = 0; index < collection.temperatures.size (); index ++) {
            for (size_t sensor = 0; sensor < collection.resistances.size (); sensor ++) {
                float temperature;
                if (!calculate (temperature, collection.resistances [sensor] [index]) || std::abs (temperature - collection.temperatures [index]) > 10.0f)  // Allow 10 degrees of error
                    return String ("unreliable result, error = ") + FloatToString (std::abs (temperature - collection.temperatures [index]));
            }
        }
         
        return String ();
    }
    //
    bool calculate (uint16_t& resistance, const float temperature) const {
        if (!isTemperatureReasonable (temperature)) {
            DEBUG_PRINTF ("calculateSteinhartHart: invalid temperature value.\n");
            return false;
        }      
        double lnR = std::log (10000.0);
        for (int i = 0; i < 10; i ++) {
            const double delta = (A + B * lnR + C * lnR * lnR + D * lnR * lnR * lnR - (1.0 / (temperature + 273.15))) / (B + 2 * C * lnR + 3 * D * lnR * lnR);
            lnR -= delta;
            if (std::abs (delta) < 1e-9) break;
        }
        resistance = static_cast <uint16_t> (std::exp (lnR));
        return isResistanceReasonable (resistance);
    };
    bool calculate (float& temperature, const uint16_t resistance) const override {
        if (!isResistanceReasonable (resistance)) {
            DEBUG_PRINTF ("calculateSteinhartHart: invalid resistance value.\n");
            return false;
        }      
        const double lnR = std::log (static_cast <double> (resistance));
        temperature = static_cast <float> (1.0 / (A + B*lnR + C*lnR*lnR + D*lnR*lnR*lnR) - 273.15);
        return isTemperatureReasonable (temperature);
    }
    //
    void serialize (JsonObject& obj) const override {
        obj ["A"] = A, obj ["B"] = B, obj ["C"] = C, obj ["D"] = D;
    }
    void deserialize (JsonObject& obj) override {
        A = obj ["A"], B = obj ["B"], C = obj ["C"], D = obj ["D"];
    }
    String getName () const override { return NAME; }
    String getDetails () const override { return "steinhart (A=" + FloatToString (A, 12) + ", B=" + FloatToString (B, 12) + ", C=" + FloatToString (C, 12) + ", D=" + FloatToString (D, 12) + ")"; }
};

// -----------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------

template <size_t SENSOR_SIZE, float TEMP_START, float TEMP_END, float TEMP_STEP>
class TemperatureCalibrationCalculator {
public:
    using Definitions = TemperatureCalibrationDefinitions <SENSOR_SIZE, TEMP_START, TEMP_END, TEMP_STEP>;
    using StrategyType = TemperatureCalibrationAdjustmentStrategy <SENSOR_SIZE, TEMP_START, TEMP_END, TEMP_STEP>;
    using StrategyDefault = TemperatureCalibrationAdjustmentStrategy_Steinhart <SENSOR_SIZE, TEMP_START, TEMP_END, TEMP_STEP>;
    using StrategyFactory = std::function <std::shared_ptr <StrategyType> ()>;
    using StrategyFactories = std::map <String, StrategyFactory>;

    using __CalibrationStrategySet = std::vector <std::shared_ptr <StrategyType>>;
    using CalibrationStrategies = std::array <__CalibrationStrategySet, SENSOR_SIZE>;

    bool compute (CalibrationStrategies& calibrations, const Definitions::Collection &collection, const StrategyFactories &factories) {
        DEBUG_PRINTF ("TemperatureCalibrationCalculator: calculating from %.2f°C to %.2f°C in %.2f°C steps (%d total) for %d NTC resistances\n", TEMP_START, TEMP_END, TEMP_STEP, Definitions::TEMP_SIZE, SENSOR_SIZE);
        for (size_t sensor = 0; sensor < SENSOR_SIZE; sensor ++) {
            DEBUG_PRINTF ("[%d/%d]: ", sensor, SENSOR_SIZE);
            for (const auto& factory : factories) {
                auto name = factory.first, strategy = factory.second ();
                const String faults = strategy->calibrate (collection.temperatures, collection.resistances [sensor]);
                if (faults.isEmpty ()) {
                    auto stats = strategy->calculateStatsErrors (collection.temperatures, collection.resistances [sensor]);
                    DEBUG_PRINTF ("%s%s (okay, error avg=%.4f,max=%.4f,min=%.4f)", calibrations [sensor].size () > 0 ? ", ": "", name.c_str (), stats.avg (), stats.max (), stats.min ());
                    calibrations [sensor].push_back (strategy);
                } else {
                    DEBUG_PRINTF ("%s%s (fail, %s)", calibrations [sensor].size () > 0 ? ", ": "", name.c_str (), faults.c_str ());
                    //return false;
                }
            }
            DEBUG_PRINTF ("\n");
        }
        return true;
    }

    bool computeDefault (StrategyDefault& strategyDefault, const Definitions::Collection& collection) {
        DEBUG_PRINTF ("TemperatureCalibrationCalculator::computeDefault: using %s\n", strategyDefault.getName ().c_str ());
        const String faults = strategyDefault.calibrate (collection);
        if (!faults.isEmpty ()) {
            DEBUG_PRINTF ("TemperatureCalibrationCalculator::computeDefault: fail, %s\n", faults.c_str ());
            return false;
        }
        DEBUG_PRINTF ("TemperatureCalibrationCalculator::computeDefault: %s\n", strategyDefault.getDetails ().c_str ());
        DEBUG_PRINTF ("(resistances [%.2f°C -> %.2f°C at %.2f°C]: ", TEMP_START, TEMP_END, TEMP_STEP);
        for (float temperature = TEMP_START; temperature <= TEMP_END; temperature += TEMP_STEP) {
            uint16_t resistance = 0; strategyDefault.calculate (resistance, temperature);
            DEBUG_PRINTF ("%s%u", (temperature == TEMP_START) ? "" : ",", resistance);
        }
        DEBUG_PRINTF (")\n");
        return true;
    }    
};

// -----------------------------------------------------------------------------------------------
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

        for (size_t sensor = 0; sensor < SENSOR_SIZE; sensor ++) {
            JsonObject sensorObj = doc ["sensor" + IntToString (sensor)].to <JsonObject> ();
            for (const auto& strategy : calibrationStrategies [sensor]) {
                JsonObject strategyDetails = sensorObj [String (strategy->getName ())].to <JsonObject> ();
                strategy->serialize (strategyDetails);
            }
        }
        JsonObject strategyDefault = doc ["default"].to <JsonObject> ();
        defaultStrategy.serialize (strategyDefault);
        
        SPIFFSFile file (filename); size_t size;
        if (!file.begin () || !((size = file.write (doc)) > 0)) {
            DEBUG_PRINTF ("TemperatureCalibrationStorage::serialize: could not write to '%s'\n", filename.c_str ());
            return false;
        }
        DEBUG_PRINTF ("TemperatureCalibrationStorage::serialize: wrote %d bytes to '%s'\n", size, filename.c_str ());
        return true;
    }

    static bool deserialize (const String& filename, StrategyDefault& defaultStrategy, CalibrationStrategies& calibrationStrategies, const StrategyFactories& strategyFactories) {
        JsonDocument doc;

        SPIFFSFile file (filename); size_t size;
        if (!file.begin () || !((size = file.read (doc)) > 0)) {
            DEBUG_PRINTF ("TemperatureCalibrationStorage::deserialize: could not read from '%s'\n", filename.c_str ());          
            return false;
        }
        DEBUG_PRINTF ("TemperatureCalibrationStorage::deserialize: read %d bytes from '%s'\n", size, filename.c_str ());

        int count = 0;
        for (size_t sensor = 0; sensor < SENSOR_SIZE; sensor ++) {
            JsonObject sensorObj = doc ["sensor" + IntToString (sensor)].as <JsonObject> ();
            if (sensorObj) {
                for (JsonPair kv : sensorObj) {
                    auto factoryIt = strategyFactories.find (String (kv.key ().c_str ()));
                    if (factoryIt != strategyFactories.end ()) {
                        auto strategy = factoryIt->second ();
                        JsonObject details = kv.value ().as <JsonObject> ();
                        strategy->deserialize (details);
                        DEBUG_PRINTF ("TemperatureCalibrationStorage::deserialize: sensor %d, installed %s\n", sensor, strategy->getDetails ().c_str ());
                        calibrationStrategies [sensor].push_back (strategy);
                        count ++;
                    }
                }
            } else
                DEBUG_PRINTF ("TemperatureCalibrationStorage::deserialize: did not find sensor %d\n", sensor);
        }
        JsonObject details = doc ["default"].as <JsonObject> ();
        if (details)
            defaultStrategy.deserialize (details), count ++;
        return count > 0;
    }
};

// -----------------------------------------------------------------------------------------------
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
    TemperatureCalibrationRuntime (const StrategyDefault& defaultStrategy, const CalibrationStrategies& calibrationStrategies): defaultStrategy (defaultStrategy), calibrationStrategies (calibrationStrategies) {      
        DEBUG_PRINTF ("TemperatureCalibrationRuntime::init: default [%s", defaultStrategy.getDetails ().c_str ());
        for (int index = 0; index < calibrationStrategies.size (); index ++) {
            DEBUG_PRINTF ("], %d [", index); int count = 0;
            for (const auto& strategy : calibrationStrategies [index])
                DEBUG_PRINTF ("%s%s", count ++ == 0 ? "" : ",", strategy->getName ().c_str ());
        }
        DEBUG_PRINTF ("]\n");
    }

    float calculateTemperature (const size_t index, const uint16_t resistance) const {
        float temperature;
        for (const auto& strategy : calibrationStrategies [index])
            if (strategy->calculate (temperature, resistance))
                return temperature;
        if (defaultStrategy.calculate (temperature, resistance))
            return temperature;
        DEBUG_PRINTF ("TemperatureCalibrationRuntime::calculateTemperature: failed, sensor %d, resistance %u", index, resistance);
        return NAN;
    }
};

// -----------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------

//#define CALIBRATE_FROM_STATIC_DATA
#ifdef CALIBRATE_FROM_STATIC_DATA
static const char* temperatureCalibrationData_STATIC [] = {
"0,5.01,3145,3078,3083,3081,3160,3084,3155,3168,3148,3094,2827,2925,2967,2924,3073,2991",
"1,5.51,3133,3081,3079,3077,3156,3096,3160,3150,3126,3083,2864,2923,2960,2826,3064,2991",
"2,6.04,3094,3055,3075,3069,3112,3123,3194,3138,3058,3031,2851,2911,2916,2824,3059,2987",
"3,6.50,3122,3062,3064,3066,3134,3098,3170,3134,3092,3070,2869,2913,2884,2817,3058,2975",
"4,7.00,3077,3073,3068,3071,3130,3077,3104,3113,3099,2981,2857,2888,2896,2786,2994,2941",
"5,7.51,3069,3046,3079,3059,3104,3084,3117,3068,3086,3006,2854,2893,2887,2794,2958,2945",
"6,8.02,3069,3009,3050,3055,3071,3099,3145,3011,3107,2998,2807,2903,2828,2830,2949,2952",
"7,8.50,3067,3019,3084,3031,3092,3068,3113,3012,3063,3024,2877,2892,2828,2807,2957,2942",
"8,9.00,3056,3014,3002,3001,3073,2985,2988,3031,3034,3023,2839,2819,2837,2746,2987,2898",
"9,9.50,2997,2982,3050,2968,3038,2976,3054,3028,3002,2976,2824,2813,2878,2860,2937,2914",
"10,10.03,2977,2971,2989,2967,3039,2959,3026,2991,3014,2981,2820,2814,2865,2848,2895,2868",
"11,10.51,2966,2951,2938,2954,3010,2970,3004,2964,2981,2945,2796,2807,2833,2830,2887,2853",
"12,11.00,2905,2916,2895,2903,2962,2921,2954,2918,2941,2897,2771,2777,2785,2765,2844,2807",
"13,11.51,2886,2877,2883,2873,2940,2898,2932,2917,2906,2886,2730,2755,2743,2792,2825,2782",
"14,12.00,2875,2904,2852,2874,2932,2901,2945,2894,2951,2859,2756,2701,2851,2842,2802,2756",
"15,12.51,2799,2837,2846,2843,2886,2845,2874,2856,2854,2801,2688,2703,2749,2734,2735,2730",
"16,13.00,2801,2823,2827,2825,2869,2833,2839,2823,2835,2813,2670,2679,2736,2741,2733,2740",
"17,13.50,2750,2742,2984,2727,2801,2895,2748,2728,2799,2727,2643,2675,2585,2656,2778,2712",
"18,14.01,2773,2735,2722,2734,2778,2746,2813,2805,2793,2745,2581,2631,2678,2642,2681,2651",
"19,14.50,2759,2701,2660,2727,2745,2694,2765,2739,2737,2692,2581,2622,2627,2695,2669,2616",
"20,15.00,2707,2687,2685,2688,2731,2705,2734,2722,2730,2679,2546,2609,2633,2631,2637,2615",
"21,15.50,2713,2668,2678,2675,2721,2689,2763,2723,2720,2682,2547,2589,2626,2633,2637,2600",
"22,16.00,2631,2699,2600,2706,2712,2623,2732,2652,2678,2744,2530,2539,2516,2571,2653,2569",
"23,16.50,2641,2616,2621,2619,2653,2630,2693,2660,2661,2619,2510,2532,2580,2579,2570,2547",
"24,17.00,2673,2627,2549,2583,2603,2603,2640,2633,2621,2565,2428,2534,2480,2597,2584,2504",
"25,17.50,2563,2557,2568,2604,2659,2578,2640,2640,2616,2604,2491,2454,2570,2505,2521,2502",
"26,18.00,2565,2552,2555,2570,2597,2577,2619,2573,2568,2565,2426,2448,2472,2490,2514,2498",
"27,18.50,2560,2536,2532,2528,2570,2519,2599,2566,2563,2531,2412,2443,2461,2505,2499,2460",
"28,19.00,2533,2491,2501,2510,2543,2505,2528,2537,2528,2504,2387,2433,2441,2476,2461,2438",
"29,19.50,2507,2473,2475,2472,2515,2479,2546,2515,2508,2486,2378,2402,2412,2474,2445,2416",
"30,20.00,2485,2457,2465,2469,2503,2464,2520,2491,2476,2471,2362,2395,2391,2449,2427,2410",
"31,20.50,2449,2427,2434,2429,2449,2432,2468,2459,2458,2440,2349,2368,2357,2410,2394,2374",
"32,21.00,2441,2403,2414,2424,2447,2410,2449,2431,2433,2435,2308,2342,2362,2372,2376,2362",
"33,21.50,2402,2377,2382,2385,2414,2378,2435,2410,2410,2385,2298,2319,2342,2383,2360,2336",
"34,22.00,2378,2355,2364,2366,2394,2359,2419,2393,2388,2365,2272,2320,2309,2363,2343,2325",
"35,22.53,2360,2333,2353,2344,2376,2355,2384,2356,2360,2346,2250,2281,2295,2341,2318,2297",
"36,23.00,2334,2311,2346,2322,2351,2310,2360,2352,2339,2311,2233,2251,2279,2294,2297,2258",
"37,23.50,2301,2281,2273,2293,2331,2280,2322,2289,2289,2317,2198,2221,2236,2273,2251,2259",
"38,24.01,2317,2269,2251,2299,2268,2283,2268,2288,2277,2226,2268,2196,2261,2248,2254,2195",
"39,24.50,2245,2236,2261,2251,2280,2237,2284,2278,2259,2242,2168,2197,2218,2252,2229,2225",
"40,25.01,2219,2209,2245,2227,2251,2208,2259,2252,2260,2191,2133,2188,2221,2221,2214,2209",
"41,25.50,2176,2190,2206,2197,2221,2180,2231,2203,2203,2181,2124,2156,2173,2209,2187,2173",
"42,26.01,2203,2170,2190,2185,2194,2174,2211,2204,2190,2188,2108,2156,2158,2120,2172,2156",
"43,26.54,2183,2165,2169,2173,2186,2170,2193,2193,2189,2158,2108,2149,2194,2155,2159,2142",
"44,27.00,2149,2126,2156,2134,2169,2140,2154,2136,2152,2137,2084,2109,2120,2112,2128,2121",
"45,27.50,2120,2094,2114,2109,2135,2103,2139,2127,2125,2109,2052,2084,2102,2136,2103,2089",
"46,28.01,2085,2078,2096,2088,2103,2086,2122,2095,2101,2072,2033,2073,2100,2125,2067,2065",
"47,28.50,2061,2033,2056,2050,2086,2046,2090,2073,2074,2044,2000,2034,2042,2058,2038,2041",
"48,29.01,2038,2021,2061,2027,2068,2048,2043,2049,2049,2026,2022,2074,2086,2065,2008,2037",
"49,29.50,2026,1997,2027,2013,2019,1995,2001,2014,2000,2016,1963,1991,1998,2022,2017,2011",
"50,30.00,1993,1969,1973,1988,1994,1994,2020,1999,1980,1970,1943,2005,1995,2018,1971,1971",
"51,30.50,1948,1908,1986,1948,2002,1970,1947,1947,1934,1956,1921,1910,1927,2020,1921,1956",
"52,31.00,1951,1931,1944,1930,1953,1935,1946,1939,1946,1943,1900,1945,1961,1963,1937,1937",
"53,31.50,1923,1913,1929,1918,1953,1909,1961,1915,1918,1922,1893,1918,1936,1952,1915,1927",
"54,32.01,1888,1870,1890,1885,1904,1881,1903,1888,1883,1876,1854,1890,1892,1916,1894,1897",
"55,32.51,1858,1847,1866,1863,1893,1864,1890,1856,1881,1858,1848,1887,1896,1905,1865,1886",
"56,33.01,1774,1840,1835,1857,1899,1898,1868,1851,1842,1838,1886,1862,1795,1946,1851,1881",
"57,33.50,1820,1804,1833,1817,1837,1818,1834,1834,1840,1806,1806,1844,1836,1865,1845,1841",
"58,34.00,1787,1776,1803,1787,1830,1788,1836,1811,1820,1790,1786,1831,1858,1846,1811,1820",
"59,34.51,1769,1750,1773,1761,1788,1770,1802,1773,1797,1765,1757,1802,1821,1843,1795,1805",
"60,35.01,1750,1751,1781,1749,1772,1750,1757,1740,1747,1740,1745,1789,1770,1802,1775,1802",
"61,35.50,1722,1721,1760,1729,1765,1748,1781,1759,1779,1725,1744,1784,1789,1821,1779,1791",
"62,36.00,1701,1705,1731,1714,1741,1713,1701,1726,1731,1707,1732,1760,1762,1799,1754,1771",
"63,36.51,1703,1677,1737,1687,1690,1699,1691,1690,1718,1667,1689,1709,1753,1745,1701,1774",
"64,37.00,1667,1654,1675,1670,1691,1659,1694,1680,1690,1658,1689,1714,1698,1741,1710,1721",
"65,37.50,1543,1669,1617,1648,1722,1659,1638,1624,1654,1591,1601,1673,1734,1803,1743,1682",
"66,38.00,1621,1604,1618,1612,1632,1591,1638,1628,1634,1611,1646,1673,1662,1708,1667,1678",
"67,38.50,1604,1591,1607,1601,1630,1597,1627,1614,1623,1608,1645,1660,1650,1691,1662,1667",
"68,39.01,1571,1578,1581,1575,1609,1570,1606,1579,1610,1550,1612,1646,1641,1658,1630,1654",
"69,39.50,1551,1545,1574,1551,1577,1553,1576,1566,1576,1545,1606,1623,1613,1649,1613,1626",
"70,40.00,1537,1531,1544,1532,1550,1527,1547,1549,1545,1519,1576,1612,1590,1624,1601,1607",
"71,40.52,1511,1511,1516,1520,1532,1511,1517,1527,1539,1504,1541,1598,1572,1617,1580,1585",
"72,41.00,1506,1474,1485,1493,1503,1477,1502,1498,1518,1466,1533,1621,1596,1582,1569,1579",
"73,41.50,1476,1459,1469,1465,1490,1437,1498,1477,1497,1461,1515,1565,1550,1577,1542,1552",
"74,42.02,1453,1456,1440,1495,1493,1507,1463,1461,1477,1417,1524,1492,1463,1584,1495,1502",
"75,42.51,1431,1417,1437,1431,1475,1425,1470,1433,1440,1413,1483,1505,1496,1530,1488,1509",
"76,43.00,1419,1403,1419,1403,1430,1422,1432,1419,1434,1399,1468,1503,1501,1522,1489,1500",
"77,43.50,1399,1374,1402,1391,1418,1400,1434,1409,1418,1390,1461,1499,1493,1511,1479,1492",
"78,44.01,1394,1371,1392,1386,1401,1392,1406,1399,1395,1383,1437,1486,1471,1489,1467,1472",
"79,44.50,1364,1363,1370,1374,1385,1362,1383,1368,1388,1367,1419,1462,1460,1467,1443,1465",
"80,45.00,1348,1335,1353,1342,1370,1335,1380,1355,1364,1346,1416,1451,1438,1459,1435,1443",
"81,45.50,1326,1307,1326,1319,1346,1309,1337,1336,1337,1318,1386,1419,1410,1400,1420,1423",
"82,46.00,1306,1294,1307,1304,1323,1302,1334,1308,1321,1298,1374,1405,1400,1421,1393,1400",
"83,46.50,1287,1275,1301,1289,1307,1266,1289,1289,1288,1283,1364,1381,1369,1361,1385,1389",
"84,47.00,1290,1233,1246,1264,1293,1250,1316,1251,1326,1275,1383,1390,1400,1293,1384,1427",
"85,47.51,1248,1238,1281,1277,1272,1270,1269,1239,1284,1255,1327,1350,1360,1344,1349,1343",
"86,48.01,1248,1229,1245,1244,1250,1240,1262,1257,1269,1245,1327,1367,1335,1306,1356,1348",
"87,48.50,1228,1205,1233,1222,1242,1219,1226,1221,1244,1222,1308,1343,1332,1339,1317,1334",
"88,49.00,1209,1194,1212,1206,1220,1209,1228,1208,1211,1199,1291,1336,1317,1271,1310,1321",
"89,49.50,1180,1176,1188,1175,1191,1165,1203,1176,1196,1175,1268,1303,1292,1278,1284,1294",
"90,50.00,1186,1129,1151,1178,1149,1204,1161,1168,1154,1148,1274,1301,1225,1264,1332,1309",
"91,50.50,1161,1143,1155,1149,1176,1163,1185,1164,1170,1148,1257,1291,1277,1199,1278,1274",
"92,51.00,1126,1114,1138,1137,1147,1113,1154,1127,1147,1128,1231,1253,1256,1196,1236,1261",
"93,51.50,1148,1084,1115,1126,1146,1138,1165,1148,1136,1111,1240,1286,1244,1241,1258,1260",
"94,52.00,1106,1109,1067,1094,1129,1091,1134,1107,1114,1094,1208,1250,1225,1207,1206,1218",
"95,52.50,1104,1067,1099,1082,1068,1060,1111,1110,1147,1078,1174,1219,1236,1256,1226,1262",
"96,53.00,1083,1065,1077,1076,1091,1096,1101,1088,1100,1070,1181,1227,1204,1150,1203,1197",
"97,53.51,1054,1045,1062,1052,1074,1048,1068,1052,1060,1053,1163,1182,1206,1192,1174,1196",
"98,54.00,1036,1054,1057,1053,1073,1044,1077,1063,1082,1053,1148,1165,1185,1188,1148,1163",
"99,54.50,1034,1019,1036,1023,1039,1023,1054,1039,1055,1018,1157,1178,1159,1120,1149,1165",
"100,55.00,1015,1005,1017,1018,1035,1003,1026,1015,1026,1017,1132,1157,1149,1113,1149,1162",
"101,55.50,1001,997,1000,1002,1017,991,1008,1003,1010,1001,1119,1141,1129,1132,1120,1139",
"102,56.01,1010,979,990,994,1007,997,1020,1005,1013,986,1115,1141,1129,1055,1136,1130",
"103,56.52,964,960,984,969,993,972,973,961,975,965,1094,1115,1108,1129,1089,1119",
"104,57.01,962,944,983,972,999,977,994,983,958,976,1055,1116,1089,1104,1130,1117",
"105,57.50,949,925,943,947,957,943,956,944,953,939,1070,1082,1081,1096,1091,1086",
"106,58.00,942,924,927,929,939,934,953,939,952,929,1062,1083,1075,1066,1073,1082",
"107,58.50,914,919,924,942,934,924,916,921,934,918,1051,1058,1075,1049,1066,1104",
"108,59.00,911,872,896,881,936,884,901,881,891,906,1018,1049,1018,1053,1022,1062",
"109,59.50,884,880,876,887,912,867,891,882,870,880,992,1035,1030,1010,1025,1045",
"110,60.00,880,841,871,864,900,876,880,870,894,893,981,1014,1047,957,1015,1036"
};

template <size_t SENSOR_SIZE, float TEMP_START, float TEMP_END, float TEMP_STEP>
class TemperatureCalibrationStaticDataLoader {
    using Definitions = TemperatureCalibrationDefinitions <SENSOR_SIZE, TEMP_START, TEMP_END, TEMP_STEP>;
    using Collector = TemperatureCalibrationCollector <SENSOR_SIZE, TEMP_START, TEMP_END, TEMP_STEP>;

    bool parse (const char* line, float& temperature, std::array <uint16_t, SENSOR_SIZE>& resistances) {
        const char* token = line, *next_token;
        if (!(token = strchr (token, ',')))
            return false;
        token ++;
        temperature = strtof (token, const_cast <char**> (&next_token));
        if (next_token == token)
            return false;
        token = next_token + 1;
        for (int i = 0; i < SENSOR_SIZE; i++) {
            resistances [i] = static_cast <uint16_t> (strtol (token, const_cast <char**> (&next_token), 10));
            if (next_token == token)
                return false;
            token = next_token + 1;
        }
        return true;
    }

public:
    bool load (Collector::Collection& collection) {
        for (int index = 0; index < collection.temperatures.size (); index ++) {
            std::array <uint16_t, SENSOR_SIZE> resistances; // need to be transposed
            if (!parse (temperatureCalibrationData_STATIC [index], collection.temperatures [index], resistances)) {
                DEBUG_PRINTF ("TemperatureCalibrationManager::calibateTemperatures - parsing failed\n");
                return false;
            }
            for (int sensor = 0; sensor < SENSOR_SIZE; sensor ++)
                collection.resistances [sensor] [index] = resistances [sensor];
        }
        return true;
    }
};
#endif

// -----------------------------------------------------------------------------------------------
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
        StrategyDefault::Config strategyDefault;
    };

private:
    const Config &config;
    std::shared_ptr <Runtime> runtime;

    StrategyFactories createStrategyFactoriesForCalibration () const { // store the lookup tables, but don't use them
        return StrategyFactories {
            { StrategyLookup::NAME, [] { return std::make_shared <StrategyLookup> (); } },
            { StrategySteinhart::NAME, [] { return std::make_shared <StrategySteinhart> (); } }
        };
    }
    StrategyFactories createStrategyFactoriesForOperation () const {
        return StrategyFactories {
            { StrategySteinhart::NAME, [] { return std::make_shared <StrategySteinhart> (); } }
        };
    }

public:
    TemperatureCalibrationManager (const Config& cfg) : config (cfg) {}

    void begin () override {
        std::shared_ptr <typename Calculator::CalibrationStrategies> calibrationStrategies = std::make_shared <typename Calculator::CalibrationStrategies> ();
        StrategyDefault defaultStrategy (config.strategyDefault);
        if (!Storage::deserialize (config.filename, defaultStrategy, *calibrationStrategies, createStrategyFactoriesForOperation ()))
            DEBUG_PRINTF ("TemperatureCalibrationManager:: no stored calibrations (filename = %s), will rely upon default\n", config.filename.c_str ());
        runtime = std::make_shared <Runtime> (defaultStrategy, *calibrationStrategies);
    }

    float calculateTemperature (const size_t index, const float resistance) const {
        if (!runtime) return -273.15f;
        return runtime->calculateTemperature (index, resistance);
    }

    bool calibrateTemperatures (const Collector::TemperatureReadFunc readTemperature, const Collector::ResistanceReadFunc readResistance) {
        std::shared_ptr <typename Collector::Collection> calibrationData = std::make_shared <typename Collector::Collection> ();
#ifdef CALIBRATE_FROM_STATIC_DATA
        if (!TemperatureCalibrationStaticDataLoader <SENSOR_SIZE, TEMP_START, TEMP_END, TEMP_STEP> ().load (*calibrationData)) {
            DEBUG_PRINTF ("TemperatureCalibrationManager::calibateTemperatures - static load failed\n");
            return false;
        }
#else
        if (!Collector ().collect (*calibrationData, readTemperature, readResistance)) {
            DEBUG_PRINTF ("TemperatureCalibrationManager::calibateTemperatures - collector failed\n");
            return false;
        }
        for (int index = 0; index < calibrationData->temperatures.size (); index ++) {
            DEBUG_PRINTF ("= %d,%.2f", index, calibrationData->temperatures [index]);
            for (size_t sensor = 0; sensor < calibrationData->resistances.size (); sensor ++)
                DEBUG_PRINTF (",%u", calibrationData->resistances [sensor] [index]);
            DEBUG_PRINTF ("\n");
        }
#endif
        Calculator calculator;
        std::shared_ptr <typename Calculator::CalibrationStrategies> calibrationStrategies = std::make_shared <typename Calculator::CalibrationStrategies> ();
        StrategyDefault defaultStrategy;
        if (!calculator.compute (*calibrationStrategies, *calibrationData, createStrategyFactoriesForCalibration ()) || !calculator.computeDefault (defaultStrategy, *calibrationData)) {
            DEBUG_PRINTF ("TemperatureCalibrationManager::calibateTemperatures - calculator failed\n");
            return false;
        }
        Storage::serialize (config.filename, defaultStrategy, *calibrationStrategies);
        runtime = std::make_shared <Runtime> (defaultStrategy, *calibrationStrategies);
        return true;
    }
};

// -----------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------
