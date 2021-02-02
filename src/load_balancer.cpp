#include "load_balancer.h"
#include "app_config.h"
#include "input.h"
#include "openevse.h"
#include "mqtt.h"
#include "lcd.h"

#include <functional>

#define RAPI_GET_STATE "/rapi/in/$GS"
#define RAPI_SET_CURRENT "/rapi/in/$SC"

#define IDLE_TICK_RATE 5000
#define WAKING_TICK_RATE 200

#define MQTT_TIMEOUT 10000
#define MQTT_TIMEOUT_MSG "     Safety check failed! It may not be safe to charge your vehicle at this moment. "

unsigned long nextTick = 0;
unsigned long wakeupStarted = 0;

std::function<void(String)> rapi_callback = [](String result){};
bool waking_up = false;

void sendCommand(String topic, String command, std::function<void(String)> callback){
    mqtt_publish(topic, command);
    rapi_callback = callback;
}

void setCurrent(int current){
    String cmd = "$SC ";
    cmd.concat(current);
    rapiSender.sendCmd(cmd);
}

String getToken(String string, int index){
    char charArr[string.length() + 1];
    string.toCharArray(charArr, string.length()+1);
    char* token = strtok(charArr, " ");
    for(uint8_t i = 0; i < index; i++){
        token = strtok(NULL, " ");
    }
    return token;
}

void safetyCheck(std::function<void()> onSafe){
    sendCommand(load_balancing_topics + RAPI_GET_STATE, "", [onSafe](String result){
        uint8_t state = atoi(getToken(result, 1).c_str());
        if(state == OPENEVSE_STATE_SLEEPING){
            setCurrent(total_current);
            onSafe();
        }else{
            setCurrent(safe_current_level);
            onSafe();
        }
    });
}

uint8_t msgRoll = 0;
void load_balancing_loop(){
    if(millis() < nextTick)
        return;

    if(waking_up){
        if(wakeupStarted - millis() < MQTT_TIMEOUT){
            lcd_display("Please wait...", 0, 0, 0, LCD_CLEAR_LINE);
            lcd_display("", 0, 1, 1000, LCD_CLEAR_LINE);
            rapiSender.sendCmd(F("$FB 7"));
            nextTick = millis() + WAKING_TICK_RATE;
        }else{
            waking_up = false;
            rapiSender.sendCmd(F("$FD"));
            msgRoll = 0;
        }
    }else{
        if(state == OPENEVSE_STATE_DISABLED){
            safetyCheck([](){
                rapiSender.sendCmd(F("$FS"));
            });
            rapiSender.sendCmd(F("$FB 1"));
            lcd_display("", 0, 1, 0, LCD_CLEAR_LINE);
            for(uint8_t i = 0; i < IDLE_TICK_RATE / 500; i++)
                lcd_display(MQTT_TIMEOUT_MSG + (msgRoll++ % strlen(MQTT_TIMEOUT_MSG)), 0, 0, 500, LCD_CLEAR_LINE);
            if(msgRoll >= strlen(MQTT_TIMEOUT_MSG) * 2){
                rapiSender.sendCmd(F("$FS 1"));
            }
        }else{
            safetyCheck([](){});
        }
        nextTick = millis() + IDLE_TICK_RATE;
    }
}

void safe_wakeup(){
    waking_up = true;
    wakeupStarted = millis() + MQTT_TIMEOUT;
    safetyCheck([](){
        String currentStr = "";
        currentStr.concat(safe_current_level);
        sendCommand(load_balancing_topics + RAPI_SET_CURRENT, currentStr, [](String result){
            rapiSender.sendCmd(F("$FE"));
            waking_up = false;
        });
    });
}

void load_balance_rapi_result(String device, String result){
    if(getToken(result, 0) == "$OK" && device == load_balancing_topics)
        rapi_callback(result);
    else
        rapiSender.sendCmd(F("$FB 1"));
}