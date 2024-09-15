
#ifndef __CONFIG_HPP__
#define __CONFIG_HPP__

// -----------------------------------------------------------------------------------------------

#include "Secrets.hpp"

#define DEFAULT_SERIAL_BAUD 115200

#define DEFAULT_WIFI_HOST "BatteryMonitor"
// #define DEFAULT_WIFI_SSID "SSID"
// #define DEFAULT_WIFI_PASS "PASS"

#define DEFAULT_MQTT_HOST "mqtt.local"
#define DEFAULT_MQTT_PORT 1883
// #define DEFAULT_MQTT_USER "USER"
// #define DEFAULT_MQTT_PASS "PASS"
#define DEFAULT_MQTT_TOPIC "batterymonitor/data"
#define DEFAULT_MQTT_CLIENT "BatteryMonitor"

#define DEFAULT_BLUE_NAME "BatteryMonitor"
#define DEFAULT_BLUE_SERVICE "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define DEFAULT_BLUE_CHARACTERISTIC "beb5483e-36e1-4688-b7f5-ea07361b26a8"

#define DEFAULT_TIME_SERVER "http://google.com"
#define DEFAULT_TIME_UPDATE (1000 * 60 * 60)
#define DEFAULT_TIME_ADJUST (1000 * 60)
#define DEFAULT_TIME_FAILURES (3)

#define DEFAULT_STORAGE_NAME "data.log"
#define DEFAULT_STORAGE_WRITE (1000 * 30)
#define DEFAULT_STORAGE_LENGTH_MAXIMUM (1024 * 1024)
#define DEFAULT_STORAGE_LENGTH_CRITICAL (DEFAULT_STORAGE_LENGTH_MAXIMUM * 0.80)
#define DEFAULT_STORAGE_FAILURES (3)

#define DEFAULT_INTERVAL_PROCESS (1000 * 5)
#define DEFAULT_INTERVAL_DELIVER (1000 * 15)
#define DEFAULT_INTERVAL_CAPTURE (1000 * 15)

// -----------------------------------------------------------------------------------------------

typedef unsigned long interval_t;

struct Config {

    struct TemperatureInterfaceConfig { 
        struct MuxInterfaceConfig { 
            const int PIN_S0 = 2, PIN_S1 = 3, PIN_S2 = 4, PIN_S3 = 5, PIN_SIG = 6;
        } mux;
        const float REFERENCE_RESISTANCE = 10000.0, NOMINAL_RESISTANCE = 10000.0, NOMINAL_TEMPERATURE = 25.0, BETA = 3950.0;
        const int PROBE_NUMBER = 16, PROBE_ENVIRONMENT = 15;
        const float MINIMAL = -20.0, WARNING = 40.0, CRITICAL = 50.0;
    } temperature;
    struct FanInterfaceConfig {
        const int PIN_PWM = 7, MIN_SPEED = 85, MAX_SPEED = 255;
    } fan;
  
    struct ConnectConfig {
        const String host = DEFAULT_WIFI_HOST, ssid = DEFAULT_WIFI_SSID, pass = DEFAULT_WIFI_PASS;
    } network;
    struct DeliverConfig {
        const String name = DEFAULT_BLUE_NAME, service = DEFAULT_BLUE_SERVICE, characteristic = DEFAULT_BLUE_CHARACTERISTIC;
    } deliver;
    struct PublishConfig {
        const String client = DEFAULT_MQTT_CLIENT, host = DEFAULT_MQTT_HOST, user = DEFAULT_MQTT_USER, pass = DEFAULT_MQTT_PASS, topic = DEFAULT_MQTT_TOPIC;
        const uint16_t port = DEFAULT_MQTT_PORT;
    } publish;
    struct NettimeConfig {
        const String server = DEFAULT_TIME_SERVER;
        const interval_t intervalUpdate = DEFAULT_TIME_UPDATE, intervalAdjust = DEFAULT_TIME_ADJUST;
        const int failureLimit = DEFAULT_TIME_FAILURES;
    } nettime;
    struct StorageConfig {
        const String filename = DEFAULT_STORAGE_NAME;
        const int failureLimit = DEFAULT_STORAGE_FAILURES;
        const size_t lengthMaximum = DEFAULT_STORAGE_LENGTH_MAXIMUM, lengthCritical = DEFAULT_STORAGE_LENGTH_CRITICAL;
    } storage;

    struct AlarmConfig {
        const int PIN_ALARM = 8;
    } alarm;

    const interval_t intervalProcess = DEFAULT_INTERVAL_PROCESS, intervalDeliver = DEFAULT_INTERVAL_DELIVER, intervalCapture = DEFAULT_INTERVAL_CAPTURE;
};

// -----------------------------------------------------------------------------------------------

#endif
