
// -----------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------

// #define DEFAULT_WIFI_PEERS { "ssid:pass", "_Heathrow Wi-Fi" }
// #define DEFAULT_MQTT_PEERS { "mqtt.in.the.cloud/user@pass", "mqtt.local:1883/user@pass" }

#if ! defined(DEFAULT_WIFI_PEERS)
#error "Require DEFAULT_WIFI_PEERS"
#endif
#ifndef DEFAULT_MQTT_PEERS
#define DEFAULT_MQTT_PEERS { "mqtt.local:1883/user@pass" }
#endif
#ifndef DEFAULT_BLUE_PIN
#define DEFAULT_BLUE_PIN 123456    // Secrets.hpp
#endif

// -----------------------------------------------------------------------------------------------

/*
    OPENSMART
        https://www.aliexpress.com/item/1005003356486895.html
        https://www.tinytronics.nl/product_files/005376_Open-Smart_Quad_Motor_Driver_schematic.pdf
        https://www.tinytronics.nl/product_files/005376_QuadMotorDriver.zip
    CD74HC4067
        https://www.ti.com/lit/ds/symlink/cd74hc4067.pdf
        https://deepbluembedded.com/arduino-cd74hc4067-analog-multiplexer-library-code/
        https://github.com/mikedotalmond/Arduino-Mux-CD74HC4067
    DS18B20
        https://www.analog.com/media/en/technical-documentation/data-sheets/DS18B20.pdf
        https://github.com/milesburton/Arduino-Temperature-Control-Library
*/

/*
    C3-ZERO
        https://www.espressif.com/sites/default/files/documentation/esp32-c3_datasheet_en.pdf
        https://www.waveshare.com/esp32-c3-zero.htm, https://www.waveshare.com/wiki/ESP32-C3-Zero

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

    4MB flash
    - partitions_esp32c3fn4.csv
    -      x0009K
    - NVS, x0007K (28Kb)
    - APP, x0270K (~2.4Mb)
    - FFS, x0180K (1.5Mb) -- calibration data & offline data storage

*/

/*
    S3-SUPER-MINI
        TBC
        https://www.espressif.com/sites/default/files/documentation/esp32-s3_datasheet_en.pdf
        https://www.aliexpress.com/item/1005006365700692.html

    8MB flash
    - partitions_esp32s3fn8.csv
    -      x0009K
    - NVS, x000DK (52Kb)
    - OTA, x0002K (8Kb) (OTADATA)
    - APP, x0300K (3Mb) (OTA0)
    - APP, x0300K (3Mb) (OTA1)
    - FFS, x0180K (1.5Mb) -- calibration data & offline data storage

*/

/*
  HARDWARE_ESP32_S3_YD_ESP32_S3_C
  ESP32-S3-DEVKITC-1

  - COM (non-native USB) for develop/debug, using Serial1.begin(SERIAL1_BAUD, SERIAL_8N1, GPIO_NUM_44, GPIO_NUM_43) - leaving Serial2
  - USB (native USB) otherwise, using Serial, leaving Serial1/Serial2

*/

#if defined(HARDWARE_ESP32_C3_ZERO)
#define PIN_DS18B0_DAT         21
#define PIN_CD74HC4067_EN      20
#define PIN_CD74HC4067_SIG     0
#define PIN_CD74HC4067_ADDR_S0 10
#define PIN_CD74HC4067_ADDR_S1 9
#define PIN_CD74HC4067_ADDR_S2 8
#define PIN_CD74HC4067_ADDR_S3 7
#define PIN_OSQMD_I2CSDA       1
#define PIN_OSQMD_I2CSCL       2
#define PIN_OSQMD_PWM_0        3
#define PIN_OSQMD_PWM_1        4
#define PIN_OSQMD_PWM_2        5
#define PIN_OSQMD_PWM_3        6
#elif defined(HARDWARE_ESP32_S3_YD_ESP32_S3_C)
#define PIN_DS18B0_DAT         1     // MOVE
#define PIN_CD74HC4067_EN      2     // MOVE
#define PIN_CD74HC4067_SIG     13    // AS IS
#define PIN_CD74HC4067_ADDR_S0 3     // AS IS
#define PIN_CD74HC4067_ADDR_S1 4     // AS IS
#define PIN_CD74HC4067_ADDR_S2 5     // AS IS
#define PIN_CD74HC4067_ADDR_S3 6     // AS IS
#define PIN_OSQMD_I2CSDA       12    // AS IS
#define PIN_OSQMD_I2CSCL       11    // AS IS
#define PIN_OSQMD_PWM_0        10    // AS IS
#define PIN_OSQMD_PWM_1        9     // AS IS
#define PIN_OSQMD_PWM_2        8     // AS IS
#define PIN_OSQMD_PWM_3        7     // AS IS
#endif

// TBC
#define PIN_DALY_MANAGER_SERIAL_ID 1
#define PIN_DALY_MANAGER_SERIAL_RX GPIO_NUM_5
#define PIN_DALY_MANAGER_SERIAL_TX GPIO_NUM_6
#define PIN_DALY_MANAGER_SERIAL_EN GPIO_NUM_7
#define PIN_DALY_BALANCE_SERIAL_ID 2
#define PIN_DALY_BALANCE_SERIAL_RX GPIO_NUM_15
#define PIN_DALY_BALANCE_SERIAL_TX GPIO_NUM_16
#define PIN_DALY_BALANCE_SERIAL_EN GPIO_NUM_17
#define PIN_RANDOM_NOISE           GPIO_NUM_1

// -----------------------------------------------------------------------------------------------

struct Config {

    // PLATFORM
    PlatformArduinoESP32::Config programPlatform = { .pinRandomNoise = PIN_RANDOM_NOISE };
    struct {
        int PIN_SDA, PIN_SCL;
    } i2c0 = { .PIN_SDA = -1, .PIN_SCL = -1 }, i2c1 = { .PIN_SDA = -1, .PIN_SCL = -1 };

    // BATTERYPACK
    ModuleBatterypack::Config moduleBatterypack = {
        .temperatureSensorsCalibrator = { .filename = "/temperaturecalibrations.json", .strategyDefault = { .A = -0.012400427786, .B = 0.006860769298, .C = -0.001057743719, .D = 0.000056166727 } }, // XXX populate from calibration data
        .temperatureSensorsInterface = { .hardware = { .PIN_EN = PIN_CD74HC4067_EN, .PIN_SIG = PIN_CD74HC4067_SIG, .PIN_ADDR = { PIN_CD74HC4067_ADDR_S0, PIN_CD74HC4067_ADDR_S1, PIN_CD74HC4067_ADDR_S2, PIN_CD74HC4067_ADDR_S3 } },
#ifdef TEMPERATURE_INTERFACE_DONTUSECALIBRATION
                                         .thermister = { .REFERENCE_RESISTANCE = 10000.0, .NOMINAL_RESISTANCE = 10000.0, .NOMINAL_TEMPERATURE = 25.0 }
#endif
        },
        .temperatureSensorsManagerBatterypack = { .channels = { 0, 1, 2, 3, 4, 5, 6, 7, 9, 10, 11, 12, 13, 14, 15 }, .SETPOINT = 25.0, .FAILURE = -100.0, .MINIMAL = -20.0, .WARNING = 35.0, .MAXIMAL = 45.0 },
        .temperatureSensorsManagerEnvironment = { .channel = 8, .FAILURE = -100.0 },
        .FAN_CONTROL_P = 10.0,
        .FAN_CONTROL_I = 0.1,
        .FAN_CONTROL_D = 1.0,
        .FAN_SMOOTH_A = 0.1,
        .fanControllersInterface = { .hardware = { .I2C_ADDR = OpenSmart_QuadMotorDriver::I2cAddress, .PIN_I2C_SDA = PIN_OSQMD_I2CSDA, .PIN_I2C_SCL = PIN_OSQMD_I2CSCL, .PIN_PWMS = { PIN_OSQMD_PWM_0, PIN_OSQMD_PWM_1, PIN_OSQMD_PWM_2, PIN_OSQMD_PWM_3 }, .frequency = 5000, .invertedPWM = true },
                                         .DIRECTION = OpenSmart_QuadMotorDriver::MOTOR_CLOCKWISE,
                                         .MIN_SPEED = 96,
                                         .MAX_SPEED = 255,
                                         .MOTOR_ORDER = { 0, 1, 2, 3 },
                                         .MOTOR_ROTATE = 5 * 60 * 1000 },
        .fanControllersManager = {},
        .batteryManagerManager = { .manager = { .manager = {
                                                    .id = "manager",
                                                    .capabilities = daly_bms::Capabilities::Managing + daly_bms::Capabilities::TemperatureSensing - daly_bms::Capabilities::FirmwareIndex - daly_bms::Capabilities::RealTimeClock,
                                                    .categories = daly_bms::Categories::All,
                                                    .debugging = daly_bms::Debugging::Errors + daly_bms::Debugging::Requests + daly_bms::Debugging::Responses,
                                                },
                                                .serialId = PIN_DALY_MANAGER_SERIAL_ID,
                                                .serialRxPin = PIN_DALY_MANAGER_SERIAL_RX,
                                                .serialTxPin = PIN_DALY_MANAGER_SERIAL_TX,
                                                .enPin = PIN_DALY_MANAGER_SERIAL_EN },
                                         .balance = { .manager = {
                                                    .id = "balance",
                                                    .capabilities = daly_bms::Capabilities::Balancing + daly_bms::Capabilities::TemperatureSensing - daly_bms::Capabilities::FirmwareIndex,
                                                    .categories = daly_bms::Categories::All,
                                                    .debugging = daly_bms::Debugging::Errors + daly_bms::Debugging::Requests + daly_bms::Debugging::Responses,
                                                },
                                                .serialId = PIN_DALY_BALANCE_SERIAL_ID,
                                                .serialRxPin = PIN_DALY_BALANCE_SERIAL_RX,
                                                .serialTxPin = PIN_DALY_BALANCE_SERIAL_TX,
                                                .enPin = PIN_DALY_BALANCE_SERIAL_EN },
                                         .intervalInstant = 15 * 1000,
                                         .intervalStatus = 60 * 1000,
                                         .intervalDiagnostics = 5 * 60 * 1000 },
        .ds18b20 = { .PIN_DAT = PIN_DS18B0_DAT, .INDEX = 0 }
    };

    // CONDITIONS
    ProgramManageBluetoothTPMS::Config tyrePressureManager = {
        .front = "38:89:00:00:36:02", .rear = "38:8b:00:00:ed:63"
    };

    // CONNECTIVITY
    ModuleConnectivity::Config moduleConnectivity = {
        .blue = { .name = DEFAULT_NAME, .serviceUUID = "4fafc201-1fb5-459e-8fcc-c5c9c331914b", .characteristicUUID = "beb5483e-36e1-4688-b7f5-ea07361b26a8", .pin = DEFAULT_BLUE_PIN, .intervalConnectionCheck = 1 * 60 * 1000 },
        .mdns = {},
        .mqtt = { .client = DEFAULT_NAME, .peers = { .order = DEFAULT_MQTT_PEERS, .retries = 3 }, .bufferSize = 3 * 1024 },
        .webserver = { .enabled = true, .port = 80 },
        .websocket = { .enabled = true, .port = 81, .root = "/" },
        .wifi = { .host = DEFAULT_NAME, .peers = { .order = DEFAULT_WIFI_PEERS, .retries = 3 }, .intervalConnectionCheck = 1 * 60 * 1000 }
    };

    // CONTENT
    ProgramDataDeliver::Config dataDeliver = { .topic = DEFAULT_NAME, .failureLimit = 3 };
    ProgramDataPublish::Config dataPublish = { .topic = DEFAULT_NAME, .failureLimit = 3 };
    ProgramDataStorage::Config dataStorage = { .filename = "/data.log", .remainLimit = 0.20, .failureLimit = 3 };
    ProgramDataControl::Config dataControl = { .url_version = "/version" };
    bool dataPublishEnabled = true, dataStorageEnabled = true, diagPublishEnabled = true, diagDeliverEnabled = true;
    interval_t dataProcessInterval = 5 * 1000, dataDeliverInterval = 5 * 1000, dataCaptureInterval = 15 * 1000, dataDiagnoseInterval = 60 * 1000;

    // PROGRAM
    ProgramTime::Config programTime = { .hardware = {}, .useragent = String (DEFAULT_NAME) + String ("/1.0"), .server = "http://matthewgream.net", .intervalUpdate = 60 * 60 * 1000, .intervalAdjust = 60 * 1000, .failureLimit = 3 };
    ProgramLogging::Config programLogging = { .enableSerial = true, .enableMqtt = true, .mqttTopic = DEFAULT_NAME };
    ProgramUpdates::Config programUpdater = { .startupCheck = true, .updateImmmediately = true, .intervalCheck = 1 * 24 * 60 * 60 * 1000, .intervalLong = (interval_t) 28 * 24 * 60 * 60 * 1000, .json = DEFAULT_JSON, .type = DEFAULT_TYPE, .vers = DEFAULT_VERS, .addr = getMacAddressBase () };
    ProgramAlarmsInterface::Config programAlarmsInterface = { .PIN = -1, .ACTIVE = LOW };
    ProgramAlarms::Config programAlarms = {};
    DiagnosticablesManager::Config programDiagnostics = {};
    interval_t programInterval = 5 * 1000;
};

// -----------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------
