
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

template <typename T>
inline String IntToString (const T n, const int b = 10) {
    static_assert (std::is_integral_v <T>, "T must be an integral type");
    char s [64 + 1]; return String (ltoa (static_cast <long> (n), s, b));
};
template <typename T>
inline String FloatToString (const T n, const int p = 2) {
    static_assert (std::is_floating_point_v <T>, "T must be a floating point type");
    char s [64 + 1]; snprintf (s, sizeof (s) - 1, "%.*f", p, n); return s;
};

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
    size_t count = 0;

public:
    virtual T update (const T value) {
        if (count < WINDOW)
            values [count ++] = value;
        else {
            sum -= values [0];
            std::rotate (values.begin (), values.begin () + 1, values.end ());
            values.back () = value;
        }
        sum += value;
        return sum / static_cast<T> (count);
    }
};

template <typename T, int WINDOW = 16>
class MovingAverageWithValue: public MovingAverage <T, WINDOW> {
    T val = T (0);
    std::function <T(T)> process;
public:
    MovingAverageWithValue (std::function <T(T)> proc = [] (T x) { return x; }): process (proc) {}
    virtual T update (const T value) override {
        val = process (MovingAverage <T, WINDOW>::update (value));
        return val;
    }
    inline MovingAverageWithValue& operator= (const T value) { update (value); return *this; }
    inline operator const T& () const { return val; }
};

inline float round2places (float value) {
    return std::round (value * 100.0f) / 100.0f;
}

// -----------------------------------------------------------------------------------------------

#include <type_traits>

template <typename T>
class Stats {
    static_assert (std::is_arithmetic_v <T>, "T must be an arithmetic type");
    T _sum, _min, _max, _avg = T (0);
    size_t _cnt = 0;
public:
    inline void reset () { _cnt = 0; _avg = T (0); }
    inline Stats <T>& operator+= (const T& val) {
        if (_cnt ++ == 0)
            _sum = val, _min = val, _max = val;
        else {
            _sum += val;
            if (val < _min) _min = val;
            if (val > _max) _max = val;
        }
        return *this;
    }
    inline size_t cnt () const { return _cnt; }
    inline T min () const { return _min; }
    inline T max () const { return _max; }
    inline T avg () const { 
        if (_cnt > 0) {
            const_cast <Stats <T> *> (this)->_avg = _sum / static_cast <T> (_cnt);
            const_cast <Stats <T> *> (this)->_cnt = 0;
        }
        return _avg;
    }
};
template <typename T>
class StatsWithValue: public Stats <T> {
    T _val = T (0);
public:
    inline T val () const { return _val; }
    inline StatsWithValue <T>& operator+= (const T& val) {
        _val = val;
        Stats <T>::operator+= (val);
        return *this;
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
    inline float apply (const float value) {
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
    inline interval_t seconds () const {
        return (millis () - _started) / 1000;
    }
};

// -----------------------------------------------------------------------------------------------

#include <Arduino.h>
#include <ArduinoJson.h>

class ActivationTracker {
    counter_t _number = 0;
    unsigned long _millis = 0;

public:
    ActivationTracker () {}
    inline interval_t seconds () const { return _millis / 1000; }
    inline counter_t  number () const { return _number; }
    ActivationTracker& operator ++ (int) {
        _millis = millis ();
        _number ++;
        return *this;
    }
    template <typename JsonObjectT>
    void serialize (JsonObjectT&& obj) const {
        obj ["last"] = _millis / 1000;
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
    template <typename JsonObjectT>
    void serialize (JsonObjectT&& obj) const {
        ActivationTracker::serialize (obj);
        if (!_detail.isEmpty ())
            obj ["detail"] = _detail;
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

float steinharthart_calculator (const float value, const float value_max, const float resistance_reference, const float resistance_nominal, const float temperature_nominal) {
    static constexpr float beta = 3950.0f, kelvin_constant = 273.15f;
    float resistance = resistance_reference / ((value_max / value) - 1.0f);
    float steinhart = logf (resistance / resistance_nominal) / beta + 1.0f / (temperature_nominal + kelvin_constant);
    return 1.0f / steinhart - kelvin_constant;
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
