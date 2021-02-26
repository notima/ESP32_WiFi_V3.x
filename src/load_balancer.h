#include <Arduino.h>
#include <MicroTasks.h>

#include <functional>

class LoadBalancer : public MicroTasks::Task {
    private:
        uint8_t status;
        std::function<void(boolean, String)> rapi_callback = [](boolean ok, String result){};
        unsigned long wakeupStarted = 0;
        unsigned long startTime = 0;
        String lastCommand = "";

    protected:
        void setup();
        unsigned long loop(MicroTasks::WakeReason reason);

        void sendCommand(String topic, String command, std::function<void(boolean, String)> callback);
        void setCurrent(int current, std::function<void()> onSuccess);
        void safetyCheck(std::function<void(boolean)> onSafe);

    public:
        LoadBalancer();
        void begin();
        void wakeup();
        void reportRapiResult(String device, String result);
};

extern LoadBalancer loadBalancer;