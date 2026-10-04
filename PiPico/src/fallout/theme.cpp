// Terminale RobCo Industries (Fallout): boot stile Fallout 4, monitor di sistema con
// statistiche del PC, minigioco di hacking della password e terminale seriale.
// Ogni elemento ed effetto si accende/spegne da seriale (o dalla GUI tools/display_gui.py):
//   C,<maschera>   elementi visibili (bit E_* qui sotto)
//   M,<schermata>  auto | home | hack | term | graph | boot
//   N,<nome>       nome dell'amministratore
//   G,<metrica>    grafico: cpu | ram | dsk | gpu | tmp | net | auto (schermata M,graph)
//   ?              il Pico risponde "ID,fallout"
#include "../common/shared.h"
#include "../common/theme.h"
#include "../common/stats.h"
#include "logo.h"

namespace {  // tutto interno a questo file: i temi non si disturbano tra loro
GFXcanvas16 &cv = sharedCanvas();  // disegno fuori schermo, poi un solo blit (niente flicker)

uint16_t G, GM, GD, INV, BAND;
uint16_t GLOW[6];          // bagliore dal bordo (0) al centro (5)
uint8_t glowW[5][240];     // semi-larghezza di ogni anello di bagliore, riga per riga

// ---------- Elementi visibili ----------
enum : uint32_t {
  E_HEADER    = 1u << 0,   // intestazione RobCo
  E_TITLE     = 1u << 1,   // titolo della sezione + linea ====
  E_CPU       = 1u << 2,
  E_RAM       = 1u << 3,
  E_DSK       = 1u << 4,
  E_GPU       = 1u << 5,
  E_INFO      = 1u << 6,   // temp GPU / rete / uptime (offline: user log)
  E_MENU      = 1u << 7,   // menu con voce evidenziata
  E_LOG       = 1u << 8,   // righe > LOG: ...
  E_CURSOR    = 1u << 9,   // prompt con cursore a blocco
  E_BIGSTATS  = 1u << 10,  // statistiche a caratteri grandi invece che barre ASCII
  E_SCANLINES = 1u << 11,
  E_SCANBAR   = 1u << 12,
  E_GLOW      = 1u << 13,
  E_FLICKER   = 1u << 14,
  E_TYPEON    = 1u << 15,  // scrittura progressiva del testo
  E_LOGO      = 1u << 16,  // logo RobCo Industries nel boot
};
uint32_t flags = (1u << 17) - 1;
static bool on(uint32_t f) { return (flags & f) != 0; }

enum Screen { SCR_AUTO, SCR_HOME, SCR_HACK, SCR_TERM, SCR_GRAPH };
Screen screenSel = SCR_AUTO;  // scelta da seriale/GUI
Screen shown = SCR_HACK;      // schermata effettivamente mostrata
bool booting = true;
int bootHold = 0;
int frameNo = 0;
String adminName = "SAMU";

// ---------- Testo con scrittura progressiva ----------
int revealed = 0;    // caratteri gia' "battuti" sulla schermata corrente
int budget = 0;      // caratteri ancora disegnabili in questo fotogramma
uint16_t txtCol;     // colore del testo di questo fotogramma

static void resetType() { revealed = 0; }

// Stampa fino a 'budget' caratteri; le righe troppo lunghe vengono strette (passo 5 px).
// Se la riga e' scritta solo in parte, alla fine compare il cursore a blocco.
static void tprint(int x, int y, const char *s, uint8_t size = 1, uint16_t col = 0) {
  if (!col) col = txtCol;
  int n = strlen(s);
  if (budget <= 0) return;
  int k = min(n, budget);
  budget -= n;
  int adv = (size == 1 && x + n * 6 > 240) ? 5 : 6 * size;
  for (int i = 0; i < k; i++) cv.drawChar(x + i * adv, y, s[i], col, col, size);
  if (k < n) cv.fillRect(x + k * adv, y, 5 * size, 8 * size, col);
}

static void tcenter(int y, const char *s) {
  int n = strlen(s), adv = n * 6 > 240 ? 5 : 6;
  tprint((240 - n * adv) / 2, y, s);
}

// Riga evidenziata in negativo (voce di menu selezionata)
static void tinverse(int y, const char *s) {
  if (budget <= 0) return;
  cv.fillRect(0, y - 1, 240, 10, txtCol);
  tprint(6, y, s, 1, INV);
}

static void cursorBlock(int x, int y) {
  if (on(E_CURSOR) && budget > 0 && (frameNo / 12) % 2 == 0) cv.fillRect(x, y, 6, 8, txtCol);
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

static void drawBackground() {
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

// Righe dispari al 75% di luminosita': effetto scanline del monitor
static void applyScanlines() {
  uint16_t *b = cv.getBuffer();
  for (int y = 1; y < 240; y += 2) {
    uint16_t *p = b + y * 240;
    for (int x = 0; x < 240; x++) {
      uint16_t c = p[x];
      p[x] = ((c >> 1) & 0x7BEF) + ((c >> 2) & 0x39E7);
    }
  }
}

// Disegna una maschera alpha a 8 bit nel colore (r,g,b) con luminosita' k
static void blitMask(int x0, int y0, int w, int h, const uint8_t *a, uint8_t r, uint8_t g, uint8_t b, float k) {
  for (int y = 0; y < h; y++)
    for (int x = 0; x < w; x++) {
      uint8_t v = a[y * w + x];
      if (v < 8) continue;
      float f = v / 255.0f * k;
      cv.drawPixel(x0 + x, y0 + y, tft.color565(r * f, g * f, b * f));
    }
}

// ---------- Boot (sequenza di Fallout 4) ----------
static void drawBoot() {
  if (bootHold > 50 && on(E_LOGO)) {  // dopo le righe: logo RobCo in dissolvenza
    float k = min(1.0f, (bootHold - 50) / 25.0f);
    blitMask((240 - ROBCO_LOGO_W) / 2, 50, ROBCO_LOGO_W, ROBCO_LOGO_H, ROBCO_LOGO, 120, 255, 160, k);
    if (bootHold > 80) {
      budget = (bootHold - 80) * 2;  // sottotitolo battuto a mano
      tcenter(170, "TERMLINK PROTOCOL v2.3.0");
      tcenter(184, "(C) 2075-2077 ROBCO INDUSTRIES");
    }
    if (++bootHold > 190) { booting = false; resetType(); }
    return;
  }
  const char *lines[] = {
    "WELCOME TO ROBCO INDUSTRIES (TM) TERMLINK",
    "",
    ">SET TERMINAL/INQUIRE",
    "",
    "RIT-V300",
    "",
    ">SET FILE/PROTECTION=OWNER:RWED ACCOUNTS.F",
    ">SET HALT RESTART/MAINT",
    "",
    "Initializing Robco Industries(TM)",
    "MF Boot Agent v2.3.0",
    "RETROS BIOS",
    "RBIOS-4.02.08.00 52EE5.E7.E8",
    "Copyright 2201-2203 Robco Ind.",
    "Uppermem: 64 KB",
    "Root (5A8)",
    "Maintenance Mode",
    "",
    ">RUN DEBUG/ACCOUNTS.F",
  };
  int y = 4;
  for (const char *l : lines) { tprint(4, y, l); y += 10; }
  if (budget > 0) {  // tutto scritto: breve pausa e poi la schermata vera
    cursorBlock(4 + 22 * 6, y - 10);
    if (++bootHold > 50 && !on(E_LOGO)) { booting = false; resetType(); }
  }
}

// ---------- Monitor di sistema ----------
static void statRow(int &y, const char *lab, int val) {
  bool blink = (frameNo / 4) % 2 == 0;
  uint16_t col = (val >= 90 && !blink) ? GD : txtCol;
  char b[48];
  if (on(E_BIGSTATS)) {
    tprint(6, y, lab, 2, col);
    if (budget > 0) {
      int segs = (val + 5) / 10;
      for (int s = 0; s < 10; s++) {
        if (s < segs) cv.fillRect(54 + s * 12, y + 1, 10, 13, col);
        else          cv.drawRect(54 + s * 12, y + 1, 10, 13, GD);
      }
    }
    budget -= 10;
    snprintf(b, sizeof(b), "%3d%%", val);
    tprint(180, y, b, 2, col);
    y += 22;
  } else {  // barra ASCII con i caratteri pieni/ombreggiati del set CP437
    int n = snprintf(b, sizeof(b), "%s [", lab);
    int segs = (val * 20 + 50) / 100;
    for (int s = 0; s < 20; s++) b[n++] = s < segs ? (char)0xDB : (char)0xB0;
    snprintf(b + n, sizeof(b) - n, "] %3d%%", val);
    tprint(6, y, b, 1, col);
    y += 12;
  }
}

static void drawHome() {
  bool live = pcLive(), alarm = pcAlarm();
  bool blink = (frameNo / 4) % 2 == 0;
  char b[48];
  int y = 4;

  if (on(E_HEADER)) {
    tcenter(y, "ROBCO INDUSTRIES UNIFIED OPERATING SYSTEM"); y += 10;
    tcenter(y, "COPYRIGHT 2075-2077 ROBCO INDUSTRIES");     y += 10;
    tcenter(y, "-Server 76-");                               y += 16;
  }
  if (on(E_TITLE)) {
    tprint(6, y, live ? "-RobCo System Monitor-" : "-RobCo Trespasser Management System-"); y += 10;
    tprint(0, y, "========================================");                           y += 14;
  }

  if (live) {
    if (on(E_CPU)) statRow(y, "CPU", pc.cpu);
    if (on(E_RAM)) statRow(y, "RAM", pc.ram);
    if (on(E_DSK)) statRow(y, "DSK", pc.disk);
    if (on(E_GPU)) statRow(y, "GPU", pc.gpu);
    if (on(E_INFO)) {
      y += 2;
      if (pc.net >= 1000) snprintf(b, sizeof(b), "GPU TEMP: %dC   NET: %d.%dM/S", pc.temp, pc.net / 1000, (pc.net % 1000) / 100);
      else                snprintf(b, sizeof(b), "GPU TEMP: %dC   NET: %dK/S", pc.temp, pc.net);
      tprint(6, y, b, 1, pc.temp >= 85 && !blink ? GD : 0); y += 10;
      if (netExtraLive()) {  // rete divisa in scaricamento / invio e latenza
        char r1[8], r2[8];
        fmtRate(r1, sizeof(r1), pcx.down); fmtRate(r2, sizeof(r2), pcx.up);
        if (pcx.ping >= 0) snprintf(b, sizeof(b), "DN: %s UP: %s  PING: %dMS", r1, r2, pcx.ping);
        else snprintf(b, sizeof(b), "DN: %s UP: %s  PING: --", r1, r2);
        tprint(6, y, b); y += 10;
      }
      int n = snprintf(b, sizeof(b), "UPTIME: %dH", pc.up);
      if (extraLive() && pcx.cpuTemp > 0) n += snprintf(b + n, sizeof(b) - n, "  CPU: %dC", pcx.cpuTemp);
      if (extraLive() && pcx.batt >= 0) snprintf(b + n, sizeof(b) - n, "  BAT: %d%%%s", pcx.batt, pcx.plugged ? "+" : "");
      tprint(6, y, b); y += 14;
    }
  } else if (on(E_INFO)) {
    tprint(6, y, "| User Log:"); y += 10;
    snprintf(b, sizeof(b), "| >> Administrator: %s", adminName.c_str());
    tprint(6, y, b); y += 10;
    tprint(6, y, "| >> Helpdesk"); y += 10;
    tprint(6, y, "|======="); y += 14;
  }

  if (on(E_MENU)) {
    const char *itemsLive[3] = {"> View System Log", "> Network Diagnostics", "> Logout"};
    const char *itemsOff[3]  = {"> Request Access", "> Helpdesk", "> Logout"};
    const char **items = live ? itemsLive : itemsOff;
    int sel = (frameNo / 45) % 3;
    for (int i = 0; i < 3; i++) {
      if (i == sel) tinverse(y, items[i]); else tprint(6, y, items[i]);
      y += 11;
    }
    y += 4;
  }

  if (on(E_LOG)) {
    if (!live)       tprint(6, y, "> LOG: waiting for host link...");
    else if (alarm)  tprint(6, y, "> WARNING: SYSTEM OVERLOAD", 1, blink ? 0 : GD);
    else             tprint(6, y, "> LOG: all systems nominal");
    y += 12;
    if (live && extraLive() && pcx.proc[0]) {  // processo che usa piu' CPU
      snprintf(b, sizeof(b), "> TOP: %s %d%%", pcx.proc, pcx.procPct);
      tprint(6, y, b); y += 12;
    }
  }
  if (on(E_CURSOR) && y < 232) { tprint(6, y, ">"); cursorBlock(16, y); }
}

// ---------- Minigioco di hacking (animazione automatica) ----------
const int HROWS = 16, HCOLS = 12, HCELLS = 2 * HROWS * HCOLS;
const char *WORDS[] = {"BARRIER", "CONTROL", "DESTROY", "FALLING", "HEALING", "MACHINE",
                       "PRIVATE", "SURFACE", "WARNING", "VIRTUAL", "FORTUNE", "PICKING"};
const int NWORDS_ALL = 12, NW = 8, WLEN = 7;
char hbuf[HCELLS];
int wpos[NW], wid[NW];
int pwd, attempts, tries, successAt, target, selPos, hState, hTimer;
uint16_t hAddr;
String hlog[3];
bool hackReady = false;

static void hlogSet(const String &a, const String &b, const String &c) { hlog[0] = a; hlog[1] = b; hlog[2] = c; }

static void pickTarget() {
  if (tries + 1 >= successAt) { target = pwd; return; }
  do { target = random(NW); } while (target == pwd);
}

void hackNew() {
  const char *junk = "!@#$%^&*()-_=+[]{}<>?/|\\;:'\",.";
  int jl = strlen(junk);
  for (int i = 0; i < HCELLS; i++) hbuf[i] = junk[random(jl)];

  int rows[2 * HROWS], ws[NWORDS_ALL];
  for (int i = 0; i < 2 * HROWS; i++) rows[i] = i;
  for (int i = 0; i < NWORDS_ALL; i++) ws[i] = i;
  for (int i = 2 * HROWS - 1; i > 0; i--) { int j = random(i + 1); int t = rows[i]; rows[i] = rows[j]; rows[j] = t; }
  for (int i = NWORDS_ALL - 1; i > 0; i--) { int j = random(i + 1); int t = ws[i]; ws[i] = ws[j]; ws[j] = t; }
  for (int w = 0; w < NW; w++) {  // una parola per riga, in righe diverse
    wid[w] = ws[w];
    wpos[w] = rows[w] * HCOLS + random(HCOLS - WLEN + 1);
    memcpy(hbuf + wpos[w], WORDS[wid[w]], WLEN);
  }
  pwd = random(NW);
  attempts = 4;
  tries = 0;
  successAt = random(2, 5);  // la password viene trovata al 2o, 3o o 4o tentativo
  selPos = random(HCELLS);
  hState = 0;
  hAddr = 0xF000 + (random(0, 0x80) << 4);
  hlogSet("", "", "");
  pickTarget();
  hackReady = true;
}

static void hackStep() {
  if (hState == 0) {  // il selettore scorre carattere per carattere verso la parola scelta
    int tp = wpos[target];
    for (int i = 0; i < 2 && selPos != tp; i++) selPos += selPos < tp ? 1 : -1;
    if (selPos == tp) { hState = 1; hTimer = 25; }
  } else if (hState == 1) {  // parola evidenziata, poi esito
    if (--hTimer > 0) return;
    tries++;
    String w = String(">") + WORDS[wid[target]];
    if (target == pwd) {
      hlogSet(w, ">Exact match!", ">Please wait while system is accessed.");
      hState = 2; hTimer = 100;
    } else {
      int like = 0;
      for (int i = 0; i < WLEN; i++) like += WORDS[wid[target]][i] == WORDS[wid[pwd]][i];
      hlogSet(w, ">Entry denied.", String(">Likeness=") + like);
      attempts--;
      pickTarget();
      hState = 0;
    }
  } else if (--hTimer <= 0) {  // accesso riuscito: nuova partita
    hackNew();
    resetType();
  }
}

static void drawHack() {
  if (!hackReady) hackNew();
  char b[48];
  int y = 4;
  if (on(E_HEADER)) {
    tprint(3, y, "ROBCO INDUSTRIES (TM) TERMLINK PROTOCOL"); y += 10;
    tprint(3, y, hState == 2 ? "ACCESS GRANTED" : "ENTER PASSWORD NOW");  y += 14;
  }
  int n = snprintf(b, sizeof(b), "%d ATTEMPT(S) LEFT:", attempts);
  tprint(3, y, b);
  if (budget > 0) for (int a = 0; a < attempts; a++) cv.fillRect(3 + (n + 1 + a * 2) * 6, y, 6, 8, txtCol);
  y += 14;

  int hlA, hlB;  // celle evidenziate [hlA, hlB)
  if (hState == 0) { hlA = selPos; hlB = selPos + 1; }
  else             { hlA = wpos[target]; hlB = hlA + WLEN; }
  for (int c = 0; c < 2; c++) {
    for (int r = 0; r < HROWS; r++) {
      int yy = y + r * 10, x0 = c * 120, row = c * HROWS + r;
      snprintf(b, sizeof(b), "0x%04X", hAddr + row * HCOLS);
      tprint(x0, yy, b);
      for (int k = 0; k < HCOLS && budget > 0; k++, budget--) {
        int i = row * HCOLS + k, x = x0 + 42 + k * 6;
        if (i >= hlA && i < hlB) { cv.fillRect(x, yy - 1, 6, 10, txtCol); cv.drawChar(x, yy, hbuf[i], INV, INV, 1); }
        else                     { cv.drawChar(x, yy, hbuf[i], txtCol, txtCol, 1); }
      }
    }
  }

  if (on(E_LOG)) {
    int ly = y + HROWS * 10 + 2;
    for (int i = 0; i < 3; i++) if (hlog[i].length()) tprint(3, ly + i * 10, hlog[i].c_str());
    int last = hlog[2].length() ? 2 : 0;
    cursorBlock(3 + hlog[last].length() * 6 + 2, ly + last * 10);
  }
  if (budget > 0) hackStep();  // si muove solo quando la schermata e' tutta scritta
}

// ---------- Grafico delle statistiche ----------
static void drawGraph() {
  int m = curMetric();
  HistStats s = histStats(m);
  int scale = histScale(m);
  char b[48];
  int y = 4;
  if (on(E_HEADER)) {
    tcenter(y, "ROBCO INDUSTRIES UNIFIED OPERATING SYSTEM"); y += 10;
    tcenter(y, "-Server 76-");                               y += 14;
  }
  if (on(E_TITLE)) {
    snprintf(b, sizeof(b), "-RobCo Telemetry: %s-", METRIC_NAME[m]);
    tprint(6, y, b); y += 10;
    tprint(0, y, "========================================"); y += 12;
  }
  // selettore: la metrica mostrata e' evidenziata in negativo, come una voce di menu
  for (int i = 0; i < MT_COUNT; i++) {
    int x = 6 + i * 33;
    if (i == m) {
      if (budget > 0) cv.fillRect(x - 3, y - 1, 24, 10, txtCol);
      tprint(x, y, METRIC_SHORT[i], 1, INV);
    } else {
      tprint(x, y, METRIC_SHORT[i]);
    }
  }
  y += 14;

  // area del grafico, con etichette di scala a sinistra e linee tratteggiate ai quarti
  const int GX = 34, GW = HIST_N;
  int GH = max(60, 172 - y);
  if (budget > 0) {
    cv.drawRect(GX - 1, y - 1, GW + 2, GH + 2, GD);
    for (int q = 1; q < 4; q++)
      for (int x = GX; x < GX + GW; x += 4) cv.drawPixel(x, y + GH * q / 4, GD);
  }
  snprintf(b, sizeof(b), "%d", scale);     tprint(0, y, b);
  snprintf(b, sizeof(b), "%d", scale / 2); tprint(0, y + GH / 2 - 4, b);
  tprint(0, y + GH - 8, "0");
  if (!histCount) tprint(GX + 30, y + GH / 2 - 4, "> NO TELEMETRY DATA");
  else if (budget > 0) histPlot(cv, GX, y, GW, GH, m, txtCol, tft.color565(16, 70, 36), scale);
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

static void drawTerm() {
  int y = 4;
  if (on(E_HEADER)) {
    tcenter(y, "ROBCO INDUSTRIES (TM) TERMLINK PROTOCOL"); y += 10;
    tprint(0, y, "========================================"); y += 12;
  }
  for (int i = 0; i < logCount && y < 186; i++) { tprint(6, y, logLines[i].c_str()); y += 10; }
  if (on(E_CURSOR) && y < 186) { tprint(6, y, ">"); cursorBlock(16, y); }

  // grafico dei numeri ricevuti
  const int PY = 196, PH = 40;
  cv.drawRect(2, PY, 236, PH, GD);
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

// ---------- Fotogramma ----------
void renderFrame() {
  Screen want = screenSel == SCR_AUTO ? (pcLive() ? SCR_HOME : SCR_HACK) : screenSel;
  if (want != shown) { shown = want; resetType(); }

  budget = on(E_TYPEON) ? revealed : (1 << 30);
  if (revealed < 1000000) revealed += booting ? 5 : 14;
  txtCol = (on(E_FLICKER) && random(0, 70) == 0) ? GM : G;

  drawBackground();
  if (booting) drawBoot();
  else if (shown == SCR_HOME) drawHome();
  else if (shown == SCR_HACK) drawHack();
  else if (shown == SCR_GRAPH) drawGraph();
  else drawTerm();
  if (on(E_SCANLINES)) applyScanlines();

  alertOverlay(cv, G, frameNo);
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

static void startBoot() { booting = true; bootHold = 0; resetType(); }

void handleLine(const String &s) {
  String t = s;
  t.trim();
  if (t.length() == 0) return;

  if (t.startsWith("C,")) { flags = strtoul(t.c_str() + 2, nullptr, 10); Serial.println("OK " + t); return; }
  if (t.startsWith("N,")) { adminName = t.substring(2); adminName.trim(); Serial.println("OK " + t); return; }
  if (t.startsWith("M,")) {
    String m = t.substring(2);
    if (m == "auto") screenSel = SCR_AUTO;
    else if (m == "home") screenSel = SCR_HOME;
    else if (m == "hack") screenSel = SCR_HACK;
    else if (m == "term") screenSel = SCR_TERM;
    else if (m == "graph") screenSel = SCR_GRAPH;
    else if (m == "boot") startBoot();
    Serial.println("OK " + t);
    return;
  }
  if (t.equalsIgnoreCase("home"))  { screenSel = SCR_HOME; return; }
  if (t.equalsIgnoreCase("hack"))  { screenSel = SCR_HACK; return; }
  if (t.equalsIgnoreCase("auto"))  { screenSel = SCR_AUTO; return; }
  if (t.equalsIgnoreCase("graph")) { screenSel = SCR_GRAPH; return; }
  if (t.equalsIgnoreCase("intro")) { startBoot(); return; }

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
    addLog("> " + t);
  }
}

// ---------- Interfaccia del tema ----------
static void thBegin() {
  G    = tft.color565(120, 255, 160);
  GM   = tft.color565(70, 190, 110);
  GD   = tft.color565(20, 110, 55);
  INV  = tft.color565(4, 24, 12);
  BAND = tft.color565(14, 58, 32);
  for (int k = 0; k < 6; k++) GLOW[k] = tft.color565(2 + k * 2, 10 + k * 7, 5 + k * 4);
  initGlow();
  cv.cp437(true);                 // set di caratteri CP437 (blocchi pieni per le barre)
  randomSeed(micros());
  if (!logCount) { addLog("> ROBCO TERMLINK READY"); addLog("> AWAITING INPUT"); }
  startBoot();
}
static void thTick() { renderFrame(); }
static void thGetState(ThemeState &s) {
  s.flags = flags;
  s.screen = (uint8_t)screenSel;
  strncpy(s.name, adminName.c_str(), 16);
  s.name[16] = 0;
}
static void thSetState(const ThemeState &s) {
  if (s.flags) flags = s.flags & ((1u << 17) - 1);
  screenSel = (Screen)min<int>(s.screen, SCR_GRAPH);
  if (s.name[0]) adminName = s.name;
}
static void thNext() { screenSel = (Screen)((screenSel + 1) % 5); }  // AUTO, HOME, HACK, TERM, GRAPH
static void thReboot() { startBoot(); }
}  // namespace

const Theme THEME_FALLOUT = {"fallout", thBegin, thTick, handleLine, thGetState, thSetState, thNext, thReboot};
