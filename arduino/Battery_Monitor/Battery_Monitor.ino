
// -----------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------

#include "Program.hpp"
#include "Factory.hpp"

// -----------------------------------------------------------------------------------------------

#define BUILD_Y ((__DATE__[7] - '0') * 1000 + (__DATE__[8] - '0') * 100 + (__DATE__[9] - '0') * 10 + (__DATE__[10] - '0'))
#define BUILD_M ((__DATE__[0] == 'J') ? ((__DATE__[1] == 'a') ? 1 : ((__DATE__[2] == 'n') ? 6 : 7)) : (__DATE__[0] == 'F') ? 2 : (__DATE__[0] == 'M') ? ((__DATE__[2] == 'r') ? 3 : 5) \
    : (__DATE__[0] == 'A') ? ((__DATE__[2] == 'p') ? 4 : 8) : (__DATE__[0] == 'S') ? 9 : (__DATE__[0] == 'O') ? 10 : (__DATE__[0] == 'N') ? 11 : (__DATE__[0] == 'D') ? 12 : 0)
#define BUILD_D ((__DATE__[4] == ' ') ? (__DATE__[5] - '0') : (__DATE__[4] - '0') * 10 + (__DATE__[5] - '0'))
#define BUILD_T __TIME__

static inline constexpr const char __build_name [] = DEFAULT_NAME;
static inline constexpr const char __build_vers [] = DEFAULT_VERS;
static inline constexpr const char __build_time [] = {
    BUILD_Y/1000 + '0', (BUILD_Y%1000)/100 + '0', (BUILD_Y%100)/10 + '0', BUILD_Y%10 + '0',  BUILD_M/10 + '0', BUILD_M%10 + '0',  BUILD_D/10 + '0', BUILD_D%10 + '0',
    BUILD_T [0], BUILD_T [1], BUILD_T [3], BUILD_T [4], BUILD_T [6], BUILD_T [7],
    '\0'
};  
static inline String __build_plat () { String platform = ESP.getChipModel (); platform.toLowerCase (); platform.replace ("-", ""); return platform; }

const String build_info (String (__build_name) + " V" + String (__build_vers) + "-" + String (__build_time) + " (" + __build_plat () + ")");

// -----------------------------------------------------------------------------------------------

Program *program;
Watchdog watchdog (DEFAULT_WATCHDOG_SECS);

void setup () {
    DEBUG_START ();
    const std::pair <String, String> r = getResetDetails ();
    DEBUG_PRINTF ("\n[%s: %s]", r.first.c_str (), r.second.c_str ());
    DEBUG_PRINTF ("\n*** %s ***\n\n", build_info.c_str ());

    // 3-pin connector for DS18B20 is GND/VCC/DAT and DAT connected to ESP32 pin 21
    // --> install DS18B20 to boot to temperature calibration
    // --> tie pin 21 to high to boot to hardware testing
    // --> leave pin 21 n/c to boot normally to program
#define BOOT_MODE_PIN 21
    if (TemperatureSensor_DS18B20::present (BOOT_MODE_PIN)) {
        DEBUG_PRINTF ("BOOT: TEMPERATURE CALIBRATION\n");
        factory_temperatureCalibration ();
        esp_deep_sleep_start ();
    } else if (digitalRead (BOOT_MODE_PIN) == HIGH) {
        DEBUG_PRINTF ("BOOT: HARDWARE INTERFACE TEST\n");
        factory_hardwareInterfaceTest ();
        esp_deep_sleep_start ();
    }

    watchdog.start ();
    exception_catcher ([&] () { 
        program = new Program ();
        program->setup (); 
    });
}

void loop () {
    exception_catcher ([&] () { 
        program->loop ();
        program->sleep ();
    });
    watchdog.reset ();
}

// -----------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------
