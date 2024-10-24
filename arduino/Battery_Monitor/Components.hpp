
// -----------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------

class ConnectionSignalTracker: public JsonSerializable {
public:
    using RssiType = int8_t;
    enum class QualityType {
        UNKNOWN,
        EXCELLENT,
        GOOD,
        FAIR,
        POOR
    };
    using Callback = std::function <void (const RssiType, const QualityType)>;
    static constexpr RssiType RSSI_THRESHOLD_EXCELLENT = RssiType (-50);
    static constexpr RssiType RSSI_THRESHOLD_GOOD      = RssiType (-60);
    static constexpr RssiType RSSI_THRESHOLD_FAIR      = RssiType (-70);

    static QualityType signalQuality (const RssiType rssi) {
        if (rssi > RSSI_THRESHOLD_EXCELLENT) return QualityType::EXCELLENT;
        else if (rssi > RSSI_THRESHOLD_GOOD) return QualityType::GOOD;
        else if (rssi > RSSI_THRESHOLD_FAIR) return QualityType::FAIR;
        else                                 return QualityType::POOR;
    }
    static String toString (const QualityType quality) {
        switch (quality) {
          case QualityType::UNKNOWN: return "UNKNOWN";
          case QualityType::POOR: return "POOR";
          case QualityType::FAIR: return "FAIR";
          case QualityType::GOOD: return "GOOD";
          case QualityType::EXCELLENT: return "EXCELLENT";
          default: return "UNDEFINED";
        }
    }

private:
    const Callback _callback;
    RssiType _rssiLast = std::numeric_limits <RssiType>::min ();
    QualityType _qualityLast = QualityType::UNKNOWN;

public:
    explicit ConnectionSignalTracker (const Callback callback = nullptr): _callback (callback) {}
    RssiType rssi () const {
        return _rssiLast;
    }
    void update (const RssiType rssi) {
        QualityType quality = signalQuality (_rssiLast = rssi);
        if (quality != _qualityLast) {
            _qualityLast = quality;
            if (_callback)
                _callback (rssi, quality);
        }
    }
    void reset () {
        _rssiLast = std::numeric_limits <RssiType>::min ();
        _qualityLast = QualityType::UNKNOWN;
    }
    String toString () const {
        return toString (_qualityLast);
    }
    //
    void serialize (JsonVariant &obj) const override {
        obj ["rssi"] = _rssiLast;
        obj ["quality"] = toString (_qualityLast);
    }
};

// -----------------------------------------------------------------------------------------------

#include <HTTPClient.h>
#include <ctime>

class NetworkTimeFetcher {
    const String _useragent, _server;

public:
    explicit NetworkTimeFetcher (const String& useragent, const String& server) : _useragent (useragent), _server (server) {}
    time_t fetch () {
        HTTPClient client;
        String header = "";
        time_t time = 0;
        client.setUserAgent (_useragent);
        client.begin (_server);
        const char *headerList [] = { "Date" };
        client.collectHeaders (headerList, sizeof (headerList) / sizeof (headerList [0]));
        int code = client.sendRequest ("HEAD");
        if (code > 0) {
            header = client.header ("Date");
            if (!header.isEmpty ()) {
                struct tm timeinfo;
                if (strptime (header.c_str (), "%a, %d %b %Y %H:%M:%S GMT", &timeinfo) != nullptr)
                    time = mktime (&timeinfo);
            }
        }
        client.end ();
        DEBUG_PRINTF ("NetworkTimeFetcher::fetch: server=%s, code=%d, header=[%s], time=%lu\n", _server.c_str (), code, header.c_str (), (unsigned long) time);
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
    long _driftMs, _highDrift = 0;

public:
    explicit TimeDriftCalculator (const long driftMs) : _driftMs (driftMs) {}
    long updateDrift (const time_t periodSecs, const interval_t periodMs) {
        long driftMs = (((periodSecs * 1000) - periodMs) * (60 * 60 * 1000)) / periodMs; // ms per hour
        driftMs = (_driftMs * 3 + driftMs) / 4; // 75% old value, 25% new value
        if (driftMs > MAX_DRIFT_MS || driftMs < -MAX_DRIFT_MS) _highDrift = driftMs;
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
    inline long highDrift () const {
      return _highDrift;
    }
    inline long drift () const {
      return _driftMs;
    }
};

// -----------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------

#include <SPIFFS.h>

// should be two classes
class SPIFFSFile: public JsonSerializable {
public:
    class LineCallback {
    public:
        virtual bool process (const String& line) = 0;
    };

private:
    const String _filename;
    long _size = -1;
    long _totalBytes = 0, _usedBytes = 0;
    typedef enum {
        MODE_ERROR = 0,
        MODE_CLOSED = 1,
        MODE_WRITING = 2,
        MODE_READING = 3,
    } mode_t;
    mode_t _mode = MODE_ERROR;
    File _file;

    bool _init () {
        if (!SPIFFS.begin (true)) {
            DEBUG_PRINTF ("SPIFFSFile[%s]::begin: failed on SPIFFS.begin (), file activity not available\n", _filename.c_str ());
            _mode = MODE_ERROR;
            return false;
        }
        _totalBytes = SPIFFS.totalBytes ();
        _usedBytes = SPIFFS.usedBytes ();
        DEBUG_PRINTF ("SPIFFSFile[%s]::begin: available=%.2f%% (totalBytes=%lu, usedBytes=%lu)\n", _filename.c_str (), static_cast <float> ((_totalBytes - _usedBytes) * 100.0) / static_cast <float> (_totalBytes), _totalBytes, _usedBytes);
        _mode = MODE_CLOSED;
        return true;
    }
    void _close () {
        _file.close ();
        _mode = MODE_CLOSED;
    }
    bool _open (const mode_t mode) {
        // MODE_WRITING and MODE_READING only
        const bool exists = SPIFFS.exists (_filename);
        _file = SPIFFS.open (_filename, mode == MODE_WRITING ? FILE_APPEND : FILE_READ);
        if (_file) {
            _size = exists ? _file.size () : 0;
            _mode = mode;
            DEBUG_PRINTF ("SPIFFSFile[%s]::_open: %s, size=%ld\n", _filename.c_str (), mode == MODE_WRITING ? "WRITING" : "READING", _size);
            return true;
        } else {
            _size = -1;
            _mode = MODE_ERROR;
            DEBUG_PRINTF ("SPIFFSFile[%s]::_open: %s, failed on SPIFFS.open (), file activity not available\n", _filename.c_str (), mode == MODE_WRITING ? "WRITING" : "READING");
            return false;
        }
    }
    size_t _append (const char * type, const std::function <size_t ()> impl, const size_t length) {
        DEBUG_PRINTF ("SPIFFSFile[%s]::append[%s]: size=%ld, length=%u\n", _filename.c_str (), type, _size, length);
        if (length > (_totalBytes - _usedBytes)) {
            DEBUG_PRINTF ("SPIFFSFile[%s]::_append: erasing, as size %ld would exceed %ld\n", _filename.c_str (), _size + length, _totalBytes);
            _file.close ();
            SPIFFS.remove (_filename);
            _usedBytes = 0;
            if (!_open (MODE_WRITING))
                return 0;
        }
        size_t length_actual = impl ();
        _size += length_actual;
        _usedBytes += length_actual; // probably is not 100% correct due to metadata (e.g. FAT)
        return length_actual;
    }
    size_t _append (const String& data) { return _append ("String", [&] () { return _file.println (data); }, data.length () + 2); }
    size_t _append (const JsonDocument& doc) { return _append ("JsonDocument", [&] () { return serializeJson (doc, _file); }, measureJson (doc)); }

    size_t _read (LineCallback& callback) {
        size_t count = 0;
        while (_file.available ()) {
            const String input = _file.readStringUntil ('\n');
            if (!callback.process (input))
                break;
            count += input.length ();
        }
        return count;
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
    void _ssize () {
        _file = SPIFFS.open (_filename, FILE_READ);
        if (_file) {
            _size = _file.size ();
            _file.close ();
        } else
            _size = 0;
    }

public:
    explicit SPIFFSFile (const String& filename): _filename (filename) {}
    ~SPIFFSFile () { close (); }

    bool begin () {
        return _init ();
    }
    bool available () const {
        return _mode != MODE_ERROR;
    }
    //
    long size () const {
        if (_mode == MODE_CLOSED)   if (_size < 0) const_cast <SPIFFSFile *> (this)->_ssize (); // zero if not exists
        if (_mode == MODE_ERROR)    return -1;
        return _size;
    }
    float remains () const {
        return round2places (static_cast <float> ((_totalBytes - _usedBytes) * 100.0f) / static_cast <float> (_totalBytes));
    }
    template <typename T>
    size_t append (const T& data) {
        if (_mode == MODE_READING)  _close ();
        if (_mode == MODE_CLOSED)   _open (MODE_WRITING);
        if (_mode == MODE_ERROR)    return 0;
        return _append (data);
    }
    template <typename T>
    size_t write (const T& data) {
        if (_mode == MODE_READING)  _close ();
        if (_mode == MODE_CLOSED)   if (_erase ()) _open (MODE_WRITING); // will set ERROR
        if (_mode == MODE_ERROR)    return 0;
        return _append (data);
    }
    template <typename T>
    size_t read (T& vessel) { // XXX const
        DEBUG_PRINTF ("SPIFFSFile[%s]::read: size=%ld\n", _filename.c_str (), _size);
        if (_mode == MODE_WRITING)  _close ();
        if (_mode == MODE_CLOSED)   _open (MODE_READING);
        if (_mode == MODE_ERROR)    return 0;
        return _read (vessel);
    }
    bool close () {
        DEBUG_PRINTF ("SPIFFSFile[%s]::close\n", _filename.c_str ());
        if (_mode == MODE_WRITING)  _close ();
        if (_mode == MODE_READING)  _close ();
        if (_mode == MODE_ERROR)    return false;
        return true;
    }
    bool erase () {
        DEBUG_PRINTF ("SPIFFSFile[%s]::erase\n", _filename.c_str ());
        if (_mode == MODE_WRITING)  _close ();
        if (_mode == MODE_READING)  _close ();
        if (_mode == MODE_ERROR)    return false;
        return _erase ();
    }
    //
    void serialize (JsonVariant &obj) const override {
        obj ["mode"] = _mode_to_string (static_cast <int> (_mode));
        obj ["size"] = size ();
        obj ["left"] = _totalBytes - _usedBytes;
        obj ["remains"] = remains ();
    }

private:
    static String _mode_to_string (const int mode) {
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
