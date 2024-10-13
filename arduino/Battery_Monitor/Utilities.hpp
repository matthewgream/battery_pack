
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

#include <Arduino.h>

template <typename T>
inline String ArithmeticToString (const T n, const int x = -1, const bool t = false) {
    static_assert (std::is_arithmetic_v <T>, "T must be an arithmetic type");
    char s [64 + 1];
    if constexpr (std::is_integral_v <T>)
        return (x == -1 || x == 10) ? String (n) : String (ltoa (static_cast <long> (n), s, x));
    else if constexpr (std::is_floating_point_v <T>) {
        dtostrf (n, 0, x == -1 ? 2 : x, s);
        if (t) {
            char *d = nullptr, *e = s;
            while (*e != '\0') {
                if (*e == '.')
                    d = e;
                e ++;
            }
            e --;
            if (d)
                while (e > d + 1 && *e == '0') *e -- = '\0';
            else
                *e ++ = '.', *e ++ = '0', *e = '\0';
        }
        return String (s);
    }
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
    explicit TypeOfAverageWithValue (std::function <T (T)> proc = [] (T x) { return x; }): process (proc) {}
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

#include <cmath>

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
    T _min = std::numeric_limits <T>::max (), _max = std::numeric_limits <T>::min (), _avg = T (0);
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
public: // for serialization
    const T _Kp, _Ki, _Kd;
    T _p = T (0), _i = T (0), _d = T (0), _e = T (0);
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
    explicit AlphaSmoothing (const T alpha) : _alpha (alpha) {}
    inline T apply (const T value) {
        return (_value = (_alpha * value + (1.0 - _alpha) * _value));
    }
};

// -----------------------------------------------------------------------------------------------

class Initialisable {
    bool initialised = false;

public:
    operator bool () {
        if (!initialised) {
            initialised = true;
            return false;
        }
        return true;
    }
};

// -----------------------------------------------------------------------------------------------

#include <Arduino.h>

class Intervalable {
    interval_t _interval, _previous;

public:
    explicit Intervalable (const interval_t interval = 0) : _interval (interval), _previous (0) {}
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

class ActivationTracker {
    counter_t _count = 0;
    unsigned long _millis = 0;

public:
    inline interval_t seconds () const { return _millis / 1000; }
    inline counter_t count () const { return _count; }
    ActivationTracker& operator ++ (int) {
        _millis = millis ();
        _count ++;
        return *this;
    }
};

class ActivationTrackerWithDetail: public ActivationTracker {
    String _detail;

public:
    const String& detail () const { return _detail; }
    ActivationTrackerWithDetail& operator += (const String& detail) {
        ActivationTracker::operator++ (1);
        _detail = detail;
        return *this;
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
    explicit Singleton (T* t) {
      assert (_instance == nullptr && "duplicate Singleton initializer");
      _instance = t;
    }
    virtual ~Singleton () { _instance = nullptr; }
};

// -----------------------------------------------------------------------------------------------

#include <array>
#include <cstdlib>

namespace gaussian {

    template <typename T>
    using vector4 = std::array <T, 4>;
    template <typename T>
    using matrix4 = vector4 <vector4 <T>>;

    String solve (matrix4 <double>& matrix, vector4 <double>& result) {
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
            return "matrix is singular/near-singular, determinant: " + ArithmeticToString (determinant, 12);

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

    String solve (matrix4 <double>& XtX, vector4 <double>& XtY, vector4 <double>& result) {
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
            return "matrix ill-conditioned, condition number estimate: " + ArithmeticToString (condition_number, 12);
        const double determinant =
              XtX[0][0] * (XtX[1][1] * XtX[2][2] * XtX[3][3] + XtX[1][2] * XtX[2][3] * XtX[3][1] + XtX[1][3] * XtX[2][1] * XtX[3][2] - XtX[1][3] * XtX[2][2] * XtX[3][1] - XtX[1][2] * XtX[2][1] * XtX[3][3] - XtX[1][1] * XtX[2][3] * XtX[3][2])
            - XtX[0][1] * (XtX[1][0] * XtX[2][2] * XtX[3][3] + XtX[1][2] * XtX[2][3] * XtX[3][0] + XtX[1][3] * XtX[2][0] * XtX[3][2] - XtX[1][3] * XtX[2][2] * XtX[3][0] - XtX[1][2] * XtX[2][0] * XtX[3][3] - XtX[1][0] * XtX[2][3] * XtX[3][2])
            + XtX[0][2] * (XtX[1][0] * XtX[2][1] * XtX[3][3] + XtX[1][1] * XtX[2][3] * XtX[3][0] + XtX[1][3] * XtX[2][0] * XtX[3][1] - XtX[1][3] * XtX[2][1] * XtX[3][0] - XtX[1][1] * XtX[2][0] * XtX[3][3] - XtX[1][0] * XtX[2][3] * XtX[3][1])
            - XtX[0][3] * (XtX[1][0] * XtX[2][1] * XtX[3][2] + XtX[1][1] * XtX[2][2] * XtX[3][0] + XtX[1][2] * XtX[2][0] * XtX[3][1] - XtX[1][2] * XtX[2][1] * XtX[3][0] - XtX[1][1] * XtX[2][0] * XtX[3][2] - XtX[1][0] * XtX[2][2] * XtX[3][1]);
        if (std::abs (determinant) < DETERMINANT_DEMINIMUS)
            return "matrix is singular/near-singular, determinant: " + ArithmeticToString (determinant, 12);

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
}

// -----------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------

#include <ctime>

String getTimeString (time_t timet = 0) {
    struct tm timeinfo;
    char timeString [sizeof ("yyyy-mm-ddThh:mm:ssZ") + 1] = { '\0' };
    if (timet == 0) time (&timet);
    if (gmtime_r (&timet, &timeinfo) != NULL)
        strftime (timeString, sizeof (timeString), "%Y-%m-%dT%H:%M:%SZ", &timeinfo);
    return timeString;
}

// -----------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------
