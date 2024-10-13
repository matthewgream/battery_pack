
// -----------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------

float steinharthart_calculator (const float value, const float value_max, const float resistance_reference, const float resistance_nominal, const float temperature_nominal) {
    static constexpr float beta = 3950.0f, kelvin_constant = 273.15f;
    float resistance = resistance_reference / ((value_max / value) - 1.0f);
    float steinhart = logf (resistance / resistance_nominal) / beta + 1.0f / (temperature_nominal + kelvin_constant);
    return 1.0f / steinhart - kelvin_constant;
}

// -----------------------------------------------------------------------------------------------

class Tester_HardwareInterfaces {
    const Config config;

    class RepetitionRunner {
        int repetitions, timedelay;
    public:
        explicit RepetitionRunner (int repetitions, int timedelay = 0): repetitions (repetitions), timedelay (timedelay) {}
        void run (std::function <bool ()> func) {
            int iterations = 0;
            while (iterations < repetitions) {
                if (func ())
                    iterations ++;
                if (timedelay > 0)
                    delay (timedelay);
            }
        }
    };

public:
    void allInterface (const int repetitions = 16) {
        static constexpr int SPEED_MIN = 0, SPEED_MAX = 255, SPEED_STEP = 16;
        static constexpr int DELAY = 5*1000;

        DEBUG_PRINTF ("*** Tester_HardwareInterfaces:: all (%d repetitions)\n", repetitions);
        FanInterfaceStrategy_motorMap strategyMap;
        // FanInterfaceStrategy_motorAll strategyAll;
        FanInterface fanInterface (config.fanInterface, strategyMap);
        TemperatureSensor_DS18B20 ds18b20 (config.ds18b20);
        TemperatureInterface temperatureInterface (config.temperatureInterface, [&] (const int channel, const uint16_t resistance) { return steinharthart_calculator (static_cast <float> (resistance), 4095.0f, 10000.0f, 10000.0f, 25.0f); });
        fanInterface.begin ();
        temperatureInterface.begin ();

        int speed = SPEED_MIN;
        RepetitionRunner (repetitions, DELAY).run ([&] () {

            DEBUG_PRINTF ("+++ FAN: speed=%d\n", speed);
            fanInterface.setSpeed (static_cast <FanInterface::FanSpeedType> (speed));
            speed = (speed + SPEED_STEP) > SPEED_MAX ? SPEED_MIN : speed + SPEED_STEP;

            DEBUG_PRINTF ("+++ TEMPERATURE: channels=%d\n", TemperatureInterface::CHANNELS);
            DEBUG_PRINTF ("ds18b20[ref]: %.2f\n", ds18b20.getTemperature ());
            for (int channel = 0; channel < TemperatureInterface::CHANNELS; channel ++)
                DEBUG_PRINTF ("channel [%2d]: %.2f\n", channel, temperatureInterface.getTemperature (channel));
            DEBUG_PRINTF ("\n");

            return true;
        });
    }
    //
    void run (const int repetitions = std::numeric_limits <int>::max () - 1) {
        DEBUG_PRINTF ("*** Tester_HardwareInterfaces (%d repetitions)\n", repetitions);
        RepetitionRunner (repetitions).run ([&] () {
            allInterface ();
            return true;
        });
    }
};

// -----------------------------------------------------------------------------------------------

void factory_hardwareInterfaceTest () {
    Tester_HardwareInterfaces tester;
    tester.run ();
}

// -----------------------------------------------------------------------------------------------

void factory_temperatureCalibration (bool use_static = false) {

    const Config config;

    TemperatureCalibrator calibrator (config.temperatureCalibrator);

    if (use_static) {
        calibrator.calibrateTemperatures ();
    } else {
        TemperatureSensor_DS18B20 ds18b20 (config.ds18b20);
        analogReadResolution (TemperatureInterface::AdcResolution);
        MuxInterface_CD74HC4067 <TemperatureInterface::AdcValueType> interface (config.temperatureInterface.hardware);
        interface.enable ();

        calibrator.calibrateTemperatures ([&] () { return ds18b20.getTemperature (); }, [&] (size_t channel) { return interface.get (channel); });
    }
}

// -----------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------
