
// -----------------------------------------------------------------------------------------------

#include <nvs_flash.h>

class PersistentData {

    static inline constexpr const char* DEFAULT_PERSISTENT_PARTITION = "nvs";

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

#include <esp_mac.h>

String mac_address (void) {
#define __MAC_MACBYTETOSTRING(byte) String (NIBBLE_TO_HEX_CHAR ((byte) >> 4)) + String (NIBBLE_TO_HEX_CHAR ((byte) & 0xF))
#define __MAC_FORMAT_ADDRESS(addr) __MAC_MACBYTETOSTRING ((addr)[0]) + ":" + __MAC_MACBYTETOSTRING ((addr)[1]) + ":" + __MAC_MACBYTETOSTRING ((addr)[2]) + ":" + __MAC_MACBYTETOSTRING ((addr)[3]) + ":" + __MAC_MACBYTETOSTRING ((addr)[4]) + ":" + __MAC_MACBYTETOSTRING ((addr)[5])
    uint8_t macaddr [6];
    esp_read_mac (macaddr, ESP_MAC_WIFI_STA);
    return __MAC_FORMAT_ADDRESS (macaddr);
}

// -----------------------------------------------------------------------------------------------
