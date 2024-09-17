
// -----------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------

#include "Program.hpp"

static Program program;

// -----------------------------------------------------------------------------------------------

static constexpr const char __build_name [] = "battery monitor";
static constexpr const char __build_vers [] = "0.99" ;

#define BUILD_Y ((__DATE__[7] - '0') * 1000 + (__DATE__[8] - '0') * 100 + (__DATE__[9] - '0') * 10 + (__DATE__[10] - '0'))
#define BUILD_M ((__DATE__[0] == 'J') ? ((__DATE__[1] == 'a') ? 1 : ((__DATE__[2] == 'n') ? 6 : 7)) : (__DATE__[0] == 'F') ? 2 : (__DATE__[0] == 'M') ? ((__DATE__[2] == 'r') ? 3 : 5) \
    : (__DATE__[0] == 'A') ? ((__DATE__[2] == 'p') ? 4 : 8) : (__DATE__[0] == 'S') ? 9 : (__DATE__[0] == 'O') ? 10 : (__DATE__[0] == 'N') ? 11 : (__DATE__[0] == 'D') ? 12 : 0)
#define BUILD_D ((__DATE__[4] == ' ') ? (__DATE__[5] - '0') : (__DATE__[4] - '0') * 10 + (__DATE__[5] - '0'))
#define BUILD_T __TIME__
static constexpr const char __build_time [] = {
  BUILD_Y/1000 + '0', (BUILD_Y%1000)/100 + '0', (BUILD_Y%100)/10 + '0', BUILD_Y%10 + '0',  BUILD_M/10 + '0', BUILD_M%10 + '0',  BUILD_D/10 + '0', BUILD_D%10 + '0',
  BUILD_T [0], BUILD_T [1], BUILD_T [3], BUILD_T [4], BUILD_T [6], BUILD_T [7],
  '\0'
};

const String build_info (String (__build_name) + String (" V") + String (__build_vers) + String ("-") + String (__build_time)); 

// -----------------------------------------------------------------------------------------------

void setup () {

    DEBUG_START ();
    DEBUG_PRINTLN ();
    DEBUG_PRINTLN ("*** " + build_info + " ***");
    DEBUG_PRINTLN ();

    exception_catcher ([&] () { program.setup (); });
}

void loop () {
    exception_catcher ([&] () { program.loop (); });
    program.sleep ();
}

// -----------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------
