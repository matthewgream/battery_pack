
// -----------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------

#include "Secrets.hpp"

// #define DEFAULT_WIFI_SSID "wifi_ssid" // Secrets.hpp
// #define DEFAULT_WIFI_PASS "wifi_pass" // Secrets.hpp
// #define DEFAULT_MQTT_USER "mqtt_user" // Secrets.hpp
// #define DEFAULT_MQTT_PASS "mqtt_pass" // Secrets.hpp

#if !defined (DEFAULT_WIFI_SSID) || !defined (DEFAULT_WIFI_PASS) || !defined (DEFAULT_MQTT_USER) || !defined (DEFAULT_MQTT_PASS)
#error "Require all of DEFAULT_WIFI_SSID, DEFAULT_WIFI_PASS, DEFAULT_MQTT_USER, DEFAULT_MQTT_PASS"
#endif
#ifndef DEFAULT_BLUE_PIN
#define DEFAULT_BLUE_PIN 123456 // Secrets.hpp
#endif

// -----------------------------------------------------------------------------------------------

/*
    C3-ZERO
        https://www.waveshare.com/esp32-c3-zero.htm, https://www.waveshare.com/wiki/ESP32-C3-Zero

    OPENSMART
        https://www.aliexpress.com/item/1005003356486895.html

    CD74HC4067
        https://deepbluembedded.com/arduino-cd74hc4067-analog-multiplexer-library-code/

    +-----+---------------+-----------+------------+----------+
    | DIR | C3-ZERO       | OPENSMART | CD74HC4067 | DS18B20  |
    +-----+---------------+-----------+------------+----------+
    | GND | 1  GND        | 8 GND     | 8 GND      | 3 GND    | input/output (power,   gnd)
    | VCC | 2  5V         | 7 VCC     |            |          | input/output (power,   5v0)
    | N/C | 3  3V3        |           | 7 VCC      | 2 VCC    | output       (power,   3v3)
    | OUT | 4  GP0  (ADC) |           | 1 SIG      |          | input        (analog,  ADC, 12 bit)
    | OUT | 5  GP1  (I2C) | 6 SDA     |            |          | input/output (digital, I2C)
    | OUT | 6  GP2  (I2C) | 5 SDL     |            |          | input/output (digital, I2C)
    | OUT | 7  GP3  (PWM) | 4 PWMA    |            |          | output       (digital, PWM, 8 bit)
    | OUT | 8  GP4  (PWM) | 3 PWMB    |            |          | output       (digital, PWM, 8 bit)
    | OUT | 9  GP5  (PWM) | 2 PWMC    |            |          | output       (digital, PWM, 8 bit)
    | OUT | 10 GP6  (PWM) | 1 PWMD    |            |          | output       (digital, PWM, 8 bit)
    | OUT | 11 GP7  (TTL) |           | 2 S3       |          | output       (digital)
    | OUT | 12 GP8  (TTL) |           | 3 S2       |          | output       (digital)
    | OUT | 13 GP9  (TTL) |           | 4 S1       |          | output       (digital)
    | OUT | 14 GP10 (TTL) |           | 5 S0       |          | output       (digital)
    | IN  | 15 GP18 [USB] |           |            |          | -            (don't use or will interfere with USB develop/debug)
    | N/C | 16 GP19 [USB] |           |            |          | -            (don't use or will interfere with USB develop/debug)
    | N/C | 17 GP20 (TTL) |           | 6 EN       |          | output       (digital, active low)
    | N/C | 18 GP21 (TTL) |           |            | 1 DATA   | input/output (digital, one-wire)
    +-----+---------------+-----------+------------+----------+

*/

// -----------------------------------------------------------------------------------------------

struct Config {

    TemperatureSensor_DS18B20::Config ds18b20 = {
        .PIN_DAT = 21, .INDEX = 0
    };

    // hardware interfaces
    TemperatureInterface::Config temperatureInterface = {
        .hardware = { .PIN_EN = 20, .PIN_SIG = 0, .PIN_ADDR = { 10, 9, 8, 7 } },
#ifdef TEMPERATURE_INTERFACE_DONTUSECALIBRATION
        .thermister = { .REFERENCE_RESISTANCE = 10000.0, .NOMINAL_RESISTANCE = 10000.0, .NOMINAL_TEMPERATURE = 25.0 }
#endif
    };
    TemperatureCalibrator::Config temperatureCalibrator = {
        .filename = "/temperaturecalibrations.json",
        .strategyDefault = { .A = -0.012400427786, .B = 0.006860769298, .C = -0.001057743719, .D = 0.000056166727 } // XXX populate from calibration data
    };
    FanInterface::Config fanInterface = {
        .hardware = { .I2C_ADDR = OpenSmart_QuadMotorDriver::I2cAddress, .PIN_I2C_SDA = 1, .PIN_I2C_SCL = 2, .PIN_PWMS = { 3, 4, 5, 6 }, .frequency = 5000, .invertedPWM = true },
        .DIRECTION = OpenSmart_QuadMotorDriver::MOTOR_CLOCKWISE, .MIN_SPEED = 96, .MAX_SPEED = 255, // duplicated, not ideal
        .MOTOR_ORDER = { 0, 1, 2, 3 }, .MOTOR_ROTATE = 5*60*1000
    };
    // hardware managers
    TemperatureManagerBatterypack::Config temperatureManagerBatterypack = {
        .channels = { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15 },
        .SETPOINT = 25.0,
        .FAILURE = -100.0, .MINIMAL = -20.0, .WARNING = 35.0, .MAXIMAL = 45.0
    };
    TemperatureManagerEnvironment::Config temperatureManagerEnvironment = {
        .channel = 0,
        .FAILURE = -100.0
    };
    double FAN_CONTROL_P = 10.0, FAN_CONTROL_I = 0.1, FAN_CONTROL_D = 1.0, FAN_SMOOTH_A = 0.1;
    FanManager::Config fanManager = {
    };

    // devices
    DeviceManager::Config devices = {
        .blue = { .name = DEFAULT_NAME, .serviceUUID = "4fafc201-1fb5-459e-8fcc-c5c9c331914b", .characteristicUUID = "beb5483e-36e1-4688-b7f5-ea07361b26a8", .pin = DEFAULT_BLUE_PIN, .intervalConnectionCheck = 1*60*1000 },
        .mdns = {},
        .mqtt = { .client = DEFAULT_NAME, .host = "mqtt.local", .user = DEFAULT_MQTT_USER, .pass = DEFAULT_MQTT_PASS, .port = 1883, .bufferSize = 3*1024 },
        .webserver = { .enabled  = true, .port = 80 },
        .websocket = { .enabled  = true, .port = 81, .root = "/" },
        .logging = { .enableSerial = true, .enableMqtt = true, .mqttTopic = DEFAULT_NAME }
    };

    // network managers
    NetwerkManager::Config network = {
        .host = DEFAULT_NAME, .ssid = DEFAULT_WIFI_SSID, .pass = DEFAULT_WIFI_PASS, .intervalConnectionCheck = 1*60*1000
    };
    NettimeManager::Config nettime = {
        .useragent = String (DEFAULT_NAME) + String ("/1.0"), .server = "http://matthewgream.net",
        .intervalUpdate = 60*60*1000, .intervalAdjust = 60*1000,
        .failureLimit = 3
    };

    // data managers
    DeliverManager::Config deliver = {
        .failureLimit = 3
    };
    PublishManager::Config publish = {
        .topic = DEFAULT_NAME,
        .failureLimit = 3
    };
    StorageManager::Config storage = {
        .filename = "/data.log",
        .remainLimit = 0.20, .failureLimit = 3,
    };

    // program
    ControlManager::Config control = { .url_version = "/version" };
    UpdateManager::Config updater = { .intervalCheck = 1*24*60*60*1000, .intervalLong = (interval_t) 28*24*60*60*1000, .json = DEFAULT_JSON, .type = DEFAULT_TYPE, .vers = DEFAULT_VERS, .addr = getMacAddressBase () };
    AlarmManager::Config alarms = { }; ActivablePIN::Config alarmsInterface = { .PIN = -1, .ACTIVE = LOW };
    DiagnosticManager::Config diagnostics = { };
    bool publishData = true, storageData = true, publishDiag = true, deliverDiag = true;
    interval_t intervalProcess = 5*1000, intervalDeliver = 5*1000, intervalCapture = 15*1000, intervalDiagnose = 60*1000;
};

// -----------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------
