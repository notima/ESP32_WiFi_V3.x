#include "load_balancer.h"
#include "app_config.h"
#include "debug.h"
#include "input.h"
#include "openevse.h"
#include "mqtt.h"
#include "lcd_manager.h"
#include "sleep_timer.h"

#define RAPI_GET_STATE "/rapi/in/$GS"
#define RAPI_SET_CURRENT "/rapi/in/$SC"

#define MQTT_TIMEOUT 10000
#define START_DELAY 10000

#define MQTT_TIMEOUT_MSG "     Unable to determine a safe current level. "
#define WAITING_FOR_CURRENT_MSG "     Charging will start when current is available. "

#define LOAD_BALANCER_STATUS_IDLE 0
#define LOAD_BALANCER_STATUS_WAKING 1
#define LOAD_BALANCER_STATUS_WAITING 2
#define LOAD_BALANCER_STATUS_TIMEOUT 3
#define LOAD_BALANCER_STATUS_STARTING 4

LoadBalancer::LoadBalancer() :
  MicroTasks::Task() {
}

void LoadBalancer::begin() {
  MicroTask.startTask(this);
}

void LoadBalancer::setup() {
}

void LoadBalancer::sendCommand(String topic, String command, std::function<void(boolean, String)> callback){
    mqtt_publish(topic, command);
    rapi_callback = callback;
    lastCommand = command;
}

void LoadBalancer::setCurrent(int current, std::function<void()> onSuccess){
    String cmd = "$SC ";
    cmd.concat(current);
    rapiSender.sendCmd(cmd, [onSuccess](int ret) {
        if(RAPI_RESPONSE_OK == ret) {
            onSuccess();
        }
    });
}

String getToken(String string, int index){
    char charArr[string.length() + 1];
    string.toCharArray(charArr, string.length()+1);
    char* token = strtok(charArr, " ");
    for(uint8_t i = 0; i < index; i++){
        if(!token)
            return "";
        token = strtok(NULL, " ");
    }
    return token;
}

void LoadBalancer::safetyCheck(std::function<void(boolean)> onSafe){
    sendCommand(load_balancing_topics + RAPI_GET_STATE, "", [this, onSafe](boolean ok, String result){
        if(!ok)
            return;
        uint8_t state = atoi(getToken(result, 1).c_str());
        uint8_t current;
        boolean otherAwake;
        if(state >= OPENEVSE_STATE_SLEEPING){
            current = total_current;
            otherAwake = false;
        }else{
            current = safe_current_level;
            otherAwake = true;
        }
        setCurrent(current, [onSafe, otherAwake](){
            onSafe(otherAwake);
        });
    });
}

void showWaitIndicator(){
    lcdManager.display("Please wait...", 0, 0, 1);
    lcdManager.display("", 0, 1, 3100);
    rapiSender.sendCmd(F("$FB 6"));
}

uint8_t msgRoll = 0;
unsigned long LoadBalancer::loop(MicroTasks::WakeReason reason){
    if(!config_load_balancing_enabled())
        return 5000;
    switch (status) {
    case LOAD_BALANCER_STATUS_WAKING:
        if(wakeupStarted - millis() < MQTT_TIMEOUT){
            showWaitIndicator();
            if(safe_current_level == 0){
                status = LOAD_BALANCER_STATUS_WAITING;
                msgRoll = 0;
                lcdManager.claim([this](LcdManager::Lcd * lcd){
                    lcd->display("Not charging", 0, 0);
                    lcd->display(WAITING_FOR_CURRENT_MSG + (msgRoll++ % strlen(WAITING_FOR_CURRENT_MSG)), 0, 1, false);
                }, 500);
                break;
            }
            safetyCheck([this](boolean otherAwake){
                String currentStr = "";
                currentStr.concat(safe_current_level);
                sendCommand(load_balancing_topics + RAPI_SET_CURRENT, currentStr, [this, otherAwake](boolean ok, String result){
                    if(ok){
                        status = LOAD_BALANCER_STATUS_STARTING;
                        lcdManager.display("Starting", 0, 1, START_DELAY + 2000);
                        startTime = millis() + (START_DELAY * otherAwake);
                        DEBUG.print("OTHER AWAKE?: ");
                        DEBUG.println(otherAwake);
                    }else{
                        status = LOAD_BALANCER_STATUS_IDLE;
                    }
                });
            });
        }else{
            status = LOAD_BALANCER_STATUS_TIMEOUT;
            msgRoll = 0;
            DEBUG.println("safety check timed out.");
            char msg[50];
            sprintf(msg, "Safety check timed out! %s might be offline.", load_balancing_topics.c_str());
            mqtt_log_error(msg);
            msgRoll = 0;
            lcdManager.claim([this](LcdManager::Lcd *lcd){
                lcd->display("Out of order", 0, 0);
                lcd->display(MQTT_TIMEOUT_MSG + (msgRoll++ % strlen(MQTT_TIMEOUT_MSG)), 0, 1);
                if(msgRoll >= strlen(MQTT_TIMEOUT_MSG) * 2){
                    status = LOAD_BALANCER_STATUS_IDLE;
                    lcdManager.release();
                }
            }, 500);
        }
        break;

    case LOAD_BALANCER_STATUS_STARTING:
        if(millis() > startTime){
            rapiSender.sendCmd(F("$FE"));
            status = LOAD_BALANCER_STATUS_IDLE;
        }
        break;

    case LOAD_BALANCER_STATUS_WAITING:
        rapiSender.sendCmd(F("$FB 6"));
        safetyCheck([this](boolean otherAwake){
            if(!otherAwake){
                lcdManager.release();
                rapiSender.sendCmd(F("$FE"));
                status = LOAD_BALANCER_STATUS_IDLE;
            }
        });
        break;
    
    case LOAD_BALANCER_STATUS_TIMEOUT:
        safetyCheck([this](boolean otherAwake){
            status = LOAD_BALANCER_STATUS_IDLE;
        });
        rapiSender.sendCmd(F("$FB 1"));
        break;
        
    case LOAD_BALANCER_STATUS_IDLE:
    default:
        safetyCheck([](boolean otherAwake){});
    }
    return 1000;
}

void LoadBalancer::wakeup(){
    status = LOAD_BALANCER_STATUS_WAKING;
    DEBUG.println("Waking up");
    wakeupStarted = millis() + MQTT_TIMEOUT;
    showWaitIndicator();
}

void LoadBalancer::reportRapiResult(String device, String result){
    boolean ok = getToken(result, 0) == "$OK";
    if(!ok && device == load_balancing_topics){
        rapiSender.sendCmd(F("$FB 1"));
        char msg[100];
        sprintf(msg ,"Incorrect response received from %s. Last command: %s. Response: %s.", device.c_str(), lastCommand.c_str(), result.c_str());
        mqtt_log_error(msg);
    }
    rapi_callback(ok, result);
}

LoadBalancer loadBalancer;