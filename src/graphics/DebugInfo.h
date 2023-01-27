#ifndef MESHTASTIC_GRAPHICS_DEBUGINFO
#define MESHTASTIC_GRAPHICS_DEBUGINFO

#include "concurrency/Lock.h"


#include "OLEDDisplay.h"
#include "OLEDDisplayUi.h"

namespace graphics {

class OLEDDisplayScreen;

/// Handles gathering and displaying debug information.
class DebugInfo
{
  public:
    DebugInfo(const DebugInfo &) = delete;
    DebugInfo &operator=(const DebugInfo &) = delete;

  protected:
    DebugInfo() {}
  private:
    friend class OLEDDisplayScreen;


    /// Renders the debug screen.
    void drawFrame(OLEDDisplay *display, OLEDDisplayUiState *state, int16_t x, int16_t y);
    void drawFrameSettings(OLEDDisplay *display, OLEDDisplayUiState *state, int16_t x, int16_t y);
    void drawFrameWiFi(OLEDDisplay *display, OLEDDisplayUiState *state, int16_t x, int16_t y);

    /// Protects all of internal state.
    concurrency::Lock lock;
};
}

#endif
