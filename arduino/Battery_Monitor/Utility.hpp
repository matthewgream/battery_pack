
#ifndef __UTILITY_HPP__
#define __UTILITY_HPP__

// -----------------------------------------------------------------------------------------------

#include <Arduino.h>

//#define DEBUG
#ifdef DEBUG
    bool DEBUG_AVAILABLE = false;
    #define DEBUG_START(...) Serial.begin (DEFAULT_SERIAL_BAUD); DEBUG_AVAILABLE = !Serial ? false : true;
    #define DEBUG_END(...) Serial.flush (); Serial.end ()
    #define DEBUG_PRINT(...) if (DEBUG_AVAILABLE) Serial.print (__VA_ARGS__)
    #define DEBUG_PRINTLN(...) if (DEBUG_AVAILABLE) Serial.println (__VA_ARGS__)
#else
    #define DEBUG_START(...)
    #define DEBUG_END(...)
    #define DEBUG_PRINT(...)
    #define DEBUG_PRINTLN(...)
#endif

// -----------------------------------------------------------------------------------------------

typedef unsigned long interval_t;
typedef unsigned long counter_t;

// -----------------------------------------------------------------------------------------------

#include <array>
#include <type_traits>
#include <cstddef>

template <typename T, int W = 16>
class MovingAverage {
    static_assert (std::is_arithmetic_v <T>, "T must be an arithmetic type");
    static_assert (W > 0, "Window size must be positive");
    std::array <T, W> V {};
    T S = T (0);
    std::size_t I = 0, C = 0;

public:
    T update (const T X) {
        if (C == W) S -= V [I];
        else C ++;
        V [I] = X;
        S += X;
        I = (I + 1) % W;
        return  S / static_cast <T> (C);
    }
};

// -----------------------------------------------------------------------------------------------

#include <Arduino.h>
#include <algorithm>

class PidController {
    const float _Kp, _Ki, _Kd;
    float _p, _i = 0.0f, _d;
    float _lastError = 0.0f;
    interval_t _lastTime = 0;

public:
    PidController (const float kp, const float ki, const float kd) : _Kp (kp), _Ki (ki), _Kd (kd) {}
    float apply (const float setpoint, const float current) {
        const interval_t time = millis ();
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
    float apply (const float value) {
        return (_value = (_alpha * value + (1.0f - _alpha) * _value));
    }
};

// -----------------------------------------------------------------------------------------------

#include <Arduino.h>

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
    const interval_t _started;

public:
    Uptime () : _started (millis ()) {}
    interval_t seconds () const {
        return (millis () - _started) / 1000;
    }
};

// -----------------------------------------------------------------------------------------------

#include <Arduino.h>
#include <ArduinoJson.h>

class ActivationTracker {
    interval_t _seconds = 0, _number = 0;

public:
    ActivationTracker () {}
    interval_t seconds () const { return _seconds; }
    interval_t  number () const { return _number; }
    ActivationTracker& operator ++ (int) {
        _seconds = millis () / 1000;
        _number ++;
        return *this;
    }
    void serialize (JsonObject &obj) const {
        obj ["last"] = _seconds;
        obj ["number"] = _number;
    }
    void serialize (JsonObject &&obj) const { // XXX not 100% sure about this
        obj ["last"] = _seconds;
        obj ["number"] = _number;
    }
};

// -----------------------------------------------------------------------------------------------

template <typename T>
inline T map (const T x, const T in_min, const T in_max, const T out_min, const T out_max) {
    static_assert (std::is_arithmetic_v <T>, "T must be an arithmetic type");
    return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

// -----------------------------------------------------------------------------------------------

#include <esp_mac.h>

String mac_address (void) {
#define NIBBLE_TO_HEX_CHAR(nibble) ((char) ((nibble) < 10 ? '0' + (nibble) : 'A' + ((nibble) - 10)))
#define BYTE_TO_HEX(byte) NIBBLE_TO_HEX_CHAR ((byte) >> 4), NIBBLE_TO_HEX_CHAR ((byte) & 0x0F)
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

#include <ArduinoJson.h>

class JsonCollector {
    JsonDocument doc;

public:
    JsonCollector (const String &type, const String &time) {
        doc ["type"] = type;
        doc ["time"] = time;
    }
    JsonDocument& document () { return doc; }
    operator String () const {
        String output;
        serializeJson (doc, output);
        return output;
    }
};

// -----------------------------------------------------------------------------------------------

#include <type_traits>
#include <exception>
#include <utility>

template <typename F>
void exception_catcher (F&& f) {
    static_assert (std::is_invocable_v <F>, "F must be an invocable type");
    try {
        std::forward <F> (f) ();
    } catch (const std::exception& e) {
        DEBUG_PRINT ("exception: "); DEBUG_PRINTLN (e.what ());
    } catch (...) {
        DEBUG_PRINT ("exception: "); DEBUG_PRINTLN ("unknown");
    }
}

// -----------------------------------------------------------------------------------------------

template <typename T>
class Singleton {
    static_assert (std::is_class_v <T>, "T must be a class type");
    inline static T* _instance = nullptr; 
public:
    inline static T* instance () { return _instance; }
    Singleton (T* t) { _instance = t; } 
    virtual ~Singleton () { _instance = nullptr; }
};

// -----------------------------------------------------------------------------------------------

#endif