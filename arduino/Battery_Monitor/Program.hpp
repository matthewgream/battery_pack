
/*
    https://www.waveshare.com/esp32-c3-zero.htm
    https://www.waveshare.com/wiki/ESP32-C3-Zero
    https://github.com/mikedotalmond/Arduino-MuxInterface-CD74HC4067
    https://github.com/ugurakas/Esp32-C3-LP-Project
    https://github.com/ClaudeMarais/ContinousAnalogRead_ESP32-C3
    https://docs.espressif.com/projects/esp-idf/en/release-v4.4/esp32/api-reference/peripherals/adc.html
    https://github.com/khoih-prog/ESP32_FastPWM
*/

// -----------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------

#include <Arduino.h>
#include <WiFi.h>

#include <ArduinoJson.h>

#include <ctime>
#include <array>
#include <vector>

#include "Utility.hpp"
#include "Helpers.hpp"
#include "Config.hpp"

// -----------------------------------------------------------------------------------------------

#include "ProgramFoundations.hpp"
#include "ProgramHardwareInterface.hpp"
#include "ProgramHardwareManage.hpp"
#include "ProgramNetworkManage.hpp"
#include "ProgramDataManage.hpp"

// -----------------------------------------------------------------------------------------------

class Program;
class OperationalManager {
    const Program& _program;
    public:
        OperationalManager (const Program& program) : _program (program) {};
        void collect (JsonDocument &doc) const;
};

// -----------------------------------------------------------------------------------------------

class Program : public Component, public Diagnosticable {
    friend class OperationalManager;
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

    AlarmManager alarms;
    DiagnosticManager diagnostics;
    OperationalManager operational;

    Uptime uptime;
    ActivationTracker activations;

    //

    template <typename Func>
    String doCollect (const String& name, Func func) const {
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
                DEBUG_PRINTLN ("Program::doCapture: publish.connected () && storage.size () > 0");
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
        const bool deliver = intervalDeliver, capture = intervalDeliver, diagnose = intervalDiagnose;
        DEBUG_PRINT ("Program::process: deliver=");
        DEBUG_PRINT (deliver);
        DEBUG_PRINT (", capture=");
        DEBUG_PRINT (capture);
        DEBUG_PRINT (", diagnose=");
        DEBUG_PRINTLN (diagnose);
        if (deliver || capture) {
            const String data = doCollect ("data", [this] (JsonDocument& doc) { operational.collect (doc); });
            if (deliver) doDeliver (data);
            if (capture) doCapture (data);
            DEBUG_PRINT ("Program::process: data, length=");
            DEBUG_PRINT (data.length ());
            DEBUG_PRINTLN (", content=<<<");
            DEBUG_PRINTLN (data);
            DEBUG_PRINTLN ("<<<");
        }
        if (diagnose) {
            const String diag = doCollect ("diag", [this] (JsonDocument& doc) { diagnostics.collect (doc); });
            doDeliver (diag);
            DEBUG_PRINT ("Program::process: diag, length=");
            DEBUG_PRINT (diag.length ());
            DEBUG_PRINTLN (", content=<<<");
            DEBUG_PRINTLN (diag);
            DEBUG_PRINTLN ("<<<");
        }
        activations ++;
    }

public:
    Program () :
        fanControllingAlgorithm (10.0f, 0.1f, 1.0f), fanSmoothingAlgorithm (0.1f),
        temperatureInterface (config.temperature), temperatureManagerBatterypack (config.temperature, temperatureInterface), temperatureManagerEnvironment (config.temperature, temperatureInterface),
        fanInterface (config.fan), fanManager (config.fan, fanInterface, temperatureManagerBatterypack, fanControllingAlgorithm, fanSmoothingAlgorithm),
        network (config.network), nettime (config.nettime),
        deliver (config.deliver), publish (config.publish), storage (config.storage),
        alarms (config.alarm, { &temperatureManagerEnvironment, &temperatureManagerBatterypack, &nettime, &publish, &storage }),
        diagnostics (config.diagnostic, { &temperatureInterface, &fanInterface, &network, &nettime, &deliver, &publish, &storage, &alarms, this }), operational (*this),
        intervalDeliver (config.intervalDeliver), intervalCapture (config.intervalCapture), intervalDiagnose (config.intervalDiagnose),
        components ({ &temperatureInterface, &fanInterface, &temperatureManagerBatterypack, &temperatureManagerEnvironment, &fanManager,
            &alarms, &network, &nettime, &deliver, &publish, &storage, &diagnostics, this }) {
        DEBUG_PRINT ("Program::constructor: intervals - process=");
        DEBUG_PRINT (config.intervalProcess);
        DEBUG_PRINT (", deliver=");
        DEBUG_PRINT (config.intervalDeliver);
        DEBUG_PRINT (", capture=");
        DEBUG_PRINT (config.intervalCapture);
        DEBUG_PRINT (", diagnose=");
        DEBUG_PRINTLN (config.intervalDiagnose);
    };

    Component::List components;
    void setup () { for (const auto& component : components) component->begin (); }
    void loop () { for (const auto& component : components) component->process (); }
    void sleep () { delay (config.intervalProcess); }

protected:
    void collectDiagnostics (JsonObject &obj) const override {
        JsonObject program = obj ["program"].to <JsonObject> ();
        extern const String build_info; 
        program ["build"] = build_info;
        program ["uptime"] = uptime.seconds ();
        activations.serialize (program ["iterations"].to <JsonObject> ());
    }
};

// -----------------------------------------------------------------------------------------------

void OperationalManager::collect (JsonDocument &doc) const {
    JsonObject temperatures = doc ["temperatures"].to <JsonObject> ();
    temperatures ["environment"] = _program.temperatureManagerEnvironment.getTemperature ();
    JsonObject batterypack = temperatures ["batterypack"].to <JsonObject> ();
    batterypack ["avg"] = _program.temperatureManagerBatterypack.avg ();
    batterypack ["min"] = _program.temperatureManagerBatterypack.min ();
    batterypack ["max"] = _program.temperatureManagerBatterypack.max ();
    JsonArray values = batterypack ["values"].to <JsonArray> ();
    for (const auto& temperature : _program.temperatureManagerBatterypack.getTemperatures ())
        values.add (temperature);
    JsonObject fan = doc ["fan"].to <JsonObject> ();
    fan ["fanspeed"] = _program.fanInterface.getSpeed ();
    doc ["alarms"] = _program.alarms.getAlarms ();
}

// -----------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------
