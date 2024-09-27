
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

#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

static String __ble_disconnect_reason (const esp_gatt_conn_reason_t reason) {
    switch (reason) {
      case ESP_GATT_CONN_UNKNOWN: return "UNKNOWN";
      case ESP_GATT_CONN_L2C_FAILURE: return "L2C_FAILURE";
      case ESP_GATT_CONN_TIMEOUT: return "TIMEOUT";
      case ESP_GATT_CONN_TERMINATE_PEER_USER: return "PEER_USER";
      case ESP_GATT_CONN_TERMINATE_LOCAL_HOST: return "LOCAL_HOST";
      case ESP_GATT_CONN_FAIL_ESTABLISH: return "FAIL_ESTABLISH";
      case ESP_GATT_CONN_LMP_TIMEOUT: return "LMP_TIMEOUT";
      case ESP_GATT_CONN_CONN_CANCEL: return "CANCELLED";
      case ESP_GATT_CONN_NONE: return "NONE";
      default: return "UNDEFINED";
    }
}

static String __ble_address_to_string (const uint8_t bleaddr []) {
#define __BLE_MACBYTETOSTRING(byte) String (NIBBLE_TO_HEX_CHAR ((byte) >> 4)) + String (NIBBLE_TO_HEX_CHAR ((byte) & 0xF))
#define __BLE_FORMAT_ADDRESS(addr) __BLE_MACBYTETOSTRING ((addr) [0]) + ":" + __BLE_MACBYTETOSTRING ((addr) [1]) + ":" + __BLE_MACBYTETOSTRING ((addr) [2]) + ":" + __BLE_MACBYTETOSTRING ((addr) [3]) + ":" + __BLE_MACBYTETOSTRING ((addr) [4]) + ":" + __BLE_MACBYTETOSTRING ((addr) [5])
    return __BLE_FORMAT_ADDRESS (bleaddr);
}
static String __ble_linkrole_to_string (const int linkrole) {
    return String (linkrole == 0 ? "master" : "slave");
}

class BluetoothNotifier : protected BLEServerCallbacks {

public:
    typedef struct {
        const String name, serviceUUID, characteristicUUID;
        uint32_t pin;
    } Config;
private:
    const Config& config;

    BLEServer *_server = nullptr;
    BLECharacteristic *_characteristic = nullptr;
    bool _connected = false;

protected:
    void onConnect (BLEServer *, esp_ble_gatts_cb_param_t* param) override { 
        _connected = true;
        DEBUG_PRINTF ("BluetoothNotifier::events: BLE_CONNECTED, %s (conn_id=%d, role=%s)\n",
            __ble_address_to_string (param->connect.remote_bda).c_str (), param->connect.conn_id, __ble_linkrole_to_string (param->connect.link_role).c_str ());
    }
    void onDisconnect (BLEServer *, esp_ble_gatts_cb_param_t* param) override {
        _connected = false;
        DEBUG_PRINTF ("BluetoothNotifier::events: BLE_DISCONNECTED, %s (conn_id=%d, reason=%s)\n",
            __ble_address_to_string (param->disconnect.remote_bda).c_str (), param->disconnect.conn_id, __ble_disconnect_reason ((esp_gatt_conn_reason_t) param->disconnect.reason).c_str ());
    }

public:
    BluetoothNotifier (const Config& cfg) : config (cfg) {}
    void advertise ()  {
        BLEDevice::init (config.name);
        _server = BLEDevice::createServer ();
        _server->setCallbacks (this);
        BLEService *service = _server->createService (config.serviceUUID);
        _characteristic = service->createCharacteristic (config.characteristicUUID, BLECharacteristic::PROPERTY_READ |  BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_NOTIFY);
        _characteristic->addDescriptor (new BLE2902 ());
        _characteristic->setAccessPermissions (ESP_GATT_PERM_READ_ENCRYPTED | ESP_GATT_PERM_WRITE_ENCRYPTED);
        service->start ();
        BLEAdvertising *advertising = BLEDevice::getAdvertising();
        advertising->addServiceUUID (config.serviceUUID);
        advertising->setScanResponse (true);
        advertising->setMinPreferred (0x06);
        advertising->setMinPreferred (0x12);
        advertising->start ();
        BLESecurity *security = new BLESecurity ();
        security->setStaticPIN (config.pin); 
        DEBUG_PRINTF ("BluetoothNotifier::advertise\n");
    }
    // json only
    void notify (const String& data) {
        const int _ble_mtu = 512;
        if (data.length () < _ble_mtu) {
            _characteristic->setValue (data.c_str ());
            _characteristic->notify ();
            DEBUG_PRINTF ("BluetoothNotifier::notify: length=%u\n", data.length ());
        } else {
            JsonSplitter splitter (_ble_mtu, { "type", "time" });
            splitter.splitJson (data, [&] (const String& part, const int elements) {
                _characteristic->setValue (part.c_str ());
                _characteristic->notify ();
                DEBUG_PRINTF ("BluetoothNotifier::notify: length=%u, part=%u, elements=%d\n", data.length (), part.length (), elements);
                delay (20);
            });
        }
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

#include <Wire.h>
#include <array>

class OpenSmart_QuadMotorDriver {
public:
    static constexpr int MotorCount = 4;
    static constexpr int I2cAddress = 0x20;
    enum MotorID {
        MOTOR_A = 0,
        MOTOR_B = 1,
        MOTOR_C = 2,
        MOTOR_D = 3
    };
    enum MotorDirection {
        MOTOR_CLOCKWISE = 0,
        MOTOR_ANTICLOCKWISE = 1
    };
    typedef std::array <int, MotorCount> MotorSpeedPins;
private:
    enum MotorControl {
        MOTOR_CONTROL_OFF = 0x00,
        MOTOR_CONTROL_ANTICLOCKWISE = 0x01,
        MOTOR_CONTROL_CLOCKWISE = 0x02
    };
    static constexpr uint8_t encode_controlvalue (const int motorID, const uint8_t directions, const uint8_t value) {
        return (directions & (~(0x03 << (2 * motorID)))) | (value << (2 * motorID));
    }
    
    const uint8_t _i2c;
    const MotorSpeedPins _pwms;
    uint8_t _directions;

    void directions_update (int motorID, uint8_t value) {
        for (int id = 0; id < MotorCount; id ++)
            if (motorID == -1 || motorID == id)
                _directions = encode_controlvalue (_directions, id, value);
        Wire.beginTransmission (_i2c);
        Wire.write (_directions);
        Wire.endTransmission ();
    }
    void speed_update (int motorID, uint8_t value) {
        for (int id = 0; id < MotorCount; id ++)
            if (motorID == -1 || motorID == id)
                analogWrite (_pwms [id], value);
    }

public:
    OpenSmart_QuadMotorDriver (const uint8_t i2c, const MotorSpeedPins& pwms) : _i2c (i2c), _pwms (pwms), _directions (0x00) {
        for (uint8_t pin : _pwms)
            pinMode (pin, OUTPUT), digitalWrite (pin, LOW);
        Wire.begin ();
        stop ();
    }
    void setSpeed (int speed, int motorID = -1) { speed_update (motorID, (speed > 255) ? (uint8_t) 255 : static_cast <uint8_t> (speed)); }
    void setDirection (int direction, int motorID = -1) { directions_update (motorID, (direction == MOTOR_CLOCKWISE) ? MOTOR_CONTROL_CLOCKWISE : MOTOR_CONTROL_ANTICLOCKWISE); }
    void stop (int motorID = -1) { directions_update (motorID, MOTOR_CONTROL_OFF); }
};

// -----------------------------------------------------------------------------------------------
