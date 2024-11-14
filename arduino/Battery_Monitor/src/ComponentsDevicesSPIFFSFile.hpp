
// -----------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------

#include <SPIFFS.h>

// should be two classes
class SPIFFSFile : public JsonSerializable {
public:
    class LineCallback {
    public:
        virtual bool process (const String &line) = 0;
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
        if (! SPIFFS.begin (true)) {
            DEBUG_PRINTF ("SPIFFSFile[%s]::begin: failed on SPIFFS.begin (), file activity not available\n", _filename.c_str ());
            _mode = MODE_ERROR;
            return false;
        }
        _totalBytes = SPIFFS.totalBytes ();
        _usedBytes = SPIFFS.usedBytes ();
        DEBUG_PRINTF ("SPIFFSFile[%s]::begin: available=%.2f%% (totalBytes=%lu, usedBytes=%lu)\n", _filename.c_str (), static_cast<float> ((_totalBytes - _usedBytes) * 100.0) / static_cast<float> (_totalBytes), _totalBytes, _usedBytes);
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
    size_t _append (const char *type, const std::function<size_t ()> impl, const size_t length) {
        DEBUG_PRINTF ("SPIFFSFile[%s]::append[%s]: size=%ld, length=%u\n", _filename.c_str (), type, _size, length);
        if (length > (_totalBytes - _usedBytes)) {
            DEBUG_PRINTF ("SPIFFSFile[%s]::_append: erasing, as size %ld would exceed %ld\n", _filename.c_str (), _size + length, _totalBytes);
            _file.close ();
            SPIFFS.remove (_filename);
            _usedBytes = 0;
            if (! _open (MODE_WRITING))
                return 0;
        }
        size_t length_actual = impl ();
        _size += length_actual;
        _usedBytes += length_actual;    // probably is not 100% correct due to metadata (e.g. FAT)
        return length_actual;
    }
    size_t _append (const String &data) {
        return _append (
            "String", [&] () {
                return _file.println (data);
            },
            data.length () + 2);
    }
    size_t _append (const JsonDocument &doc) {
        return _append (
            "JsonDocument", [&] () {
                return serializeJson (doc, _file);
            },
            measureJson (doc));
    }

    size_t _read (LineCallback &callback) {
        size_t count = 0;
        while (_file.available ()) {
            const String input = _file.readStringUntil ('\n');
            if (! callback.process (input))
                break;
            count += input.length ();
        }
        return count;
    }
    size_t _read (JsonDocument &doc) {
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
        if (SPIFFS.exists (_filename) && ! SPIFFS.remove (_filename)) {
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
    explicit SPIFFSFile (const String &filename) :
        _filename (filename) { }
    ~SPIFFSFile () {
        close ();
    }

    bool begin () {
        return _init ();
    }
    bool available () const {
        return _mode != MODE_ERROR;
    }
    //
    long size () const {
        if (_mode == MODE_CLOSED)
            if (_size < 0)
                const_cast<SPIFFSFile *> (this)->_ssize ();    // zero if not exists
        if (_mode == MODE_ERROR)
            return -1;
        return _size;
    }
    float remains () const {
        return round2places (static_cast<float> ((_totalBytes - _usedBytes) * 100.0f) / static_cast<float> (_totalBytes));
    }
    template <typename T>
    size_t append (const T &data) {
        if (_mode == MODE_READING)
            _close ();
        if (_mode == MODE_CLOSED)
            _open (MODE_WRITING);
        if (_mode == MODE_ERROR)
            return 0;
        return _append (data);
    }
    template <typename T>
    size_t write (const T &data) {
        if (_mode == MODE_READING)
            _close ();
        if (_mode == MODE_CLOSED)
            if (_erase ())
                _open (MODE_WRITING);    // will set ERROR
        if (_mode == MODE_ERROR)
            return 0;
        return _append (data);
    }
    template <typename T>
    size_t read (T &vessel) {    // XXX const
        DEBUG_PRINTF ("SPIFFSFile[%s]::read: size=%ld\n", _filename.c_str (), _size);
        if (_mode == MODE_WRITING)
            _close ();
        if (_mode == MODE_CLOSED)
            _open (MODE_READING);
        if (_mode == MODE_ERROR)
            return 0;
        return _read (vessel);
    }
    bool close () {
        DEBUG_PRINTF ("SPIFFSFile[%s]::close\n", _filename.c_str ());
        if (_mode == MODE_WRITING)
            _close ();
        if (_mode == MODE_READING)
            _close ();
        if (_mode == MODE_ERROR)
            return false;
        return true;
    }
    bool erase () {
        DEBUG_PRINTF ("SPIFFSFile[%s]::erase\n", _filename.c_str ());
        if (_mode == MODE_WRITING)
            _close ();
        if (_mode == MODE_READING)
            _close ();
        if (_mode == MODE_ERROR)
            return false;
        return _erase ();
    }
    //
    void serialize (JsonVariant &obj) const override {
        obj ["mode"] = _mode_to_string (static_cast<int> (_mode));
        obj ["size"] = size ();
        obj ["left"] = _totalBytes - _usedBytes;
        obj ["remains"] = remains ();
    }

private:
    static String _mode_to_string (const int mode) {
        switch (mode) {
        case 0 :
            return "ERROR";
        case 1 :
            return "CLOSED";
        case 2 :
            return "WRITING";
        case 3 :
            return "READING";
        default :
            return "UNDEFINED";
        }
    }
};

// -----------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------
