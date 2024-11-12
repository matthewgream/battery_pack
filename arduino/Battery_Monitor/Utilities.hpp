
// -----------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------

typedef unsigned long interval_t;
typedef unsigned long counter_t;

// -----------------------------------------------------------------------------------------------

template<size_t N>
String BytesToHexString(const uint8_t bytes[], const char* separator = ":") {
    constexpr size_t separator_max = 1;    // change if needed
    if (strlen(separator) > separator_max)
        return String("");
    char buffer[(N * 2) + ((N - 1) * separator_max) + 1] = { '\0' }, *buffer_ptr = buffer;
    for (size_t i = 0; i < N; i++) {
        if (i > 0 && separator[0] != '\0')
            for (const char* separator_ptr = separator; *separator_ptr != '\0';)
                *buffer_ptr++ = *separator_ptr++;
        static const char hex_chars[] = "0123456789abcdef";
        *buffer_ptr++ = hex_chars[(bytes[i] >> 4) & 0x0F];
        *buffer_ptr++ = hex_chars[bytes[i] & 0x0F];
    }
    *buffer_ptr = '\0';
    return String(buffer);
}

// -----------------------------------------------------------------------------------------------

#include <Arduino.h>

template<typename T>
String ArithmeticToString(const T& n, const int x = -1, const bool t = false) {
    static_assert(std::is_arithmetic_v<T>, "T must be an arithmetic type");
    char s[64 + 1];
    if constexpr (std::is_integral_v<T>)
        return (x == -1 || x == 10) ? String(n) : String(ltoa(static_cast<long>(n), s, x));
    else if constexpr (std::is_floating_point_v<T>) {
        dtostrf(n, 0, x == -1 ? 2 : x, s);
        if (t) {
            char *d = nullptr, *e = s;
            while (*e != '\0') {
                if (*e == '.')
                    d = e;
                e++;
            }
            e--;
            if (d)
                while (e > d + 1 && *e == '0') *e-- = '\0';
            else
                *e++ = '.', *e++ = '0', *e = '\0';
        }
        return String(s);
    }
};

// -----------------------------------------------------------------------------------------------

#include <array>
#include <type_traits>
#include <cstddef>
#include <functional>
#include <numeric>

template<typename T, int WINDOW = 16>
class MovingAverage {
public:
    using ProcessFunc = std::function<T(const T&)>;

private:
    static_assert(std::is_arithmetic_v<T>, "T must be an arithmetic type");
    static_assert(WINDOW > 0, "WINDOW size must be positive");
    using U = std::conditional_t<std::is_integral_v<T>, std::conditional_t<sizeof(T) < sizeof(int32_t), int32_t, int64_t>, double>;
    std::array<T, WINDOW> values;
    U sum = U(0), cnt = U(0);
    T val = T(0);
    ProcessFunc postprocessor;

public:
    explicit MovingAverage(ProcessFunc proc = [](const T& t) {
        return t;
    })
        : postprocessor(proc) {}
    const T& update(const T& value) {
        if (cnt < WINDOW) {
            sum = sum + static_cast<U>(value);
            values[cnt++] = value;
        } else {
            sum = sum - static_cast<U>(values[0]) + static_cast<U>(value);
            for (size_t i = 1; i < WINDOW; i++)
                values[i - 1] = values[i];
            values[WINDOW - 1] = value;
        }
        return (val = postprocessor(static_cast<T>(sum / cnt)));
    }
    MovingAverage& operator=(const T& value) {
        update(value);
        return *this;
    }
    inline operator const T&() const {
        return val;
    }
};

// -----------------------------------------------------------------------------------------------

#include <cmath>

inline float round2places(const float& value) {
    return std::round(value * 100.0f) / 100.0f;
}

// -----------------------------------------------------------------------------------------------

#include <type_traits>

template<typename T>
class Stats {
    static_assert(std::is_arithmetic_v<T>, "T must be an arithmetic type");
    using S = std::conditional_t<std::is_integral_v<T>, std::conditional_t<sizeof(T) < sizeof(int32_t), int32_t, int64_t>, double>;
    S _sum = S(0);
    T _min = std::numeric_limits<T>::max(), _max = std::numeric_limits<T>::min(), _avg = T(0);
    size_t _cnt = 0;

public:
    void reset() {
        _cnt = 0;
        _avg = T(0);
    }
    Stats<T>& operator+=(const T& val) {
        if (_cnt++ == 0)
            _sum = static_cast<S>(_min = _max = val);
        else {
            _sum += static_cast<S>(val);
            if (val < _min) _min = val;
            if (val > _max) _max = val;
        }
        return *this;
    }
    inline const size_t& cnt() const {
        return _cnt;
    }
    inline const T& min() const {
        return _min;
    }
    inline const T& max() const {
        return _max;
    }
    inline const T& avg() const {
        if (_cnt > 0) {
            const_cast<Stats<T>*>(this)->_avg = static_cast<T>(_sum / static_cast<S>(_cnt));
            const_cast<Stats<T>*>(this)->_cnt = 0;
        }
        return _avg;
    }
};
template<template<typename> class BaseStats, typename T>
class TypeOfStatsWithValue : public BaseStats<T> {
    T _val = T(0);

public:
    inline const T& val() const {
        return _val;
    }
    TypeOfStatsWithValue& operator+=(const T& val) {
        _val = val;
        BaseStats<T>::operator+=(val);
        return *this;
    }
};

template<typename T>
using StatsWithValue = TypeOfStatsWithValue<Stats, T>;

// -----------------------------------------------------------------------------------------------

#include <Arduino.h>
#include <algorithm>

template<typename T>
class PidController {
public:    // for serialization
    const T _Kp, _Ki, _Kd;
    T _p = T(0), _i = T(0), _d = T(0), _e = T(0);
    interval_t _t = 0;

public:
    PidController(const T& kp, const T& ki, const T& kd)
        : _Kp(kp), _Ki(ki), _Kd(kd) {}
    T apply(const T& setpoint, const T& current) {
        const interval_t t = millis();
        const T d = (t - _t) / 1000.0;
        const T e = setpoint - current;
        _p = _Kp * e;
        _i = std::clamp(_i + (_Ki * e * d), -100.0, 100.0);
        _d = _Kd * (d > 0.0 ? (e - _e) / d : 0.0);
        _t = t;
        _e = e;
        return _p + _i + _d;
    }
};

// -----------------------------------------------------------------------------------------------

template<typename T>
class AlphaSmoothing {
    const T _alpha;
    T _val = T(0);

public:
    explicit AlphaSmoothing(const T& alpha)
        : _alpha(alpha) {}
    const T& apply(const T& val) {
        return (_val = (_alpha * val + (1.0 - _alpha) * _val));
    }
};

// -----------------------------------------------------------------------------------------------

class Initialisable {
    bool initialised = false;

public:
    operator bool() {
        if (!initialised) {
            initialised = true;
            return false;
        }
        return true;
    }
    void reset() {
        initialised = false;
    }
};

// -----------------------------------------------------------------------------------------------

#include <Arduino.h>

class Intervalable {
    interval_t _interval, _previous;
    counter_t _exceeded = 0;
public:
    explicit Intervalable(const interval_t interval = 0, const interval_t previous = 0)
        : _interval(interval), _previous(previous) {}
    operator bool() {
        const interval_t current = millis();
        if (current - _previous > _interval) {
            _previous = current;
            return true;
        }
        return false;
    }
    bool passed(interval_t* interval = nullptr, const bool atstart = false) {
        const interval_t current = millis();
        if ((atstart && _previous == 0) || current - _previous > _interval) {
            if (interval != nullptr)
                (*interval) = current - _previous;
            _previous = current;
            return true;
        }
        return false;
    }
    void reset(const interval_t interval = std::numeric_limits<interval_t>::max()) {
        if (interval != std::numeric_limits<interval_t>::max())
            _interval = interval;
        _previous = millis();
    }
    void setat(const interval_t place) {
        _previous = millis() - ((_interval - place) % _interval);
    }
    void wait() {
        const interval_t current = millis();
        if (current - _previous < _interval)
            delay(_interval - (current - _previous));
        else if (_previous > 0) _exceeded++;
        _previous = millis();
    }
    counter_t exceeded() const {
        return _exceeded;
    }
};

// -----------------------------------------------------------------------------------------------

#include <Arduino.h>

class Uptime {
    const interval_t _started;

public:
    Uptime()
        : _started(millis()) {}
    inline interval_t seconds() const {
        return (millis() - _started) / 1000;
    }
};

// -----------------------------------------------------------------------------------------------

class Enableable {
    bool _enabled;
public:
    explicit Enableable(const bool enabled = false): _enabled (enabled) {}
    inline void operator++(int) {
        _enabled = true;
    }
    inline operator bool() const {
        return _enabled;
    }
    void operator=(const bool state) {
        _enabled = state;
    }
};

// -----------------------------------------------------------------------------------------------

#include <Arduino.h>

class ActivationTracker {
    counter_t _count = 0;
    interval_t _seconds = 0;

public:
    inline const interval_t& seconds() const {
        return _seconds;
    }
    inline const counter_t& count() const {
        return _count;
    }
    ActivationTracker& operator++(int) {
        _seconds = millis() / 1000;
        _count++;
        return *this;
    }
    ActivationTracker& operator=(const counter_t count) {
        _seconds = millis() / 1000;
        _count = count;
        return *this;
    }
    inline operator counter_t() const {
        return _count;
    }
};

class ActivationTrackerWithDetail : public ActivationTracker {
    String _detail;

public:
    inline const String& detail() const {
        return _detail;
    }
    ActivationTrackerWithDetail& operator+=(const String& detail) {
        ActivationTracker::operator++(1);
        _detail = detail;
        return *this;
    }
    ActivationTrackerWithDetail& operator=(const counter_t count) {
        ActivationTracker::operator=(count);
        _detail = "";
        return *this;
    }
};

// -----------------------------------------------------------------------------------------------

#include <type_traits>

template<typename T>
T map(const T& x, const T& in_min, const T& in_max, const T& out_min, const T& out_max) {
    static_assert(std::is_arithmetic_v<T>, "T must be an arithmetic type");
    return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

// -----------------------------------------------------------------------------------------------

#include <type_traits>
#include <exception>
#include <utility>

template<typename F>
void exception_catcher(F&& f) {
    static_assert(std::is_invocable_v<F>, "F must be an invocable type");
    try {
        std::forward<F>(f)();
    } catch (const std::exception& e) {
        DEBUG_PRINTF("exception: %s\n", e.what());
    } catch (...) {
        DEBUG_PRINTF("exception: unknown\n");
    }
}

// -----------------------------------------------------------------------------------------------

#include <type_traits>

template<typename T>
class Singleton {
    static_assert(std::is_class_v<T>, "T must be a class type");
    inline static T* _instance = nullptr;

public:
    inline static T* instance() {
        return _instance;
    }
    explicit Singleton(T* t) {
        assert(_instance == nullptr && "duplicate Singleton initializer");
        _instance = t;
    }
    virtual ~Singleton() {
        _instance = nullptr;
    }
};

// -----------------------------------------------------------------------------------------------

#include <mutex>
#include <queue>

template<typename T>
class QueueSimpleConcurrentSafe {
    std::mutex _mutex;
    std::queue<T> _queue;

public:
    void push(const T& t) {
        std::lock_guard<std::mutex> guard(_mutex);
        _queue.push(t);
    }
    bool pull(T& t) {
        std::lock_guard<std::mutex> guard(_mutex);
        if (!_queue.empty()) {
            t = _queue.front();
            _queue.pop();
            return true;
        }
        return false;
    }
    void drain() {
        std::lock_guard<std::mutex> guard(_mutex);
        while (!_queue.empty())
            _queue.pop();
    }
};

// -----------------------------------------------------------------------------------------------

#include <array>
#include <cstdlib>

namespace gaussian {

template<typename T>
using vector4 = std::array<T, 4>;
template<typename T>
using matrix4 = vector4<vector4<T>>;

String solve(matrix4<double>& matrix, vector4<double>& result) {
    static constexpr double DETERMINANT_DEMINIMUS = 1e-10;

    const double determinant = matrix[0][0] * (matrix[1][1] * matrix[2][2] * matrix[3][3] + matrix[1][2] * matrix[2][3] * matrix[3][1] + matrix[1][3] * matrix[2][1] * matrix[3][2] - matrix[1][3] * matrix[2][2] * matrix[3][1] - matrix[1][2] * matrix[2][1] * matrix[3][3] - matrix[1][1] * matrix[2][3] * matrix[3][2]);
    if (std::abs(determinant) < DETERMINANT_DEMINIMUS)
        return "matrix is singular/near-singular, determinant: " + ArithmeticToString(determinant, 12);

    for (size_t i = 0; i < 4; i++) {
        for (size_t j = i + 1; j < 4; j++) {
            const double factor = matrix[j][i] / matrix[i][i];
            for (size_t k = i; k < 4; k++)
                matrix[j][k] -= factor * matrix[i][k];
            result[j] -= factor * result[i];
        }
    }
    for (int i = 4 - 1; i >= 0; i--) {
        for (int j = i + 1; j < 4; j++)
            result[i] -= matrix[i][j] * result[j];
        result[i] /= matrix[i][i];
    }
    return String();
}

String solve(matrix4<double>& XtX, vector4<double>& XtY, vector4<double>& result) {
    static constexpr double CONDITION_DEMAXIMUS = 1e15, DETERMINANT_DEMINIMUS = 1e-10;

    double max_singular = std::numeric_limits<double>::min(), min_singular = std::numeric_limits<double>::max();
    for (int i = 0; i < 4; i++) {
        double sum_singular = 0;
        for (int j = 0; j < 4; j++)
            sum_singular += std::abs(XtX[i][j]);
        if (sum_singular > max_singular) max_singular = sum_singular;
        if (sum_singular < min_singular) min_singular = sum_singular;
    }
    const double condition_number = max_singular / min_singular;
    if (condition_number > CONDITION_DEMAXIMUS)
        return "matrix ill-conditioned, condition number estimate: " + ArithmeticToString(condition_number, 12);
    const double determinant =
        XtX[0][0] * (XtX[1][1] * XtX[2][2] * XtX[3][3] + XtX[1][2] * XtX[2][3] * XtX[3][1] + XtX[1][3] * XtX[2][1] * XtX[3][2] - XtX[1][3] * XtX[2][2] * XtX[3][1] - XtX[1][2] * XtX[2][1] * XtX[3][3] - XtX[1][1] * XtX[2][3] * XtX[3][2])
        - XtX[0][1] * (XtX[1][0] * XtX[2][2] * XtX[3][3] + XtX[1][2] * XtX[2][3] * XtX[3][0] + XtX[1][3] * XtX[2][0] * XtX[3][2] - XtX[1][3] * XtX[2][2] * XtX[3][0] - XtX[1][2] * XtX[2][0] * XtX[3][3] - XtX[1][0] * XtX[2][3] * XtX[3][2])
        + XtX[0][2] * (XtX[1][0] * XtX[2][1] * XtX[3][3] + XtX[1][1] * XtX[2][3] * XtX[3][0] + XtX[1][3] * XtX[2][0] * XtX[3][1] - XtX[1][3] * XtX[2][1] * XtX[3][0] - XtX[1][1] * XtX[2][0] * XtX[3][3] - XtX[1][0] * XtX[2][3] * XtX[3][1])
        - XtX[0][3] * (XtX[1][0] * XtX[2][1] * XtX[3][2] + XtX[1][1] * XtX[2][2] * XtX[3][0] + XtX[1][2] * XtX[2][0] * XtX[3][1] - XtX[1][2] * XtX[2][1] * XtX[3][0] - XtX[1][1] * XtX[2][0] * XtX[3][2] - XtX[1][0] * XtX[2][2] * XtX[3][1]);
    if (std::abs(determinant) < DETERMINANT_DEMINIMUS)
        return "matrix is singular/near-singular, determinant: " + ArithmeticToString(determinant, 12);

    for (int i = 0; i < 4; i++) {
        int max_row = i;
        for (int j = i + 1; j < 4; j++)
            if (std::abs(XtX[j][i]) > std::abs(XtX[max_row][i]))
                max_row = j;
        if (max_row != i)
            std::swap(XtX[i], XtX[max_row]), std::swap(XtY[i], XtY[max_row]);
        for (int j = i + 1; j < 4; j++) {
            const double factor = XtX[j][i] / XtX[i][i];
            for (int k = i; k < 4; k++)
                XtX[j][k] -= factor * XtX[i][k];
            XtY[j] -= factor * XtY[i];
        }
    }
    for (int i = 3; i >= 0; i--) {
        result[i] = XtY[i];
        for (int j = i + 1; j < 4; j++)
            result[i] -= XtX[i][j] * result[j];
        result[i] /= XtX[i][i];
    }
    return String();
}
}

// -----------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------

#include <ctime>

String getTimeString(time_t timet = 0) {
    struct tm timeinfo;
    char timeString[sizeof("yyyy-mm-ddThh:mm:ssZ") + 1] = { '\0' };
    if (timet == 0) time(&timet);
    if (gmtime_r(&timet, &timeinfo) != nullptr)
        strftime(timeString, sizeof(timeString), "%Y-%m-%dT%H:%M:%SZ", &timeinfo);
    return timeString;
}

// -----------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------
