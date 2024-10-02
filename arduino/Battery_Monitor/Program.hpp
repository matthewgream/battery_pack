
// -----------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------

#include <Arduino.h>

#include "Utilities.hpp"
#include "UtilitiesJson.hpp"
#include "UtilitiesArduino.hpp"

#include "Components.hpp"
#include "ComponentsHardware.hpp"
#include "ComponentsBluetooth.hpp"

// -----------------------------------------------------------------------------------------------

#include "ProgramBase.hpp"
#include "ProgramHardwareInterface.hpp"
#include "ProgramHardwareManage.hpp"
#include "ProgramNetworkManage.hpp"
#include "ProgramDataManage.hpp"

#include "Config.hpp"

// -----------------------------------------------------------------------------------------------

class Program : public Component, public Diagnosticable {

    const Config config;

    PidController fanControllingAlgorithm;
    AlphaSmoothing fanSmoothingAlgorithm;

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

    AlarmInterface_SinglePIN alarmInterface;
    AlarmManager alarms;
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

    String doCollect (const String& name, const std::function<void (JsonDocument& doc)> func) const {
        JsonCollector collector (name, nettime);
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
                return line.isEmpty () ? true : _publish.publish (line);
            }
        };
        if (publish.connected ()) {
            if (storage.size () > 0) {
                DEBUG_PRINTF ("Program::doCapture: publish.connected () && storage.size () > 0\n");
                StorageLineHandler handler (publish);
                if (storage.retrieve (handler))
                    storage.erase ();
            }
            if (!publish.publish (data))
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
                publish.publish (diag);
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

    static inline constexpr float FAN_CONTROL_P = 10.0, FAN_CONTROL_I = 0.1, FAN_CONTROL_D = 1.0, FAN_SMOOTH_A = 0.1;
public:
    Program () :
        fanControllingAlgorithm (FAN_CONTROL_P, FAN_CONTROL_I, FAN_CONTROL_D), fanSmoothingAlgorithm (FAN_SMOOTH_A),
        temperatureInterface (config.temperatureInterface), temperatureManagerBatterypack (config.temperatureManager, temperatureInterface), temperatureManagerEnvironment (config.temperatureManager, temperatureInterface),
        fanInterface (config.fanInterface), fanManager (config.fanManager, fanInterface, temperatureManagerBatterypack, fanControllingAlgorithm, fanSmoothingAlgorithm),
        network (config.network), nettime (config.nettime, network),
        deliver (config.deliver), publish (config.publish, network), storage (config.storage),
        alarmInterface (config.alarmInterface), alarms (config.alarmManager, alarmInterface, { &temperatureManagerEnvironment, &temperatureManagerBatterypack, &nettime, &deliver, &publish, &storage }),
        diagnostics (config.diagnosticManager, { &temperatureInterface, &fanInterface, &network, &nettime, &deliver, &publish, &storage, &alarms, this }), operational (this),
        intervalDeliver (config.intervalDeliver), intervalCapture (config.intervalCapture), intervalDiagnose (config.intervalDiagnose),
        components ({ &temperatureInterface, &fanInterface, &temperatureManagerBatterypack, &temperatureManagerEnvironment, &fanManager,
            &alarms, &network, &nettime, &deliver, &publish, &storage, &diagnostics, this }) {
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
