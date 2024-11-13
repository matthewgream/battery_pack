
// -----------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------

#include <utility>

inline String __encodeUrlWithParameters (const String &result) {
    return result;
}
template <typename K, typename V, typename... R>
inline String __encodeUrlWithParameters (const String &result, K &&key, V &&value, R &&...rest) {
    return __encodeUrlWithParameters (result + (result.isEmpty () ? "?" : "&") + String (key) + "=" + String (value), std::forward<R> (rest)...);
}
template <typename... A>
inline String encodeUrlWithParameters (const String &url, A &&...args) {
    return url + __encodeUrlWithParameters ("", std::forward<A> (args)...);
}

// -----------------------------------------------------------------------------------------------

#include <flashz.hpp>
#include <esp32fota.h>
#include <WiFi.h>

static bool __ota_image_update_check (esp32FOTA &ota, const char *json, const char *type, const char *vers, const char *addr, char *newr) {
    DEBUG_PRINTF ("OTA_IMAGE_CHECK: fetch json=%s, type=%s, vers=%s, addr=%s ...", json, type, vers, addr);
    ota.setManifestURL (encodeUrlWithParameters (json, "type", type, "vers", vers, "addr", addr).c_str ());
    const bool update = ota.execHTTPcheck ();
    if (update) {
        ota.getPayloadVersion (newr);
        DEBUG_PRINTF (" newer version=%s\n", newr);
        return true;
    } else {
        DEBUG_PRINTF (" no newer version (or error)\n");
        return false;
    }
}
static void __ota_image_update_execute (esp32FOTA &ota, const char *newr, const std::function<void ()> &func) {
    bool updated = false;
    DEBUG_PRINTF ("OTA_IMAGE_UPDATE: download and install, vers=%s\n", newr);
    ota.setProgressCb ([&] (size_t progress, size_t size) {
        if (progress >= size)
            updated = true;
        DEBUG_PRINTF (! updated ? "." : "\n");
    });
    ota.setUpdateBeginFailCb ([&] (int partition) {
        if (! updated)
            DEBUG_PRINTF ("\n");
        DEBUG_PRINTF ("\nOTA_IMAGE_UPDATE: failed begin, partition=%\n", partition == U_SPIFFS ? "spiffs" : "firmware");
    });
    ota.setUpdateCheckFailCb ([&] (int partition, int error) {
        if (! updated)
            DEBUG_PRINTF ("\n");
        DEBUG_PRINTF ("OTA_IMAGE_UPDATE: failed check, partition=%s, error=%d\n", partition == U_SPIFFS ? "spiffs" : "firmware", error);
    });
    bool restart = false;
    ota.setUpdateFinishedCb ([&] (int partition, bool _restart) {
        if (! updated)
            DEBUG_PRINTF ("\n");
        DEBUG_PRINTF ("OTA_IMAGE_UPDATE: success, partition=%s, restart=%d\n", partition == U_SPIFFS ? "spiffs" : "firmware", _restart);
        restart = _restart;
    });
    ota.execOTA ();
    if (func != nullptr)
        func ();
    if (restart)
        ESP.restart ();
}

// -----------------------------------------------------------------------------------------------

bool ota_image_update (const String &json, const String &type, const String &vers, const String &addr, const std::function<void ()> &func) {
    esp32FOTA ota (type.c_str (), vers.c_str ());
    char buffer [32] = { 0 };
    if (__ota_image_update_check (ota, json.c_str (), type.c_str (), vers.c_str (), addr.c_str (), buffer))
        __ota_image_update_execute (ota, buffer, func);
    return false;
}
bool ota_image_check (const String &json, const String &type, const String &vers, const String &addr, String *newr) {
    esp32FOTA ota (type.c_str (), vers.c_str ());
    char buffer [32] = { 0 };
    const bool result = __ota_image_update_check (ota, json.c_str (), type.c_str (), vers.c_str (), addr.c_str (), buffer);
    *newr = buffer;
    return result;
}

// -----------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------
