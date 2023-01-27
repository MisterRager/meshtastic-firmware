#include "DebugInfo.h"
#include "images.h"
#include "Screen.h"
#include "PowerStatus.h"
#include "NodeStatus.h"
#include "GPSStatus.h"
#include "GeoCoord.h"
#include "mesh/http/WiFiAPClient.h"
#include "concurrency/LockGuard.h"
#include "RTC.h"
#include "airtime.h"
#include "OLEDDisplayScreen.h"

#include "modules/ExternalNotificationModule.h"

#ifdef ARCH_ESP32
#include "modules/esp32/StoreForwardModule.h"
#include "WiFiType.h"
#endif

namespace graphics
{

    // Draw power bars or a charging indicator on an image of a battery, determined by battery charge voltage or percentage.
    static void drawBattery(OLEDDisplay *display, int16_t x, int16_t y, uint8_t *imgBuffer, const meshtastic::PowerStatus *powerStatus)
    {
        static const uint8_t powerBar[3] = {0x81, 0xBD, 0xBD};
        static const uint8_t lightning[8] = {0xA1, 0xA1, 0xA5, 0xAD, 0xB5, 0xA5, 0x85, 0x85};
        // Clear the bar area on the battery image
        for (int i = 1; i < 14; i++)
        {
            imgBuffer[i] = 0x81;
        }
        // If charging, draw a charging indicator
        if (powerStatus->getIsCharging())
        {
            memcpy(imgBuffer + 3, lightning, 8);
            // If not charging, Draw power bars
        }
        else
        {
            for (int i = 0; i < 4; i++)
            {
                if (powerStatus->getBatteryChargePercent() >= 25 * i)
                    memcpy(imgBuffer + 1 + (i * 3), powerBar, 3);
            }
        }
        display->drawFastImage(x, y, 16, 8, imgBuffer);
    }

    // Draw nodes status
    static void drawNodes(OLEDDisplay *display, int16_t x, int16_t y, meshtastic::NodeStatus *nodeStatus)
    {
        char usersString[20];
        snprintf(usersString, sizeof(usersString), "%d/%d", nodeStatus->getNumOnline(), nodeStatus->getNumTotal());
#if defined(USE_EINK) || defined(ILI9341_DRIVER) || defined(ST7735_CS)
        display->drawFastImage(x, y + 3, 8, 8, imgUser);
#else
        display->drawFastImage(x, y, 8, 8, imgUser);
#endif
        display->drawString(x + 10, y - 2, usersString);
        if (config.display.heading_bold)
            display->drawString(x + 11, y - 2, usersString);
    }

    uint8_t imgBattery[16] = {0xFF, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0xE7, 0x3C};

    // Threshold values for the GPS lock accuracy bar display
    uint32_t dopThresholds[5] = {2000, 1000, 500, 200, 100};

    // Draw GPS status summary
    static void drawGPS(OLEDDisplay *display, int16_t x, int16_t y, meshtastic::GPSStatus *const gps)
    {
        if (config.position.fixed_position)
        {
            // GPS coordinates are currently fixed
            display->drawString(x - 1, y - 2, "Fixed GPS");
            if (config.display.heading_bold)
                display->drawString(x, y - 2, "Fixed GPS");
            return;
        }
        if (!gps->getIsConnected())
        {
            display->drawString(x, y - 2, "No GPS");
            if (config.display.heading_bold)
                display->drawString(x + 1, y - 2, "No GPS");
            return;
        }
        display->drawFastImage(x, y, 6, 8, gps->getHasLock() ? imgPositionSolid : imgPositionEmpty);
        if (!gps->getHasLock())
        {
            display->drawString(x + 8, y - 2, "No sats");
            if (config.display.heading_bold)
                display->drawString(x + 9, y - 2, "No sats");
            return;
        }
        else
        {
            char satsString[3];
            uint8_t bar[2] = {0};

            // Draw DOP signal bars
            for (int i = 0; i < 5; i++)
            {
                if (gps->getDOP() <= dopThresholds[i])
                    bar[0] = ~((1 << (5 - i)) - 1);
                else
                    bar[0] = 0b10000000;
                // bar[1] = bar[0];
                display->drawFastImage(x + 9 + (i * 2), y, 2, 8, bar);
            }

            // Draw satellite image
            display->drawFastImage(x + 24, y, 8, 8, imgSatellite);

            // Draw the number of satellites
            snprintf(satsString, sizeof(satsString), "%u", gps->getNumSatellites());
            display->drawString(x + 34, y - 2, satsString);
            if (config.display.heading_bold)
                display->drawString(x + 35, y - 2, satsString);
        }
    }

    // Draw status when gps is disabled by PMU
    static void drawGPSpowerstat(OLEDDisplay *display, int16_t x, int16_t y, const meshtastic::GPSStatus *gps)
    {
#ifdef HAS_PMU
        String displayLine = "GPS disabled";
        int16_t xPos = display->getStringWidth(displayLine);

        if (!config.position.gps_enabled)
        {
            display->drawString(x + xPos, y, displayLine);
#ifdef GPS_POWER_TOGGLE
            display->drawString(x + xPos, y - 2 + FONT_HEIGHT_SMALL, " by button");
#endif
            // display->drawString(x + xPos, y + 2, displayLine);
        }
#endif
    }

    // GeoCoord object for the screen
    GeoCoord geoCoord;

    static void drawGPSAltitude(OLEDDisplay *display, int16_t x, int16_t y, const meshtastic::GPSStatus *gps)
    {
        String displayLine = "";
        if (!gps->getIsConnected() && !config.position.fixed_position)
        {
            // displayLine = "No GPS Module";
            // display->drawString(x + (display->getWidth() - (display->getStringWidth(displayLine))) / 2, y, displayLine);
        }
        else if (!gps->getHasLock() && !config.position.fixed_position)
        {
            // displayLine = "No GPS Lock";
            // display->drawString(x + (display->getWidth() - (display->getStringWidth(displayLine))) / 2, y, displayLine);
        }
        else
        {
            geoCoord.updateCoords(int32_t(gps->getLatitude()), int32_t(gps->getLongitude()), int32_t(gps->getAltitude()));
            displayLine = "Altitude: " + String(geoCoord.getAltitude()) + "m";
            if (config.display.units == meshtastic_Config_DisplayConfig_DisplayUnits_IMPERIAL)
                displayLine = "Altitude: " + String(geoCoord.getAltitude() * METERS_TO_FEET) + "ft";
            display->drawString(x + (display->getWidth() - (display->getStringWidth(displayLine))) / 2, y, displayLine);
        }
    }

    // Draw GPS status coordinates
    static void drawGPScoordinates(OLEDDisplay *display, int16_t x, int16_t y, const meshtastic::GPSStatus *gps)
    {
        auto gpsFormat = config.display.gps_format;
        String displayLine = "";

        if (!gps->getIsConnected() && !config.position.fixed_position)
        {
            displayLine = "No GPS Module";
            display->drawString(x + (display->getWidth() - (display->getStringWidth(displayLine))) / 2, y, displayLine);
        }
        else if (!gps->getHasLock() && !config.position.fixed_position)
        {
            displayLine = "No GPS Lock";
            display->drawString(x + (display->getWidth() - (display->getStringWidth(displayLine))) / 2, y, displayLine);
        }
        else
        {

            geoCoord.updateCoords(int32_t(gps->getLatitude()), int32_t(gps->getLongitude()), int32_t(gps->getAltitude()));

            if (gpsFormat != meshtastic_Config_DisplayConfig_GpsCoordinateFormat_DMS)
            {
                char coordinateLine[22];
                if (gpsFormat == meshtastic_Config_DisplayConfig_GpsCoordinateFormat_DEC)
                { // Decimal Degrees
                    snprintf(coordinateLine, sizeof(coordinateLine), "%f %f", geoCoord.getLatitude() * 1e-7,
                             geoCoord.getLongitude() * 1e-7);
                }
                else if (gpsFormat == meshtastic_Config_DisplayConfig_GpsCoordinateFormat_UTM)
                { // Universal Transverse Mercator
                    snprintf(coordinateLine, sizeof(coordinateLine), "%2i%1c %06u %07u", geoCoord.getUTMZone(), geoCoord.getUTMBand(),
                             geoCoord.getUTMEasting(), geoCoord.getUTMNorthing());
                }
                else if (gpsFormat == meshtastic_Config_DisplayConfig_GpsCoordinateFormat_MGRS)
                { // Military Grid Reference System
                    snprintf(coordinateLine, sizeof(coordinateLine), "%2i%1c %1c%1c %05u %05u", geoCoord.getMGRSZone(),
                             geoCoord.getMGRSBand(), geoCoord.getMGRSEast100k(), geoCoord.getMGRSNorth100k(),
                             geoCoord.getMGRSEasting(), geoCoord.getMGRSNorthing());
                }
                else if (gpsFormat == meshtastic_Config_DisplayConfig_GpsCoordinateFormat_OLC)
                { // Open Location Code
                    geoCoord.getOLCCode(coordinateLine);
                }
                else if (gpsFormat == meshtastic_Config_DisplayConfig_GpsCoordinateFormat_OSGR)
                {                                                                         // Ordnance Survey Grid Reference
                    if (geoCoord.getOSGRE100k() == 'I' || geoCoord.getOSGRN100k() == 'I') // OSGR is only valid around the UK region
                        snprintf(coordinateLine, sizeof(coordinateLine), "%s", "Out of Boundary");
                    else
                        snprintf(coordinateLine, sizeof(coordinateLine), "%1c%1c %05u %05u", geoCoord.getOSGRE100k(),
                                 geoCoord.getOSGRN100k(), geoCoord.getOSGREasting(), geoCoord.getOSGRNorthing());
                }

                // If fixed position, display text "Fixed GPS" alternating with the coordinates.
                if (config.position.fixed_position)
                {
                    if ((millis() / 10000) % 2)
                    {
                        display->drawString(x + (display->getWidth() - (display->getStringWidth(coordinateLine))) / 2, y, coordinateLine);
                    }
                    else
                    {
                        display->drawString(x + (display->getWidth() - (display->getStringWidth("Fixed GPS"))) / 2, y, "Fixed GPS");
                    }
                }
                else
                {
                    display->drawString(x + (display->getWidth() - (display->getStringWidth(coordinateLine))) / 2, y, coordinateLine);
                }
            }
            else
            {
                char latLine[22];
                char lonLine[22];
                snprintf(latLine, sizeof(latLine), "%2i° %2i' %2u\" %1c", geoCoord.getDMSLatDeg(), geoCoord.getDMSLatMin(),
                         geoCoord.getDMSLatSec(), geoCoord.getDMSLatCP());
                snprintf(lonLine, sizeof(lonLine), "%3i° %2i' %2u\" %1c", geoCoord.getDMSLonDeg(), geoCoord.getDMSLonMin(),
                         geoCoord.getDMSLonSec(), geoCoord.getDMSLonCP());
                display->drawString(x + (display->getWidth() - (display->getStringWidth(latLine))) / 2, y - FONT_HEIGHT_SMALL * 1, latLine);
                display->drawString(x + (display->getWidth() - (display->getStringWidth(lonLine))) / 2, y, lonLine);
            }
        }
    }

    void DebugInfo::drawFrame(OLEDDisplay *display, OLEDDisplayUiState *state, int16_t x, int16_t y)
    {
        OLEDDisplayScreen *screen = reinterpret_cast<OLEDDisplayScreen *>(state->userData);

        displayedNodeNum = 0; // Not currently showing a node pane

        display->setFont(FONT_SMALL);

        // The coordinates define the left starting point of the text
        display->setTextAlignment(TEXT_ALIGN_LEFT);

        if (config.display.displaymode == meshtastic_Config_DisplayConfig_DisplayMode_INVERTED)
        {
            display->fillRect(0 + x, 0 + y, x + display->getWidth(), y + FONT_HEIGHT_SMALL);
            display->setColor(BLACK);
        }

        char channelStr[20];
        {
            concurrency::LockGuard guard(&lock);
            auto chName = channels.getPrimaryName();
            snprintf(channelStr, sizeof(channelStr), "%s", chName);
        }

        // Display power status
        if (powerStatus->getHasBattery())
        {
            if (config.display.displaymode == meshtastic_Config_DisplayConfig_DisplayMode_DEFAULT)
            {
                drawBattery(display, x, y + 2, imgBattery, powerStatus);
            }
            else
            {
                drawBattery(display, x + 1, y + 3, imgBattery, powerStatus);
            }
        }
        else if (powerStatus->knowsUSB())
        {
            if (config.display.displaymode == meshtastic_Config_DisplayConfig_DisplayMode_DEFAULT)
            {
                display->drawFastImage(x, y + 2, 16, 8, powerStatus->getHasUSB() ? imgUSB : imgPower);
            }
            else
            {
                display->drawFastImage(x + 1, y + 3, 16, 8, powerStatus->getHasUSB() ? imgUSB : imgPower);
            }
        }
        // Display nodes status
        if (config.display.displaymode == meshtastic_Config_DisplayConfig_DisplayMode_DEFAULT)
        {
            drawNodes(display, x + (display->getWidth() * 0.25), y + 2, nodeStatus);
        }
        else
        {
            drawNodes(display, x + (display->getWidth() * 0.25), y + 3, nodeStatus);
        }
        // Display GPS status
        if (!config.position.gps_enabled)
        {
            int16_t yPos = y + 2;
#ifdef GPS_POWER_TOGGLE
            yPos = (y + 10 + FONT_HEIGHT_SMALL);
#endif
            drawGPSpowerstat(display, x, yPos, gpsStatus);
        }
        else
        {
            if (config.display.displaymode == meshtastic_Config_DisplayConfig_DisplayMode_DEFAULT)
            {
                drawGPS(display, x + (display->getWidth() * 0.63), y + 2, gpsStatus);
            }
            else
            {
                drawGPS(display, x + (display->getWidth() * 0.63), y + 3, gpsStatus);
            }
        }

        display->setColor(WHITE);
        // Draw the channel name
        display->drawString(x, y + FONT_HEIGHT_SMALL, channelStr);
        // Draw our hardware ID to assist with bluetooth pairing. Either prefix with Info or S&F Logo
        if (moduleConfig.store_forward.enabled)
        {
#ifdef ARCH_ESP32
            if (millis() - storeForwardModule->lastHeartbeat >
                (storeForwardModule->heartbeatInterval * 1200))
            { // no heartbeat, overlap a bit
#if defined(USE_EINK) || defined(ILI9341_DRIVER) || defined(ST7735_CS)
                display->drawFastImage(x + display->getWidth() - 14 - display->getStringWidth(screen->ourId), y + 3 + FONT_HEIGHT_SMALL, 12, 8,
                                       imgQuestionL1);
                display->drawFastImage(x + display->getWidth() - 14 - display->getStringWidth(screen->ourId), y + 11 + FONT_HEIGHT_SMALL, 12, 8,
                                       imgQuestionL2);
#else
                display->drawFastImage(x + display->getWidth() - 10 - display->getStringWidth(screen->ourId), y + 2 + FONT_HEIGHT_SMALL, 8, 8,
                                       imgQuestion);
#endif
            }
            else
            {
#if defined(USE_EINK) || defined(ILI9341_DRIVER) || defined(ST7735_CS)
                display->drawFastImage(x + display->getWidth() - 18 - display->getStringWidth(screen->ourId), y + 3 + FONT_HEIGHT_SMALL, 16, 8,
                                       imgSFL1);
                display->drawFastImage(x + display->getWidth() - 18 - display->getStringWidth(screen->ourId), y + 11 + FONT_HEIGHT_SMALL, 16, 8,
                                       imgSFL2);
#else
                display->drawFastImage(x + display->getWidth() - 13 - display->getStringWidth(screen->ourId), y + 2 + FONT_HEIGHT_SMALL, 11, 8,
                                       imgSF);
#endif
            }
#endif
        }
        else
        {
#if defined(USE_EINK) || defined(ILI9341_DRIVER) || defined(ST7735_CS)
            display->drawFastImage(x + display->getWidth() - 14 - display->getStringWidth(screen->ourId), y + 3 + FONT_HEIGHT_SMALL, 12, 8,
                                   imgInfoL1);
            display->drawFastImage(x + display->getWidth() - 14 - display->getStringWidth(screen->ourId), y + 11 + FONT_HEIGHT_SMALL, 12, 8,
                                   imgInfoL2);
#else
            display->drawFastImage(x + display->getWidth() - 10 - display->getStringWidth(screen->ourId), y + 2 + FONT_HEIGHT_SMALL, 8, 8, imgInfo);
#endif
        }

        display->drawString(x + display->getWidth() - display->getStringWidth(screen->ourId), y + FONT_HEIGHT_SMALL, screen->ourId);

        // Draw any log messages
        display->drawLogBuffer(x, y + (FONT_HEIGHT_SMALL * 2));

        /* Display a heartbeat pixel that blinks every time the frame is redrawn */
#ifdef SHOW_REDRAWS
        if (screen->heartbeat)
            display->setPixel(0, 0);
        screen->heartbeat = !screen->heartbeat;
#endif
    }

    // Jm
    void DebugInfo::drawFrameWiFi(OLEDDisplay *display, OLEDDisplayUiState *state, int16_t x, int16_t y)
    {
#if HAS_WIFI
        const char *wifiName = config.network.wifi_ssid;

        displayedNodeNum = 0; // Not currently showing a node pane

        display->setFont(FONT_SMALL);

        // The coordinates define the left starting point of the text
        display->setTextAlignment(TEXT_ALIGN_LEFT);

        if (config.display.displaymode == meshtastic_Config_DisplayConfig_DisplayMode_INVERTED)
        {
            display->fillRect(0 + x, 0 + y, x + display->getWidth(), y + FONT_HEIGHT_SMALL);
            display->setColor(BLACK);
        }

        if (WiFi.status() != WL_CONNECTED)
        {
            display->drawString(x, y, String("WiFi: Not Connected"));
            if (config.display.heading_bold)
                display->drawString(x + 1, y, String("WiFi: Not Connected"));
        }
        else
        {
            display->drawString(x, y, String("WiFi: Connected"));
            if (config.display.heading_bold)
                display->drawString(x + 1, y, String("WiFi: Connected"));

            display->drawString(x + display->getWidth() - display->getStringWidth("RSSI " + String(WiFi.RSSI())), y,
                                "RSSI " + String(WiFi.RSSI()));
            if (config.display.heading_bold)
            {
                display->drawString(x + display->getWidth() - display->getStringWidth("RSSI " + String(WiFi.RSSI())) - 1, y,
                                    "RSSI " + String(WiFi.RSSI()));
            }
        }

        display->setColor(WHITE);

        /*
        - WL_CONNECTED: assigned when connected to a WiFi network;
        - WL_NO_SSID_AVAIL: assigned when no SSID are available;
        - WL_CONNECT_FAILED: assigned when the connection fails for all the attempts;
        - WL_CONNECTION_LOST: assigned when the connection is lost;
        - WL_DISCONNECTED: assigned when disconnected from a network;
        - WL_IDLE_STATUS: it is a temporary status assigned when WiFi.begin() is called and remains active until the number of
        attempts expires (resulting in WL_CONNECT_FAILED) or a connection is established (resulting in WL_CONNECTED);
        - WL_SCAN_COMPLETED: assigned when the scan networks is completed;
        - WL_NO_SHIELD: assigned when no WiFi shield is present;

        */
        if (WiFi.status() == WL_CONNECTED)
        {
            display->drawString(x, y + FONT_HEIGHT_SMALL * 1, "IP: " + String(WiFi.localIP().toString().c_str()));
        }
        else if (WiFi.status() == WL_NO_SSID_AVAIL)
        {
            display->drawString(x, y + FONT_HEIGHT_SMALL * 1, "SSID Not Found");
        }
        else if (WiFi.status() == WL_CONNECTION_LOST)
        {
            display->drawString(x, y + FONT_HEIGHT_SMALL * 1, "Connection Lost");
        }
        else if (WiFi.status() == WL_CONNECT_FAILED)
        {
            display->drawString(x, y + FONT_HEIGHT_SMALL * 1, "Connection Failed");
        }
        else if (WiFi.status() == WL_IDLE_STATUS)
        {
            display->drawString(x, y + FONT_HEIGHT_SMALL * 1, "Idle ... Reconnecting");
        }
        else
        {
            // Codes:
            // https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-guides/wifi.html#wi-fi-reason-code
            if (getWifiDisconnectReason() == 2)
            {
                display->drawString(x, y + FONT_HEIGHT_SMALL * 1, "Authentication Invalid");
            }
            else if (getWifiDisconnectReason() == 3)
            {
                display->drawString(x, y + FONT_HEIGHT_SMALL * 1, "De-authenticated");
            }
            else if (getWifiDisconnectReason() == 4)
            {
                display->drawString(x, y + FONT_HEIGHT_SMALL * 1, "Disassociated Expired");
            }
            else if (getWifiDisconnectReason() == 5)
            {
                display->drawString(x, y + FONT_HEIGHT_SMALL * 1, "AP - Too Many Clients");
            }
            else if (getWifiDisconnectReason() == 6)
            {
                display->drawString(x, y + FONT_HEIGHT_SMALL * 1, "NOT_AUTHED");
            }
            else if (getWifiDisconnectReason() == 7)
            {
                display->drawString(x, y + FONT_HEIGHT_SMALL * 1, "NOT_ASSOCED");
            }
            else if (getWifiDisconnectReason() == 8)
            {
                display->drawString(x, y + FONT_HEIGHT_SMALL * 1, "Disassociated");
            }
            else if (getWifiDisconnectReason() == 9)
            {
                display->drawString(x, y + FONT_HEIGHT_SMALL * 1, "ASSOC_NOT_AUTHED");
            }
            else if (getWifiDisconnectReason() == 10)
            {
                display->drawString(x, y + FONT_HEIGHT_SMALL * 1, "DISASSOC_PWRCAP_BAD");
            }
            else if (getWifiDisconnectReason() == 11)
            {
                display->drawString(x, y + FONT_HEIGHT_SMALL * 1, "DISASSOC_SUPCHAN_BAD");
            }
            else if (getWifiDisconnectReason() == 13)
            {
                display->drawString(x, y + FONT_HEIGHT_SMALL * 1, "IE_INVALID");
            }
            else if (getWifiDisconnectReason() == 14)
            {
                display->drawString(x, y + FONT_HEIGHT_SMALL * 1, "MIC_FAILURE");
            }
            else if (getWifiDisconnectReason() == 15)
            {
                display->drawString(x, y + FONT_HEIGHT_SMALL * 1, "AP Handshake Timeout");
            }
            else if (getWifiDisconnectReason() == 16)
            {
                display->drawString(x, y + FONT_HEIGHT_SMALL * 1, "GROUP_KEY_UPDATE_TIMEOUT");
            }
            else if (getWifiDisconnectReason() == 17)
            {
                display->drawString(x, y + FONT_HEIGHT_SMALL * 1, "IE_IN_4WAY_DIFFERS");
            }
            else if (getWifiDisconnectReason() == 18)
            {
                display->drawString(x, y + FONT_HEIGHT_SMALL * 1, "Invalid Group Cipher");
            }
            else if (getWifiDisconnectReason() == 19)
            {
                display->drawString(x, y + FONT_HEIGHT_SMALL * 1, "Invalid Pairwise Cipher");
            }
            else if (getWifiDisconnectReason() == 20)
            {
                display->drawString(x, y + FONT_HEIGHT_SMALL * 1, "AKMP_INVALID");
            }
            else if (getWifiDisconnectReason() == 21)
            {
                display->drawString(x, y + FONT_HEIGHT_SMALL * 1, "UNSUPP_RSN_IE_VERSION");
            }
            else if (getWifiDisconnectReason() == 22)
            {
                display->drawString(x, y + FONT_HEIGHT_SMALL * 1, "INVALID_RSN_IE_CAP");
            }
            else if (getWifiDisconnectReason() == 23)
            {
                display->drawString(x, y + FONT_HEIGHT_SMALL * 1, "802_1X_AUTH_FAILED");
            }
            else if (getWifiDisconnectReason() == 24)
            {
                display->drawString(x, y + FONT_HEIGHT_SMALL * 1, "CIPHER_SUITE_REJECTED");
            }
            else if (getWifiDisconnectReason() == 200)
            {
                display->drawString(x, y + FONT_HEIGHT_SMALL * 1, "BEACON_TIMEOUT");
            }
            else if (getWifiDisconnectReason() == 201)
            {
                display->drawString(x, y + FONT_HEIGHT_SMALL * 1, "AP Not Found");
            }
            else if (getWifiDisconnectReason() == 202)
            {
                display->drawString(x, y + FONT_HEIGHT_SMALL * 1, "AUTH_FAIL");
            }
            else if (getWifiDisconnectReason() == 203)
            {
                display->drawString(x, y + FONT_HEIGHT_SMALL * 1, "ASSOC_FAIL");
            }
            else if (getWifiDisconnectReason() == 204)
            {
                display->drawString(x, y + FONT_HEIGHT_SMALL * 1, "HANDSHAKE_TIMEOUT");
            }
            else if (getWifiDisconnectReason() == 205)
            {
                display->drawString(x, y + FONT_HEIGHT_SMALL * 1, "Connection Failed");
            }
            else
            {
                display->drawString(x, y + FONT_HEIGHT_SMALL * 1, "Unknown Status");
            }
        }

        display->drawString(x, y + FONT_HEIGHT_SMALL * 2, "SSID: " + String(wifiName));

        display->drawString(x, y + FONT_HEIGHT_SMALL * 3, "http://meshtastic.local");

        /* Display a heartbeat pixel that blinks every time the frame is redrawn */
#ifdef SHOW_REDRAWS
        ActiveScreen *screen = reinterpret_cast<ActiveScreen *>(state->userData);

        if (screen->heartbeat)
            display->setPixel(0, 0);
        screen->heartbeat = !screen->heartbeat;
#endif
#endif
    }

    void DebugInfo::drawFrameSettings(OLEDDisplay *display, OLEDDisplayUiState *state, int16_t x, int16_t y)
    {
        displayedNodeNum = 0; // Not currently showing a node pane

        display->setFont(FONT_SMALL);

        // The coordinates define the left starting point of the text
        display->setTextAlignment(TEXT_ALIGN_LEFT);

        if (config.display.displaymode == meshtastic_Config_DisplayConfig_DisplayMode_INVERTED)
        {
            display->fillRect(0 + x, 0 + y, x + display->getWidth(), y + FONT_HEIGHT_SMALL);
            display->setColor(BLACK);
        }

        char batStr[20];
        if (powerStatus->getHasBattery())
        {
            int batV = powerStatus->getBatteryVoltageMv() / 1000;
            int batCv = (powerStatus->getBatteryVoltageMv() % 1000) / 10;

            snprintf(batStr, sizeof(batStr), "B %01d.%02dV %3d%% %c%c", batV, batCv, powerStatus->getBatteryChargePercent(),
                     powerStatus->getIsCharging() ? '+' : ' ', powerStatus->getHasUSB() ? 'U' : ' ');

            // Line 1
            display->drawString(x, y, batStr);
            if (config.display.heading_bold)
                display->drawString(x + 1, y, batStr);
        }
        else
        {
            // Line 1
            display->drawString(x, y, String("USB"));
            if (config.display.heading_bold)
                display->drawString(x + 1, y, String("USB"));
        }

        auto mode = "";

        switch (config.lora.modem_preset)
        {
        case meshtastic_Config_LoRaConfig_ModemPreset_SHORT_SLOW:
            mode = "ShortS";
            break;
        case meshtastic_Config_LoRaConfig_ModemPreset_SHORT_FAST:
            mode = "ShortF";
            break;
        case meshtastic_Config_LoRaConfig_ModemPreset_MEDIUM_SLOW:
            mode = "MedS";
            break;
        case meshtastic_Config_LoRaConfig_ModemPreset_MEDIUM_FAST:
            mode = "MedF";
            break;
        case meshtastic_Config_LoRaConfig_ModemPreset_LONG_SLOW:
            mode = "LongS";
            break;
        case meshtastic_Config_LoRaConfig_ModemPreset_LONG_FAST:
            mode = "LongF";
            break;
        case meshtastic_Config_LoRaConfig_ModemPreset_VERY_LONG_SLOW:
            mode = "VeryL";
            break;
        default:
            mode = "Custom";
            break;
        }

        display->drawString(x + display->getWidth() - display->getStringWidth(mode), y, mode);
        if (config.display.heading_bold)
            display->drawString(x + display->getWidth() - display->getStringWidth(mode) - 1, y, mode);

        // Line 2
        uint32_t currentMillis = millis();
        uint32_t seconds = currentMillis / 1000;
        uint32_t minutes = seconds / 60;
        uint32_t hours = minutes / 60;
        uint32_t days = hours / 24;
        // currentMillis %= 1000;
        // seconds %= 60;
        // minutes %= 60;
        // hours %= 24;

        display->setColor(WHITE);

        // Show uptime as days, hours, minutes OR seconds
        String uptime;
        if (days >= 2)
            uptime += String(days) + "d ";
        else if (hours >= 2)
            uptime += String(hours) + "h ";
        else if (minutes >= 1)
            uptime += String(minutes) + "m ";
        else
            uptime += String(seconds) + "s ";

        uint32_t rtc_sec = getValidTime(RTCQuality::RTCQualityDevice);
        if (rtc_sec > 0)
        {
            long hms = rtc_sec % SEC_PER_DAY;
            // hms += tz.tz_dsttime * SEC_PER_HOUR;
            // hms -= tz.tz_minuteswest * SEC_PER_MIN;
            // mod `hms` to ensure in positive range of [0...SEC_PER_DAY)
            hms = (hms + SEC_PER_DAY) % SEC_PER_DAY;

            // Tear apart hms into h:m:s
            int hour = hms / SEC_PER_HOUR;
            int min = (hms % SEC_PER_HOUR) / SEC_PER_MIN;
            int sec = (hms % SEC_PER_HOUR) % SEC_PER_MIN; // or hms % SEC_PER_MIN

            char timebuf[9];
            snprintf(timebuf, sizeof(timebuf), "%02d:%02d:%02d", hour, min, sec);
            uptime += timebuf;
        }

        display->drawString(x, y + FONT_HEIGHT_SMALL * 1, uptime);

        // Display Channel Utilization
        char chUtil[13];
        snprintf(chUtil, sizeof(chUtil), "ChUtil %2.0f%%", airTime->channelUtilizationPercent());
        display->drawString(x + display->getWidth() - display->getStringWidth(chUtil), y + FONT_HEIGHT_SMALL * 1, chUtil);
        if (config.position.gps_enabled)
        {
            // Line 3
            if (config.display.gps_format !=
                meshtastic_Config_DisplayConfig_GpsCoordinateFormat_DMS) // if DMS then don't draw altitude
                drawGPSAltitude(display, x, y + FONT_HEIGHT_SMALL * 2, gpsStatus);

            // Line 4
            drawGPScoordinates(display, x, y + FONT_HEIGHT_SMALL * 3, gpsStatus);
        }
        else
        {
            drawGPSpowerstat(display, x - (display->getWidth() / 4), y + FONT_HEIGHT_SMALL * 2, gpsStatus);
        }
        /* Display a heartbeat pixel that blinks every time the frame is redrawn */
#ifdef SHOW_REDRAWS
        if (screen->heartbeat)
            display->setPixel(0, 0);
        screen->heartbeat = !screen->heartbeat;
#endif
    }

    // namespace graphics
}
