
#ifndef __DEBUG_HPP__
#define __DEBUG_HPP__

// -----------------------------------------------------------------------------------------------

#include <Arduino.h>

//#define DEBUG
#ifdef DEBUG
    bool DEBUG_AVAILABLE = false;
    #define DEBUG_START(...) Serial.begin (DEFAULT_SERIAL_BAUD); DEBUG_AVAILABLE = !Serial ? false : true;
    #define DEBUG_END(...) Serial.flush (); Serial.end ()
    #define DEBUG_PRINT(...) if (DEBUG_AVAILABLE) Serial.print (__VA_ARGS__)
    #define DEBUG_PRINTLN(...) if (DEBUG_AVAILABLE) Serial.println (__VA_ARGS__)
#else
    #define DEBUG_START(...)
    #define DEBUG_END(...)
    #define DEBUG_PRINT(...)
    #define DEBUG_PRINTLN(...)
#endif

// -----------------------------------------------------------------------------------------------

#endif
