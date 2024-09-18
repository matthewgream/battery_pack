
// -----------------------------------------------------------------------------------------------

class ConnectManager;
static void __ConnectManager_WiFiEventHandler (WiFiEvent_t event);

class ConnectManager : public Component, public Diagnosticable, private Singleton <ConnectManager>  {
    const Config::ConnectConfig& config;
    ActivationTracker _connected, _disconnected;

public:
    ConnectManager (const Config::ConnectConfig& cfg) : Singleton <ConnectManager> (this), config (cfg) {}
    ~ConnectManager () { WiFi.removeEvent (__ConnectManager_WiFiEventHandler); }
    void begin () override {
        WiFi.onEvent (__ConnectManager_WiFiEventHandler, WiFiEvent_t::ARDUINO_EVENT_WIFI_STA_CONNECTED);
        WiFi.onEvent (__ConnectManager_WiFiEventHandler, WiFiEvent_t::ARDUINO_EVENT_WIFI_STA_DISCONNECTED);
        WiFi.setHostname (config.client.c_str ());
        WiFi.setAutoReconnect (true);
        WiFi.mode (WIFI_STA);
        WiFi.begin (config.ssid.c_str (), config.pass.c_str ());
    }
    void events (const WiFiEvent_t event) {
        if (event == WiFiEvent_t::ARDUINO_EVENT_WIFI_STA_CONNECTED)
            _connected ++;
        else if (event == WiFiEvent_t::ARDUINO_EVENT_WIFI_STA_DISCONNECTED)
            _disconnected ++;
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

RTC_DATA_ATTR long _persistentDriftMs = 0;
RTC_DATA_ATTR struct timeval _persistentTime = { .tv_sec = 0, .tv_usec = 0 };

class NettimeManager : public Component, public Alarmable, public Diagnosticable {
    const Config::NettimeConfig& config;
    NetworkTimeFetcher _fetcher;
    TimeDriftCalculator _drifter;
    ActivationTracker _activations;
    counter_t _failures = 0;
    interval_t _previousTimeUpdate = 0, _previousTimeAdjust = 0;
    time_t _previousTime = 0;

public:
    NettimeManager (const Config::NettimeConfig& cfg) : config (cfg), _fetcher (cfg.useragent, cfg.server), _drifter (_persistentDriftMs) {
      if (_persistentTime.tv_sec > 0)
          settimeofday (&_persistentTime, nullptr);
    }
    void process () override {
        interval_t currentTime = millis ();
        if (WiFi.isConnected ()) {
          if (currentTime - _previousTimeUpdate >= config.intervalUpdate) {
              const time_t fetchedTime = _fetcher.fetch ();
              if (fetchedTime > 0) {
                  _activations ++;
                  _persistentTime.tv_sec = fetchedTime; _persistentTime.tv_usec = 0;
                  settimeofday (&_persistentTime, nullptr);
                  if (_previousTime > 0)
                      _persistentDriftMs = _drifter.updateDrift (fetchedTime - _previousTime, currentTime - _previousTimeUpdate);
                  _previousTime = fetchedTime;
                  _previousTimeUpdate = currentTime;
                  _failures = 0;
              } else _failures ++;
          }
        }
        if (currentTime - _previousTimeAdjust >= config.intervalAdjust) {
            gettimeofday (&_persistentTime, nullptr);
            if (_drifter.applyDrift (_persistentTime, currentTime - _previousTimeAdjust) > 0)
                settimeofday (&_persistentTime, nullptr);
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
