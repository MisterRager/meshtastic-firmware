#ifndef MESHTASTIC_GRAPHICS_TOUCHSCREEN
#define MESHTASTIC_GRAPHICS_TOUCHSCREEN

#include "Screen.h"
#include "TypedQueue.h"

namespace graphics {


class TouchScreen : public Screen
{
public:

    TouchScreen();

    void onPress() override;
    void setup() override;
    void setOn(bool) override;
    void print(const char *) override;
    void adjustBrightness() override;
    void doDeepSleep() override;
    void forceDisplay() override;
    void startBluetoothPinScreen(uint32_t pin) override;
    void stopBluetoothPinScreen() override;
    void startRebootScreen() override;
    void startShutdownScreen() override;
    void startFirmwareUpdateScreen() override;
    void blink() override;
    void setSSLFrames() override;

private:
    enum ScreenEventType {
        BLINK,
        DEEP_SLEEP,
        SET_ON,
        SET_OFF,
        FLUSH,
    };

    enum InputType {
        PRESS_MAIN,
        ADJUST_BRIGHTNESS,
    };

    enum NavigationType {
        BLUETOOTH_PIN,
        SHUTDOWN,
        FIRMWARE_UPDATE,
        REBOOT,
        SSL_FRAME,
    };

    union NavigationData {
        uint32_t bluetoothPin;
    };

    typedef struct NavigationEvent {
        NavigationType type;
        bool isEnter;
        NavigationData data;
    } NavigationEvent;

    TypedQueue<NavigationEvent> navigationQueue;
    TypedQueue<InputType> inputQueue;
    TypedQueue<ScreenEventType> screenQueue;
};

};

// MESHTASTIC_GRAPHICS_TOUCHSCREEN
#endif