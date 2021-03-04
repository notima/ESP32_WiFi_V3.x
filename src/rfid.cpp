#include "rfid.h"

#include "debug.h"
#include "mqtt.h"
#include "app_config.h"
#include "input.h"
#include "openevse.h"
#include "lcd_manager.h"
#include "load_balancer.h"

RfidTask::RfidTask() :
  MicroTasks::Task(),
  nfc(PN532_IRQ, PN532_POLLING)
{
}

void RfidTask::begin(){
    MicroTask.startTask(this);
}

void RfidTask::setup(){

    if(!config_rfid_enabled()){
        status = RFID_STATUS_NOT_ENABLED;
        return;
    }
    
    wakeup();
}

boolean RfidTask::wakeup(){
    boolean awake = nfc.begin();
    if(awake){
        status = RFID_STATUS_ACTIVE;
    }else{
        DEBUG.println("RFID module did not respond!");
        mqtt_log_error("RFID module did not respond!");
        status = RFID_STATUS_NOT_FOUND;
    }
    return awake;
}

void RfidTask::scanCard(){
    NFCcard = nfc.getInformation();
    String uid = nfc.readUid();

    if(waitingForTag > 0){
        waitingForTag = 0;
        cardFound = true;
        lcdManager.display("Tag detected!", 0, 0, 3000);
        lcdManager.display(uid, 0, 1, 3000);
    }else{
        // Check if tag is stored locally
        char storedTags[rfid_storage.length() + 1];
        rfid_storage.toCharArray(storedTags, rfid_storage.length()+1);
        char* storedTag = strtok(storedTags, ",");
        while(storedTag)
        {
            String storedTagStr = storedTag;
            storedTagStr.replace(" ", "");
            uid.trim();
            if(storedTagStr.compareTo(uid) == 0){
                authenticatedTag = uid;
                lcdManager.setColor(LCD_COLOR_YELLOW);
                break;
            }
            storedTag = strtok(NULL, ",");
        }

        // Send to MQTT broker
        DynamicJsonDocument data(4096);
        data["rfid"] = uid;
        mqtt_publish(data);
    }
}

unsigned long RfidTask::loop(MicroTasks::WakeReason reason){
    unsigned long nextScan = SCAN_FREQ;

    if(!config_rfid_enabled()){
        if(status != RFID_STATUS_NOT_FOUND)
            status = RFID_STATUS_NOT_ENABLED;
        return MicroTask.WaitForMask;
    }

    if(isAuthenticated() && state >= OPENEVSE_STATE_SLEEPING){
        lcdManager.setColor(LCD_COLOR_YELLOW);
        if(allowedToStart()){
            DEBUG.println("STARING");
            lcdManager.release();
            if(config_load_balancing_enabled()){
                loadBalancer.wakeup();
            }else{
                rapiSender.sendCmd(F("$FE"));
            }
        } else {
            DEBUG.println("NOT STARTING");
            lcdManager.claim([](LcdManager::Lcd *lcd){
                lcd->display("Charging will", 0, 0);
                if(rapiSender.sendCmdSync(F("$GD")) == 0){
                    uint8_t startHr = String(rapiSender.getToken(1)).toInt();
                    uint8_t startMn = String(rapiSender.getToken(2)).toInt();
                    String msg = "start at ";
                    msg.concat(startHr);
                    msg.concat(":");
                    msg.concat(startMn);
                    lcd->display(msg, 0, 1);
                }
            }, 5000);
        }
        return 5000;
    }

    if(state != evseState){
        if(state >= OPENEVSE_STATE_SLEEPING){
            std::function<void(LcdManager::Lcd *lcd)> displayUpdate = [](LcdManager::Lcd *lcd){
                lcd->display("Scan RFID tag", 0, 0);
                lcd->display("to start", 0, 1);
            };
            lcdManager.addInfoPage(0, displayUpdate);
            lcdManager.addInfoPage(1, displayUpdate);
        }
        evseState = state;
    }

    if(state < OPENEVSE_STATE_SLEEPING && !isAuthenticated()){
        rapiSender.sendCmd(F("$F1"));
        rapiSender.sendCmd(config_pause_uses_disabled() ? F("$FD") : F("$FS"));
    }
    
    boolean foundCard = (state >= OPENEVSE_STATE_SLEEPING || waitingForTag) && nfc.scan();

    if(foundCard && !hasContact){
        scanCard();
        hasContact = true;
        return nextScan;
    }

    if(!foundCard && hasContact){
        hasContact = false;
    }

    return nextScan;
}

boolean RfidTask::allowedToStart(){
    boolean allowedToStart = false;
    if(rapiSender.sendCmdSync(F("$GD")) == 0){
        uint8_t startHr = String(rapiSender.getToken(1)).toInt();
        uint8_t startMn = String(rapiSender.getToken(2)).toInt();
        uint8_t stopHr = String(rapiSender.getToken(3)).toInt();
        uint8_t stopMn = String(rapiSender.getToken(4)).toInt();
        uint16_t startTime = startHr * 60 + startMn;
        uint16_t stopTime = stopHr * 60 + stopMn;
        if(rapiSender.sendCmdSync(F("$GT")) == 0){
            uint8_t hour = String(rapiSender.getToken(4)).toInt();
            uint8_t minute = String(rapiSender.getToken(5)).toInt();
            uint16_t currentTime = hour * 60 + minute;

            if(startTime < stopTime){
                allowedToStart = currentTime >= startTime && currentTime < stopTime;
            }else{
                allowedToStart = currentTime >= startTime || currentTime < stopTime;
            }

            DEBUG.print("Start: ");
            DEBUG.print(startHr);
            DEBUG.print(":");
            DEBUG.println(startMn);
            DEBUG.print("Stop: ");
            DEBUG.print(stopHr);
            DEBUG.print(":");
            DEBUG.println(stopMn);
            DEBUG.print("Now: ");
            DEBUG.print(hour);
            DEBUG.print(":");
            DEBUG.println(minute);
            DEBUG.print("allowed to start? ");
            DEBUG.println(allowedToStart);
        }
    }
    return allowedToStart;
}

uint8_t RfidTask::getStatus(){
    return status;
}

bool RfidTask::isAuthenticated(){
    return !authenticatedTag.isEmpty();
}

String RfidTask::getAuthenticatedTag(){
    return authenticatedTag;
}

void RfidTask::resetAuthentication(){
    authenticatedTag = "";
}

void RfidTask::waitForTag(uint8_t seconds){
    if(!config_rfid_enabled())
        return;
    waitingForTag = seconds;
    stopWaiting = millis() + seconds * 1000;
    cardFound = false;
    lcdManager.claim([this](LcdManager::Lcd *lcd){
        if(waitingForTag <= 0){
            lcdManager.release();
            return;
        }
        waitingForTag = (stopWaiting - millis()) / 1000;
        String msg = "tag... ";
        msg.concat(waitingForTag);
        lcd->display("Waiting for RFID", 0, 0);
        lcd->display(msg, 0, 1);
    }, 500);
}

DynamicJsonDocument RfidTask::rfidPoll() {
    const size_t capacity = JSON_ARRAY_SIZE(4);
    DynamicJsonDocument doc(capacity);
    if(waitingForTag > 0){
        // respond with remainding time
        doc["status"] = "waiting";
        doc["timeLeft"] = waitingForTag;
    } 
    else if(cardFound){
        // respond with the scanned tags uid and reset
        doc["status"] = "done";
        doc["scanned"] = nfc.readUid();
        cardFound = false;
    }
    else  {
        doc["status"] = "notStarted";
    }
    return doc;
}

RfidTask rfid;
