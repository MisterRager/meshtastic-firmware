#include "TouchScreen.h"

namespace graphics {
    TouchScreen::TouchScreen() :
        Screen('T')
    {}

    void TouchScreen::onPress() {}
    void TouchScreen::setup() {}
    void TouchScreen::setOn(bool _) {}
    void TouchScreen::print(const char *_) {}
    void TouchScreen::adjustBrightness() {}
    void TouchScreen::doDeepSleep() {}
    void TouchScreen::forceDisplay() {}
    void TouchScreen::startBluetoothPinScreen(uint32_t pin) {}
    void TouchScreen::stopBluetoothPinScreen() {}
    void TouchScreen::startRebootScreen() {}
    void TouchScreen::startShutdownScreen() {}
    void TouchScreen::startFirmwareUpdateScreen() {}
    void TouchScreen::blink() {}
    void TouchScreen::setSSLFrames() {}
}