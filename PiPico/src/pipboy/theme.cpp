#include "../common/shared.h"
#include "../common/theme.h"
#include "../common/stats.h"
#include "sprites.h"

namespace {  // tutto interno a questo file: i temi non si disturbano tra loro

// Layout 240x240
const int TEXT_Y  = 0;    // area testo (ultima riga ricevuta)
const int TEXT_H  = 60;
const int PLOT_Y  = 64;   // area grafico
const int PLOT_W  = 240;
const int PLOT_H  = 176;
const int N_SAMP  = PLOT_W - 2;  // un campione per pixel

GFXcanvas16 &plotCanvasRef() { static GFXcanvas16 c(PLOT_W, PLOT_H); return c; }  // allocato solo se serve

float samples[N_SAMP];
int count = 0;  // campioni validi (max N_SAMP)
int head = 0;   // prossima posizione di scrittura

// Animazione: sinusoide verde che scorre. Parte all'avvio; da seriale
// 'anim' la riavvia, qualsiasi altro input passa alla modalita' grafico.
bool animMode = true;
float phase = 0;
GFXcanvas16 &animCanvas = sharedCanvas();

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
bool graphMode = false;  // scheda DATA con il grafico delle statistiche
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

// Barra verticale a segmenti stile Pip-Boy (valore 0-100) con etichetta e percentuale
static void gauge(GFXcanvas16 &c, int x, int y, int w, int h, int val, const char *label, uint16_t col, bool blink) {
  c.setTextSize(1);
  c.setTextColor(col);
  c.setCursor(x + w / 2 - 9, y - 11); c.print(label);
  c.drawRect(x, y, w, h, col);
  int seg = (h - 4) / 6, on = val * seg / 100;
  for (int i = 0; i < on; i++)
    if (!(blink && val >= 90 && i >= seg - 3)) c.fillRect(x + 2, y + h - 2 - (i + 1) * 6 + 2, w - 4, 4, col);
  char b[8];
  snprintf(b, sizeof(b), "%d%%", val);
  c.setCursor(x + w / 2 - (int)strlen(b) * 3, y + h + 5); c.print(b);
}

void drawPipFrame() {
  const uint16_t G   = tft.color565(40, 255, 100);
  const uint16_t GD  = tft.color565(0, 110, 40);
  const uint16_t BG  = tft.color565(0, 10, 3);
  const uint16_t BAR = tft.color565(0, 28, 10);
  GFXcanvas16 &c = animCanvas;
  bool live = pcLive(), alarm = pcAlarm();
  bool blink = (frameNo / 4) % 2 == 0;

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
  if (live) {  // con i dati del PC le tacche laterali diventano indicatori CPU e GPU
    gauge(c, 36, 58, 16, 124, pc.cpu, "CPU", G, blink);
    gauge(c, 188, 58, 16, 124, pc.gpu, "GPU", G, blink);
  } else {
    c.fillRect(62, 96, 18, 3, G);    c.fillRect(160, 96, 18, 3, G);
    c.fillRect(62, 160, 18, 3, G);   c.fillRect(160, 160, 18, 3, G);
  }

  // Vault Boy (sprite ricavato dalla schermata del Pip-Boy) con dondolio da camminata
  float bob = 2.5f * fabsf(sinf(walkT));
  float flick = 0.92f + 0.08f * sinf(frameNo * 0.9f);  // leggero sfarfallio CRT
  if (alarm) flick = blink ? 1.0f : 0.35f;             // sovraccarico: lampeggia
  blitAlpha(c, 120 - VB_SPRITE_W / 2, 46 - (int)bob, VB_SPRITE_W, VB_SPRITE_H, VB_SPRITE,
            flick, 0.05f * sinf(walkT), VB_SPRITE_W / 2, VB_SPRITE_H);

  // Nome e barra inferiore: HP / LEVEL / AP
  c.setTextColor(G);
  char nb[40];
  if (live) {
    if (alarm) strcpy(nb, blink ? "!! OVERLOAD !!" : "");
    else       snprintf(nb, sizeof(nb), "TEMP %dC  DSK %d%%  NET %dK", pc.temp, pc.disk, pc.net);
  } else {
    snprintf(nb, sizeof(nb), "DWELLER");
  }
  c.setCursor((240 - (int)strlen(nb) * 6) / 2, 204); c.print(nb);
  c.fillRect(0, 218, 240, 20, BAR);
  c.drawFastHLine(0, 218, 240, GD);
  if (live) {  // CPU / RAM / GPU al posto di HP / LEVEL / AP
    snprintf(nb, sizeof(nb), "CPU %d%%", pc.cpu); c.setCursor(6, 224);   c.print(nb);
    snprintf(nb, sizeof(nb), "RAM %d%%", pc.ram); c.setCursor(76, 224);  c.print(nb);
    c.drawRect(122, 223, 52, 9, G);
    c.fillRect(124, 225, pc.ram * 48 / 100, 5, G);
    snprintf(nb, sizeof(nb), "GPU %d%%", pc.gpu); c.setCursor(186, 224); c.print(nb);
  } else {
    c.setCursor(6, 224);   c.print("HP 90/90");
    c.setCursor(76, 224);  c.print("LEVEL 1");
    c.drawRect(122, 223, 52, 9, G);
    c.fillRect(124, 225, 14 + (frameNo / 4) % 20, 5, G);  // barra XP che si riempie
    c.setCursor(186, 224); c.print("AP 70/70");
  }

  tft.drawRGBBitmap(0, 0, c.getBuffer(), 240, 240);
  // sotto sforzo l'omino cammina piu' veloce
  walkT += live ? 0.1f + 0.4f * max(pc.cpu, pc.gpu) / 100.0f : 0.25f;
  frameNo++;
}

// Scheda DATA: grafico di una statistica alla volta, scelta con G,<metrica>
void drawPipGraph() {
  const uint16_t G    = tft.color565(40, 255, 100);
  const uint16_t GD   = tft.color565(0, 110, 40);
  const uint16_t BG   = tft.color565(0, 10, 3);
  const uint16_t BAR  = tft.color565(0, 28, 10);
  const uint16_t FILL = tft.color565(0, 60, 22);
  GFXcanvas16 &c = animCanvas;
  int m = curMetric();
  HistStats s = histStats(m);
  int scale = histScale(m);
  char b[40];

  c.fillScreen(BG);
  c.fillRect(0, (frameNo * 3) % 260 - 20, 240, 20, BAR);  // barra di scansione CRT

  // tab in alto con la tacca sotto DATA
  c.setTextSize(1);
  c.setTextColor(G);
  const char *tabs[5] = {"STAT", "INV", "DATA", "MAP", "RADIO"};
  const int tx[5] = {12, 62, 102, 148, 190};
  for (int i = 0; i < 5; i++) { c.setCursor(tx[i], 10); c.print(tabs[i]); }
  c.drawFastHLine(0, 22, 96, G);   c.drawFastVLine(96, 6, 17, G);
  c.drawFastVLine(132, 6, 17, G);  c.drawFastHLine(132, 22, 104, G);

  // sottoschede = metriche, quella mostrata in chiaro
  for (int i = 0; i < MT_COUNT; i++) {
    c.setTextColor(i == m ? G : GD);
    c.setCursor(10 + i * 38, 30);
    c.print(METRIC_SHORT[i]);
  }

  // grafico con tacche sugli assi
  const int GX = 34, GY = 46, GW = HIST_N, GH = 136;
  c.drawFastHLine(GX, GY + GH, GW, G);
  c.drawFastVLine(GX - 1, GY, GH + 1, G);
  for (int x = GX; x <= GX + GW; x += 20) c.drawFastVLine(x, GY + GH, 4, G);
  for (int q = 1; q < 4; q++)
    for (int x = GX; x < GX + GW; x += 4) c.drawPixel(x, GY + GH * q / 4, GD);
  c.setTextColor(G);
  c.setCursor(2, GY);              c.print(scale);
  c.setCursor(2, GY + GH / 2 - 4); c.print(scale / 2);
  c.setCursor(2, GY + GH - 8);     c.print(0);
  if (histCount) histPlot(c, GX, GY, GW, GH, m, G, FILL, scale);
  else { c.setCursor(GX + 30, GY + GH / 2 - 4); c.print("NO DATA - RUN PC_STATS"); }

  snprintf(b, sizeof(b), "%s%s", METRIC_NAME[m], graphAuto ? " (AUTO)" : "");
  c.setCursor((240 - (int)strlen(b) * 6) / 2, 196); c.print(b);

  // barra inferiore: attuale / minimo-massimo / media
  const char *u = METRIC_UNIT[m];
  c.fillRect(0, 218, 240, 20, BAR);
  c.drawFastHLine(0, 218, 240, GD);
  snprintf(b, sizeof(b), "NOW %d%s", s.now, u);           c.setCursor(6, 224);   c.print(b);
  snprintf(b, sizeof(b), "MIN %d MAX %d", s.mn, s.mx);    c.setCursor(74, 224);  c.print(b);
  snprintf(b, sizeof(b), "AVG %d%s", s.avg, u);           c.setCursor(180, 224); c.print(b);

  tft.drawRGBBitmap(0, 0, c.getBuffer(), 240, 240);
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
  GFXcanvas16 &plotCanvas = plotCanvasRef();
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

  if (t.startsWith("M,")) {  // schermate scelte dalla GUI
    String m = t.substring(2);
    Serial.println("OK " + t);
    if (m == "pip")        { animMode = true; pipMode = true;  graphMode = false; }
    else if (m == "anim")  { animMode = true; pipMode = false; graphMode = false; }
    else if (m == "graph") { animMode = true; graphMode = true; }
    else if (m == "boot")  { animMode = true; pipMode = true;  graphMode = false; playIntro(); }
    else if (m == "term" && animMode) leaveAnim();
    return;
  }
  if (t.equalsIgnoreCase("anim"))  { animMode = true; pipMode = false; graphMode = false; return; }
  if (t.equalsIgnoreCase("pip"))   { animMode = true; pipMode = true;  graphMode = false; return; }
  if (t.equalsIgnoreCase("graph")) { animMode = true; graphMode = true; return; }
  if (t.equalsIgnoreCase("intro")) { animMode = true; pipMode = true; graphMode = false; playIntro(); return; }
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

// ---------- Interfaccia del tema ----------
// Schermate: 0 STAT, 1 grafico (scheda DATA), 2 sinusoide, 3 grafico seriale / testo
static int curScreen() { return !animMode ? 3 : graphMode ? 1 : pipMode ? 0 : 2; }
static void thSetScreen(int n) {
  if (n == 3) { if (animMode) leaveAnim(); return; }
  animMode = true;
  graphMode = n == 1;
  pipMode = n != 2;
}
static void thBegin() {
  tft.fillScreen(ST77XX_BLACK);
  playIntro();
  if (!animMode) { tft.fillScreen(ST77XX_BLACK); drawPlot(); }
}
static void thTick() {
  if (animMode) { if (graphMode) drawPipGraph(); else if (pipMode) drawPipFrame(); else drawAnimFrame(); }
}
static void thGetState(ThemeState &s) { s.flags = 0; s.screen = (uint8_t)curScreen(); s.name[0] = 0; }
static void thSetState(const ThemeState &s) { thSetScreen(min<int>(s.screen, 3)); }
static void thNext() { thSetScreen((curScreen() + 1) % 4); }
static void thReboot() { animMode = true; pipMode = true; graphMode = false; playIntro(); }
}  // namespace

const Theme THEME_PIPBOY = {"pipboy", thBegin, thTick, handleLine, thGetState, thSetState, thNext, thReboot};
