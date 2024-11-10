
// -----------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------

#undef HARDWARE_ESP32_C3_ZERO
#define HARDWARE_ESP32_S3_YD_ESP32_S3_C

// as matches build tools (not so great)
#if defined(HARDWARE_ESP32_C3_ZERO)
#define HARDWARE_VARIANT_PLATFORM "c3zero-esp32"
#elif defined(HARDWARE_ESP32_S3_YD_ESP32_S3_C)
#define HARDWARE_VARIANT_PLATFORM "s3devkitc1-esp32"
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
#define DEFAULT_VERS "1.3.5"
#define DEFAULT_TYPE "batterymonitor-" HARDWARE_VARIANT_PLATFORM
#define DEFAULT_JSON "http://ota.local:8090/images/images.json"

// -----------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------

#include <Arduino.h>

#include "Utilities.hpp"
#include "UtilitiesJson.hpp"
#include "UtilitiesPlatform.hpp"

#include "Components.hpp"
#include "ComponentsHardware.hpp"
#include "ComponentsDevices.hpp"
#include "ComponentsDevicesBluetooth.hpp"

// -----------------------------------------------------------------------------------------------

using AlarmInterface = ActivablePIN;

#include "ProgramBase.hpp"
#include "ProgramManage.hpp"
#include "ProgramHardwareInterface.hpp"
#include "ProgramHardwareManage.hpp"
#include "ProgramNetworkManage.hpp"
#include "ComponentsHardwareDalyBMS.hpp" // XXX
#include "ProgramDataManage.hpp"
#include "ProgramTemperatureCalibration.hpp"
#include "UtilitiesOTA.hpp"    // breaks if included earlier due to SPIFFS headers

static inline constexpr size_t HARDWARE_TEMP_SIZE = TemperatureInterface::CHANNELS;
static inline constexpr float HARDWARE_TEMP_START = 5.0f, HARDWARE_TEMP_END = 60.0f, HARDWARE_TEMP_STEP = 0.5f;
using TemperatureManagerBatterypack = TemperatureManagerBatterypackTemplate<HARDWARE_TEMP_SIZE - 1>;
using TemperatureManagerEnvironment = TemperatureManagerEnvironmentTemplate<1>;
using TemperatureCalibrator = TemperatureCalibrationManager<HARDWARE_TEMP_SIZE, HARDWARE_TEMP_START, HARDWARE_TEMP_END, HARDWARE_TEMP_STEP>;

// -----------------------------------------------------------------------------------------------

#include "driver/temperature_sensor.h"

class PlatformArduino : public Alarmable, public Diagnosticable {

public:
    struct Config {
        int pinRandomNoise;
    };

private:
    const Config& config;

    static constexpr int HEAP_FREE_PERCENTAGE_MINIMUM = 5;
    static constexpr int TEMP_RANGE_MINIMUM = 0, TEMP_RANGE_MAXIMUM = 80;
    const unsigned long code_size, heap_size;
    temperature_sensor_handle_t temp_handle;
    bool temp_okay;
    const int reset_reason;
    const std::pair<String, String> reset_details;
    const bool reset_okay;

public:
    PlatformArduino(const Config& conf)
        : Alarmable({ AlarmCondition(ALARM_SYSTEM_MEMORYLOW, [this]() {
                          return ((100 * esp_get_minimum_free_heap_size()) / heap_size) < HEAP_FREE_PERCENTAGE_MINIMUM;
                      }),
                      AlarmCondition(ALARM_SYSTEM_BADRESET, [this]() {
                          return !reset_okay;
                      }) }), config (conf),
          code_size(ESP.getSketchSize()), heap_size(ESP.getHeapSize()), reset_reason(getResetReason()), reset_details(getResetDetails(reset_reason)), reset_okay(getResetOkay(reset_reason)) {
        const temperature_sensor_config_t temp_sensor = TEMPERATURE_SENSOR_CONFIG_DEFAULT(TEMP_RANGE_MINIMUM, TEMP_RANGE_MAXIMUM);
        float temp_read = 0.0f;
        temp_okay = (temperature_sensor_install(&temp_sensor, &temp_handle) == ESP_OK && temperature_sensor_enable(temp_handle) == ESP_OK && temperature_sensor_get_celsius(temp_handle, &temp_read) == ESP_OK);
        if (!temp_okay) DEBUG_PRINTF("PlatformArduino::init: could not enable temperature sensor\n");
        DEBUG_PRINTF("PlatformArduino::init: code=%lu, heap=%lu, temp=%.2f, reset=%d\n", code_size, heap_size, temp_read, reset_reason);
        RandomNumber::seed (analogRead (config.pinRandomNoise)); // XXX TIDY
    }

protected:
    void collectDiagnostics(JsonVariant& obj) const override {
        JsonObject system = obj["system"].to<JsonObject>();
        JsonObject code = system["code"].to<JsonObject>();
        code["size"] = code_size;
        JsonObject heap = system["heap"].to<JsonObject>();
        heap["size"] = heap_size;
        heap["free"] = esp_get_free_heap_size();
        heap["min"] = esp_get_minimum_free_heap_size();
        float temp;
        if (temp_okay && temperature_sensor_get_celsius(temp_handle, &temp) == ESP_OK)
            system["temp"] = temp;
        JsonObject reset = system["reset"].to<JsonObject>();
        reset["okay"] = reset_okay;
        reset["reason"] = reset_details.first;
        //reset ["details"] = reset_details.second;
    }
};

// -----------------------------------------------------------------------------------------------

#include "Config.hpp"

// -----------------------------------------------------------------------------------------------

class Program : public Component, public Diagnosticable {

    const Config config;
    const String address;

    PlatformArduino platform;

    FanInterfaceStrategy_motorMapWithRotation fanInterfaceSetrategy;
    PidController<double> fanControllingAlgorithm;
    AlphaSmoothing<double> fanSmoothingAlgorithm;

    TemperatureCalibrator temperatureCalibrator;
    TemperatureInterface temperatureInterface;
    TemperatureManagerBatterypack temperatureManagerBatterypack;
    TemperatureManagerEnvironment temperatureManagerEnvironment;
    FanInterface fanInterface;
    FanManager fanManager;

    DalyBMSManager batteryManager;

    DeviceManager devices;

    NetwerkManager network;
    NettimeManager nettime;
    ControlManager control;    //
    DeliverManager deliver;    //
    PublishManager publish;    //
    StorageManager storage;    //

    UpdateManager updater;
    AlarmInterface alarmsInterface;
    AlarmManager alarms;

    Uptime uptime;
    ActivationTracker cycles;

    //

    DiagnosticManager diagnostics;
    class OperationalManager {
        const Program* _program;
    public:
        explicit OperationalManager(const Program* program)
            : _program(program){};
        void collect(JsonVariant&) const;
    } operational;
    Intervalable intervalDeliver, intervalCapture, intervalDiagnose;
    String dataCollect(const String& name, const std::function<void(JsonVariant&)> func) const {
        JsonCollector collector(name, getTimeString(), address);
        JsonVariant obj = collector.document().as<JsonVariant>();
        func(obj);
        return collector;
    }
    void dataCapture(const String& data, const bool publishData, const bool storageData) {
        class StorageLineHandler : public StorageManager::LineCallback {
            PublishManager& _publish;
        public:
            explicit StorageLineHandler(PublishManager& publish)
                : _publish(publish) {}
            bool process(const String& line) override {
                return line.isEmpty() ? true : _publish.publish(line, "data");
            }
        };
        if (publishData) {
            if (storage.available() && storage.size() > 0) {    // even if !storageData, still drain it
                DEBUG_PRINTF("Program::doCapture: publish.connected () && storage.size () > 0\n");
                // probably needs to be time bound and recall and reuse an offset ... or this will cause infinite watchdog loop one day
                StorageLineHandler handler(publish);
                if (storage.retrieve(handler))
                    storage.erase();
            }
            if (!publish.publish(data, "data"))
                if (storageData)
                    storage.append(data);
        } else if (storageData)
            storage.append(data);
    }
    void dataProcess() {

        const bool dataShouldDeliver = intervalDeliver, dataToDeliver = dataShouldDeliver && deliver.available();
        const bool dataShouldCapture = intervalCapture, dataToCaptureToPublish = dataShouldCapture && (config.publishData && publish.available()), dataToCaptureToStorage = dataShouldCapture && (config.storageData && storage.available());
        const bool diagShould = intervalDiagnose && (config.deliverDiag || config.publishDiag), diagToDeliver = diagShould && (config.deliverDiag && deliver.available()), diagToPublish = diagShould && (config.publishDiag && publish.available());

        DEBUG_PRINTF("Program::process: deliver=%d/%d, capture=%d/%d/%d, diagnose=%d/%d/%d\n", dataShouldDeliver, dataToDeliver, dataShouldCapture, dataToCaptureToPublish, dataToCaptureToStorage, diagShould, diagToDeliver, diagToPublish);

        if (dataToDeliver || (dataToCaptureToPublish || dataToCaptureToStorage)) {
            const String data = dataCollect("data", [&](JsonVariant& obj) {
                operational.collect(obj);
            });
            DEBUG_PRINTF("Program::process: data, length=%d, content=<<<%s>>>\n", data.length(), data.c_str());
            if (dataToDeliver) deliver.deliver(data, "data", config.publishData && publish.available());
            if (dataToCaptureToPublish || dataToCaptureToStorage) dataCapture(data, dataToCaptureToPublish, dataToCaptureToStorage);
        }

        if (diagToDeliver || diagToPublish) {
            const String diag = dataCollect("diag", [&](JsonVariant& obj) {
                diagnostics.collect(obj);
            });
            DEBUG_PRINTF("Program::process: diag, length=%d, content=<<<%s>>>\n", diag.length(), diag.c_str());
            if (diagToDeliver)
                deliver.deliver(diag, "diag", config.publishData && publish.available());
            if (diagToPublish)
                publish.publish(diag, "diag");
        }
    }

    //

    void process() override {
        dataProcess();
        cycles++;
    }

    Intervalable intervalProcess;
    void gate() {
        intervalProcess.wait();
    }

public:
    Program()
        : address(getMacAddressBase("")),
          platform(config.platform),
          fanControllingAlgorithm(config.FAN_CONTROL_P, config.FAN_CONTROL_I, config.FAN_CONTROL_D),
          fanSmoothingAlgorithm(config.FAN_SMOOTH_A),
          temperatureCalibrator(config.temperatureCalibrator),
          temperatureInterface(config.temperatureInterface, [&](const int channel, const uint16_t resistance) {
              return temperatureCalibrator.calculateTemperature(channel, resistance);
          }),
          temperatureManagerBatterypack(config.temperatureManagerBatterypack, temperatureInterface),
          temperatureManagerEnvironment(config.temperatureManagerEnvironment, temperatureInterface),
          fanInterface(config.fanInterface, fanInterfaceSetrategy),
          fanManager(config.fanManager, fanInterface, fanControllingAlgorithm, fanSmoothingAlgorithm, [&]() {
              return FanManager::TargetSet(temperatureManagerBatterypack.setpoint(), temperatureManagerBatterypack.current());
          }),
          batteryManager(config.batteryManager),
          devices(config.devices, [&]() {
              return network.available();
          }),
          network(config.network, devices.mdns()), nettime(config.nettime, [&]() {
              return network.available();
          }),
          control(config.control, address, devices),
          deliver(config.deliver, address, devices.blue(), devices.mqtt(), devices.websocket()),
          publish(config.publish, address, devices.mqtt()),
          storage(config.storage),
          updater(config.updater, [&]() {
              return network.available();
          }),
          alarmsInterface(config.alarmsInterface),
          alarms(config.alarms, alarmsInterface, { &temperatureManagerEnvironment, &temperatureManagerBatterypack, &nettime, &deliver, &publish, &storage, &platform }),
          diagnostics(config.diagnostics, { &temperatureCalibrator, &temperatureInterface, &fanInterface, &temperatureManagerBatterypack, &temperatureManagerEnvironment, &fanManager, &batteryManager, &devices, &network, &nettime, &deliver, &publish, &storage, &control, &updater, &alarms, &platform, this }),
          operational(this),
          intervalDeliver(config.intervalDeliver), intervalCapture(config.intervalCapture), intervalDiagnose(config.intervalDiagnose), intervalProcess(config.intervalProcess),
          components({ &temperatureCalibrator, &temperatureInterface, &fanInterface, &temperatureManagerBatterypack, &temperatureManagerEnvironment, &fanManager, &alarms, &batteryManager, &devices, &network, &nettime, &deliver, &publish, &storage, &control, &updater, &diagnostics, this }) {
        DEBUG_PRINTF("Program::constructor: intervals - process=%lu, deliver=%lu, capture=%lu, diagnose=%lu\n", config.intervalProcess, config.intervalDeliver, config.intervalCapture, config.intervalDiagnose);
    };

    Component::List components;
    void setup() {
        for (const auto& component : components) component->begin();
    }
    void loop() {
        gate();
        for (const auto& component : components) component->process();
    }

protected:
    void collectDiagnostics(JsonVariant& obj) const override {
        JsonObject program = obj["program"].to<JsonObject>();
        extern const String build;
        program["build"] = build;
        program["uptime"] = uptime;
        program["cycles"] = cycles;
        if (intervalProcess.exceeded() > 0)
            program["exceeda"] = intervalProcess.exceeded();
    }
};

// -----------------------------------------------------------------------------------------------

void Program::OperationalManager::collect(JsonVariant& obj) const {
    JsonObject tmp = obj["tmp"].to<JsonObject>();
    JsonObject bms = tmp["bms"].to<JsonObject>();
        auto x = _program->batteryManager.instant();
        bms["V"] = x.voltage;
        bms["I"] = x.current;
        bms["C"] = x.charge;
    tmp["env"] = _program->temperatureManagerEnvironment.getTemperature();
    JsonObject bat = tmp["bat"].to<JsonObject>();
        bat["avg"] = _program->temperatureManagerBatterypack.avg();
        bat["min"] = _program->temperatureManagerBatterypack.min();
        bat["max"] = _program->temperatureManagerBatterypack.max();
    JsonArray val = bat["val"].to<JsonArray>();
        for (const auto& v : _program->temperatureManagerBatterypack.getTemperatures())
            val.add(v);
    obj["fan"] = _program->fanInterface.getSpeed();
    obj["alm"] = _program->alarms.toString();
}

// -----------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------
