
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

typedef unsigned long interval_t;

class Intervalable {
    const interval_t _interval;
    interval_t _previous;
public:
    Intervalable (const interval_t interval) : _interval (interval), _previous (0) {}
    explicit operator bool () {                  
        const interval_t current = millis ();
        if (current - _previous > _interval) {
            _previous = current;
            return true;
        }
        return false;
    }
};

// -----------------------------------------------------------------------------------------------

template <typename T> inline T map (const T x, const T in_min, const T in_max, const T out_min, const T out_max) {
    return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

// -----------------------------------------------------------------------------------------------

#endif
