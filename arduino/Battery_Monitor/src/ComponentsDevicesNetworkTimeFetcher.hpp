
// -----------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------

#include <HTTPClient.h>
#include <ctime>

class NetworkTimeFetcher {
    const String _useragent, _server;

public:
    explicit NetworkTimeFetcher (const String &useragent, const String &server) :
        _useragent (useragent),
        _server (server) { }
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
            if (! header.isEmpty ()) {
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
    explicit TimeDriftCalculator (const long driftMs) :
        _driftMs (driftMs) { }
    long updateDrift (const time_t periodSecs, const interval_t periodMs) {
        long driftMs = (((periodSecs * 1000) - periodMs) * (60 * 60 * 1000)) / periodMs;    // ms per hour
        driftMs = (_driftMs * 3 + driftMs) / 4;                                             // 75% old value, 25% new value
        if (driftMs > MAX_DRIFT_MS || driftMs < -MAX_DRIFT_MS)
            _highDrift = driftMs;
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
