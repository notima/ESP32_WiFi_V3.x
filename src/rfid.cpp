#define STATUS_NOT_ENABLED 0
#define STATUS_NOT_FOUND 1
#define STATUS_ACTIVE 2

#include "rfid.h"

#include "debug.h"
#include "mqtt.h"
#include "lcd.h"
#include "app_config.h"
#include "RapiSender.h"
#include "input.h"
#include "openevse.h"

DFRobot_PN532_IIC  nfc(PN532_IRQ, PN532_POLLING); 
struct card NFCcard;
extern RapiSender rapiSender;

uint8_t status = STATUS_NOT_ENABLED;
boolean hasContact = false;

unsigned long nextScan = 0;
unsigned long nextSleepTimerTick = 0;

uint8_t sleep_timer = 60;
static void sleep_timer_update();

// How many more seconds to wait for tag
uint8_t waitingForTag = 0;
unsigned long stopWaiting = 0;
boolean cardFound = false;



void rfid_setup(){
    if(!config_rfid_enabled()){
        DEBUG.println("RFID disabled");
        status = STATUS_NOT_ENABLED;
        return;
    }
    DEBUG.println("RFID enabled");

    lcd_display("RFID status:", 0, 0, 0, LCD_CLEAR_LINE);

    if(nfc.begin()){
        DEBUG.println("RFID module initialized");
        lcd_display("connected", 0, 1, 3000, LCD_CLEAR_LINE);
        status = STATUS_ACTIVE;
    }else{
        DEBUG.println("RFID module not found");
        lcd_display("not found", 0, 1, 3000, LCD_CLEAR_LINE);
        config_save_rfid(false, rfid_storage);
        status = STATUS_NOT_FOUND;
    }
}

String getUidHex(card NFCcard){
    String uidHex = "";
    for(int i = 0; i < NFCcard.uidlenght; i++){
        char hex[NFCcard.uidlenght * 3];
        sprintf(hex,"%x",NFCcard.uid[i]);
        uidHex = uidHex + hex + " ";
    }
    uidHex.trim();
    return uidHex;
}

String getUidBytes(card NFCcard){
    String bytes = "";
    bytes.concat((char)NFCcard.uidlenght);
    for(int i = 0; i < NFCcard.uidlenght; i+= 1){
        bytes.concat((char)NFCcard.uid[i]);
    }

    Serial.println();
    Serial.println(bytes);
    Serial.println();

    const char* ptr = bytes.c_str();
    for(int i = 0; i < 4; i++){
        Serial.print((int)ptr[i], HEX);
    }

    return bytes;
}

void scanCard(){
    NFCcard = nfc.getInformation();
    String uidHex = getUidHex(NFCcard);

    if(waitingForTag > 0){
        waitingForTag = 0;
        cardFound = true;
        lcd_display("Tag detected!", 0, 0, 0, LCD_CLEAR_LINE);
        lcd_display(uidHex, 0, 1, 3000, LCD_CLEAR_LINE);
    }else{
        // Check if tag is stored locally
        char storedTags[rfid_storage.length() + 1];
        rfid_storage.toCharArray(storedTags, rfid_storage.length()+1);
        char* storedTag = strtok(storedTags, ",");
        while(storedTag)
        {
            String storedTagStr = storedTag;
            storedTagStr.trim();
            uidHex.trim();
            if(storedTagStr.compareTo(uidHex) == 0){
                rapiSender.sendCmd(F("$FE"));
                break;
            }
            storedTag = strtok(NULL, ",");
        }

        // Send to MQTT broker
        DynamicJsonDocument data(4096);
        data["rfid"] = uidHex;
        mqtt_publish(data);
    }
}

void rfid_loop(){
    if(!config_rfid_enabled()){
        if(status != STATUS_NOT_FOUND)
            status = STATUS_NOT_ENABLED;
        return;
    }

    if(millis() > nextSleepTimerTick && waitingForTag == 0){
        sleep_timer_update();
        nextSleepTimerTick = millis() + 2000;
    }

    if(millis() < nextScan){
        return;
    }

    if(status != STATUS_ACTIVE){
        rfid_setup();
        return;
    }

    if(waitingForTag > 0){
        waitingForTag = (stopWaiting - millis()) / 1000;
        String msg = "tag... ";
        msg.concat(waitingForTag);
        lcd_display("Waiting for RFID", 0, 0, 0, LCD_CLEAR_LINE);
        lcd_display(msg, 0, 1, 1000, LCD_CLEAR_LINE);
    }

    nextScan = millis() + SCAN_FREQ;

    boolean foundCard = nfc.scan();

    if(foundCard && !hasContact){
        scanCard();
        hasContact = true;
        return;
    }

    if(!foundCard && hasContact){
        hasContact = false;
    }
}

uint8_t rfid_status(){
    return status;
}

void rfid_wait_for_tag(uint8_t seconds){
    if(!config_rfid_enabled())
        return;
    waitingForTag = seconds;
    stopWaiting = millis() + seconds * 1000;
    cardFound = false;
    lcd_display("Waiting for RFID", 0, 0, seconds * 1000, LCD_CLEAR_LINE);
}

DynamicJsonDocument rfid_poll() {
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
        doc["scanned"] = getUidHex(NFCcard);
        cardFound = false;
    }
    else  {
        doc["status"] = "notStarted";
    }
    return doc;
}

unsigned long goToSleep = 0;
void sleep_timer_update(){
    if(state == OPENEVSE_STATE_NOT_CONNECTED){
        if(goToSleep == 0){
            goToSleep = millis() + sleep_timer * 1000;
        }
        else if(millis() > goToSleep){
            rapiSender.sendCmd(F("$FS"));
            goToSleep = 0;
        }
    } else {
        goToSleep = 0;
    }

    uint8_t messageToDisplay = millis() / 2000 % 4;
    if(messageToDisplay == 0 && state == OPENEVSE_STATE_NOT_CONNECTED){
        lcd_display("Connect your", 0, 0, 0, LCD_CLEAR_LINE);
        lcd_display("vehicle", 0, 1, 2000, LCD_CLEAR_LINE);
    }else if(messageToDisplay == 1 && state == OPENEVSE_STATE_NOT_CONNECTED){
        lcd_display("Going to", 0, 0, 0, LCD_CLEAR_LINE);
        String timerMsg = "sleep in: ";
        timerMsg.concat((goToSleep - millis()) / 1000);
        timerMsg.concat("s");
        lcd_display(timerMsg, 0, 1, 2000, LCD_CLEAR_LINE);
    }else if(messageToDisplay == 0 && OPENEVSE_STATE_SLEEPING){
        lcd_display("Scan RFID tag", 0, 0, 0, LCD_CLEAR_LINE);
        lcd_display("to start", 0, 1, 4000, LCD_CLEAR_LINE);
    }
}