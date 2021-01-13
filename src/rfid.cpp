#include "rfid.h"

#include "debug.h"
#include "mqtt.h"
#include "lcd.h"
#include "RapiSender.h"

DFRobot_PN532_IIC  nfc(PN532_IRQ, PN532_POLLING); 
struct card NFCcard;
extern RapiSender rapiSender;

boolean rfidModuleActive;
boolean hasContact = false;

unsigned long nextScan = 0;

void rfid_setup(){

    lcd_display("RFID status:", 0, 0, 0, LCD_CLEAR_LINE);

    if(nfc.begin()){
        DEBUG.println("RFID module initialized");
        lcd_display("connected", 0, 1, 3000, LCD_CLEAR_LINE);
        rfidModuleActive = true;
    }else{
        DEBUG.println("RFID module not found");
        lcd_display("not found", 0, 1, 3000, LCD_CLEAR_LINE);
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
    //lcd_display("Tag detected!", 0, 0, 0, LCD_CLEAR_LINE);

    String uidHex = getUidHex(NFCcard);

    DynamicJsonDocument data(4096);
    data["rfid"] = uidHex;
    mqtt_publish(data);

    //lcd_display(uidHex, 0, 1, 3000, LCD_CLEAR_LINE);
    //rapiSender.sendCmd(F("$F1"));
}

void rfid_loop(){
    if(!rfidModuleActive || millis() < nextScan)
        return;

    nextScan = millis() + SCAN_FREQ;

    boolean success = nfc.scan();

    if(success && !hasContact){
        scanCard();
        hasContact = true;
        return;
    }

    if(success && hasContact){
        hasContact = false;
    }
}