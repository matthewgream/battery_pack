
// -----------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------

#define DEFAULT_SERIAL_BAUD 115200
#define DEFAULT_WATCHDOG_SECS 60

#define DEFAULT_NAME "BatteryMonitor"
#define DEFAULT_VERS "0.9.9"

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
#include "UtilitiesOTA.hpp" // breaks if included earlier

static inline constexpr size_t HARDWARE_TEMP_SIZE = TemperatureInterface::CHANNELS;
static inline constexpr float HARDWARE_TEMP_START = 5.0f, HARDWARE_TEMP_END = 60.0f, HARDWARE_TEMP_STEP = 0.5f;
using TemperatureManagerBatterypack = TemperatureManagerBatterypackTemplate <HARDWARE_TEMP_SIZE - 1>;
using TemperatureManagerEnvironment = TemperatureManagerEnvironmentTemplate <1>;
using TemperatureCalibrator = TemperatureCalibrationManager <HARDWARE_TEMP_SIZE, HARDWARE_TEMP_START, HARDWARE_TEMP_END, HARDWARE_TEMP_STEP>;

static inline constexpr double FAN_CONTROL_P = 10.0, FAN_CONTROL_I = 0.1, FAN_CONTROL_D = 1.0, FAN_SMOOTH_A = 0.1;

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
    void collectDiagnostics (JsonDocument &obj) const override {
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
            OperationalManager (const Program* program) : _program (program) {};
            void collect (JsonDocument &doc) const;
    } operational;

    Uptime uptime;
    ActivationTracker cycles;

    //

    using JsonDocumentFunc = std::function <void (JsonDocument& doc)>;
    String doCollect (const String& name, const JsonDocumentFunc func) const {
        JsonCollector collector (name, getTimeString ());
        func (collector.document ());
        return collector;
    }
    void doDeliver (const String& data) {
        deliver.deliver (data);
    }
    void doCapture (const String& data) {
        class StorageLineHandler: public StorageManager::LineCallback {
            PublishManager& _publish;
        public:
            StorageLineHandler (PublishManager& publish): _publish (publish) {}
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
        const bool deliver = intervalDeliver, capture = intervalCapture, diagnose = intervalDiagnose;
        DEBUG_PRINTF ("\nProgram::process: deliver=%d, capture=%d, diagnose=%d\n", deliver, capture, diagnose);
        if (deliver || capture) {
            const String data = doCollect ("data", [&] (JsonDocument& doc) { operational.collect (doc); });
            if (deliver) doDeliver (data);
            if (capture) doCapture (data);
            DEBUG_PRINTF ("Program::process: data, length=%d, content=<<<%s>>>\n", data.length (), data.c_str ());
        }
        if (diagnose) {
            const String diag = doCollect ("diag", [&] (JsonDocument& doc) { diagnostics.collect (doc); });
            doDeliver (diag);
            DEBUG_PRINTF ("Program::process: diag, length=%d, content=<<<%s>>>\n", diag.length (), diag.c_str ());
#ifdef DEBUG
            if (publish.connected ())
                publish.publish (diag, "diag");
#endif
        }
        cycles ++;

        // const String x = doCollect ("diag", [&] (JsonDocument& doc) { diagnostics.collect (doc); });
        // DEBUG_PRINTF ("Program::_debug_: diag, length=%d, content=<<<%s>>>\n", x.length (), x.c_str ());
        // JsonSplitter splitter (512, { "type", "time" });
        // splitter.splitJson (x, [&] (const String& part, const int elements) {
        //     DEBUG_PRINTF ("Program::_debug_: length=%u, part=%u, elements=%d\n", x.length (), part.length (), elements);
        //     DEBUG_PRINTF ("--> %s\n", part.c_str ());
        // });

    }

public:
    Program () :
        fanControllingAlgorithm (FAN_CONTROL_P, FAN_CONTROL_I, FAN_CONTROL_D), fanSmoothingAlgorithm (FAN_SMOOTH_A),
        temperatureCalibrator (config.temperatureCalibrator),
        temperatureInterface (config.temperatureInterface, [&] (const int channel, const uint16_t resistance) { return temperatureCalibrator.calculateTemperature (channel, resistance); }),
        temperatureManagerBatterypack (config.temperatureManagerBatterypack, temperatureInterface), temperatureManagerEnvironment (config.temperatureManagerEnvironment, temperatureInterface),
        fanInterface (config.fanInterface, fanInterfaceSetrategy), fanManager (config.fanManager, fanInterface, fanControllingAlgorithm, fanSmoothingAlgorithm, 
            [&] () { return FanManager::TargetSet (temperatureManagerBatterypack.setpoint (), temperatureManagerBatterypack.current ()); }),
        network (config.network), nettime (config.nettime, [&] () { return network.isAvailable (); }),
        deliver (config.deliver), publish (config.publish, [&] () { return network.isAvailable (); }), storage (config.storage),
        updater (config.updateManager, [&] () { return network.isAvailable (); }),
        alarmInterface (config.alarmInterface), alarms (config.alarmManager, alarmInterface, { &temperatureManagerEnvironment, &temperatureManagerBatterypack, &nettime, &deliver, &publish, &storage, &platform }),
        diagnostics (config.diagnosticManager, { &temperatureInterface, &fanInterface, &temperatureManagerBatterypack, &temperatureManagerEnvironment, &network, &nettime, &deliver, &publish, &storage, &updater, &alarms, &platform, this }),
        operational (this),
        intervalDeliver (config.intervalDeliver), intervalCapture (config.intervalCapture), intervalDiagnose (config.intervalDiagnose),
        components ({ &temperatureCalibrator, &temperatureInterface, &fanInterface, &temperatureManagerBatterypack, &temperatureManagerEnvironment, &fanManager,
            &alarms, &network, &nettime, &deliver, &publish, &storage, &updater, &diagnostics, this }) {
        DEBUG_PRINTF ("Program::constructor: intervals - process=%lu, deliver=%lu, capture=%lu, diagnose=%lu\n", config.intervalProcess, config.intervalDeliver, config.intervalCapture, config.intervalDiagnose);
    };

    Component::List components;
    void setup () { for (const auto& component : components) component->begin (); }
    void loop () { for (const auto& component : components) component->process (); }
    void sleep () { delay (config.intervalProcess); }

protected:
    void collectDiagnostics (JsonDocument &obj) const override {
        JsonObject program = obj ["program"].to <JsonObject> ();
        extern const String build_info;
        program ["build"] = build_info;
        program ["uptime"] = uptime.seconds ();
        cycles.serialize (program ["cycles"].to <JsonObject> ());
    }
};

// -----------------------------------------------------------------------------------------------

void Program::OperationalManager::collect (JsonDocument &doc) const {
    JsonObject tmp = doc ["tmp"].to <JsonObject> ();
    tmp ["env"] = _program->temperatureManagerEnvironment.getTemperature ();
    JsonObject bat = tmp ["bat"].to <JsonObject> ();
    bat ["avg"] = _program->temperatureManagerBatterypack.avg ();
    bat ["min"] = _program->temperatureManagerBatterypack.min ();
    bat ["max"] = _program->temperatureManagerBatterypack.max ();
    JsonArray val = bat ["val"].to <JsonArray> ();
    for (const auto& v : _program->temperatureManagerBatterypack.getTemperatures ())
        val.add (v);
    doc ["fan"] = _program->fanInterface.getSpeed ();
    doc ["alm"] = _program->alarms.getAlarmsAsString ();
}

// -----------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------
