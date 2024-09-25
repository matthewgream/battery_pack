
#ifndef __CONFIG_HPP__
#define __CONFIG_HPP__

// -----------------------------------------------------------------------------------------------

#include "Secrets.hpp"

#define DEFAULT_SERIAL_BAUD 115200

#define DEFAULT_NAME "BatteryManager"

#define DEFAULT_WIFI_CLIENT DEFAULT_NAME
// #define DEFAULT_WIFI_SSID "wifi_ssid" // Secrets.hpp
// #define DEFAULT_WIFI_PASS "wifi_pass" // Secrets.hpp

#define DEFAULT_MQTT_CLIENT DEFAULT_NAME
// #define DEFAULT_MQTT_HOST "mqtt.local" // Secrets.hpp
// #define DEFAULT_MQTT_PORT 1883 // Secrets.hpp
// #define DEFAULT_MQTT_USER "mqtt_user" // Secrets.hpp
// #define DEFAULT_MQTT_PASS "mqtt_pass" // Secrets.hpp
#define DEFAULT_MQTT_TOPIC DEFAULT_NAME
#define DEFAULT_PUBLISH_FAILURES (3)

#define DEFAULT_BLUE_NAME DEFAULT_NAME
#define DEFAULT_BLUE_SERVICE "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define DEFAULT_BLUE_CHARACTERISTIC "beb5483e-36e1-4688-b7f5-ea07361b26a8"
#define DEFAULT_BLUE_PIN 123456
#define DEFAULT_DELIVER_FAILURES (3)

#define DEFAULT_NETTIME_USERAGENT (String (DEFAULT_NAME) + String ("/1.0"))
#define DEFAULT_NETTIME_SERVER "https://www.google.com"
#define DEFAULT_NETTIME_UPDATE (1000 * 60 * 60)
#define DEFAULT_NETTIME_ADJUST (1000 * 60)
#define DEFAULT_NETTIME_FAILURES (3)

#define DEFAULT_STORAGE_NAME "data.log"
#define DEFAULT_STORAGE_WRITE (1000 * 30)
#define DEFAULT_STORAGE_LENGTH_MAXIMUM (1024 * 1024)
#define DEFAULT_STORAGE_LENGTH_CRITICAL (DEFAULT_STORAGE_LENGTH_MAXIMUM * 0.80)
#define DEFAULT_STORAGE_FAILURES (3)

#define DEFAULT_INTERVAL_PROCESS (1000 * 5)
#define DEFAULT_INTERVAL_DELIVER (1000 * 15)
#define DEFAULT_INTERVAL_CAPTURE (1000 * 15)
#define DEFAULT_INTERVAL_DIAGNOSE (1000 * 60)

// -----------------------------------------------------------------------------------------------

struct Config {

    struct TemperatureInterfaceConfig {
        MuxInterface_CD74HC4067::Config mux = { .PIN_S0 = 2, .PIN_S1 = 3, .PIN_S2 = 4, .PIN_S3 = 5, .PIN_SIG = 6 };
        struct Thermister {
            const float REFERENCE_RESISTANCE = 10000.0, NOMINAL_RESISTANCE = 10000.0, NOMINAL_TEMPERATURE = 25.0;
        } thermister;
        const int PROBE_NUMBER = 16, PROBE_ENVIRONMENT = 15;
        const float MINIMAL = -20.0, WARNING = 40.0, CRITICAL = 50.0;
    } temperature;
    struct FanInterfaceConfig {
        const int PIN_PWMA = 7, PIN_PWMB = 8, PIN_PWMC = 9, PIN_PWMD = 10, I2C = OpenSmart_QuadMotorDriver::MOTOR_CONTROL_I2CADDRESS;
        const uint8_t MIN_SPEED = 85, MAX_SPEED = 255;
    } fan;

    struct ConnectConfig {
        const String client = DEFAULT_WIFI_CLIENT, ssid = DEFAULT_WIFI_SSID, pass = DEFAULT_WIFI_PASS;
    } network;
    struct NettimeConfig {
        const String useragent = DEFAULT_NETTIME_USERAGENT, server = DEFAULT_NETTIME_SERVER;
        const interval_t intervalUpdate = DEFAULT_NETTIME_UPDATE, intervalAdjust = DEFAULT_NETTIME_ADJUST;
        const int failureLimit = DEFAULT_NETTIME_FAILURES;
    } nettime;
    struct DeliverConfig {
        BluetoothNotifier::Config blue = { .name = DEFAULT_BLUE_NAME, .serviceUUID = DEFAULT_BLUE_SERVICE, .characteristicUUID = DEFAULT_BLUE_CHARACTERISTIC, .pin = DEFAULT_BLUE_PIN };
    } deliver;
    struct PublishConfig {
        MQTTPublisher::Config mqtt = { .client = DEFAULT_MQTT_CLIENT, .host = DEFAULT_MQTT_HOST, .user = DEFAULT_MQTT_USER, .pass = DEFAULT_MQTT_PASS, .topic = DEFAULT_MQTT_TOPIC, .port = DEFAULT_MQTT_PORT };
        const int failureLimit = DEFAULT_PUBLISH_FAILURES;
    } publish;
    struct StorageConfig {
        const String filename = DEFAULT_STORAGE_NAME;
        const size_t lengthMaximum = DEFAULT_STORAGE_LENGTH_MAXIMUM, lengthCritical = DEFAULT_STORAGE_LENGTH_CRITICAL;
        const int failureLimit = DEFAULT_STORAGE_FAILURES;
    } storage;

    struct AlarmConfig {
        const int PIN_ALARM = 8;
    } alarm;
    struct DiagnosticConfig {
    } diagnostic;

    const interval_t intervalProcess = DEFAULT_INTERVAL_PROCESS, intervalDeliver = DEFAULT_INTERVAL_DELIVER, intervalCapture = DEFAULT_INTERVAL_CAPTURE, intervalDiagnose = DEFAULT_INTERVAL_DIAGNOSE;
};

// -----------------------------------------------------------------------------------------------

#endif
