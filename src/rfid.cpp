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
    lcd_display("Tag detected!", 0, 0, 0, LCD_CLEAR_LINE);

    String uidHex = getUidHex(NFCcard);

    DynamicJsonDocument data(4096);
    data["rfid"] = uidHex;
    mqtt_publish(data);

    lcd_display(uidHex, 0, 1, 3000, LCD_CLEAR_LINE);
    rapiSender.sendCmd(F("$F1"));
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
}

uint8_t rfid_status(){
    return status;
}