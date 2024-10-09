
// -----------------------------------------------------------------------------------------------
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

    static inline constexpr long MAX_DRIFT_MS = 60 * 1000;

    long _driftMs;
    bool _isHighDrift = false;

public:
    TimeDriftCalculator (const long driftMs) : _driftMs (driftMs) {}
    long updateDrift (time_t periodSecs, const interval_t periodMs) {
        long driftMs = (((periodSecs * 1000) - periodMs) * (60 * 60 * 1000)) / periodMs; // ms per hour
        driftMs = (_driftMs * 3 + driftMs) / 4; // 75% old value, 25% new value
        if (driftMs > MAX_DRIFT_MS || driftMs < -MAX_DRIFT_MS) _isHighDrift = true;
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
    inline bool isHighDrift () const {
      return _isHighDrift;
    }
    inline long drift () const {
      return _driftMs;
    }
};

// -----------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------

#include <ArduinoJson.h>
#include <WiFi.h>
#include <PubSubClient.h>

class MQTTPublisher: public JsonSerializable {

public:
    typedef struct {
        String client, host, user, pass, topic;
        uint16_t port;
        uint16_t bufferSize;
    } Config;

private:
    const Config &config;

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
    inline bool connected () {
        return _mqttClient.connected ();
    }
    //
    void serialize (JsonObject &obj) const override {
        PubSubClient& mqttClient = const_cast <MQTTPublisher *> (this)->_mqttClient;
        JsonObject mqtt = obj ["mqtt"].to <JsonObject> ();
        if ((mqtt ["connected"] = mqttClient.connected ())) {
            //
        }
        mqtt ["state"] = __mqtt_state_to_string (mqttClient.state ());
    }

private:
    static String __mqtt_state_to_string (const int state) {
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
};

// -----------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------

#include <SPIFFS.h>

// should be two classes
class SPIFFSFile: public JsonSerializable  {

public:
    class LineCallback {
    public:
        virtual bool process (const String& line) = 0;
    };

private:
    const String _filename;
    long _size = -1;
    long _totalBytes, _usedBytes;
    typedef enum {
        MODE_ERROR = 0,
        MODE_CLOSED = 1,
        MODE_WRITING = 2,
        MODE_READING = 3,
    } mode_t;
    mode_t _mode;
    File _file;

private:
    bool _init () {
        if (!SPIFFS.begin (true)) {
            DEBUG_PRINTF ("SPIFFSFile[%s]::begin: failed on SPIFFS.begin (), file activity not available\n", _filename.c_str ());
            _mode = MODE_ERROR;
            return false;
        }        
        _totalBytes = SPIFFS.totalBytes ();
        _usedBytes = SPIFFS.usedBytes ();
        DEBUG_PRINTF ("SPIFFSFile[%s]::begin: available=%.2f%% (totalBytes=%lu, usedBytes=%lu)\n", _filename.c_str (), (float) ((_totalBytes - _usedBytes) * 100.0) / (float) _totalBytes, _totalBytes, _usedBytes);
        _mode = MODE_CLOSED;
        return true;
    }
    void _close () {
        _file.close ();
        _mode = MODE_CLOSED;
    }
    void _open (const mode_t mode) {
        // MODE_WRITING and MODE_READING only
        _file = SPIFFS.open (_filename, mode == MODE_WRITING ? FILE_APPEND : FILE_READ);
        if (_file) {
            _size = _file.size ();
            _mode = mode;
            DEBUG_PRINTF ("SPIFFSFile[%s]::_open: %s, size=%ld\n", _filename.c_str (), mode == MODE_WRITING ? "WRITING" : "READING", _size);
        } else {
            _size = -1;
            _mode = MODE_ERROR;
            DEBUG_PRINTF ("SPIFFSFile[%s]::_open: %s, failed on SPIFFS.open (), file activity not available\n", _filename.c_str (), mode == MODE_WRITING ? "WRITING" : "READING");
        }
    }
    size_t _append (const char * type, std::function <size_t ()> impl, size_t length) {
        DEBUG_PRINTF ("SPIFFSFile[%s]::append[%s]: size=%ld, length=%u\n", _filename.c_str (), type, _size, length);
        if (length > (_totalBytes - _usedBytes)) {
            DEBUG_PRINTF ("SPIFFSFile[%s]::_append: erasing, as size %ld would exceed %ld\n", _filename.c_str (), _size + length, _totalBytes);
            _file.close ();
            SPIFFS.remove (_filename);
            _usedBytes = 0;
            _open (MODE_WRITING);
            if (_mode == MODE_ERROR)
                return 0;
        }
        length = impl ();
        _size += length;
        _usedBytes += length; // probably is not 100% correct due to metadata (e.g. FAT)
        return length;
    }
    size_t _append (const String& data) { return _append ("String", [&] () { return _file.println (data); }, data.length () + 2); }
    size_t _append (const JsonDocument& doc) { return _append ("JsonDocument", [&] () { return serializeJson (doc, _file); }, measureJson (doc)); }

    size_t _read (LineCallback& callback) {
        size_t size = 0;
        while (_file.available ()) {
            const String input = _file.readStringUntil ('\n');
            if (!callback.process (input))
                break;
            size += input.length ();
        }
        return size;
    }
    size_t _read (JsonDocument& doc) {
        DeserializationError error;
        if ((error = deserializeJson (doc, _file)) != DeserializationError::Ok) {
            DEBUG_PRINTF ("SPIFFSFile[%s]::_read: deserializeJson fault: %s\n", _filename.c_str (), error.c_str ());
            return 0;
        }
        return measureJson (doc);
    }

    bool _erase () {
        _size = -1;
        _usedBytes = 0;
        if (SPIFFS.exists (_filename) && !SPIFFS.remove (_filename)) {
            _mode = MODE_ERROR;
            return false;
        }
        return true;
    }

public:
    SPIFFSFile (const String& filename): _filename (filename) {}
    ~SPIFFSFile () { close (); }
    bool begin () {
        return _init ();
    }
    //
    inline long size () const {
        return _size;
    }
    inline float remains () const {
        return (float) ((_totalBytes - _usedBytes) * 100.0) / (float) _totalBytes;
    }
    template <typename T>
    size_t append (const T& data) {
        if (_mode == MODE_ERROR)    return 0;
        if (_mode == MODE_READING)  _close ();
        if (_mode == MODE_CLOSED)   _open (MODE_WRITING);
        if (_mode == MODE_ERROR)    return 0;
        return _append (data);
    }
    template <typename T>
    size_t write (const T& data) {
        if (_mode == MODE_ERROR)    return 0;
        if (_mode == MODE_READING)  _close ();
        if (_mode == MODE_CLOSED)   if (_erase ()) _open (MODE_WRITING);
        if (_mode == MODE_ERROR)    return 0;
        return _append (data);
    }
    template <typename T>
    size_t read (T& vessel) { // XXX const
        DEBUG_PRINTF ("SPIFFSFile[%s]::read: size=%ld\n", _filename.c_str (), _size);
        if (_mode == MODE_ERROR)    return 0;
        if (_mode == MODE_WRITING)  _close ();
        if (_mode == MODE_CLOSED)   _open (MODE_READING);
        if (_mode == MODE_ERROR)    return 0;
        return _read (vessel);
    }
    bool close () {
        DEBUG_PRINTF ("SPIFFSFile[%s]::close\n", _filename.c_str ());
        if (_mode == MODE_ERROR)    return false;
        if (_mode == MODE_WRITING)  _close ();
        if (_mode == MODE_READING)  _close ();
        if (_mode == MODE_ERROR)    return false;
        return true;
    }
    bool erase () {
        DEBUG_PRINTF ("SPIFFSFile[%s]::erase\n", _filename.c_str ());
        if (_mode == MODE_ERROR)    return false;
        if (_mode == MODE_WRITING)  _close ();
        if (_mode == MODE_READING)  _close ();
        if (_mode == MODE_ERROR)    return false;
        return _erase ();
    }
    //
    void serialize (JsonObject &obj) const override {
        JsonObject file = obj ["file"].to <JsonObject> ();
        file ["mode"] = __file_mode_to_string ((int) _mode);
        file ["size"] = size ();
        file ["left"] = _totalBytes - _usedBytes;
        file ["remains"] = remains ();
    }

private:
    static String __file_mode_to_string (const int mode) {
        switch (mode) {
          case 0: return "ERROR";
          case 1: return "CLOSED";
          case 2: return "WRITING";
          case 3: return "READING";
          default: return "UNDEFINED";
        }
    }
};

// -----------------------------------------------------------------------------------------------
