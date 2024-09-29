
// -----------------------------------------------------------------------------------------------

#include <HTTPClient.h>
#include <ctime>

class NetworkTimeFetcher {
    const String _useragent, _server;

public:
    NetworkTimeFetcher (const String& useragent, const String& server) : _useragent (useragent), _server (server) {}
    time_t fetch () {
        HTTPClient client;
        String header = "";
        time_t time = 0;
        client.setUserAgent (_useragent);
        client.begin (_server);
        const char *headerList [] = { "Date" };
        client.collectHeaders (headerList, sizeof (headerList) / sizeof (headerList [0]));
        if (client.sendRequest ("HEAD") > 0) {
            header = client.header ("Date");
            if (!header.isEmpty ()) {
                struct tm timeinfo;
                if (strptime (header.c_str (), "%a, %d %b %Y %H:%M:%S GMT", &timeinfo) != NULL)
                    time = mktime (&timeinfo);
            }
        }
        client.end ();
        DEBUG_PRINTF ("NetworkTimeFetcher::fetch: server=%s, header=[%s], time=%lu\n", _server.c_str (), header.c_str (), (unsigned long) time);
        return time;
    }
};

/* perhaps just rely upon SNTP
sntp_setoperatingmode(SNTP_OPMODE_POLL);
sntp_setservername(0, "pool.ntp.org");
sntp_init();
https://github.com/espressif/esp-idf/blob/v4.3/examples/protocols/sntp/main/sntp_example_main.c
*/

// -----------------------------------------------------------------------------------------------

#include <algorithm>
#include <ctime>

class TimeDriftCalculator {
    long _driftMs;
    bool _highDrift = false;
    static constexpr long MAX_DRIFT_MS = 60 * 1000;

public:
    TimeDriftCalculator (const long driftMs) : _driftMs (driftMs) {}
    long updateDrift (time_t periodSecs, const interval_t periodMs) {
        long driftMs = (((periodSecs * 1000) - periodMs) * (60 * 60 * 1000)) / periodMs; // ms per hour
        driftMs = (_driftMs * 3 + driftMs) / 4; // 75% old value, 25% new value
        if (driftMs > MAX_DRIFT_MS || driftMs < -MAX_DRIFT_MS) _highDrift = true;
        _driftMs = std::clamp (driftMs, -MAX_DRIFT_MS, MAX_DRIFT_MS);
        DEBUG_PRINTF ("TimeDriftCalculator::updateDrift: driftMs=%ld\n", _driftMs);
        return _driftMs;
    }
    long applyDrift (struct timeval &currentTime, const interval_t periodMs) {
        const long adjustMs = (_driftMs * periodMs) / (60 * 60 * 1000);
        currentTime.tv_sec += adjustMs / 1000;
        currentTime.tv_usec += (adjustMs % 1000) * 1000;
        if (currentTime.tv_usec >= 1000000) {
            currentTime.tv_sec += currentTime.tv_usec / 1000000;
            currentTime.tv_usec %= 1000000;
        } else if (currentTime.tv_usec < 0) {
            currentTime.tv_sec -= 1 + (-currentTime.tv_usec / 1000000);
            currentTime.tv_usec = 1000000 - (-currentTime.tv_usec % 1000000);
        }
        DEBUG_PRINTF ("TimeDriftCalculator::applyDrift: adjustMs=%ld\n", adjustMs);
        return adjustMs;
    }
    bool highDrift () const {
      return _highDrift;
    }
    long drift () const {
      return _driftMs;
    }
};


// -----------------------------------------------------------------------------------------------

#include <ArduinoJson.h>

#include <WiFi.h>
#include <PubSubClient.h>

static String __mqtt_state_to_string (int state) {
    switch (state) {
      case MQTT_CONNECTION_TIMEOUT: return "CONNECTION_TIMEOUT";
      case MQTT_CONNECTION_LOST: return "CONNECTION_LOST";
      case MQTT_CONNECT_FAILED: return "CONNECT_FAILED";
      case MQTT_CONNECTED: return "CONNECTED";
      case MQTT_CONNECT_BAD_PROTOCOL: return "CONNECT_BAD_PROTOCOL";
      case MQTT_CONNECT_BAD_CLIENT_ID: return "CONNECT_BAD_CLIENT_ID";
      case MQTT_CONNECT_UNAVAILABLE: return "CONNECT_UNAVAILABLE";
      case MQTT_CONNECT_BAD_CREDENTIALS: return "CONNECT_BAD_CREDENTIALS";
      case MQTT_CONNECT_UNAUTHORIZED: return "CONNECT_UNAUTHORIZED";
      default: return "UNDEFINED";
    }
}

class MQTTPublisher {

public:
    typedef struct {
        const String client, host, user, pass, topic;
        const uint16_t port;
        const uint16_t bufferSize;
    } Config;
private:
    const Config& config;

    WiFiClient _wifiClient;
    PubSubClient _mqttClient;

    bool connect () {
        const bool result = _mqttClient.connect (config.client.c_str (), config.user.c_str (), config.pass.c_str ());
        DEBUG_PRINTF ("MQTTPublisher::connect: host=%s, port=%u, client=%s, user=%s, pass=%s, bufferSize=%u, result=%d\n", config.host.c_str (), config.port, config.client.c_str (), config.user.c_str (), config.pass.c_str (), config.bufferSize, result);
        return result;
    }
public:
    MQTTPublisher (const Config& cfg) : config (cfg), _mqttClient (_wifiClient) {}
    void setup () {
        _mqttClient.setServer (config.host.c_str (), config.port)
            .setBufferSize (config.bufferSize);
    }
    bool publish (const String& topic, const String& data) {
        const bool result = _mqttClient.publish (topic.c_str (), data.c_str ());
        DEBUG_PRINTF ("MQTTPublisher::publish: length=%u, result=%d\n", data.length (), result);
        return result;
    }
    void process () {
        _mqttClient.loop ();
        if (!_mqttClient.connected ())
            connect ();
    }
    bool connected () {
        return _mqttClient.connected ();
    }
    //
    void serialize (JsonObject &obj) const {
        PubSubClient& mqttClient = const_cast <MQTTPublisher *> (this)->_mqttClient;
        JsonObject mqtt = obj ["mqtt"].to <JsonObject> ();
        if ((mqtt ["connected"] = mqttClient.connected ())) {
            //
        }
        mqtt ["state"] = __mqtt_state_to_string (mqttClient.state ()).c_str ();    
    }
};

// -----------------------------------------------------------------------------------------------

#include <SPIFFS.h>

static String __file_state_to_string (int mode) {
    switch (mode) {
      case 0: return "ERROR";
      case 1: return "CLOSED";
      case 2: return "WRITING";
      case 3: return "READING";
      default: return "UNDEFINED";
    }
}

class SPIFFSFile {

public:
    class LineCallback {
    public:
        virtual bool process (const String& line) = 0;
    };

private:
    const String _filename;
    const size_t _maximum;
    size_t _size = -1;
    typedef enum {
        MODE_ERROR = 0,
        MODE_CLOSED = 1,
        MODE_WRITING = 2,
        MODE_READING = 3,
    } mode_t;
    mode_t _mode;
    File _file;

private:
    inline void _close () {
        _file.close ();
        _mode = MODE_CLOSED;
    }
    inline void _open (const mode_t mode) {
        // MODE_WRITING and MODE_READING only
        _file = SPIFFS.open (_filename, mode == MODE_WRITING ? FILE_APPEND : FILE_READ);
        if (_file) {
            _size = _file.size ();
            _mode = mode;
            DEBUG_PRINTF ("SPIFFSFile::_open: %s, size=%d\n", mode == MODE_WRITING ? "WRITING" : "READING", _size);
        } else {
            _size = -1;
            _mode = MODE_ERROR;
            DEBUG_PRINTF ("SPIFFSFile::_open: %s, failed on SPIFFS.open (), file activity not available\n", mode == MODE_WRITING ? "WRITING" : "READING");
        }
    }
    inline bool _append (const String& data) {
        if (_size + (data.length () + 2) > _maximum) {
            DEBUG_PRINTF ("SPIFFSFile::append: erasing, as size %d would exceed %d\n", _size + (data.length () + 1), _maximum);
            _file.close ();
            SPIFFS.remove (_filename);
            _open (MODE_WRITING);
            if (_mode == MODE_ERROR)
                return false;
        }
        _file.println (data);
        _size += data.length () + 2; // \r\n
        return true;
    }
    inline bool _read (LineCallback& callback) {
        while (_file.available ())
            if (!callback.process (_file.readStringUntil ('\n')))
                return false;
        return true;
    }
    inline bool _erase () {
        _size = -1;
        if (!SPIFFS.remove (_filename)) {
            _mode = MODE_ERROR;
            return false;
        }
        return true;
    }

public:
    SPIFFSFile (const String& filename, const size_t maximum): _filename (filename), _maximum (maximum) {}
    bool begin () {
        if (!SPIFFS.begin (true)) {
            DEBUG_PRINTF ("SPIFFSFile::begin: failed on SPIFFS.begin (), file activity not available\n");
            _mode = MODE_ERROR;
            return false;
        }
        const size_t totalBytes = SPIFFS.totalBytes (), usedBytes = SPIFFS.usedBytes ();
        DEBUG_PRINTF ("SPIFFSFile::begin: FS totalBytes=%d, usedBytes=%d, available=%.2f%%\n", totalBytes, usedBytes, (float) ((totalBytes - usedBytes) * 100.0) / (float) totalBytes);
        _mode = MODE_CLOSED;
        return true;
    }
    //
    size_t size () const { 
        return _size;
    }
    bool append (const String& data) {
        DEBUG_PRINTF ("SPIFFSFile::append: size=%d, length=%u\n", _size, data.length ());
        if (_mode == MODE_ERROR)    return false;
        if (_mode == MODE_READING)  _close ();
        if (_mode == MODE_CLOSED)   _open (MODE_WRITING);
        if (_mode == MODE_ERROR)    return false;
        return _append (data);
    }
    bool read (LineCallback& callback) { // XXX const
        DEBUG_PRINTF ("SPIFFSFile::read: size=%d\n", _size);
        if (_mode == MODE_ERROR)    return false;
        if (_mode == MODE_WRITING)  _close ();
        if (_mode == MODE_CLOSED)   _open (MODE_READING);
        if (_mode == MODE_ERROR)    return false;
        return _read (callback);
    }
    bool erase () {
        DEBUG_PRINTF ("SPIFFSFile::erase\n");
        if (_mode == MODE_ERROR)    return false;
        if (_mode == MODE_WRITING)  _close ();
        if (_mode == MODE_READING)  _close ();
        if (_mode == MODE_ERROR)    return false;
        return _erase ();
    }
    //
    void serialize (JsonObject &obj) const {
        JsonObject file = obj ["file"].to <JsonObject> ();
        file ["state"] = __file_state_to_string ((int) _mode).c_str ();    
    }
};

// -----------------------------------------------------------------------------------------------
