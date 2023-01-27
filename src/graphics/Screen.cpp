#include <OLEDDisplay.h>

#include "GPS.h"
#include "MeshService.h"
#include "NodeDB.h"
#include "Screen.h"
#include "gps/GeoCoord.h"
#include "gps/RTC.h"
#include "graphics/images.h"
#include "main.h"
#include "mesh-pb-constants.h"
#include "mesh/Channels.h"
#include "mesh/generated/meshtastic/deviceonly.pb.h"
#include "modules/ExternalNotificationModule.h"
#include "modules/TextMessageModule.h"
#include "sleep.h"
#include "target_specific.h"
#include "utils.h"

#ifdef ARCH_ESP32
#include "esp_task_wdt.h"
#include "mesh/http/WiFiAPClient.h"
#include "modules/esp32/StoreForwardModule.h"
#endif

#ifdef OLED_RU
#include "fonts/OLEDDisplayFontsRU.h"
#endif

using namespace meshtastic; /** @todo remove */

namespace graphics
{

uint32_t targetFramerate = IDLE_FRAMERATE;
char btPIN[16] = "888888";

uint32_t logo_timeout = 5000; // 4 seconds for EACH logo

// At some point, we're going to ask all of the modules if they would like to display a screen frame
// we'll need to hold onto pointers for the modules that can draw a frame.
std::vector<MeshModule *> moduleFrames;

// Stores the last 4 of our hardware ID, to make finding the device for pairing easier
char ourId[5];

#ifdef SHOW_REDRAWS
bool heartbeat = true;
#endif


Screen::Screen(char _) { }
void Screen::onPress() {}
void Screen::setup() {}
void Screen::setOn(bool _) {}
void Screen::print(const char * _) {}
void Screen::adjustBrightness() {}
void Screen::doDeepSleep() {}
void Screen::forceDisplay() {}
void Screen::startBluetoothPinScreen(uint32_t pin) {}
void Screen::stopBluetoothPinScreen() {}
void Screen::startRebootScreen() {}
void Screen::startFirmwareUpdateScreen() {}
void Screen::blink() {}
void Screen::setSSLFrames() {}

} // namespace graphics
