
// -----------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------

#include <WiFi.h>

// -----------------------------------------------------------------------------------------------

class ProgramNetworkManager : private Singleton<ProgramNetworkManager>, public Component, public Diagnosticable { 
public:
    struct Peer {
        String ssid;
        String pass;
    };
    using PeersManager = ConnectionPeers<Peer>;

    typedef struct {
        String host;
        PeersManager::Config peers;
        interval_t intervalConnectionCheck;
    } Config;

private:
    //

    void events (const WiFiEvent_t event, const WiFiEventInfo_t info) {
        if (event == WiFiEvent_t::ARDUINO_EVENT_WIFI_STA_CONNECTED) {
            const int8_t rssi (WiFi.RSSI ());
            DEBUG_PRINTF ("NetworkManager::events: WIFI_CONNECTED, ssid=%s, bssid=%s, channel=%d, authmode=%s, rssi=%d (%s)\n",
                          _ssid_to_string (info.wifi_sta_connected.ssid, info.wifi_sta_connected.ssid_len).c_str (),
                          _bssid_to_string (info.wifi_sta_connected.bssid).c_str (),
                          (int) info.wifi_sta_connected.channel,
                          _authmode_to_string (info.wifi_sta_connected.authmode).c_str (),
                          rssi,
                          ConnectionSignal::toString (ConnectionSignal::signalQuality (rssi)).c_str ());
            _connected (rssi);
        } else if (event == WiFiEvent_t::ARDUINO_EVENT_WIFI_STA_GOT_IP) {
            const IPAddress address (info.got_ip.ip_info.ip.addr);
            DEBUG_PRINTF ("NetworkManager::events: WIFI_ALLOCATED, address=%s\n", address.toString ().c_str ());
            _connected_allocated (address);
        } else if (event == WiFiEvent_t::ARDUINO_EVENT_WIFI_STA_DISCONNECTED) {
            const String reason (_error_to_string ((wifi_err_reason_t) info.wifi_sta_disconnected.reason));
            DEBUG_PRINTF ("NetworkManager::events: WIFI_DISCONNECTED, ssid=%s, bssid=%s, reason=%s\n",
                          _ssid_to_string (info.wifi_sta_disconnected.ssid, info.wifi_sta_disconnected.ssid_len).c_str (),
                          _bssid_to_string (info.wifi_sta_disconnected.bssid).c_str (),
                          reason.c_str ());
            _disconnected (reason);
        }
    }
    static void __wiFiEventHandler (WiFiEvent_t event, WiFiEventInfo_t info) {
        ProgramNetworkManager *instance = Singleton<ProgramNetworkManager>::instance ();
        if (instance != nullptr)
            instance->events (event, info);
    }

    const Config &config;

    PeersManager _peers;
    MulticastDNS &_mdns;
    ConnectionSignal _connectionSignalTracker;
    Intervalable _intervalConnectionCheck;
    ActivationTracker _connections, _allocations;
    ActivationTrackerWithDetail _disconnections;
    IPAddress _address;
    bool _connectionActive = false, _connectionAvailable = false;

    void _connect () {
        if (! _connectionActive && _peers.available () > 0) {
            const Peer peer = _peers.select ();
            DEBUG_PRINTF ("NetworkManager::connect: ssid=%s, pass=%s, mac=%s, host=%s\n", peer.ssid.c_str (), peer.pass.c_str (), getMacAddressWifi ().c_str (), config.host.c_str ());
            WiFi.begin (peer.ssid.c_str (), peer.pass.c_str ());
            WiFi.setTxPower (WIFI_POWER_8_5dBm);    // XXX ?!? for AUTH_EXPIRE ... flash access problem ...  https://github.com/espressif/arduino-esp32/issues/2144
        }
    }
    void _connected (const int8_t rssi) {
        if (! _connectionActive) {
            _connectionSignalTracker.reset ();
            _intervalConnectionCheck.reset ();
            _connections++;
            _connectionActive = true;
            _connectionAvailable = false;
            _connectionSignalTracker.update (rssi);
            _peers.update (true);
        }
    }
    void _connected_allocated (const IPAddress &address) {    // careful, reallocations
        if (_connectionActive && ! _connectionAvailable) {
            _allocations++;
            _connectionAvailable = true;
            _address = address;
            _mdns.start (address, config.host);
        }
    }
    void _disconnected (const String &reason) {
        if (_connectionActive) {
            _mdns.stop ();
            _intervalConnectionCheck.reset ();
            _connectionAvailable = false;
            _connectionActive = false;
            _disconnections += reason;
        }
    }
    void _connection_init () {
        WiFi.persistent (false);
        WiFi.onEvent (__wiFiEventHandler);
        WiFi.setHostname (config.host.c_str ());
        WiFi.setAutoReconnect (true);
        WiFi.mode (WIFI_STA);
    }
    void _connection_process () {
        if (_connectionActive && _intervalConnectionCheck) {
            if (! _connectionAvailable) {
                DEBUG_PRINTF ("NetworkManager::check: connection timeout, restarting\n");
                WiFi.disconnect (true);
                const String reason ("LOCAL_TIMEOUT");
                _connectionActive = false;
                _disconnections += reason;
                _peers.update (false);
                _connect ();
            } else {
                _connectionSignalTracker.update (WiFi.RSSI ());
                DEBUG_PRINTF ("NetworkManager::process: rssi=%d (%s)\n", _connectionSignalTracker.rssi (), _connectionSignalTracker.toString ().c_str ());
            }
        }
    }

    //

public:
    explicit ProgramNetworkManager (const Config &cfg, MulticastDNS &mdns, const ConnectionSignal::Callback connectionSignalCallback = nullptr) :
        Singleton<ProgramNetworkManager> (this),
        _peers (config.peers, [] (const String &details) {    // ssid:pass
            return details.indexOf (':') != -1
                       ? Peer { .ssid = details.substring (0, details.indexOf (':')), .pass = details.substring (details.indexOf (':') + 1) }
                       : Peer { .ssid = details };
        }),

        config (cfg),
        _mdns (mdns),
        _connectionSignalTracker (connectionSignalCallback),
        _intervalConnectionCheck (config.intervalConnectionCheck) { }

    void begin () override {
        _connection_init ();
        _connect ();
    }
    void process () override {
        _connection_process ();
    }
    bool available () const {
        return _connectionAvailable;
    }

protected:
    void collectDiagnostics (JsonVariant &obj) const override {
        JsonObject sub = obj ["network"].to<JsonObject> ();
        sub ["macaddr"] = getMacAddressWifi ();
        if ((sub ["connected"] = _connectionActive)) {
            if ((sub ["available"] = _connectionAvailable))
                sub ["address"] = WiFi.localIP ();
            obj ["signal"] = _connectionSignalTracker;
        }
        if (_connections)
            sub ["connects"] = _connections;
        if (_allocations)
            sub ["allocations"] = _allocations;
        if (_disconnections)
            sub ["disconnects"] = _disconnections;
    }

private:
    static String _ssid_to_string (const uint8_t ssid [], const uint8_t ssid_len) {
        return String (reinterpret_cast<const char *> (ssid), ssid_len);
    }
    static String _bssid_to_string (const uint8_t bssid []) {
        return BytesToHexString<6> (bssid);
    }
    static String _authmode_to_string (const wifi_auth_mode_t authmode) {
        switch (authmode) {
        case WIFI_AUTH_OPEN :
            return "OPEN";
        case WIFI_AUTH_WEP :
            return "WEP";
        case WIFI_AUTH_WPA_PSK :
            return "WPA-PSK";
        case WIFI_AUTH_WPA2_PSK :
            return "WPA2-PSK";
        case WIFI_AUTH_WPA_WPA2_PSK :
            return "WPA/2-PSK";
        case WIFI_AUTH_WPA2_ENTERPRISE :
            return "ENTERPRISE";
        default :
            return "UNDEFINED_(" + ArithmeticToString (static_cast<int> (authmode)) + ")";
        }
    }
    // note: WiFi.disconnectReasonName
    static String _error_to_string (const wifi_err_reason_t reason) {
        switch (reason) {
        case WIFI_REASON_UNSPECIFIED :
            return "UNSPECIFIED";
        case WIFI_REASON_AUTH_EXPIRE :
            return "AUTH_EXPIRE";
        case WIFI_REASON_AUTH_LEAVE :
            return "AUTH_LEAVE";
        case WIFI_REASON_ASSOC_EXPIRE :
            return "ASSOC_EXPIRE";
        case WIFI_REASON_ASSOC_TOOMANY :
            return "ASSOC_TOOMANY";
        case WIFI_REASON_NOT_AUTHED :
            return "NOT_AUTHED";
        case WIFI_REASON_NOT_ASSOCED :
            return "NOT_ASSOCED";
        case WIFI_REASON_ASSOC_LEAVE :
            return "ASSOC_LEAVE";
        case WIFI_REASON_ASSOC_NOT_AUTHED :
            return "ASSOC_NOT_AUTHED";
        case WIFI_REASON_DISASSOC_PWRCAP_BAD :
            return "DISASSOC_PWRCAP_BAD";
        case WIFI_REASON_DISASSOC_SUPCHAN_BAD :
            return "DISASSOC_SUPCHAN_BAD";
        case WIFI_REASON_IE_INVALID :
            return "IE_INVALID";
        case WIFI_REASON_MIC_FAILURE :
            return "MIC_FAILURE";
        case WIFI_REASON_4WAY_HANDSHAKE_TIMEOUT :
            return "4WAY_HANDSHAKE_TIMEOUT";
        case WIFI_REASON_GROUP_KEY_UPDATE_TIMEOUT :
            return "GROUP_KEY_UPDATE_TIMEOUT";
        case WIFI_REASON_IE_IN_4WAY_DIFFERS :
            return "IE_IN_4WAY_DIFFERS";
        case WIFI_REASON_GROUP_CIPHER_INVALID :
            return "GROUP_CIPHER_INVALID";
        case WIFI_REASON_PAIRWISE_CIPHER_INVALID :
            return "PAIRWISE_CIPHER_INVALID";
        case WIFI_REASON_AKMP_INVALID :
            return "AKMP_INVALID";
        case WIFI_REASON_UNSUPP_RSN_IE_VERSION :
            return "UNSUPP_RSN_IE_VERSION";
        case WIFI_REASON_INVALID_RSN_IE_CAP :
            return "INVALID_RSN_IE_CAP";
        case WIFI_REASON_802_1X_AUTH_FAILED :
            return "802_1X_AUTH_FAILED";
        case WIFI_REASON_CIPHER_SUITE_REJECTED :
            return "CIPHER_SUITE_REJECTED";
        case WIFI_REASON_BEACON_TIMEOUT :
            return "BEACON_TIMEOUT";
        case WIFI_REASON_NO_AP_FOUND :
            return "NO_AP_FOUND";
        case WIFI_REASON_AUTH_FAIL :
            return "AUTH_FAIL";
        case WIFI_REASON_ASSOC_FAIL :
            return "ASSOC_FAIL";
        case WIFI_REASON_HANDSHAKE_TIMEOUT :
            return "HANDSHAKE_TIMEOUT";
        case WIFI_REASON_CONNECTION_FAIL :
            return "CONNECTION_FAIL";
        default :
            return "UNDEFINED_(" + ArithmeticToString (static_cast<int> (reason)) + ")";
        }
    }
};

// -----------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------
