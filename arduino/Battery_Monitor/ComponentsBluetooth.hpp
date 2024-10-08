
// -----------------------------------------------------------------------------------------------

#include <ArduinoJson.h>

#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

// -----------------------------------------------------------------------------------------------

class BluetoothNotifier: public JsonSerializable, protected BLEServerCallbacks, protected BLECharacteristicCallbacks {

public:
    typedef struct {
        String name, serviceUUID, characteristicUUID;
        uint32_t pin;
        int mtu;
    } Config;

private:
    const Config &config;

    BLEServer *_server = nullptr;
    BLECharacteristic *_characteristic = nullptr;

    bool _advertising = false;

    ActivationTracker _connections; ActivationTrackerWithDetail _disconnections;
    bool _connected = false;
  
    int _maxpacket = 0;

    void onConnect (BLEServer *, esp_ble_gatts_cb_param_t* param) override {
        DEBUG_PRINTF ("BluetoothNotifier::events: BLE_CONNECTED, %s (conn_id=%d, role=%s)\n",
            __ble_address_to_string (param->connect.remote_bda).c_str (), param->connect.conn_id, __ble_linkrole_to_string (param->connect.link_role).c_str ());
        _connected = true; _connections ++;
        advertise (false);
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
    }

    void onWrite (BLECharacteristic *) override {
        DEBUG_PRINTF  ("BluetoothNotifier::events: BLE_CHARACTERISTIC_WRITE\n");
    }
    void onNotify (BLECharacteristic *characteristic) override {
        DEBUG_PRINTF  ("BluetoothNotifier::events: BLE_CHARACTERISTIC_NOTIFY, (uuid=%s, value=%s)\n", characteristic->getUUID ().toString ().c_str (), characteristic->getValue ().c_str ());
    }
    void onStatus(BLECharacteristic* characteristic, Status s, uint32_t code) override {
        DEBUG_PRINTF  ("BluetoothNotifier::events: BLE_CHARACTERISTIC_STATUS, (uuid=%s, status=%s. code=%lu)\n", characteristic->getUUID ().toString ().c_str (), __ble_status_to_string (s).c_str (), code);
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
    BluetoothNotifier (const Config& cfg) : config (cfg) {}

    void advertise ()  {
        BLEDevice::init (config.name);
        _server = BLEDevice::createServer ();
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
    }
    // json only
    void notify (const String& data) {
        if (!_connected) {
            DEBUG_PRINTF ("BluetoothNotifier::notify: not in notifying state");
            return;
        }
        if (data.length () < config.mtu) {
            _characteristic->setValue (data.c_str ());
            _characteristic->notify ();
            DEBUG_PRINTF ("BluetoothNotifier::notify: length=%u\n", data.length ());
        } else {
            JsonSplitter splitter (config.mtu, { "type", "time" });
            splitter.splitJson (data, [&] (const String& part, const int elements) {
                _characteristic->setValue (part.c_str ());
                _characteristic->notify ();
                DEBUG_PRINTF ("BluetoothNotifier::notify: length=%u, part=%u, elements=%d\n", data.length (), part.length (), elements);
                if (part.length () > _maxpacket)
                    _maxpacket = part.length ();
                delay (20);
            });
        }
    }
    inline int maxpacket (void) const {
        return _maxpacket;
    }
    inline bool connected (void) const {
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
            DEBUG_PRINTF ("BluetoothNotifier::check: not connected, resetting\n");
            reset ();
        }
    }
    //
    void serialize (JsonObject &obj) const override {
        JsonObject bluetooth = obj ["bluetooth"].to <JsonObject> ();
        if ((bluetooth ["connected"] = _connected)) {
            bluetooth ["address"] = BLEDevice::getAddress ().toString ();
            bluetooth ["devices"] = _server->getPeerDevices (true).size ();
            bluetooth ["mtu"] = config.mtu;
        }
        bluetooth ["maxpacket"] = _maxpacket;
        _connections.serialize (bluetooth ["connects"].to <JsonObject> ());
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
    static String __ble_address_to_string (const uint8_t bleaddr []) {
        #define __BLE_MACBYTETOSTRING(byte) String (NIBBLE_TO_HEX_CHAR ((byte) >> 4)) + String (NIBBLE_TO_HEX_CHAR ((byte) & 0xF))
        #define __BLE_FORMAT_ADDRESS(addr) __BLE_MACBYTETOSTRING ((addr) [0]) + ":" + __BLE_MACBYTETOSTRING ((addr) [1]) + ":" + __BLE_MACBYTETOSTRING ((addr) [2]) + ":" + __BLE_MACBYTETOSTRING ((addr) [3]) + ":" + __BLE_MACBYTETOSTRING ((addr) [4]) + ":" + __BLE_MACBYTETOSTRING ((addr) [5])
        return __BLE_FORMAT_ADDRESS (bleaddr);
    }
    static String __ble_linkrole_to_string (const int linkrole) {
        return String (linkrole == 0 ? "master" : "slave");
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
