
// -----------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------

void factory_temperatureCalibration (bool use_static = false) {

    const Config config;

    ProgramManageTemperatureSensorsCalibration calibrator (config.moduleBatterypack.temperatureSensorsCalibrator);

    if (use_static) {
        calibrator.calibrateTemperatures ();
    } else {
        TemperatureSensor_DS18B20 ds18b20 (config.moduleBatterypack.ds18b20);
        analogReadResolution (ProgramInterfaceTemperatureSensors::AdcResolution);
        MuxInterface_CD74HC4067<ProgramInterfaceTemperatureSensors::AdcValueType> interface (config.moduleBatterypack.temperatureSensorsInterface.hardware);
        interface.enable ();

        calibrator.calibrateTemperatures ([&] () { return ds18b20.getTemperature (); }, [&] (size_t channel) { return interface.get (channel); });
    }
}

// -----------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------
