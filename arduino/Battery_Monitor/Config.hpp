
// -----------------------------------------------------------------------------------------------

#define DEFAULT_SERIAL_BAUD 115200

#define DEFAULT_NAME "BatteryMonitor"

#include "Secrets.hpp"
// #define DEFAULT_WIFI_SSID "wifi_ssid" // Secrets.hpp
// #define DEFAULT_WIFI_PASS "wifi_pass" // Secrets.hpp
// #define DEFAULT_MQTT_USER "mqtt_user" // Secrets.hpp
// #define DEFAULT_MQTT_PASS "mqtt_pass" // Secrets.hpp
// #define DEFAULT_BLUE_PIN 123456 // Secrets.hpp

// -----------------------------------------------------------------------------------------------

/*
    C3-ZERO
        https://www.waveshare.com/esp32-c3-zero.htm, https://www.waveshare.com/wiki/ESP32-C3-Zero

    OPENSMART
        https://www.aliexpress.com/item/1005003356486895.html

    CD74HC4067
        https://deepbluembedded.com/arduino-cd74hc4067-analog-multiplexer-library-code/

    +-----+---------------+-----------+------------+
    | DIR | C3-ZERO       | OPENSMART | CD74HC4067 |
    +-----+---------------+-----------+------------+
    | GND | 1  GND        | 8  GND    | 8  GND     |
    | VCC | 2  5V         | 7  VCC    | 7  VCC     |
    | N/C | 3  3V3        |           |            |
    | OUT | 4  GP0  (I2C) | 6  SCL    |            |
    | OUT | 5  GP1  (I2C) | 5  SDA    |            |
    | OUT | 6  GP2  (PWM) | 4  PWMA   |            |
    | OUT | 7  GP3  (PWM) | 3  PWMB   |            |
    | OUT | 8  GP4  (PWM) | 2  PWMC   |            |
    | OUT | 9  GP5  (PWM) | 1  PWMD   |            |
    | OUT | 10 GP6  (ADC) |           | 1  SIG     |
    | OUT | 11 GP7  (TTL) |           | 2  S3      |
    | OUT | 12 GP8  (TTL) |           | 3  S2      |
    | OUT | 13 GP9  (TTL) |           | 4  S1      |
    | OUT | 14 GP10 (TTL) |           | 5  S0      |
    | IN  | 15 GP18 [USB] |           |            |
    | N/C | 16 GP19 [USB] |           |            |
    | N/C | 17 GP20 (TTL) |           | 6  EN      |
    | N/C | 18 GP21       |           |            |
    +-----+---------------+-----------+------------+

    https://github.com/mikedotalmond/Arduino-MuxInterface-CD74HC4067
    https://github.com/ugurakas/Esp32-C3-LP-Project
    https://github.com/ClaudeMarais/ContinousAnalogRead_ESP32-C3
*/

// -----------------------------------------------------------------------------------------------

struct Config {

    // hardware interfaces
    TemperatureInterface::Config temperatureInterface = {
        .mux = { .PIN_EN = 20, .PIN_SIG = 6, .PIN_ADDR = { 10, 9, 8, 7 } },
        .thermister = { .REFERENCE_RESISTANCE = 10000.0, .NOMINAL_RESISTANCE = 10000.0, .NOMINAL_TEMPERATURE = 25.0 }
    };
    FanInterface::Config fanInterface = {
        .qmd = { .I2C_ADDR = OpenSmart_QuadMotorDriver::I2cAddress, .PIN_I2C_SDA = 1, .PIN_I2C_SCL = 0, .PIN_PWMS = { 2, 3, 4, 5 } },
        .MIN_SPEED = 85, .MAX_SPEED = 255 // duplicated, not ideal
    };
    // hardware managers
    TemperatureManager::Config temperatureManager = {
        .PROBE_NUMBER = 16, .PROBE_ENVIRONMENT = 15,
        .SETPOINT = 25.0,
        .FAILURE = -100.0, .MINIMAL = -20.0, .WARNING = 35.0, .MAXIMAL = 45.0
    };
    FanManager::Config fanManager = {
        .NO_SPEED = 0, .MIN_SPEED = 85, .MAX_SPEED = 255
    };

    // network managers
    ConnectManager::Config network = {
        .client = DEFAULT_NAME, .ssid = DEFAULT_WIFI_SSID, .pass = DEFAULT_WIFI_PASS
    };
    NettimeManager::Config nettime = {
        .useragent = String (DEFAULT_NAME) + String ("/1.0"), .server = "https://www.google.com",
        .intervalUpdate = 60*60*1000, .intervalAdjust = 60*1000,
        .failureLimit = 3
    };

    // data managers
    DeliverManager::Config deliver = {
        .blue = { .name = DEFAULT_NAME, .serviceUUID = "4fafc201-1fb5-459e-8fcc-c5c9c331914b", .characteristicUUID = "beb5483e-36e1-4688-b7f5-ea07361b26a8", .pin = DEFAULT_BLUE_PIN, .mtu = 512 }
    };
    PublishManager::Config publish = {
        .mqtt = { .client = DEFAULT_NAME, .host = "mqtt.local", .user = DEFAULT_MQTT_USER, .pass = DEFAULT_MQTT_PASS, .topic = DEFAULT_NAME, .port = 1883, .bufferSize = 2048 },
        .failureLimit = 3
    };
    StorageManager::Config storage = {
        .filename = "/data.log",
        .remainLimit = 0.20, .failureLimit = 3,
    };

    // program
    AlarmInterface_SinglePIN::Config alarmInterface = { .PIN_ALARM = -1 };
    AlarmManager::Config alarmManager = { };
    DiagnosticManager::Config diagnosticManager = { }; 
    interval_t intervalProcess = 5*1000, intervalDeliver = 15*1000, intervalCapture = 15*1000, intervalDiagnose = 60*1000;
};

// -----------------------------------------------------------------------------------------------
