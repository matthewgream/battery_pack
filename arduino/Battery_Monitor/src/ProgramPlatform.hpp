
// -----------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------

#include <driver/temperature_sensor.h>

class ProgramPlatformArduino : public Alarmable, public Diagnosticable {

public:
    struct Config {
        int pinRandomNoise;
    };

private:
    const Config &config;

    static constexpr int HEAP_FREE_PERCENTAGE_MINIMUM = 5;
    static constexpr int TEMP_RANGE_MINIMUM = 0, TEMP_RANGE_MAXIMUM = 80;
    const unsigned long code_size, heap_size;
    temperature_sensor_handle_t temp_handle;
    bool temp_okay;
    const int reset_reason;
    const std::pair<String, String> reset_details;
    const bool reset_okay;

public:
    explicit ProgramPlatformArduino (const Config &conf) :
        Alarmable ({ AlarmCondition (ALARM_SYSTEM_MEMORYLOW, [this] () { return ((100 * esp_get_minimum_free_heap_size ()) / heap_size) < HEAP_FREE_PERCENTAGE_MINIMUM; }),
                     AlarmCondition (ALARM_SYSTEM_BADRESET, [this] () { return ! reset_okay; }) }),
        config (conf),
        code_size (ESP.getSketchSize ()),
        heap_size (ESP.getHeapSize ()),
        reset_reason (getResetReason ()),
        reset_details (getResetDetails (reset_reason)),
        reset_okay (getResetOkay (reset_reason)) {
        const temperature_sensor_config_t temp_sensor = TEMPERATURE_SENSOR_CONFIG_DEFAULT (TEMP_RANGE_MINIMUM, TEMP_RANGE_MAXIMUM);
        float temp_read = 0.0f;
        temp_okay = (temperature_sensor_install (&temp_sensor, &temp_handle) == ESP_OK && temperature_sensor_enable (temp_handle) == ESP_OK && temperature_sensor_get_celsius (temp_handle, &temp_read) == ESP_OK);
        if (! temp_okay)
            DEBUG_PRINTF ("PlatformArduino::init: could not enable temperature sensor\n");
        DEBUG_PRINTF ("PlatformArduino::init: code=%lu, heap=%lu, temp=%.2f, reset=%d\n", code_size, heap_size, temp_read, reset_reason);
        RandomNumber::seed (analogRead (config.pinRandomNoise));    // XXX TIDY
    }

protected:
    void collectDiagnostics (JsonVariant &obj) const override {
        JsonObject platform = obj ["platform"].to<JsonObject> ();
        JsonObject code = platform ["code"].to<JsonObject> ();
        code ["size"] = code_size;
        JsonObject heap = platform ["heap"].to<JsonObject> ();
        heap ["size"] = heap_size;
        heap ["free"] = esp_get_free_heap_size ();
        heap ["min"] = esp_get_minimum_free_heap_size ();
        float temp;
        if (temp_okay && temperature_sensor_get_celsius (temp_handle, &temp) == ESP_OK)
            platform ["temp"] = temp;
        JsonObject reset = platform ["reset"].to<JsonObject> ();
        reset ["okay"] = reset_okay;
        reset ["reason"] = reset_details.first;
        // reset ["details"] = reset_details.second;
    }
};

// -----------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------
