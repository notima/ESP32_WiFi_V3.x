#include <Arduino.h>
#include <MicroTasks.h>

#include <functional>

class LoadBalancer : public MicroTasks::Task {
    private:
        uint8_t status;
        std::function<void(boolean, String)> rapi_callback = [](boolean ok, String result){};
        // At what time will we stop waiting for response from other station?
        unsigned long timeout = 0;
        // At what time will the station start?
        unsigned long startTime = 0;
        // At what time is it safe to increase max current
        unsigned long safeTime = 0;
        // Current max current
        uint8_t current;
        String lastCommand = "";

    protected:
        void setup();
        unsigned long loop(MicroTasks::WakeReason reason);

        void sendCommand(String topic, String command, std::function<void(boolean, String)> callback);
        void setCurrent(int current, std::function<void()> onSuccess);
        void updateCurrent();
        void safetyCheck(std::function<void(boolean)> onSafe);

    public:
        LoadBalancer();
        void begin();
        void wakeup();
        void reportRapiResult(String device, String result);
};

extern LoadBalancer loadBalancer;