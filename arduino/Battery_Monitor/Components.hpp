
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

class MQTTPublisher {

public:
    typedef struct {
        const String client, host, user, pass, topic;
        const uint16_t port;
    } Config;
private:
    const Config& config;

    WiFiClient _wifiClient;
    PubSubClient _mqttClient;

    bool connect () {
        const bool result = _mqttClient.connect (config.client.c_str (), config.user.c_str (), config.pass.c_str ());
        DEBUG_PRINTF ("MQTTPublisher::connect: host=%s, port=%u, client=%s, user=%s, pass=%s, result=%d\n", config.host.c_str (), config.port, config.client.c_str (), config.user.c_str (), config.pass.c_str (), result);
        return result;
    }
public:
    MQTTPublisher (const Config& cfg) : config (cfg), _mqttClient (_wifiClient) {}
    void setup () {
        _mqttClient.setServer (config.host.c_str (), config.port);
    }
    bool publish (const String& topic, const String& data) {
        if (!_mqttClient.connected ())
            if (!connect ())
                return false;
        const bool result = _mqttClient.publish (topic.c_str (), data.c_str ());
        DEBUG_PRINTF ("MQTTPublisher::publish: length=%u, result=%d\n", data.length (), result);
        return result;
    }
    void process () {
        _mqttClient.loop ();
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
        mqtt ["state"] = mqttClient.state ();    
    }
};

// -----------------------------------------------------------------------------------------------

#include <SPIFFS.h>

class SPIFFSFile {
    const String _filename;
    const size_t _maximum;
    size_t _size = 0;

public:
    class LineCallback {
    public:
        virtual bool process (const String& line) = 0;
    };
    SPIFFSFile (const String& filename, const size_t maximum): _filename (filename), _maximum (maximum) {}
    bool begin () {
        if (!SPIFFS.begin (true)) {
            DEBUG_PRINTF ("SPIFFSFile::begin: failed on SPIFFS.begin ()\n");
            return false;
        }
        File file = SPIFFS.open (_filename, FILE_READ);
        if (file) {
            _size = file.size ();
            file.close ();
        }
        DEBUG_PRINTF ("SPIFFSFile::begin: size=%d\n", _size);
        return true;
    }
    //
    size_t size () const { return _size; }
    bool append (const String& data) {
        DEBUG_PRINTF ("SPIFFSFile::append: size=%d, length=%u\n", _size, data.length ());
        if (_size + data.length () > _maximum)
            erase ();
        File file = SPIFFS.open (_filename, FILE_APPEND);
        if (file) {
            file.println (data);
            _size = file.size ();
            file.close ();
            return true;
        }
        return false;
    }
    bool read (LineCallback& callback) const {
        DEBUG_PRINTF ("SPIFFSFile::read: size=%d\n", _size);
        File file = SPIFFS.open (_filename, FILE_READ);
        if (file) {
            while (file.available ()) {
                if (!callback.process (file.readStringUntil ('\n'))) {
                    file.close ();
                    return false;
                }
            }
            file.close ();
        }
        return true;
    }
    void erase () {
        DEBUG_PRINTF ("SPIFFSFile::erase: size=%d\n", _size);
        SPIFFS.remove (_filename);
        _size = 0;
    }
};

// -----------------------------------------------------------------------------------------------
