#include "shared.h"

Adafruit_ST7789 tft(&SPI, TFT_CS, TFT_DC, TFT_RST);

GFXcanvas16 &sharedCanvas() {
  static GFXcanvas16 c(240, 240);  // allocato alla prima chiamata
  return c;
}

// drawRGBBitmap manda i pixel uno alla volta (circa 60 ms per fotogramma). Qui il canvas viene
// scambiato di byte sul posto (il display vuole il byte alto per primo), inviato con un unico
// trasferimento SPI a blocchi e rimesso a posto, cosi' le anteprime (SNAP) leggono i colori giusti.
void blitCanvas(GFXcanvas16 &c) {
  uint16_t *b = c.getBuffer();
  const int n = 240 * 240;
  for (int i = 0; i < n; i++) b[i] = __builtin_bswap16(b[i]);
  tft.startWrite();
  tft.setAddrWindow(0, 0, 240, 240);
  SPI.transfer(b, nullptr, n * 2);
  tft.endWrite();
  for (int i = 0; i < n; i++) b[i] = __builtin_bswap16(b[i]);
}
