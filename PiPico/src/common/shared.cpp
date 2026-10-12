#include "shared.h"
#include "hardware/spi.h"  // scrittura SPI diretta dell'SDK del Pico

Adafruit_ST7789 tft(&SPI, TFT_CS, TFT_DC, TFT_RST);

GFXcanvas16 &sharedCanvas() {
  static GFXcanvas16 c(240, 240);  // allocato alla prima chiamata
  return c;
}

// REV16: scambia i due byte di ogni mezza parola in un'unica istruzione (ARMv6-M)
static inline uint32_t rev16(uint32_t x) {
  uint32_t r;
  asm("rev16 %0, %1" : "=l"(r) : "l"(x));
  return r;
}

// drawRGBBitmap manda i pixel uno alla volta (circa 60 ms per fotogramma). Qui il canvas viene
// scambiato di byte sul posto (il display vuole il byte alto per primo), inviato con un unico
// trasferimento SPI a blocchi e rimesso a posto, cosi' le anteprime (SNAP) leggono i colori giusti.
unsigned long blitUs = 0;  // durata dell'ultimo blitCanvas (per il comando FPS)

void blitCanvas(GFXcanvas16 &c) {
  unsigned long t0 = micros();
  uint16_t *b = c.getBuffer();
  const int n = 240 * 240;
  uint32_t *w = (uint32_t *)b;  // due pixel alla volta
  for (int i = 0; i < n / 2; i++) w[i] = rev16(w[i]);
  tft.startWrite();
  tft.setAddrWindow(0, 0, 240, 240);
  spi_write_blocking(spi0, (const uint8_t *)b, n * 2);  // solo scrittura, senza leggere la risposta: piu' veloce di SPI.transfer
  tft.endWrite();
  for (int i = 0; i < n / 2; i++) w[i] = rev16(w[i]);
  blitUs = micros() - t0;
}
