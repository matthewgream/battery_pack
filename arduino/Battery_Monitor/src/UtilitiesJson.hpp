
// -----------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------

#include <ArduinoJson.h>

// -----------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------

class JsonCollector {
    JsonDocument doc;

public:
    explicit JsonCollector(const String& type, const String& time, const String& addr) {
        doc["type"] = type;
        doc["time"] = time;
        doc["addr"] = addr;
    }
    inline JsonDocument& document() {
        return doc;
    }
    operator String() const {
        String output;
        serializeJson(doc, output);
        return output;
    }
};

// -----------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------

namespace JsonFunctions {
int findValueKey(const String& json, const String& key) {
    return (json.isEmpty() || key.isEmpty()) ? -1 : json.indexOf("\"" + key + "\":");
}
int findValueStart(const String& json, const int keyPos) {
    int valueStart = json.indexOf(":", keyPos);
    if (valueStart != -1)
        do valueStart++;
        while (valueStart < json.length() && isSpace(json.charAt(valueStart)));
    return valueStart;
}
int findValueEnd(const String& json, const int valueStart) {
    int valueEnd;
    if (valueStart >= json.length()) return -1;
    if (json.charAt(valueStart) == '"') {
        valueEnd = json.indexOf("\"", valueStart + 1);
        while (valueEnd > 0 && json.charAt(valueEnd - 1) == '\\')
            valueEnd = json.indexOf("\"", valueEnd + 1);
        if (valueEnd >= 0)
            valueEnd++;
    } else {
        const int commaPos = json.indexOf(",", valueStart), bracePos = json.indexOf("}", valueStart);
        if (commaPos == -1) valueEnd = bracePos;
        else if (bracePos == -1) valueEnd = commaPos;
        else valueEnd = min(commaPos, bracePos);
    }
    return valueEnd;
}
int findValue(const String& json, const String& key, String& value) {
    const int keyPos = findValueKey(json, key);
    if (keyPos >= 0) {
        const int valueStart = findValueStart(json, keyPos);
        if (valueStart >= 0) {
            const int valueEnd = findValueEnd(json, valueStart);
            if (valueEnd >= 0) {
                value = json.substring(valueStart, valueEnd);
                return value.length();
            }
        }
    }
    return -1;
}
int findNextElement(const String& json, const int startPos, String& element) {
    int braceCount = 0;
    bool inQuotes = false;
    for (int i = startPos, j = startPos; i < json.length(); i++) {
        const char c = json.charAt(i);
        if (c == '"' && (i == startPos || json.charAt(i - 1) != '\\'))
            inQuotes = !inQuotes;
        else if (!inQuotes) {
            if (c == '{' || c == '[') {
                braceCount++;
            } else if (c == '}' || c == ']') {
                braceCount--;
                if (braceCount == 0) {
                    element = json.substring(j, i + 1);
                    return i + 1;
                }
            } else if (c == ',') {
                if (i == j) j++;
                if (i != startPos && braceCount == 0) {
                    element = json.substring(j, i + 1);
                    return i + 1;
                }
            }
        }
    }
    return -1;
}
}

// -----------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------

#include <vector>

class JsonSplitter {
    const int splitLength;
    const std::vector<String> commonElements;

public:
    JsonSplitter(const int splitLength, const std::vector<String>& commonElements)
        : splitLength(splitLength), commonElements(commonElements) {}

    void splitJson(const String& json, const std::function<void(const String&, const int)> callback) {
        String common;
        for (const auto& commonElement : commonElements) {
            String value;
            if (JsonFunctions::findValue(json, commonElement, value) >= 0) {
                if (!common.isEmpty()) common += ",";
                common += "\"" + commonElement + "\":" + value;
            }
        }
        if (common.length() > 0) common += ',';
        String current, element;
        int numbers = 0;
        for (int cur = 1, nxt = 0; (nxt = JsonFunctions::findNextElement(json, cur, element)) > 0; cur = nxt) {
            if (std::any_of(commonElements.begin(), commonElements.end(), [&element](const String& commonElement) {
                    return element.startsWith("\"" + commonElement + "\":");
                }))
                continue;
            if ((1 + common.length()) + current.length() + (element.length() + 1) < splitLength) {    // +1 for the comma or brace
                if (!current.isEmpty()) current += ",";
                current += element;
                numbers++;
            } else {
                if (!current.isEmpty())
                    callback("{" + common + current + "}", numbers);
                current = element;
                numbers = 1;
            }
        }
        if (!current.isEmpty())
            callback("{" + common + current + "}", numbers);
    }
};

// -----------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------

template<typename T>
bool convertToJson(const StatsWithValue<T>& src, JsonVariant dst) {
    if ((dst["cnt"] = src.cnt()) > 0) {
        dst["val"] = src.val();
        dst["avg"] = src.avg();
        dst["min"] = src.min();
        dst["max"] = src.max();
    }
    return true;
}
template<typename T>
bool convertToJson(const Stats<T>& src, JsonVariant dst) {
    if ((dst["cnt"] = src.cnt()) > 0) {
        dst["avg"] = src.avg();
        dst["min"] = src.min();
        dst["max"] = src.max();
    }
    return true;
}
template<typename T>
bool convertToJson(const PidController<T>& src, JsonVariant dst) {
    dst["Kp"] = src._Kp;
    dst["Ki"] = src._Ki;
    dst["Kd"] = src._Kd;
    if (src._t > 0) {
        dst["p"] = src._p;
        dst["i"] = src._i;
        dst["d"] = src._d;
        dst["e"] = src._e;
        dst["t"] = src._t;
    }
    return true;
}
bool convertToJson(const ActivationTrackerWithDetail& src, JsonVariant dst) {
    if (dst["count"] = src.count() > 0) {
        dst["last"] = src.seconds();
        if (!src.detail().isEmpty())
            dst["detail"] = src.detail();
    }
    return true;
}
bool convertToJson(const ActivationTracker& src, JsonVariant dst) {
    if (dst["count"] = src.count() > 0) {
        dst["last"] = src.seconds();
    }
    return true;
}
bool convertToJson(const Uptime& src, JsonVariant dst) {
    dst.set(src.seconds());
    return true;
}

class JsonSerializable {
public:
    virtual void serialize(JsonVariant&) const = 0;
};
bool convertToJson(const JsonSerializable& src, JsonVariant dst) {
    src.serialize(dst);
    return true;
}

// -----------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------
