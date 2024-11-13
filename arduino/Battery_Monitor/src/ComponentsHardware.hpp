
// -----------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------

#include <Arduino.h>

class ActivablePIN {
public:
    typedef struct {
        int PIN;
        uint8_t ACTIVE;
    } Config;

private:
    const Config& config;

    bool _enabled = false;

public:
    explicit ActivablePIN(const Config& cfg)
        : config(cfg) {
        if (config.PIN >= 0) {
            pinMode(config.PIN, OUTPUT);
            digitalWrite(config.PIN, !config.ACTIVE);
        }
    }

    void set(const bool enabled) {
        if (config.PIN >= 0)
            if (enabled != _enabled) {
                digitalWrite(config.PIN, enabled ? config.ACTIVE : !config.ACTIVE);
                _enabled = enabled;
            }
    }
};

// -----------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------

#include <OneWire.h>
#include <DallasTemperature.h>

class TemperatureSensor_DS18B20 {
public:
    typedef struct {
        int PIN_DAT;
        int INDEX;
    } Config;

private:
    const Config& config;

    OneWire oneWire;
    DallasTemperature sensors;
    bool _available;
    DeviceAddress _address;

public:
    explicit TemperatureSensor_DS18B20(const Config& cfg)
        : config(cfg), oneWire(config.PIN_DAT), sensors(&oneWire) {
        sensors.begin();
        DEBUG_PRINTF("TemperatureSensor_DS18B20::init: (DAT=%d) found %d devices on bus, %d are DS18", config.PIN_DAT, sensors.getDeviceCount(), sensors.getDS18Count());
        if ((_available = sensors.getAddress(_address, config.INDEX)))
            DEBUG_PRINTF(" [0] = %s", BytesToHexString<8>(_address, "").c_str());
        DEBUG_PRINTF("\n");
    }
    float getTemperature() {
        float temp = -273.15;
        if (!_available || !sensors.requestTemperaturesByAddress(_address) || (temp = sensors.getTempC(_address)) == DEVICE_DISCONNECTED_C)
            DEBUG_PRINTF("TemperatureSensor_DS18B20::getTemperature: device is disconnected\n");
        return temp;
    }

    static bool present(const int pin) {
        static constexpr uint8_t DS18B20_ADDRESS = 0x28;
        OneWire onewire(pin);
        DeviceAddress address;
        return onewire.reset() && onewire.search(address) && (address[0] == DS18B20_ADDRESS) && (OneWire::crc8(address, sizeof(address) - 1) == address[sizeof(address) - 1]);
    }
};

// -----------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------

#include <Arduino.h>
#include <array>

template<typename ADC_VALUE_TYPE>
class MuxInterface_CD74HC4067 {    // technically this is ADC as well due to PIN_SIG
public:
    static inline constexpr int ADDRESS_WIDTH = 4;
    static inline constexpr int CHANNELS = (1 << ADDRESS_WIDTH);

    typedef struct {
        int PIN_EN, PIN_SIG;
        std::array<int, ADDRESS_WIDTH> PIN_ADDR;
    } Config;

private:
    const Config& config;

public:
    explicit MuxInterface_CD74HC4067(const Config& cfg)
        : config(cfg) {
        DEBUG_PRINTF("MuxInterface_CD74HC4067::init: (EN=%d,S0=%d,S1=%d,S2=%d,S3=%d,SIG=%d)\n", config.PIN_EN, config.PIN_ADDR[0], config.PIN_ADDR[1], config.PIN_ADDR[2], config.PIN_ADDR[3], config.PIN_SIG);
        pinMode(config.PIN_EN, OUTPUT);
        digitalWrite(config.PIN_EN, HIGH);    // OFF
        pinMode(config.PIN_ADDR[0], OUTPUT);
        pinMode(config.PIN_ADDR[1], OUTPUT);
        pinMode(config.PIN_ADDR[2], OUTPUT);
        pinMode(config.PIN_ADDR[3], OUTPUT);
        pinMode(config.PIN_SIG, INPUT);
    }
    ADC_VALUE_TYPE get(const int channel) const {
        digitalWrite(config.PIN_ADDR[0], channel & 0x01 ? HIGH : LOW);
        digitalWrite(config.PIN_ADDR[1], channel & 0x02 ? HIGH : LOW);
        digitalWrite(config.PIN_ADDR[2], channel & 0x04 ? HIGH : LOW);
        digitalWrite(config.PIN_ADDR[3], channel & 0x08 ? HIGH : LOW);
        delay(10);
        return analogRead(config.PIN_SIG);
    }
    void enable(const bool state = true) {
        digitalWrite(config.PIN_EN, state ? LOW : HIGH);    // Active-LOW
    }
};

// -----------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------

#include <Arduino.h>
#include <Wire.h>
#include <array>

class OpenSmart_QuadMotorDriver {
public:
    static inline constexpr int MotorCount = 4;
    static inline constexpr uint8_t I2cAddress = 0x20;
    typedef std::array<int, MotorCount> MotorSpeedPins;

    typedef struct {
        uint8_t I2C_ADDR;
        int PIN_I2C_SDA, PIN_I2C_SCL;
        MotorSpeedPins PIN_PWMS;
        int frequency;
        bool invertedPWM;
    } Config;

    enum MotorID {
        MOTOR_ALL = -1,
        MOTOR_A = 0,
        MOTOR_B = 1,
        MOTOR_C = 2,
        MOTOR_D = 3
    };
    enum MotorDirection {
        MOTOR_CLOCKWISE = 0,
        MOTOR_ANTICLOCKWISE = 1
    };

    static inline constexpr int MotorSpeedResolution = 8;
    typedef uint8_t MotorSpeed;

private:
    const Config& config;

    int _status;
    uint8_t _directions;

    enum MotorControl {
        MOTOR_CONTROL_OFF = 0x00,
        MOTOR_CONTROL_ANTICLOCKWISE = 0x01,
        MOTOR_CONTROL_CLOCKWISE = 0x02
    };
    static constexpr uint8_t controlvalue_alloff = 0x00;

    static constexpr uint8_t encodeControlValue(const int motorID, const uint8_t directions, const uint8_t value) {
        return (directions & ~(0x03 << (2 * motorID))) | (value << (2 * motorID));
    }

    void applyDirections(const int motorID, const MotorControl value) {
        for (int id = 0; id < MotorCount; id++)
            if (motorID == MOTOR_ALL || motorID == id)
                _directions = encodeControlValue(id, _directions, value);
        Wire.beginTransmission(config.I2C_ADDR);
        Wire.write(_directions);
        Wire.endTransmission();
    }
    void applySpeed(const int motorID, const MotorSpeed value) {
        for (int id = 0; id < MotorCount; id++)
            if (motorID == MOTOR_ALL || motorID == id)
                analogWrite(config.PIN_PWMS[id], config.invertedPWM ? (255 - value) : value);
    }

public:
    explicit OpenSmart_QuadMotorDriver(const Config& cfg)
        : config(cfg), _directions(0x00) {
        Wire.begin(config.PIN_I2C_SDA, config.PIN_I2C_SCL);
        Wire.beginTransmission(config.I2C_ADDR);
        Wire.write(controlvalue_alloff);
        _status = Wire.endTransmission();
        for (uint8_t pin : config.PIN_PWMS) {
            analogWriteResolution(pin, MotorSpeedResolution);
            analogWriteFrequency(pin, config.frequency);
            pinMode(pin, OUTPUT);
            analogWrite(pin, config.invertedPWM ? 255 : 0);
        }
        DEBUG_PRINTF("OpenSmart_QuadMotorDriver::init: device %s i2c (address=0x%x, sda=%d, scl=%d, status=%d), pwm (resolution=%d, frequency=%d) \n", (_status == 0 ? "connected" : "unresponsive"), config.I2C_ADDR, config.PIN_I2C_SDA, config.PIN_I2C_SCL, _status, MotorSpeedResolution, config.frequency);
    }
    void setSpeed(const MotorSpeed speed, const int motorID = MOTOR_ALL) {
        DEBUG_PRINTF("OpenSmart_QuadMotorDriver::setSpeed: %s -> %d\n", _motorid_to_string(motorID).c_str(), speed);
        applySpeed(motorID, speed);
    }
    void setDirection(const MotorDirection direction, const int motorID = MOTOR_ALL) {
        DEBUG_PRINTF("OpenSmart_QuadMotorDriver::setDirection: %s -> %s\n", _motorid_to_string(motorID).c_str(), (direction == MOTOR_CLOCKWISE) ? "CLOCKWISE" : "ANTICLOCKWISE");
        applyDirections(motorID, (direction == MOTOR_CLOCKWISE) ? MOTOR_CONTROL_CLOCKWISE : MOTOR_CONTROL_ANTICLOCKWISE);
    }
    void stop(const int motorID = MOTOR_ALL) {
        DEBUG_PRINTF("OpenSmart_QuadMotorDriver::stop: %s\n", _motorid_to_string(motorID).c_str());
        applyDirections(motorID, MOTOR_CONTROL_OFF);
    }

private:
    static String _motorid_to_string(const int motorId) {
        switch (motorId) {
            case -1: return "ABCD";
            case 0: return "A";
            case 1: return "B";
            case 2: return "C";
            case 3: return "D";
            default: return "UNDEFINED";
        }
    }
};

// -----------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------
