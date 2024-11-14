
// -----------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------

#include "BluetoothTPMS.hpp"

#include <BLEDevice.h>
#include <BLEScan.h>

#include <array>

// -----------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------

class ProgramManageBluetoothTPMS : private Singleton<ProgramManageBluetoothTPMS>, public Component, public Diagnosticable, protected BLEAdvertisedDeviceCallbacks {

    static constexpr int SCAN_TIME = 5;                                // seconds
    static constexpr uint16_t SCAN_INTERVAL = 75, SCAN_WINDOW = 50;    // scan for 75 msec, window for 50 msec

public:
    using Address = String;
    struct Config {
        Address front, rear;
    };

private:
    const Config &config;
    struct Tyre {
        ActivationTracker updated;
        TpmsDataBluetooth details;
    };
    Tyre front, rear;
    BLEScan *scanner = nullptr;
    bool scanning = false;

public:
    explicit ProgramManageBluetoothTPMS (const Config &conf) :
        Singleton<ProgramManageBluetoothTPMS> (this),
        config (conf) { }

    void begin () override {
        BLEDevice::init ("");
        scanner = BLEDevice::getScan ();
        scanner->setAdvertisedDeviceCallbacks (this);
        scanner->setActiveScan (false);
        scanner->setInterval (SCAN_INTERVAL);
        scanStart ();
    }
    void process () override {
        if (! scanning)
            scanStart ();
    }

protected:
    Tyre *getTyre (const String &address) {
        if (address == config.front)
            return &front;
        else if (address == config.rear)
            return &rear;
        return nullptr;
    }
    void onResult (BLEAdvertisedDevice device) {
        Tyre *tyre = getTyre (device.getAddress ().toString ());
        if (tyre) {
            tyre->updated++;
            tyre->details = TpmsDataBluetooth::fromAdvertisedDevice (device);
        }
    }
    static void __endOfScan (BLEScanResults) {
        auto instance = Singleton<ProgramManageBluetoothTPMS>::instance ();
        if (instance != nullptr)
            instance->scanStart ();
    }
    void scanStart () {
        DEBUG_PRINTF ("BluetoothTPMSManager: scanStart\n");
        scanning = scanner->start (SCAN_TIME, __endOfScan);
    }

    void collectDiagnostics (JsonVariant &obj) const override {
        JsonObject f = obj ["front"].to<JsonObject> ();
        f ["updated"] = front.updated;
        f ["details"] = front.details;
        JsonObject r = obj ["rear"].to<JsonObject> ();
        r ["updated"] = rear.updated;
        r ["details"] = rear.details;
    }
};

// -----------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------

bool convertToJson (const TpmsData &src, JsonVariant dst) {
    dst ["pressure"]= src.pressure;
    dst ["temperature"]= src.temperature;
    dst ["battery"]= src.battery;
    auto alarmStrings = src.alarmStrings ();
    if (alarmStrings.size () > 0) {
        JsonArray alarms = dst ["alarms"].to<JsonArray> ();
        for (const auto& alarmString : alarmStrings)
            alarms.add (alarmString);
    }
    return true;
}
bool convertToJson (const TpmsDataBluetooth &src, JsonVariant dst) {
    dst.set ((TpmsData)src);
    JsonObject bluetooth;
    if (src.name.has_value ()) bluetooth ["name"] = src.name.value ();
    if (src.rssi.has_value ()) bluetooth ["rssi"] = src.rssi.value ();
    if (src.txpower.has_value ()) bluetooth ["txpower"] = src.txpower.value ();
    if (bluetooth.size () > 0)
        dst ["bluetooth"] = bluetooth;
    return true;
}

// -----------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------
