#include "sleep_timer.h"
#include "input.h"
#include "lcd.h"
#include "app_config.h"
#include "debug.h"
#include <openevse.h>

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

void SleepTimer::updateDisplay(){
    uint8_t messageToDisplay = millis() / 2000 % 4;
    if(messageToDisplay == 0 && state == OPENEVSE_STATE_NOT_CONNECTED && counting){
        lcd_display("Connect your", 0, 0, 0, LCD_CLEAR_LINE);
        lcd_display("vehicle", 0, 1, 1100, LCD_CLEAR_LINE);
    }else if(messageToDisplay == 0 && state == OPENEVSE_STATE_CONNECTED){
        lcd_display("Not Charging", 0, 1, 1100, LCD_CLEAR_LINE);
    }else if(messageToDisplay == 1 && (state == OPENEVSE_STATE_NOT_CONNECTED || state == OPENEVSE_STATE_CONNECTED)){
        lcd_display("Going to", 0, 0, 0, LCD_CLEAR_LINE);
        String timerMsg = "sleep in: ";
        int timeLeft = (goToSleep - millis()) / 1000;
        timerMsg.concat(createTimeString(timeLeft));
        lcd_display(timerMsg, 0, 1, 1100, LCD_CLEAR_LINE);
    }else if(messageToDisplay == 0 && state >= OPENEVSE_STATE_SLEEPING){
        lcd_display("Scan RFID tag", 0, 0, 0, LCD_CLEAR_LINE);
        lcd_display("to start", 0, 1, 1100, LCD_CLEAR_LINE);
    }
}

unsigned long SleepTimer::loop(MicroTasks::WakeReason reason){
    if(counting){
        if(millis() > goToSleep){
            // Simulate a button press in case there is a timer active
            rapiSender.sendCmd(F("$F1"));
            rapiSender.sendCmd(config_pause_uses_disabled() ? F("$FD") : F("$FS"));
            counting = false;
        }
    }

    if(displayUpdates){
        updateDisplay();
    }

    return 1000;
}

void SleepTimer::setTimer(uint16_t seconds){
    goToSleep = millis() + seconds * 1000;
    counting = true;
    DEBUG.printf("Timer set for %d seconds\n", seconds);
}

void SleepTimer::stopTimer() {
    counting = false;
    DEBUG.println("Timer stopped");
}

void SleepTimer::onStateChange(uint8_t state){
    DEBUG.printf("State changed to: %d\n", state);
    stopTimer();

    if(evseState >= OPENEVSE_STATE_SLEEPING && state == OPENEVSE_STATE_NOT_CONNECTED && config_rfid_enabled){
        if((sleep_timer_enabled_flags & SLEEP_TIMER_NOT_CONNECTED_FLAG) == SLEEP_TIMER_NOT_CONNECTED_FLAG){
            DEBUG.println("Woke up");
            setTimer(sleep_timer_not_connected);
        }
    }
    else if(state == OPENEVSE_STATE_CONNECTED){
        if((sleep_timer_enabled_flags & SLEEP_TIMER_CONNECTED_FLAG) == SLEEP_TIMER_CONNECTED_FLAG){
            DEBUG.println("Connected");
            setTimer(sleep_timer_connected);
        }
    }
    else if(evseState >= OPENEVSE_STATE_CONNECTED && state == OPENEVSE_STATE_NOT_CONNECTED){
        if((sleep_timer_enabled_flags & SLEEP_TIMER_DISCONNECTED_FLAG) == SLEEP_TIMER_DISCONNECTED_FLAG){
            DEBUG.println("Disconnected");
            setTimer(sleep_timer_disconnected);
        }
    }

    evseState = state;
}

void SleepTimer::sleep_timer_display_updates(bool enabled){
    displayUpdates = enabled;
}

SleepTimer sleepTimer;