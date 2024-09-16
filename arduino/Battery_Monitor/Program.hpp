
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

#include "Debug.hpp"
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

    AlarmManager alarms;
    DiagnosticManager diagnostics;

    Uptime uptime;
    Upstamp iterations;

    //

    String diagCollect () {
        JsonDocument doc;
        doc ["type"] = "diag", doc ["time"] = nettime.getTimeString ();

        diagnostics.collect (doc);

        String output;
        serializeJson (doc, output);
        return output;
    }

    //

    String dataCollect () {
        JsonDocument doc;
        doc ["type"] = "data", doc ["time"] = nettime.getTimeString ();

        JsonObject temperatures = doc ["temperatures"].to <JsonObject> ();
        temperatures ["environment"] = temperatureManagerEnvironment.getTemperature ();
        JsonObject batterypack = temperatures ["batterypack"].to <JsonObject> ();
        batterypack ["avg"] = temperatureManagerBatterypack.avg ();
        batterypack ["min"] = temperatureManagerBatterypack.min ();
        batterypack ["max"] = temperatureManagerBatterypack.max ();
        JsonArray values = batterypack ["values"].to <JsonArray> ();
        for (const auto& temperature : temperatureManagerBatterypack.getTemperatures ())
            values.add (temperature);
        JsonObject fan = doc ["fan"].to <JsonObject> ();
        fan ["fanspeed"] = fanInterface.getSpeed ();
        doc ["alarms"] = alarms.getAlarms ();

        String output;
        serializeJson (doc, output);
        return output;
    }

    //

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
                StorageLineHandler handler (publish);
                if (storage.retrieve (handler))
                    storage.erase ();
            }
            if (!publish.publish (data))
              storage.append (data);
        } else
            storage.append (data);
    }

    //

    void collect (JsonObject &obj) const override {
        JsonObject program = obj ["program"].to <JsonObject> ();
        program ["uptime"] = uptime.seconds ();
        program ["iterations"] = iterations.number ();
    }

    //

    Intervalable intervalDeliver, intervalCapture, intervalDiagnose;
    void process () override {
        const bool deliver = intervalDeliver, capture = intervalDeliver, diagnose = intervalDiagnose;
        if (deliver || capture) {
          const String data = dataCollect ();
          if (deliver) doDeliver (data);
          if (capture) doCapture (data);
        }
        if (diagnose)
            doDeliver (diagCollect ());
        iterations ++;
    }

public:
    Program () :
        fanControllingAlgorithm (10.0f, 0.1f, 1.0f), fanSmoothingAlgorithm (0.1f),
        temperatureInterface (config.temperature), temperatureManagerBatterypack (config.temperature, temperatureInterface), temperatureManagerEnvironment (config.temperature, temperatureInterface),
        fanInterface (config.fan), fanManager (config.fan, fanInterface, temperatureManagerBatterypack, fanControllingAlgorithm, fanSmoothingAlgorithm),
        network (config.network), nettime (config.nettime),
        deliver (config.deliver), publish (config.publish), storage (config.storage),
        alarms (config.alarm, { &temperatureManagerEnvironment, &temperatureManagerBatterypack, &storage, &nettime }),
        diagnostics (config.diagnostic, { &temperatureInterface, &fanInterface, &network, &nettime, &deliver, &publish, &storage, &alarms, this }),
        intervalDeliver (config.intervalDeliver), intervalCapture (config.intervalCapture), intervalDiagnose (config.intervalDiagnose),
        components ({ &temperatureInterface, &fanInterface, &temperatureManagerBatterypack, &temperatureManagerEnvironment, &fanManager,
            &alarms, &network, &nettime, &deliver, &publish, &storage, &diagnostics, this }) {
    };

    Component::List components;
    void setup () { for (const auto& component : components) component->begin (); }
    void loop () { for (const auto& component : components) component->process (); }
    void sleep () { delay (config.intervalProcess); }
};

// -----------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------
