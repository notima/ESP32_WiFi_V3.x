#include <lcd_manager.h>
#include <debug.h>
#include <input.h>

#define LCD_INFO_PAGES 4
#define TIME_PER_PAGE 2
#define LCD_MAX_LEN 16

LcdManager::InfoPage::InfoPage(std::function<void(Lcd *lcd)> onUpdate, uint16_t updateInterval) {
    this->onUpdate = onUpdate;
    this->updateInterval = min(updateInterval, (uint16_t)(TIME_PER_PAGE * 1000));
}

LcdManager::Lcd::Lcd(){

}

void LcdManager::Lcd::display(String message, uint8_t x, uint8_t y, boolean clearLine) {
    if(!claimed){
        rapiSender.sendCmd(F("$F0 0"));
        claimed = true;
    }
    String cmd = F("$FP ");
    cmd += x;
    cmd += " ";
    cmd += y;
    cmd += " ";
    cmd += message;
    rapiSender.sendCmd(cmd);
    if(clearLine){
        for(int i = x + strlen(message.c_str()); i < LCD_MAX_LEN; i += 6) {
            // Older versions of the firmware crash if sending more than 6 spaces so clear the rest
            // of the line using blocks of 6 spaces
            cmd = F("$FP ");
            cmd += i;
            cmd += " ";
            cmd += y;
            cmd += "       "; // 7 spaces 1 separator and 6 to display
            rapiSender.sendCmd(cmd);
        }
    }
}

void LcdManager::Lcd::release() {
    rapiSender.sendCmd(F("$F0 1"));
    claimed = false;
}

LcdManager::LcdManager() : MicroTasks::Task(),
evsePage(InfoPage([](Lcd *lcd){lcd->release();}, TIME_PER_PAGE * 1000)),
lcd(new Lcd()) {
}

void LcdManager::begin() {
    MicroTask.startTask(this);
}

void LcdManager::setup() {

}

unsigned long LcdManager::loop(MicroTasks::WakeReason reason) {
    uint8_t currentPage = millis() / TIME_PER_PAGE / 1000 % LCD_INFO_PAGES;
    pages[currentPage].onUpdate(lcd);
    DEBUG.printf("Page %d\n", currentPage);
    return max(pages[currentPage].updateInterval, (uint16_t)100);
}

void LcdManager::addInfoPage(uint8_t index, std::function<void(Lcd *lcd)> onScreenUpdate, uint16_t updateInterval) {
    InfoPage page(onScreenUpdate, updateInterval);
    pages[index] = page;
}

void LcdManager::claim(std::function<void(Lcd *lcd)> onScreenUpdate, uint16_t  updateInterval) {

}

void LcdManager::release() {

}

void LcdManager::clearPages(){
    for(int i = 0; i < LCD_INFO_PAGES; i++){
        pages[i] = evsePage;
    }
}

LcdManager lcdManager;