#include <Arduino.h>
#include <MicroTasks.h>
#include <functional>
#include <openevse.h>

class LcdManager : MicroTasks::Task{
    public:
        class Lcd;
    private:
        class InfoPage{
            public:
            std::function<void(Lcd *lcd)> onUpdate;
            uint16_t updateInterval;
            InfoPage(std::function<void(Lcd *lcd)> onUpdate, uint16_t updateInterval);
        };
        InfoPage evsePage;
        InfoPage pages[4] = {evsePage, evsePage, evsePage, evsePage};
        Lcd *lcd;
        boolean flipBookEnabled = true;
        int updateInterval = 1000;
        std::function<void(Lcd *lcd)> onScreenUpdate = [](Lcd *lcd){};
        unsigned long releaseTime = ULONG_MAX;

    protected:
        void setup();
        unsigned long loop(MicroTasks::WakeReason reason);

    public:
        class Lcd{
            private:
                bool claimed;
            public:
                Lcd();
                void display(String message, uint8_t x, uint8_t y, boolean clearLine = true);
                void release();
        };
        LcdManager();
        void begin();
        void addInfoPage(uint8_t index, std::function<void(Lcd *lcd)> onScreenUpdate, uint16_t  updateInterval = 1000);
        void claim(std::function<void(Lcd *lcd)> onScreenUpdate, int updateInterval = 1000);
        void release();
        void display(String message, uint8_t x, uint8_t y, int time, boolean clearLine = true);
        void clearPages();
        void enableFlipBook(boolean enabled);
};

extern LcdManager lcdManager;