#define STATUS_NOT_ENABLED 0
#define STATUS_NOT_FOUND 1
#define STATUS_ACTIVE 2

#include "rfid.h"

#include "debug.h"
#include "mqtt.h"
#include "lcd.h"
#include "app_config.h"
#include "RapiSender.h"

DFRobot_PN532_IIC  nfc(PN532_IRQ, PN532_POLLING); 
struct card NFCcard;
extern RapiSender rapiSender;

uint8_t status = STATUS_NOT_ENABLED;
boolean hasContact = false;

unsigned long nextScan = 0;

// How many more seconds to wait for tag
uint8_t waitingForTag = 0;
unsigned long stopWaiting = 0;
String foundUID;

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
        config_save_rfid(false);
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
    return uidHex;
}

void scanCard(){
    NFCcard = nfc.getInformation();
    String uidHex = getUidHex(NFCcard);

    if(waitingForTag > 0){
        waitingForTag = 0;
        foundUID = uidHex;
        lcd_display("Tag detected!", 0, 0, 0, LCD_CLEAR_LINE);
        lcd_display(uidHex, 0, 1, 3000, LCD_CLEAR_LINE);
    }else{
        DynamicJsonDocument data(4096);
        data["rfid"] = uidHex;
        mqtt_publish(data);
        rapiSender.sendCmd(F("$F1"));
    }
}

void rfid_loop(){
    if(millis() < nextScan){
        return;
    }

    if(!config_rfid_enabled()){
        if(status != STATUS_NOT_FOUND)
            status = STATUS_NOT_ENABLED;
        return;
    }

    if(status != STATUS_ACTIVE){
        rfid_setup();
        return;
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

    if(waitingForTag > 0){
        waitingForTag = (stopWaiting - millis()) / 1000;
        String msg = "tag... ";
        msg.concat(waitingForTag);
        lcd_display(msg, 0, 1, 1000, LCD_CLEAR_LINE);
    }
}

uint8_t rfid_status(){
    return status;
}

DynamicJsonDocument rfid_get_stored_tags(){
    // compute the required size
    const size_t capacity = JSON_ARRAY_SIZE(3);

    // allocate the memory for the document
    DynamicJsonDocument doc(capacity);

    // create an empty array
    JsonArray array = doc.to<JsonArray>();

    // add some values
    array.add("hello");
    array.add(42);
    array.add(3.14);

    return doc;
}

void rfid_wait_for_tag(uint8_t seconds){
    if(!config_rfid_enabled())
        return;
    waitingForTag = seconds;
    stopWaiting = millis() + seconds * 1000;
    foundUID = emptyString;
    lcd_display("Waiting for RFID", 0, 0, seconds * 1000, LCD_CLEAR_LINE);
}

DynamicJsonDocument rfid_poll() {
    const size_t capacity = JSON_ARRAY_SIZE(3);
    DynamicJsonDocument doc(capacity);
    if(waitingForTag > 0){
        // respons with remainding time
        doc["status"] = "waiting";
        doc["timeLeft"] = waitingForTag;
    } 
    else if(emptyString.compareTo(foundUID) != 0){
        // respond with the scanned tags uid and reset
        doc["status"] = "done";
        doc["scanned"] = foundUID;
        foundUID = emptyString;
    }
    else  {
        doc["status"] = "notStarted";
    }
    return doc;
}