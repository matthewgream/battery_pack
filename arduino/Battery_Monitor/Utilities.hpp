
// -----------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------

#include <Arduino.h>

#define DEBUG
#ifdef DEBUG
    #define DEBUG_START(...) Serial.begin (DEFAULT_SERIAL_BAUD); delay (5*1000L);
    #define DEBUG_END(...) Serial.flush (); Serial.end ();
    #define DEBUG_PRINTF(...) Serial.printf (__VA_ARGS__)
#else
    #define DEBUG_START(...)
    #define DEBUG_END(...)
    #define DEBUG_PRINTF(...) do {} while (0)
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
#include <functional>
#include <numeric>

template <typename T, int WINDOW = 16>
class MovingAverage {
    static_assert (std::is_arithmetic_v <T>, "T must be an arithmetic type");
    static_assert (WINDOW > 0, "WINDOW size must be positive");
    using S = std::conditional_t <std::is_integral_v <T>, std::conditional_t <sizeof (T) < sizeof (int32_t), int32_t, int64_t>, double>;
    std::array <T, WINDOW> values;
    S sum = S (0);
    size_t count = 0;

public:
    virtual T update (const T value) {
        if (count < WINDOW) {
            values [count] = value;
            sum += static_cast <S> (value);
            count ++;
        } else {
            sum -= static_cast <S> (values [0]);
            for (size_t i = 1; i < WINDOW; i ++)
                values [i - 1] = values [i];
            values [WINDOW - 1] = value;
            sum += static_cast <S> (value);
        }
        return static_cast <T> (sum / static_cast <S> (count));
    }
};

template <template <typename, int> class BaseAverage, typename T = float, int WINDOW = 16>
class TypeOfAverageWithValue: public BaseAverage <T, WINDOW> {
    T val = T (0);
    std::function <T (T)> process;
public:
    TypeOfAverageWithValue (std::function <T (T)> proc = [] (T x) { return x; }): process (proc) {}
    T update (const T value) override {
        val = process (BaseAverage <T, WINDOW>::update (value));
        return val;
    }
    inline TypeOfAverageWithValue& operator= (const T value) { update (value); return *this; }
    inline operator const T& () const { return val; }
};

template <typename T, int WINDOW = 16>
using MovingAverageWithValue = TypeOfAverageWithValue <MovingAverage, T, WINDOW>;

// -----------------------------------------------------------------------------------------------

inline float round2places (float value) {
    return std::round (value * 100.0f) / 100.0f;
}

// -----------------------------------------------------------------------------------------------

#include <type_traits>

template <typename T>
class Stats {
    static_assert (std::is_arithmetic_v <T>, "T must be an arithmetic type");
    using S = std::conditional_t <std::is_integral_v <T>, std::conditional_t <sizeof (T) < sizeof (int32_t), int32_t, int64_t>, double>;
    S _sum = S (0);
    T _min, _max, _avg = T (0);
    size_t _cnt = 0;
public:
    inline void reset () { _cnt = 0; _avg = T (0); }
    inline Stats <T>& operator+= (const T& val) {
        if (_cnt ++ == 0)
            _sum = static_cast <S> (_min = _max = val);
        else {
            _sum += static_cast <S> (val);
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
            const_cast <Stats <T> *> (this)->_avg = static_cast <T> (_sum / static_cast <S> (_cnt));
            const_cast <Stats <T> *> (this)->_cnt = 0;
        }
        return _avg;
    }
};
template <template <typename> class BaseStats, typename T>
class TypeOfStatsWithValue: public BaseStats <T> {
    T _val = T (0);
public:
    inline T val () const { return _val; }
    inline TypeOfStatsWithValue& operator+= (const T& val) {
        _val = val;
        BaseStats <T>::operator+= (val);
        return *this;
    }
};

template <typename T>
using StatsWithValue = TypeOfStatsWithValue <Stats, T>;

// -----------------------------------------------------------------------------------------------

#include <Arduino.h>
#include <algorithm>

template <typename T>
class PidController {
    const T _Kp, _Ki, _Kd;
    T _p, _i = T (0), _d, _e = T (0);
    interval_t _t = 0;

public:
    PidController (const T kp, const T ki, const T kd) : _Kp (kp), _Ki (ki), _Kd (kd) {}
    T apply (const T setpoint, const T current) {
        const interval_t t = millis ();
        const T d = (t - _t) / 1000.0;
        const T e = setpoint - current;
        _p = _Kp * e;
        _i = std::clamp (_i + (_Ki * e * d), -100.0, 100.0);
        _d = _Kd * (d > 0.0 ? (e - _e) / d : 0.0);
        _t = t;
        _e = e;
        return _p + _i + _d;
    }
};

// -----------------------------------------------------------------------------------------------

template <typename T>
class AlphaSmoothing {
    const T _alpha;
    T _value = T (0);

public:
    AlphaSmoothing (const T alpha) : _alpha (alpha) {}
    inline T apply (const T value) {
        return (_value = (_alpha * value + (1.0 - _alpha) * _value));
    }
};

// -----------------------------------------------------------------------------------------------

#include <Arduino.h>

class Intervalable {
    interval_t _interval, _previous;

public:
    Intervalable (const interval_t interval = 0) : _interval (interval), _previous (0) {}
    operator bool () {
        const interval_t current = millis ();
        if (current - _previous > _interval) {
            _previous = current;
            return true;
        }
        return false;
    }
    void reset (const interval_t interval = std::numeric_limits <interval_t>::max ()) {
        if (interval != std::numeric_limits <interval_t>::max ())
            _interval = interval;
        _previous = millis ();      
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

#include <array>
#include <cstdlib>

template <typename T>
using vector4 = std::array <T, 4>;
template <typename T>
using matrix4 = vector4 <vector4 <T>>;

String gaussian_solve (matrix4 <double>& matrix, vector4 <double>& result) {
    static constexpr double DETERMINANT_DEMINIMUS = 1e-10;

    const double determinant = matrix [0][0] * (
        matrix [1][1] * matrix [2][2] * matrix [3][3] + 
        matrix [1][2] * matrix [2][3] * matrix [3][1] + 
        matrix [1][3] * matrix [2][1] * matrix [3][2] - 
        matrix [1][3] * matrix [2][2] * matrix [3][1] - 
        matrix [1][2] * matrix [2][1] * matrix [3][3] - 
        matrix [1][1] * matrix [2][3] * matrix [3][2]
    );
    if (std::abs (determinant) < DETERMINANT_DEMINIMUS)
        return "matrix is singular/near-singular, determinant: " + FloatToString (determinant, 12);

    for (size_t i = 0; i < 4; i ++) {
        for (size_t j = i + 1; j < 4; j ++) {
            const double factor = matrix [j][i] / matrix [i][i];
            for (size_t k = i; k < 4; k ++)
                matrix [j][k] -= factor * matrix [i][k];
            result [j] -= factor * result [i];
        }
    }
    for (int i = 4 - 1; i >= 0; i --) {
        for (int j = i + 1; j < 4; j ++)
            result [i] -= matrix [i][j] * result [j];
        result [i] /= matrix [i][i];
    }
    return String ();
}

String gaussian_solve (matrix4 <double>& XtX, vector4 <double>& XtY, vector4 <double>& result) {
    static constexpr double CONDITION_DEMAXIMUS = 1e15, DETERMINANT_DEMINIMUS = 1e-10;

    double max_singular = std::numeric_limits <double>::min (), min_singular = std::numeric_limits <double>::max ();
    for (int i = 0; i < 4; i ++) {
        double sum_singular = 0;
        for (int j = 0; j < 4; j ++)
            sum_singular += std::abs (XtX [i][j]);
        if (sum_singular > max_singular) max_singular = sum_singular;
        if (sum_singular < min_singular) min_singular = sum_singular;
    }
    const double condition_number = max_singular / min_singular;
    if (condition_number > CONDITION_DEMAXIMUS)
        return "matrix ill-conditioned, condition number estimate: " + FloatToString (condition_number, 12);
    const double determinant = 
          XtX[0][0] * (XtX[1][1] * XtX[2][2] * XtX[3][3] + XtX[1][2] * XtX[2][3] * XtX[3][1] + XtX[1][3] * XtX[2][1] * XtX[3][2] - XtX[1][3] * XtX[2][2] * XtX[3][1] - XtX[1][2] * XtX[2][1] * XtX[3][3] - XtX[1][1] * XtX[2][3] * XtX[3][2])
        - XtX[0][1] * (XtX[1][0] * XtX[2][2] * XtX[3][3] + XtX[1][2] * XtX[2][3] * XtX[3][0] + XtX[1][3] * XtX[2][0] * XtX[3][2] - XtX[1][3] * XtX[2][2] * XtX[3][0] - XtX[1][2] * XtX[2][0] * XtX[3][3] - XtX[1][0] * XtX[2][3] * XtX[3][2])
        + XtX[0][2] * (XtX[1][0] * XtX[2][1] * XtX[3][3] + XtX[1][1] * XtX[2][3] * XtX[3][0] + XtX[1][3] * XtX[2][0] * XtX[3][1] - XtX[1][3] * XtX[2][1] * XtX[3][0] - XtX[1][1] * XtX[2][0] * XtX[3][3] - XtX[1][0] * XtX[2][3] * XtX[3][1])
        - XtX[0][3] * (XtX[1][0] * XtX[2][1] * XtX[3][2] + XtX[1][1] * XtX[2][2] * XtX[3][0] + XtX[1][2] * XtX[2][0] * XtX[3][1] - XtX[1][2] * XtX[2][1] * XtX[3][0] - XtX[1][1] * XtX[2][0] * XtX[3][2] - XtX[1][0] * XtX[2][2] * XtX[3][1]);
    if (std::abs (determinant) < DETERMINANT_DEMINIMUS)
        return "matrix is singular/near-singular, determinant: " + FloatToString (determinant, 12);

    for (int i = 0; i < 4; i ++) {
        int max_row = i;
        for (int j = i + 1; j < 4; j ++)
            if (std::abs (XtX [j][i]) > std::abs (XtX [max_row][i]))
                max_row = j;
        if (max_row != i)
            std::swap (XtX [i], XtX [max_row]), std::swap (XtY [i], XtY [max_row]);
        for (int j = i + 1; j < 4; j ++) {
            const double factor = XtX [j][i] / XtX [i][i];
            for (int k = i; k < 4; k ++)
                XtX [j][k] -= factor * XtX [i][k];
            XtY [j] -= factor * XtY [i];
        }
    }
    for (int i = 3; i >= 0; i --) {
        result [i] = XtY [i];
        for (int j = i + 1; j < 4; j ++)
            result [i] -= XtX [i][j] * result [j];
        result [i] /= XtX [i][i];
    }
    return String ();      
}

// -----------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------
