#include <Arduino.h>

extern void load_balancing_loop();

// -------------------------------------------------------------------
// Activate charging station after making sure it is safe to do so
// -------------------------------------------------------------------
extern void safe_wakeup();

extern void load_balance_rapi_result(String device, String result);