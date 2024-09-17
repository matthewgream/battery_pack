
#ifndef __HELPERS_HPP__
#define __HELPERS_HPP__

// -----------------------------------------------------------------------------------------------

#include <HTTPClient.h>
#include <ctime>

class NetworkTimeFetcher {
    const String _server;

public:
    NetworkTimeFetcher (const String& server) : _server (server) {}
    time_t fetch () {
        HTTPClient client;
        client.begin (_server);
        if (client.GET () > 0) {
            String header = client.header ("Date");
            if (!header.isEmpty ()) {
                struct tm timeinfo;
                if (strptime (header.c_str (), "%a, %d %b %Y %H:%M:%S GMT", &timeinfo) != NULL) {
                    client.end ();
                    return mktime (&timeinfo);
                }
            }
        }
        client.end ();
        return static_cast <time_t> (0);
    }
};

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
        return _driftMs = std::clamp (driftMs, -MAX_DRIFT_MS, MAX_DRIFT_MS);
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

#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

class BluetoothNotifier : protected BLEServerCallbacks {

public:
    typedef struct {
        const String name, serviceUUID, characteristicUUID;
    } Config;
private:
    const Config& config;

    BLEServer *_server = nullptr;
    BLECharacteristic *_characteristic = nullptr;
    bool _connected = false;

protected:
    void onConnect (BLEServer *) override { _connected = true; }
    void onDisconnect (BLEServer *) override { _connected = false; }

public:
    BluetoothNotifier (const Config& cfg) : config (cfg) {}
    void advertise ()  {
        BLEDevice::init (config.name);
        _server = BLEDevice::createServer ();
        _server->setCallbacks (this);
        BLEService *service = _server->createService (config.serviceUUID);
        _characteristic = service->createCharacteristic (config.characteristicUUID, BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY);
        _characteristic->addDescriptor (new BLE2902 ());
        service->start ();
        BLEAdvertising *advertising = BLEDevice::getAdvertising ();
        advertising->addServiceUUID (config.serviceUUID);
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
    //
    void serialize (JsonObject &obj) const {
        JsonObject bluetooth = obj ["bluetooth"].to <JsonObject> ();
        if ((bluetooth ["connected"] = _connected)) {
            bluetooth ["address"] = BLEDevice::getAddress ().toString ();
            bluetooth ["devices"] = _server->getPeerDevices (true).size ();
        }
    }    
};

// -----------------------------------------------------------------------------------------------

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

public:
    MQTTPublisher (const Config& cfg) : config (cfg), _mqttClient (_wifiClient) {}
    void setup () {
        _mqttClient.setServer (config.host.c_str (), config.port);
    }
    bool publish (const String& topic, const String& data) {
        if (!WiFi.isConnected ())
            return false;
        if (!_mqttClient.connected () && !_mqttClient.connect (config.client.c_str (), config.user.c_str (), config.pass.c_str ()))
            return false;
        return _mqttClient.publish (topic.c_str (), data.c_str ());
    }
    void process () {
        if (WiFi.isConnected ())
            _mqttClient.loop ();
    }
    bool connected () {
        return WiFi.isConnected () && _mqttClient.connected ();
    }
    //
    void serialize (JsonObject &obj) const {
        PubSubClient& mqttClient = const_cast <MQTTPublisher *> (this)->_mqttClient;
        JsonObject mqtt = obj ["mqtt"].to <JsonObject> ();
        if ((mqtt ["connected"] = (WiFi.isConnected () && mqttClient.connected ()))) {
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

#include <Arduino.h>

class MuxInterface_CD74HC4067 { // technically this is ADC as well due to PIN_SIG

public:
    typedef struct {
        const int PIN_S0, PIN_S1, PIN_S2, PIN_S3, PIN_SIG;
    } Config;
    static constexpr int CHANNELS = 16;
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
