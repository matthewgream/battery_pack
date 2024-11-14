
// -----------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------

#include <cmath>

float steinharthart_calculator (const float value, const float value_max, const float resistance_reference, const float resistance_nominal, const float temperature_nominal) {
    static constexpr float beta = 3950.0f, kelvin_constant = 273.15f;
    float resistance = resistance_reference / ((value_max / value) - 1.0f);
    float steinhart = std::log (resistance / resistance_nominal) / beta + 1.0f / (temperature_nominal + kelvin_constant);
    return 1.0f / steinhart - kelvin_constant;
}

// -----------------------------------------------------------------------------------------------

class Tester_HardwareInterfaces {
    const Config config;

    class RepetitionRunner {
        int repetitions, timedelay;

    public:
        explicit RepetitionRunner (int repetitions, int timedelay = 0) :
            repetitions (repetitions),
            timedelay (timedelay) { }
        void run (std::function<bool ()> func) {
            int iterations = 0;
            while (iterations < repetitions) {
                if (func ())
                    iterations++;
                if (timedelay > 0)
                    delay (timedelay);
            }
        }
    };

public:
    void allInterface (const int repetitions = 16) {
        static constexpr float SPEED_MIN = 0.0f, SPEED_MAX = 100.0f, SPEED_STEP = 5.0f;
        static constexpr int DELAY = 5 * 1000;

        DEBUG_PRINTF ("*** Tester_HardwareInterfaces:: all (%d repetitions)\n", repetitions);
        // FanInterfaceStrategy_motorMap strategyMap;
        ProgramInterfaceFanControllersStrategy_motorAll strategyAll;
        ProgramInterfaceFanControllers fanInterface (config.fanControllersInterface, strategyAll);
        // TemperatureSensor_DS18B20 ds18b20 (config.ds18b20);
        ProgramInterfaceTemperatureSensors temperatureInterface (config.temperatureSensorsInterface, [&] (const int channel, const uint16_t resistance) {
            return steinharthart_calculator (static_cast<float> (resistance), 4095.0f, 10000.0f, 10000.0f, 25.0f);
        });
        fanInterface.begin ();
        temperatureInterface.begin ();

        float speed = SPEED_MIN;
        RepetitionRunner (repetitions, DELAY).run ([&] () {
            DEBUG_PRINTF ("+++ FAN: speed=%.2f\n", speed);
            fanInterface.setSpeed (speed);
            speed += SPEED_STEP;
            if (speed > SPEED_MAX)
                speed = SPEED_MIN;

            DEBUG_PRINTF ("+++ TEMPERATURE: channels=%d\n", ProgramInterfaceTemperatureSensors::CHANNELS);
            // DEBUG_PRINTF ("ds18b20[ref]: %.2f\n", ds18b20.getTemperature ());
            for (int channel = 0; channel < ProgramInterfaceTemperatureSensors::CHANNELS; channel++) {
                float temperature;
                if (temperatureInterface.getTemperature (channel, &temperature))
                    DEBUG_PRINTF ("channel [%2d]: %.2f\n", channel, temperature);
                else
                    DEBUG_PRINTF ("channel [%2d]: BAD\n", channel);
            }
            DEBUG_PRINTF ("\n");

            return true;
        });
    }
    //
    void run (const int repetitions = std::numeric_limits<int>::max () - 1) {
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

    ProgramManageTemperatureSensorsCalibration calibrator (config.temperatureSensorsCalibrator);

    if (use_static) {
        calibrator.calibrateTemperatures ();
    } else {
        TemperatureSensor_DS18B20 ds18b20 (config.ds18b20);
        analogReadResolution (ProgramInterfaceTemperatureSensors::AdcResolution);
        MuxInterface_CD74HC4067<ProgramInterfaceTemperatureSensors::AdcValueType> interface (config.temperatureSensorsInterface.hardware);
        interface.enable ();

        calibrator.calibrateTemperatures ([&] () { return ds18b20.getTemperature (); }, [&] (size_t channel) { return interface.get (channel); });
    }
}

// -----------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------
