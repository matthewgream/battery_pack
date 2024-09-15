
#ifndef __UTILITY_HPP__
#define __UTILITY_HPP__

// -----------------------------------------------------------------------------------------------

#include <array>

class MovingAverage {
    static constexpr int WINDOW_SIZE = 16;
    std::array <float, WINDOW_SIZE> values;
    int index = 0, count = 0;
public:
    float update (float value) {
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
    float _Kp, _Ki, _Kd;
    float _p, _i = 0.0f, _d;
    float _lastError = 0.0f;
    unsigned long _lastTime = 0;
public:
    PidController (float kp, float ki, float kd) : _Kp (kp), _Ki (ki), _Kd (kd) {} 
    float process (float setpoint, float current) {
        unsigned long time = millis ();
        float delta = (time - _lastTime) / 1000.0f;
        float error = setpoint - current;
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
    float _alpha;
    float _value = 0.0f;
public:    
    AlphaSmoothing (float alpha) : _alpha (alpha) {}
    float process (float value) {
        return (_value = (_alpha * value + (1.0f - _alpha) * _value));
    }
};

// -----------------------------------------------------------------------------------------------

template <typename T> inline T map (T x, T in_min, T in_max, T out_min, T out_max) {
    return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

// -----------------------------------------------------------------------------------------------

#endif
