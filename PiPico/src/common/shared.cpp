#include "shared.h"

Adafruit_ST7789 tft(&SPI, TFT_CS, TFT_DC, TFT_RST);

GFXcanvas16 &sharedCanvas() {
  static GFXcanvas16 c(240, 240);  // allocato alla prima chiamata
  return c;
}
