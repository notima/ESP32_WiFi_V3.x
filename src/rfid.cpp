#include "rfid.h"

#include "debug.h"
#include "mqtt.h"
#include "lcd.h"
#include "app_config.h"
#include "RapiSender.h"
#include "input.h"
#include "openevse.h"
#include "load_balancer.h"

DFRobot_PN532_IIC nfc(PN532_IRQ, PN532_POLLING); 

RfidTask::RfidTask() :
    MicroTasks::Task(),
    _evse(NULL),
    _scheduler(NULL),
    _evseStateEvent(this),
    nfc(PN532_IRQ, PN532_POLLING)
{
}

void RfidTask::begin(EvseManager &evse, Scheduler &scheduler){
    _evse = &evse;
    _scheduler = &scheduler;
    MicroTask.startTask(this);
    // HACK
    // Schedulers do not work unless this one exists
    _scheduler->addEvent(100, 0, 0, 0, ~0, EvseState(EvseState::Disabled));
}

void RfidTask::setup(){
    _evse->onStateChange(&_evseStateEvent);

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
        lcd.display("Tag detected!", 0, 0, 0, LCD_CLEAR_LINE);
        lcd.display(uid, 0, 1, 3000, LCD_CLEAR_LINE);
    }else{
        // Check if tag is stored locally
        char storedTags[rfid_storage.length() + 1];
        rfid_storage.toCharArray(storedTags, rfid_storage.length()+1);
        char* storedTag = strtok(storedTags, ",");
        while(storedTag)
        {
            String storedTagStr = storedTag;
            storedTagStr.trim();
            uid.trim();
            if(storedTagStr.compareTo(uid) == 0){
                if(config_load_balancing_enabled){
                    safe_wakeup();
                }else{
                    rapiSender.sendCmd(F("$FE"));
                }
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

void RfidTask::startTimer(uint8_t seconds){
    static int day = 0;
    static int32_t offset = 0;
    _scheduler->getCurrentTime(day, offset);
    offset += seconds;
    int schedulerDay = 1 << day;
    int hour = offset / 3600;
    int minute = (offset % 3600) / 60;
    int second = offset % 60;
    _scheduler->addEvent(0xFFFFFFFF, hour, minute, second, schedulerDay, EvseState(EvseState::Disabled));
}

void RfidTask::abortTimer(){
    _scheduler->removeEvent(0xFFFFFFFF);
}

unsigned long RfidTask::loop(MicroTasks::WakeReason reason){
    unsigned long nextScan = SCAN_FREQ;

    if(!config_rfid_enabled()){
        if(status != RFID_STATUS_NOT_FOUND)
            status = RFID_STATUS_NOT_ENABLED;
        return MicroTask.WaitForMask;
    }

    if(_evseStateEvent.IsTriggered()){
        abortTimer();
        uint8_t _state = evse.getEvseState();
        if(_state == OPENEVSE_STATE_NOT_CONNECTED && lastState >= OPENEVSE_STATE_SLEEPING){
            startTimer(sleep_timer_not_connected);
        }
        else if(_state == OPENEVSE_STATE_NOT_CONNECTED){
            startTimer(sleep_timer_disconnected);
        }
        else if(_state == OPENEVSE_STATE_CONNECTED){
            startTimer(sleep_timer_connected);
        }
        lastState = _state;
    }

    if(waitingForTag > 0){
        waitingForTag = (stopWaiting - millis()) / 1000;
        String msg = "tag... ";
        msg.concat(waitingForTag);
        lcd.display("Waiting for RFID", 0, 0, 0, LCD_CLEAR_LINE);
        lcd.display(msg, 0, 1, 1000, LCD_CLEAR_LINE);
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

uint8_t RfidTask::getStatus(){
    return status;
}

void RfidTask::waitForTag(uint8_t seconds){
    if(!config_rfid_enabled())
        return;
    waitingForTag = seconds;
    stopWaiting = millis() + seconds * 1000;
    cardFound = false;
    lcd.display("Waiting for RFID", 0, 0, seconds * 1000, LCD_CLEAR_LINE);
}

DynamicJsonDocument RfidTask::rfidPoll() {
    const size_t capacity = JSON_ARRAY_SIZE(3);
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
