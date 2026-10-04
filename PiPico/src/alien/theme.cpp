// Interfaccia MU-TH-UR 6000 della Nostromo (Alien): testo ambra su monitor a tubo catodico, boot
// Weyland-Yutani, stato della nave (le statistiche del PC), motion tracker M314 e conversazione
// con il computer di bordo. Ogni elemento si accende/spegne da seriale (o da tools/display_gui.py):
//   C,<maschera>   elementi visibili (bit E_* qui sotto)
//   M,<schermata>  auto | system | tracker | term | graph | boot
//   N,<nome>       nome dell'ufficiale ("RIPLEY")
#include "../common/shared.h"
#include "../common/theme.h"
#include "../common/stats.h"
#include "logo.h"

namespace {  // tutto interno a questo file: i temi non si disturbano tra loro

GFXcanvas16 &cv = sharedCanvas();

uint16_t A, AM, AD, INV, BAND;  // ambra: pieno, medio, scuro, testo in negativo, fascia di scansione
uint16_t GLOW[6];               // bagliore dal bordo (0) al centro (5)
uint8_t glowW[5][240];          // semi-larghezza di ogni anello di bagliore, riga per riga

// ---------- Elementi visibili (E_* perche' F_CPU e' gia' una macro di Arduino) ----------
enum : uint32_t {
  E_HEADER    = 1u << 0,   // intestazione Weyland-Yutani
  E_TITLE     = 1u << 1,   // titolo della sezione + linea ====
  E_CPU       = 1u << 2,   // REACTOR
  E_RAM       = 1u << 3,   // LIFE SUPPORT
  E_DSK       = 1u << 4,   // CARGO HOLD
  E_GPU       = 1u << 5,   // MAIN DRIVE
  E_INFO      = 1u << 6,   // temperatura, comunicazioni, ibernazione
  E_LOG       = 1u << 7,   // righe di conversazione / stato
  E_CURSOR    = 1u << 8,   // prompt con cursore a blocco
  E_SCANLINES = 1u << 9,
  E_SCANBAR   = 1u << 10,
  E_GLOW      = 1u << 11,
  E_FLICKER   = 1u << 12,
  E_TYPEON    = 1u << 13,  // scrittura progressiva del testo
  E_LOGO      = 1u << 14,  // logo Weyland-Yutani nel boot
  E_BLIPS     = 1u << 15,  // contatti sul motion tracker
};
const uint32_t ALL_FLAGS = (1u << 16) - 1;
uint32_t flags = ALL_FLAGS;
bool on(uint32_t f) { return (flags & f) != 0; }

enum Screen { SCR_AUTO, SCR_SYSTEM, SCR_TRACKER, SCR_TERM, SCR_GRAPH };
Screen screenSel = SCR_AUTO;
Screen shown = SCR_TRACKER;
bool booting = true;
int bootHold = 0;
int frameNo = 0;
String crewName = "RIPLEY";

uint32_t rnd(uint32_t x) {  // xorshift: numeri pseudo-casuali ripetibili
  x ^= x << 13; x ^= x >> 17; x ^= x << 5;
  return x;
}

uint16_t shade(float f) {  // ambra alla luminosita' f (0..1)
  f = constrain(f, 0.0f, 1.0f);
  return tft.color565((int)(255 * f), (int)(176 * f), 0);
}

// ---------- Testo con scrittura progressiva ----------
int revealed = 0, budget = 0;
uint16_t txtCol;
void resetType() { revealed = 0; }

// Stampa fino a 'budget' caratteri; le righe troppo lunghe vengono strette (passo 5 px).
// Se la riga e' scritta solo in parte, alla fine compare il cursore a blocco.
void tprint(int x, int y, const char *s, uint8_t size = 1, uint16_t col = 0) {
  if (!col) col = txtCol;
  int n = strlen(s);
  if (budget <= 0) return;
  int k = min(n, budget);
  budget -= n;
  int adv = (size == 1 && x + n * 6 > 240) ? 5 : 6 * size;
  for (int i = 0; i < k; i++) cv.drawChar(x + i * adv, y, s[i], col, col, size);
  if (k < n) cv.fillRect(x + k * adv, y, 5 * size, 8 * size, col);
}

void tcenter(int y, const char *s) {
  int n = strlen(s), adv = n * 6 > 240 ? 5 : 6;
  tprint((240 - n * adv) / 2, y, s);
}

void cursorBlock(int x, int y) {
  if (on(E_CURSOR) && budget > 0 && (frameNo / 12) % 2 == 0) cv.fillRect(x, y, 6, 8, txtCol);
}

// Maschera alpha a 8 bit in ambra con luminosita' k
void blitMask(int x0, int y0, int w, int h, const uint8_t *a, float k) {
  for (int y = 0; y < h; y++)
    for (int x = 0; x < w; x++) {
      uint8_t v = a[y * w + x];
      if (v < 8) continue;
      cv.drawPixel(x0 + x, y0 + y, shade(v / 255.0f * k));
    }
}

// ---------- Effetti CRT ----------
void initGlow() {
  for (int k = 0; k < 5; k++) {
    float ax = 175 - k * 26, ay = 165 - k * 26;  // anelli ellittici sempre piu' piccoli
    for (int y = 0; y < 240; y++) {
      float dy = (y - 120) / ay;
      float w = dy * dy < 1 ? ax * sqrtf(1 - dy * dy) : 0;
      glowW[k][y] = (uint8_t)min(w, 120.0f);
    }
  }
}

void drawBackground() {
  if (on(E_GLOW)) {
    cv.fillScreen(GLOW[0]);
    for (int k = 0; k < 5; k++)
      for (int y = 0; y < 240; y++)
        if (glowW[k][y]) cv.drawFastHLine(120 - glowW[k][y], y, glowW[k][y] * 2, GLOW[k + 1]);
  } else {
    cv.fillScreen(GLOW[3]);
  }
  if (on(E_SCANBAR)) cv.fillRect(0, (frameNo * 2) % 280 - 30, 240, 18, BAND);
}

void applyScanlines() {  // righe dispari al 75% di luminosita'
  uint16_t *b = cv.getBuffer();
  for (int y = 1; y < 240; y += 2) {
    uint16_t *p = b + y * 240;
    for (int x = 0; x < 240; x++) {
      uint16_t c = p[x];
      p[x] = ((c >> 1) & 0x7BEF) + ((c >> 2) & 0x39E7);
    }
  }
}

// ---------- Boot ----------
void drawBoot() {
  if (bootHold > 40 && on(E_LOGO)) {  // dopo le righe: logo Weyland-Yutani in dissolvenza
    float k = min(1.0f, (bootHold - 40) / 25.0f);
    blitMask((240 - WY_LOGO_W) / 2, 54, WY_LOGO_W, WY_LOGO_H, WY_LOGO, k);
    if (bootHold > 75) {
      budget = (bootHold - 75) * 2;  // sottotitolo battuto
      tcenter(160, "MU/TH/UR 6000");
      tcenter(174, "INTERFACE 2037 READY FOR INQUIRY");
    }
    if (++bootHold > 180) { booting = false; resetType(); }
    return;
  }
  const char *lines[] = {
    "WEYLAND-YUTANI CORP",
    "COMMERCIAL TOWING VEHICLE NOSTROMO",
    "",
    "MU/TH/UR 6000 - INTERFACE 2037",
    "",
    "> MEMORY CHECK .......... OK",
    "> LIFE SUPPORT .......... OK",
    "> HYPERSLEEP BAYS ....... OK",
    "> NAVIGATION ............ OK",
    "> HOST LINK ............. STANDBY",
    "",
    "SEVEN CREW IN HYPERSLEEP",
  };
  int y = 6;
  for (const char *l : lines) { tprint(6, y, l); y += 12; }
  if (budget > 0) {  // tutto scritto: breve pausa e poi il logo
    cursorBlock(6 + 24 * 6, y - 12);
    if (++bootHold > 40 && !on(E_LOGO)) { booting = false; resetType(); }
  }
}

// ---------- Schermata di nave (statistiche del PC) / conversazione ----------
void header() {
  tcenter(4, "WEYLAND-YUTANI CORP");
  tcenter(14, "MU/TH/UR 6000  INTERFACE 2037");
  tcenter(24, "NOSTROMO COMMAND LINK");
}

void statRow(int &y, const char *lab, int val) {
  bool blink = (frameNo / 4) % 2 == 0;
  uint16_t col = (val >= 90 && !blink) ? AD : txtCol;
  char b[48];
  snprintf(b, sizeof(b), "%-12s", lab);
  int segs = (val * 12 + 50) / 100;
  for (int s = 0; s < 12; s++) b[12 + s] = s < segs ? (char)0xDB : (char)0xB0;  // blocchi pieni / ombreggiati (CP437)
  snprintf(b + 24, sizeof(b) - 24, " %3d%%", val);
  tprint(6, y, b, 1, col);
  y += 12;
}

// Conversazione con MU-TH-UR, a rotazione (schermata di nave senza dati dal PC)
void drawConvo() {
  static int lastSet = -1;
  int set = (frameNo / 520) % 3;
  if (set != lastSet) { lastSet = set; resetType(); }
  char b[48], ofc[32];
  snprintf(ofc, sizeof(ofc), "OFFICER: %s", crewName.c_str());
  const char *conv[3][8] = {
    {"INTERFACE 2037 READY FOR INQUIRY", "", ">WHAT IS THE STATE OF THE CREW", "SEVEN CREW IN HYPERSLEEP", "NO CREW AWAKE", "", ">WHAT ARE MY ORDERS", "AWAIT HOST LINK"},
    {">WHAT IS THE STATE OF THE SHIP", "ALL SYSTEMS NOMINAL", "COURSE: ZETA RETICULI", "ETA: 10 MONTHS", "", ">WHO IS ON DUTY", ofc, "REPORT TO COMMAND LINK"},
    {">IS THERE ANYTHING ELSE ON BOARD", "NEGATIVE", "NO FOREIGN BODIES DETECTED", "", ">REQUEST STATUS OF HOST LINK", "HOST LINK: NONE", "WAITING FOR SIGNAL", ""},
  };
  int y = 4;
  if (on(E_HEADER)) { header(); y = 40; }
  if (on(E_TITLE)) {
    tprint(6, y, "-MU/TH/UR 6000 CONVERSATION-"); y += 10;
    tprint(0, y, "========================================"); y += 14;
  }
  if (on(E_LOG)) for (int i = 0; i < 8; i++) { tprint(6, y, conv[set][i]); y += 11; }
  (void)b;
  if (on(E_CURSOR) && y < 232) { tprint(6, y + 4, ">"); cursorBlock(16, y + 4); }
}

void drawSystem() {
  if (!pcLive()) { drawConvo(); return; }
  bool alarm = pcAlarm(), blink = (frameNo / 4) % 2 == 0;
  char b[56];
  int y = 4;
  if (on(E_HEADER)) { header(); y = 40; }
  if (on(E_TITLE)) {
    tprint(6, y, alarm ? "SHIP SYSTEMS STATUS: CRITICAL" : "SHIP SYSTEMS STATUS"); y += 10;
    tprint(0, y, "========================================"); y += 14;
  }
  if (on(E_CPU)) statRow(y, "REACTOR", pc.cpu);
  if (on(E_RAM)) statRow(y, "LIFE SUPPORT", pc.ram);
  if (on(E_DSK)) statRow(y, "CARGO HOLD", pc.disk);
  if (on(E_GPU)) statRow(y, "MAIN DRIVE", pc.gpu);

  if (on(E_INFO)) {
    y += 4;
    int n = snprintf(b, sizeof(b), "HULL TEMP: %dC", pc.temp);
    if (extraLive() && pcx.cpuTemp > 0) snprintf(b + n, sizeof(b) - n, "   CORE: %dC", pcx.cpuTemp);
    tprint(6, y, b, 1, pc.temp >= 85 && !blink ? AD : 0); y += 10;
    char r1[8], r2[8];
    if (netExtraLive()) {
      fmtRate(r1, sizeof(r1), pcx.down); fmtRate(r2, sizeof(r2), pcx.up);
      if (pcx.ping >= 0) snprintf(b, sizeof(b), "COMMS DN:%s UP:%s  LAG:%dMS", r1, r2, pcx.ping);
      else snprintf(b, sizeof(b), "COMMS DN:%s UP:%s", r1, r2);
    } else {
      fmtRate(r1, sizeof(r1), pc.net);
      snprintf(b, sizeof(b), "COMMS TRAFFIC: %s/S", r1);
    }
    tprint(6, y, b); y += 10;
    n = snprintf(b, sizeof(b), "HYPERSLEEP CYCLE: %dH", pc.up);
    if (extraLive() && pcx.batt >= 0) snprintf(b + n, sizeof(b) - n, "  CELL: %d%%", pcx.batt);
    tprint(6, y, b); y += 14;
  }

  if (on(E_LOG)) {
    if (extraLive() && pcx.proc[0]) {
      snprintf(b, sizeof(b), "> UNKNOWN LIFEFORM: %s %d%%", pcx.proc, pcx.procPct);
      tprint(6, y, b); y += 11;
    }
    tprint(6, y, ">WHAT ARE MY ORDERS"); y += 11;
    if (alarm) tprint(6, y, "SPECIAL ORDER 937: CREW EXPENDABLE", 1, blink ? 0 : AD);
    else       tprint(6, y, "MAINTAIN COURSE. ALL SYSTEMS NOMINAL");
    y += 14;
  }
  if (on(E_CURSOR) && y < 232) { tprint(6, y, ">"); cursorBlock(16, y); }
}

// ---------- Motion tracker M314 ----------
void drawTracker() {
  bool live = pcLive(), alarm = pcAlarm(), blink = (frameNo / 4) % 2 == 0;
  int load = live ? max(pc.cpu, pc.gpu) : 6;
  const int CX = 120, CY = 114, R = 86;
  char b[48];

  if (on(E_HEADER)) tcenter(4, alarm ? "SPECIAL ORDER 937" : "MOTION TRACKER M314");

  cv.drawCircle(CX, CY, R, AM);
  cv.drawCircle(CX, CY, R * 2 / 3, AD);
  cv.drawCircle(CX, CY, R / 3, AD);
  cv.drawFastHLine(CX - R, CY, 2 * R + 1, AD);
  cv.drawFastVLine(CX, CY - R, 2 * R + 1, AD);
  for (int d = 0; d < 360; d += 30) {  // tacche sul bordo
    float a = d * DEG_TO_RAD;
    cv.drawLine(CX + cosf(a) * (R - 5), CY + sinf(a) * (R - 5), CX + cosf(a) * R, CY + sinf(a) * R, AM);
  }
  tprint(CX + 3, CY - R / 3 - 8, "10M", 1, AD);
  tprint(CX + 3, CY - R * 2 / 3 - 8, "20M", 1, AD);

  // raggio che ruota, con coda che sfuma
  float sweep = frameNo * 0.075f;
  for (int k = 32; k >= 0; k--) {
    float a = sweep - k * 0.022f;
    cv.drawLine(CX, CY, CX + cosf(a) * R, CY + sinf(a) * R, shade(k == 0 ? 1.0f : 0.8f * (1.0f - k / 33.0f)));
  }

  // contatti: piu' carico = piu' blip; compaiono quando li raggiunge il raggio e poi sfumano
  int n = on(E_BLIPS) ? min(8, 1 + load / 14) : 0;
  float sw = fmodf(sweep, 2 * PI);
  int closest = 999;
  uint32_t epoch = (frameNo / 500) + 1;
  for (int i = 0; i < n; i++) {
    uint32_t s = rnd((i + 1) * 7919u + epoch * 104729u);
    float ang = (s % 628) / 100.0f, dist = 14 + (s >> 10) % (R - 18);
    float delta = sw - ang;
    while (delta < 0) delta += 2 * PI;
    if (delta < 3.4f) {
      float f = 1.0f - delta / 3.4f;
      cv.fillCircle(CX + cosf(ang) * dist, CY + sinf(ang) * dist, f > 0.75f ? 3 : 2, shade(f));
    }
    closest = min(closest, (int)dist);
  }

  if (on(E_LOG)) {
    if (n) {
      snprintf(b, sizeof(b), "CONTACTS: %d   CLOSEST: %d.%dM", n, closest * 5 / 43, (closest * 50 / 43) % 10);
      tprint(6, 208, b);
      if (closest < 30 || alarm) tprint(6, 220, "!! PROXIMITY ALERT !!", 1, blink ? 0 : AD);
      else tprint(6, 220, live ? "MOVEMENT DETECTED" : "SCANNING...");
    } else {
      tprint(6, 208, "NO CONTACTS");
    }
  }
}

// ---------- Terminale seriale ----------
const int LOG_N = 14;
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
  int y = 4;
  if (on(E_HEADER)) {
    tcenter(y, "MU/TH/UR 6000  INTERFACE 2037"); y += 10;
    tprint(0, y, "========================================"); y += 12;
  }
  for (int i = 0; i < logCount && y < 186; i++) { tprint(6, y, logLines[i].c_str()); y += 10; }
  if (on(E_CURSOR) && y < 186) { tprint(6, y, ">"); cursorBlock(16, y); }

  const int PY = 196, PH = 40;  // grafico dei numeri ricevuti
  cv.drawRect(2, PY, 236, PH, AD);
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
      if (i > 0) cv.drawLine(px, py, x, yv, txtCol);
      px = x; py = yv;
    }
  }
}

// ---------- Grafico delle statistiche ----------
void drawGraph() {
  int m = curMetric();
  HistStats s = histStats(m);
  int scale = histScale(m);
  char b[48];
  int y = 4;
  if (on(E_HEADER)) {
    tcenter(y, "WEYLAND-YUTANI CORP"); y += 10;
    tcenter(y, "MU/TH/UR 6000"); y += 14;
  }
  if (on(E_TITLE)) {
    snprintf(b, sizeof(b), "-TELEMETRY: %s-", METRIC_NAME[m]);
    tprint(6, y, b); y += 10;
    tprint(0, y, "========================================"); y += 12;
  }
  for (int i = 0; i < MT_COUNT; i++) {  // selettore: la metrica mostrata e' in negativo
    int x = 6 + i * 33;
    if (i == m) {
      if (budget > 0) cv.fillRect(x - 3, y - 1, 24, 10, txtCol);
      tprint(x, y, METRIC_SHORT[i], 1, INV);
    } else {
      tprint(x, y, METRIC_SHORT[i]);
    }
  }
  y += 14;

  const int GX = 34, GW = HIST_N;
  int GH = max(60, 172 - y);
  if (budget > 0) {
    cv.drawRect(GX - 1, y - 1, GW + 2, GH + 2, AD);
    for (int q = 1; q < 4; q++)
      for (int x = GX; x < GX + GW; x += 4) cv.drawPixel(x, y + GH * q / 4, AD);
  }
  snprintf(b, sizeof(b), "%d", scale);     tprint(0, y, b);
  snprintf(b, sizeof(b), "%d", scale / 2); tprint(0, y + GH / 2 - 4, b);
  tprint(0, y + GH - 8, "0");
  if (!histCount) tprint(GX + 30, y + GH / 2 - 4, "> NO TELEMETRY DATA");
  else if (budget > 0) histPlot(cv, GX, y, GW, GH, m, txtCol, tft.color565(70, 40, 0), scale);
  y += GH + 6;

  if (on(E_LOG)) {
    const char *u = METRIC_UNIT[m];
    snprintf(b, sizeof(b), "NOW %d%s  MIN %d  MAX %d  AVG %d", s.now, u, s.mn, s.mx, s.avg);
    tprint(6, y, b); y += 10;
    int tr = histTrend(m);
    snprintf(b, sizeof(b), "> LOG: trend %s, %ds%s", tr > 0 ? "rising" : tr < 0 ? "falling" : "stable",
             histCount, graphAuto ? " [AUTO]" : "");
    tprint(6, y, b); y += 12;
  }
  if (on(E_CURSOR) && y < 232) {
    snprintf(b, sizeof(b), "> run:// telemetry -%s", METRIC_KEY[m]);
    tprint(6, y, b);
    cursorBlock(6 + strlen(b) * 6 + 4, y);
  }
}

// ---------- Fotogramma ----------
void renderFrame() {
  Screen want = screenSel == SCR_AUTO ? (pcLive() ? SCR_SYSTEM : SCR_TRACKER) : screenSel;
  if (want != shown) { shown = want; resetType(); }

  budget = on(E_TYPEON) ? revealed : (1 << 30);
  if (revealed < 1000000) revealed += booting ? 5 : 14;
  txtCol = (on(E_FLICKER) && random(0, 70) == 0) ? AM : A;

  drawBackground();
  if (booting) drawBoot();
  else if (shown == SCR_SYSTEM) drawSystem();
  else if (shown == SCR_TRACKER) { budget = 1 << 30; drawTracker(); }  // il radar non si "scrive"
  else if (shown == SCR_GRAPH) drawGraph();
  else drawTerm();
  if (on(E_SCANLINES)) applyScanlines();

  alertOverlay(cv, A, frameNo);
  tft.drawRGBBitmap(0, 0, cv.getBuffer(), 240, 240);
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

void startBoot() { booting = true; bootHold = 0; resetType(); }

void handleLine(const String &s) {
  String t = s;
  t.trim();
  if (t.length() == 0) return;

  if (t.startsWith("C,")) { flags = strtoul(t.c_str() + 2, nullptr, 10); Serial.println("OK " + t); return; }
  if (t.startsWith("N,")) { crewName = t.substring(2); crewName.trim(); crewName.toUpperCase(); Serial.println("OK " + t); return; }
  if (t.startsWith("M,")) {
    String m = t.substring(2);
    if (m == "auto") screenSel = SCR_AUTO;
    else if (m == "system") screenSel = SCR_SYSTEM;
    else if (m == "tracker") screenSel = SCR_TRACKER;
    else if (m == "term") screenSel = SCR_TERM;
    else if (m == "graph") screenSel = SCR_GRAPH;
    else if (m == "boot") startBoot();
    Serial.println("OK " + t);
    return;
  }
  if (t.equalsIgnoreCase("system"))  { screenSel = SCR_SYSTEM; return; }
  if (t.equalsIgnoreCase("tracker")) { screenSel = SCR_TRACKER; return; }
  if (t.equalsIgnoreCase("auto"))    { screenSel = SCR_AUTO; return; }
  if (t.equalsIgnoreCase("graph"))   { screenSel = SCR_GRAPH; return; }
  if (t.equalsIgnoreCase("intro"))   { startBoot(); return; }

  screenSel = SCR_TERM;
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
void thBegin() {
  A    = tft.color565(255, 176, 0);
  AM   = tft.color565(190, 120, 0);
  AD   = tft.color565(95, 55, 0);
  INV  = tft.color565(28, 12, 0);
  BAND = tft.color565(60, 34, 0);
  for (int k = 0; k < 6; k++) GLOW[k] = tft.color565(4 + k * 3, 2 + k * 3, 0);
  initGlow();
  cv.cp437(true);  // blocchi pieni per le barre
  randomSeed(micros());
  if (!logCount) { addLog("> MU/TH/UR 6000 READY"); addLog("> AWAITING INPUT"); }
  startBoot();
}
void thTick() { renderFrame(); }
void thGetState(ThemeState &s) {
  s.flags = flags;
  s.screen = (uint8_t)screenSel;
  strncpy(s.name, crewName.c_str(), 16);
  s.name[16] = 0;
}
void thSetState(const ThemeState &s) {
  if (s.flags) flags = s.flags & ALL_FLAGS;
  screenSel = (Screen)min<int>(s.screen, SCR_GRAPH);
  if (s.name[0]) crewName = s.name;
}
void thNext() { screenSel = (Screen)((screenSel + 1) % 5); }  // AUTO, SYSTEM, TRACKER, TERM, GRAPH
void thReboot() { startBoot(); }
}  // namespace

const Theme THEME_ALIEN = {"alien", thBegin, thTick, handleLine, thGetState, thSetState, thNext, thReboot};
