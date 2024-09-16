
// -----------------------------------------------------------------------------------------------

static Upstamp __ConnectManager_WiFiEvents_connected;
static Upstamp __ConnectManager_WiFiEvents_disconnected;
static void __ConnectManager_WiFiEvents (WiFiEvent_t event) {
    if (event == ARDUINO_EVENT_WIFI_STA_CONNECTED)
        __ConnectManager_WiFiEvents_connected ++;
    else if (event == ARDUINO_EVENT_WIFI_STA_DISCONNECTED)
        __ConnectManager_WiFiEvents_disconnected ++;
}

class ConnectManager : public Component, public Diagnosticable  {
    const Config::ConnectConfig& config;
public:
    ConnectManager (const Config::ConnectConfig& cfg) : config (cfg) {}
    void begin () override {
        WiFi.onEvent (__ConnectManager_WiFiEvents);
        WiFi.setHostname (config.host.c_str ());
        WiFi.setAutoReconnect (true);
        WiFi.mode (WIFI_STA);
        WiFi.begin (config.ssid.c_str (), config.pass.c_str ());
    }
    void collect (JsonObject &obj) const override {
        JsonObject wifi = obj ["wifi"].to <JsonObject> ();
        wifi ["mac"] = mac_address ();
        if ((wifi ["connected"] = WiFi.isConnected ())) {
            wifi ["address"] = WiFi.localIP ();
            wifi ["rssi"] = WiFi.RSSI ();
        }
        JsonObject connect = wifi ["connect"].to <JsonObject> ();
        connect ["last"] = __ConnectManager_WiFiEvents_connected.seconds ();
        connect ["numb"] = __ConnectManager_WiFiEvents_connected.number ();
        JsonObject disconnect = wifi ["disconnect"].to <JsonObject> ();
        disconnect ["last"] = __ConnectManager_WiFiEvents_disconnected.seconds ();
        disconnect ["numb"] = __ConnectManager_WiFiEvents_disconnected.number ();
    }
};

// -----------------------------------------------------------------------------------------------

RTC_DATA_ATTR long _persistentDriftMs = 0;
RTC_DATA_ATTR struct timeval _persistentTime = { .tv_sec = 0, .tv_usec = 0 };

class NettimeManager : public Component, public Alarmable, public Diagnosticable {
    const Config::NettimeConfig& config;
    NetworkTimeFetcher _fetcher;
    TimeDriftCalculator _drifter;
    unsigned long _previousTimeUpdate = 0, _previousTimeAdjust = 0;
    time_t _previousTime = 0;
    Upstamp _last;
public:
    NettimeManager (const Config::NettimeConfig& cfg) : config (cfg), _fetcher (cfg.server), _drifter (_persistentDriftMs) {
      if (_persistentTime.tv_sec > 0)
          settimeofday (&_persistentTime, nullptr);
    }
    void process () override {
        unsigned long currentTime = millis ();
        if (WiFi.isConnected ()) {
          if (currentTime - _previousTimeUpdate >= config.intervalUpdate) {
              const time_t fetchedTime = _fetcher.fetch ();
              if (fetchedTime > 0) {
                  _last ++;
                  _persistentTime.tv_sec = fetchedTime; _persistentTime.tv_usec = 0;
                  settimeofday (&_persistentTime, nullptr);
                  if (_previousTime > 0)
                      _persistentDriftMs = _drifter.updateDrift (fetchedTime - _previousTime, currentTime - _previousTimeUpdate);
                  _previousTime = fetchedTime;
                  _previousTimeUpdate = currentTime;
              }
          }
        }
        if (currentTime - _previousTimeAdjust >= config.intervalAdjust) {
            gettimeofday (&_persistentTime, nullptr);
            if (_drifter.applyDrift (_persistentTime, currentTime - _previousTimeAdjust) > 0)
                settimeofday (&_persistentTime, nullptr);
            _previousTimeAdjust = currentTime;
        }
    }
    void collect (JsonObject &obj) const override {
        JsonObject nettime = obj ["nettime"].to <JsonObject> ();
        nettime ["now"] = getTimeString ();
        nettime ["drift"] = _drifter.drift ();
        JsonObject update = nettime ["update"].to <JsonObject> ();
        update ["last"] = _last.seconds ();
        update ["numb"] = _last.number ();
    }
    //
    AlarmSet alarm () const override {
        AlarmSet alarms = ALARM_NONE;
        if (_fetcher.failures () > config.failureLimit) alarms |= ALARM_TIME_NETWORK;
        if (_drifter.highDrift ()) alarms |= ALARM_TIME_DRIFT;
        return alarms;
    }
    String getTimeString () const {
        struct tm timeinfo;
        char timeString [sizeof ("yyyy-mm-ddThh:mm:ssZ") + 1] = { '\0' };
        if (getLocalTime (&timeinfo))
            strftime (timeString, sizeof (timeString), "%Y-%m-%dT%H:%M:%SZ", &timeinfo);
        return String (timeString);
    }
};

// -----------------------------------------------------------------------------------------------
