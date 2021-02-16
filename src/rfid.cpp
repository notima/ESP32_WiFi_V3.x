#include "rfid.h"

#include "debug.h"
#include "mqtt.h"
#include "lcd.h"
#include "app_config.h"
#include "RapiSender.h"
#include "input.h"
#include "openevse.h"
#include "Wire.h"

#ifndef I2C_SDA
#define I2C_SDA 21
#endif

#ifndef I2C_SCL
#define I2C_SCL 22
#endif

DFRobot_PN532_IIC nfc(PN532_IRQ, PN532_POLLING); 

RfidTask::RfidTask() :
  MicroTasks::Task(),
  _evse(NULL),
  _scheduler(NULL),
  _evseStateEvent(this)
{
}

void RfidTask::begin(EvseManager &evse, Scheduler &scheduler){
    _evse = &evse;
    _scheduler = &scheduler;
    MicroTask.startTask(this);
}

void RfidTask::setup(){
    if(!config_rfid_enabled()){
        status = RFID_STATUS_NOT_ENABLED;
        return;
    }

    Wire.begin(I2C_SDA, I2C_SCL);
    
    if(nfc.begin()){
        status = RFID_STATUS_ACTIVE;
    }else{
        if(status == RFID_STATUS_NOT_FOUND){
            config_save_rfid(false, rfid_storage);
            DEBUG.println("RFID still not responding and has been disabled.");
        }else{
            DEBUG.println("RFID module did not respond!");
            status = RFID_STATUS_NOT_FOUND;
        }
    }
}

void RfidTask::verifyUID(String uid){
    Serial.printf("\n%s\n", uid.c_str());

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
                rapiSender.sendCmd(F("$FE"));
                //schedulePause();
                break;
            }
            storedTag = strtok(NULL, ",");
        }

        // Send to MQTT broker
        DynamicJsonDocument data(4096);
        data["rfid"] = uid;
        mqtt_publish(data);
                rapiSender.sendCmd(F("$FE"));
                schedulePause();
    }
}

unsigned long RfidTask::loop(MicroTasks::WakeReason reason){
    unsigned long nextScan = SCAN_FREQ;

    if(!config_rfid_enabled()){
        if(status != RFID_STATUS_NOT_FOUND)
            status = RFID_STATUS_NOT_ENABLED;
        return MicroTask.WaitForMask;
    }

    if(status != RFID_STATUS_ACTIVE){
        this->setup();
        return nextScan;
    }

    if(waitingForTag > 0){
        waitingForTag = (stopWaiting - millis()) / 1000;
        String msg = "tag... ";
        msg.concat(waitingForTag);
        lcd.display("Waiting for RFID", 0, 0, 0, LCD_CLEAR_LINE);
        lcd.display(msg, 0, 1, 1000, LCD_CLEAR_LINE);
    }

    boolean foundCard = false;
    if((state >= OPENEVSE_STATE_SLEEPING || waitingForTag)){
        nfc.begin();
        foundCard = nfc.scan();
    }

    if(foundCard && !hasContact){
        verifyUID(nfc.readUid());
        hasContact = true;
        return nextScan;
    }

    if(!foundCard && hasContact){
        hasContact = false;
    }

    return nextScan;
}

void RfidTask::schedulePause(){
    //_scheduler->addEvent((uint32_t)100, 11, 27, 00, SCHEDULER_DAY_TUESDAY, EvseState(EvseState::Disabled));
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
