
// -----------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------

#include <WiFi.h>

// -----------------------------------------------------------------------------------------------

class ConnectManager: private Singleton <ConnectManager>, public Component, public Diagnosticable {
public:
    typedef struct {
        String client, ssid, pass;
        interval_t intervalConnectionCheck;
    } Config;

private:
    const Config &config;

    ConnectionSignalTracker _connectionSignalTracker;
    bool _connected = false, _available = false;
    Intervalable _intervalConnectionCheck;
    ActivationTracker _connections, _allocations; ActivationTrackerWithDetail _disconnections;

    static void __wiFiEventHandler (WiFiEvent_t event, WiFiEventInfo_t info) {
        ConnectManager *connectManager = Singleton <ConnectManager>::instance ();
        if (connectManager != nullptr) connectManager->events (event, info);
    }
    void events (const WiFiEvent_t event, const WiFiEventInfo_t info) {
        if (event == WiFiEvent_t::ARDUINO_EVENT_WIFI_STA_CONNECTED) {
            _connectionSignalTracker.reset ();
            _intervalConnectionCheck.reset (); _connections ++; _connected = true;
            _connectionSignalTracker.update (WiFi.RSSI ());
            DEBUG_PRINTF ("ConnectManager::events: WIFI_CONNECTED, ssid=%s, bssid=%s, channel=%d, authmode=%s, rssi=%d (%s)\n",
                __wifi_ssid_to_string (info.wifi_sta_connected.ssid, info.wifi_sta_connected.ssid_len).c_str (),
                __wifi_bssid_to_string (info.wifi_sta_connected.bssid).c_str (), (int) info.wifi_sta_connected.channel,
                __wifi_authmode_to_string ((wifi_auth_mode_t) info.wifi_sta_connected.authmode).c_str (), _connectionSignalTracker.rssi (), _connectionSignalTracker.toString ().c_str ());
        } else if (event == WiFiEvent_t::ARDUINO_EVENT_WIFI_STA_GOT_IP) {
            _allocations ++; _available = true;
            DEBUG_PRINTF ("ConnectManager::events: WIFI_ALLOCATED, address=%s\n", IPAddress (info.got_ip.ip_info.ip.addr).toString ().c_str ());
        } else if (event == WiFiEvent_t::ARDUINO_EVENT_WIFI_STA_DISCONNECTED) {
            const String reason = __wifi_error_to_string ((wifi_err_reason_t) info.wifi_sta_disconnected.reason);
            _intervalConnectionCheck.reset (); _available = false; _connected = false; _disconnections += reason;
            DEBUG_PRINTF ("ConnectManager::events: WIFI_DISCONNECTED, ssid=%s, bssid=%s, reason=%s\n",
                __wifi_ssid_to_string (info.wifi_sta_disconnected.ssid, info.wifi_sta_disconnected.ssid_len).c_str (),
                __wifi_bssid_to_string (info.wifi_sta_disconnected.bssid).c_str (), reason.c_str ());
        }
    }

public:
    explicit ConnectManager (const Config& cfg, const ConnectionSignalTracker::Callback connectionSignalCallback = nullptr) : Singleton <ConnectManager> (this), config (cfg), _connectionSignalTracker (connectionSignalCallback), _intervalConnectionCheck (config.intervalConnectionCheck) {}
    void begin () override {
        WiFi.persistent (false);
        WiFi.onEvent (__wiFiEventHandler);
        WiFi.setHostname (config.client.c_str ());
        WiFi.setAutoReconnect (true);
        WiFi.mode (WIFI_STA);
        connect ();
    }
    void connect () {
        WiFi.begin (config.ssid.c_str (), config.pass.c_str ());
        DEBUG_PRINTF ("ConnectManager::connect: ssid=%s, pass=%s, mac=%s, host=%s\n", config.ssid.c_str (), config.pass.c_str (), getMacAddress ().c_str (), config.client.c_str ());
    }
    void reset () {
        DEBUG_PRINTF ("ConnectManager::reset\n");
        const String reason = "LOCAL_TIMEOUT";
        _available = false; _connected = false; _disconnections += reason;
        WiFi.disconnect (true);
    }
    void process () override {
        if (_connected && _intervalConnectionCheck) {
            if (!_available) {
                DEBUG_PRINTF ("ConnectManager::check: connection timeout, resetting\n");
                reset ();
            } else {
                _connectionSignalTracker.update (WiFi.RSSI ());
                DEBUG_PRINTF ("ConnectManager::process: rssi=%d (%s)\n", _connectionSignalTracker.rssi (), _connectionSignalTracker.toString ().c_str ());
            }
        // } else if (!_connected && _intervalConnectionCheck) {
        //     WiFi.disconnect (true);
        //     connect ();
        }
    }
    inline bool isAvailable () const {
        return _available;
    }

protected:
    void collectDiagnostics (JsonVariant &obj) const override {
        JsonObject network = obj ["network"].to <JsonObject> ();
        network ["macaddr"] = getMacAddress ();
        if ((network ["connected"] = _connected)) {
            if ((network ["available"] = _available))
                network ["ipaddr"] = WiFi.localIP ();
            obj ["signal"] = _connectionSignalTracker;
        }
        if (_connections.count () > 0)
            network ["connects"] = _connections;
        if (_allocations.count () > 0)
            network ["allocations"] = _allocations;
        if (_disconnections.count () > 0)
            network ["disconnects"] = _disconnections;
    }

private:
    static String __wifi_ssid_to_string (const uint8_t ssid [], const uint8_t ssid_len) {
        return String (reinterpret_cast <const char*> (ssid), ssid_len);
    }
    static String __wifi_bssid_to_string (const uint8_t bssid []) {
        return hexabyte_to_hexastring (bssid);
    }
    static String __wifi_authmode_to_string (const wifi_auth_mode_t authmode) {
        switch (authmode) {
            case WIFI_AUTH_OPEN: return "OPEN";
            case WIFI_AUTH_WEP: return "WEP";
            case WIFI_AUTH_WPA_PSK: return "WPA-PSK";
            case WIFI_AUTH_WPA2_PSK: return "WPA2-PSK";
            case WIFI_AUTH_WPA_WPA2_PSK: return "WPA/2-PSK";
            case WIFI_AUTH_WPA2_ENTERPRISE: return "ENTERPRISE";
            default: return "UNDEFINED_(" + ArithmeticToString (static_cast <int> (authmode)) + ")";
        }
    }
    // note: WiFi.disconnectReasonName
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
            default: return "UNDEFINED_(" + ArithmeticToString (static_cast <int> (reason)) + ")";
        }
    }
};

// -----------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------

class NettimeManager : public Component, public Alarmable, public Diagnosticable {
public:
    typedef struct {
        String useragent, server;
        interval_t intervalUpdate, intervalAdjust;
        int failureLimit;
    } Config;

    using BooleanFunc = std::function <bool ()>;

private:
    const Config &config;

    const BooleanFunc _networkIsAvailable;

    NetworkTimeFetcher _fetcher;
    ActivationTrackerWithDetail _fetches;

    PersistentData _persistentData;
    PersistentValue <long> _persistentDrift;
    TimeDriftCalculator _drifter;

    counter_t _failures = 0;
    interval_t _previousTimeUpdate = 0, _previousTimeAdjust = 0;
    time_t _previousTime = 0;
    PersistentValue <uint32_t> _persistentTime;

public:
    explicit NettimeManager (const Config& cfg, const BooleanFunc networkIsAvailable): Alarmable ({
            AlarmCondition (ALARM_TIME_SYNC, [this] () { return _failures > config.failureLimit; }),
            AlarmCondition (ALARM_TIME_DRIFT, [this] () { return _drifter.isHighDrift (); })
        }), config (cfg), _networkIsAvailable (networkIsAvailable), _fetcher (cfg.useragent, cfg.server),
            _persistentData ("nettime"), _persistentDrift (_persistentData, "drift", 0), _drifter (_persistentDrift), _persistentTime (_persistentData, "time", 0) {
        if (_persistentTime > 0UL) {
            struct timeval tv = { .tv_sec = _persistentTime, .tv_usec = 0 };
            settimeofday (&tv, nullptr);
        }
        DEBUG_PRINTF ("NettimeManager::constructor: persistentTime=%lu, persistentDrift=%ld, time=%s\n", (unsigned long) _persistentTime, (long) _persistentDrift, getTimeString ().c_str ());
    }
    void process () override {
        interval_t currentTime = millis ();
        if (_networkIsAvailable ()) {
          if (!_previousTimeUpdate || (currentTime - _previousTimeUpdate >= config.intervalUpdate)) {
              const time_t fetchedTime = _fetcher.fetch ();
              if (fetchedTime > 0) {
                  _fetches += ArithmeticToString (fetchedTime);
                  const struct timeval tv = { .tv_sec = fetchedTime, .tv_usec = 0 };
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

protected:
    void collectDiagnostics (JsonVariant &obj) const override {
        JsonObject nettime = obj ["nettime"].to <JsonObject> ();
        nettime ["now"] = getTimeString ();
        nettime ["drift"] = _drifter.drift ();
        if (_fetches.count () > 0)
            nettime ["fetches"] = _fetches;
    }
};

// -----------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------
