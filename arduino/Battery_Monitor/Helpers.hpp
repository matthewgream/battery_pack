
#ifndef __HELPERS_HPP__
#define __HELPERS_HPP__

// -----------------------------------------------------------------------------------------------

#include <HTTPClient.h>

class NetworkTimeFetcher {
    const String _server;
    int _failures = 0;
public:
    // XXX last time fetch
    NetworkTimeFetcher (const String& server) : _server (server) {}
    time_t fetch () {
        HTTPClient client;
        client.begin (_server);
        if (client.GET () > 0) {
            String header = client.header ("Date");
            client.end ();
            if (!header.isEmpty ()) {
                struct tm timeinfo;
                if (strptime (header.c_str (), "%a, %d %b %Y %H:%M:%S GMT", &timeinfo) != NULL) {
                    _failures = 0;
                    return mktime (&timeinfo);
                }
            }
        }
        client.end ();
        _failures ++;
        return (time_t) 0;
    }
    int failures () const { 
        return _failures;
    }
};

// -----------------------------------------------------------------------------------------------

class TimeDriftCalculator {
    long _driftMs;
    bool _highDrift = false;
    static constexpr long MAX_DRIFT_MS = 60 * 1000;
public:
    // XXX average drift
    TimeDriftCalculator (const long driftMs) : _driftMs (driftMs) {}
    long updateDrift (time_t periodSecs, const unsigned long periodMs) {
        long driftMs = (((periodSecs * 1000) - periodMs) * (60 * 60 * 1000)) / periodMs; // ms per hour
        driftMs = (_driftMs * 3 + driftMs) / 4; // 75% old value, 25% new value
        if (driftMs > MAX_DRIFT_MS || driftMs < -MAX_DRIFT_MS) _highDrift = true;
        return _driftMs = std::clamp (driftMs, -MAX_DRIFT_MS, MAX_DRIFT_MS);
    }    
    long applyDrift (struct timeval &currentTime, const unsigned long periodMs) {
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
        return adjustMs;
    }
    bool highDrift () const { 
      return _highDrift;
    }
};

// -----------------------------------------------------------------------------------------------

#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

class BluetoothNotifier : protected BLEServerCallbacks {
    BLEServer *_server = nullptr;
    BLECharacteristic *_characteristic = nullptr;
    bool _connected = false;
protected:
    void onConnect (BLEServer *) override { _connected = true; }
    void onDisconnect (BLEServer *) override { _connected = false; }
public:
    // XXX last notify
    void advertise (const String& name, const String& serviceUUID, const String& characteristicUUID)  {
        BLEDevice::init (name);
        _server = BLEDevice::createServer ();
        _server->setCallbacks (this);
        BLEService *service = _server->createService (serviceUUID);
        _characteristic = service->createCharacteristic (characteristicUUID, BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY);
        _characteristic->addDescriptor (new BLE2902 ());
        service->start ();
        BLEAdvertising *advertising = BLEDevice::getAdvertising ();
        advertising->addServiceUUID (serviceUUID);
        advertising->setScanResponse (true);
        advertising->setMinPreferred (0x06);
        advertising->setMinPreferred (0x12);
        BLEDevice::startAdvertising ();
    }
    void notify (const String& data) {
        _characteristic->setValue (data.c_str ());
        _characteristic->notify ();
    }
    bool connected (void) const {
        return _connected;
    }
};

// -----------------------------------------------------------------------------------------------

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
public:
    // XXX last publish
    MQTTPublisher (const Config& cfg) : config (cfg), _mqttClient (_wifiClient) {}
    void connect () {
        _mqttClient.setServer (config.host.c_str (), config.port);
    }
    bool publish (const String& topic, const String& data) {
        if (!connected ())
            return false;
        _mqttClient.loop ();
        return _mqttClient.publish (topic.c_str (), data.c_str ());
    }
    bool connected () {
        if (!WiFi.isConnected ())
            return false;
        if (!_mqttClient.connected ())
            _mqttClient.connect (config.client.c_str (), config.user.c_str (), config.pass.c_str ());
        return _mqttClient.connected ();
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
            DEBUG_PRINTLN ("Error mounting SPIFFS");
            return false;
        }
        File file = SPIFFS.open (_filename, FILE_READ);
        if (file) {
            _size = file.size ();
            file.close ();
        }
        return true;
    }
    //
    size_t size () const { return _size; }
    bool append (const String& data) {
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
        SPIFFS.remove (_filename);
        _size = 0;
    }
};

// -----------------------------------------------------------------------------------------------

class MuxInterface_CD74HC4067 {
public:
    typedef struct {
        const int PIN_S0, PIN_S1, PIN_S2, PIN_S3, PIN_SIG;
    } Config;
private:    
    const Config& config;
public:
    MuxInterface_CD74HC4067 (const Config& cfg) : config (cfg) {}
    void configure () {
        pinMode (config.PIN_S0, OUTPUT);
        pinMode (config.PIN_S1, OUTPUT);
        pinMode (config.PIN_S2, OUTPUT);
        pinMode (config.PIN_S3, OUTPUT);
        pinMode (config.PIN_SIG, INPUT);
    }
    //
    uint16_t get (const int channel) const {
        digitalWrite (config.PIN_S0, channel & 1);
        digitalWrite (config.PIN_S1, (channel >> 1) & 1);
        digitalWrite (config.PIN_S2, (channel >> 2) & 1);
        digitalWrite (config.PIN_S3, (channel >> 3) & 1);
        delay (10);
        return analogRead (config.PIN_SIG);
    }
};

// -----------------------------------------------------------------------------------------------

#endif
