#include "ActiveScreen.h"
#include "OLEDDisplay.h"
#include "OLEDDisplayUi.h"
#include "images.h"
#include "GeoCoord.h"
#include "modules/TextMessageModule.h"
#include "utils.h"

#include <cstring>

#include "main.h"

// myRegion
#include "MeshRadio.h"

#include "target_specific.h"
#ifdef ARCH_ESP32
#include "esp_task_wdt.h"
#include "mesh/http/WiFiAPClient.h"
#include "modules/esp32/StoreForwardModule.h"
#include "modules/ExternalNotificationModule.h"
#endif

namespace graphics {

FrameCallback normalFrames[MAX_NUM_NODES + NUM_EXTRA_FRAMES];


/**
 * Draw the icon with extra info printed around the corners
 */
void drawIconScreen(const char *upperMsg, OLEDDisplay *display, OLEDDisplayUiState *state, int16_t x, int16_t y)
{
    // draw an xbm image.
    // Please note that everything that should be transitioned
    // needs to be drawn relative to x and y
    ActiveScreen * screen = reinterpret_cast<ActiveScreen *>(state->userData);

    // draw centered icon left to right and centered above the one line of app text
    display->drawXbm(x + (screen->displayWidth - icon_width) / 2, y + (screen->displayHeight - FONT_HEIGHT_MEDIUM - icon_height) / 2 + 2,
                     icon_width, icon_height, (const uint8_t *)icon_bits);

    display->setFont(FONT_MEDIUM);
    display->setTextAlignment(TEXT_ALIGN_LEFT);
    const char *title = "meshtastic.org";
    display->drawString(x + screen->getStringCenteredX(title), y + screen->displayHeight - FONT_HEIGHT_MEDIUM, title);
    display->setFont(FONT_SMALL);

    // Draw region in upper left
    if (upperMsg)
        display->drawString(x + 0, y + 0, upperMsg);

    // Draw version in upper right
    char buf[16];
    snprintf(buf, sizeof(buf), "%s",
             xstr(APP_VERSION_SHORT)); // Note: we don't bother printing region or now, it makes the string too long
    display->drawString(x + screen->displayWidth - display->getStringWidth(buf), y + 0, buf);
    screen->forceDisplay();

    // FIXME - draw serial # somewhere?
}

static void drawBootScreen(OLEDDisplay *display, OLEDDisplayUiState *state, int16_t x, int16_t y)
{
    // Draw region in upper left
    const char *region = myRegion ? myRegion->name : NULL;
    drawIconScreen(region, display, state, x, y);
}

void drawOEMIconScreen(const char *upperMsg, OLEDDisplay *display, OLEDDisplayUiState *state, int16_t x, int16_t y)
{
    // draw an xbm image.
    // Please note that everything that should be transitioned
    // needs to be drawn relative to x and y
    ActiveScreen * screen = reinterpret_cast<ActiveScreen *>(state->userData);

    // draw centered icon left to right and centered above the one line of app text
    display->drawXbm(x + (screen->displayWidth - oemStore.oem_icon_width) / 2,
                     y + (screen->displayHeight - FONT_HEIGHT_MEDIUM - oemStore.oem_icon_height) / 2 + 2, oemStore.oem_icon_width,
                     oemStore.oem_icon_height, (const uint8_t *)oemStore.oem_icon_bits.bytes);

    switch (oemStore.oem_font) {
    case 0:
        display->setFont(FONT_SMALL);
        break;
    case 2:
        display->setFont(FONT_LARGE);
        break;
    default:
        display->setFont(FONT_MEDIUM);
        break;
    }

    display->setTextAlignment(TEXT_ALIGN_LEFT);
    const char *title = oemStore.oem_text;

    display->drawString(x + screen->getStringCenteredX(title), y + screen->displayHeight - FONT_HEIGHT_MEDIUM, title);
    display->setFont(FONT_SMALL);

    // Draw region in upper left
    if (upperMsg)
        display->drawString(x + 0, y + 0, upperMsg);

    // Draw version in upper right
    char buf[16];
    snprintf(buf, sizeof(buf), "%s",
             xstr(APP_VERSION_SHORT)); // Note: we don't bother printing region or now, it makes the string too long
    display->drawString(x + screen->displayWidth - display->getStringWidth(buf), y + 0, buf);
    screen->forceDisplay();

    // FIXME - draw serial # somewhere?
}

static void drawOEMBootScreen(OLEDDisplay *display, OLEDDisplayUiState *state, int16_t x, int16_t y)
{
    // Draw region in upper left
    const char *region = myRegion ? myRegion->name : NULL;
    drawOEMIconScreen(region, display, state, x, y);
}

// Used on boot when a certificate is being created
static void drawSSLScreen(OLEDDisplay *display, OLEDDisplayUiState *state, int16_t x, int16_t y)
{
    display->setTextAlignment(TEXT_ALIGN_CENTER);
    display->setFont(FONT_SMALL);
    display->drawString(64 + x, y, "Creating SSL certificate");

#ifdef ARCH_ESP32
    yield();
    esp_task_wdt_reset();
#endif

    display->setFont(FONT_SMALL);
    if ((millis() / 1000) % 2) {
        display->drawString(64 + x, FONT_HEIGHT_SMALL + y + 2, "Please wait . . .");
    } else {
        display->drawString(64 + x, FONT_HEIGHT_SMALL + y + 2, "Please wait . .  ");
    }
}

// Used when booting without a region set
static void drawWelcomeScreen(OLEDDisplay *display, OLEDDisplayUiState *state, int16_t x, int16_t y)
{
    display->setFont(FONT_SMALL);
    display->setTextAlignment(TEXT_ALIGN_CENTER);
    display->drawString(64 + x, y, "//\\ E S H T /\\ S T / C");
    display->drawString(64 + x, y + FONT_HEIGHT_SMALL, getDeviceName());
    display->setTextAlignment(TEXT_ALIGN_LEFT);

    if ((millis() / 10000) % 2) {
        display->drawString(x, y + FONT_HEIGHT_SMALL * 2 - 3, "Set the region using the");
        display->drawString(x, y + FONT_HEIGHT_SMALL * 3 - 3, "Meshtastic Android, iOS,");
        display->drawString(x, y + FONT_HEIGHT_SMALL * 4 - 3, "Flasher or CLI client.");
    } else {
        display->drawString(x, y + FONT_HEIGHT_SMALL * 2 - 3, "Visit meshtastic.org");
        display->drawString(x, y + FONT_HEIGHT_SMALL * 3 - 3, "for more information.");
        display->drawString(x, y + FONT_HEIGHT_SMALL * 4 - 3, "");
    }

#ifdef ARCH_ESP32
    yield();
    esp_task_wdt_reset();
#endif
}

#ifdef USE_EINK
/// Used on eink displays while in deep sleep
static void drawSleepScreen(OLEDDisplay *display, OLEDDisplayUiState *state, int16_t x, int16_t y)
{
    drawIconScreen("Sleeping...", display, state, x, y);
}
#endif

static void drawModuleFrame(OLEDDisplay *display, OLEDDisplayUiState *state, int16_t x, int16_t y)
{
    uint8_t module_frame;
    // there's a little but in the UI transition code
    // where it invokes the function at the correct offset
    // in the array of "drawScreen" functions; however,
    // the passed-state doesn't quite reflect the "current"
    // screen, so we have to detect it.
    if (state->frameState == IN_TRANSITION && state->transitionFrameRelationship == INCOMING) {
        // if we're transitioning from the end of the frame list back around to the first
        // frame, then we want this to be `0`
        module_frame = state->transitionFrameTarget;
    } else {
        // otherwise, just display the module frame that's aligned with the current frame
        module_frame = state->currentFrame;
        // LOG_DEBUG("Screen is not in transition.  Frame: %d\n\n", module_frame);
    }
    // LOG_DEBUG("Drawing Module Frame %d\n\n", module_frame);
    MeshModule &pi = *moduleFrames.at(module_frame);
    pi.drawFrame(display, state, x, y);
}

static void drawFrameBluetooth(OLEDDisplay *display, OLEDDisplayUiState *state, int16_t x, int16_t y)
{
    int x_offset = display->width() / 2;
    int y_offset = display->height() == 64 ? 0 : 32;
    display->setTextAlignment(TEXT_ALIGN_CENTER);
    display->setFont(FONT_MEDIUM);
    display->drawString(x_offset + x, y_offset + y, "Bluetooth");

    display->setFont(FONT_SMALL);
    y_offset = display->height() == 64 ? y_offset + FONT_HEIGHT_MEDIUM - 4 : y_offset + FONT_HEIGHT_MEDIUM + 5;
    display->drawString(x_offset + x, y_offset + y, "Enter this code");

    display->setFont(FONT_LARGE);
    String displayPin(btPIN);
    String pin = displayPin.substring(0, 3) + " " + displayPin.substring(3, 6);
    y_offset = display->height() == 64 ? y_offset + FONT_HEIGHT_SMALL - 5 : y_offset + FONT_HEIGHT_SMALL + 5;
    display->drawString(x_offset + x, y_offset + y, pin);

    display->setFont(FONT_SMALL);
    String deviceName = "Name: ";
    deviceName.concat(getDeviceName());
    y_offset = display->height() == 64 ? y_offset + FONT_HEIGHT_LARGE - 6 : y_offset + FONT_HEIGHT_LARGE + 5;
    display->drawString(x_offset + x, y_offset + y, deviceName);
}

static void drawFrameShutdown(OLEDDisplay *display, OLEDDisplayUiState *state, int16_t x, int16_t y)
{
    display->setTextAlignment(TEXT_ALIGN_CENTER);

    display->setFont(FONT_MEDIUM);
    display->drawString(64 + x, 26 + y, "Shutting down...");
}

static void drawFrameReboot(OLEDDisplay *display, OLEDDisplayUiState *state, int16_t x, int16_t y)
{
    display->setTextAlignment(TEXT_ALIGN_CENTER);

    display->setFont(FONT_MEDIUM);
    display->drawString(64 + x, 26 + y, "Rebooting...");
}

static void drawFrameFirmware(OLEDDisplay *display, OLEDDisplayUiState *state, int16_t x, int16_t y)
{
    display->setTextAlignment(TEXT_ALIGN_CENTER);
    display->setFont(FONT_MEDIUM);
    display->drawString(64 + x, y, "Updating");

    display->setFont(FONT_SMALL);
    display->setTextAlignment(TEXT_ALIGN_LEFT);
    display->drawStringMaxWidth(0 + x, 2 + y + FONT_HEIGHT_SMALL * 2, x + display->getWidth(),
                                "Please be patient and do not power off.");
}

/// Draw the last text message we received
static void drawCriticalFaultFrame(OLEDDisplay *display, OLEDDisplayUiState *state, int16_t x, int16_t y)
{
    displayedNodeNum = 0; // Not currently showing a node pane

    display->setTextAlignment(TEXT_ALIGN_LEFT);
    display->setFont(FONT_MEDIUM);

    char tempBuf[24];
    snprintf(tempBuf, sizeof(tempBuf), "Critical fault #%d", myNodeInfo.error_code);
    display->drawString(0 + x, 0 + y, tempBuf);
    display->setTextAlignment(TEXT_ALIGN_LEFT);
    display->setFont(FONT_SMALL);
    display->drawString(0 + x, FONT_HEIGHT_MEDIUM + y, "For help, please visit \nmeshtastic.org");
}

// Ignore messages orginating from phone (from the current node 0x0) unless range test or store and forward module are enabled
static bool shouldDrawMessage(const meshtastic_MeshPacket *packet)
{
    return packet->from != 0 && !moduleConfig.range_test.enabled && !moduleConfig.store_forward.enabled;
}

/// Draw the last text message we received
static void drawTextMessageFrame(OLEDDisplay *display, OLEDDisplayUiState *state, int16_t x, int16_t y)
{
    displayedNodeNum = 0; // Not currently showing a node pane

    // the max length of this buffer is much longer than we can possibly print
    static char tempBuf[237];

    meshtastic_MeshPacket &mp = devicestate.rx_text_message;
    meshtastic_NodeInfo *node = nodeDB.getNode(getFrom(&mp));
    // LOG_DEBUG("drawing text message from 0x%x: %s\n", mp.from,
    // mp.decoded.variant.data.decoded.bytes);

    // Demo for drawStringMaxWidth:
    // with the third parameter you can define the width after which words will
    // be wrapped. Currently only spaces and "-" are allowed for wrapping
    display->setTextAlignment(TEXT_ALIGN_LEFT);
    display->setFont(FONT_SMALL);
    if (config.display.displaymode == meshtastic_Config_DisplayConfig_DisplayMode_INVERTED) {
        display->fillRect(0 + x, 0 + y, x + display->getWidth(), y + FONT_HEIGHT_SMALL);
        display->setColor(BLACK);
    }
    display->drawStringf(0 + x, 0 + y, tempBuf, "From: %s", (node && node->has_user) ? node->user.short_name : "???");
    if (config.display.heading_bold) {
        display->drawStringf(1 + x, 0 + y, tempBuf, "From: %s", (node && node->has_user) ? node->user.short_name : "???");
    }
    display->setColor(WHITE);
    snprintf(tempBuf, sizeof(tempBuf), "%s", mp.decoded.payload.bytes);
    display->drawStringMaxWidth(0 + x, 0 + y + FONT_HEIGHT_SMALL, x + display->getWidth(), tempBuf);
}

/// Draw a series of fields in a column, wrapping to multiple colums if needed
void drawColumns(ActiveScreen * screen, int16_t x, int16_t y, const char **fields)
{
    // The coordinates define the left starting point of the text
    screen->dispdev->setTextAlignment(TEXT_ALIGN_LEFT);

    const char **f = fields;
    int xo = x, yo = y;
    while (*f) {
        screen->dispdev->drawString(xo, yo, *f);
        if ((screen->dispdev->getColor() == BLACK) && config.display.heading_bold)
            screen->dispdev->drawString(xo + 1, yo, *f);

        screen->dispdev->setColor(WHITE);
        yo += FONT_HEIGHT_SMALL;
        if (yo > screen->displayHeight - FONT_HEIGHT_SMALL) {
            xo += screen->displayWidth / 2;
            yo = 0;
        }
        f++;
    }
}

#if 0
    /// Draw a series of fields in a row, wrapping to multiple rows if needed
    /// @return the max y we ended up printing to
    static uint32_t drawRows(OLEDDisplay *display, int16_t x, int16_t y, const char **fields)
    {
        // The coordinates define the left starting point of the text
        display->setTextAlignment(TEXT_ALIGN_LEFT);

        ActiveScreen * screen = reinterpret_cast<ActiveScreen *>(state->userData);
        const char **f = fields;
        int xo = x, yo = y;
        const int COLUMNS = 2; // hardwired for two columns per row....
        int col = 0;           // track which column we are on
        while (*f) {
            display->drawString(xo, yo, *f);
            xo += screen->displayWidth / COLUMNS;
            // Wrap to next row, if needed.
            if (++col >= COLUMNS) {
                xo = x;
                yo += FONT_HEIGHT_SMALL;
                col = 0;
            }
            f++;
        }
        if (col != 0) {
            // Include last incomplete line in our total.
            yo += FONT_HEIGHT_SMALL;
        }

        return yo;
    }
#endif

namespace
{

/// A basic 2D point class for drawing
class Point
{
  public:
    float x, y;

    Point(float _x, float _y) : x(_x), y(_y) {}

    /// Apply a rotation around zero (standard rotation matrix math)
    void rotate(float radian)
    {
        float cos = cosf(radian), sin = sinf(radian);
        float rx = x * cos + y * sin, ry = -x * sin + y * cos;

        x = rx;
        y = ry;
    }

    void translate(int16_t dx, int dy)
    {
        x += dx;
        y += dy;
    }

    void scale(float f)
    {
        // We use -f here to counter the flip that happens
        // on the y axis when drawing and rotating on screen
        x *= f;
        y *= -f;
    }
};

} // namespace

static void drawLine(OLEDDisplay *d, const Point &p1, const Point &p2)
{
    d->drawLine(p1.x, p1.y, p2.x, p2.y);
}

/**
 * Given a recent lat/lon return a guess of the heading the user is walking on.
 *
 * We keep a series of "after you've gone 10 meters, what is your heading since
 * the last reference point?"
 */
static float estimatedHeading(double lat, double lon)
{
    static double oldLat, oldLon;
    static float b;

    if (oldLat == 0) {
        // just prepare for next time
        oldLat = lat;
        oldLon = lon;

        return b;
    }

    float d = GeoCoord::latLongToMeter(oldLat, oldLon, lat, lon);
    if (d < 10) // haven't moved enough, just keep current bearing
        return b;

    b = GeoCoord::bearing(oldLat, oldLon, lat, lon);
    oldLat = lat;
    oldLon = lon;

    return b;
}

/// Sometimes we will have Position objects that only have a time, so check for
/// valid lat/lon
static bool hasPosition(meshtastic_NodeInfo *n)
{
    return n->has_position && (n->position.latitude_i != 0 || n->position.longitude_i != 0);
}

static uint16_t getCompassDiam(OLEDDisplay *display)
{
    uint16_t diam = 0;
    uint16_t offset = 0;

    if (config.display.displaymode != meshtastic_Config_DisplayConfig_DisplayMode_DEFAULT)
        offset = FONT_HEIGHT_SMALL;

    // get the smaller of the 2 dimensions and subtract 20
    if (display->getWidth() > (display->getHeight() - offset)) {
        diam = display->getHeight() - offset;
        // if 2/3 of the other size would be smaller, use that
        if (diam > (display->getWidth() * 2 / 3)) {
            diam = display->getWidth() * 2 / 3;
        }
    } else {
        diam = display->getWidth();
        if (diam > ((display->getHeight() - offset) * 2 / 3)) {
            diam = (display->getHeight() - offset) * 2 / 3;
        }
    }

    return diam - 20;
};

/// We will skip one node - the one for us, so we just blindly loop over all
/// nodes
static size_t nodeIndex;
static int8_t prevFrame = -1;

// Draw the arrow pointing to a node's location
static void drawNodeHeading(OLEDDisplay *display, int16_t compassX, int16_t compassY, float headingRadian)
{
    Point tip(0.0f, 0.5f), tail(0.0f, -0.5f); // pointing up initially
    float arrowOffsetX = 0.2f, arrowOffsetY = 0.2f;
    Point leftArrow(tip.x - arrowOffsetX, tip.y - arrowOffsetY), rightArrow(tip.x + arrowOffsetX, tip.y - arrowOffsetY);

    Point *arrowPoints[] = {&tip, &tail, &leftArrow, &rightArrow};

    for (int i = 0; i < 4; i++) {
        arrowPoints[i]->rotate(headingRadian);
        arrowPoints[i]->scale(getCompassDiam(display) * 0.6);
        arrowPoints[i]->translate(compassX, compassY);
    }
    drawLine(display, tip, tail);
    drawLine(display, leftArrow, tip);
    drawLine(display, rightArrow, tip);
}

// Draw north
static void drawCompassNorth(OLEDDisplay *display, int16_t compassX, int16_t compassY, float myHeading)
{
    // If north is supposed to be at the top of the compass we want rotation to be +0
    if (config.display.compass_north_top)
        myHeading = -0;

    Point N1(-0.04f, 0.65f), N2(0.04f, 0.65f);
    Point N3(-0.04f, 0.55f), N4(0.04f, 0.55f);
    Point *rosePoints[] = {&N1, &N2, &N3, &N4};

    for (int i = 0; i < 4; i++) {
        // North on compass will be negative of heading
        rosePoints[i]->rotate(-myHeading);
        rosePoints[i]->scale(getCompassDiam(display));
        rosePoints[i]->translate(compassX, compassY);
    }
    drawLine(display, N1, N3);
    drawLine(display, N2, N4);
    drawLine(display, N1, N4);
}

/// Convert an integer GPS coords to a floating point
#define DegD(i) (i * 1e-7)

void drawNodeInfo(OLEDDisplay *display, OLEDDisplayUiState *state, int16_t x, int16_t y)
{
    ActiveScreen * screen = reinterpret_cast<ActiveScreen *>(state->userData);

    // We only advance our nodeIndex if the frame # has changed - because
    // drawNodeInfo will be called repeatedly while the frame is shown
    if (state->currentFrame != prevFrame) {
        prevFrame = state->currentFrame;

        nodeIndex = (nodeIndex + 1) % nodeDB.getNumNodes();
        meshtastic_NodeInfo *n = nodeDB.getNodeByIndex(nodeIndex);
        if (n->num == nodeDB.getNodeNum()) {
            // Don't show our node, just skip to next
            nodeIndex = (nodeIndex + 1) % nodeDB.getNumNodes();
            n = nodeDB.getNodeByIndex(nodeIndex);
        }
        displayedNodeNum = n->num;
    }

    meshtastic_NodeInfo *node = nodeDB.getNodeByIndex(nodeIndex);

    display->setFont(FONT_SMALL);

    // The coordinates define the left starting point of the text
    display->setTextAlignment(TEXT_ALIGN_LEFT);

    if (config.display.displaymode == meshtastic_Config_DisplayConfig_DisplayMode_INVERTED) {
        display->fillRect(0 + x, 0 + y, x + display->getWidth(), y + FONT_HEIGHT_SMALL);
    }

    const char *username = node->has_user ? node->user.long_name : "Unknown Name";

    static char signalStr[20];
    snprintf(signalStr, sizeof(signalStr), "Signal: %d%%", clamp((int)((node->snr + 10) * 5), 0, 100));

    uint32_t agoSecs = sinceLastSeen(node);
    static char lastStr[20];
    if (agoSecs < 120) // last 2 mins?
        snprintf(lastStr, sizeof(lastStr), "%u seconds ago", agoSecs);
    else if (agoSecs < 120 * 60) // last 2 hrs
        snprintf(lastStr, sizeof(lastStr), "%u minutes ago", agoSecs / 60);
    else {

        uint32_t hours_in_month = 730;

        // Only show hours ago if it's been less than 6 months. Otherwise, we may have bad
        //   data.
        if ((agoSecs / 60 / 60) < (hours_in_month * 6)) {
            snprintf(lastStr, sizeof(lastStr), "%u hours ago", agoSecs / 60 / 60);
        } else {
            snprintf(lastStr, sizeof(lastStr), "unknown age");
        }
    }

    static char distStr[20];
    strncpy(distStr, "? km", sizeof(distStr)); // might not have location data
    meshtastic_NodeInfo *ourNode = nodeDB.getNode(nodeDB.getNodeNum());
    const char *fields[] = {username, distStr, signalStr, lastStr, NULL};
    int16_t compassX = 0, compassY = 0;

    // coordinates for the center of the compass/circle
    if (config.display.displaymode == meshtastic_Config_DisplayConfig_DisplayMode_DEFAULT) {
        compassX = x + screen->displayWidth - getCompassDiam(display) / 2 - 5;
        compassY = y + screen->displayHeight / 2;
    } else {
        compassX = x + screen->displayWidth - getCompassDiam(display) / 2 - 5;
        compassY = y + FONT_HEIGHT_SMALL + (screen->displayHeight - FONT_HEIGHT_SMALL) / 2;
    }
    bool hasNodeHeading = false;

    if (ourNode && hasPosition(ourNode)) {
        meshtastic_Position &op = ourNode->position;
        float myHeading = estimatedHeading(DegD(op.latitude_i), DegD(op.longitude_i));
        drawCompassNorth(display, compassX, compassY, myHeading);

        if (hasPosition(node)) {
            // display direction toward node
            hasNodeHeading = true;
            meshtastic_Position &p = node->position;
            float d =
                GeoCoord::latLongToMeter(DegD(p.latitude_i), DegD(p.longitude_i), DegD(op.latitude_i), DegD(op.longitude_i));

            if (config.display.units == meshtastic_Config_DisplayConfig_DisplayUnits_IMPERIAL) {
                if (d < (2 * MILES_TO_FEET))
                    snprintf(distStr, sizeof(distStr), "%.0f ft", d * METERS_TO_FEET);
                else
                    snprintf(distStr, sizeof(distStr), "%.1f mi", d * METERS_TO_FEET / MILES_TO_FEET);
            } else {
                if (d < 2000)
                    snprintf(distStr, sizeof(distStr), "%.0f m", d);
                else
                    snprintf(distStr, sizeof(distStr), "%.1f km", d / 1000);
            }

            float bearingToOther =
                GeoCoord::bearing(DegD(op.latitude_i), DegD(op.longitude_i), DegD(p.latitude_i), DegD(p.longitude_i));
            // If the top of the compass is a static north then bearingToOther can be drawn on the compass directly
            // If the top of the compass is not a static north we need adjust bearingToOther based on heading
            if (!config.display.compass_north_top)
                bearingToOther -= myHeading;
            drawNodeHeading(display, compassX, compassY, bearingToOther);
        }
    }
    if (!hasNodeHeading) {
        // direction to node is unknown so display question mark
        // Debug info for gps lock errors
        // LOG_DEBUG("ourNode %d, ourPos %d, theirPos %d\n", !!ourNode, ourNode && hasPosition(ourNode), hasPosition(node));
        display->drawString(compassX - FONT_HEIGHT_SMALL / 4, compassY - FONT_HEIGHT_SMALL / 2, "?");
    }
    display->drawCircle(compassX, compassY, getCompassDiam(display) / 2);

    if (config.display.displaymode == meshtastic_Config_DisplayConfig_DisplayMode_INVERTED) {
        display->setColor(BLACK);
    }
    // Must be after distStr is populated
    drawColumns(screen, x, y, fields);
}



// #ifdef RAK4630
// ActiveScreen::ActiveScreen(uint8_t address, int sda, int scl) : OSThread("Screen"), cmdQueue(32), dispdev(address, sda, scl),
// dispdev_oled(address, sda, scl), ui(&dispdev)
// {
//     address_found = address;
//     cmdQueue.setReader(this);
//     if (screen_found) {
//         (void)dispdev;
//         AutoOLEDWire dispdev = dispdev_oled;
//         (void)ui;
//         OLEDDisplayUi ui(&dispdev);
//     }
// }
// #else

// #endif

ActiveScreen::ActiveScreen(std::unique_ptr<OLEDDisplay> display)
    : Screen('s'),
      OSThread("Screen"),
      cmdQueue(32),
      dispdev(std::move(display)),
      ui(dispdev.get())
{
    cmdQueue.setReader(this);
}

/**
 * Prepare the display for the unit going to the lowest power mode possible.  Most screens will just
 * poweroff, but eink screens will show a "I'm sleeping" graphic, possibly with a QR code
 */
void ActiveScreen::doDeepSleep()
{
#ifdef USE_EINK
    static FrameCallback sleepFrames[] = {drawSleepScreen};
    static const int sleepFrameCount = sizeof(sleepFrames) / sizeof(sleepFrames[0]);
    ui.setFrames(sleepFrames, sleepFrameCount);
    ui.update();
#endif
    setOn(false);
}

bool ActiveScreen::enqueueCmd(const ScreenCmd &cmd)
{
    if (!useDisplay)
        return true; // claim success if our display is not in use
    else {
        bool success = cmdQueue.enqueue(cmd, 0);
        enabled = true; // handle ASAP (we are the registered reader for cmdQueue, but might have been disabled)
        return success;
    }
}

void ActiveScreen::handleSetOn(bool on)
{
    if (!useDisplay)
        return;

    if (on != screenOn) {
        if (on) {
            LOG_INFO("Turning on screen\n");
            dispdev->displayOn();
            dispdev->displayOn();
            enabled = true;
            setInterval(0); // Draw ASAP
            runASAP = true;
        } else {
            LOG_INFO("Turning off screen\n");
            dispdev->displayOff();
            enabled = false;
        }
        screenOn = on;
    }
}

void ActiveScreen::setup()
{
    // We don't set useDisplay until setup() is called, because some boards have a declaration of this object but the device
    // is never found when probing i2c and therefore we don't call setup and never want to do (invalid) accesses to this device.
    useDisplay = true;

    // Initialising the UI will init the display too.
    ui.init();

    displayWidth = dispdev->width();
    displayHeight = dispdev->height();

    ui.setTimePerTransition(0);

    ui.setIndicatorPosition(BOTTOM);
    // Defines where the first frame is located in the bar.
    ui.setIndicatorDirection(LEFT_RIGHT);
    ui.setFrameAnimation(SLIDE_LEFT);
    // Don't show the page swipe dots while in boot screen.
    ui.disableAllIndicators();
    // Store a pointer to Screen so we can get to it from static functions.
    ui.getUiState()->userData = static_cast<void*>(this);

    // Set the utf8 conversion function
    dispdev->setFontTableLookupFunction(customFontTableLookup);

    if (strlen(oemStore.oem_text) > 0)
        logo_timeout *= 2;

    // Add frames.
    static FrameCallback bootFrames[] = {drawBootScreen};
    static const int bootFrameCount = sizeof(bootFrames) / sizeof(bootFrames[0]);
    ui.setFrames(bootFrames, bootFrameCount);
    // No overlays.
    ui.setOverlays(nullptr, 0);

    // Require presses to switch between frames.
    ui.disableAutoTransition();

    // Set up a log buffer with 3 lines, 32 chars each.
    dispdev->setLogBuffer(3, 32);

#ifdef SCREEN_MIRROR
    dispdev->mirrorScreen();
#else
    // Standard behaviour is to FLIP the screen (needed on T-Beam). If this config item is set, unflip it, and thereby logically
    // flip it. If you have a headache now, you're welcome.
    if (!config.display.flip_screen) {
        dispdev->flipScreenVertically();
    }
#endif

    // Get our hardware ID
    uint8_t dmac[6];
    getMacAddr(dmac);
    snprintf(graphics::ourId, sizeof(ourId), "%02x%02x", dmac[4], dmac[5]);

    // Turn on the display.
    handleSetOn(true);

    // On some ssd1306 clones, the first draw command is discarded, so draw it
    // twice initially. Skip this for EINK Displays to save a few seconds during boot
    ui.update();
#ifndef USE_EINK
    ui.update();
#endif
    serialSinceMsec = millis();

    // Subscribe to status updates
    powerStatusObserver.observe(&powerStatus->onNewStatus);
    gpsStatusObserver.observe(&gpsStatus->onNewStatus);
    nodeStatusObserver.observe(&nodeStatus->onNewStatus);
    if (textMessageModule)
        textMessageObserver.observe(textMessageModule);

    // Modules can notify screen about refresh
    MeshModule::observeUIEvents(&uiFrameEventObserver);
}

void ActiveScreen::setOn(bool on)
{
    if (!on)
        handleSetOn(
            false); // We handle off commands immediately, because they might be called because the CPU is shutting down
    else
        enqueueCmd(ScreenCmd{.cmd = on ? Cmd::SET_ON : Cmd::SET_OFF});
}

void ActiveScreen::forceDisplay()
{
    // Nasty hack to force epaper updates for 'key' frames.  FIXME, cleanup.
#ifdef USE_EINK
    dispdev->forceDisplay();
#endif
}

static uint32_t lastScreenTransition;

int32_t ActiveScreen::runOnce()
{
    // If we don't have a screen, don't ever spend any CPU for us.
    if (!useDisplay) {
        enabled = false;
        return RUN_SAME;
    }

    // Show boot screen for first logo_timeout seconds, then switch to normal operation.
    // serialSinceMsec adjusts for additional serial wait time during nRF52 bootup
    static bool showingBootScreen = true;
    if (showingBootScreen && (millis() > (logo_timeout + serialSinceMsec))) {
        LOG_INFO("Done with boot screen...\n");
        stopBootScreen();
        showingBootScreen = false;
    }

    // If we have an OEM Boot screen, toggle after logo_timeout seconds
    if (strlen(oemStore.oem_text) > 0) {
        static bool showingOEMBootScreen = true;
        if (showingOEMBootScreen && (millis() > ((logo_timeout / 2) + serialSinceMsec))) {
            LOG_INFO("Switch to OEM screen...\n");
            // Change frames.
            static FrameCallback bootOEMFrames[] = {drawOEMBootScreen};
            static const int bootOEMFrameCount = sizeof(bootOEMFrames) / sizeof(bootOEMFrames[0]);
            ui.setFrames(bootOEMFrames, bootOEMFrameCount);
            ui.update();
#ifndef USE_EINK
            ui.update();
#endif
            showingOEMBootScreen = false;
        }
    }

#ifndef DISABLE_WELCOME_UNSET
    if (showingNormalScreen && config.lora.region == meshtastic_Config_LoRaConfig_RegionCode_UNSET) {
        setWelcomeFrames();
    }
#endif

    // Process incoming commands.
    for (;;) {
        ScreenCmd cmd;
        if (!cmdQueue.dequeue(&cmd, 0)) {
            break;
        }
        switch (cmd.cmd) {
        case Cmd::SET_ON:
            handleSetOn(true);
            break;
        case Cmd::SET_OFF:
            handleSetOn(false);
            break;
        case Cmd::ON_PRESS:
            // If a nag notification is running, stop it
            if (moduleConfig.external_notification.enabled && (externalNotificationModule->nagCycleCutoff != UINT32_MAX)) {
                externalNotificationModule->stopNow();
            } else {
                // Don't advance the screen if we just wanted to switch off the nag notification
                handleOnPress();
            }
            break;
        case Cmd::START_BLUETOOTH_PIN_SCREEN:
            handleStartBluetoothPinScreen(cmd.bluetooth_pin);
            break;
        case Cmd::START_FIRMWARE_UPDATE_SCREEN:
            handleStartFirmwareUpdateScreen();
            break;
        case Cmd::STOP_BLUETOOTH_PIN_SCREEN:
        case Cmd::STOP_BOOT_SCREEN:
            setFrames();
            break;
        case Cmd::PRINT:
            handlePrint(cmd.print_text);
            free(cmd.print_text);
            break;
        case Cmd::START_SHUTDOWN_SCREEN:
            handleShutdownScreen();
            break;
        case Cmd::START_REBOOT_SCREEN:
            handleRebootScreen();
            break;
        default:
            LOG_ERROR("Invalid screen cmd\n");
        }
    }

    if (!screenOn) { // If we didn't just wake and the screen is still off, then
                     // stop updating until it is on again
        enabled = false;
        return 0;
    }

    // this must be before the frameState == FIXED check, because we always
    // want to draw at least one FIXED frame before doing forceDisplay
    ui.update();

    // Switch to a low framerate (to save CPU) when we are not in transition
    // but we should only call setTargetFPS when framestate changes, because
    // otherwise that breaks animations.
    if (targetFramerate != IDLE_FRAMERATE && ui.getUiState()->frameState == FIXED) {
        // oldFrameState = ui.getUiState()->frameState;
        targetFramerate = IDLE_FRAMERATE;

        ui.setTargetFPS(targetFramerate);
        forceDisplay();
    }

    // While showing the bootscreen or Bluetooth pair screen all of our
    // standard screen switching is stopped.
    if (showingNormalScreen) {
        // standard screen loop handling here
        if (config.display.auto_screen_carousel_secs > 0 &&
            (millis() - lastScreenTransition) > (config.display.auto_screen_carousel_secs * 1000)) {
            LOG_DEBUG("LastScreenTransition exceeded %ums transitioning to next frame\n", (millis() - lastScreenTransition));
            handleOnPress();
        }
    }

    // LOG_DEBUG("want fps %d, fixed=%d\n", targetFramerate,
    // ui.getUiState()->frameState); If we are scrolling we need to be called
    // soon, otherwise just 1 fps (to save CPU) We also ask to be called twice
    // as fast as we really need so that any rounding errors still result with
    // the correct framerate
    return (1000 / targetFramerate);
}

void ActiveScreen::drawDebugInfoTrampoline(OLEDDisplay *display, OLEDDisplayUiState *state, int16_t x, int16_t y)
{
    ActiveScreen *screen2 = reinterpret_cast<ActiveScreen *>(state->userData);
    screen2->debugInfo.drawFrame(display, state, x, y);
}

void ActiveScreen::drawDebugInfoSettingsTrampoline(OLEDDisplay *display, OLEDDisplayUiState *state, int16_t x, int16_t y)
{
    ActiveScreen *screen2 = reinterpret_cast<ActiveScreen *>(state->userData);
    screen2->debugInfo.drawFrameSettings(display, state, x, y);
}

void ActiveScreen::drawDebugInfoWiFiTrampoline(OLEDDisplay *display, OLEDDisplayUiState *state, int16_t x, int16_t y)
{
    ActiveScreen *screen2 = reinterpret_cast<ActiveScreen *>(state->userData);
    screen2->debugInfo.drawFrameWiFi(display, state, x, y);
}

/* show a message that the SSL cert is being built
 * it is expected that this will be used during the boot phase */
void ActiveScreen::setSSLFrames()
{
    if (dispdev.get()) {
        // LOG_DEBUG("showing SSL frames\n");
        static FrameCallback sslFrames[] = {drawSSLScreen};
        ui.setFrames(sslFrames, 1);
        ui.update();
    }
}

/* show a message that the SSL cert is being built
 * it is expected that this will be used during the boot phase */
void ActiveScreen::setWelcomeFrames()
{
    if (dispdev.get()) {
        // LOG_DEBUG("showing Welcome frames\n");
        ui.disableAllIndicators();

        static FrameCallback welcomeFrames[] = {drawWelcomeScreen};
        ui.setFrames(welcomeFrames, 1);
    }
}

// restore our regular frame list
void ActiveScreen::setFrames()
{
    LOG_DEBUG("showing standard frames\n");
    showingNormalScreen = true;

    moduleFrames = MeshModule::GetMeshModulesWithUIFrames();
    LOG_DEBUG("Showing %d module frames\n", moduleFrames.size());
    int totalFrameCount = MAX_NUM_NODES + NUM_EXTRA_FRAMES + moduleFrames.size();
    LOG_DEBUG("Total frame count: %d\n", totalFrameCount);

    // We don't show the node info our our node (if we have it yet - we should)
    size_t numnodes = nodeStatus->getNumTotal();
    if (numnodes > 0)
        numnodes--;

    size_t numframes = 0;

    // put all of the module frames first.
    // this is a little bit of a dirty hack; since we're going to call
    // the same drawModuleFrame handler here for all of these module frames
    // and then we'll just assume that the state->currentFrame value
    // is the same offset into the moduleFrames vector
    // so that we can invoke the module's callback
    for (auto i = moduleFrames.begin(); i != moduleFrames.end(); ++i) {
        normalFrames[numframes++] = drawModuleFrame;
    }

    LOG_DEBUG("Added modules.  numframes: %d\n", numframes);

    // If we have a critical fault, show it first
    if (myNodeInfo.error_code)
        normalFrames[numframes++] = drawCriticalFaultFrame;

    // If we have a text message - show it next, unless it's a phone message and we aren't using any special modules
    if (devicestate.has_rx_text_message && shouldDrawMessage(&devicestate.rx_text_message)) {
        normalFrames[numframes++] = drawTextMessageFrame;
    }

    // then all the nodes
    // We only show a few nodes in our scrolling list - because meshes with many nodes would have too many screens
    size_t numToShow = min(numnodes, 4U);
    for (size_t i = 0; i < numToShow; i++)
        normalFrames[numframes++] = drawNodeInfo;

    // then the debug info
    //
    // Since frames are basic function pointers, we have to use a helper to
    // call a method on debugInfo object.
    normalFrames[numframes++] = &ActiveScreen::drawDebugInfoTrampoline;

    // call a method on debugInfoScreen object (for more details)
    normalFrames[numframes++] = &ActiveScreen::drawDebugInfoSettingsTrampoline;

#ifdef ARCH_ESP32
    if (isWifiAvailable()) {
        // call a method on debugInfoScreen object (for more details)
        normalFrames[numframes++] = &ActiveScreen::drawDebugInfoWiFiTrampoline;
    }
#endif

    LOG_DEBUG("Finished building frames. numframes: %d\n", numframes);

    ui.setFrames(normalFrames, numframes);
    ui.enableAllIndicators();

    prevFrame = -1; // Force drawNodeInfo to pick a new node (because our list
                    // just changed)

    setFastFramerate(); // Draw ASAP
}

void ActiveScreen::handleStartBluetoothPinScreen(uint32_t pin)
{
    LOG_DEBUG("showing bluetooth screen\n");
    showingNormalScreen = false;

    static FrameCallback btFrames[] = {drawFrameBluetooth};

    snprintf(btPIN, sizeof(btPIN), "%06u", pin);

    ui.disableAllIndicators();
    ui.setFrames(btFrames, 1);
    setFastFramerate();
}

void ActiveScreen::handleShutdownScreen()
{
    LOG_DEBUG("showing shutdown screen\n");
    showingNormalScreen = false;

    static FrameCallback shutdownFrames[] = {drawFrameShutdown};

    ui.disableAllIndicators();
    ui.setFrames(shutdownFrames, 1);
    setFastFramerate();
}

void ActiveScreen::handleRebootScreen()
{
    LOG_DEBUG("showing reboot screen\n");
    showingNormalScreen = false;

    static FrameCallback rebootFrames[] = {drawFrameReboot};

    ui.disableAllIndicators();
    ui.setFrames(rebootFrames, 1);
    setFastFramerate();
}

void ActiveScreen::handleStartFirmwareUpdateScreen()
{
    LOG_DEBUG("showing firmware screen\n");
    showingNormalScreen = false;

    static FrameCallback btFrames[] = {drawFrameFirmware};

    ui.disableAllIndicators();
    ui.setFrames(btFrames, 1);
    setFastFramerate();
}

void ActiveScreen::blink()
{
    setFastFramerate();
    uint8_t count = 10;
    dispdev->setBrightness(254);
    while (count > 0) {
        dispdev->fillRect(0, 0, displayWidth, displayHeight);
        dispdev->display();
        delay(50);
        dispdev->clear();
        dispdev->display();
        delay(50);
        count = count - 1;
    }
    dispdev->setBrightness(brightness);
}

void ActiveScreen::onPress()
{
    enqueueCmd(ScreenCmd{.cmd = Cmd::ON_PRESS});
}

void ActiveScreen::handlePrint(const char *text)
{
    // the string passed into us probably has a newline, but that would confuse the logging system
    // so strip it
    LOG_DEBUG("Screen: %.*s\n", strlen(text) - 1, text);
    if (!useDisplay || !showingNormalScreen)
        return;

    dispdev->print(text);
}

void ActiveScreen::handleOnPress()
{
    // If screen was off, just wake it, otherwise advance to next frame
    // If we are in a transition, the press must have bounced, drop it.
    if (ui.getUiState()->frameState == FIXED) {
        ui.nextFrame();
        lastScreenTransition = millis();
        setFastFramerate();
    }
}

#ifndef SCREEN_TRANSITION_FRAMERATE
#define SCREEN_TRANSITION_FRAMERATE 30 // fps
#endif

void ActiveScreen::setFastFramerate()
{
    // We are about to start a transition so speed up fps
    targetFramerate = SCREEN_TRANSITION_FRAMERATE;

    ui.setTargetFPS(targetFramerate);
    setInterval(0); // redraw ASAP
    runASAP = true;
}


// adjust Brightness cycle trough 1 to 254 as long as attachDuringLongPress is true
void ActiveScreen::adjustBrightness()
{
    if (!useDisplay)
        return;

    if (brightness == 254) {
        brightness = 0;
    } else {
        brightness++;
    }
    int width = brightness / (254.00 / displayWidth);
    dispdev->drawRect(0, 30, displayWidth, 4);
    dispdev->fillRect(0, 31, width, 2);
    dispdev->display();
    dispdev->setBrightness(brightness);
}

void ActiveScreen::startBluetoothPinScreen(uint32_t pin)
{
    ScreenCmd cmd;
    cmd.cmd = Cmd::START_BLUETOOTH_PIN_SCREEN;
    cmd.bluetooth_pin = pin;
    enqueueCmd(cmd);
}

void ActiveScreen::startFirmwareUpdateScreen()
{
    ScreenCmd cmd;
    cmd.cmd = Cmd::START_FIRMWARE_UPDATE_SCREEN;
    enqueueCmd(cmd);
}

void ActiveScreen::startShutdownScreen()
{
    ScreenCmd cmd;
    cmd.cmd = Cmd::START_SHUTDOWN_SCREEN;
    enqueueCmd(cmd);
}

void ActiveScreen::startRebootScreen()
{
    ScreenCmd cmd;
    cmd.cmd = Cmd::START_REBOOT_SCREEN;
    enqueueCmd(cmd);
}

void ActiveScreen::stopBluetoothPinScreen()
{
    enqueueCmd(ScreenCmd{.cmd = Cmd::STOP_BLUETOOTH_PIN_SCREEN});
}

void ActiveScreen::stopBootScreen()
{
    enqueueCmd(ScreenCmd{.cmd = Cmd::STOP_BOOT_SCREEN});
}

void ActiveScreen::print(const char *text)
{
    ScreenCmd cmd;
    cmd.cmd = Cmd::PRINT;
    // TODO(girts): strdup() here is scary, but we can't use std::string as
    // FreeRTOS queue is just dumbly copying memory contents. It would be
    // nice if we had a queue that could copy objects by value.
    cmd.print_text = strdup(text);
    if (!enqueueCmd(cmd)) {
        free(cmd.print_text);
    }
}

char ActiveScreen::customFontTableLookup(const uint8_t ch)
{
    // UTF-8 to font table index converter
    // Code form http://playground.arduino.cc/Main/Utf8ascii
    static uint8_t LASTCHAR;
    static bool SKIPREST; // Only display a single unconvertable-character symbol per sequence of unconvertable characters

    if (ch < 128) { // Standard ASCII-set 0..0x7F handling
        LASTCHAR = 0;
        SKIPREST = false;
        return ch;
    }

    uint8_t last = LASTCHAR; // get last char
    LASTCHAR = ch;

    switch (last) { // conversion depending on first UTF8-character
    case 0xC2: {
        SKIPREST = false;
        return (uint8_t)ch;
    }
    case 0xC3: {
        SKIPREST = false;
        return (uint8_t)(ch | 0xC0);
    }
    // map UTF-8 cyrillic chars to it Windows-1251 (CP-1251) ASCII codes
    // note: in this case we must use compatible font - provided ArialMT_Plain_10/16/24 by 'ThingPulse/esp8266-oled-ssd1306'
    // library have empty chars for non-latin ASCII symbols
    case 0xD0: {
        SKIPREST = false;
        if (ch == 129)
            return (uint8_t)(168); // Ё
        if (ch > 143 && ch < 192)
            return (uint8_t)(ch + 48);
        break;
    }
    case 0xD1: {
        SKIPREST = false;
        if (ch == 145)
            return (uint8_t)(184); // ё
        if (ch > 127 && ch < 144)
            return (uint8_t)(ch + 112);
        break;
    }
    }

    // We want to strip out prefix chars for two-byte char formats
    if (ch == 0xC2 || ch == 0xC3 || ch == 0x82 || ch == 0xD0 || ch == 0xD1)
        return (uint8_t)0;

    // If we already returned an unconvertable-character symbol for this unconvertable-character sequence, return NULs for the
    // rest of it
    if (SKIPREST)
        return (uint8_t)0;
    SKIPREST = true;

    return (uint8_t)191; // otherwise: return ¿ if character can't be converted (note that the font map we're using doesn't
                         // stick to standard EASCII codes)
}

DebugInfo * ActiveScreen::debug_info()
{
    return &debugInfo;
}

int ActiveScreen::handleStatusUpdate(const meshtastic::Status *arg)
{
    // LOG_DEBUG("Screen got status update %d\n", arg->getStatusType());
    switch (arg->getStatusType()) {
    case STATUS_TYPE_NODE:
        if (showingNormalScreen && nodeStatus->getLastNumTotal() != nodeStatus->getNumTotal()) {
            setFrames(); // Regen the list of screens
        }
        nodeDB.updateGUI = false;
        break;
    }

    return 0;
}

int ActiveScreen::handleTextMessage(const meshtastic_MeshPacket *packet)
{
    if (showingNormalScreen) {
        setFrames(); // Regen the list of screens (will show new text message)
    }

    return 0;
}

int ActiveScreen::handleUIFrameEvent(const UIFrameEvent *event)
{
    if (showingNormalScreen) {
        if (event->frameChanged) {
            setFrames(); // Regen the list of screens (will show new text message)
        } else if (event->needRedraw) {
            setFastFramerate();
            // TODO: We might also want switch to corresponding frame,
            //       but we don't know the exact frame number.
            // ui.switchToFrame(0);
        }
    }

    return 0;
}

int ActiveScreen::getStringCenteredX(const char * s)
{
    return (displayWidth - dispdev->getStringWidth(s)) / 2;
}

} // namespace graphics
