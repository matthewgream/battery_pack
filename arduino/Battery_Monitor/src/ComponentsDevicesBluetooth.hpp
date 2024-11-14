
// -----------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------

#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

#include <esp_gap_ble_api.h>

// -----------------------------------------------------------------------------------------------

class BluetoothDevice : private Singleton<BluetoothDevice>, public ConnectionReceiver<BluetoothDevice>::Insertable, protected BLEServerCallbacks, protected BLECharacteristicCallbacks, public JsonSerializable {
public:
    static inline constexpr uint16_t MIN_MTU = 32, MAX_MTU = 517;

    typedef struct {
        String name, serviceUUID, characteristicUUID;
        uint32_t pin;
        interval_t intervalConnectionCheck;
    } Config;

private:
    //

    void onConnect (BLEServer *server, esp_ble_gatts_cb_param_t *param) override {
        DEBUG_PRINTF ("BluetoothDevice::events: BLE_CONNECTED, %s (conn_id=%d, role=%s)\n",
                      _address_to_string (param->connect.remote_bda).c_str (),
                      param->connect.conn_id,
                      _linkrole_to_string (param->connect.link_role).c_str ());
        _connected (param->connect.conn_id, param->connect.remote_bda);
    }
    void onMtuChanged (BLEServer *, esp_ble_gatts_cb_param_t *param) override {
        DEBUG_PRINTF ("BluetoothDevice::events: BLE_MTU_CHANGED, (conn_id=%d, mtu=%u)\n", param->mtu.conn_id, param->mtu.mtu);
        _connected_mtuChanged (param->mtu.mtu);
    }
    void onRead (BLECharacteristic *characteristic, esp_ble_gatts_cb_param_t *param) override {
        // param->offset, param->is_long
        DEBUG_PRINTF ("BluetoothDevice::events: BLE_CHARACTERISTIC_READ, (uuid=%s, offset=%d, is_long=%d)\n", characteristic->getUUID ().toString ().c_str (), param->read.offset, param->read.is_long);
        String str;
        _connected_readRequested (str);    // XXX uuid
        if (! str.isEmpty ())
            characteristic->setValue (str.c_str ());
    }
    void onWrite (BLECharacteristic *characteristic, esp_ble_gatts_cb_param_t *) override {
        // param->write.offset, param->write.len, param->write.value
        DEBUG_PRINTF ("BluetoothDevice::events: BLE_CHARACTERISTIC_WRITE, (uuid=%s, value=%s)\n", characteristic->getUUID ().toString ().c_str (), characteristic->getValue ().c_str ());
        _connected_writeReceived (characteristic->getValue ());
    }
    void onNotify (BLECharacteristic *characteristic) override {
        DEBUG_PRINTF ("BluetoothDevice::events: BLE_CHARACTERISTIC_NOTIFY, (uuid=%s, value=%s)\n", characteristic->getUUID ().toString ().c_str (), characteristic->getValue ().c_str ());
    }
    void onStatus (BLECharacteristic *characteristic, Status s, uint32_t code) override {
        if (s != BLECharacteristicCallbacks::Status::SUCCESS_NOTIFY)
            DEBUG_PRINTF ("BluetoothDevice::events: BLE_CHARACTERISTIC_STATUS, (uuid=%s, status=%s. code=%lu)\n", characteristic->getUUID ().toString ().c_str (), _status_to_string (s).c_str (), code);
    }
    void onDisconnect (BLEServer *, esp_ble_gatts_cb_param_t *param) override {
        const String reason = _disconnect_reason_to_string ((esp_gatt_conn_reason_t) param->disconnect.reason);
        DEBUG_PRINTF ("BluetoothDevice::events: BLE_DISCONNECTED, %s (conn_id=%d, reason=%s)\n",
                      _address_to_string (param->disconnect.remote_bda).c_str (),
                      param->disconnect.conn_id,
                      reason.c_str ());
        _disconnected (param->disconnect.conn_id, param->disconnect.remote_bda, reason);
    }
    void events (const esp_gap_ble_cb_event_t event, const esp_ble_gap_cb_param_t *param) {
        if (event == ESP_GAP_BLE_READ_RSSI_COMPLETE_EVT) {
            if (param->read_rssi_cmpl.status == ESP_BT_STATUS_SUCCESS) {
                DEBUG_PRINTF ("BluetoothDevice::events: BLE_READ_RSSI_COMPLETE, (rssi=%d, quality=%s)\n", param->read_rssi_cmpl.rssi, ConnectionSignalTracker::toString (ConnectionSignalTracker::signalQuality (param->read_rssi_cmpl.rssi)).c_str ());
                _connected_rssiResponse (param->read_rssi_cmpl.rssi);
            }
        }
    }
    static void __gapEventHandler (esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param) {
        auto instance = Singleton<BluetoothDevice>::instance ();
        if (instance != nullptr)
            instance->events (event, param);
    }

    //

    const Config &config;

    BLEServer *_server = nullptr;
    BLECharacteristic *_characteristic = nullptr;
    void _serverInitAndStartService () {
        BLEDevice::init (config.name);
        _server = BLEDevice::createServer ();
        // BLEDevice::setEncryptionLevel (ESP_BLE_SEC_ENCRYPT);
        esp_ble_gap_register_callback (__gapEventHandler);
        _server->setCallbacks (this);
        BLEService *service = _server->createService (config.serviceUUID);
        _characteristic = service->createCharacteristic (config.characteristicUUID, BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_NOTIFY);
        _characteristic->setCallbacks (this);
        _characteristic->addDescriptor (new BLE2902 ());
        // _characteristic->setAccessPermissions (ESP_GATT_PERM_READ_ENCRYPTED | ESP_GATT_PERM_WRITE_ENCRYPTED); // must enable BLEDevice::setEncryptionLevel (ESP_BLE_SEC_ENCRYPT)
        _characteristic->setAccessPermissions (ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE);    // no need for encryption
        service->start ();
        BLESecurity *security = new BLESecurity ();
        security->setStaticPIN (config.pin);
    }

    //

    bool _advertisingActive = false;
    void _advertisingInit () {
        BLEAdvertising *advertising = BLEDevice::getAdvertising ();
        advertising->addServiceUUID (config.serviceUUID);
        advertising->setScanResponse (true);
        advertising->setMinPreferred (0x06);
        advertising->setMinPreferred (0x12);
    }
    void _advertisingEnable () {
        if (! _advertisingActive) {
            _server->getAdvertising ()->start ();
            _advertisingActive = true;
            DEBUG_PRINTF ("BluetoothDevice::advertise: start, name=%s, pin=%d, mac=%s\n", config.name.c_str (), config.pin, getMacAddressBlue ().c_str ());
        }
    }
    void _advertisingDisable () {
        if (_advertisingActive) {
            _server->getAdvertising ()->stop ();
            _advertisingActive = false;
            DEBUG_PRINTF ("BluetoothDevice::advertise: stop\n");
        }
    }

    //

    esp_bd_addr_t _peerAddress;
    uint16_t _peerConnId = 0;
    uint16_t _peerMtu = 0;
    ConnectionReceiver<BluetoothDevice> _connectionReceiver;
    ConnectionSender<BluetoothDevice> _connectionSender;
    ActivationTracker _connections;
    ActivationTrackerWithDetail _disconnections;
    Intervalable _connectionActiveChecker;
    ConnectionSignalTracker _connectionSignalTracker;
    bool _connectionActive = false;
    void _connect () {
        _advertisingEnable ();
    }
    void _connected (const uint16_t conn_id, const esp_bd_addr_t &addr) {
        if (! _connectionActive) {
            _advertisingDisable ();
            _peerMtu = 0;
            _peerConnId = conn_id;
            memcpy (_peerAddress, addr, sizeof (esp_bd_addr_t));
            _connectionSignalTracker.reset ();
            _connectionActiveChecker.reset ();
            _connections++;
            _connectionActive = true;
            _server->updatePeerMTU (_peerConnId, MAX_MTU);
            esp_ble_gap_read_rssi (_peerAddress);
        }
    }
    void _connected_mtuChanged (const uint16_t mtu) {
        if (_connectionActive) {
            _peerMtu = mtu;
        }
    }
    void _connected_rssiResponse (const int8_t rssi) {
        if (_connectionActive) {
            _connectionSignalTracker.update (rssi);
        }
    }
    void _connected_writeReceived (const String &str) {
        if (_connectionActive) {
            _connectionReceiver.insert (str);
        }
    }
    void _connected_readRequested (String &str) {
        if (_connectionActive) {
            //
        }
    }
    bool _connected_send (const String &data) {
        if (_connectionActive) {
            if (! _peerMtu) {
                DEBUG_PRINTF ("BluetoothDevice::send: awaiting peer MTU negotiation, sliently discarding");
                return true;
            } else if (_peerMtu < MIN_MTU) {
                DEBUG_PRINTF ("BluetoothDevice::send: peer MTU size (%u) is below minimum (%u)", _peerMtu, MIN_MTU);
                return false;
            }
            return _connectionSender.send (data, _peerMtu - 3);
        } else {
            DEBUG_PRINTF ("BluetoothDevice::send: not connected");
            return false;
        }
    }
    void _disconnect (const bool forced = false) {    // in case of weird situation
        if (_connectionActive || forced) {
            if (! _connectionActive && forced)
                _advertisingDisable ();
            _server->disconnect (_peerConnId);
            _connectionActive = false;
            _disconnections += String ("Locally initiated");
            if (forced)
                delay (500);
            _connectionReceiver.drain ();
            _connect ();
        }
    }
    void _disconnected (const uint16_t conn_id, const esp_bd_addr_t &addr, const String &reason) {
        if (_connectionActive) {
            _connectionActive = false;
            _disconnections += reason;
            _connectionReceiver.drain ();
            _connect ();
        }
    }
    void _connection_init () {
        _advertisingInit ();
    }
    void _connection_process () {
        if (_connectionActive) {
            if (_connectionActiveChecker) {
                if (! _peerMtu) {
                    DEBUG_PRINTF ("BluetoothDevice::process: connection timeout waiting for MTU, resetting\n");
                    _disconnect (true);
                } else {
                    esp_ble_gap_read_rssi (_peerAddress);
                }
            }
            _connectionReceiver.process ();
        }
    }

    //

public:
    explicit BluetoothDevice (const Config &cfg, const ConnectionSignalTracker::Callback connectionSignalCallback = nullptr) :
        Singleton<BluetoothDevice> (this),
        ConnectionReceiver<BluetoothDevice>::Insertable (&_connectionReceiver),
        config (cfg),
        _connectionReceiver (this),
        _connectionSender ([&] (const String &str) {
            _characteristic->setValue (str.c_str ());
            _characteristic->notify ();
        }),
        _connectionActiveChecker (config.intervalConnectionCheck),
        _connectionSignalTracker (connectionSignalCallback) { }

    void begin () {
        _serverInitAndStartService ();
        _connection_init ();
        _connect ();
    }
    void process () {
        if (! _connectionActive && ! _advertisingActive) {
            DEBUG_PRINTF ("BluetoothDevice::process: not connected and not advertising, resetting\n");
            _disconnect (true);
            return;
        }
        _connection_process ();
    }
    bool available () const {
        return _connectionActive && _peerMtu;
    }
    //
    // json only
    bool send (const String &data) {
        return _connected_send (data);
    }
    bool payloadExceeded () const {
        return _connectionSender._payloadExceeded;
    }
    //
    void serialize (JsonVariant &obj) const override {
        obj ["macaddr"] = getMacAddressBlue ();
        if ((obj ["connected"] = _connectionActive)) {
            obj ["address"] = _address_to_string (_peerAddress);
            obj ["mtu"] = _peerMtu;
            obj ["signal"] = _connectionSignalTracker;
        }
        if (_connectionSender._payloadExceeded)
            obj ["payloadExceeded"] = _connectionSender._payloadExceeded;
        if (_connectionReceiver._failures)
            obj ["receiveFailures"] = _connectionReceiver._failures;
        if (_connections)
            obj ["connects"] = _connections;
        if (_disconnections)
            obj ["disconnects"] = _disconnections;
    }

private:
    static String _disconnect_reason_to_string (const esp_gatt_conn_reason_t reason) {
        switch (reason) {
        case ESP_GATT_CONN_UNKNOWN :
            return "UNKNOWN";
        case ESP_GATT_CONN_L2C_FAILURE :
            return "L2C_FAILURE";
        case ESP_GATT_CONN_TIMEOUT :
            return "TIMEOUT";
        case ESP_GATT_CONN_TERMINATE_PEER_USER :
            return "PEER_USER";
        case ESP_GATT_CONN_TERMINATE_LOCAL_HOST :
            return "LOCAL_HOST";
        case ESP_GATT_CONN_FAIL_ESTABLISH :
            return "FAIL_ESTABLISH";
        case ESP_GATT_CONN_LMP_TIMEOUT :
            return "LMP_TIMEOUT";
        case ESP_GATT_CONN_CONN_CANCEL :
            return "CANCELLED";
        case ESP_GATT_CONN_NONE :
            return "NONE";
        default :
            return "UNDEFINED_(" + ArithmeticToString (static_cast<int> (reason)) + ")";
        }
    }
    static String _address_to_string (const esp_bd_addr_t bleaddr) {
        return BytesToHexString<6> (bleaddr);
    }
    static String _linkrole_to_string (const int linkrole) {
        return linkrole == 0 ? "master" : "slave";
    }
    static String _status_to_string (const BLECharacteristicCallbacks::Status status) {
        switch (status) {
        case BLECharacteristicCallbacks::Status::ERROR_NO_CLIENT :
            return "NO_CLIENT";
        case BLECharacteristicCallbacks::Status::ERROR_NOTIFY_DISABLED :
            return "NOTIFY_DISABLED";
        case BLECharacteristicCallbacks::Status::SUCCESS_NOTIFY :
            return "NOTIFY_SUCCESS";
        case BLECharacteristicCallbacks::Status::ERROR_INDICATE_DISABLED :
            return "INDICATE_DISABLED";
        case BLECharacteristicCallbacks::Status::ERROR_INDICATE_TIMEOUT :
            return "INDICATE_TIMEOUT";
        case BLECharacteristicCallbacks::Status::ERROR_INDICATE_FAILURE :
            return "INDICATE_FAILURE";
        case BLECharacteristicCallbacks::Status::SUCCESS_INDICATE :
            return "INDICATE_SUCCESS";
        default :
            return "UNDEFINED_(" + ArithmeticToString (static_cast<int> (status)) + ")";
        }
    }
};

// -----------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------
