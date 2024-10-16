
// -----------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------

#define DEFAULT_WATCHDOG_SECS (60)
#define DEFAULT_INITIAL_DELAY (5*1000L)
#ifdef DEBUG
#define DEFAULT_DEBUG_LOGGING_BUFFER (2048+1)
#define DEFAULT_SCRUB_SENSITIVE_CONTENT_FROM_NETWORK_LOGGING
#endif

#define DEFAULT_NAME "BatteryMonitor"
#define DEFAULT_VERS "1.1.1"
#define DEFAULT_TYPE "batterymonitor-custom-esp32c3"
#define DEFAULT_JSON "http://ota.local:8090/images/images.json"

// -----------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------

#include <Arduino.h>

#include "Utilities.hpp"
#include "UtilitiesJson.hpp"
#include "UtilitiesPlatform.hpp"

#include "Components.hpp"
#include "ComponentsHardware.hpp"
#include "ComponentsBluetooth.hpp"

// -----------------------------------------------------------------------------------------------

using AlarmInterface = ActivablePIN;

#include "ProgramBase.hpp"
#include "ProgramHardwareInterface.hpp"
#include "ProgramHardwareManage.hpp"
#include "ProgramNetworkManage.hpp"
#include "ProgramDataManage.hpp"
#include "ProgramManager.hpp"
#include "ProgramTemperatureCalibration.hpp"
#include "UtilitiesOTA.hpp" // breaks if included earlier due to SPIFFS headers

static inline constexpr size_t HARDWARE_TEMP_SIZE = TemperatureInterface::CHANNELS;
static inline constexpr float HARDWARE_TEMP_START = 5.0f, HARDWARE_TEMP_END = 60.0f, HARDWARE_TEMP_STEP = 0.5f;
using TemperatureManagerBatterypack = TemperatureManagerBatterypackTemplate <HARDWARE_TEMP_SIZE - 1>;
using TemperatureManagerEnvironment = TemperatureManagerEnvironmentTemplate <1>;
using TemperatureCalibrator = TemperatureCalibrationManager <HARDWARE_TEMP_SIZE, HARDWARE_TEMP_START, HARDWARE_TEMP_END, HARDWARE_TEMP_STEP>;

// -----------------------------------------------------------------------------------------------

#include "Config.hpp"

// -----------------------------------------------------------------------------------------------

class PlatformArduino: public Alarmable, public Diagnosticable {
    static constexpr int HEAP_FREE_PERCENTAGE_MINIMUM = 5;
    const unsigned long code_size, heap_size;
    const int reset_reason;
    const std::pair <String, String> reset_details;
    const bool reset_okay;

public:
    PlatformArduino (): Alarmable ({
            AlarmCondition (ALARM_SYSTEM_MEMORYLOW, [this] () { return ((100 * esp_get_minimum_free_heap_size ()) / heap_size) < HEAP_FREE_PERCENTAGE_MINIMUM; }),
            AlarmCondition (ALARM_SYSTEM_BADRESET, [this] () { return !reset_okay; })
        }), code_size (ESP.getSketchSize ()), heap_size (ESP.getHeapSize ()), reset_reason (getResetReason ()), reset_details (getResetDetails (reset_reason)), reset_okay (getResetOkay (reset_reason)) {
        DEBUG_PRINTF ("PlatformArduino::init: code=%lu, heap=%lu, reset=%d\n", code_size, heap_size, reset_reason);
    }

protected:
    void collectDiagnostics (JsonVariant &obj) const override {
        JsonObject system = obj ["system"].to <JsonObject> ();
            JsonObject code = system ["code"].to <JsonObject> ();
                code ["size"] = code_size;
            JsonObject heap = system ["heap"].to <JsonObject> ();
                heap ["size"] = heap_size;
                heap ["free"] = esp_get_free_heap_size ();
                heap ["min"] = esp_get_minimum_free_heap_size ();
            JsonObject reset = system ["reset"].to <JsonObject> ();
                reset ["okay"] = reset_okay;
                reset ["reason"] = reset_details.first;
                //reset ["details"] = reset_details.second;
    }
};

// -----------------------------------------------------------------------------------------------

class Program: public Component, public Diagnosticable {

    const Config config;

    FanInterfaceStrategy_motorMapWithRotation fanInterfaceSetrategy;
    PidController <double> fanControllingAlgorithm;
    AlphaSmoothing <double> fanSmoothingAlgorithm;

    TemperatureCalibrator temperatureCalibrator;
    TemperatureInterface temperatureInterface;
    TemperatureManagerBatterypack temperatureManagerBatterypack;
    TemperatureManagerEnvironment temperatureManagerEnvironment;
    FanInterface fanInterface;
    FanManager fanManager;

    DeviceManager devices;

    NetwerkManager network;
    NettimeManager nettime;
    DeliverManager deliver;
    PublishManager publish;
    StorageManager storage;

    ControlManager control;
    UpdateManager updater;
    AlarmInterface alarmsInterface;
    AlarmManager alarms;
    PlatformArduino platform;

    DiagnosticManager diagnostics;
    class OperationalManager {
        const Program* _program;
    public:
        explicit OperationalManager (const Program* program) : _program (program) {};
        void collect (JsonVariant &) const;
    } operational;

    Uptime uptime;
    ActivationTracker cycles;

    //

    String doCollect (const String& name, const std::function <void (JsonVariant &)> func) const {
        JsonCollector collector (name, getTimeString ());
        JsonVariant obj = collector.document ().as <JsonVariant> ();
        func (obj);
        return collector;
    }
    void dataCapture (const String& data) {
        class StorageLineHandler: public StorageManager::LineCallback {
            PublishManager& _publish;
        public:
            explicit StorageLineHandler (PublishManager& publish): _publish (publish) {}
            bool process (const String& line) override {
                return line.isEmpty () ? true : _publish.publish (line, "data");
            }
        };
        if (publish.available ()) {
            if (storage.size () > 0) {
                DEBUG_PRINTF ("Program::doCapture: publish.connected () && storage.size () > 0\n");
                // probably needs to be time bound and recall and reuse an offset ... or this will cause infinite watchdog loop one day
                StorageLineHandler handler (publish);
                if (storage.retrieve (handler))
                    storage.erase ();
            }
            if (!publish.publish (data, "data"))
                storage.append (data);
        } else
            storage.append (data);
    }

    Intervalable intervalDeliver, intervalCapture, intervalDiagnose;
    void process () override {

        const bool dataShouldDeliver = intervalDeliver, dataToDeliver = dataShouldDeliver && deliver.available ();
        const bool dataShouldCapture = intervalCapture, captureToPublish = dataShouldCapture && publish.available (), captureToStorage = dataShouldCapture && storage.available (), dataToCapture = captureToPublish || captureToStorage;
        const bool diagShould = intervalDiagnose && (config.deliverDiagnostics || config.publishDiagnostics), diagToDeliver = diagShould && (config.deliverDiagnostics && deliver.available ()), diagToPublish = diagShould && (config.publishDiagnostics && publish.available ());

        DEBUG_PRINTF ("Program::process: deliver=%d/%d, capture=%d/%d/%d, diagnose=%d/%d/%d\n", dataShouldDeliver, dataToDeliver, dataShouldCapture, captureToPublish, captureToStorage, diagShould, diagToDeliver, diagToPublish);

        if (dataToDeliver || dataToCapture) {
            const String data = doCollect ("data", [&] (JsonVariant& obj) { operational.collect (obj); });
            DEBUG_PRINTF ("Program::process: data, length=%d, content=<<<%s>>>\n", data.length (), data.c_str ());
            if (dataToDeliver) deliver.deliver (data);
            if (dataToCapture) dataCapture (data);
        }

        if (diagToDeliver || diagToPublish) {
            const String diag = doCollect ("diag", [&] (JsonVariant& obj) { diagnostics.collect (obj); });
            DEBUG_PRINTF ("Program::process: diag, length=%d, content=<<<%s>>>\n", diag.length (), diag.c_str ());
            if (diagToDeliver)
                deliver.deliver (diag);
            if (diagToPublish)
                publish.publish (diag, "diag");
        }

        cycles ++;
    }

public:
    Program () :
        fanControllingAlgorithm (config.FAN_CONTROL_P, config.FAN_CONTROL_I, config.FAN_CONTROL_D), fanSmoothingAlgorithm (config.FAN_SMOOTH_A),
        temperatureCalibrator (config.temperatureCalibrator),
        temperatureInterface (config.temperatureInterface, [&] (const int channel, const uint16_t resistance) { return temperatureCalibrator.calculateTemperature (channel, resistance); }),
        temperatureManagerBatterypack (config.temperatureManagerBatterypack, temperatureInterface), temperatureManagerEnvironment (config.temperatureManagerEnvironment, temperatureInterface),
        fanInterface (config.fanInterface, fanInterfaceSetrategy), fanManager (config.fanManager, fanInterface, fanControllingAlgorithm, fanSmoothingAlgorithm,
            [&] () { return FanManager::TargetSet (temperatureManagerBatterypack.setpoint (), temperatureManagerBatterypack.current ()); }),
        devices (config.devices, [&] () { return network.isAvailable (); }),
        network (config.network), nettime (config.nettime, [&] () { return network.isAvailable (); }),
        deliver (config.deliver, devices.blue ()), publish (config.publish, devices.mqtt (), [&] () { return network.isAvailable (); }), storage (config.storage),
        control (config.control, devices),
        updater (config.updater, [&] () { return network.isAvailable (); }),
        alarmsInterface (config.alarmsInterface), alarms (config.alarms, alarmsInterface, { &temperatureManagerEnvironment, &temperatureManagerBatterypack, &nettime, &deliver, &publish, &storage, &platform }),
        diagnostics (config.diagnostics, { &temperatureCalibrator, &temperatureInterface, &fanInterface, &temperatureManagerBatterypack, &temperatureManagerEnvironment, &fanManager, &devices, &network, &nettime, &deliver, &publish, &storage, &control, &updater, &alarms, &platform, this }),
        operational (this),
        intervalDeliver (config.intervalDeliver), intervalCapture (config.intervalCapture), intervalDiagnose (config.intervalDiagnose),
        components ({ &temperatureCalibrator, &temperatureInterface, &fanInterface, &temperatureManagerBatterypack, &temperatureManagerEnvironment, &fanManager, &alarms, &devices, &network, &nettime, &deliver, &publish, &storage, &control, &updater, &diagnostics, this }) {
        DEBUG_PRINTF ("Program::constructor: intervals - process=%lu, deliver=%lu, capture=%lu, diagnose=%lu\n", config.intervalProcess, config.intervalDeliver, config.intervalCapture, config.intervalDiagnose);
    };

    Component::List components;
    void setup () { for (const auto& component : components) component->begin (); }
    void loop () { for (const auto& component : components) component->process (); }
    void sleep () { delay (config.intervalProcess); }

protected:
    void collectDiagnostics (JsonVariant &obj) const override {
        JsonObject program = obj ["program"].to <JsonObject> ();
        extern const String build_info;
        program ["build"] = build_info;
        program ["uptime"] = uptime;
        program ["cycles"] = cycles;
    }
};

// -----------------------------------------------------------------------------------------------

void Program::OperationalManager::collect (JsonVariant &obj) const {
    JsonObject tmp = obj ["tmp"].to <JsonObject> ();
        tmp ["env"] = _program->temperatureManagerEnvironment.getTemperature ();
    JsonObject bat = tmp ["bat"].to <JsonObject> ();
        bat ["avg"] = _program->temperatureManagerBatterypack.avg ();
        bat ["min"] = _program->temperatureManagerBatterypack.min ();
        bat ["max"] = _program->temperatureManagerBatterypack.max ();
        JsonArray val = bat ["val"].to <JsonArray> ();
        for (const auto& v : _program->temperatureManagerBatterypack.getTemperatures ())
            val.add (v);
    obj ["fan"] = _program->fanInterface.getSpeed ();
    obj ["alm"] = _program->alarms.toString ();
}

// -----------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------
