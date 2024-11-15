
// -----------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------

#include <WiFiUdp.h>
#include <LightMDNS.hpp>

class MulticastDNSPublisher : public JsonSerializable {
public:
    typedef struct {
    } Config;
    using __implementation_t = MDNS;

private:
    const Config &config;

    WiFiUDP _udp;
    MDNS _mdns;

public:
    explicit MulticastDNSPublisher (const Config &cfg) :
        config (cfg),
        _mdns (_udp) { }

    void begin () {
        auto status = _mdns.begin ();
        if (status != MDNS::Status::Success)
            DEBUG_PRINTF ("NetworkManager::begin: mdns begin error=%d\n", status);
    }
    void process () {
        auto status = _mdns.process ();
        if (status != MDNS::Status::Success)
            DEBUG_PRINTF ("NetworkManager::process: mdns process error=%d\n", status);
    }
    __implementation_t &__implementation () {
        return _mdns;
    }
    //
    void start (const IPAddress &address, const String &host) {
        _mdns.start (address, host.c_str ());
    }
    void stop () {
        _mdns.stop ();
    }
    //
    void serialize (JsonVariant &obj) const override {
    }
};

// -----------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------
