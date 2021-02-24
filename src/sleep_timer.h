
#define SLEEP_TIMER_H
#include <ArduinoJson.h>
#include <MicroTasks.h>

#define SLEEP_TIMER_NOT_CONNECTED_FLAG (1 << 0)
#define SLEEP_TIMER_CONNECTED_FLAG (1 << 1)
#define SLEEP_TIMER_DISCONNECTED_FLAG (1 << 2)

class SleepTimer : MicroTasks::Task {
    private:
        uint8_t evseState;
        unsigned long goToSleep = 0;
        bool counting = true;
        bool displayUpdates = true;

    protected:
        void setup();
        unsigned long loop(MicroTasks::WakeReason reason);
        void updateDisplay();
        void setTimer(uint16_t seconds);
        void stopTimer();

    public:
        SleepTimer();
        void begin();
        void onStateChange(uint8_t state);
        // -------------------------------------------------------------------
        // Enable/disable display updates
        // When enabled, the timer loop will display timer information
        // on the lcd regularly.
        // -------------------------------------------------------------------
        void sleep_timer_display_updates(bool enabled);
};

extern SleepTimer sleepTimer;