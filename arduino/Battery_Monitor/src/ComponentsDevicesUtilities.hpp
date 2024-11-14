
// -----------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------

#include <map>
#include <memory>

template <typename C>
class ConnectionReceiver {
public:
    class Handler {
    public:
        virtual bool process (C &, JsonDocument &) = 0;
        virtual ~Handler () {};
    };
    using Handlers = std::map<String, std::shared_ptr<Handler>>;
    class Insertable {
        ConnectionReceiver<C> *_manager;

    public:
        explicit Insertable (ConnectionReceiver<C> *manager) :
            _manager (manager) { }
        void insertReceivers (const ConnectionReceiver<C>::Handlers &handlers) {
            (*_manager) += handlers;
        }
    };

private:
    using Queue = QueueSimpleConcurrentSafe<String>;
    C *const _c;
    Queue _queue;
    Handlers _handlers;

    void processJson (const String &str) {
        JsonDocument doc;
        DeserializationError error;
        if ((error = deserializeJson (doc, str)) != DeserializationError::Ok) {
            DEBUG_PRINTF ("Receiver::process: deserializeJson fault: %s\n", error.c_str ());
            _failures += String ("write failed to deserialize Json: ") + String (error.c_str ());
            return;
        }
        const char *type = doc ["type"];
        if (type != nullptr) {
            auto handler = _handlers.find (String (type));
            if (handler != _handlers.end ()) {
                if (! handler->second->process (*_c, doc))
                    _failures += String ("write failed in handler for 'type': ") + String (type);
            } else
                _failures += String ("write failed to find handler for 'type': ") + String (type);
        } else
            _failures += String ("write failed to contain 'type'");
    }

public:
    ActivationTrackerWithDetail _failures;
    explicit ConnectionReceiver (C *c) :
        _c (c) { }
    ConnectionReceiver &operator+= (const Handlers &handlers) {
        _handlers.insert (handlers.begin (), handlers.end ());
        return *this;
    }
    void insert (const String &str) {
        _queue.push (str);
    }
    void process () {
        String str;
        while (_queue.pull (str)) {
            DEBUG_PRINTF ("Receiver::process: content=<<<%s>>>\n", str.c_str ());
            if (str.startsWith ("{\"type\""))
                processJson (str);
            else
                _failures += String ("write failed to identify as Json or have leading 'type'");
        }
    }
    void drain () {
        _queue.drain ();
    }
};

// -----------------------------------------------------------------------------------------------

template <typename C>
class ConnectionSender {
    using Function = std::function<void (const String &)>;
    const Function _func;

public:
    ActivationTrackerWithDetail _payloadExceeded;
    explicit ConnectionSender (Function func) :
        _func (func) { }
    bool send (const String &data, const int maxPayloadSize = -1) {
        bool result = true;
        if (maxPayloadSize == -1 || data.length () <= maxPayloadSize) {
            _func (data);
            DEBUG_PRINTF ("Sender::send: length=%u\n", data.length ());
        } else {
            JsonSplitter splitter (maxPayloadSize, { "type", "time", "addr" });
            splitter.splitJson (data, [&] (const String &part, const int elements) {
                const size_t length = part.length ();
                _func (part);
                DEBUG_PRINTF ("Sender::send: data=%u, part=%u, elements=%d\n", data.length (), length, elements);
                if (length > maxPayloadSize)
                    _payloadExceeded += ArithmeticToString (length), result = false;
                delay (20);
            });
        }
        return result;
    }
};

// -----------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------

template <typename Peer>
class ConnectionPeers {
public:
    using Peers = std::vector<String>;
    using Parser = std::function<Peer (const String &)>;

    struct Config {
        Peers order;
        int retries;
    };

private:
    const Config &_config;
    const Parser _parser;
    int _current = 0, _attempts = 0;

public:
    explicit ConnectionPeers (const Config &config, const Parser& parser) :
        _config (config),
        _parser (std::move (parser)) { }

    Peer select () {
        if (_config.order.empty ())
            return Peer {};
        return _parser (_config.order [_current]);
    }
    void update (bool connected) {
        if (connected)
            _attempts = 0;
        else if (++_attempts > _config.retries) {
            _attempts = 0;
            _current = (_current + 1) % _config.order.size ();
        }
    }
    bool available () const {
        return _config.order.size () > 0;
    }
};

// -----------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------

class ConnectionSignal : public JsonSerializable {
public:
    using RssiType = int8_t;
    enum class QualityType {
        UNKNOWN,
        EXCELLENT,
        GOOD,
        FAIR,
        POOR
    };
    using Callback = std::function<void (const RssiType, const QualityType)>;
    static constexpr RssiType RSSI_THRESHOLD_EXCELLENT = RssiType (-50);
    static constexpr RssiType RSSI_THRESHOLD_GOOD = RssiType (-60);
    static constexpr RssiType RSSI_THRESHOLD_FAIR = RssiType (-70);

    static QualityType signalQuality (const RssiType rssi) {
        if (rssi > RSSI_THRESHOLD_EXCELLENT)
            return QualityType::EXCELLENT;
        else if (rssi > RSSI_THRESHOLD_GOOD)
            return QualityType::GOOD;
        else if (rssi > RSSI_THRESHOLD_FAIR)
            return QualityType::FAIR;
        else
            return QualityType::POOR;
    }
    static String toString (const QualityType quality) {
        switch (quality) {
        case QualityType::UNKNOWN :
            return "UNKNOWN";
        case QualityType::POOR :
            return "POOR";
        case QualityType::FAIR :
            return "FAIR";
        case QualityType::GOOD :
            return "GOOD";
        case QualityType::EXCELLENT :
            return "EXCELLENT";
        default :
            return "UNDEFINED";
        }
    }

private:
    const Callback _callback;
    RssiType _rssiLast = std::numeric_limits<RssiType>::min ();
    QualityType _qualityLast = QualityType::UNKNOWN;

public:
    explicit ConnectionSignal (const Callback callback = nullptr) :
        _callback (callback) { }
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
        _rssiLast = std::numeric_limits<RssiType>::min ();
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
// -----------------------------------------------------------------------------------------------
