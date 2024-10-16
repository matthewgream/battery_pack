
// -----------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------

class DeviceManager: public Component, public Diagnosticable {
public:
    typedef struct {
        BluetoothDevice::Config blue;
        MQTTPublisher::Config mqtt;
    } Config;

    using BooleanFunc = std::function <bool ()>;

private:
    const Config &config;
    const BooleanFunc _networkIsAvailable;

    BluetoothDevice _blue;
    MQTTPublisher _mqtt;

public:
    explicit DeviceManager (const Config& cfg, const BooleanFunc networkIsAvailable): config (cfg), _networkIsAvailable (networkIsAvailable), _blue (config.blue), _mqtt (config.mqtt) {}
    void begin () override {
        _blue.begin ();
        _mqtt.setup ();
    }
    void process () override {
        _blue.process ();
        if (_networkIsAvailable ())
            _mqtt.process ();
    }

    BluetoothDevice& blue () { return _blue; }
    MQTTPublisher& mqtt () { return _mqtt; }

protected:
    void collectDiagnostics (JsonVariant &obj) const override {
        JsonObject sub = obj ["devices"].to <JsonObject> ();
            sub ["blue"] = _blue;
            sub ["mqtt"] = _mqtt;
    }
};

// -----------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------

class ControlManager: public Component, public Diagnosticable {
public:
    typedef struct {
    } Config;

private:
    const Config &config;

    class BluetoothWriteHandler_TypeCtrl: public BluetoothWriteHandler_TypeSpecific {
    public:
        BluetoothWriteHandler_TypeCtrl (): BluetoothWriteHandler_TypeSpecific ("ctrl") {};
        bool process (BluetoothDevice& device, const String& time, const String& ctrl, JsonDocument& doc) override {
            DEBUG_PRINTF ("BluetoothWriteHandler_TypeCtrl:: type=ctrl, time=%s, ctrl='%s'\n", time.c_str (), ctrl.c_str ());
            // XXX
            // turn on/off diag
            // force reboot
            // wipe spiffs data file
            // changw wifi
            // suppress specific alarms
            return true;
        }
    };
    class BluetoothReadHandler_TypeCtrl: public BluetoothReadHandler_TypeSpecific {
    public:
        BluetoothReadHandler_TypeCtrl (): BluetoothReadHandler_TypeSpecific ("ctrl") {};
        bool process (BluetoothDevice& device, const String& time, const String& ctrl, JsonDocument& doc) override {
            DEBUG_PRINTF ("BluetoothReadHandler_TypeCtrl:: type=ctrl\n");
            // XXX
            // send all of the variables that can be controlled
            return false;
        }
    };

public:
    explicit ControlManager (const Config& cfg, BluetoothDevice& blue): config (cfg) {
        blue.insert ({ { String ("ctrl"), std::make_shared <BluetoothWriteHandler_TypeCtrl> () } });
        blue.insert ({ { std::make_shared <BluetoothReadHandler_TypeCtrl> () } });
    }

protected:
    void collectDiagnostics (JsonVariant &) const override {
    }
};

// -----------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------

#include <functional>

extern String ota_image_check (const String& json, const String& type, const String& vers, const String& addr);

class UpdateManager: public Component, public Alarmable, public Diagnosticable {
public:
    typedef struct {
        interval_t intervalUpdate;
        time_t intervalCheck;
        String json, type, vers;
    } Config;

    using BooleanFunc = std::function <bool ()>;

private:
    const Config& config;
    const BooleanFunc _networkIsAvailable;

    PersistentData _persistent_data;
    PersistentValue <uint32_t> _persistent_data_previous;
    PersistentValue <String> _persistent_data_version;
    bool _available;

    Intervalable _interval;

public:
    UpdateManager (const Config& cfg, const BooleanFunc networkIsAvailable): Alarmable ({
            AlarmCondition (ALARM_UPDATE_VERS, [this] () { return _available; })
        }), config (cfg), _networkIsAvailable (networkIsAvailable),
        _persistent_data ("updates"), _persistent_data_previous (_persistent_data, "previous", 0), _persistent_data_version (_persistent_data, "version", String ("")),
        _available (!static_cast <String> (_persistent_data_version).isEmpty ()),
        _interval (config.intervalUpdate) {}
    void process () override {
        if (static_cast <bool> (_interval)) {
            if (_networkIsAvailable ()) {
                time_t previous = (time_t) static_cast <uint32_t> (_persistent_data_previous), current = time (nullptr);
                if ((current > 0 && (current - previous) > (config.intervalCheck / 1000)) || (previous > current)) {
                    String version = ota_image_check (config.json, config.type, config.vers, getMacAddress ());
                    if (_persistent_data_version != version) {
                        _persistent_data_version = version;
                        _available = !version.isEmpty ();
                    }
                    _persistent_data_previous = current;
                }
            }
        }
    }

protected:
    void collectDiagnostics (JsonVariant &obj) const override {
        JsonObject sub = obj ["updates"].to <JsonObject> ();
            sub ["current"] = config.vers;
            if (_available)
                sub ["available"] = static_cast <String> (_persistent_data_version);
            const time_t time = (time_t) static_cast <uint32_t> (_persistent_data_previous);
            if (time)
                sub ["checked"] = getTimeString (time);
    }
};

// -----------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------
