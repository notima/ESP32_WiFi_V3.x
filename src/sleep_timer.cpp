#include "sleep_timer.h"
#include "input.h"
#include "lcd.h"
#include "app_config.h"
#include "debug.h"
#include <openevse.h>
#include <lcd_manager.h>

SleepTimer::SleepTimer() : MicroTasks::Task() {

}

void SleepTimer::begin(){
    MicroTask.startTask(this);
}

void SleepTimer::setup(){
}

String createTimeString(uint16_t seconds){
    String str = "";
    uint8_t min = seconds / 60;
    uint8_t sec = seconds % 60;
    str.concat(min / 10);
    str.concat(min % 10);
    str.concat(":");
    str.concat(sec / 10);
    str.concat(sec % 10);
    return str;
}

unsigned long SleepTimer::loop(MicroTasks::WakeReason reason){
    if(evseState != state){
        onStateChange(state);
    }

    if(counting){
        if(millis() > goToSleep){
            lcdManager.clearPages();
            // Simulate a button press in case there is a timer active
            rapiSender.sendCmd(F("$F1"));
            rapiSender.sendCmd(config_pause_uses_disabled() ? F("$FD") : F("$FS"));
            counting = false;
        }
    }

    return 1000;
}

void SleepTimer::setTimer(uint16_t seconds){
    goToSleep = millis() + seconds * 1000;
    counting = true;
    DEBUG.printf("Timer set for %d seconds\n", seconds);
    lcdManager.addInfoPage(1, [this](LcdManager::Lcd *lcd){
        lcd->display("Going to", 0, 0);
        String timerMsg = "sleep in: ";
        int timeLeft = (goToSleep - millis()) / 1000;
        timerMsg.concat(createTimeString(timeLeft));
        lcd->display(timerMsg, 0, 1);
    }, 500);
}

void SleepTimer::stopTimer() {
    counting = false;
    DEBUG.println("Timer stopped");
    lcdManager.clearPages();
}

void SleepTimer::onStateChange(uint8_t state){
    DEBUG.printf("State changed to: %d\n", state);
    stopTimer();

    if(evseState >= OPENEVSE_STATE_SLEEPING && state == OPENEVSE_STATE_NOT_CONNECTED && config_rfid_enabled){
        if((sleep_timer_enabled_flags & SLEEP_TIMER_NOT_CONNECTED_FLAG) == SLEEP_TIMER_NOT_CONNECTED_FLAG){
            lcdManager.clearPages();
            DEBUG.println("Woke up");
            setTimer(sleep_timer_not_connected);
            lcdManager.addInfoPage(0, [](LcdManager::Lcd *lcd){
                lcd->display("Connect your", 0, 0);
                lcd->display("vehicle", 0, 1);
            });
        }
    }
    else if(state == OPENEVSE_STATE_CONNECTED){
        if((sleep_timer_enabled_flags & SLEEP_TIMER_CONNECTED_FLAG) == SLEEP_TIMER_CONNECTED_FLAG){
            lcdManager.clearPages();
            DEBUG.println("Connected");
            setTimer(sleep_timer_connected);
            lcdManager.addInfoPage(0, [](LcdManager::Lcd *lcd){
                lcd->display("Not Charging", 0, 0);
                lcd->display("", 0, 1);
            });
        }
    }
    else if(evseState >= OPENEVSE_STATE_CONNECTED && state == OPENEVSE_STATE_NOT_CONNECTED){
        if((sleep_timer_enabled_flags & SLEEP_TIMER_DISCONNECTED_FLAG) == SLEEP_TIMER_DISCONNECTED_FLAG){
            lcdManager.clearPages();
            DEBUG.println("Disconnected");
            setTimer(sleep_timer_disconnected);
            std::function<void(LcdManager::Lcd *lcd)> displayUpdate = [](LcdManager::Lcd *lcd){
                lcd->display("Disconnected", 0, 0);
                lcd->display("", 0, 1);
            };
            lcdManager.addInfoPage(0, displayUpdate);
            lcdManager.addInfoPage(2, displayUpdate);
            lcdManager.addInfoPage(3, displayUpdate);
        }
    }

    evseState = state;
}

void SleepTimer::sleep_timer_display_updates(bool enabled){
    displayUpdates = enabled;
}

SleepTimer sleepTimer;