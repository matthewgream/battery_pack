
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

namespace JsonFunctions {
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
    inline int findValue (const String& json, const String& key, String& value) {
        const int keyPos = findValueKey (json, key);
        if (keyPos >= 0) {
            const int valueStart = findValueStart (json, keyPos), valueEnd = findValueEnd (json, valueStart);
            if (valueEnd >= 0) {
                value = json.substring (valueStart, valueEnd);
                return value.length ();
            }
        }
        return -1;
    }
    inline int findNextElement (const String& json, const int startPos, String& element) {
        int braceCount = 0;
        bool inQuotes = false;
        for (int i = startPos, j = startPos; i < json.length (); i ++) {
            const char c = json.charAt (i);
            if (c == '"' && (i == startPos || json.charAt (i - 1) != '\\'))
                inQuotes = !inQuotes;
            else if (!inQuotes) {
                if (c == '{' || c == '[') {
                    braceCount ++;
                } else if (c == '}' || c == ']') {
                    braceCount --;
                    if (braceCount == 0) {
                        element = json.substring (j, i + 1);
                        return i + 1;
                    }
                } else if (c == ',') {
                    if (i == j) j ++;
                    if (i != startPos && braceCount == 0) {
                        element = json.substring (j, i + 1);
                        return i + 1;
                    }
                }
            }
        }
        return -1;
    }
}

// -----------------------------------------------------------------------------------------------

#include <vector>

class JsonSplitter {
private:
    const int splitLength;
    const std::vector <String> commonElements;

public:
    JsonSplitter (const int splitLength, const std::vector<String> &commonElements) : splitLength (splitLength), commonElements (commonElements) {}

    void splitJson (const String& json, const std::function<void (const String&, const int)> callback) {
        String common;
        for (const auto& commonElement : commonElements) {
            String value; 
            if (JsonFunctions::findValue (json, commonElement, value) >= 0) {
                if (!common.isEmpty ()) common += ",";
                common += "\"" + commonElement + "\":" + value;
            }
        }
        if (common.length () > 0) common += ',';
        String current, element;
        int numbers = 0;
        for (int cur = 1, nxt = 0; (nxt = JsonFunctions::findNextElement (json, cur, element)) > 0; cur = nxt) {  
            if (std::any_of (commonElements.begin (), commonElements.end (), [&element] (const String& commonElement) { return element.startsWith ("\"" + commonElement + "\":"); }))
                continue;
            if ((1 + common.length ()) + current.length () + (element.length () + 1) < splitLength) {  // +1 for the comma or brace
                if (!current.isEmpty ()) current += ",";
                current += element;
                numbers ++;
            } else {
                if (!current.isEmpty ())
                    callback ("{" + common + current + "}", numbers);
                current = element;
                numbers = 1;
            }
        }
        if (!current.isEmpty ())
            callback ("{" + common + current + "}", numbers);
    }
};

// -----------------------------------------------------------------------------------------------
