#ifndef MESHTASTIC_GRAPHICS_SCREEN
#define MESHTASTIC_GRAPHICS_SCREEN

#include "OLEDDisplayUi.h"
#include "MeshModule.h"

// 0 to 255, though particular variants might define different defaults
#ifndef BRIGHTNESS_DEFAULT
#define BRIGHTNESS_DEFAULT 150
#endif

// Meters to feet conversion
#ifndef METERS_TO_FEET
#define METERS_TO_FEET 3.28
#endif

// Feet to miles conversion
#ifndef MILES_TO_FEET
#define MILES_TO_FEET 5280
#endif

#if defined(USE_EINK) || defined(ILI9341_DRIVER) || defined(ST7735_CS)
// The screen is bigger so use bigger fonts
#define FONT_SMALL ArialMT_Plain_16  // Height: 19
#define FONT_MEDIUM ArialMT_Plain_24 // Height: 28
#define FONT_LARGE ArialMT_Plain_24  // Height: 28
#else
#ifdef OLED_RU
#define FONT_SMALL ArialMT_Plain_10_RU
#else
#define FONT_SMALL ArialMT_Plain_10 // Height: 13
#endif
#define FONT_MEDIUM ArialMT_Plain_16 // Height: 19
#define FONT_LARGE ArialMT_Plain_24  // Height: 28
#endif

#define fontHeight(font) ((font)[1] + 1) // height is position 1

#define FONT_HEIGHT_SMALL fontHeight(FONT_SMALL)
#define FONT_HEIGHT_MEDIUM fontHeight(FONT_MEDIUM)
#define FONT_HEIGHT_LARGE fontHeight(FONT_LARGE)

namespace graphics
{

// This means the *visible* area (sh1106 can address 132, but shows 128 for example)
#define IDLE_FRAMERATE 1 // in fps

// DEBUG
#define NUM_EXTRA_FRAMES 3 // text message and debug frame
// if defined a pixel will blink to show redraws
// #define SHOW_REDRAWS

// At some point, we're going to ask all of the modules if they would like to display a screen frame
// we'll need to hold onto pointers for the modules that can draw a frame.
extern std::vector<MeshModule *> moduleFrames;

/*
Base class Screen

This was the no-screen "Screen" class definition. The methods are all no-ops, including the constructor.
*/
class Screen
{
    public:
        Screen(char);
        virtual void onPress();
        virtual void setup();
        virtual void setOn(bool);
        virtual void print(const char *);
        virtual void adjustBrightness();
        virtual void doDeepSleep();
        virtual void forceDisplay();
        virtual void startBluetoothPinScreen(uint32_t pin);
        virtual void stopBluetoothPinScreen();
        virtual void startRebootScreen();
        virtual void startFirmwareUpdateScreen();
        virtual void blink();
        virtual void setSSLFrames();
};

} // namespace graphics

// MESHTASTIC_GRAPHICS_SCREEN
#endif 
