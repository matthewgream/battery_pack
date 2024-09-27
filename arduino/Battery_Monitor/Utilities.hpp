
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

#include <nvs_flash.h>

#define DEFAULT_PERSISTENT_PARTITION "nvs"

class PersistentData {
public:
    static int _initialised;
    static bool _initialise () { return _initialised || (++ _initialised && nvs_flash_init_partition (DEFAULT_PERSISTENT_PARTITION) == ESP_OK); }
private:
    nvs_handle_t _handle;
    const bool _okay = false;
public:
    PersistentData (const char *space): _okay (_initialise () && nvs_open_from_partition (DEFAULT_PERSISTENT_PARTITION, space, NVS_READWRITE, &_handle) == ESP_OK) {}
    ~PersistentData () { if (_okay) nvs_close (_handle); }
    inline bool get (const char *name, uint32_t *value) const { return (_okay && nvs_get_u32 (_handle, name, value) == ESP_OK); }
    inline bool set (const char *name, uint32_t value) { return  (_okay && nvs_set_u32 (_handle, name, value) == ESP_OK); }
    inline bool get (const char *name, int32_t *value) const { return (_okay && nvs_get_i32 (_handle, name, value) == ESP_OK); }
    inline bool set (const char *name, int32_t value) { return  (_okay && nvs_set_i32 (_handle, name, value) == ESP_OK); }
    inline bool get (const char *name, String *value) const {
        size_t size;
        if (_okay && nvs_get_str (_handle, name, NULL, &size) == ESP_OK) {
            char *str = (char *) malloc (size);
            if (nvs_get_str (_handle, name, str, &size) == ESP_OK) {
                (*value) = str;
                free (str);
                return true;
            }
            free (str);
        }
        return false;
    }
    inline bool set (const char *name, const String &value) { return  (_okay && nvs_set_str (_handle, name, value.c_str ()) == ESP_OK); }

};
int PersistentData::_initialised = 0;
template <typename T>
class PersistentValue {
    PersistentData &_data;
    const String _name;
    const T _value_default;
public:
    PersistentValue (PersistentData& data, const char *name, const T value_default): _data (data), _name (name), _value_default (value_default) {}
    inline operator T () const { T value; return _data.get (_name.c_str (), &value) ? value : _value_default; }
    inline bool operator= (const T value) { return _data.set (_name.c_str (), value); }
    inline bool operator+= (const T value2) { T value = _value_default; _data.get (_name.c_str (), &value); value += value2; return _data.set (_name.c_str (), value); }
    inline bool operator>= (const T value2) const { T value = _value_default; _data.get (_name.c_str (), &value); return value >= value2; }
    inline bool operator> (const T value2) const { T value = _value_default; _data.get (_name.c_str (), &value); return value > value2; }
};

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

#include <type_traits>

template <typename T>
inline T map (const T x, const T in_min, const T in_max, const T out_min, const T out_max) {
    static_assert (std::is_arithmetic_v <T>, "T must be an arithmetic type");
    return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

// -----------------------------------------------------------------------------------------------

#define NIBBLE_TO_HEX_CHAR(nibble) ((char) ((nibble) < 10 ? '0' + (nibble) : 'A' + ((nibble) - 10)))
#define BYTE_TO_HEX(byte) NIBBLE_TO_HEX_CHAR ((byte) >> 4), NIBBLE_TO_HEX_CHAR ((byte) & 0x0F)

#include <esp_mac.h>

String mac_address (void) {
#define __MAC_MACBYTETOSTRING(byte) String (NIBBLE_TO_HEX_CHAR ((byte) >> 4)) + String (NIBBLE_TO_HEX_CHAR ((byte) & 0xF))
#define __MAC_FORMAT_ADDRESS(addr) __MAC_MACBYTETOSTRING ((addr)[0]) + ":" + __MAC_MACBYTETOSTRING ((addr)[1]) + ":" + __MAC_MACBYTETOSTRING ((addr)[2]) + ":" + __MAC_MACBYTETOSTRING ((addr)[3]) + ":" + __MAC_MACBYTETOSTRING ((addr)[4]) + ":" + __MAC_MACBYTETOSTRING ((addr)[5])
    uint8_t macaddr [6];
    esp_read_mac (macaddr, ESP_MAC_WIFI_STA);
    return __MAC_FORMAT_ADDRESS (macaddr);
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

#include <vector>

    inline int findValueKey (const String& json, const String& key) {
        return json.indexOf ("\"" + key + "\":");
    }
    inline int findValueStart (const String& json, const int keyPos) {
        int valueStart = json.indexOf (":", keyPos) + 1;
        while (isSpace (json.charAt (valueStart))) valueStart ++;
        return valueStart;
    }
    inline int findValueEnd (const String& json, const int valueStart) {
        int valueEnd;
        if (json.charAt (valueStart) == '"') {
            valueEnd = json.indexOf ("\"", valueStart + 1);
            while (valueEnd > 0 && json.charAt (valueEnd - 1) == '\\')
                valueEnd = json.indexOf ("\"", valueEnd + 1);
            if (valueEnd >= 0)
                valueEnd ++;
        } else {
            valueEnd = json.indexOf (",", valueStart);
            if (valueEnd == -1)
                valueEnd = json.indexOf ("}", valueStart);
        }
        return valueEnd;
    }
    inline String findValue (const String& json, const String& key) {
        const int keyPos = findValueKey (json, key);
        if (keyPos >= 0) {
            const int valueStart = findValueStart (json, keyPos), valueEnd = findValueEnd (json, valueStart);
            if (valueEnd >= 0)
                return json.substring (valueStart, valueEnd);
        }
        return "";
    }
    inline int findNextElement (const String& json, int startPos) {
        int braceCount = 0;
        bool inQuotes = false;
        for (int i = startPos; i < json.length (); i ++) {
            const char c = json.charAt (i);
            if (c == '"' && (i == startPos || json.charAt (i - 1) != '\\'))
                inQuotes = !inQuotes;
            else if (!inQuotes) {
                if (c == '{' || c == '[')
                    braceCount ++;
                else if (c == '}' || c == ']') {
                    braceCount --;
                    if (braceCount == 0)
                        return i + 1;
                }
            }
        }
        return -1;
    }

class JsonSplitter {
private:
    const int splitLength;
    const std::vector <String> commonElements;

public:
    JsonSplitter (const int splitLength, const std::vector<String> &commonElements) : splitLength (splitLength), commonElements (commonElements) {}

    void splitJson (const String& json, const std::function<void (const String&, const int)> packetCallback) {

        String commonStart = "{";
        for (const auto& commonElement : commonElements) {
            String value = findValue (json, commonElement);
            if (value.length () > 0) {
                if (commonStart.length () > 1) commonStart += ",";
                commonStart += "\"" + commonElement + "\":" + value;
            }
        }

        String currentPart = commonStart;
        int currentSize = commonStart.length ();
        int currentCount = 0;

        for (int pos = 1, nextPos = 0; pos < json.length () && (nextPos = findNextElement (json, pos)) != -1; pos = nextPos) {
            const String element = json.substring (pos, nextPos);

            bool isCommonElement = false;
            for (const auto& commonElement : commonElements) {
                if (element.startsWith ("\"" + commonElement + "\":")) {
                    isCommonElement = true;
                    break;
                }
            }
            if (isCommonElement)
                continue;

            currentCount ++;
            if (currentSize + element.length () + 1 <= splitLength) {  // +1 for the comma
                if (currentSize > commonStart.length ()) currentPart += ",";
                currentPart += element;
                currentSize += element.length () + 1;
            } else {
                if (currentSize > commonStart.length ())            
                    packetCallback (currentPart + "}", elements);
                currentPart = commonStart + "," + element;
                currentSize = commonStart.length () + 1 + element.length ();
                currentCount = 0;
            }
  
            DEBUG_PRINTF ("element (%d) --> %s\n", element.length () - (element [0] == ',' ? 1 : 0), &element [element [0] == ',' ? 1 : 0]);
        }

        if (currentSize > commonStart.length ())
            packetCallback (currentPart + "}", elements);
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
