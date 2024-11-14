
// -----------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------

#undef HARDWARE_ESP32_C3_ZERO
#define HARDWARE_ESP32_S3_YD_ESP32_S3_C

// -----------------------------------------------------------------------------------------------

#ifdef PLATFORMIO
static String __DEFAULT_TYPE_BUILDER (const char *prefix, const char *platform, const char *hardware) {
    String r = String (prefix) + "-" + platform + "-" + hardware;
    r.toLowerCase ();
    return r;
}
#ifndef BUILD_PLATFORM
#define BUILD_PLATFORM "D3AD"
#endif
#ifndef BUILD_HARDWARE
#define BUILD_HARDWARE "B33F"
#endif
#define __DEFAULT_TYPE_BUILT __DEFAULT_TYPE_BUILDER (DEFAULT_NAME, BUILD_PLATFORM, BUILD_HARDWARE)
#else
// {build.source.path}\tools\upload_fota.ps1 -file_info {build.source.path}\Program.hpp -path_build {build.path} -platform {build.board}-{build.arch} -image {build.path}\{build.project_name}.bin -server http://ota.local:8090/images -verbose
// -DARDUINO_BOARD="ESP32S3_DEV" -DARDUINO_ARCH_ESP32
static String __DEFAULT_TYPE_BUILDER (const char *prefix, const char *board, const char *arch) {
    String a = arch;
    a.replace ("ARDUINO_ARCH_", "");
    String b = board;
    b.replace ("_", "");
    b.replace ("-", "");
    String r = String (prefix) + "-" + b + "-" + a;
    r.toLowerCase ();
    return r;
}
#define __DEFAULT_TYPE_BUILT          __DEFAULT_TYPE_BUILDER (DEFAULT_NAME, ARDUINO_BOARD, __DEFAULT_TYPE_STRINGIFIER (ARDUINO_ARCH_ESP32))
#define __DEFAULT_TYPE_STRINGIFIER(x) #x
#endif

// -----------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------

#define DEFAULT_WATCHDOG_SECS (60)
#define DEFAULT_INITIAL_DELAY (5 * 1000L)
#ifdef DEBUG
#define DEFAULT_DEBUG_LOGGING_BUFFER (2 * 1024)
#define DEFAULT_SCRUB_SENSITIVE_CONTENT_FROM_NETWORK_LOGGING
#endif

#define DEFAULT_NAME "BatteryMonitor"
#define DEFAULT_VERS "1.5.0"
#define DEFAULT_TYPE __DEFAULT_TYPE_BUILT
#define DEFAULT_JSON "http://ota.local:8090/images/images.json"

// -----------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------

#include <Arduino.h>

#include "Utilities.hpp"
#include "UtilitiesMath.hpp"
#include "UtilitiesJson.hpp"
#include "UtilitiesPlatform.hpp"

#include "ComponentsHardware.hpp"
#include "ComponentsDevicesUtilities.hpp"
#include "ComponentsDevicesMulticastDNSPublisher.hpp"
#include "ComponentsDevicesWebSocket.hpp"
#include "ComponentsDevicesWebServer.hpp"
#include "ComponentsDevicesBluetoothServer.hpp"
#include "ComponentsDevicesMQTTClient.hpp"
#include "ComponentsDevicesSPIFFSFile.hpp"
#include "ComponentsDevicesNetworkTimeFetcher.hpp"
#include "ComponentsDevicesWifiNetworkClient.hpp"

// -----------------------------------------------------------------------------------------------

class Component {
protected:
    ~Component () {};

public:
    typedef std::vector<Component *> List;
    virtual void begin () { }
    virtual void process () { }
};

using ProgramAlarmsInterface = ActivablePIN;

#include "ProgramDiagnostics.hpp"
#include "ProgramAlarms.hpp"
#include "ProgramPlatform.hpp"
#include "ProgramComponents.hpp"
#include "ProgramInterfaceTemperatureSensors.hpp"
#include "ProgramInterfaceFanControllers.hpp"
#include "ProgramManageTemperatureSensors.hpp"
#include "ProgramMechanicsTemperatureCalibration.hpp"
#include "ProgramManageTemperatureCalibration.hpp"
#include "ProgramManageFanControllers.hpp"
#include "ProgramManageSerialDalyBMS.hpp"
#include "ProgramManageBluetoothTPMS.hpp"
#include "ProgramUpdates.hpp"
#include "ProgramLogging.hpp"
#include "ProgramTime.hpp"
#include "ProgramDataControl.hpp"
#include "ProgramDataManage.hpp"
#include "UtilitiesOTA.hpp"    // breaks if included earlier due to SPIFFS headers

static inline constexpr size_t HARDWARE_TEMP_SIZE = ProgramInterfaceTemperatureSensors::CHANNELS;
static inline constexpr float HARDWARE_TEMP_START = 5.0f, HARDWARE_TEMP_END = 60.0f, HARDWARE_TEMP_STEP = 0.5f;
using ProgramManageTemperatureSensorsBatterypack = ProgramManageTemperatureBatterypackTemplate<HARDWARE_TEMP_SIZE - 1>;
using ProgramManageTemperatureSensorsEnvironment = ProgramManageTemperatureEnvironmentTemplate<1>;
using ProgramManageTemperatureSensorsCalibration = ProgramManageTemperatureCalibrationTemplate<HARDWARE_TEMP_SIZE, HARDWARE_TEMP_START, HARDWARE_TEMP_END, HARDWARE_TEMP_STEP>;

// -----------------------------------------------------------------------------------------------

#include "ProgramSecrets.hpp"
#include "ProgramConfig.hpp"

// -----------------------------------------------------------------------------------------------

class Program : public Component, public Diagnosticable {

    const Config config;
    const String address;

    ProgramPlatformArduino programPlatform;
    TwoWire i2c_bus0, i2c_bus1;
    ProgramComponents programComponentsManager;

    // Batterypack management
    ProgramInterfaceFanControllersStrategy_motorMapWithRotation fanInterfaceStrategy;
    PidController<double> fanControllingAlgorithm;
    AlphaSmoothing<double> fanSmoothingAlgorithm;
    ProgramManageTemperatureSensorsCalibration temperatureSensorsCalibrator;
    ProgramInterfaceTemperatureSensors temperatureSensorsInterface;
    ProgramInterfaceFanControllers fanControllersInterface;
    ProgramManageTemperatureSensorsBatterypack temperatureSensorsManagerBatterypack;
    ProgramManageTemperatureSensorsEnvironment temperatureSensorsManagerEnvironment;
    ProgramManageFanControllers fanControllersManager;
    ProgramManageSerialDalyBMS batteryManager;
    ProgramManageBluetoothTPMS tyrePressureManager;    // XXX needs to be after bluetooth deliver has initialised. need to refactor this

    //

    ProgramDataControl dataControl;
    ProgramDataDeliver dataDeliver;
    ProgramDataPublish dataPublish;
    ProgramDataStorage dataStorage;

    class OperationalManager {
        const Program *_program;

    public:
        explicit OperationalManager (const Program *program) :
            _program (program) {};
        void collect (JsonVariant &) const;
    } operational;
    Intervalable dataProcessInterval, dataDeliverInterval, dataCaptureInterval, dataDiagnoseInterval;
    String dataCollect (const String &name, const std::function<void (JsonVariant &)> func) const {
        JsonCollector collector (name, getTimeString (), address);
        JsonVariant obj = collector.document ().as<JsonVariant> ();
        func (obj);
        return collector;
    }

    void dataCapture (const String &data, const bool publishData, const bool storageData) {
        class StorageLineHandler : public ProgramDataStorage::LineCallback {
            ProgramDataPublish &_publish;
        public:
            explicit StorageLineHandler (ProgramDataPublish &publish) :
                _publish (publish) { }
            bool process (const String &line) override {
                return line.isEmpty () ? true : _publish.publish (line, "data");
            }
        };
        if (publishData) {
            if (dataStorage.available () && dataStorage.size () > 0) {    // even if !storageData, still drain it
                DEBUG_PRINTF ("Program::doCapture: publish.connected () && storage.size () > 0\n");
                // probably needs to be time bound and recall and reuse an offset ... or this will cause infinite watchdog loop one day
                StorageLineHandler handler (dataPublish);
                if (dataStorage.retrieve (handler))
                    dataStorage.erase ();
            }
            if (! dataPublish.publish (data, "data"))
                if (storageData)
                    dataStorage.append (data);
        } else if (storageData)
            dataStorage.append (data);
    }

    void dataProcess () {

        const bool dataShouldDeliver = dataDeliverInterval, dataToDeliver = dataShouldDeliver && dataDeliver.available ();
        const bool dataShouldCapture = dataCaptureInterval, dataToCaptureToPublish = dataShouldCapture && (config.dataPublishEnabled && dataPublish.available ()), dataToCaptureToStorage = dataShouldCapture && (config.dataStorageEnabled && dataStorage.available ());
        const bool diagShould = dataDiagnoseInterval && (config.diagDeliverEnabled || config.diagPublishEnabled), diagToDeliver = diagShould && (config.diagDeliverEnabled && dataDeliver.available ()), diagToPublish = diagShould && (config.diagPublishEnabled && dataPublish.available ());

        DEBUG_PRINTF ("Program::process: deliver=%d/%d, capture=%d/%d/%d, diagnose=%d/%d/%d\n", dataShouldDeliver, dataToDeliver, dataShouldCapture, dataToCaptureToPublish, dataToCaptureToStorage, diagShould, diagToDeliver, diagToPublish);

        if (dataToDeliver || (dataToCaptureToPublish || dataToCaptureToStorage)) {
            const String data = dataCollect ("data", [&] (JsonVariant &obj) {
                operational.collect (obj);
            });
            DEBUG_PRINTF ("Program::process: data, length=%d, content=<<<%s>>>\n", data.length (), data.c_str ());
            if (dataToDeliver)
                dataDeliver.deliver (data, "data", config.dataPublishEnabled && dataPublish.available ());
            if (dataToCaptureToPublish || dataToCaptureToStorage)
                dataCapture (data, dataToCaptureToPublish, dataToCaptureToStorage);
        }

        if (diagToDeliver || diagToPublish) {
            const String diag = dataCollect ("diag", [&] (JsonVariant &obj) {
                programDiagnostics.collect (obj);
            });
            DEBUG_PRINTF ("Program::process: diag, length=%d, content=<<<%s>>>\n", diag.length (), diag.c_str ());
            if (diagToDeliver)
                dataDeliver.deliver (diag, "diag", config.dataPublishEnabled && dataPublish.available ());
            if (diagToPublish)
                dataPublish.publish (diag, "diag");
        }
    }

    void process () override {
        if (dataProcessInterval)
            dataProcess ();
    }

    //

    ProgramTime programTime;
    ProgramLogging programLogging;
    ProgramUpdates programUpdater;
    ProgramAlarmsInterface programAlarmsInterface;
    ProgramAlarms programAlarms;
    ProgramDiagnostics programDiagnostics;
    Uptime programUptime;
    ActivationTracker programCycles;
    Intervalable programInterval;
    Component::List programComponents;
    template <auto MethodPtr>
    void forEachComponent () {
        for (auto &component : programComponents)
            (component->*MethodPtr) ();
    }

    //

public:
    Program () :
        address (getMacAddressBase ("")),
        programPlatform (config.programPlatform),
        i2c_bus0 (0),
        i2c_bus1 (1),
        programComponentsManager (config.programComponents, [&] () { return programComponentsManager.wifi ().available (); }),    // for now, until other networks
        //
        fanControllingAlgorithm (config.FAN_CONTROL_P, config.FAN_CONTROL_I, config.FAN_CONTROL_D),
        fanSmoothingAlgorithm (config.FAN_SMOOTH_A),
        temperatureSensorsCalibrator (config.temperatureSensorsCalibrator),
        temperatureSensorsInterface (config.temperatureSensorsInterface, [&] (const int channel, const uint16_t resistance) {
            return temperatureSensorsCalibrator.calculateTemperature (channel, resistance);
        }),
        fanControllersInterface (config.fanControllersInterface, fanInterfaceStrategy),
        temperatureSensorsManagerBatterypack (config.temperatureSensorsManagerBatterypack, temperatureSensorsInterface),
        temperatureSensorsManagerEnvironment (config.temperatureSensorsManagerEnvironment, temperatureSensorsInterface),
        fanControllersManager (config.fanControllersManager, fanControllersInterface, fanControllingAlgorithm, fanSmoothingAlgorithm, [&] () {
            return ProgramManageFanControllers::TargetSet (temperatureSensorsManagerBatterypack.setpoint (), temperatureSensorsManagerBatterypack.current ());
        }),
        batteryManager (config.batteryManagerManager),
        tyrePressureManager (config.tyrePressureManager),    // XXX needs to be earlier
        //
        dataControl (config.dataControl, address, programComponentsManager),
        dataDeliver (config.dataDeliver, address, programComponentsManager.blue (), programComponentsManager.mqtt (), programComponentsManager.websocket ()),
        dataPublish (config.dataPublish, address, programComponentsManager.mqtt ()),
        dataStorage (config.dataStorage),
        operational (this),
        dataProcessInterval (config.dataProcessInterval),
        dataDeliverInterval (config.dataDeliverInterval),
        dataCaptureInterval (config.dataCaptureInterval),
        dataDiagnoseInterval (config.dataDiagnoseInterval),
        //
        programTime (config.programTime, i2c_bus0, [&] () { return programComponentsManager.wifi ().available (); }),
        programLogging (config.programLogging, getMacAddressBase (""), &programComponentsManager.mqtt ()),
        programUpdater (config.programUpdater, [&] () { return programComponentsManager.wifi ().available (); }),    // for now, until other networks
        programAlarmsInterface (config.programAlarmsInterface),
        programAlarms (config.programAlarms, programAlarmsInterface, { &temperatureSensorsManagerEnvironment, &temperatureSensorsManagerBatterypack, &dataDeliver, &dataPublish, &dataStorage, &programTime, &programPlatform }),
        programDiagnostics (config.programDiagnostics, { &programComponentsManager, &temperatureSensorsCalibrator, &temperatureSensorsInterface, &fanControllersInterface, &temperatureSensorsManagerBatterypack, &temperatureSensorsManagerEnvironment, &fanControllersManager, &tyrePressureManager, &dataDeliver, &dataPublish, &dataStorage, &dataControl, &programTime, &programUpdater, &programAlarms, &programPlatform, this }),
        programComponents ({ &temperatureSensorsCalibrator, &temperatureSensorsInterface, &fanControllersInterface, &temperatureSensorsManagerBatterypack, &temperatureSensorsManagerEnvironment, &fanControllersManager, &programAlarms, &tyrePressureManager, &programComponentsManager, &dataDeliver, &dataPublish, &dataStorage, &dataControl, &programTime, &programUpdater, &programDiagnostics, this }),
        programInterval (config.programInterval) {
        DEBUG_PRINTF ("Program::constructor: intervals [program=%lu] - process=%lu, deliver=%lu, capture=%lu, diagnose=%lu\n", config.programInterval, config.dataProcessInterval, config.dataDeliverInterval, config.dataCaptureInterval, config.dataDiagnoseInterval);
        i2c_bus0.setPins (config.i2c0.PIN_SDA, config.i2c0.PIN_SCL);
        i2c_bus1.setPins (config.i2c1.PIN_SDA, config.i2c1.PIN_SCL);
    };

    void setup () {
        forEachComponent<&Component::begin> ();
    }
    void loop () {
        programInterval.wait (); // regularity
        forEachComponent<&Component::process> ();
        programCycles++;
    }

protected:
    void collectDiagnostics (JsonVariant &obj) const override {
        JsonObject program = obj ["program"].to<JsonObject> ();
        extern const String build;
        program ["build"] = build;
        program ["uptime"] = programUptime;
        program ["cycles"] = programCycles;
        if (programInterval.exceeded () > 0)
            program ["delays"] = programInterval.exceeded ();
    }
};

// -----------------------------------------------------------------------------------------------

void Program::OperationalManager::collect (JsonVariant &obj) const {
    JsonObject tmp = obj ["tmp"].to<JsonObject> ();
    JsonObject bms = tmp ["bms"].to<JsonObject> ();
    auto x = _program->batteryManager.instant ();
    bms ["V"] = x.voltage;
    bms ["I"] = x.current;
    bms ["C"] = x.charge;
    tmp ["env"] = _program->temperatureSensorsManagerEnvironment.getTemperature ();
    JsonObject bat = tmp ["bat"].to<JsonObject> ();
    bat ["avg"] = _program->temperatureSensorsManagerBatterypack.avg ();
    bat ["min"] = _program->temperatureSensorsManagerBatterypack.min ();
    bat ["max"] = _program->temperatureSensorsManagerBatterypack.max ();
    JsonArray val = bat ["val"].to<JsonArray> ();
    for (const auto &v : _program->temperatureSensorsManagerBatterypack.getTemperatures ())
        val.add (v);
    obj ["fan"] = _program->fanControllersInterface.getSpeed ();
    obj ["alm"] = _program->programAlarms.toString ();
}

// -----------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------
