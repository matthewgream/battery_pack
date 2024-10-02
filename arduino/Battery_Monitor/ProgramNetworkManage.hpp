
// -----------------------------------------------------------------------------------------------

static String __wifi_ssid_to_string (const uint8_t ssid [], const uint8_t ssid_len) {
    return String (reinterpret_cast <const char*> (ssid), ssid_len);
}
static String __wifi_bssid_to_string (const uint8_t bssid []) {
#define __BSSID_MACBYTETOSTRING(byte) String (NIBBLE_TO_HEX_CHAR ((byte) >> 4)) + String (NIBBLE_TO_HEX_CHAR ((byte) & 0xF))
#define __BSSID_FORMAT_BSSID(addr) __BSSID_MACBYTETOSTRING ((addr)[0]) + ":" + __BSSID_MACBYTETOSTRING ((addr)[1]) + ":" + __BSSID_MACBYTETOSTRING ((addr)[2]) + ":" + __BSSID_MACBYTETOSTRING ((addr)[3]) + ":" + __BSSID_MACBYTETOSTRING ((addr)[4]) + ":" + __BSSID_MACBYTETOSTRING ((addr)[5])
    return __BSSID_FORMAT_BSSID (bssid);
}
static String __wifi_authmode_to_string (const wifi_auth_mode_t authmode) {
    switch (authmode) {
        case WIFI_AUTH_OPEN: return "OPEN";
        case WIFI_AUTH_WEP: return "WEP";
        case WIFI_AUTH_WPA_PSK: return "WPA-PSK";
        case WIFI_AUTH_WPA2_PSK: return "WPA2-PSK";
        case WIFI_AUTH_WPA_WPA2_PSK: return "WPA/2-PSK";
        case WIFI_AUTH_WPA2_ENTERPRISE: return "ENTERPRISE";
        default: return "UNDEFINED";
    }
}
static String __wifi_error_to_string (const wifi_err_reason_t reason) {
    switch (reason) {
        case WIFI_REASON_UNSPECIFIED: return "UNSPECIFIED";
        case WIFI_REASON_AUTH_EXPIRE: return "AUTH_EXPIRE";
        case WIFI_REASON_AUTH_LEAVE: return "AUTH_LEAVE";
        case WIFI_REASON_ASSOC_EXPIRE: return "ASSOC_EXPIRE";
        case WIFI_REASON_ASSOC_TOOMANY: return "ASSOC_TOOMANY";
        case WIFI_REASON_NOT_AUTHED: return "NOT_AUTHED";
        case WIFI_REASON_NOT_ASSOCED: return "NOT_ASSOCED";
        case WIFI_REASON_ASSOC_LEAVE: return "ASSOC_LEAVE";
        case WIFI_REASON_ASSOC_NOT_AUTHED: return "ASSOC_NOT_AUTHED";
        case WIFI_REASON_DISASSOC_PWRCAP_BAD: return "DISASSOC_PWRCAP_BAD";
        case WIFI_REASON_DISASSOC_SUPCHAN_BAD: return "DISASSOC_SUPCHAN_BAD";
        case WIFI_REASON_IE_INVALID: return "IE_INVALID";
        case WIFI_REASON_MIC_FAILURE: return "MIC_FAILURE";
        case WIFI_REASON_4WAY_HANDSHAKE_TIMEOUT: return "4WAY_HANDSHAKE_TIMEOUT";
        case WIFI_REASON_GROUP_KEY_UPDATE_TIMEOUT: return "GROUP_KEY_UPDATE_TIMEOUT";
        case WIFI_REASON_IE_IN_4WAY_DIFFERS: return "IE_IN_4WAY_DIFFERS";
        case WIFI_REASON_GROUP_CIPHER_INVALID: return "GROUP_CIPHER_INVALID";
        case WIFI_REASON_PAIRWISE_CIPHER_INVALID: return "PAIRWISE_CIPHER_INVALID";
        case WIFI_REASON_AKMP_INVALID: return "AKMP_INVALID";
        case WIFI_REASON_UNSUPP_RSN_IE_VERSION: return "UNSUPP_RSN_IE_VERSION";
        case WIFI_REASON_INVALID_RSN_IE_CAP: return "INVALID_RSN_IE_CAP";
        case WIFI_REASON_802_1X_AUTH_FAILED: return "802_1X_AUTH_FAILED";
        case WIFI_REASON_CIPHER_SUITE_REJECTED: return "CIPHER_SUITE_REJECTED";
        case WIFI_REASON_BEACON_TIMEOUT: return "BEACON_TIMEOUT";
        case WIFI_REASON_NO_AP_FOUND: return "NO_AP_FOUND";
        case WIFI_REASON_AUTH_FAIL: return "AUTH_FAIL";
        case WIFI_REASON_ASSOC_FAIL: return "ASSOC_FAIL";
        case WIFI_REASON_HANDSHAKE_TIMEOUT: return "HANDSHAKE_TIMEOUT";
        case WIFI_REASON_CONNECTION_FAIL: return "CONNECTION_FAIL";
        default: return "UNDEFINED";
    }
}

class ConnectManager;
static void __ConnectManager_WiFiEventHandler (WiFiEvent_t event, WiFiEventInfo_t info);

class ConnectManager : public Component, public Diagnosticable, private Singleton <ConnectManager>  {

public:
    typedef struct {
        String client, ssid, pass;
    } Config;

private:
    const Config &config;
    ActivationTracker _connections, _allocations; ActivationTrackerWithDetail _disconnections;
    bool _available = false;
    WiFiEventId_t _events = 0;

public:
    ConnectManager (const Config& cfg) : Singleton <ConnectManager> (this), config (cfg) {}
    ~ConnectManager () { if (_events) WiFi.removeEvent (_events); }
    void begin () override {
        _events = WiFi.onEvent (__ConnectManager_WiFiEventHandler);
        WiFi.setHostname (config.client.c_str ());
        WiFi.setAutoReconnect (true);
        WiFi.mode (WIFI_STA);
        WiFi.begin (config.ssid.c_str (), config.pass.c_str ());
        DEBUG_PRINTF ("ConnectManager::begin: ssid=%s, pass=%s, mac=%s, host=%s\n", config.ssid.c_str (), config.pass.c_str (), mac_address ().c_str (), config.client.c_str ());
    }
    inline bool isAvailable () const {
        return _available;
    }

protected:
    friend void __ConnectManager_WiFiEventHandler (WiFiEvent_t event, WiFiEventInfo_t info);
    void events (const WiFiEvent_t event, const WiFiEventInfo_t info) {
        if (event == WiFiEvent_t::ARDUINO_EVENT_WIFI_STA_CONNECTED) {
            _connections ++;
            DEBUG_PRINTF ("ConnectManager::events: WIFI_CONNECTED, ssid=%s, bssid=%s, channel=%d, authmode=%s, rssi=%d\n",
                __wifi_ssid_to_string (info.wifi_sta_connected.ssid, info.wifi_sta_connected.ssid_len).c_str (),
                __wifi_bssid_to_string (info.wifi_sta_connected.bssid).c_str (), (int) info.wifi_sta_connected.channel,
                __wifi_authmode_to_string ((wifi_auth_mode_t) info.wifi_sta_connected.authmode).c_str (), WiFi.RSSI ());
        } else if (event == WiFiEvent_t::ARDUINO_EVENT_WIFI_STA_GOT_IP) {
            _available = true; _allocations ++;
            DEBUG_PRINTF ("ConnectManager::events: WIFI_ALLOCATED, address=%s\n", IPAddress (info.got_ip.ip_info.ip.addr).toString ().c_str ());
        } else if (event == WiFiEvent_t::ARDUINO_EVENT_WIFI_STA_DISCONNECTED) {
            const String reason = __wifi_error_to_string ((wifi_err_reason_t) info.wifi_sta_disconnected.reason);
            _available = false; _disconnections += reason;
            DEBUG_PRINTF ("ConnectManager::events: WIFI_DISCONNECTED, ssid=%s, bssid=%s, reason=%s\n",
                __wifi_ssid_to_string (info.wifi_sta_disconnected.ssid, info.wifi_sta_disconnected.ssid_len).c_str (),
                __wifi_bssid_to_string (info.wifi_sta_disconnected.bssid).c_str (), 
                reason.c_str ());
        }
    }

protected:
    void collectDiagnostics (JsonDocument &obj) const override {
        JsonObject network = obj ["network"].to <JsonObject> ();
        network ["macaddr"] = mac_address ();
        if ((network ["connected"] = WiFi.isConnected ())) {
            network ["ipaddr"] = WiFi.localIP ();
            network ["rssi"] = WiFi.RSSI ();
        }
        _connections.serialize (network ["connects"].to <JsonObject> ());
        _allocations.serialize (network ["allocations"].to <JsonObject> ());
        _disconnections.serialize (network ["disconnects"].to <JsonObject> ());
    }
};

static void __ConnectManager_WiFiEventHandler (WiFiEvent_t event, WiFiEventInfo_t info) {
    ConnectManager *connectManager = Singleton <ConnectManager>::instance ();
    if (connectManager != nullptr) connectManager->events (event, info);
}

// -----------------------------------------------------------------------------------------------

class NettimeManager : public Component, public Alarmable, public Diagnosticable {

public:
    typedef struct {
        String useragent, server;
        interval_t intervalUpdate, intervalAdjust;
        int failureLimit;
    } Config;

private:
    const Config &config;
    ConnectManager &_network;
    NetworkTimeFetcher _fetcher;
    PersistentData _persistentData;
    PersistentValue <long> _persistentDrift;
    TimeDriftCalculator _drifter;
    ActivationTrackerWithDetail _fetches;
    counter_t _failures = 0;
    interval_t _previousTimeUpdate = 0, _previousTimeAdjust = 0;
    time_t _previousTime = 0;
    PersistentValue <uint32_t> _persistentTime;

public:
    NettimeManager (const Config& cfg, ConnectManager &network) : config (cfg), _network (network), _fetcher (cfg.useragent, cfg.server), _persistentData ("nettime"), _persistentDrift (_persistentData, "drift", 0), _drifter (_persistentDrift), _persistentTime (_persistentData, "time", 0) {
      if (_persistentTime > 0UL) {
          struct timeval tv = { .tv_sec = _persistentTime, .tv_usec = 0 };
          settimeofday (&tv, nullptr);
      }
      DEBUG_PRINTF ("NettimeManager::constructor: persistentTime=%lu, persistentDrift=%ld, time=%s\n", (unsigned long) _persistentTime, (long) _persistentDrift, getTimeString ().c_str ());
    }
    void process () override {
        interval_t currentTime = millis ();
        if (_network.isAvailable ()) {
          if (!_previousTimeUpdate || (currentTime - _previousTimeUpdate >= config.intervalUpdate)) {
              const time_t fetchedTime = _fetcher.fetch ();
              if (fetchedTime > 0) {
                  _fetches += IntToString (fetchedTime);
                  struct timeval tv = { .tv_sec = fetchedTime, .tv_usec = 0 };
                  settimeofday (&tv, nullptr);
                  if (_previousTime > 0)
                      _persistentDrift = _drifter.updateDrift (fetchedTime - _previousTime, currentTime - _previousTimeUpdate);
                  _previousTime = fetchedTime;
                  _previousTimeUpdate = currentTime;
                  _persistentTime = (uint32_t) fetchedTime;
                  _failures = 0;
                  DEBUG_PRINTF ("NettimeManager::process: time=%s\n", getTimeString ().c_str ());
              } else _failures ++;
          }
        }
        if (currentTime - _previousTimeAdjust >= config.intervalAdjust) {
            struct timeval tv;
            gettimeofday (&tv, nullptr);
            if (_drifter.applyDrift (tv, currentTime - _previousTimeAdjust) > 0) {
                settimeofday (&tv, nullptr);
                _persistentTime = tv.tv_sec;
            }
            _previousTimeAdjust = currentTime;
        }
    }
    String getTimeString () const {
        struct tm timeinfo;
        char timeString [sizeof ("yyyy-mm-ddThh:mm:ssZ") + 1] = { '\0' };
        if (getLocalTime (&timeinfo))
            strftime (timeString, sizeof (timeString), "%Y-%m-%dT%H:%M:%SZ", &timeinfo);
        return String (timeString);
    }
    inline operator String () const { return getTimeString (); }

protected:
    void collectAlarms (AlarmSet& alarms) const override {
        if (_failures > config.failureLimit) alarms += ALARM_TIME_NETWORK;
        if (_drifter.isHighDrift ()) alarms += ALARM_TIME_DRIFT;
    }
    void collectDiagnostics (JsonDocument &obj) const override {
        JsonObject nettime = obj ["nettime"].to <JsonObject> ();
        nettime ["now"] = getTimeString ();
        nettime ["drift"] = _drifter.drift ();
        _fetches.serialize (nettime ["fetches"].to <JsonObject> ());
    }
};

// -----------------------------------------------------------------------------------------------
