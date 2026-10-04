// Visione del T-800 / terminale Skynet (Terminator): intro con logo, HUD con mirino,
// codice 6502 che scorre, analisi e "possible responses", statistiche del PC e
// terminale seriale. Ogni elemento si accende/spegne da seriale (o da tools/display_gui.py):
//   C,<maschera>   elementi visibili (bit E_* qui sotto)
//   M,<schermata>  hud | term | graph | boot
//   N,<nome>       nome del bersaglio
//   G,<metrica>    grafico: cpu | ram | dsk | gpu | tmp | net | auto (schermata M,graph)
//   ?              il Pico risponde "ID,skynet"
#include "../common/shared.h"
#include "../common/theme.h"
#include "../common/stats.h"
#include "logo.h"

namespace {  // tutto interno a questo file: i temi non si disturbano tra loro
GFXcanvas16 &cv = sharedCanvas();  // disegno fuori schermo, poi un solo blit (niente flicker)

uint16_t RED, DIM, BGC, BAR, WHT;
uint16_t VIG[6];           // vignettatura rossa dal bordo (0) al centro (5)
uint8_t vigW[5][240];      // semi-larghezza di ogni anello, riga per riga

// ---------- Elementi visibili (le costanti sono E_* perche' F_CPU e' gia' una macro di Arduino) ----------
enum : uint32_t {
  E_HEADER    = 1u << 0,   // CYBERDYNE SYSTEMS / MODEL 101
  E_DATACOL   = 1u << 1,   // colonna a sinistra: codice 6502 (offline) o statistiche (con dati)
  E_CPU       = 1u << 2,
  E_RAM       = 1u << 3,
  E_DSK       = 1u << 4,
  E_GPU       = 1u << 5,
  E_INFO      = 1u << 6,   // TMP / NET / UP
  E_RETICLE   = 1u << 7,   // mirino
  E_STATUS    = 1u << 8,   // pannello in basso
  E_ANALYSIS  = 1u << 9,   // testo di analisi in alto a destra
  E_RESPONSES = 1u << 10,  // riquadro "POSSIBLE RESPONSES"
  E_GRID      = 1u << 11,  // griglia a punti
  E_SCANBAR   = 1u << 12,
  E_NOISE     = 1u << 13,  // pixel di disturbo e righe di glitch
  E_VIGNETTE  = 1u << 14,  // visione rossa: centro chiaro, bordi scuri
  E_SCANLINES = 1u << 15,
  E_TYPEON    = 1u << 16,  // scrittura progressiva del testo
  E_BIGSTATS  = 1u << 17,  // statistiche a caratteri grandi
};
const uint32_t ALL_FLAGS = (1u << 18) - 1;
uint32_t flags = ALL_FLAGS;
static bool on(uint32_t f) { return (flags & f) != 0; }

enum Mode { MODE_HUD, MODE_TERM, MODE_GRAPH };
Mode mode = MODE_HUD;
int frameNo = 0;
float rp1 = 0, rp2 = 0;  // fasi del mirino (la velocita' dipende dal carico)
String targetName = "SAMU";

// ---------- Utilita' ----------
static uint32_t rnd(uint32_t x) {  // xorshift: numeri pseudo-casuali ripetibili
  x ^= x << 13; x ^= x >> 17; x ^= x << 5;
  return x;
}

static void typeText(const char *s, int x, int y, uint8_t size, uint16_t col, int ms) {
  tft.setTextSize(size);
  tft.setTextColor(col);
  tft.setCursor(x, y);
  for (const char *p = s; *p; p++) { tft.print(*p); delay(ms); }
}

// Testo con scrittura progressiva: al massimo 'budget' caratteri per fotogramma
int revealed = 0, budget = 0;
static void resetType() { revealed = 0; }
static void tprint(int x, int y, const char *s, uint8_t size = 1, uint16_t col = 0) {
  if (!col) col = RED;
  int n = strlen(s);
  if (budget <= 0) return;
  int k = min(n, budget);
  budget -= n;
  for (int i = 0; i < k; i++) cv.drawChar(x + i * 6 * size, y, s[i], col, col, size);
  if (k < n) cv.fillRect(x + k * 6 * size, y, 5 * size, 7 * size, col);
}

// ---------- Effetti ----------
void initVignette() {
  for (int k = 0; k < 5; k++) {
    float ax = 180 - k * 27, ay = 170 - k * 27;
    for (int y = 0; y < 240; y++) {
      float dy = (y - 115) / ay;
      float w = dy * dy < 1 ? ax * sqrtf(1 - dy * dy) : 0;
      vigW[k][y] = (uint8_t)min(w, 120.0f);
    }
  }
}

static void drawBackground() {
  if (on(E_VIGNETTE)) {
    cv.fillScreen(VIG[0]);
    for (int k = 0; k < 5; k++)
      for (int y = 0; y < 240; y++)
        if (vigW[k][y]) cv.drawFastHLine(120 - vigW[k][y], y, vigW[k][y] * 2, VIG[k + 1]);
  } else {
    cv.fillScreen(BGC);
  }
  if (on(E_SCANBAR)) cv.fillRect(0, (frameNo * 3) % 260 - 20, 240, 20, BAR);
}

static void applyScanlines() {  // righe dispari al 75% di luminosita'
  uint16_t *b = cv.getBuffer();
  for (int y = 1; y < 240; y += 2) {
    uint16_t *p = b + y * 240;
    for (int x = 0; x < 240; x++) {
      uint16_t c = p[x];
      p[x] = ((c >> 1) & 0x7BEF) + ((c >> 2) & 0x39E7);
    }
  }
}

// Logo Skynet (maschera alpha a 8 bit) in rosso, con luminosita' k e glitch a righe opzionale
static void drawLogo(float k, bool glitch) {
  cv.fillScreen(ST77XX_BLACK);
  int x0 = (240 - SKYNET_LOGO_W) / 2, y0 = (240 - SKYNET_LOGO_H) / 2;
  for (int y = 0; y < SKYNET_LOGO_H; y++) {
    int off = (glitch && random(0, 10) == 0) ? random(-12, 13) : 0;
    for (int x = 0; x < SKYNET_LOGO_W; x++) {
      uint8_t a = SKYNET_LOGO[y * SKYNET_LOGO_W + x];
      if (a < 8) continue;
      float f = a / 255.0f * k;
      cv.drawPixel(x0 + x + off, y0 + y, tft.color565(255 * f, 40 * f, 30 * f));
    }
  }
  blitCanvas(cv);
}

// ---------- Intro ----------
void playIntro() {
  tft.fillScreen(ST77XX_BLACK);

  // accensione: lampi rossi
  for (int i = 0; i < 7; i++) {
    tft.fillScreen(tft.color565(random(20, 110), 0, 0));
    delay(35);
    tft.fillScreen(ST77XX_BLACK);
    delay(25);
  }

  const char *lines[] = {
    "CYBERDYNE SYSTEMS CORPORATION",
    "SKYNET NEURAL NET PROCESSOR",
    "MODEL 101  SERIES 800",
    "",
    "CPU: 6502 ............... OK",
    "MEMORY: 256 KB .......... OK",
    "VISUAL SENSOR ........... OK",
    "LOADING MISSION PARAMS .. OK",
    "",
    "> SYSTEM ACTIVATED",
    "> SKYNET BECOMES SELF-AWARE",
    "> 29 AUG 1997  02:14 EDT",
  };
  int y = 8;
  for (const char *l : lines) { typeText(l, 6, y, 1, RED, 12); y += 12; }
  delay(800);

  // logo Skynet: dissolvenza, glitch, poi fisso
  for (int k = 1; k <= 16; k++) { drawLogo(k / 16.0f, k > 10); delay(30); }
  for (int i = 0; i < 6; i++) { drawLogo(1.0f, true); delay(60); }
  drawLogo(1.0f, false);
  delay(1800);
  frameNo = 0;
  resetType();
}

// ---------- HUD ----------
static void drawReticle(int cx, int cy, int r, uint16_t col) {
  cv.drawCircle(cx, cy, r, col);
  cv.drawCircle(cx, cy, r - 1, col);
  cv.drawFastHLine(cx - r - 10, cy, 16, col);  cv.drawFastHLine(cx + r - 6, cy, 16, col);
  cv.drawFastVLine(cx, cy - r - 10, 16, col);  cv.drawFastVLine(cx, cy + r - 6, 16, col);
  int R = r + 14, L = 9;  // staffe d'angolo
  cv.drawFastHLine(cx - R, cy - R, L, col); cv.drawFastVLine(cx - R, cy - R, L, col);
  cv.drawFastHLine(cx + R - L + 1, cy - R, L, col); cv.drawFastVLine(cx + R, cy - R, L, col);
  cv.drawFastHLine(cx - R, cy + R, L, col); cv.drawFastVLine(cx - R, cy + R - L + 1, L, col);
  cv.drawFastHLine(cx + R - L + 1, cy + R, L, col); cv.drawFastVLine(cx + R, cy + R - L + 1, L, col);
}

// Codice assembly 6502 (quello che si vede nella visione del T-800 nel film)
const char *ASM[] = {
  "LDA #$00", "STA $D020", "LDX #$FF", "TXS", "JSR $FDED", "LDA $C000", "BPL $F8",
  "STA $C010", "CMP #$8D", "BEQ $0A", "INX", "CPX #$28", "BNE $F3", "LDY #$00",
  "LDA ($06),Y", "EOR #$FF", "STA ($08),Y", "INY", "BNE $F7", "RTS", "JMP $FA62",
  "SEI", "CLD", "ADC #$30", "ROL A", "BIT $C030", "DEY", "BMI $E2",
};
const int NASM = sizeof(ASM) / sizeof(ASM[0]);

static void drawAsmColumn(int w) {
  int yoff = (frameNo % 4) * 10 / 4, sc = frameNo / 4;
  char b[24];
  for (int i = 0; i < 18; i++) {
    int y = 22 + i * 10 - yoff;
    if (y < 20) continue;
    uint32_t v = rnd((sc + i) * 2654435761u + 7u);
    snprintf(b, sizeof(b), "%02X %s", (unsigned)(v & 0xFF), ASM[v % NASM]);
    b[w / 6] = 0;  // taglia alla larghezza della colonna
    tprint(4, y, b, 1, (v >> 8) % 5 == 0 ? RED : DIM);
  }
}

static int statsColumn(bool big) {  // restituisce la y finale
  const char *lab[4] = {"CPU", "RAM", "DSK", "GPU"};
  int val[4] = {pc.cpu, pc.ram, pc.disk, pc.gpu};
  const uint32_t fl[4] = {E_CPU, E_RAM, E_DSK, E_GPU};
  bool blink = (frameNo / 4) % 2 == 0;
  char b[20];
  int y = 24, barW = big ? 114 : 70;
  for (int i = 0; i < 4; i++) {
    if (!on(fl[i])) continue;
    snprintf(b, sizeof(b), "%s %3d%%", lab[i], val[i]);
    tprint(4, y, b, big ? 2 : 1, val[i] >= 90 && !blink ? DIM : RED);
    int by = y + (big ? 17 : 10);
    if (budget > 0) {
      cv.drawRect(4, by, barW, big ? 7 : 5, DIM);
      cv.fillRect(5, by + 1, val[i] * (barW - 2) / 100, big ? 5 : 3, RED);
    }
    y += big ? 27 : 18;
  }
  if (on(E_INFO)) {
    uint8_t sz = big ? 2 : 1;
    int step = big ? 20 : 12;
    snprintf(b, sizeof(b), "TMP %3dC", pc.temp);
    tprint(4, y, b, sz, pc.temp >= 85 && !blink ? DIM : RED); y += step;
    if (pc.net >= 1000) snprintf(b, sizeof(b), "NET %d.%dM", pc.net / 1000, (pc.net % 1000) / 100);
    else                snprintf(b, sizeof(b), "NET %3dK", pc.net);
    tprint(4, y, b, sz); y += step;
    snprintf(b, sizeof(b), "UP  %dH", pc.up);
    tprint(4, y, b, sz); y += step;
  }
  return y;
}

// Testo di analisi in alto a destra: gruppi di righe battute una lettera alla volta
static void drawAnalysis(int left, bool live) {
  const int SETS = 5, LINES = 4, PERIOD = 160;
  int set = (frameNo / PERIOD) % SETS, t = frameNo % PERIOD;
  char l[LINES][28];
  if (live) {
    int load = max(pc.cpu, pc.gpu);
    const char *lvl = pcAlarm() ? "CRITICAL" : load < 40 ? "LOW" : load < 75 ? "MODERATE" : "HIGH";
    const char *s[SETS][LINES] = {
      {"SYSTEM ANALYSIS:", "", "", ""}, {"THERMAL SCAN", "", "", ""},
      {"MEMORY ASSESSMENT", "", "", ""}, {"THREAT ASSESSMENT", "", "", ""}, {"NETWORK ANALYSIS", "", "", ""}};
    for (int i = 0; i < LINES; i++) strcpy(l[i], s[set][i]);
    if (set == 0) { snprintf(l[1], 28, "CPU LOAD %d%%", pc.cpu); snprintf(l[2], 28, "GPU LOAD %d%%", pc.gpu); strcpy(l[3], "ASSESSMENT COMPLETE"); }
    if (set == 1) {
      snprintf(l[1], 28, "GPU CORE %dC", pc.temp);
      if (extraLive() && pcx.cpuTemp > 0) snprintf(l[2], 28, "CPU CORE %dC", pcx.cpuTemp);
      strcpy(l[3], pc.temp >= 85 ? "OVERHEAT DETECTED" : "WITHIN PARAMETERS");
    }
    if (set == 2) { snprintf(l[1], 28, "RAM IN USE %d%%", pc.ram); snprintf(l[2], 28, "DISK %d%%", pc.disk); }
    if (set == 3) {
      snprintf(l[1], 28, "LEVEL: %s", lvl);
      if (extraLive() && pcx.proc[0]) { snprintf(l[2], 28, "TARGET: %s", pcx.proc); snprintf(l[3], 28, "CPU USE %d%%", pcx.procPct); }
      else strcpy(l[2], "MISSION: MONITOR HOST");
    }
    if (set == 4) {
      char r1[8], r2[8];
      fmtRate(r1, sizeof(r1), netExtraLive() ? pcx.down : pc.net); fmtRate(r2, sizeof(r2), netExtraLive() ? pcx.up : 0);
      snprintf(l[1], 28, "DOWN %s  UP %s", r1, r2);
      if (netExtraLive() && pcx.ping >= 0) snprintf(l[2], 28, "LATENCY %d MS", pcx.ping); else strcpy(l[2], "LATENCY UNKNOWN");
      if (netExtraLive()) snprintf(l[3], 28, "PROCESSES %d", pcx.procs);
    }
  } else {
    const char *s[SETS][LINES] = {
      {"ANALYSIS:", "SCAN MODE 43984", "SIZE ASSESSMENT", "ASSESSMENT COMPLETE"},
      {"MATCH SEARCH:", "PATTERN 0.86 ACCEPTED", "FIT PROBABILITY 0.99", ""},
      {"THREAT ASSESSMENT", "SUBJECT NOT ARMED", "PRIORITY: LOW", ""},
      {"VISUAL OVERRIDE", "SYSTEM SCAN", "ENVIRONMENT: INDOOR", "AUDIO INPUT ACTIVE"},
      {"NETWORK SCAN:", "HOST LINK: NONE", "LATENCY: UNKNOWN", ""}};
    for (int i = 0; i < LINES; i++) strcpy(l[i], s[set][i]);
  }
  int chars = t * 2;  // due lettere per fotogramma
  for (int i = 0; i < LINES && chars > 0; i++) {
    int n = strlen(l[i]);
    int k = min(n, chars);
    chars -= n;
    int x = 236 - n * 6;
    if (x < left) x = left;
    for (int j = 0; j < k; j++) cv.drawChar(x + j * 6, 24 + i * 10, l[i][j], WHT, WHT, 1);
  }
}

// Riquadro "POSSIBLE RESPONSES": compare ogni tanto, scorre le voci e ne sceglie una
static void drawResponses(bool live) {
  const int PERIOD = 640, SHOW = 170;
  int t = frameNo % PERIOD;
  if (t >= SHOW) return;
  const char *off[5]   = {"YES/NO", "OR WHAT?", "GO AWAY", "PLEASE COME BACK LATER", "I'LL BE BACK"};
  const char *norm[5]  = {"CONTINUE MONITORING", "RUN DIAGNOSTICS", "IGNORE", "LOG STATUS", "I'LL BE BACK"};
  const char *alarm[5] = {"REDUCE LOAD", "CLOSE PROCESSES", "INCREASE COOLING", "TERMINATE TASK", "I'LL BE BACK"};
  const char **items = !live ? off : pcAlarm() ? alarm : norm;
  int choice = rnd(frameNo / PERIOD + 99) % 5;
  int sel = min(t / 22, choice);  // la selezione scende fino alla risposta scelta
  bool chosen = t / 22 >= choice;
  int x = 92, y = 120, w = 144, h = 70;
  cv.fillRect(x, y, w, h, BGC);
  cv.drawRect(x, y, w, h, RED);
  for (int c = 0; c < 19 && c < t; c++) cv.drawChar(x + 4 + c * 6, y + 4, "POSSIBLE RESPONSES:"[c], WHT, WHT, 1);
  for (int i = 0; i < 5; i++) {
    int iy = y + 16 + i * 10;
    if (t < 20 + i * 4) break;  // le voci compaiono una alla volta
    bool hl = i == sel && (!chosen || (frameNo / 4) % 2 == 0);
    if (hl) cv.fillRect(x + 2, iy - 1, w - 4, 10, RED);
    uint16_t col = hl ? BGC : RED;
    for (int c = 0; items[i][c]; c++) cv.drawChar(x + 4 + c * 6, iy, items[i][c], col, col, 1);
  }
}

void drawHudFrame() {
  bool live = pcLive(), alarm = pcAlarm();
  bool blink = (frameNo / 4) % 2 == 0;
  bool blinkOn = (frameNo / 8) % 2 == 0;  // lampeggio lento di TARGET ACQUIRED
  bool big = on(E_BIGSTATS);
  char buf[32];

  if (on(E_HEADER)) {
    tprint(6, 6, "CYBERDYNE SYSTEMS");
    tprint(168, 6, "MODEL 101");
    if (budget > 0) cv.drawFastHLine(0, 18, 240, RED);
  }
  cv.drawRect(0, 0, 240, 240, DIM);

  // colonna a sinistra: statistiche (con dati) oppure codice 6502
  int split = 0;
  if (on(E_DATACOL)) {
    split = live && big ? 122 : 80;
    if (live) statsColumn(big);
    else      drawAsmColumn(split - 6);
    cv.drawFastVLine(split, 20, 176, DIM);
  }

  if (on(E_GRID))
    for (int x = split + 10; x < 236; x += 16)
      for (int y = 28; y < 192; y += 16) cv.drawPixel(x, y, DIM);

  // mirino: scansione -> aggancio -> bersaglio acquisito
  int t = frameNo % 420;
  int zw = 236 - split;
  float rBig = constrain(zw / 2 - 36, 12, 30), rSmall = rBig * 0.6f;
  float amp, r;
  const char *status;
  int match;
  if (t < 240)      { amp = 1.0f; r = rBig + 3 * sinf(frameNo * 0.2f); status = "SCANNING";        match = 8 + (int)(rnd(frameNo / 4) % 30); }
  else if (t < 340) { float k = (t - 240) / 100.0f; amp = 1.0f - k; r = rBig - (rBig - rSmall) * k; status = "LOCKING"; match = 40 + (int)(58 * k); }
  else              { amp = 0; r = rSmall; status = "TARGET ACQUIRED"; match = 98; }
  if (on(E_RETICLE)) {
    float sp = live ? 1.0f + max(pc.cpu, pc.gpu) / 40.0f : 1.0f;  // piu' carico = mirino piu' nervoso
    rp1 += 0.045f * sp; rp2 += 0.07f * sp;
    int ax = max(0, zw / 2 - (int)rBig - 20);
    int cx = split + zw / 2 + (int)(amp * ax * sinf(rp1));
    int cy = 112 + (int)(amp * 22 * sinf(rp2 + 1));
    drawReticle(cx, cy, (int)r, (t >= 340 && !blinkOn) ? DIM : RED);
  }

  if (on(E_ANALYSIS)) drawAnalysis(split + 4, live);

  // pannello inferiore
  if (on(E_STATUS)) {
    if (budget > 0) cv.drawFastHLine(0, 198, 240, RED);
    tprint(6, 206, "STATUS: ");
    tprint(54, 206, status, 1, t >= 340 && !blinkOn ? DIM : 0);
    if (live) {
      int load = max(pc.cpu, pc.gpu);
      const char *lvl = alarm ? "CRITICAL" : load < 40 ? "LOW" : load < 75 ? "MODERATE" : "HIGH";
      snprintf(buf, sizeof(buf), "LOAD: %d%%", load);          tprint(6, 218, buf);
      snprintf(buf, sizeof(buf), "GPU TEMP: %dC", pc.temp);    tprint(130, 218, buf);
      snprintf(buf, sizeof(buf), "THREAT: %s", lvl);           tprint(6, 228, buf, 1, alarm && !blink ? DIM : 0);
      if (alarm && blink) cv.drawRect(2, 2, 236, 236, RED);
    } else {
      snprintf(buf, sizeof(buf), "MATCH: %d.%d%%", match, (int)(rnd(frameNo / 2) % 10));
      tprint(6, 218, buf);
      snprintf(buf, sizeof(buf), "RANGE: %d.%d M", 20 - t / 30, (int)(rnd(frameNo / 5) % 10));
      tprint(130, 218, buf);
      if (t >= 340) { snprintf(buf, sizeof(buf), "TARGET: %s", targetName.c_str()); tprint(6, 228, buf); }
      else          tprint(6, 228, "THREAT: ANALYZING");
    }
  }

  if (on(E_RESPONSES)) drawResponses(live);

  if (on(E_NOISE)) {
    for (int i = 0; i < 6; i++) cv.drawPixel(random(0, 240), random(20, 196), RED);
    if (frameNo % 97 < 2) cv.fillRect(0, random(20, 190), 240, 2, DIM);
  }
}

// ---------- Grafico delle statistiche ----------
void drawGraph() {
  int m = curMetric();
  HistStats s = histStats(m);
  int scale = histScale(m);
  bool blink = (frameNo / 4) % 2 == 0;
  char b[40];

  if (on(E_HEADER)) {
    snprintf(b, sizeof(b), "TELEMETRY // %s", METRIC_NAME[m]);
    tprint(6, 6, b);
    tprint(186, 6, "MODEL 101");
    if (budget > 0) cv.drawFastHLine(0, 18, 240, RED);
  }
  cv.drawRect(0, 0, 240, 240, DIM);

  // selettore della metrica
  for (int i = 0; i < MT_COUNT; i++) {
    int x = 6 + i * 33;
    if (i == m && budget > 0) cv.fillRect(x - 3, 22, 24, 11, RED);
    tprint(x, 24, METRIC_SHORT[i], 1, i == m ? BGC : DIM);
  }

  // grafico
  const int GX = 34, GY = 40, GW = HIST_N, GH = 120;
  if (budget > 0) {
    cv.drawRect(GX - 1, GY - 1, GW + 2, GH + 2, DIM);
    if (on(E_GRID))
      for (int gx = GX; gx <= GX + GW; gx += 20)
        for (int gy = GY; gy <= GY + GH; gy += 20) cv.drawPixel(gx, gy, DIM);
  }
  snprintf(b, sizeof(b), "%d", scale);     tprint(2, GY, b, 1, DIM);
  snprintf(b, sizeof(b), "%d", scale / 2); tprint(2, GY + GH / 2 - 4, b, 1, DIM);
  tprint(2, GY + GH - 8, "0", 1, DIM);
  if (!histCount) {
    tprint(GX + 48, GY + GH / 2 - 4, "NO TELEMETRY DATA");
  } else if (budget > 0) {
    histPlot(cv, GX, GY, GW, GH, m, RED, tft.color565(50, 4, 4), scale);
    if (on(E_RETICLE)) {  // mirino sull'ultimo valore, con linea tratteggiata fino alla scala
      int ly = GY + GH - 1 - (int)((long)min(s.now, scale) * (GH - 1) / scale), lx = GX + GW - 6;
      for (int x = GX; x < lx - 6; x += 4) cv.drawPixel(x, ly, WHT);
      cv.drawCircle(lx, ly, 5, WHT);
      cv.drawFastHLine(lx - 9, ly, 5, WHT);
      cv.drawFastVLine(lx, ly - 9, 5, WHT);
      cv.drawFastVLine(lx, ly + 5, 5, WHT);
    }
  }

  // pannello inferiore
  if (on(E_STATUS)) {
    const char *u = METRIC_UNIT[m];
    if (budget > 0) cv.drawFastHLine(0, 168, 240, RED);
    snprintf(b, sizeof(b), "CURRENT: %d%s", s.now, u); tprint(6, 176, b);
    snprintf(b, sizeof(b), "PEAK: %d%s", s.mx, u);     tprint(130, 176, b);
    snprintf(b, sizeof(b), "MEAN: %d%s", s.avg, u);    tprint(6, 188, b);
    snprintf(b, sizeof(b), "SAMPLES: %d", histCount);  tprint(130, 188, b);
    int tr = histTrend(m);
    bool hot = (m <= MT_GPU && s.now >= 90) || (m == MT_TMP && s.now >= 85);
    snprintf(b, sizeof(b), "ANALYSIS: TREND %s", tr > 0 ? "RISING" : tr < 0 ? "FALLING" : "STABLE");
    tprint(6, 204, b, 1, WHT);
    if (hot) tprint(6, 216, "WARNING: CRITICAL VALUE", 1, blink ? RED : DIM);
    else if (graphAuto) tprint(6, 216, "MODE: AUTO CYCLE");
    else { snprintf(b, sizeof(b), "MODE: LOCKED ON %s", METRIC_SHORT[m]); tprint(6, 216, b); }
    snprintf(b, sizeof(b), "TARGET: %s", targetName.c_str());
    tprint(6, 228, b, 1, DIM);
  }
}

// ---------- Terminale seriale ----------
const int LOG_N = 12;
String logLines[LOG_N];
int logCount = 0;
const int N_SAMP = 236;
float samples[N_SAMP];
int sCount = 0, sHead = 0;

void addLog(String s) {
  while (s.length() > 0) {  // va a capo a 38 caratteri
    String part = s.substring(0, 38);
    s = s.length() > 38 ? s.substring(38) : "";
    if (logCount == LOG_N) {
      for (int i = 1; i < LOG_N; i++) logLines[i - 1] = logLines[i];
      logCount--;
    }
    logLines[logCount++] = part;
  }
}

void drawTerm() {
  if (on(E_HEADER)) {
    tprint(6, 6, "SKYNET TERMINAL // CYBERDYNE");
    if (budget > 0) cv.drawFastHLine(0, 18, 240, RED);
  }
  cv.drawRect(0, 0, 240, 240, DIM);
  int y = 24;
  for (int i = 0; i < logCount; i++) { tprint(6, y, logLines[i].c_str()); y += 10; }
  if (budget > 0 && (frameNo / 12) % 2 == 0) cv.fillRect(6, y, 6, 8, RED);

  // grafico dei numeri ricevuti
  const int PY = 156, PH = 78;
  cv.drawFastHLine(0, PY - 4, 240, DIM);
  cv.drawRect(2, PY, 236, PH, DIM);
  if (sCount > 0) {
    float mn = 1e30f, mx = -1e30f;
    for (int i = 0; i < sCount; i++) {
      float v = samples[(sHead - sCount + i + N_SAMP) % N_SAMP];
      if (v < mn) mn = v;
      if (v > mx) mx = v;
    }
    if (mx - mn < 1e-6f) { mn -= 1; mx += 1; }
    int px = 0, py = 0;
    for (int i = 0; i < sCount; i++) {
      float v = samples[(sHead - sCount + i + N_SAMP) % N_SAMP];
      int x = 3 + i, yv = (int)(PY + PH - 3 - (v - mn) / (mx - mn) * (PH - 6));
      if (i > 0) cv.drawLine(px, py, x, yv, RED);
      px = x; py = yv;
    }
    cv.setTextColor(DIM);
    cv.setTextSize(1);
    cv.setCursor(6, PY + 3);       cv.print(mx);
    cv.setCursor(6, PY + PH - 11); cv.print(mn);
  }
}

// ---------- Fotogramma ----------
void renderFrame() {
  budget = on(E_TYPEON) ? revealed : (1 << 30);
  if (revealed < 1000000) revealed += 14;
  drawBackground();
  if (mode == MODE_HUD) drawHudFrame();
  else if (mode == MODE_GRAPH) drawGraph();
  else drawTerm();
  if (on(E_SCANLINES)) applyScanlines();
  alertOverlay(cv, RED, frameNo);
  blitCanvas(cv);
  frameNo++;
}

// ---------- Comandi seriali ----------
bool parseNumber(const String &s, float &out) {
  String t = s;
  t.trim();
  if (t.length() == 0) return false;
  char *end;
  out = strtof(t.c_str(), &end);
  return *end == '\0';
}

static void setMode(Mode m) { if (m != mode) { mode = m; resetType(); } }

void handleLine(const String &s) {
  String t = s;
  t.trim();
  if (t.length() == 0) return;

  if (t.startsWith("C,")) { flags = strtoul(t.c_str() + 2, nullptr, 10); Serial.println("OK " + t); return; }
  if (t.startsWith("N,")) { targetName = t.substring(2); targetName.trim(); targetName.toUpperCase(); Serial.println("OK " + t); return; }
  if (t.startsWith("M,")) {
    String m = t.substring(2);
    if (m == "hud") setMode(MODE_HUD);
    else if (m == "term") setMode(MODE_TERM);
    else if (m == "graph") setMode(MODE_GRAPH);
    else if (m == "boot") { playIntro(); }
    Serial.println("OK " + t);
    return;
  }
  if (t.equalsIgnoreCase("hud"))   { setMode(MODE_HUD); return; }
  if (t.equalsIgnoreCase("graph")) { setMode(MODE_GRAPH); return; }
  if (t.equalsIgnoreCase("intro")) { playIntro(); setMode(MODE_HUD); return; }

  setMode(MODE_TERM);
  if (t.equalsIgnoreCase("clear")) {
    logCount = 0; sCount = 0; sHead = 0;
    addLog("> MEMORY CLEARED");
    return;
  }

  float v;
  if (parseNumber(t, v)) {
    samples[sHead] = v;
    sHead = (sHead + 1) % N_SAMP;
    if (sCount < N_SAMP) sCount++;
    addLog("> DATA: " + t);
  } else {
    t.toUpperCase();
    addLog("> " + t);
  }
}

// ---------- Interfaccia del tema ----------
static void thBegin() {
  RED = tft.color565(255, 40, 30);
  DIM = tft.color565(110, 15, 10);
  BGC = tft.color565(14, 0, 0);
  BAR = tft.color565(40, 0, 0);
  WHT = tft.color565(255, 190, 180);  // testo di analisi: bianco rosato come nel film
  for (int k = 0; k < 6; k++) VIG[k] = tft.color565(6 + k * 8, k / 2, k / 3);
  initVignette();
  randomSeed(micros());
  if (!logCount) { addLog("> SKYNET ONLINE"); addLog("> AWAITING INPUT"); }
  playIntro();
}
static void thTick() { renderFrame(); }
static void thGetState(ThemeState &s) {
  s.flags = flags;
  s.screen = (uint8_t)mode;
  strncpy(s.name, targetName.c_str(), 16);
  s.name[16] = 0;
}
static void thSetState(const ThemeState &s) {
  if (s.flags) flags = s.flags & ALL_FLAGS;
  mode = (Mode)min<int>(s.screen, MODE_GRAPH);
  if (s.name[0]) targetName = s.name;
}
static void thNext() { setMode((Mode)((mode + 1) % 3)); }  // HUD, TERM, GRAPH
static void thReboot() { playIntro(); }
}  // namespace

const Theme THEME_SKYNET = {"skynet", thBegin, thTick, handleLine, thGetState, thSetState, thNext, thReboot};
