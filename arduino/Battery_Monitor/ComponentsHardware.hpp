
// -----------------------------------------------------------------------------------------------

#include <Arduino.h>

class MuxInterface_CD74HC4067 { // technically this is ADC as well due to PIN_SIG

public:
    typedef struct {
        const int PIN_S0, PIN_S1, PIN_S2, PIN_S3, PIN_SIG;
    } Config;
    static inline constexpr int CHANNELS = 16;
private:
    const Config& config;

public:
    MuxInterface_CD74HC4067 (const Config& cfg) : config (cfg) {}
    void configure () {
        pinMode (config.PIN_S0, OUTPUT);
        pinMode (config.PIN_S1, OUTPUT);
        pinMode (config.PIN_S2, OUTPUT);
        pinMode (config.PIN_S3, OUTPUT);
        pinMode (config.PIN_SIG, INPUT);
    }
    uint16_t get (const int channel) const {
        digitalWrite (config.PIN_S0, channel & 1);
        digitalWrite (config.PIN_S1, (channel >> 1) & 1);
        digitalWrite (config.PIN_S2, (channel >> 2) & 1);
        digitalWrite (config.PIN_S3, (channel >> 3) & 1);
        delay (10);
        return analogRead (config.PIN_SIG);
    }
};


// -----------------------------------------------------------------------------------------------

#include <Wire.h>
#include <array>

class OpenSmart_QuadMotorDriver {

public:
    static inline constexpr int MotorCount = 4;
    static inline constexpr int I2cAddress = 0x20;
    enum MotorID {
        MOTOR_A = 0,
        MOTOR_B = 1,
        MOTOR_C = 2,
        MOTOR_D = 3
    };
    enum MotorDirection {
        MOTOR_CLOCKWISE = 0,
        MOTOR_ANTICLOCKWISE = 1
    };
    typedef std::array <int, MotorCount> MotorSpeedPins;

private:
    enum MotorControl {
        MOTOR_CONTROL_OFF = 0x00,
        MOTOR_CONTROL_ANTICLOCKWISE = 0x01,
        MOTOR_CONTROL_CLOCKWISE = 0x02
    };
    static constexpr uint8_t encode_controlvalue (const int motorID, const uint8_t directions, const uint8_t value) {
        return (directions & (~(0x03 << (2 * motorID)))) | (value << (2 * motorID));
    }

    const uint8_t _i2c;
    const MotorSpeedPins _pwms;
    uint8_t _directions;

    void directions_update (const int motorID, const uint8_t value) {
        for (int id = 0; id < MotorCount; id ++)
            if (motorID == -1 || motorID == id)
                _directions = encode_controlvalue (_directions, id, value);
        Wire.beginTransmission (_i2c);
        Wire.write (_directions);
        Wire.endTransmission ();
    }
    void speed_update (const int motorID, const uint8_t value) {
        for (int id = 0; id < MotorCount; id ++)
            if (motorID == -1 || motorID == id)
                analogWrite (_pwms [id], value);
    }

public:
    OpenSmart_QuadMotorDriver (const uint8_t i2c, const MotorSpeedPins& pwms) : _i2c (i2c), _pwms (pwms), _directions (0x00) {
        for (uint8_t pin : _pwms)
            pinMode (pin, OUTPUT), digitalWrite (pin, LOW);
        Wire.begin ();
        stop ();
    }
    void setSpeed (const int speed, const int motorID = -1) {
        DEBUG_PRINTF ("OpenSmart_QuadMotorDriver::setSpeed: %d -> %d\n", motorID, (speed > 255) ? (uint8_t) 255 : static_cast <uint8_t> (speed));
        speed_update (motorID, (speed > 255) ? (uint8_t) 255 : static_cast <uint8_t> (speed));
    }
    void setDirection (const int direction, const int motorID = -1) {
        DEBUG_PRINTF ("OpenSmart_QuadMotorDriver::setDirection: %d -> %s\n", motorID, (direction == MOTOR_CLOCKWISE) ? "CLOCKWISE" : "ANTICLOCKWISE");
        directions_update (motorID, (direction == MOTOR_CLOCKWISE) ? MOTOR_CONTROL_CLOCKWISE : MOTOR_CONTROL_ANTICLOCKWISE);
    }
    void stop (const int motorID = -1) {
        DEBUG_PRINTF ("OpenSmart_QuadMotorDriver::stop: %d\n", motorID);
        directions_update (motorID, MOTOR_CONTROL_OFF);
    }
};

// -----------------------------------------------------------------------------------------------
