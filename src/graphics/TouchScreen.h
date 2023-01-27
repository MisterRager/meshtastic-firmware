#ifndef MESHTASTIC_GRAPHICS_TOUCHSCREEN
#define MESHTASTIC_GRAPHICS_TOUCHSCREEN

#include "Screen.h"
#include "TypedQueue.h"

namespace graphics {

enum InputType {
    PRESS_MAIN,
    SETUP,
    SET_ON,
    SET_OFF,
    ADJUST_BRIGHTNESS,
    DEEP_SLEEP,
};

enum NavigationType {
    BLUETOOTH_PIN,
    SHUTDOWN,
    FIRMWARE_UPDATE,
};

typedef struct InputEvent {
    InputType type;
} InputEvent;

typedef struct NavigationEvent {
    NavigationType type;
    bool isEnter;
} NavigationEvent;

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

    TypedQueue<NavigationEvent> navigationQueue;
    TypedQueue<InputEvent> inputQueue;
};

};

// MESHTASTIC_GRAPHICS_TOUCHSCREEN
#endif