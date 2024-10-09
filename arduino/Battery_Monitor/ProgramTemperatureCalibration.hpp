
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
        static constexpr int DELAY = 100, COUNT = 5*1000/DELAY, AVG_MASTER = 12, AVG_SAMPLE = 6;
        MovingAverageWithValue <float, AVG_MASTER> temperature;

        static constexpr float temperatureBegin = (TEMP_START - TEMP_STEP);
        if ((temperature = readTemperature ()) > temperatureBegin) {
            DEBUG_PRINTF ("TemperatureCalibrationCollector: waiting for DS18B20 temperature to reduce to %.2f°C ... \n", temperatureBegin);
            int where = 0;
            while (temperature > temperatureBegin) {
                if (-- where < 0)
                    DEBUG_PRINTF ("(at %.2f°C)\n", static_cast <float> (temperature)), where = COUNT;
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
                if (-- where < 0)
                    DEBUG_PRINTF ("(at %.2f°C)\n", static_cast <float> (temperature)), where = COUNT;
                delay (DELAY);
                temperature = readTemperature ();
            }

            DEBUG_PRINTF ("... reached %.2f°C, collecting %d NTC resistances ...\n", static_cast <float> (temperature), SENSOR_SIZE);
            collection.temperatures [step] = temperature;
            std::array <uint32_t, Definitions::TEMP_SIZE> resistances;
            resistances.fill (static_cast <uint32_t> (0));
            for (int count = 0; count < AVG_SAMPLE; count ++)
              for (size_t sensor = 0; sensor < SENSOR_SIZE; sensor ++)
                  resistances [sensor] += static_cast <uint32_t> (readResistance (sensor));
            for (size_t sensor = 0; sensor < SENSOR_SIZE; sensor ++)                  
                collection.resistances [sensor] [step] = static_cast <uint16_t> (resistances [sensor] / static_cast <uint32_t> (AVG_SAMPLE));
            DEBUG_PRINTF ("done\n");
            
            //
            DEBUG_PRINTF ("= %d,%.2f", step, collection.temperatures [step]);
            for (size_t sensor = 0; sensor < SENSOR_SIZE; sensor ++)
                DEBUG_PRINTF (",%u", collection.resistances [sensor] [step]);
            DEBUG_PRINTF ("\n");
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
    virtual String calibrate (const Definitions::Temperatures& temperatures, const Definitions::Resistances& resistances) = 0;
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
        for (size_t i = 0; i < Definitions::TEMP_SIZE; i ++)
            t.add (temperatures [i]), r.add (resistances [i]);
    }
    void deserialize (JsonObject& obj) override {
        JsonArray t = obj ["T"], r = obj ["R"];
        for (size_t i = 0; i < Definitions::TEMP_SIZE; i ++)
            temperatures [i] = t [i], resistances [i] = r [i];
    }
    String getName () const override {
        return "lookup"; 
    }
    String getDetails () const override {
        return "lookup (N=" + IntToString (temperatures.size ()) + ")";
    }
};

template <size_t SENSOR_SIZE, float TEMP_START, float TEMP_END, float TEMP_STEP>
class TemperatureCalibrationAdjustmentStrategy_Steinhart : public TemperatureCalibrationAdjustmentStrategy <SENSOR_SIZE, TEMP_START, TEMP_END, TEMP_STEP> {
public:
    using Definitions = TemperatureCalibrationDefinitions <SENSOR_SIZE, TEMP_START, TEMP_END, TEMP_STEP>;
    typedef struct {
        double A, B, C, D;
    } Config;

private:
    double A, B, C, D;

    inline bool isResistanceReasonable (const uint16_t resistance) const { return resistance > 0 && resistance < 100*1000; }
    inline bool isTemperatureReasonable (const float temperature) const { return temperature > -273.15f && temperature < 200.0f; }

    bool gaussian_solve_size4 (std::array <std::array <double, 4>, 4>& matrix, std::array <double, 4>& vector) {

        const double det = matrix [0][0] * (
            matrix [1][1] * matrix [2][2] * matrix [3][3] + 
            matrix [1][2] * matrix [2][3] * matrix [3][1] + 
            matrix [1][3] * matrix [2][1] * matrix [3][2] - 
            matrix [1][3] * matrix [2][2] * matrix [3][1] - 
            matrix [1][2] * matrix [2][1] * matrix [3][3] - 
            matrix [1][1] * matrix [2][3] * matrix [3][2]
        );
        if (std::abs (det) < 1e-10)
            return false;

        for (size_t i = 0; i < 4; i ++)
            for (size_t j = i + 1; j < 4; j ++) {
                const double factor = matrix [j][i] / matrix [i][i];
                for (size_t k = i; k < 4; k ++)
                    matrix [j][k] -= factor * matrix [i][k];
                vector [j] -= factor * vector [i];
            }

        for (int i = 4 - 1; i >= 0; i --) {
            for (int j = i + 1; j < 4; j ++)
                vector [i] -= matrix [i][j] * vector [j];
            vector [i] /= matrix [i][i];
        }

        return true;
    }

public:
    TemperatureCalibrationAdjustmentStrategy_Steinhart (const double a = 0.0, const double b = 0.0, const double c = 0.0, const double d = 0.0): A (a), B (b), C (c), D (d) {}
    TemperatureCalibrationAdjustmentStrategy_Steinhart (const Config& config): A (config.A), B (config.B), C (config.C), D (config.D) {}

    String calibrate (const Definitions::Temperatures& t, const Definitions::Resistances& r) override {
      
        if (t.size () < 4 || t.size () != r.size ())
            return String ("at least 4 pairs required");

        const size_t cnt = t.size ();
        std::array <double, Definitions::TEMP_SIZE> Y, L, L2, L3;
        for (size_t i = 0; i < cnt; i ++) {
            if (!isResistanceReasonable (r [i]) || !isTemperatureReasonable (t [i]))
                return String ("invalid data at index ") + IntToString (i);
            Y  [i] = 1.0 / (t [i] + 273.15);
            L  [i] = std::log (static_cast <double> (1.0 * r [i]));
            L2 [i] = L [i] * L [i];
            L3 [i] = L2 [i] * L [i];
        }

        double sum_Y = 0, sum_L = 0, sum_L2 = 0, sum_L3 = 0, sum_L4 = 0, sum_L5 = 0, sum_L6 = 0;
        double sum_YL = 0, sum_YL2 = 0, sum_YL3 = 0;
        for (size_t i = 0; i < cnt; i ++) {
            sum_Y   += Y  [i];
            sum_L   += L  [i];
            sum_L2  += L2 [i];
            sum_L3  += L3 [i];
            sum_L4  += L2 [i] * L2 [i];
            sum_L5  += L2 [i] * L3 [i];
            sum_L6  += L3 [i] * L3 [i];
            sum_YL  += Y  [i] * L  [i];
            sum_YL2 += Y  [i] * L2 [i];
            sum_YL3 += Y  [i] * L3 [i];
        }
        std::array <std::array <double, 4>, 4> matrix = {{
            { cnt*1.0,sum_L,  sum_L2, sum_L3},
            { sum_L,  sum_L2, sum_L3, sum_L4},
            { sum_L2, sum_L3, sum_L4, sum_L5},
            { sum_L3, sum_L4, sum_L5, sum_L6}
        }};
        std::array <double, 4> vector = { sum_Y, sum_YL, sum_YL2, sum_YL3 };

        if (!gaussian_solve_size4 (matrix, vector))
            return String ("ill-conditioned matrix");
        
        A = vector [0];
        B = vector [1];
        C = vector [2];
        D = vector [3];

        for (size_t i = 0; i < cnt; i ++) {
            float temperature;
            if (!calculate (temperature, r [i]) || !isTemperatureReasonable (temperature) || std::abs (temperature - t [i]) > 5.0f)  // Allow 5 degrees of error
                return String ("unreliable result");
        }

        return String ();
    }
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
    String getName () const override {
        return "steinhart";
    }
    String getDetails () const override {
        return "steinhart (A=" + FloatToString (A) + ", B=" + FloatToString (B) + ", C=" + FloatToString (C) + ", D=" + FloatToString (D) + ")";
    }
};

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
                String faults = strategy->calibrate (collection.temperatures, collection.resistances [sensor]);
                if (faults.isEmpty ()) {
                    auto stats = strategy->calculateStatsErrors (collection.temperatures, collection.resistances [sensor]);
                    DEBUG_PRINTF ("%s%s (okay, error_avg=%.2f,max=%.2f,min=%.2f)", calibrations [sensor].size () > 0 ? ", ": "", name.c_str (), stats.avg (), stats.max (), stats.min ());
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
        typename Definitions::Resistances resistances;
        for (size_t index = 0; index < Definitions::TEMP_SIZE; index ++) {
            uint32_t average = 0;
            for (size_t sensor = 0; sensor < SENSOR_SIZE; sensor ++)
                average += static_cast <uint32_t> (collection.resistances [sensor] [index]);
            resistances [index] = static_cast <uint16_t> (average / SENSOR_SIZE);
        }
        return strategyDefault.calibrate (collection.temperatures, resistances);
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
            for (const auto& strategy : calibrationStrategies [i]) {
                JsonObject strategyDetails = sensor [String (strategy->getName ())].to <JsonObject> ();
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
        size_t size = serializeJson (doc, file);
        file.close ();
        Serial.println ();
        Serial.printf ("SPIFFS: Wrote %d bytes to file (%s)\n", size, filename.c_str ());
      
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
                for (JsonPair kv : sensor) {
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

//#define CALIBRATE_FROM_STATIC_DATA
#ifdef CALIBRATE_FROM_STATIC_DATA
static const char* temperatureCalibrationData_STATIC [] = {
"0,5.05,3176,3041,3244,3265,3160,3107,3175,3123,3032,3058,3285,3143,2936,3241,3052,3138",
"1,5.50,3122,3019,3208,3255,3162,3115,3162,3089,3031,3131,3204,3068,3246,3165,3048,3143",
"2,6.00,3091,3116,3127,3264,3177,3148,3132,3105,2997,3117,3149,3138,3050,3149,3059,3147",
"3,6.52,3108,3093,3126,3272,3093,3107,3121,3019,2913,3146,3182,3150,3219,3131,3001,3137",
"4,7.03,3196,3095,3128,3243,3214,3130,3076,3128,3003,3165,3194,3154,3125,3134,3093,3085",
"5,7.51,3180,3036,3139,3240,3208,3157,3103,3089,2967,3085,3196,3160,3004,3130,3104,3116",
"6,8.02,3138,3050,3103,3175,3117,3061,3048,3029,2943,3085,3172,3127,3198,3109,3084,3113",
"7,8.55,3041,3021,3058,3107,3017,3073,2988,2955,2852,3033,3111,3069,3158,3058,3009,3032",
"8,9.00,3169,3048,2995,3056,3041,2992,2988,2951,2966,3104,3070,3086,2770,3036,3060,3062",
"9,9.50,2997,2963,2971,3065,3021,3048,2972,2962,2891,3014,3016,2992,2763,2993,3021,3050",
"10,10.02,2982,2970,2977,3014,2997,2911,2950,2948,2798,2999,3013,2971,2703,2986,3015,3007",
"11,10.53,2963,2920,2972,2969,2971,2979,2939,2909,2813,2934,2988,2939,2721,2976,2990,3001",
"12,11.00,2952,2905,2931,2929,2934,2955,2893,2909,2846,2928,2950,2908,2774,2930,2965,2953",
"13,11.50,2926,2882,2899,2922,2912,2927,2870,2843,2778,2891,2923,2881,3001,2905,2945,2924",
"14,12.00,2827,2811,2891,2885,2899,2870,2852,2824,2770,2857,2910,2857,3031,2885,2874,2907",
"15,12.50,2889,2842,2862,2882,2871,2833,2832,2808,2707,2845,2897,2857,2945,2874,2857,2880",
"16,13.02,2854,2830,2840,2843,2821,2840,2789,2810,2722,2801,2855,2810,2931,2827,2874,2850",
"17,13.50,2830,2807,2815,2825,2811,2791,2787,2796,2711,2793,2812,2804,2587,2811,2844,2843",
"18,14.01,2812,2786,2806,2812,2807,2803,2771,2757,2681,2762,2824,2805,2933,2798,2838,2825",
"19,14.50,2744,2741,2762,2770,2730,2725,2718,2713,2618,2762,2765,2749,2454,2742,2805,2764",
"20,15.00,2742,2716,2720,2729,2720,2697,2685,2714,2625,2710,2736,2695,2482,2729,2762,2751",
"21,15.50,2708,2672,2732,2719,2676,2725,2648,2702,2622,2692,2717,2708,2633,2700,2723,2776",
"22,16.02,2630,2764,2739,2726,2597,2719,2610,2644,2638,2662,2755,2600,2770,2673,2661,2620",
"23,16.50,2680,2644,2682,2698,2646,2632,2649,2604,2566,2551,2692,2643,2768,2680,2666,2684",
"24,17.01,2606,2620,2654,2680,2633,2610,2631,2562,2558,2584,2680,2636,2742,2641,2582,2672",
"25,17.50,2604,2583,2622,2630,2515,2565,2587,2585,2516,2605,2640,2594,2494,2622,2619,2638",
"26,18.00,2620,2566,2604,2594,2571,2575,2558,2557,2494,2577,2572,2538,2676,2590,2620,2602",
"27,18.52,2546,2768,2497,2631,2508,2470,2634,2471,2476,2540,2583,2562,2282,2557,2592,2585",
"28,19.02,2548,2538,2548,2554,2516,2519,2537,2485,2460,2574,2584,2552,2283,2542,2586,2548",
"29,19.51,2504,2490,2528,2502,2501,2502,2478,2490,2438,2494,2515,2486,2288,2552,2509,2536",
"30,20.00,2507,2476,2506,2506,2498,2427,2474,2486,2435,2493,2516,2477,2593,2506,2508,2507",
"31,20.51,2462,2412,2475,2478,2389,2416,2448,2424,2384,2442,2498,2451,2592,2462,2468,2502",
"32,21.03,2446,2428,2453,2444,2399,2437,2418,2378,2348,2412,2472,2434,2563,2437,2405,2450",
"33,21.52,2385,2375,2398,2423,2405,2401,2394,2397,2357,2414,2438,2391,2311,2429,2438,2440",
"34,22.00,2394,2392,2405,2407,2380,2368,2394,2368,2325,2405,2426,2395,2161,2412,2418,2481",
"35,22.53,2344,2332,2375,2403,2345,2412,2385,2375,2287,2355,2423,2386,2066,2407,2430,2416",
"36,23.00,2350,2340,2356,2361,2304,2334,2308,2282,2251,2342,2368,2335,2485,2337,2358,2360",
"37,23.50,2302,2301,2315,2343,2316,2279,2329,2324,2276,2312,2349,2327,2429,2322,2368,2331",
"38,24.00,2248,2246,2271,2294,2296,2263,2279,2279,2229,2254,2300,2260,2036,2298,2282,2291",
"39,24.50,2254,2252,2259,2282,2266,2206,2262,2269,2238,2268,2278,2263,2421,2266,2293,2265",
"40,25.00,2238,2222,2243,2239,2214,2228,2216,2210,2167,2228,2250,2219,2384,2246,2234,2253",
"41,25.52,2143,2189,2241,2242,2214,2102,2208,2196,2194,2165,2234,2159,2346,2228,2222,2234",
"42,26.02,2170,2185,2231,2212,2163,2177,2178,2155,2129,2200,2238,2180,2316,2222,2157,2229",
"43,26.50,2154,2171,2182,2185,2129,2163,2155,2124,2129,2192,2196,2153,2289,2184,2162,2167",
"44,27.00,2135,2124,2142,2154,2120,2138,2110,2135,2120,2146,2149,2122,1915,2139,2153,2143",
"45,27.52,2088,2088,2097,2110,2073,2065,2066,2070,2095,2083,2107,2073,1893,2108,2084,2126",
"46,28.01,2047,2059,2100,2086,2079,1993,2079,2090,2076,2088,2105,2077,1856,2089,2111,2105",
"47,28.50,2061,2072,2090,2082,2047,2076,2053,2052,2038,2062,2104,2060,1911,2080,2093,2096",
"48,29.02,2029,2046,2056,2064,2033,1986,2041,2051,2033,2064,2067,2037,2081,2050,2074,2054",
"49,29.50,2001,2028,2050,2044,2004,2029,2023,2033,2022,2058,2044,2020,1797,2050,2060,2052",
"50,30.00,1914,1950,1991,1987,1958,1880,1954,1985,1960,1992,1995,1960,2053,1989,1999,1984",
"51,30.52,1937,1945,1975,1988,1942,1964,1952,1947,1956,1968,1991,1956,1757,1982,1945,1990",
"52,31.00,1941,1932,1961,1967,1937,1915,1944,1947,1944,1934,1980,1951,1677,1970,1948,1977",
"53,31.50,1898,1897,1919,1958,1860,1939,1919,1853,1909,1898,1962,1927,1786,1929,1940,1943",
"54,32.03,1827,1883,1906,1908,1890,1841,1886,1899,1903,1902,1906,1889,2031,1912,1919,1905",
"55,32.52,1881,1887,1891,1899,1880,1879,1882,1871,1842,1881,1901,1872,1637,1894,1887,1892",
"56,33.00,1833,1851,1860,1865,1838,1851,1852,1854,1864,1862,1866,1846,1640,1841,1880,1862",
"57,33.50,1801,1810,1818,1834,1816,1798,1815,1803,1849,1831,1848,1827,1955,1820,1810,1841",
"58,34.00,1803,1812,1819,1820,1803,1818,1803,1808,1830,1830,1831,1813,1964,1823,1830,1819",
"59,34.52,1758,1771,1766,1789,1754,1773,1772,1784,1804,1782,1789,1769,1574,1769,1787,1789",
"60,35.00,1735,1752,1753,1740,1753,1726,1771,1773,1797,1763,1786,1768,1611,1753,1799,1764",
"61,35.50,1676,1718,1724,1743,1713,1695,1704,1735,1769,1696,1751,1726,1907,1726,1712,1724",
"62,36.02,1711,1686,1727,1746,1681,1706,1697,1688,1753,1734,1729,1696,1876,1738,1699,1723",
"63,36.51,1681,1661,1668,1697,1685,1698,1672,1701,1724,1671,1708,1688,1692,1660,1731,1675",
"64,37.01,1672,1677,1691,1691,1673,1695,1678,1690,1711,1684,1692,1675,1819,1676,1709,1688",
"65,37.53,1646,1645,1685,1671,1651,1620,1669,1649,1670,1665,1686,1668,1773,1661,1664,1661",
"66,38.00,1610,1590,1623,1641,1587,1541,1619,1550,1618,1617,1633,1610,1695,1627,1639,1628",
"67,38.50,1597,1592,1599,1631,1589,1571,1612,1602,1602,1631,1630,1608,1351,1607,1644,1602",
"68,39.03,1588,1586,1593,1598,1571,1558,1599,1567,1638,1594,1612,1595,1751,1594,1622,1604",
"69,39.50,1527,1595,1610,1652,1585,1552,1559,1573,1685,1572,1599,1591,1404,1607,1613,1569",
"70,40.03,1482,1510,1517,1546,1531,1506,1531,1530,1568,1534,1554,1529,1290,1515,1547,1515",
"71,40.50,1527,1529,1532,1556,1514,1531,1536,1522,1596,1541,1530,1530,1664,1538,1548,1534",
"72,41.00,1502,1507,1509,1515,1481,1484,1491,1499,1556,1534,1535,1494,1666,1518,1519,1523",
"73,41.50,1477,1479,1482,1499,1466,1483,1484,1500,1540,1456,1498,1484,1338,1475,1507,1493",
"74,42.04,1435,1442,1464,1473,1450,1394,1467,1474,1543,1429,1483,1436,1277,1466,1479,1476",
"75,42.50,1453,1469,1469,1476,1422,1472,1464,1452,1488,1464,1480,1462,1193,1467,1489,1468",
"76,43.00,1435,1430,1432,1447,1414,1350,1423,1433,1510,1435,1452,1437,1242,1433,1447,1442",
"77,43.50,1401,1399,1399,1420,1387,1403,1405,1403,1486,1410,1433,1407,1169,1405,1414,1406",
"78,44.00,1384,1397,1398,1389,1382,1384,1388,1361,1446,1381,1410,1391,1516,1400,1414,1402",
"79,44.51,1360,1364,1371,1376,1352,1374,1355,1343,1435,1327,1388,1371,1473,1372,1386,1371",
"80,45.00,1342,1348,1351,1361,1341,1339,1345,1354,1438,1358,1358,1342,1512,1353,1367,1341",
"81,45.50,1336,1335,1345,1341,1319,1319,1340,1338,1416,1318,1348,1338,1508,1334,1347,1314",
"82,46.02,1298,1321,1313,1334,1309,1293,1319,1315,1376,1317,1338,1321,1421,1321,1342,1304",
"83,46.50,1283,1292,1298,1306,1286,1263,1291,1299,1389,1300,1307,1292,1425,1298,1308,1299",
"84,47.02,1267,1277,1284,1289,1261,1257,1286,1257,1349,1272,1296,1289,1056,1284,1290,1277",
"85,47.50,1273,1277,1284,1286,1272,1272,1281,1293,1374,1291,1295,1278,1070,1284,1292,1286",
"86,48.00,1236,1251,1248,1259,1209,1258,1212,1216,1327,1240,1262,1225,1369,1249,1218,1267",
"87,48.50,1224,1221,1227,1227,1209,1188,1225,1229,1322,1221,1242,1216,1394,1225,1240,1215",
"88,49.02,1202,1213,1198,1225,1207,1189,1211,1220,1287,1213,1230,1216,1338,1205,1227,1200",
"89,49.52,1182,1183,1195,1224,1189,1156,1202,1210,1305,1163,1204,1199,1349,1198,1207,1193",
"90,50.00,1172,1177,1179,1191,1166,1150,1177,1181,1294,1185,1194,1166,1139,1176,1191,1184",
"91,50.51,1179,1167,1172,1189,1177,1149,1175,1173,1246,1181,1189,1177,1184,1177,1190,1174",
"92,51.02,1147,1156,1160,1156,1112,1117,1146,1154,1227,1142,1163,1138,918,1154,1158,1219",
"93,51.50,1130,1143,1135,1153,1129,1112,1141,1136,1238,1139,1157,1132,1232,1141,1145,1121",
"94,52.02,1116,1123,1120,1135,1113,1084,1126,1124,1201,1122,1145,1120,914,1129,1141,1136",
"95,52.50,1070,1094,1111,1099,1085,1077,1093,1091,1187,1082,1122,1106,1232,1105,1099,1074",
"96,53.00,1098,1097,1101,1104,1084,1086,1097,1103,1206,1094,1110,1086,1092,1100,1112,1099",
"97,53.52,1053,1078,1067,1083,1049,1065,1069,1052,1167,1080,1086,1069,935,1080,1072,1063",
"98,54.00,1033,1052,1055,1068,1038,1004,1049,1060,1167,1022,1067,1048,1172,1058,1056,1052",
"99,54.50,1021,1030,1031,1048,1003,994,1033,1036,1156,1041,1051,1029,1016,1041,1049,1029",
"100,55.02,1025,1027,1022,1032,1021,1008,1026,1034,1122,1011,1034,1020,814,1035,1036,1035",
"101,55.52,1009,1004,1008,1003,990,978,998,1008,1114,1009,1019,998,815,1014,1019,1016",
"102,56.02,985,996,999,1009,988,996,997,994,1118,999,1018,1000,1161,1002,1003,998",
"103,56.50,974,982,990,1015,982,970,986,986,1095,995,1010,953,1104,1003,998,991",
"104,57.03,963,972,979,990,953,960,981,973,1085,970,989,973,765,985,983,973",
"105,57.55,949,951,953,953,934,948,947,938,1068,940,971,943,758,953,954,952",
"106,58.02,950,950,943,955,943,937,943,951,1073,954,964,945,1081,953,956,910",
"107,58.51,913,933,938,942,921,886,929,939,1059,927,948,932,818,939,944,925",
"108,59.02,893,906,934,906,897,877,930,908,1035,917,914,910,1035,908,919,907",
"109,59.52,890,897,905,906,905,878,895,905,1008,899,916,904,859,906,910,896",
"110,60.02,864,879,895,908,879,830,891,891,1004,886,920,893,643,905,898,871"
};

template <size_t SENSOR_SIZE, float TEMP_START, float TEMP_END, float TEMP_STEP>
class TemperatureCalibrationStaticDataLoader {
    using Definitions = TemperatureCalibrationDefinitions <SENSOR_SIZE, TEMP_START, TEMP_END, TEMP_STEP>;
    using Collector = TemperatureCalibrationCollector <SENSOR_SIZE, TEMP_START, TEMP_END, TEMP_STEP>;

    bool parse_reading (const char* line, float& temperature, std::array <uint16_t, SENSOR_SIZE>& resistances) {
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
        for (int index = 0; index < Definitions::TEMP_SIZE; index ++) {
            std::array <uint16_t, SENSOR_SIZE> resistances; // need to be transposed
            if (!parse_reading (temperatureCalibrationData_STATIC [index], collection.temperatures [index], resistances)) {
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
        StrategyDefault defaultStrategy (config.strategyDefault);
        if (!Storage::deserialize (config.filename, defaultStrategy, calibrationStrategies, createStrategyFactories ()))
            DEBUG_PRINTF ("TemperatureCalibrationManager:: no stored calibrations (filename = %s), will rely upon default\n", config.filename.c_str ());
        runtime = std::make_shared <Runtime> (defaultStrategy, calibrationStrategies);
    }

    float calculateTemperature (const size_t index, const float resistance) const {
        if (!runtime) return -273.15f;
        return runtime->calculateTemperature (index, resistance);
    }

    bool calibrateTemperatures (const Collector::TemperatureReadFunc readTemperature, const Collector::ResistanceReadFunc readResistance) {

        std::unique_ptr <typename Collector::Collection> calibrationData = std::make_unique <typename Collector::Collection> ();
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
#endif

        for (int index = 0; index < Definitions::TEMP_SIZE; index ++) {
            DEBUG_PRINTF ("= %d,%.2f", index, calibrationData->temperatures [index]);
            for (size_t sensor = 0; sensor < SENSOR_SIZE; sensor ++)
                DEBUG_PRINTF (",%u", calibrationData->resistances [sensor] [index]);
            DEBUG_PRINTF ("\n");
        }

        Calculator calculator;
        std::unique_ptr <typename Calculator::CalibrationStrategies> calibrationStrategies = std::make_unique <typename Calculator::CalibrationStrategies> ();
        StrategyDefault defaultStrategy;
        if (!calculator.compute (*calibrationStrategies, *calibrationData, createStrategyFactories ()) || !calculator.computeDefault (defaultStrategy, *calibrationData)) {
            DEBUG_PRINTF ("TemperatureCalibrationManager::calibateTemperatures - calculator failed\n");
            return false;
        }
        DEBUG_PRINTF ("defaultStrategy: %s\n", defaultStrategy.getDetails ().c_str ());

        Storage::serialize (config.filename, defaultStrategy, *calibrationStrategies);
        // runtime = std::make_shared <Runtime> (defaultStrategy, calibrationStrategies);
        return true;
    }
};

// -----------------------------------------------------------------------------------------------
