
// -----------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------

#define DEFAULT_SERIAL_BAUD 115200
#define DEFAULT_WATCHDOG_SECS 60

#define DEFAULT_NAME "BatteryMonitor"
#define DEFAULT_VERS "1.0.2"
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

#include "ProgramBase.hpp"
#include "ProgramHardwareInterface.hpp"
#include "ProgramHardwareManage.hpp"
#include "ProgramNetworkManage.hpp"
#include "ProgramDataManage.hpp"
#include "ProgramTemperatureCalibration.hpp"
#include "UtilitiesOTA.hpp" // breaks if included earlier due to SPIFFS headers

static inline constexpr size_t HARDWARE_TEMP_SIZE = TemperatureInterface::CHANNELS;
static inline constexpr float HARDWARE_TEMP_START = 5.0f, HARDWARE_TEMP_END = 60.0f, HARDWARE_TEMP_STEP = 0.5f;
using TemperatureManagerBatterypack = TemperatureManagerBatterypackTemplate <HARDWARE_TEMP_SIZE - 1>;
using TemperatureManagerEnvironment = TemperatureManagerEnvironmentTemplate <1>;
using TemperatureCalibrator = TemperatureCalibrationManager <HARDWARE_TEMP_SIZE, HARDWARE_TEMP_START, HARDWARE_TEMP_END, HARDWARE_TEMP_STEP>;

static inline constexpr double HARDWARE_FAN_CONTROL_P = 10.0, HARDWARE_FAN_CONTROL_I = 0.1, HARDWARE_FAN_CONTROL_D = 1.0, HARDWARE_FAN_SMOOTH_A = 0.1;

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
                reset ["details"] = reset_details.second;
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

    ConnectManager network;
    NettimeManager nettime;
    DeliverManager deliver;
    PublishManager publish;
    StorageManager storage;

    UpdateManager updater;
    AlarmInterface_SinglePIN alarmInterface;
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
    void doDeliver (const String& data) {
        deliver.deliver (data);
    }
    void doCapture (const String& data) {
        class StorageLineHandler: public StorageManager::LineCallback {
            PublishManager& _publish;
        public:
            explicit StorageLineHandler (PublishManager& publish): _publish (publish) {}
            bool process (const String& line) override {
                return line.isEmpty () ? true : _publish.publish (line, "data");
            }
        };
        if (publish.connected ()) {
            if (storage.size () > 0) {
                DEBUG_PRINTF ("Program::doCapture: publish.connected () && storage.size () > 0\n");
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
        const bool deliverTo = intervalDeliver, captureTo = intervalCapture, diagnoseTo = intervalDiagnose;
        DEBUG_PRINTF ("\nProgram::process: deliver=%d, capture=%d, diagnose=%d\n", deliverTo, captureTo, diagnoseTo);
        if (deliverTo || captureTo) {
            const String data = doCollect ("data", [&] (JsonVariant& obj) { operational.collect (obj); });
            if (deliverTo) doDeliver (data);
            if (captureTo) doCapture (data);
            DEBUG_PRINTF ("Program::process: data, length=%d, content=<<<%s>>>\n", data.length (), data.c_str ());
        }
        if (diagnoseTo) {
            const String diag = doCollect ("diag", [&] (JsonVariant& obj) { diagnostics.collect (obj); });
            doDeliver (diag);
            DEBUG_PRINTF ("Program::process: diag, length=%d, content=<<<%s>>>\n", diag.length (), diag.c_str ());
#ifdef DEBUG
            if (publish.connected ())
                publish.publish (diag, "diag");
#endif
        }
        cycles ++;
    }

public:
    Program () :
        fanControllingAlgorithm (HARDWARE_FAN_CONTROL_P, HARDWARE_FAN_CONTROL_I, HARDWARE_FAN_CONTROL_D), fanSmoothingAlgorithm (HARDWARE_FAN_SMOOTH_A),
        temperatureCalibrator (config.temperatureCalibrator),
        temperatureInterface (config.temperatureInterface, [&] (const int channel, const uint16_t resistance) { return temperatureCalibrator.calculateTemperature (channel, resistance); }),
        temperatureManagerBatterypack (config.temperatureManagerBatterypack, temperatureInterface), temperatureManagerEnvironment (config.temperatureManagerEnvironment, temperatureInterface),
        fanInterface (config.fanInterface, fanInterfaceSetrategy), fanManager (config.fanManager, fanInterface, fanControllingAlgorithm, fanSmoothingAlgorithm,
            [&] () { return FanManager::TargetSet (temperatureManagerBatterypack.setpoint (), temperatureManagerBatterypack.current ()); }),
        network (config.network), nettime (config.nettime, [&] () { return network.isAvailable (); }),
        deliver (config.deliver), publish (config.publish, [&] () { return network.isAvailable (); }), storage (config.storage),
        updater (config.updateManager, [&] () { return network.isAvailable (); }),
        alarmInterface (config.alarmInterface), alarms (config.alarmManager, alarmInterface, { &temperatureManagerEnvironment, &temperatureManagerBatterypack, &nettime, &deliver, &publish, &storage, &platform }),
        diagnostics (config.diagnosticManager, { &temperatureInterface, &fanInterface, &temperatureManagerBatterypack, &temperatureManagerEnvironment, &fanManager, &network, &nettime, &deliver, &publish, &storage, &updater, &alarms, &platform, this }),
        operational (this),
        intervalDeliver (config.intervalDeliver), intervalCapture (config.intervalCapture), intervalDiagnose (config.intervalDiagnose),
        components ({ &temperatureCalibrator, &temperatureInterface, &fanInterface, &temperatureManagerBatterypack, &temperatureManagerEnvironment, &fanManager, &alarms, &network, &nettime, &deliver, &publish, &storage, &updater, &diagnostics, this }) {
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
