
// -----------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------

#include <utility>

String __encodeUrlWithParameters (const String& result) {
    return result;
}
template <typename K, typename V, typename... R>
String __encodeUrlWithParameters (const String& result, K&& key, V&& value, R&&... rest) {
    return __encodeUrlWithParameters (result + (result.isEmpty () ? "?" : "&") + String (key) + "=" + String (value), std::forward <R> (rest)...);
}
template <typename... A>
String encodeUrlWithParameters (const String& url, A&&... args) {
    return url + __encodeUrlWithParameters ("", std::forward <A> (args)...);
}

// -----------------------------------------------------------------------------------------------

#include <flashz.hpp>
#include <esp32fota.h>
#include <WiFi.h>

static void __ota_image_update_progress (const size_t progress, const size_t size) {
    DEBUG_PRINTF (progress < size ? "." : "\n");
}
static void __ota_image_update_failure (const char *process, const int partition, const int error = 0) {
    DEBUG_PRINTF ("OTA_IMAGE_UPDATE: failed, process=%s, partition=%s, error=%d\n", process, partition == U_SPIFFS ? "spiffs" : "firmware", error ? error : -1);
}
static void __ota_image_update_success (const int partition, const bool restart) {
    DEBUG_PRINTF ("OTA_IMAGE_UPDATE: succeeded, partition=%s, restart=%d\n", partition == U_SPIFFS ? "spiffs" : "firmware", restart);
}
static bool __ota_image_update_check (esp32FOTA& ota, const char *json, const char *type, const char *vers, const char *addr, char *newr) {
    DEBUG_PRINTF ("OTA_IMAGE_CHECK: fetch json=%s, type=%s, vers=%s, addr=%s ...", json, type, vers, addr);
    ota.setManifestURL (encodeUrlWithParameters (json, "type", type, "vers", vers, "addr", addr).c_str ());
    const bool update = ota.execHTTPcheck ();
    if (update) {
        ota.getPayloadVersion (newr);
        DEBUG_PRINTF (" newer vers=%s\n", newr);
        return true;
    } else {
        DEBUG_PRINTF (" no newer vers\n");
        return false;
    }
}
static void __ota_image_update_execute (esp32FOTA& ota, const char* newr, const std::function <void ()> &func) {
    DEBUG_PRINTF ("OTA_IMAGE_UPDATE: downloading and installing, vers=%s\n", newr);
    ota.setProgressCb (__ota_image_update_progress);
    ota.setUpdateBeginFailCb ([](int partition) { __ota_image_update_failure ("begin", partition); });
    ota.setUpdateCheckFailCb ([](int partition, int error) { __ota_image_update_failure ("check", partition, error); });
    bool restart = false;
    ota.setUpdateFinishedCb ([&](int partition, bool _restart) { __ota_image_update_success (partition, _restart); restart = _restart; });
    ota.execOTA ();
    if (func != nullptr)
      func ();
    if (restart)
        ESP.restart ();
}

// -----------------------------------------------------------------------------------------------

void ota_image_update (const String& json, const String& type, const String& vers, const String& addr, const std::function <void ()> &func = nullptr) {
    esp32FOTA ota (type.c_str (), vers.c_str ());
    char newr [32] = { '\0' };
    if (__ota_image_update_check (ota, json.c_str (), type.c_str (), vers.c_str (), addr.c_str (), newr))
        __ota_image_update_execute (ota, newr, func);
}
String ota_image_check (const String& json, const String& type, const String& vers, const String& addr) {
    esp32FOTA ota (type.c_str (), vers.c_str ());
    char newr [32] = { '\0' };
    __ota_image_update_check (ota, json.c_str (), type.c_str (), vers.c_str (), addr.c_str (), newr);
    return newr;
}

// -----------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------
