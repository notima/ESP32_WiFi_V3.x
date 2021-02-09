
#ifndef SLEEP_TIMER_H
#define SLEEP_TIMER_H
#include <ArduinoJson.h>

// Time before going to sleep after waking up and no vehicle is connected
#define SLEEP_TIMER_NOT_CONNECTED (1 * 60 * 1000)
// Time before going to sleep after a vehicle is connected
#define SLEEP_TIMER_CONNECTED (5 * 60 * 1000)
// Time before going to sleep after a vehicle is disconnected
#define SLEEP_TIMER_DISCONNECTED (10 * 1000)


extern void sleep_timer_loop();

// -------------------------------------------------------------------
// Called when the state changes from sleeping to not connected
// -------------------------------------------------------------------
extern void on_wake_up();

// -------------------------------------------------------------------
// Called when the state changes to connected
// -------------------------------------------------------------------
extern void on_vehicle_connected();

// -------------------------------------------------------------------
// Called when the state changes from connected to disconnected
// -------------------------------------------------------------------
extern void on_vehicle_disconnected();

// -------------------------------------------------------------------
// Enable/disable display updates
// When enabled, the timer loop will display timer information
// on the lcd regularly.
// -------------------------------------------------------------------
extern void sleep_timer_display_updates(bool enabled);

#endif