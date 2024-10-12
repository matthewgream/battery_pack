
// -----------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------

#include <ArduinoJson.h>

#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

#include <esp_gap_ble_api.h>

// -----------------------------------------------------------------------------------------------

enum class ConnectionQuality {
    UNKNOWN,
    EXCELLENT,
    GOOD,
    FAIR,
    POOR
};

class ConnectionQualityTracker: public JsonSerializable {
public:
    using RssiType = int8_t;
    using Callback = std::function <void (const RssiType rssi, const ConnectionQuality quality)>;
    static constexpr RssiType RSSI_THRESHOLD_EXCELLENT = RssiType (-50);
    static constexpr RssiType RSSI_THRESHOLD_GOOD      = RssiType (-60);
    static constexpr RssiType RSSI_THRESHOLD_FAIR      = RssiType (-70);

private:
    const Callback _callback;
    RssiType _rssiLast = std::numeric_limits <RssiType>::min ();
    ConnectionQuality _qualityLast = ConnectionQuality::UNKNOWN;

    static inline ConnectionQuality connectionQuality (const RssiType rssi) {
        if (rssi > RSSI_THRESHOLD_EXCELLENT) return ConnectionQuality::EXCELLENT;
        else if (rssi > RSSI_THRESHOLD_GOOD) return ConnectionQuality::GOOD;
        else if (rssi > RSSI_THRESHOLD_FAIR) return ConnectionQuality::FAIR;
        else                                 return ConnectionQuality::POOR;
    }

public:        
    ConnectionQualityTracker (const Callback callback = nullptr): _callback (callback) {}
    RssiType rssi () const {
        return _rssiLast;
    }
    void update (const RssiType rssi) {
        ConnectionQuality quality = connectionQuality (_rssiLast = rssi);
        if (quality != _qualityLast) {
            _qualityLast = quality;
            if (_callback)
                _callback (rssi, quality);
        }
    }
    void reset () {
        _rssiLast = std::numeric_limits <RssiType>::min ();
        _qualityLast = ConnectionQuality::UNKNOWN; 
    }
    void serialize (JsonObject &obj) const {
        obj ["rssi"] = _rssiLast;
        obj ["quality"] = toString ();
    }
    String toString () const {
        switch (_qualityLast) {
          case ConnectionQuality::UNKNOWN: return "UNKNOWN";
          case ConnectionQuality::POOR: return "POOR";
          case ConnectionQuality::FAIR: return "FAIR";
          case ConnectionQuality::GOOD: return "GOOD";
          case ConnectionQuality::EXCELLENT: return "EXCELLENT";
          default: return "UNDEFINED";
        }
    }
};

// -----------------------------------------------------------------------------------------------

class BluetoothNotifier: private Singleton <BluetoothNotifier>, public JsonSerializable, protected BLEServerCallbacks, protected BLECharacteristicCallbacks {

public:
    static inline constexpr uint16_t MAX_MTU = 517;

    typedef struct {
        String name, serviceUUID, characteristicUUID;
        uint32_t pin;
        uint16_t mtu;
        interval_t intervalConnectionCheck;
    } Config;

private:
    const Config &config;

    BLEServer *_server = nullptr;
    BLECharacteristic *_characteristic = nullptr;
    bool _advertising = false;
    
    esp_bd_addr_t _peerAddress;
    int _mtuNegotiated = -1, _mtuExceeded = 0;
    ConnectionQualityTracker _connectionQualityTracker;
    bool _connected = false;
    Intervalable _intervalConnectionCheck;
    ActivationTracker _connections; ActivationTrackerWithDetail _disconnections;

    void onConnect (BLEServer * server, esp_ble_gatts_cb_param_t* param) override {
        DEBUG_PRINTF ("BluetoothNotifier::events: BLE_CONNECTED, %s (conn_id=%d, role=%s)\n",
            __ble_address_to_string (param->connect.remote_bda).c_str (), param->connect.conn_id, __ble_linkrole_to_string (param->connect.link_role).c_str ());
        advertise (false);
        _mtuNegotiated = -1; _mtuExceeded = 0;
        _connectionQualityTracker.reset ();
        memcpy (_peerAddress, param->connect.remote_bda, sizeof (esp_bd_addr_t));
        _intervalConnectionCheck.reset (); _connections ++; _connected = true;
        esp_ble_gap_read_rssi (_peerAddress);
        server->updatePeerMTU (param->connect.conn_id, MAX_MTU);
    }
    void onDisconnect (BLEServer *, esp_ble_gatts_cb_param_t* param) override {
        const String reason = __ble_disconnect_reason ((esp_gatt_conn_reason_t) param->disconnect.reason);
        DEBUG_PRINTF ("BluetoothNotifier::events: BLE_DISCONNECTED, %s (conn_id=%d, reason=%s)\n",
            __ble_address_to_string (param->disconnect.remote_bda).c_str (), param->disconnect.conn_id, reason.c_str ());
        _connected = false; _disconnections += reason;
        advertise (true);
    }
    void onMtuChanged (BLEServer *, esp_ble_gatts_cb_param_t *param) override {
        DEBUG_PRINTF ("BluetoothNotifier::events: BLE_MTU_CHANGED, (conn_id=%d, mtu=%u)\n", param->mtu.conn_id, param->mtu.mtu);
        _mtuNegotiated = static_cast <int> (param->mtu.mtu);
    }

    void onWrite (BLECharacteristic *) override {
        DEBUG_PRINTF  ("BluetoothNotifier::events: BLE_CHARACTERISTIC_WRITE\n");
    }
    void onNotify (BLECharacteristic *characteristic) override {
        DEBUG_PRINTF  ("BluetoothNotifier::events: BLE_CHARACTERISTIC_NOTIFY, (uuid=%s, value=%s)\n", characteristic->getUUID ().toString ().c_str (), characteristic->getValue ().c_str ());
    }
    void onStatus (BLECharacteristic* characteristic, Status s, uint32_t code) override {
        if (s != BLECharacteristicCallbacks::Status::SUCCESS_NOTIFY && s != BLECharacteristicCallbacks::Status::SUCCESS_INDICATE)
            DEBUG_PRINTF  ("BluetoothNotifier::events: BLE_CHARACTERISTIC_STATUS, (uuid=%s, status=%s. code=%lu)\n", characteristic->getUUID ().toString ().c_str (), __ble_status_to_string (s).c_str (), code);
    }
    void onRssiRead (esp_ble_gap_cb_param_t *param) {
        if (param->read_rssi_cmpl.status == ESP_BT_STATUS_SUCCESS) {
            _connectionQualityTracker.update (param->read_rssi_cmpl.rssi);
            DEBUG_PRINTF ("BluetoothNotifier::onRssiRead: rssi=%d (%s)\n", param->read_rssi_cmpl.rssi, _connectionQualityTracker.toString ().c_str ());
        }
    }    

    static void __gapEventHandler (esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t* param) {
        BluetoothNotifier *bluetoothNotifier = Singleton <BluetoothNotifier>::instance ();
        if (bluetoothNotifier != nullptr) bluetoothNotifier->events (event, param);
    }
    void events (esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param) {
        if (event == ESP_GAP_BLE_READ_RSSI_COMPLETE_EVT)
            onRssiRead (param);
    }
    
    void advertise (const bool enable) {
        if (enable && !_advertising) {
            _server->getAdvertising ()->start ();
            _advertising = true;
            DEBUG_PRINTF ("BluetoothNotifier::advertising start\n");
        } else if (!enable && _advertising) {
            _server->getAdvertising ()->stop ();
            _advertising = false;
            DEBUG_PRINTF ("BluetoothNotifier::advertising stop\n");
        }
    }

public:
    BluetoothNotifier (const Config& cfg, const ConnectionQualityTracker::Callback connectionQualityCallback = nullptr): Singleton <BluetoothNotifier> (this), config (cfg), _connectionQualityTracker (connectionQualityCallback), _intervalConnectionCheck (config.intervalConnectionCheck) {}

    bool advertise ()  {
        BLEDevice::init (config.name);
        if (!(_server = BLEDevice::createServer ())) {
            DEBUG_PRINTF ("BluetoothNotifier::advertise unable to create BLE server\n");
            return false;
        }
        esp_ble_gap_register_callback (__gapEventHandler);
        _server->setCallbacks (this);
        BLEService *service = _server->createService (config.serviceUUID);
        _characteristic = service->createCharacteristic (config.characteristicUUID, BLECharacteristic::PROPERTY_READ |  BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_NOTIFY);
        _characteristic->setCallbacks (this);
        _characteristic->addDescriptor (new BLE2902 ());
        _characteristic->setAccessPermissions (ESP_GATT_PERM_READ_ENCRYPTED | ESP_GATT_PERM_WRITE_ENCRYPTED);
        service->start ();
        BLESecurity *security = new BLESecurity ();
        security->setStaticPIN (config.pin);
        BLEAdvertising *advertising = BLEDevice::getAdvertising();
        advertising->addServiceUUID (config.serviceUUID);
        advertising->setScanResponse (true);
        advertising->setMinPreferred (0x06);
        advertising->setMinPreferred (0x12);
        advertise (true);
        return true;
    }
    // json only
    void notify (const String& data) {
        if (!_connected || _mtuNegotiated == -1) {
            DEBUG_PRINTF ("BluetoothNotifier::notify: not in notifying state");
            return;
        }
        const uint16_t maxPayloadSize = static_cast <uint16_t> (_mtuNegotiated - 3);
        if (data.length () <= maxPayloadSize) {
            _characteristic->setValue (data.c_str ());
            _characteristic->notify ();
            DEBUG_PRINTF ("BluetoothNotifier::notify: length=%u\n", data.length ());
        } else {
            JsonSplitter splitter (maxPayloadSize, { "type", "time" });
            splitter.splitJson (data, [&] (const String& part, const int elements) {
                const size_t length = part.length ();
                _characteristic->setValue (part.c_str ());
                _characteristic->notify ();
                DEBUG_PRINTF ("BluetoothNotifier::notify: data=%u, part=%u, elements=%d\n", data.length (), length, elements);
                if (length > maxPayloadSize && length > _mtuExceeded)
                    _mtuExceeded = length;
                delay (20);
            });
        }
    }
    inline bool mtuexceeded () const {
        return _mtuExceeded > 0;
    }
    inline bool connected () const {
        return _connected;
    }
    void reset () {
        DEBUG_PRINTF ("BluetoothNotifier::reset\n");
        advertise (false);
        _server->disconnect (0);
        delay (500);
        _connected = false;
        advertise (true);
    }
    void check () {
        if (!_connected && !_advertising) {
            DEBUG_PRINTF ("BluetoothNotifier::check: not connected and not advertising, resetting\n");
            reset ();
        } else if (_connected && _intervalConnectionCheck) {
            if (_mtuNegotiated == -1) {
                DEBUG_PRINTF ("BluetoothNotifier::check: connection timeout, resetting\n");
                reset ();
            } else {
                esp_ble_gap_read_rssi (_peerAddress);
            }
        }
    }
    //
    void serialize (JsonObject &obj) const override {
        JsonObject bluetooth = obj ["bluetooth"].to <JsonObject> ();
        if ((bluetooth ["connected"] = _connected)) {
            bluetooth ["address"] = BLEDevice::getAddress ().toString ();
            bluetooth ["devices"] = _server->getPeerDevices (true).size ();
            bluetooth ["mtu"] = _mtuNegotiated;
            _connectionQualityTracker.serialize (bluetooth);
        }
        if (_mtuExceeded > 0)
            bluetooth ["mtuExceeded"] = _mtuExceeded;
        if (_connections.number () > 0)
            _connections.serialize (bluetooth ["connects"].to <JsonObject> ());
        if (_disconnections.number () > 0)
            _disconnections.serialize (bluetooth ["disconnects"].to <JsonObject> ());
    }

private:
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
    static String __ble_address_to_string (const esp_bd_addr_t bleaddr) {
        #define __BLE_MACBYTETOSTRING(byte) String (NIBBLE_TO_HEX_CHAR ((byte) >> 4)) + String (NIBBLE_TO_HEX_CHAR ((byte) & 0xF))
        #define __BLE_FORMAT_ADDRESS(addr) __BLE_MACBYTETOSTRING ((addr) [0]) + ":" + __BLE_MACBYTETOSTRING ((addr) [1]) + ":" + __BLE_MACBYTETOSTRING ((addr) [2]) + ":" + __BLE_MACBYTETOSTRING ((addr) [3]) + ":" + __BLE_MACBYTETOSTRING ((addr) [4]) + ":" + __BLE_MACBYTETOSTRING ((addr) [5])
        return __BLE_FORMAT_ADDRESS (bleaddr);
    }
    static String __ble_linkrole_to_string (const int linkrole) {
        return linkrole == 0 ? "master" : "slave";
    }
    static String __ble_status_to_string (const BLECharacteristicCallbacks::Status status) {
        switch (status) {
          case BLECharacteristicCallbacks::Status::ERROR_NO_CLIENT: return "NO_CLIENT";
          case BLECharacteristicCallbacks::Status::ERROR_NOTIFY_DISABLED: return "NOTIFY_DISABLED";
          case BLECharacteristicCallbacks::Status::SUCCESS_NOTIFY: return "NOTIFY_SUCCESS";
          case BLECharacteristicCallbacks::Status::ERROR_INDICATE_DISABLED: return "INDICATE_DISABLED";
          case BLECharacteristicCallbacks::Status::ERROR_INDICATE_TIMEOUT: return "INDICATE_TIMEOUT";
          case BLECharacteristicCallbacks::Status::ERROR_INDICATE_FAILURE: return "INDICATE_FAILURE";
          case BLECharacteristicCallbacks::Status::SUCCESS_INDICATE: return "INDICATE_SUCCESS";
          default: return "UNDEFINED";
        }
    }
};

// -----------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------
