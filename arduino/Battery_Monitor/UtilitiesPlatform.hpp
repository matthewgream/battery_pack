
// -----------------------------------------------------------------------------------------------
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
            char *buffer = new char [size];
            if (buffer) {
              if (nvs_get_str (_handle, name, buffer, &size) == ESP_OK) {
                  (*value) = buffer;
                  delete [] buffer;
                  return true;
              }
              delete [] buffer;
            }
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
// -----------------------------------------------------------------------------------------------

#include <esp_mac.h>

String getMacAddress (void) {
    #define __MAC_MACBYTETOSTRING(byte) String (NIBBLE_TO_HEX_CHAR ((byte) >> 4)) + String (NIBBLE_TO_HEX_CHAR ((byte) & 0xF))
    #define __MAC_FORMAT_ADDRESS(addr) __MAC_MACBYTETOSTRING ((addr)[0]) + ":" + __MAC_MACBYTETOSTRING ((addr)[1]) + ":" + __MAC_MACBYTETOSTRING ((addr)[2]) + ":" + __MAC_MACBYTETOSTRING ((addr)[3]) + ":" + __MAC_MACBYTETOSTRING ((addr)[4]) + ":" + __MAC_MACBYTETOSTRING ((addr)[5])
    uint8_t macaddr [6];
    esp_read_mac (macaddr, ESP_MAC_WIFI_STA);
    return __MAC_FORMAT_ADDRESS (macaddr);
}

// -----------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------

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

#include <esp_task_wdt.h>

class Watchdog {
    const int _timeout;
    bool _started;
public:
    Watchdog (const int timeout): _timeout (timeout), _started (false) {};
    void start () {
        if (!_started) {
            esp_task_wdt_deinit ();
            const esp_task_wdt_config_t wdt_config = {
                .timeout_ms = static_cast <uint32_t> (_timeout * 1000),
                .idle_core_mask = static_cast <uint32_t> ((1 << ESP.getChipCores ()) - 1),
                .trigger_panic = true
            };
            esp_task_wdt_init (&wdt_config);
            esp_task_wdt_add (NULL);
            if (esp_task_wdt_status (NULL) == ESP_OK) {
                _started = true;
                esp_task_wdt_reset ();
            }
        }
    }
    void reset () {
        esp_task_wdt_reset ();
    }
};

// -----------------------------------------------------------------------------------------------

#include <esp_system.h>
#include <esp_rom_sys.h>

int getResetReason () {
    return esp_rom_get_reset_reason (0);
}
std::pair <String, String> getResetDetails (const int reason = getResetReason ()) {
    switch (reason) {
        case RESET_REASON_CHIP_POWER_ON:    return std::pair <String, String> ("POWER_ON", "Power on reset");
        // case RESET_REASON_CHIP_BROWN_OUT: return std::pair <String, String> ("CHIP_BROWN_OUT", "VDD voltage is not stable and resets the chip");
        // case RESET_REASON_CHIP_SUPER_WDT: return std::pair <String, String> ("CHIP_SUPER_WDT", "Super watch dog resets the chip");

        case RESET_REASON_CORE_SW:          return std::pair <String, String> ("SW_CORE", "Software resets the digital core by RTC_CNTL_SW_SYS_RST");
        case RESET_REASON_CPU0_SW:          return std::pair <String, String> ("SW_CPU0", "Software resets CPU 0 by RTC_CNTL_SW_PROCPU_RST");
        case RESET_REASON_CORE_USB_UART:    return std::pair <String, String> ("USB_UART", "USB UART resets the digital core");
        case RESET_REASON_CORE_USB_JTAG:    return std::pair <String, String> ("USB_JTAG", "USB JTAG resets the digital core");
        case RESET_REASON_CORE_DEEP_SLEEP:  return std::pair <String, String> ("DEEP_SLEEP", "Deep sleep reset the digital core");

        case RESET_REASON_CORE_MWDT0:       return std::pair <String, String> ("WDT_CORE_M0", "Main watch dog 0 resets digital core");
        case RESET_REASON_CORE_MWDT1:       return std::pair <String, String> ("WDT_CORE_M1", "Main watch dog 1 resets digital core");
        case RESET_REASON_CORE_RTC_WDT:     return std::pair <String, String> ("WDT_CORE_RTC", "RTC watch dog resets digital core");
        case RESET_REASON_CPU0_MWDT0:       return std::pair <String, String> ("WDT_CPU0_M0", "Main watch dog 0 resets CPU 0");
        case RESET_REASON_CPU0_MWDT1:       return std::pair <String, String> ("WDT_CPU0_M1", "Main watch dog 1 resets CPU 0");
        case RESET_REASON_CPU0_RTC_WDT:     return std::pair <String, String> ("WDT_CPU0_RTC", "RTC watch dog resets CPU 0");
        case RESET_REASON_SYS_SUPER_WDT:    return std::pair <String, String> ("WDT_SYS_SUPER", "Super watch dog resets the digital core and rtc module");
        case RESET_REASON_SYS_RTC_WDT:      return std::pair <String, String> ("WDT_SYS_RTC", "RTC watch dog resets digital core and rtc module");

        case RESET_REASON_CORE_EFUSE_CRC:   return std::pair <String, String> ("EFUSE_CRC", "eFuse CRC error resets the digital core");
        case RESET_REASON_SYS_CLK_GLITCH:   return std::pair <String, String> ("CLK_GLITCH", "Glitch on clock resets the digital core and rtc module");
        case RESET_REASON_CORE_PWR_GLITCH:  return std::pair <String, String> ("PWR_GLITCH", "Glitch on power resets the digital core");
        case RESET_REASON_SYS_BROWN_OUT:    return std::pair <String, String> ("PWR_BROWN_OUT", "VDD voltage is not stable and resets the digital core");

        default:                            return std::pair <String, String> (IntToString (reason), "Unknown reason");
    }
}
bool getResetOkay (const int reason = getResetReason ()) {
    return (reason == RESET_REASON_CHIP_POWER_ON ||
            reason == RESET_REASON_CORE_SW || reason == RESET_REASON_CPU0_SW ||
            reason == RESET_REASON_CORE_USB_UART || reason == RESET_REASON_CORE_USB_JTAG);
}

// int getResetReason () {
//     return static_cast <int> (esp_reset_reason ());
// }
// std::pair <String, String> getResetDetails (const int reason = getResetReason ()) {
//     switch (reason) {
//         case ESP_RST_UNKNOWN:     return std::pair <String, String> ("UNKNOWN", "Reset reason can not be determined");
//         case ESP_RST_POWERON:     return std::pair <String, String> ("POWERON", "Reset due to power-on event");
//         case ESP_RST_SW:          return std::pair <String, String> ("SOFTWARE", "Software reset via esp_restart");
//         case ESP_RST_PANIC:       return std::pair <String, String> ("PANIC", "Software reset due to exception/panic");
//         case ESP_RST_INT_WDT:     return std::pair <String, String> ("WDT_INT", "Reset (software or hardware) due to interrupt watchdog");
//         case ESP_RST_TASK_WDT:    return std::pair <String, String> ("WDT_TASK", "Reset due to task watchdog");
//         case ESP_RST_WDT:         return std::pair <String, String> ("WDT_OTHER", "Reset due to other watchdogs");
//         case ESP_RST_DEEPSLEEP:   return std::pair <String, String> ("DEEPSLEEP", "Reset after exiting deep sleep mode");
//         case ESP_RST_BROWNOUT:    return std::pair <String, String> ("BROWNOUT", "Brownout reset (software or hardware)");
//         case ESP_RST_SDIO:        return std::pair <String, String> ("SDIO", "Reset over SDIO");
//         case ESP_RST_USB:         return std::pair <String, String> ("USB", "Reset by USB peripheral");
//         case ESP_RST_JTAG:        return std::pair <String, String> ("JTAG", "Reset by JTAG");
//         case ESP_RST_EFUSE:       return std::pair <String, String> ("EFUSE", "Reset due to efuse error");
//         case ESP_RST_PWR_GLITCH:  return std::pair <String, String> ("PWR_GLITCH", "Reset due to power glitch detected");
//         case ESP_RST_CPU_LOCKUP:  return std::pair <String, String> ("CPU_LOOKUP", "Reset due to CPU lock up");
//         default:                  return std::pair <String, String> (IntToString (reason), "Reset reason undefined");
//     }
// }
// bool getResetOkay (const int reason = getResetReason ()) {
//     return (reason == ESP_RST_POWERON || reason == ESP_RST_SW || reason == ESP_RST_DEEPSLEEP || reason == ESP_RST_USB || reason == ESP_RST_JTAG);
// }

// -----------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------
