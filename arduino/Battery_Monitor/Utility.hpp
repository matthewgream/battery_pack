
#ifndef __UTILITY_HPP__
#define __UTILITY_HPP__

// -----------------------------------------------------------------------------------------------

#include <array>

class MovingAverage {
    static constexpr int WINDOW_SIZE = 16;
    std::array <float, WINDOW_SIZE> values;
    int index = 0, count = 0;
public:
    float update (const float value) {
        values [index] = value;
        index = (index + 1) % WINDOW_SIZE;
        if (count < WINDOW_SIZE)
            count ++;
        float sum = 0;
        for (int i = 0; i < count; i ++) 
           sum += values [i];
        return sum / count;
    }
};

// -----------------------------------------------------------------------------------------------

#include <Arduino.h>
#include <algorithm>

class PidController {
private:
    const float _Kp, _Ki, _Kd;
    float _p, _i = 0.0f, _d;
    float _lastError = 0.0f;
    unsigned long _lastTime = 0;
public:
    PidController (const float kp, const float ki, const float kd) : _Kp (kp), _Ki (ki), _Kd (kd) {} 
    float process (const float setpoint, const float current) {
        const unsigned long time = millis ();
        const float delta = (time - _lastTime) / 1000.0f;
        const float error = setpoint - current;
        _p = _Kp * error;
        _i = std::clamp (_i + (_Ki * error * delta), -100.0f, 100.0f);
        _d = _Kd * (delta > 0.0f ? (error - _lastError) / delta : 0.0f);
        _lastTime = time;
        _lastError = error;
        return _p + _i + _d;
    }
};

// -----------------------------------------------------------------------------------------------

class AlphaSmoothing {
    const float _alpha;
    float _value = 0.0f;
public:    
    AlphaSmoothing (const float alpha) : _alpha (alpha) {}
    float process (const float value) {
        return (_value = (_alpha * value + (1.0f - _alpha) * _value));
    }
};

// -----------------------------------------------------------------------------------------------

#include <Arduino.h>

typedef unsigned long interval_t;

class Intervalable {
    const interval_t _interval;
    interval_t _previous;
public:
    Intervalable (const interval_t interval) : _interval (interval), _previous (0) {}
    operator bool () {                  
        const interval_t current = millis ();
        if (current - _previous > _interval) {
            _previous = current;
            return true;
        }
        return false;
    }
};

// -----------------------------------------------------------------------------------------------

#include <Arduino.h>

class Uptime {
    const unsigned long _started;
public:
    Uptime () : _started (millis ()) {}
    unsigned long seconds () const {
        return (millis () - _started) / 1000;
    }
};

class Upstamp {
    unsigned long _seconds = 0, _number = 0;
public:
    Upstamp () {}
    unsigned long seconds () const { return _seconds; }
    unsigned long number () const { return _number; }
    Upstamp& operator ++ (int) {
        _seconds = millis () / 1000;
        _number ++;
        return *this;
    }                  
};

// -----------------------------------------------------------------------------------------------

template <typename T> inline T map (const T x, const T in_min, const T in_max, const T out_min, const T out_max) {
    return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

// -----------------------------------------------------------------------------------------------

#include <esp_mac.h>

#define NIBBLE_TO_HEX_CHAR(nibble) ((char) ((nibble) < 10 ? '0' + (nibble) : 'A' + ((nibble) - 10)))
#define BYTE_TO_HEX(byte) NIBBLE_TO_HEX_CHAR ((byte) >> 4), NIBBLE_TO_HEX_CHAR ((byte) & 0x0F)

String mac_address (void) {
    uint8_t macaddr [6];
    esp_read_mac (macaddr, ESP_MAC_WIFI_STA);
    const char macstr [12 + 1] = { BYTE_TO_HEX (macaddr [0]), BYTE_TO_HEX (macaddr [1]), BYTE_TO_HEX (macaddr [2]), BYTE_TO_HEX (macaddr [3]), BYTE_TO_HEX (macaddr [4]), BYTE_TO_HEX (macaddr [5]), '\0' };
    return String (macstr);
}

// -----------------------------------------------------------------------------------------------

float steinharthart_calculator (const float VALUE, const float VALUE_MAX, const float REFERENCE_RESISTANCE, const float NOMINAL_RESISTANCE, const float NOMINAL_TEMPERATURE) {
    static constexpr float BETA = 3950.0;
    const float STEINHART = (log ((REFERENCE_RESISTANCE / ((VALUE_MAX / VALUE) - 1.0)) / NOMINAL_RESISTANCE) / BETA) + (1.0 / (NOMINAL_TEMPERATURE + 273.15));
    return (1.0 / STEINHART) - 273.15;
}

// -----------------------------------------------------------------------------------------------

#endif
