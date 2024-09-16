
// -----------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------

#include "Program.hpp"

static Program program;

// -----------------------------------------------------------------------------------------------

static constexpr const char __compile_name [] = "battery monitor";
static constexpr const char __compile_vers [] = "0.99";

#define COMPILE_Y ((__DATE__[7] - '0') * 1000 + (__DATE__[8] - '0') * 100 + (__DATE__[9] - '0') * 10 + (__DATE__[10] - '0'))
#define COMPILE_M ((__DATE__[0] == 'J') ? ((__DATE__[1] == 'a') ? 1 : ((__DATE__[2] == 'n') ? 6 : 7)) : (__DATE__[0] == 'F') ? 2 : (__DATE__[0] == 'M') ? ((__DATE__[2] == 'r') ? 3 : 5) \
    : (__DATE__[0] == 'A') ? ((__DATE__[2] == 'p') ? 4 : 8) : (__DATE__[0] == 'S') ? 9 : (__DATE__[0] == 'O') ? 10 : (__DATE__[0] == 'N') ? 11 : (__DATE__[0] == 'D') ? 12 : 0)
#define COMPILE_D ((__DATE__[4] == ' ') ? (__DATE__[5] - '0') : (__DATE__[4] - '0') * 10 + (__DATE__[5] - '0'))
#define COMPILE_T __TIME__

static constexpr char __compile_time [] = {
  COMPILE_Y/1000 + '0', (COMPILE_Y%1000)/100 + '0', (COMPILE_Y%100)/10 + '0', COMPILE_Y%10 + '0',  COMPILE_M/10 + '0', COMPILE_M%10 + '0',  COMPILE_D/10 + '0', COMPILE_D%10 + '0',
  COMPILE_T [0], COMPILE_T [1], COMPILE_T [3], COMPILE_T [4], COMPILE_T [6], COMPILE_T [7],
  '\0'
};

void setup () {

    DEBUG_START ();
    DEBUG_PRINTLN ();
    DEBUG_PRINTLN ("*** " + __compile_name + " V" + __compile_vers + "-" + __compile_time + " ***");
    DEBUG_PRINTLN ();

    exception_catcher ([&] () { program.setup (); });
}

void loop () {
    exception_catcher ([&] () { program.loop (); });
    program.sleep ();
}

// -----------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------
