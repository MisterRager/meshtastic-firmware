#pragma once

#include "configuration.h"

#if !HAS_SCREEN
#include "power.h"
namespace graphics
{
// Noop class for boards without screen.
class Screen
{
  public:
    explicit Screen(char);
    void onPress();
    void setup();
    void setOn(bool);
    void print(const char *);
    void adjustBrightness();
    void doDeepSleep();
    void forceDisplay();
    void startBluetoothPinScreen(uint32_t pin);
    void stopBluetoothPinScreen();
    void startRebootScreen();
    void startFirmwareUpdateScreen();
};
} // namespace graphics

#else
#include <cstring>
#include <memory>

#include <OLEDDisplayUi.h>

#include "../configuration.h"

#ifdef USE_ST7567
#include <ST7567Wire.h>
#elif defined(USE_SH1106) || defined(USE_SH1107)
#include <SH1106Wire.h>
#elif defined(USE_SSD1306)
#include <SSD1306Wire.h>
#else
// the SH1106/SSD1306 variant is auto-detected
#include <AutoOLEDWire.h>
#endif

#include "EInkDisplay2.h"
#include "TFTDisplay.h"
#include "TypedQueue.h"
#include "commands.h"
#include "concurrency/LockGuard.h"
#include "concurrency/OSThread.h"
#include "mesh/MeshModule.h"
#include "power.h"
#include <string>

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

namespace graphics
{

// Forward declarations
class Screen;

/// Handles gathering and displaying debug information.
class DebugInfo
{
  public:
    DebugInfo(const DebugInfo &) = delete;
    DebugInfo &operator=(const DebugInfo &) = delete;

  private:
    friend Screen;

    DebugInfo() {}

    /// Renders the debug screen.
    void drawFrame(OLEDDisplay *display, OLEDDisplayUiState *state, int16_t x, int16_t y);
    void drawFrameSettings(OLEDDisplay *display, OLEDDisplayUiState *state, int16_t x, int16_t y);
    void drawFrameWiFi(OLEDDisplay *display, OLEDDisplayUiState *state, int16_t x, int16_t y);

    /// Protects all of internal state.
    concurrency::Lock lock;
};

/**
 * @brief This class deals with showing things on the screen of the device.
 *
 * @details Other than setup(), this class is thread-safe as long as drawFrame is not called
 *          multiple times simultaneously. All state-changing calls are queued and executed
 *          when the main loop calls us.
 */
class Screen : public concurrency::OSThread
{
    CallbackObserver<Screen, const meshtastic::Status *> powerStatusObserver =
        CallbackObserver<Screen, const meshtastic::Status *>(this, &Screen::handleStatusUpdate);
    CallbackObserver<Screen, const meshtastic::Status *> gpsStatusObserver =
        CallbackObserver<Screen, const meshtastic::Status *>(this, &Screen::handleStatusUpdate);
    CallbackObserver<Screen, const meshtastic::Status *> nodeStatusObserver =
        CallbackObserver<Screen, const meshtastic::Status *>(this, &Screen::handleStatusUpdate);
    CallbackObserver<Screen, const meshtastic_MeshPacket *> textMessageObserver =
        CallbackObserver<Screen, const meshtastic_MeshPacket *>(this, &Screen::handleTextMessage);
    CallbackObserver<Screen, const UIFrameEvent *> uiFrameEventObserver =
        CallbackObserver<Screen, const UIFrameEvent *>(this, &Screen::handleUIFrameEvent);

  public:
    explicit Screen(std::unique_ptr<OLEDDisplay>);

    Screen(const Screen &) = delete;
    Screen &operator=(const Screen &) = delete;

    /// Initializes the UI, turns on the display, starts showing boot screen.
    //
    // Not thread safe - must be called before any other methods are called.
    void setup();

    /// Turns the screen on/off.
    void setOn(bool on);

    /**
     * Prepare the display for the unit going to the lowest power mode possible.  Most screens will just
     * poweroff, but eink screens will show a "I'm sleeping" graphic, possibly with a QR code
     */
    void doDeepSleep();

    void blink();

    /// Handles a button press.
    void onPress();

    // Implementation to Adjust Brightness
    void adjustBrightness();
    uint8_t brightness = BRIGHTNESS_DEFAULT;

    /// Starts showing the Bluetooth PIN screen.
    //
    // Switches over to a static frame showing the Bluetooth pairing screen
    // with the PIN.
    void startBluetoothPinScreen(uint32_t pin);

    void startFirmwareUpdateScreen();

    void startShutdownScreen();

    void startRebootScreen();

    /// Stops showing the bluetooth PIN screen.
    void stopBluetoothPinScreen();

    /// Stops showing the boot screen.
    void stopBootScreen();

    /// Writes a string to the screen.
    void print(const char *text);

    /// Overrides the default utf8 character conversion, to replace empty space with question marks
    static char customFontTableLookup(const uint8_t ch);

    /// Returns a handle to the DebugInfo screen.
    //
    // Use this handle to set things like battery status, user count, GPS status, etc.
    DebugInfo * debug_info();

    int handleStatusUpdate(const meshtastic::Status *arg);
    int handleTextMessage(const meshtastic_MeshPacket *arg);
    int handleUIFrameEvent(const UIFrameEvent *arg);

    /// Used to force (super slow) eink displays to draw critical frames
    void forceDisplay();

    /// Draws our SSL cert screen during boot (called from WebServer)
    void setSSLFrames();

    void setWelcomeFrames();

  protected:
    /// Updates the UI.
    //
    // Called periodically from the main loop.
    int32_t runOnce() final;

  private:
    struct ScreenCmd {
        Cmd cmd;
        union {
            uint32_t bluetooth_pin;
            char *print_text;
        };
    };

    /// Enques given command item to be processed by main loop().
    bool enqueueCmd(const ScreenCmd &cmd);

    // Implementations of various commands, called from doTask().
    void handleSetOn(bool on);
    void handleOnPress();
    void handleStartBluetoothPinScreen(uint32_t pin);
    void handlePrint(const char *text);
    void handleStartFirmwareUpdateScreen();
    void handleShutdownScreen();
    void handleRebootScreen();
    /// Rebuilds our list of frames (screens) to default ones.
    void setFrames();

    /// Try to start drawing ASAP
    void setFastFramerate();

    /// Called when debug screen is to be drawn, calls through to debugInfo.drawFrame.
    static void drawDebugInfoTrampoline(OLEDDisplay *display, OLEDDisplayUiState *state, int16_t x, int16_t y);

    static void drawDebugInfoSettingsTrampoline(OLEDDisplay *display, OLEDDisplayUiState *state, int16_t x, int16_t y);

    static void drawDebugInfoWiFiTrampoline(OLEDDisplay *display, OLEDDisplayUiState *state, int16_t x, int16_t y);

    /// Queue of commands to execute in doTask.
    TypedQueue<ScreenCmd> cmdQueue;
    /// Whether we are using a display
    bool useDisplay = false;
    /// Whether the display is currently powered
    bool screenOn = false;
    // Whether we are showing the regular screen (as opposed to booth screen or
    // Bluetooth PIN screen)
    bool showingNormalScreen = false;

    /// Holds state for debug information
    DebugInfo debugInfo;

    /// Display device

    std::unique_ptr<OLEDDisplay> dispdev;

    /// UI helper for rendering to frames and switching between them
    OLEDDisplayUi ui;
};

} // namespace graphics
#endif
