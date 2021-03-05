#include <Arduino.h>
#include <MicroTasks.h>
#include <functional>
#include <openevse.h>

#define LCD_COLOR_OFF 0
#define LCD_COLOR_RED 1
#define LCD_COLOR_GREEN 2
#define LCD_COLOR_YELLOW 3
#define LCD_COLOR_BLUE 4
#define LCD_COLOR_VIOLET 5
#define LCD_COLOR_TEAL 6
#define LCD_COLOR_WHITE 7

class LcdManager : MicroTasks::Task{
    public:
        class Lcd;
        class Message{
            public:
            std::function<void(Lcd *lcd)> onUpdate;
            uint16_t updateInterval;
            int time;
            Message(std::function<void(Lcd *lcd)> onUpdate, uint16_t updateInterval, int time = 0);
            Message();
        };
    private:
        Message evsePage;
        Message pages[4] = {evsePage, evsePage, evsePage, evsePage};
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
        void queue(std::function<void(Lcd *lcd)> onScreenUpdate, int time, int updateInterval = 1000);
        void clearQueue();
        void enableFlipBook(boolean enabled);
        void setColor(uint8_t color);
};

extern LcdManager lcdManager;