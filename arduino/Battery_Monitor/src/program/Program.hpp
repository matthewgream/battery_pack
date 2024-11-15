
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

#include "utilities/Utilities.hpp"
#include "utilities/UtilitiesMath.hpp"
#include "utilities/UtilitiesJson.hpp"
#include "platform/PlatformArduinoESP32.hpp"

// -----------------------------------------------------------------------------------------------

#include "hardware/HardwareComponents.hpp"
#include "storage/StorageSPIFFSFile.hpp"

// -----------------------------------------------------------------------------------------------

class Component {
protected:
    ~Component () {};

public:
    typedef std::vector<Component *> List;
    virtual void begin () { }
    virtual void process () { }
};

#include "program/ProgramDiagnostics.hpp"
using ProgramAlarmsInterface = ActivablePIN;
#include "program/ProgramAlarms.hpp"

// -----------------------------------------------------------------------------------------------

#include "connectivity/ConnectivityUtilities.hpp"
#include "connectivity/ConnectivityMulticastDNSPublisher.hpp"
#include "connectivity/ConnectivityWebSocket.hpp"
#include "connectivity/ConnectivityWebServer.hpp"
#include "connectivity/ConnectivityBluetoothServer.hpp"
#include "connectivity/ConnectivityMQTTClient.hpp"
#include "connectivity/ConnectivityNetworkTimeFetcher.hpp"
#include "connectivity/ConnectivityWifiNetworkClient.hpp"

// -----------------------------------------------------------------------------------------------

class ModuleConnectivity : public Component, public Diagnosticable {
public:
    typedef struct {
        BluetoothServer::Config blue;
        MulticastDNSPublisher::Config mdns;
        MQTTClient::Config mqtt;
        WebServer::Config webserver;
        WebSocket::Config websocket;
        WiFiNetworkClient::Config wifi;

    } Config;

    using BooleanFunc = std::function<bool ()>;

private:
    const Config &config;
    const BooleanFunc _networkIsAvailable;

    BluetoothServer _blue;
    MulticastDNSPublisher _mdns;
    MQTTClient _mqtt;
    WebServer _webserver;
    WebSocket _websocket;
    WiFiNetworkClient _wifi;

public:
    explicit ModuleConnectivity (const Config &cfg, const BooleanFunc networkIsAvailable) :
        config (cfg),
        _networkIsAvailable (networkIsAvailable),
        _blue (config.blue),
        _mdns (config.mdns),
        _mqtt (config.mqtt),
        _wifi (config.wifi, _mdns),
        _webserver (config.webserver),
        _websocket (config.websocket) { }

    void begin () override {
        _wifi.begin ();
        _mdns.begin ();
        _blue.begin ();
        _mqtt.begin ();
        _webserver.begin ();
        _websocket.begin ();
    }
    void process () override {
        _wifi.process ();
        _blue.process ();
        if (_networkIsAvailable ()) {
            _mdns.process ();
            _mqtt.process ();
            _webserver.process ();
            _websocket.process ();
        }
    }
    //
    MulticastDNSPublisher &mdns () {
        return _mdns;
    }
    BluetoothServer &blue () {
        return _blue;
    }
    MQTTClient &mqtt () {
        return _mqtt;
    }
    WebServer &webserver () {
        return _webserver;
    }
    WebSocket &websocket () {
        return _websocket;
    }
    WiFiNetworkClient &wifi () {
        return _wifi;
    }

protected:
    void collectDiagnostics (JsonVariant &obj) const override {
        JsonObject sub = obj ["components"].to<JsonObject> ();
        sub ["blue"] = _blue;
        sub ["mdns"] = _mdns;
        sub ["mqtt"] = _mqtt;
        sub ["webserver"] = _webserver;
        sub ["websocket"] = _websocket;
        sub ["wifi"] = _wifi;
    }
};

// -----------------------------------------------------------------------------------------------

#include "batterypack/BatterypackInterfaceTemperatureSensors.hpp"
#include "batterypack/BatterypackInterfaceFanControllers.hpp"
#include "batterypack/BatterypackManageTemperatureSensors.hpp"
#include "batterypack/BatterypackMechanicsTemperatureCalibration.hpp"
#include "batterypack/BatterypackManageTemperatureCalibration.hpp"
#include "batterypack/BatterypackManageFanControllers.hpp"
#include "batterypack/BatterypackManageSerialDalyBMS.hpp"

static inline constexpr size_t HARDWARE_TEMP_SIZE = ProgramInterfaceTemperatureSensors::CHANNELS;
static inline constexpr float HARDWARE_TEMP_START = 5.0f, HARDWARE_TEMP_END = 60.0f, HARDWARE_TEMP_STEP = 0.5f;
using ProgramManageTemperatureSensorsBatterypack = ProgramManageTemperatureBatterypackTemplate<HARDWARE_TEMP_SIZE - 1>;
using ProgramManageTemperatureSensorsEnvironment = ProgramManageTemperatureEnvironmentTemplate<1>;
using ProgramManageTemperatureSensorsCalibration = ProgramManageTemperatureCalibrationTemplate<HARDWARE_TEMP_SIZE, HARDWARE_TEMP_START, HARDWARE_TEMP_END, HARDWARE_TEMP_STEP>;

class ModuleBatterypack : public Component, public Diagnosticable {
public:
    typedef struct {
        ProgramManageTemperatureSensorsCalibration::Config temperatureSensorsCalibrator;
        ProgramInterfaceTemperatureSensors::Config temperatureSensorsInterface;
        ProgramManageTemperatureSensorsBatterypack::Config temperatureSensorsManagerBatterypack;
        ProgramManageTemperatureSensorsEnvironment::Config temperatureSensorsManagerEnvironment;
        double FAN_CONTROL_P, FAN_CONTROL_I, FAN_CONTROL_D, FAN_SMOOTH_A;
        ProgramInterfaceFanControllers::Config fanControllersInterface;
        ProgramManageFanControllers::Config fanControllersManager;
        ProgramManageSerialDalyBMS::Config batteryManagerManager;
        TemperatureSensor_DS18B20::Config ds18b20;
        DiagnosticablesManager::Config moduleDiagnostics;
    } Config;

private:
    const Config &config;

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

    DiagnosticablesManager moduleDiagnostics;
    Component::List moduleComponents;
    template <auto MethodPtr>
    void forEachComponent () {
        for (auto &component : moduleComponents)
            (component->*MethodPtr) ();
    }

public:
    ModuleBatterypack (const Config &conf) :
        config (conf),
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
        //        programAlarms (config.programAlarms, programAlarmsInterface, { &temperatureSensorsManagerEnvironment, &temperatureSensorsManagerBatterypack, &dataDeliver, &dataPublish, &dataStorage, &programTime, &programPlatform }), XXX
        moduleDiagnostics (config.moduleDiagnostics, { &temperatureSensorsCalibrator, &temperatureSensorsInterface, &fanControllersInterface, &temperatureSensorsManagerBatterypack, &temperatureSensorsManagerEnvironment, &fanControllersManager, this }),
        moduleComponents ({ &temperatureSensorsCalibrator, &temperatureSensorsInterface, &fanControllersInterface, &temperatureSensorsManagerBatterypack, &temperatureSensorsManagerEnvironment, &fanControllersManager, this }) {
    }

    // XXX for now, to connect alarms and program status reads
    const ProgramManageTemperatureSensorsBatterypack &getTemperatureSensorsManagerBatterypack () const { return temperatureSensorsManagerBatterypack; };
    const ProgramManageTemperatureSensorsEnvironment &getTemperatureSensorsManagerEnvironment () const { return temperatureSensorsManagerEnvironment; };
    const ProgramInterfaceFanControllers &getFanControllersInterface () const { return fanControllersInterface; };
    const ProgramManageSerialDalyBMS &getBatteryManager () const { return batteryManager; }

    void begin () override {
        forEachComponent<&Component::begin> ();
    }
    void process () override {
        forEachComponent<&Component::process> ();
    }

protected:
    void collectDiagnostics (JsonVariant &obj) const override {
        JsonVariant sub = obj ["batterypack"].to<JsonVariant> ();
        moduleDiagnostics.collect (sub);
    }
};

// -----------------------------------------------------------------------------------------------

#include "conditions/ProgramManageBluetoothTPMS.hpp"

// -----------------------------------------------------------------------------------------------

#include "content/ProgramDataControl.hpp"
#include "content/ProgramDataManage.hpp"
#include "utilities/UtilitiesOTA.hpp"    // breaks if included earlier due to SPIFFS headers

// -----------------------------------------------------------------------------------------------

#include "program/ProgramUpdates.hpp"
#include "program/ProgramLogging.hpp"
#include "program/ProgramTime.hpp"
#include "program/ProgramPlatformArduinoESP32.hpp"
#include "program/ProgramSecrets.hpp"
#include "program/ProgramConfig.hpp"

// -----------------------------------------------------------------------------------------------

class Program : public Component, public Diagnosticable {

    const Config config;
    const String address;

    PlatformArduinoESP32 platform;
    TwoWire i2c_bus0, i2c_bus1;

    ModuleBatterypack moduleBatterypack;
    ProgramManageBluetoothTPMS tyrePressureManager;
    ModuleConnectivity moduleConnectivity;

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
    DiagnosticablesManager programDiagnostics;
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
        platform (config.programPlatform),
        i2c_bus0 (0),
        i2c_bus1 (1),
        moduleBatterypack (config.moduleBatterypack),
        tyrePressureManager (config.tyrePressureManager),
        moduleConnectivity (config.moduleConnectivity, [&] () { return moduleConnectivity.wifi ().available (); }),    // for now, until other networks
        //
        dataControl (config.dataControl, address, moduleConnectivity),
        dataDeliver (config.dataDeliver, address, moduleConnectivity.blue (), moduleConnectivity.mqtt (), moduleConnectivity.websocket ()),
        dataPublish (config.dataPublish, address, moduleConnectivity.mqtt ()),
        dataStorage (config.dataStorage),
        operational (this),
        dataProcessInterval (config.dataProcessInterval),
        dataDeliverInterval (config.dataDeliverInterval),
        dataCaptureInterval (config.dataCaptureInterval),
        dataDiagnoseInterval (config.dataDiagnoseInterval),
        //
        programTime (config.programTime, i2c_bus0, [&] () { return moduleConnectivity.wifi ().available (); }),
        programLogging (config.programLogging, getMacAddressBase (""), &moduleConnectivity.mqtt ()),
        programUpdater (config.programUpdater, [&] () { return moduleConnectivity.wifi ().available (); }),    // for now, until other networks
        programAlarmsInterface (config.programAlarmsInterface),
        programAlarms (config.programAlarms, programAlarmsInterface, { &moduleBatterypack.getTemperatureSensorsManagerEnvironment (), &moduleBatterypack.getTemperatureSensorsManagerBatterypack (), &dataDeliver, &dataPublish, &dataStorage, &programTime, &platform }),
        programDiagnostics (config.programDiagnostics, { &moduleConnectivity, &moduleBatterypack, &tyrePressureManager, &dataDeliver, &dataPublish, &dataStorage, &dataControl, &programTime, &programUpdater, &programAlarms, &platform, this }),
        programComponents ({ &moduleBatterypack, &moduleConnectivity, &tyrePressureManager, &programAlarms, &dataDeliver, &dataPublish, &dataStorage, &dataControl, &programTime, &programUpdater, &programDiagnostics, this }),    // XXX note order of tpm / connectivity due to bluetooth init
        programInterval (config.programInterval) {
        DEBUG_PRINTF ("Program::constructor: intervals [program=%lu] - process=%lu, deliver=%lu, capture=%lu, diagnose=%lu\n", config.programInterval, config.dataProcessInterval, config.dataDeliverInterval, config.dataCaptureInterval, config.dataDiagnoseInterval);
        i2c_bus0.setPins (config.i2c0.PIN_SDA, config.i2c0.PIN_SCL);
        i2c_bus1.setPins (config.i2c1.PIN_SDA, config.i2c1.PIN_SCL);
    };

    void setup () {
        forEachComponent<&Component::begin> ();
    }
    void loop () {
        programInterval.wait ();    // regularity
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
    auto x = _program->moduleBatterypack.getBatteryManager ().instant ();
    bms ["V"] = x.voltage;
    bms ["I"] = x.current;
    bms ["C"] = x.charge;
    tmp ["env"] = _program->moduleBatterypack.getTemperatureSensorsManagerEnvironment ().getTemperature ();
    JsonObject bat = tmp ["bat"].to<JsonObject> ();
    bat ["avg"] = _program->moduleBatterypack.getTemperatureSensorsManagerBatterypack ().avg ();
    bat ["min"] = _program->moduleBatterypack.getTemperatureSensorsManagerBatterypack ().min ();
    bat ["max"] = _program->moduleBatterypack.getTemperatureSensorsManagerBatterypack ().max ();
    JsonArray val = bat ["val"].to<JsonArray> ();
    for (const auto &v : _program->moduleBatterypack.getTemperatureSensorsManagerBatterypack ().getTemperatures ())
        val.add (v);
    obj ["fan"] = _program->moduleBatterypack.getFanControllersInterface ().getSpeed ();
    obj ["alm"] = _program->programAlarms.toString ();
}

// -----------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------
