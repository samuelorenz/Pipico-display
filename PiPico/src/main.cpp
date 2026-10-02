#include <Arduino.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>
#include <SPI.h>
#include "sprites.h"

// Numeri GPIO (non i pin fisici). SPI di default: SCK=GP18, MOSI=GP19
#define TFT_DC   16  // pin fisico 21
#define TFT_CS   17  // pin fisico 22
#define TFT_RST  21  // pin fisico 27
#define TFT_BL   22  // pin fisico 29

// SPI hardware (SPI0): SCL=GP18, SDA=GP19
Adafruit_ST7789 tft(&SPI, TFT_CS, TFT_DC, TFT_RST);

// Layout 240x240
const int TEXT_Y  = 0;    // area testo (ultima riga ricevuta)
const int TEXT_H  = 60;
const int PLOT_Y  = 64;   // area grafico
const int PLOT_W  = 240;
const int PLOT_H  = 176;
const int N_SAMP  = PLOT_W - 2;  // un campione per pixel

GFXcanvas16 plotCanvas(PLOT_W, PLOT_H);  // disegno fuori schermo, poi un solo blit (niente flicker)

float samples[N_SAMP];
int count = 0;  // campioni validi (max N_SAMP)
int head = 0;   // prossima posizione di scrittura

String line;

// Animazione: sinusoide verde che scorre. Parte all'avvio; da seriale
// 'anim' la riavvia, qualsiasi altro input passa alla modalita' grafico.
bool animMode = true;
float phase = 0;
GFXcanvas16 animCanvas(240, 240);

void drawAnimFrame() {
  const uint16_t DARK  = tft.color565(0, 90, 0);
  const uint16_t GRID  = 0x2104;
  animCanvas.fillScreen(ST77XX_BLACK);
  for (int x = 0; x < 240; x += 30) animCanvas.drawFastVLine(x, 0, 240, GRID);
  for (int y = 0; y < 240; y += 30) animCanvas.drawFastHLine(0, y, 240, GRID);
  animCanvas.drawFastHLine(0, 120, 240, 0x4208);

  int py = 0;
  for (int x = 0; x < 240; x++) {
    // onda con leggera modulazione di ampiezza per renderla piu' viva
    float a = 70 + 20 * sinf(phase * 0.4f);
    int y = 120 + (int)(a * sinf((x * 2.0f * PI / 120.0f) + phase));
    if (x > 0) {
      animCanvas.drawLine(x - 1, py - 2, x, y - 2, DARK);
      animCanvas.drawLine(x - 1, py + 2, x, y + 2, DARK);
      animCanvas.drawLine(x - 1, py - 1, x, y - 1, ST77XX_GREEN);
      animCanvas.drawLine(x - 1, py,     x, y,     ST77XX_GREEN);
      animCanvas.drawLine(x - 1, py + 1, x, y + 1, ST77XX_GREEN);
    }
    py = y;
  }
  tft.drawRGBBitmap(0, 0, animCanvas.getBuffer(), 240, 240);
  phase += 0.15f;
}

// --- Stile Pip-Boy (Fallout): intro Vault-Tec + schermata STAT con Vault Boy che cammina ---
bool pipMode = true;  // true = Pip-Boy, false = sinusoide
float walkT = 0;
int frameNo = 0;

// Disegna una maschera alpha a 8 bit in verde Pip-Boy, con luminosita' k e rotazione ang attorno al perno (px,py)
static void blitAlpha(GFXcanvas16 &c, int x0, int y0, int w, int h, const uint8_t *a, float k, float ang, int px, int py) {
  float cs = cosf(ang), sn = sinf(ang);
  int pad = (ang != 0) ? 10 : 0;
  for (int dy = -pad; dy < h + pad; dy++) {
    for (int dx = -pad; dx < w + pad; dx++) {
      int sx = dx, sy = dy;
      if (ang != 0) {  // mappa inversa: da pixel destinazione a pixel sorgente
        float rx = dx - px, ry = dy - py;
        sx = (int)lroundf(px + rx * cs + ry * sn);
        sy = (int)lroundf(py - rx * sn + ry * cs);
      }
      if (sx < 0 || sy < 0 || sx >= w || sy >= h) continue;
      uint8_t al = a[sy * w + sx];
      if (al < 8) continue;
      float f = al / 255.0f * k;
      c.drawPixel(x0 + dx, y0 + dy, tft.color565(40 * f, 255 * f, 100 * f));
    }
  }
}

static void typeText(const char *s, int x, int y, uint8_t size, uint16_t col, int ms) {
  tft.setTextSize(size);
  tft.setTextColor(col);
  tft.setCursor(x, y);
  for (const char *p = s; *p; p++) { tft.print(*p); delay(ms); }
}

// Intro all'accensione: CRT che si accende, logo Vault-Tec, boot del Pip-Boy
void playIntro() {
  const uint16_t G = tft.color565(40, 255, 100);
  GFXcanvas16 &c = animCanvas;
  tft.fillScreen(ST77XX_BLACK);

  // 1) accensione CRT: una riga luminosa che si apre
  for (int h = 2; h <= 240; h += 14) {
    c.fillScreen(ST77XX_BLACK);
    c.fillRect(0, 120 - h / 2, 240, h, tft.color565(0, 45, 16));
    c.drawFastHLine(0, 120, 240, G);
    tft.drawRGBBitmap(0, 0, c.getBuffer(), 240, 240);
  }

  // 2) logo Vault-Tec (immagine) in dissolvenza
  for (int k = 1; k <= 24; k++) {
    c.fillScreen(tft.color565(0, 10, 3));
    blitAlpha(c, 0, 32, VAULTTEC_LOGO_W, VAULTTEC_LOGO_H, VAULTTEC_LOGO, k / 24.0f, 0, 0, 0);
    tft.drawRGBBitmap(0, 0, c.getBuffer(), 240, 240);
    delay(25);
  }
  delay(1400);
  delay(1200);

  // 3) boot del Pip-Boy
  tft.fillScreen(ST77XX_BLACK);
  int y = 14;
  const char *lines[] = {
    "PIP-BOY 3000 MK IV",
    "VAULT-TEC UNIFIED OS",
    "COPYRIGHT 2075 VAULT-TEC",
    "",
    "> INIT SPI BUS ......... OK",
    "> DISPLAY ST7789 ....... OK",
    "> LOADING VAULT BOY .... OK",
    "> WELCOME, DWELLER",
  };
  for (const char *l : lines) { typeText(l, 6, y, 1, G, 14); y += 14; }
  delay(900);
  walkT = 0;
  frameNo = 0;
}

void drawPipFrame() {
  const uint16_t G   = tft.color565(40, 255, 100);
  const uint16_t GD  = tft.color565(0, 110, 40);
  const uint16_t BG  = tft.color565(0, 10, 3);
  const uint16_t BAR = tft.color565(0, 28, 10);
  GFXcanvas16 &c = animCanvas;

  c.fillScreen(BG);
  c.fillRect(0, (frameNo * 3) % 260 - 20, 240, 20, BAR);  // barra di scansione CRT

  // Tab in alto con "tacca" sotto STAT, come sul Pip-Boy
  c.setTextSize(1);
  c.setTextColor(G);
  const char *tabs[5] = {"STAT", "INV", "DATA", "MAP", "RADIO"};
  const int tx[5] = {12, 62, 102, 148, 190};
  for (int i = 0; i < 5; i++) { c.setCursor(tx[i], 10); c.print(tabs[i]); }
  c.drawFastHLine(0, 24, 8, G);  c.drawFastVLine(8, 6, 19, G);
  c.drawFastVLine(46, 6, 19, G); c.drawFastHLine(46, 22, 190, G);
  c.setCursor(10, 30);  c.setTextColor(G);  c.print("STATUS");
  c.setCursor(66, 30);  c.setTextColor(GD); c.print("SPECIAL");
  c.setCursor(130, 30); c.print("PERKS");

  // Tacche di mira attorno al personaggio
  c.fillRect(110, 42, 20, 3, G);   c.fillRect(110, 196, 20, 3, G);
  c.fillRect(62, 96, 18, 3, G);    c.fillRect(160, 96, 18, 3, G);
  c.fillRect(62, 160, 18, 3, G);   c.fillRect(160, 160, 18, 3, G);

  // Vault Boy (sprite ricavato dalla schermata del Pip-Boy) con dondolio da camminata
  float bob = 2.5f * fabsf(sinf(walkT));
  float flick = 0.92f + 0.08f * sinf(frameNo * 0.9f);  // leggero sfarfallio CRT
  blitAlpha(c, 120 - VB_SPRITE_W / 2, 46 - (int)bob, VB_SPRITE_W, VB_SPRITE_H, VB_SPRITE,
            flick, 0.05f * sinf(walkT), VB_SPRITE_W / 2, VB_SPRITE_H);

  // Nome e barra inferiore: HP / LEVEL / AP
  c.setTextColor(G);
  c.setCursor(108, 204); c.print("DWELLER");
  c.fillRect(0, 218, 240, 20, BAR);
  c.drawFastHLine(0, 218, 240, GD);
  c.setCursor(6, 224);   c.print("HP 90/90");
  c.setCursor(76, 224);  c.print("LEVEL 1");
  c.drawRect(122, 223, 52, 9, G);
  c.fillRect(124, 225, 14 + (frameNo / 4) % 20, 5, G);  // barra XP che si riempie
  c.setCursor(186, 224); c.print("AP 70/70");

  tft.drawRGBBitmap(0, 0, c.getBuffer(), 240, 240);
  walkT += 0.25f;
  frameNo++;
}

void leaveAnim();

void drawText(const String &s, uint16_t color) {
  tft.fillRect(0, TEXT_Y, 240, TEXT_H, ST77XX_BLACK);
  tft.setTextColor(color);
  tft.setTextSize(s.length() > 10 ? 2 : 3);
  tft.setCursor(4, TEXT_Y + 6);
  tft.setTextWrap(true);
  tft.print(s.substring(0, 36));
}

void drawPlot() {
  plotCanvas.fillScreen(ST77XX_BLACK);
  plotCanvas.drawRect(0, 0, PLOT_W, PLOT_H, 0x4208);  // cornice grigia

  if (count < 1) { tft.drawRGBBitmap(0, PLOT_Y, plotCanvas.getBuffer(), PLOT_W, PLOT_H); return; }

  // scala automatica
  float mn = samples[0], mx = samples[0];
  for (int i = 0; i < count; i++) {
    float v = samples[(head - count + i + N_SAMP) % N_SAMP];
    if (v < mn) mn = v;
    if (v > mx) mx = v;
  }
  if (mx - mn < 1e-6f) { mn -= 1; mx += 1; }

  auto yOf = [&](float v) { return (int)(PLOT_H - 2 - (v - mn) / (mx - mn) * (PLOT_H - 4)); };

  int px = 0, py = 0;
  for (int i = 0; i < count; i++) {
    float v = samples[(head - count + i + N_SAMP) % N_SAMP];
    int x = 1 + i, y = yOf(v);
    if (i > 0) plotCanvas.drawLine(px, py, x, y, ST77XX_GREEN);
    px = x; py = y;
  }

  plotCanvas.setTextSize(1);
  plotCanvas.setTextColor(ST77XX_YELLOW);
  plotCanvas.setCursor(4, 4);          plotCanvas.print(mx);
  plotCanvas.setCursor(4, PLOT_H - 12); plotCanvas.print(mn);

  tft.drawRGBBitmap(0, PLOT_Y, plotCanvas.getBuffer(), PLOT_W, PLOT_H);
}

void addSample(float v) {
  samples[head] = v;
  head = (head + 1) % N_SAMP;
  if (count < N_SAMP) count++;
}

// true se la riga e' un numero valido
bool parseNumber(const String &s, float &out) {
  String t = s;
  t.trim();
  if (t.length() == 0) return false;
  char *end;
  out = strtof(t.c_str(), &end);
  return *end == '\0';
}

void handleLine(const String &s) {
  String t = s;
  t.trim();
  if (t.length() == 0) return;

  if (t.equalsIgnoreCase("anim")) { animMode = true; pipMode = false; return; }
  if (t.equalsIgnoreCase("pip"))  { animMode = true; pipMode = true;  return; }
  if (t.equalsIgnoreCase("intro")) { animMode = true; pipMode = true; playIntro(); return; }
  if (animMode) leaveAnim();

  if (t.equalsIgnoreCase("clear")) {
    count = 0; head = 0;
    drawText("Pulito", ST77XX_CYAN);
    drawPlot();
    return;
  }

  float v;
  if (parseNumber(t, v)) {
    addSample(v);
    drawText(t, ST77XX_YELLOW);
    drawPlot();
  } else {
    drawText(t, ST77XX_WHITE);  // testo libero: solo mostrato, non plottato
  }
}

void leaveAnim() {
  animMode = false;
  tft.fillScreen(ST77XX_BLACK);
  drawPlot();
}

void setup() {
  Serial.begin(115200);
  SPI.setSCK(18);
  SPI.setTX(19);
  pinMode(TFT_BL, OUTPUT);
  digitalWrite(TFT_BL, HIGH);

  tft.init(240, 240, SPI_MODE3);  // se il tuo schermo e' 240x320 cambia in init(240, 320)
  tft.setSPISpeed(40000000);      // default Adafruit = 8 MHz; il ST7789 regge 40-62 MHz
  tft.setRotation(0);
  tft.fillScreen(ST77XX_BLACK);

  playIntro();
  Serial.println("Pip-Boy attivo. Comandi: 'pip' Pip-Boy, 'intro' rivede l'intro, 'anim' sinusoide, numero = grafico, testo = mostra, 'clear' pulisce.");
}

void loop() {
  while (Serial.available()) {
    char c = Serial.read();
    if (c == '\n' || c == '\r') {
      handleLine(line);
      line = "";
    } else if (line.length() < 64) {
      line += c;
    }
  }
  if (animMode) { if (pipMode) drawPipFrame(); else drawAnimFrame(); }
}
