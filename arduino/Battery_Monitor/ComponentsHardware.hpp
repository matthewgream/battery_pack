
// -----------------------------------------------------------------------------------------------

#include <Arduino.h>
#include <array>

// https://deepbluembedded.com/arduino-cd74hc4067-analog-multiplexer-library-code/
class MuxInterface_CD74HC4067 { // technically this is ADC as well due to PIN_SIG
public:
    static inline constexpr int AddrWidth = 4;
    static inline constexpr int CHANNELS = 16;

    typedef struct {
        int PIN_EN, PIN_SIG;
        std::array <int, AddrWidth> PIN_ADDR;
    } Config;

private:
    const Config config;

public:
    MuxInterface_CD74HC4067 (const Config& cfg) : config (cfg) { 
        // not necessary to explicitly set pinModes
        pinMode (config.PIN_EN, OUTPUT);
        digitalWrite (config.PIN_EN, HIGH); // OFF
        pinMode (config.PIN_ADDR [0], OUTPUT);
        pinMode (config.PIN_ADDR [1], OUTPUT);
        pinMode (config.PIN_ADDR [2], OUTPUT);
        pinMode (config.PIN_ADDR [3], OUTPUT);
        pinMode (config.PIN_SIG, INPUT);
    }
    uint16_t get (const int channel) const {
        digitalWrite (config.PIN_ADDR [0], channel & 1);
        digitalWrite (config.PIN_ADDR [1], (channel >> 1) & 1);
        digitalWrite (config.PIN_ADDR [2], (channel >> 2) & 1);
        digitalWrite (config.PIN_ADDR [3], (channel >> 3) & 1);
        delay (10);
        return analogRead (config.PIN_SIG);
    }
    void enable (const bool state = true) {
        digitalWrite (config.PIN_EN, state ? LOW : HIGH); // Active-LOW
    }
};


// -----------------------------------------------------------------------------------------------

#include <Wire.h>
#include <array>

// https://www.aliexpress.com/item/1005002430639515.html
class OpenSmart_QuadMotorDriver {

public:
    static inline constexpr int MotorCount = 4;
    static inline constexpr uint8_t I2cAddress = 0x20;
    typedef std::array <int, MotorCount> MotorSpeedPins;

    typedef struct {
        uint8_t I2C_ADDR;
        int PIN_I2C_SDA, PIN_I2C_SCL;
        MotorSpeedPins PIN_PWMS;
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
    typedef uint8_t MotorSpeed;

private:
    const Config config;
    uint8_t _directions;

    enum MotorControl {
        MOTOR_CONTROL_OFF = 0x00,
        MOTOR_CONTROL_ANTICLOCKWISE = 0x01,
        MOTOR_CONTROL_CLOCKWISE = 0x02
    };
    static constexpr uint8_t controlvalue_alloff = 0x00;
    static constexpr uint8_t encode_controlvalue (const int motorID, const uint8_t directions, const uint8_t value) {
        return (directions & (~(0x03 << (2 * motorID)))) | (value << (2 * motorID));
    }

    void directions_update (const int motorID, const MotorControl value) {
        for (int id = 0; id < MotorCount; id ++)
            if (motorID == MOTOR_ALL || motorID == id)
                _directions = encode_controlvalue (_directions, id, value);
        Wire.beginTransmission (config.I2C_ADDR);
        Wire.write (_directions);
        Wire.endTransmission ();
    }
    void speed_update (const int motorID, const MotorSpeed value) {
        for (int id = 0; id < MotorCount; id ++)
            if (motorID == MOTOR_ALL || motorID == id)
                analogWrite (config.PIN_PWMS [id], value);
    }

public:
    OpenSmart_QuadMotorDriver (const Config& cfg) : config (cfg), _directions (0x00) {
        Wire.setPins (config.PIN_I2C_SDA, config.PIN_I2C_SCL);
        Wire.begin ();
        Wire.beginTransmission (config.I2C_ADDR);
        Wire.write (controlvalue_alloff);
        Wire.endTransmission ();
        for (uint8_t pin : config.PIN_PWMS)
            pinMode (pin, OUTPUT), analogWrite (pin, 0);
    }
    void setSpeed (const MotorSpeed speed, const int motorID = MOTOR_ALL) {
        DEBUG_PRINTF ("OpenSmart_QuadMotorDriver::setSpeed: %d -> %d\n", motorID, speed);
        speed_update (motorID, speed);
    }
    void setDirection (const MotorDirection direction, const int motorID = MOTOR_ALL) {
        DEBUG_PRINTF ("OpenSmart_QuadMotorDriver::setDirection: %d -> %s\n", motorID, (direction == MOTOR_CLOCKWISE) ? "CLOCKWISE" : "ANTICLOCKWISE");
        directions_update (motorID, (direction == MOTOR_CLOCKWISE) ? MOTOR_CONTROL_CLOCKWISE : MOTOR_CONTROL_ANTICLOCKWISE);
    }
    void stop (const int motorID = MOTOR_ALL) {
        DEBUG_PRINTF ("OpenSmart_QuadMotorDriver::stop: %d\n", motorID);
        directions_update (motorID, MOTOR_CONTROL_OFF);
    }
};

// -----------------------------------------------------------------------------------------------
