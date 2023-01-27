#ifndef MESHTASTIC_GRAPHICS_ACTIVESCREEN
#define MESHTASTIC_GRAPHICS_ACTIVESCREEN

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

#include "Observer.h"
#include <string>

#include "Screen.h"
#include "DebugInfo.h"

#include "EInkDisplay2.h"
#include "TFTDisplay.h"
#include "TypedQueue.h"

#include "commands.h"

#include "concurrency/LockGuard.h"
#include "concurrency/OSThread.h"
#include "mesh/MeshModule.h"
#include "power.h"

namespace graphics {

/**
 * @brief This class deals with showing things on the screen of the device.
 *
 * @details Other than setup(), this class is thread-safe as long as drawFrame is not called
 *          multiple times simultaneously. All state-changing calls are queued and executed
 *          when the main loop calls us.
 */
class ActiveScreen : public graphics::Screen, public concurrency::OSThread
{
    CallbackObserver<ActiveScreen, const meshtastic::Status *> powerStatusObserver =
        CallbackObserver<ActiveScreen, const meshtastic::Status *>(this, &ActiveScreen::handleStatusUpdate);
    CallbackObserver<ActiveScreen, const meshtastic::Status *> gpsStatusObserver =
        CallbackObserver<ActiveScreen, const meshtastic::Status *>(this, &ActiveScreen::handleStatusUpdate);
    CallbackObserver<ActiveScreen, const meshtastic::Status *> nodeStatusObserver =
        CallbackObserver<ActiveScreen, const meshtastic::Status *>(this, &ActiveScreen::handleStatusUpdate);
    CallbackObserver<ActiveScreen, const meshtastic_MeshPacket *> textMessageObserver =
        CallbackObserver<ActiveScreen, const meshtastic_MeshPacket *>(this, &ActiveScreen::handleTextMessage);
    CallbackObserver<ActiveScreen, const UIFrameEvent *> uiFrameEventObserver =
        CallbackObserver<ActiveScreen, const UIFrameEvent *>(this, &ActiveScreen::handleUIFrameEvent);

  public:
    explicit ActiveScreen(std::unique_ptr<OLEDDisplay>);

    ActiveScreen(const ActiveScreen &) = delete;
    ActiveScreen &operator=(const ActiveScreen &) = delete;

    /// Initializes the UI, turns on the display, starts showing boot screen.
    //
    // Not thread safe - must be called before any other methods are called.
    void setup() override;

    /// Turns the screen on/off.
    void setOn(bool on) override;

    /**
     * Prepare the display for the unit going to the lowest power mode possible.  Most screens will just
     * poweroff, but eink screens will show a "I'm sleeping" graphic, possibly with a QR code
     */
    void doDeepSleep() override;

    void blink() override;

    /// Handles a button press.
    void onPress() override;

    // Implementation to Adjust Brightness
    void adjustBrightness() override;
    uint8_t brightness = BRIGHTNESS_DEFAULT;

    /// Starts showing the Bluetooth PIN screen.
    //
    // Switches over to a static frame showing the Bluetooth pairing screen
    // with the PIN.
    void startBluetoothPinScreen(uint32_t pin) override;

    void startFirmwareUpdateScreen() override;

    void startShutdownScreen();

    void startRebootScreen() override;

    /// Stops showing the bluetooth PIN screen.
    void stopBluetoothPinScreen() override;

    /// Stops showing the boot screen.
    void stopBootScreen();

    /// Writes a string to the screen.
    void print(const char *text) override;

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
    void setSSLFrames() override;

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

    uint16_t displayWidth, displayHeight;
    int8_t prevFrame = -1;
    size_t nodeIndex;

    int getStringCenteredX(const char *);

    friend void drawIconScreen(const char *, OLEDDisplay *, OLEDDisplayUiState *, int16_t, int16_t);
    friend void drawOEMIconScreen(const char *, OLEDDisplay *, OLEDDisplayUiState *, int16_t, int16_t);
    friend void drawColumns(ActiveScreen *, int16_t, int16_t, const char **);
    friend void drawNodeInfo(OLEDDisplay *, OLEDDisplayUiState *, int16_t, int16_t);

};

}
#endif