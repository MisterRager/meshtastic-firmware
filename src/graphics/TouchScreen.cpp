#include "TouchScreen.h"

namespace graphics {
    TouchScreen::TouchScreen() :
        Screen('T'),
        navigationQueue(TypedQueue<NavigationEvent>(10)),
        inputQueue(TypedQueue<InputType>(10)),
        screenQueue(TypedQueue<ScreenEventType>(10))
    {
    }

    void TouchScreen::onPress() {
        inputQueue.enqueue(InputType::PRESS_MAIN, 100);
    }

    void TouchScreen::setup() {}

    void TouchScreen::setOn(bool on) {
        screenQueue.enqueue(on ? ScreenEventType::SET_ON : ScreenEventType::SET_OFF, portMAX_DELAY);
    }

    void TouchScreen::print(const char *_) {}

    void TouchScreen::adjustBrightness() {
        inputQueue.enqueue(InputType::ADJUST_BRIGHTNESS, 100);
    }

    void TouchScreen::doDeepSleep() {
        screenQueue.enqueue(ScreenEventType::DEEP_SLEEP, portMAX_DELAY);
    }

    void TouchScreen::forceDisplay() {
        screenQueue.enqueue(ScreenEventType::FLUSH, portMAX_DELAY);
    }

    void TouchScreen::startBluetoothPinScreen(uint32_t pin) {
        NavigationEvent event;
        event.data.bluetoothPin = pin;
        event.type = NavigationType::BLUETOOTH_PIN;
        event.isEnter = true;
        navigationQueue.enqueue(event, portMAX_DELAY);
    }

    void TouchScreen::stopBluetoothPinScreen() {
        NavigationEvent event;
        event.type = NavigationType::BLUETOOTH_PIN;
        event.isEnter = false;
        navigationQueue.enqueue(event, portMAX_DELAY);
    }

    void TouchScreen::startRebootScreen() {
        NavigationEvent event;
        event.type = NavigationType::REBOOT;
        event.isEnter = true;
        navigationQueue.enqueue(event, portMAX_DELAY);
    }

    void TouchScreen::startShutdownScreen() {
        NavigationEvent event;
        event.type = NavigationType::SHUTDOWN;
        event.isEnter = true;
        navigationQueue.enqueue(event, portMAX_DELAY);
    }

    void TouchScreen::startFirmwareUpdateScreen() {
        NavigationEvent event;
        event.type = NavigationType::FIRMWARE_UPDATE;
        event.isEnter = true;
        navigationQueue.enqueue(event, portMAX_DELAY);
    }

    void TouchScreen::blink() {
        screenQueue.enqueue(ScreenEventType::BLINK, 300);
    }

    void TouchScreen::setSSLFrames() {
        NavigationEvent event;
        event.type = NavigationType::SSL_FRAME;
        event.isEnter = true;
        navigationQueue.enqueue(event, portMAX_DELAY);
    }
}