
// -----------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------

#include "Program.hpp"
#include "Testing.hpp"

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

const String build_info (String (__build_name) + " V" + String (__build_vers) + "-" + String (__build_time));

// -----------------------------------------------------------------------------------------------

Program *program;
Watchdog watchdog (60);

void setup () {
    DEBUG_START ();
    DEBUG_PRINTF ("\n*** %s ***\n\n", build_info.c_str ());

    // must be a more elegant way
    // Tester_HardwareInterfaces tester;
    // tester.run ();
    // temperatureCalibration ();

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
