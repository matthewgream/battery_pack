
// -----------------------------------------------------------------------------------------------

#include <Arduino.h>

#define DEBUG
#ifdef DEBUG
    bool DEBUG_AVAILABLE = true;
    #define DEBUG_START(...) Serial.begin (DEFAULT_SERIAL_BAUD); if (DEBUG_AVAILABLE) delay (5*1000L);
    #define DEBUG_END(...) Serial.flush (); Serial.end ()
    #define DEBUG_PRINTF(...) if (DEBUG_AVAILABLE) Serial.printf (__VA_ARGS__)
#else
    #define DEBUG_START(...)
    #define DEBUG_END(...)
    #define DEBUG_PRINTF(...)
#endif

// -----------------------------------------------------------------------------------------------

#define NIBBLE_TO_HEX_CHAR(nibble) ((char) ((nibble) < 10 ? '0' + (nibble) : 'A' + ((nibble) - 10)))
#define BYTE_TO_HEX(byte) NIBBLE_TO_HEX_CHAR ((byte) >> 4), NIBBLE_TO_HEX_CHAR ((byte) & 0x0F)

// -----------------------------------------------------------------------------------------------

typedef unsigned long interval_t;
typedef unsigned long counter_t;

// -----------------------------------------------------------------------------------------------

#include <array>
#include <type_traits>
#include <cstddef>

template <typename T, int WINDOW = 16>
class MovingAverage {
    static_assert (std::is_arithmetic_v <T>, "T must be an arithmetic type");
    static_assert (WINDOW > 0, "WINDOW size must be positive");
    std::array <T, WINDOW> values {};
    T sum = T (0);
    std::size_t index = 0, count = 0;

public:
    T update (const T value) {
        if (count == WINDOW) sum -= values [index]; else count ++;
        values [index] = value; index = (index + 1) % WINDOW;
        sum += value;
        return  sum / static_cast <T> (count);
    }
};

// -----------------------------------------------------------------------------------------------

#include <Arduino.h>
#include <algorithm>

class PidController {
    const float _Kp, _Ki, _Kd;
    float _p, _i = 0.0f, _d, _e = 0.0f;
    interval_t _t = 0;

public:
    PidController (const float kp, const float ki, const float kd) : _Kp (kp), _Ki (ki), _Kd (kd) {}
    float apply (const float setpoint, const float current) {
        const interval_t t = millis ();
        const float d = (t - _t) / 1000.0f;
        const float e = setpoint - current;
        _p = _Kp * e;
        _i = std::clamp (_i + (_Ki * e * d), -100.0f, 100.0f);
        _d = _Kd * (d > 0.0f ? (e - _e) / d : 0.0f);
        _t = t;
        _e = e;
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
    interval_t _seconds = 0;
    counter_t _number = 0;

public:
    ActivationTracker () {}
    interval_t seconds () const { return _seconds; }
    counter_t  number () const { return _number; }
    ActivationTracker& operator ++ (int) {
        _seconds = millis () / 1000;
        _number ++;
        return *this;
    }
    virtual void serialize (JsonObject &obj) const {
        obj ["last"] = _seconds;
        obj ["count"] = _number;
    }
    virtual void serialize (JsonObject &&obj) const { // weird C++ stuff
        obj ["last"] = _seconds;
        obj ["count"] = _number;
    }
};

class ActivationTrackerWithDetail: public ActivationTracker {
    String _detail;

public:
    ActivationTrackerWithDetail () {}
    ActivationTrackerWithDetail& operator += (const String& detail) {
        ActivationTracker::operator++ (1);
        _detail = detail;
        return *this;
    }
    void serialize (JsonObject &obj) const override {
        ActivationTracker::serialize (obj);
        if (!_detail.isEmpty ())
            obj ["detail"] = _detail.c_str ();
    }
    void serialize (JsonObject &&obj) const override { // weird C++ stuff
        ActivationTracker::serialize (obj);
        if (!_detail.isEmpty ())
            obj ["detail"] = _detail.c_str ();
    }
};

// -----------------------------------------------------------------------------------------------

#include <type_traits>

template <typename T>
inline T map (const T x, const T in_min, const T in_max, const T out_min, const T out_max) {
    static_assert (std::is_arithmetic_v <T>, "T must be an arithmetic type");
    return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

// -----------------------------------------------------------------------------------------------

float steinharthart_calculator (const float VALUE, const float VALUE_MAX, const float REFERENCE_RESISTANCE, const float NOMINAL_RESISTANCE, const float NOMINAL_TEMPERATURE) {
    static constexpr float BETA = 3950.0;
    const float STEINHART = (log ((REFERENCE_RESISTANCE / ((VALUE_MAX / VALUE) - 1.0)) / NOMINAL_RESISTANCE) / BETA) + (1.0 / (NOMINAL_TEMPERATURE + 273.15));
    return (1.0 / STEINHART) - 273.15;
}

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
        DEBUG_PRINTF ("exception: %s\n", e.what ());
    } catch (...) {
        DEBUG_PRINTF ("exception: unknown\n");
    }
}

// -----------------------------------------------------------------------------------------------

#include <type_traits>

template <typename T>
class Singleton {
    static_assert (std::is_class_v <T>, "T must be a class type");
    inline static T* _instance = nullptr; 
public:
    inline static T* instance () { return _instance; }
    Singleton (T* t) {
      if (_instance != nullptr)
          throw std::runtime_error ("duplicate Singleton initializer");
      _instance = t;
    } 
    virtual ~Singleton () { _instance = nullptr; }
};

// -----------------------------------------------------------------------------------------------
