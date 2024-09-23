
// -----------------------------------------------------------------------------------------------

class ConnectManager;
static void __ConnectManager_WiFiEventHandler (WiFiEvent_t event);

class ConnectManager : public Component, public Diagnosticable, private Singleton <ConnectManager>  {
    const Config::ConnectConfig& config;
    ActivationTracker _connected, _disconnected;
    bool _available = false;

public:
    ConnectManager (const Config::ConnectConfig& cfg) : Singleton <ConnectManager> (this), config (cfg) {}
    ~ConnectManager () { WiFi.removeEvent (__ConnectManager_WiFiEventHandler); }
    void begin () override {
        WiFi.onEvent (__ConnectManager_WiFiEventHandler, WiFiEvent_t::ARDUINO_EVENT_WIFI_STA_CONNECTED);
        WiFi.onEvent (__ConnectManager_WiFiEventHandler, WiFiEvent_t::ARDUINO_EVENT_WIFI_STA_GOT_IP);
        WiFi.onEvent (__ConnectManager_WiFiEventHandler, WiFiEvent_t::ARDUINO_EVENT_WIFI_STA_DISCONNECTED);
        WiFi.setHostname (config.client.c_str ());
        WiFi.setAutoReconnect (true);
        WiFi.mode (WIFI_STA);
        WiFi.begin (config.ssid.c_str (), config.pass.c_str ());
        DEBUG_PRINTF ("ConnectManager::begin: ssid='%s', pass='%s', mac='%s', host='%s'\n", config.ssid.c_str (), config.pass.c_str (), mac_address ().c_str (), config.client.c_str ());
    }
    bool isAvailable () const {
        return _available;
    }

protected:
    friend void __ConnectManager_WiFiEventHandler (WiFiEvent_t event);
    void events (const WiFiEvent_t event) {
        if (event == WiFiEvent_t::ARDUINO_EVENT_WIFI_STA_CONNECTED) {
            _connected ++;
            DEBUG_PRINTF ("ConnectManager::events: WIFI_CONNECTED, rssi=%d\n", WiFi.RSSI ());
        } else if (event == WiFiEvent_t::ARDUINO_EVENT_WIFI_STA_GOT_IP) {
            _available = true;
            DEBUG_PRINTF ("ConnectManager::events: WIFI_ALLOCATED, localIP='%s'\n", WiFi.localIP ().toString ().c_str ());
        } else if (event == WiFiEvent_t::ARDUINO_EVENT_WIFI_STA_DISCONNECTED) {
            _available = false;
            _disconnected ++;
            DEBUG_PRINTF ("ConnectManager::events: WIFI_DISCONNECTED\n");
        }
    }

protected:
    void collectDiagnostics (JsonObject &obj) const override {
        JsonObject wifi = obj ["wifi"].to <JsonObject> ();
        wifi ["mac"] = mac_address ();
        if ((wifi ["connected"] = WiFi.isConnected ())) {
            wifi ["address"] = WiFi.localIP ();
            wifi ["rssi"] = WiFi.RSSI ();
        }
        _connected.serialize (wifi ["connected"].to <JsonObject> ());
        _disconnected.serialize (wifi ["disconnected"].to <JsonObject> ());
    }
};

static void __ConnectManager_WiFiEventHandler (WiFiEvent_t event) {
    ConnectManager *connectManager = Singleton <ConnectManager>::instance ();
    if (connectManager != nullptr) connectManager->events (event);
}

// -----------------------------------------------------------------------------------------------

class NettimeManager : public Component, public Alarmable, public Diagnosticable {
    const Config::NettimeConfig& config;
    ConnectManager &_network;
    NetworkTimeFetcher _fetcher;
    PersistentValue <long> _persistentDrift;
    TimeDriftCalculator _drifter;
    ActivationTracker _activations;
    counter_t _failures = 0;
    interval_t _previousTimeUpdate = 0, _previousTimeAdjust = 0;
    time_t _previousTime = 0;
    PersistentValue <uint32_t> _persistentTime;

public:
    NettimeManager (const Config::NettimeConfig& cfg, ConnectManager &network) : config (cfg), _network (network), _fetcher (cfg.useragent, cfg.server), _persistentDrift ("nettime", "drift", 0), _drifter (_persistentDrift), _persistentTime ("nettime", "time", 0) {
      if (_persistentTime > 0UL) {
          struct timeval tv = { .tv_sec = _persistentTime, .tv_usec = 0 };
          settimeofday (&tv, nullptr);
      }
      DEBUG_PRINTF ("NettimeManager::constructor: persistentTime=%lu, persistentDrift=%ld, time='%s'\n", (unsigned long) _persistentTime, (long) _persistentDrift, getTimeString ().c_str ());
    }
    void process () override {
        interval_t currentTime = millis ();
        if (_network.isAvailable ()) {
          if (!_previousTimeUpdate || (currentTime - _previousTimeUpdate >= config.intervalUpdate)) {
              const time_t fetchedTime = _fetcher.fetch ();
              if (fetchedTime > 0) {
                  _activations ++;
                  struct timeval tv = { .tv_sec = fetchedTime, .tv_usec = 0 };
                  settimeofday (&tv, nullptr);
                  if (_previousTime > 0)
                      _persistentDrift = _drifter.updateDrift (fetchedTime - _previousTime, currentTime - _previousTimeUpdate);
                  _previousTime = fetchedTime;
                  _previousTimeUpdate = currentTime;
                  _persistentTime = (uint32_t) fetchedTime;
                  _failures = 0;
                  DEBUG_PRINTF ("NettimeManager::process: time='%s'\n", getTimeString ().c_str ());
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
    operator String () const { return getTimeString (); }

protected:
    void collectAlarms (AlarmSet& alarms) const override {
        if (_failures > config.failureLimit) alarms += ALARM_TIME_NETWORK;
        if (_drifter.highDrift ()) alarms += ALARM_TIME_DRIFT;
    }
    void collectDiagnostics (JsonObject &obj) const override {
        JsonObject nettime = obj ["nettime"].to <JsonObject> ();
        nettime ["now"] = getTimeString ();
        nettime ["drift"] = _drifter.drift ();
        _activations.serialize (nettime ["fetched"].to <JsonObject> ());
    }
};

// -----------------------------------------------------------------------------------------------
