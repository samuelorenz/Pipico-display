// Interfaccia in stile Cyberpunk 2077: boot delle ottiche, monitor di sistema da netrunner
// (statistiche del PC, quickhack, scanner), minigioco Breach Protocol e terminale seriale.
// Giallo / ciano / rosso su nero, pannelli ad angolo tagliato, glitch con separazione RGB.
// Ogni elemento si accende/spegne da seriale (o da tools/display_gui.py):
//   C,<maschera>   elementi visibili (bit E_* qui sotto)
//   M,<schermata>  auto | hud | breach | term | graph | boot
//   N,<nome>       nome del netrunner
//   G,<metrica>    grafico: cpu | ram | dsk | gpu | tmp | net | auto (schermata M,graph)
//   ?              il Pico risponde "ID,cyberpunk"
#include "../common/shared.h"
#include "../common/theme.h"
#include "../common/stats.h"
#include "logo.h"

namespace {  // tutto interno a questo file: i temi non si disturbano tra loro
GFXcanvas16 &cv = sharedCanvas();  // disegno fuori schermo, poi un solo blit (niente flicker)

uint16_t YEL, CYN, RED, DYEL, DCYN, DRED, HLB, BGC, PANEL;
const uint16_t BLK = 0x0000;

// ---------- Elementi visibili (E_* perche' F_CPU e' gia' una macro di Arduino) ----------
enum : uint32_t {
  E_HEADER     = 1u << 0,   // barra gialla in alto
  E_CPU        = 1u << 1,
  E_RAM        = 1u << 2,   // RAM a celle, come la RAM dei quickhack
  E_GPU        = 1u << 3,
  E_DSK        = 1u << 4,
  E_INFO       = 1u << 5,   // temperatura, rete, uptime
  E_QUICKHACKS = 1u << 6,   // elenco quickhack con voce selezionata
  E_SCANNER    = 1u << 7,   // riquadro di scansione
  E_TICKER     = 1u << 8,   // notiziario N54 che scorre + riga di stato
  E_FRAMES     = 1u << 9,   // cornici ad angolo tagliato
  E_GLITCH     = 1u << 10,  // fasce orizzontali spostate
  E_CHROMA     = 1u << 11,  // separazione RGB sui titoli
  E_SCANLINES  = 1u << 12,
  E_NOISE      = 1u << 13,
  E_TYPEON     = 1u << 14,  // scrittura progressiva del testo
  E_LOGO       = 1u << 15,  // logo Arasaka nel boot
};
const uint32_t ALL_FLAGS = (1u << 16) - 1;
uint32_t flags = ALL_FLAGS;
static bool on(uint32_t f) { return (flags & f) != 0; }

enum Screen { SCR_AUTO, SCR_HUD, SCR_BREACH, SCR_TERM, SCR_GRAPH };
Screen screenSel = SCR_AUTO;
Screen shown = SCR_BREACH;
bool booting = true;
int bootHold = 0;
int frameNo = 0;
String runnerName = "SAMU";

// ---------- Testo ----------
int revealed = 0, budget = 0;
static void resetType() { revealed = 0; }

// Stampa fino a 'budget' caratteri (scrittura progressiva); col < 0 = giallo
static void tprint(int x, int y, const char *s, uint8_t size = 1, int32_t col = -1) {
  uint16_t c = col < 0 ? YEL : (uint16_t)col;
  int n = strlen(s);
  if (budget <= 0) return;
  int k = min(n, budget);
  budget -= n;
  for (int i = 0; i < k; i++) cv.drawChar(x + i * 6 * size, y, s[i], c, c, size);
  if (k < n) cv.fillRect(x + k * 6 * size, y, 5 * size, 7 * size, c);
}

static void drawStr(int x, int y, const char *s, uint8_t size, uint16_t c) {
  for (int i = 0; s[i]; i++) cv.drawChar(x + i * 6 * size, y, s[i], c, c, size);
}

// Titolo con separazione RGB (rosso a sinistra, ciano a destra, giallo sopra)
static void chroma(int x, int y, const char *s, uint8_t size) {
  if (budget <= 0) return;
  if (on(E_CHROMA)) {
    int o = size > 1 ? 2 : 1;
    drawStr(x - o, y, s, size, RED);
    drawStr(x + o, y, s, size, CYN);
  }
  tprint(x, y, s, size, YEL);
}

// ---------- Grafica ----------
static void cutFrame(int x, int y, int w, int h, uint16_t col) {  // rettangolo con angolo in alto a destra tagliato
  if (!on(E_FRAMES) || budget <= 0) return;
  const int k = 8;
  cv.drawFastHLine(x, y, w - k, col);
  cv.drawLine(x + w - k, y, x + w, y + k, col);
  cv.drawFastVLine(x + w, y + k, h - k, col);
  cv.drawFastHLine(x, y + h, w + 1, col);
  cv.drawFastVLine(x, y, h, col);
  cv.fillRect(x, y + h - 3, 10, 3, col);  // piccola linguetta in basso a sinistra
}

static void segBar(int x, int y, int w, int h, int segs, int val, uint16_t col, uint16_t dim) {
  if (budget <= 0) return;
  int sw = (w - (segs - 1)) / segs, on_ = (val * segs + 50) / 100;
  for (int i = 0; i < segs; i++) cv.fillRect(x + i * (sw + 1), y, sw, h, i < on_ ? col : dim);
}

static void drawHeader(const char *left, const char *right) {
  if (!on(E_HEADER)) return;
  if (budget > 0) {
    cv.fillRect(0, 0, 240, 13, YEL);
    cv.fillTriangle(228, 0, 240, 0, 240, 12, BGC);  // angolo tagliato
  }
  tprint(4, 3, left, 1, BLK);
  if (right) tprint(222 - strlen(right) * 6, 3, right, 1, BLK);
}

// Notiziario che scorre + riga di stato in fondo
static void drawFooter(const char *status, uint16_t statusCol) {
  if (!on(E_TICKER)) return;
  const char *news = "N54 NEWS // NETWATCH ADVISORY: ROGUE AI ACTIVITY IN THE OLD NET // "
                     "TRAUMA TEAM ON STANDBY IN WATSON // ACID RAIN 40% // ";
  int n = strlen(news), w = n * 6;
  if (budget > 0) {
    cv.fillRect(0, 200, 240, 13, PANEL);
    int off = (frameNo * 2) % w;
    for (int i = 0; i < n * 2; i++) {
      int x = 34 + i * 6 - off;
      if (x < 34 || x > 234) continue;
      cv.drawChar(x, 203, news[i % n], YEL, YEL, 1);
    }
    cv.fillRect(0, 200, 30, 13, RED);
    drawStr(3, 203, "N54", 1, BLK);
  }
  tprint(4, 222, status, 1, statusCol);
  tprint(186, 222, "v2.0.77", 1, DYEL);
}

// ---------- Effetti ----------
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

static void applyGlitch(int bands) {  // sposta di lato alcune fasce orizzontali
  uint16_t *b = cv.getBuffer();
  static uint16_t tmp[240];
  for (int i = 0; i < bands; i++) {
    int y0 = random(0, 232), h = random(2, 10), off = random(-16, 17);
    if (off == 0) off = 7;
    for (int y = y0; y < min(240, y0 + h); y++) {
      uint16_t *p = b + y * 240;
      memcpy(tmp, p, sizeof(tmp));
      for (int x = 0; x < 240; x++) p[x] = tmp[(x - off + 240) % 240];
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

// Logo con separazione RGB: copia ciano spostata a destra, poi il logo rosso Arasaka sopra
static void chromaMask(int x0, int y0, int w, int h, const uint8_t *a, float k) {
  if (on(E_CHROMA)) blitMask(x0 + 2, y0, w, h, a, 0, 120, 130, k);
  blitMask(x0, y0, w, h, a, 230, 0, 30, k);
}

// ---------- Boot (ottiche Kiroshi) ----------
static void drawBoot() {
  char b[40];
  if (bootHold > 25 && on(E_LOGO)) {  // dopo le righe: stemma e scritta Arasaka
    float k = min(1.0f, (bootHold - 25) / 20.0f);
    chromaMask((240 - ARASAKA_EMBLEM_W) / 2, 34, ARASAKA_EMBLEM_W, ARASAKA_EMBLEM_H, ARASAKA_EMBLEM, k);
    if (bootHold > 40) chromaMask((240 - ARASAKA_LOGO_W) / 2, 150, ARASAKA_LOGO_W, ARASAKA_LOGO_H, ARASAKA_LOGO, k);
    if (bootHold > 60) {
      budget = (bootHold - 60) * 2;
      tprint(66, 196, "NIGHT CITY // 2077");
    }
    if (++bootHold > 170) { booting = false; resetType(); }
    return;
  }
  const char *lines[] = {
    "KIROSHI OPTICS FW 2.0.77",
    "",
    "> NEURAL LINK ............ OK",
    "> OPTICAL ZOOM ........... OK",
    "> CYBERDECK .............. ONLINE",
    "> ICEBREAKER ............. LOADED",
    "> DAEMONS ................ 3 READY",
    "> NETWATCH PROXY ......... SPOOFED",
    "> RAM UNITS .............. 10",
    "",
  };
  int y = 8;
  for (const char *l : lines) { tprint(6, y, l, 1, CYN); y += 12; }
  snprintf(b, sizeof(b), "WELCOME BACK, %s", runnerName.c_str());
  tprint(6, y, b);
  if (budget > 0) {  // tutto scritto: titolo con glitch, poi la schermata vera
    bootHold++;
    if (bootHold > 25) chroma(36, 170, "NIGHT CITY", 2);
    if (bootHold > 110) { booting = false; resetType(); }
  }
}

// ---------- Monitor di sistema ----------
static void drawHud() {
  bool live = pcLive(), alarm = pcAlarm(), blink = (frameNo / 4) % 2 == 0;
  char b[40];
  snprintf(b, sizeof(b), "UP %dH", pc.up);
  drawHeader("NETRUNNER // SYS.MONITOR", live ? b : "NO LINK");

  // colonna sinistra: statistiche
  cutFrame(2, 18, 128, 176, DYEL);
  int y = 24;
  if (!live) {
    tprint(8, y, "LINK STATUS:", 1, CYN); y += 16;
    chroma(8, y, "NO SIGNAL", 2);          y += 26;
    tprint(8, y, "WAITING FOR HOST", 1, DYEL); y += 12;
    tprint(8, y, "> run pc_stats.py", 1, DCYN);
  } else {
    if (on(E_CPU)) {
      uint16_t c = pc.cpu >= 90 && !blink ? RED : YEL;
      tprint(8, y, "CPU", 1, c);
      snprintf(b, sizeof(b), "%3d%%", pc.cpu); tprint(102, y, b, 1, c);
      segBar(8, y + 10, 118, 8, 20, pc.cpu, c, DYEL);
      y += 24;
    }
    if (on(E_RAM)) {
      int cells = (pc.ram + 5) / 10;
      snprintf(b, sizeof(b), "RAM %2d/10", cells); tprint(8, y, b, 1, CYN);
      snprintf(b, sizeof(b), "%3d%%", pc.ram);     tprint(102, y, b, 1, CYN);
      if (budget > 0) for (int i = 0; i < 10; i++) cv.fillRect(8 + i * 12, y + 10, 10, 8, i < cells ? CYN : DCYN);
      y += 24;
    }
    if (on(E_GPU)) {
      uint16_t c = pc.gpu >= 90 && !blink ? YEL : RED;
      tprint(8, y, "GPU", 1, c);
      snprintf(b, sizeof(b), "%3d%%", pc.gpu); tprint(102, y, b, 1, c);
      segBar(8, y + 10, 118, 8, 20, pc.gpu, c, DRED);
      y += 24;
    }
    if (on(E_DSK)) {
      tprint(8, y, "DSK", 1, CYN);
      snprintf(b, sizeof(b), "%3d%%", pc.disk); tprint(102, y, b, 1, CYN);
      segBar(8, y + 10, 118, 4, 30, pc.disk, CYN, DCYN);
      y += 20;
    }
    if (on(E_INFO)) {
      snprintf(b, sizeof(b), "TEMP   %dC", pc.temp);
      tprint(8, y, b, 1, pc.temp >= 85 && !blink ? RED : YEL); y += 11;
      if (pc.net >= 1000) snprintf(b, sizeof(b), "NET    %d.%dM/S", pc.net / 1000, (pc.net % 1000) / 100);
      else                snprintf(b, sizeof(b), "NET    %dK/S", pc.net);
      tprint(8, y, b); y += 11;
      snprintf(b, sizeof(b), "UPTIME %dH", pc.up);
      tprint(8, y, b); y += 11;
      if (extraLive() && pcx.cpuTemp > 0) {
        snprintf(b, sizeof(b), "CPU T  %dC", pcx.cpuTemp);
        tprint(8, y, b, 1, pcx.cpuTemp >= 90 && !blink ? RED : YEL); y += 11;
      }
      if (extraLive() && pcx.batt >= 0) {
        snprintf(b, sizeof(b), "BATT   %d%%%s", pcx.batt, pcx.plugged ? " +" : "");
        tprint(8, y, b, 1, pcx.batt <= 20 && !pcx.plugged && !blink ? RED : YEL); y += 11;
      }
      if (netExtraLive() && pcx.ping >= 0) {
        snprintf(b, sizeof(b), "PING   %dMS", pcx.ping);
        tprint(8, y, b, 1, pcx.ping > 150 && !blink ? RED : CYN);
      }
    }
  }

  // colonna destra: quickhack e scanner
  int rx = 140, ry = 24;
  if (on(E_QUICKHACKS)) {
    cutFrame(136, 18, 101, 86, DCYN);
    tprint(rx, ry, "QUICKHACKS", 1, CYN); ry += 14;
    const char *qh[5] = {"PING", "REBOOT OPTICS", "SHORT CIRCUIT", "OVERHEAT", "SYSTEM RESET"};
    const int cost[5] = {1, 3, 4, 5, 6};
    int sel = alarm ? 3 : (frameNo / 50) % 5;
    for (int i = 0; i < 5; i++) {
      int iy = ry + i * 13;
      bool hl = i == sel;
      if (hl && budget > 0) cv.fillRect(rx - 2, iy - 2, 94, 11, alarm ? (blink ? RED : DRED) : CYN);
      tprint(rx, iy, qh[i], 1, hl ? BLK : CYN);
      snprintf(b, sizeof(b), "%d", cost[i]);
      tprint(rx + 84, iy, b, 1, hl ? BLK : YEL);
    }
    ry = 110;
  }
  if (on(E_SCANNER)) {
    int sx = 136, sy = ry, sw = 101, sh = 192 - ry;
    if (budget > 0) {
      const int L = 8;  // staffe d'angolo gialle
      cv.drawFastHLine(sx, sy, L, YEL); cv.drawFastVLine(sx, sy, L, YEL);
      cv.drawFastHLine(sx + sw - L, sy, L, YEL); cv.drawFastVLine(sx + sw - 1, sy, L, YEL);
      cv.drawFastHLine(sx, sy + sh - 1, L, YEL); cv.drawFastVLine(sx, sy + sh - L, L, YEL);
      cv.drawFastHLine(sx + sw - L, sy + sh - 1, L, YEL); cv.drawFastVLine(sx + sw - 1, sy + sh - L, L, YEL);
      int ly = sy + 4 + (frameNo * 2) % (sh - 8);  // linea di scansione
      cv.drawFastHLine(sx + 3, ly, sw - 6, DCYN);
    }
    int t = frameNo % 220, pct = min(100, t * 100 / 150);
    tprint(sx + 6, sy + 6, pct < 100 ? "SCANNING" : "SCAN COMPLETE", 1, CYN);
    snprintf(b, sizeof(b), "%3d%%", pct);
    tprint(sx + 6, sy + 18, b, 2, YEL);
    if (pct >= 100) {
      int load = max(pc.cpu, pc.gpu);
      const char *lvl = !live ? "UNKNOWN" : alarm ? "EXTREME" : load < 40 ? "LOW" : load < 75 ? "MODERATE" : "HIGH";
      tprint(sx + 6, sy + 40, "THREAT:", 1, DYEL);
      tprint(sx + 6, sy + 51, lvl, 1, alarm && blink ? RED : YEL);
      if (live && extraLive() && pcx.proc[0]) {  // processo che usa piu' CPU
        snprintf(b, sizeof(b), "> %s", pcx.proc);
        tprint(sx + 6, sy + 64, b, 1, CYN);
      }
    }
  }

  if (alarm)      drawFooter("!! CRITICAL: SYSTEM OVERHEAT !!", blink ? RED : DRED);
  else if (live)  drawFooter("> SYS NOMINAL // LINK STABLE", CYN);
  else            drawFooter("> NETWATCH PROXY ACTIVE", DCYN);
}

// ---------- Breach Protocol (si gioca da solo) ----------
const char *CODES[6] = {"1C", "55", "BD", "E9", "7A", "FF"};
uint8_t mtx[5][5];
bool used[5][5];
int pathR[4], pathC[4];
int bufCodes[4], bufN;
int seq1[2], seq2[3];
bool rowAxis;
int axisIdx, curR, curC, tr, tc, pickN, bState, bTimer, bTime, moveT;
bool failGame, ok1, ok2, breachReady = false;

static void chooseTarget() {
  if (!failGame || pickN < 2) { tr = pathR[pickN]; tc = pathC[pickN]; return; }
  do {  // partita "sbagliata": dopo due mosse giuste sceglie a caso lungo l'asse
    if (rowAxis) { tr = axisIdx; tc = random(5); } else { tc = axisIdx; tr = random(5); }
  } while (used[tr][tc] || (tr == pathR[pickN] && tc == pathC[pickN]));
}

void breachNew() {
  for (int r = 0; r < 5; r++)
    for (int c = 0; c < 5; c++) { mtx[r][c] = random(6); used[r][c] = false; }
  // percorso valido: riga 0, poi colonna, poi riga, poi colonna
  pathR[0] = 0; pathC[0] = random(5);
  for (int k = 1; k < 4; k++) {
    if (k % 2) { pathC[k] = pathC[k - 1]; do pathR[k] = random(5); while (pathR[k] == pathR[k - 1]); }
    else       { pathR[k] = pathR[k - 1]; do pathC[k] = random(5); while (pathC[k] == pathC[k - 1]); }
  }
  for (int i = 0; i < 2; i++) seq1[i] = mtx[pathR[i]][pathC[i]];
  for (int i = 0; i < 3; i++) seq2[i] = mtx[pathR[i + 1]][pathC[i + 1]];
  failGame = random(4) == 0;
  rowAxis = true; axisIdx = 0;
  curR = 0; curC = random(5);
  bufN = 0; pickN = 0; bState = 0; bTimer = 0; moveT = 0;
  bTime = 15 * 30;
  chooseTarget();
  breachReady = true;
}

static bool hasSeq(const int *s, int n) {
  for (int st = 0; st + n <= bufN; st++) {
    bool m = true;
    for (int i = 0; i < n && m; i++) m = bufCodes[st + i] == s[i];
    if (m) return true;
  }
  return false;
}

static void breachStep() {
  if (bState == 1) { if (--bTimer <= 0) { breachNew(); resetType(); } return; }
  if (bTime > 0) bTime--;
  if (curR == tr && curC == tc) {  // sul bersaglio: breve pausa e selezione
    if (++bTimer < 12) return;
    bTimer = 0;
    used[tr][tc] = true;
    bufCodes[bufN++] = mtx[tr][tc];
    if (rowAxis) { rowAxis = false; axisIdx = tc; } else { rowAxis = true; axisIdx = tr; }
    pickN++;
    if (bufN == 4) { ok1 = hasSeq(seq1, 2); ok2 = hasSeq(seq2, 3); bState = 1; bTimer = 120; return; }
    chooseTarget();
    return;
  }
  if (++moveT < 6) return;  // il cursore scorre di una cella ogni 6 fotogrammi
  moveT = 0;
  if (rowAxis) curC += curC < tc ? 1 : -1;
  else         curR += curR < tr ? 1 : -1;
}

static void seqLine(int x, int y, const int *s, int n, bool done, bool okv) {
  char b[4];
  for (int i = 0; i < n; i++) {
    snprintf(b, sizeof(b), "%s", CODES[s[i]]);
    tprint(x + i * 18, y, b, 1, done ? (okv ? CYN : RED) : YEL);
  }
}

static void drawBreach() {
  if (!breachReady) breachNew();
  char b[40];
  snprintf(b, sizeof(b), "%d.%02d", bTime / 30, (bTime % 30) * 100 / 30);
  drawHeader("BREACH PROTOCOL", b);
  if (on(E_HEADER) && budget > 0) cv.fillRect(0, 14, 240 * bTime / 450, 2, YEL);  // tempo rimasto

  // matrice dei codici
  tprint(6, 20, "CODE MATRIX");
  cutFrame(2, 30, 128, 98, DYEL);
  const int X0 = 12, Y0 = 37;
  if (bState == 0 && budget > 0) {  // riga o colonna attiva
    if (rowAxis) cv.fillRect(4, Y0 + axisIdx * 18 - 4, 124, 15, HLB);
    else         cv.fillRect(X0 + axisIdx * 24 - 5, 32, 22, 94, HLB);
  }
  for (int r = 0; r < 5; r++) {
    for (int c = 0; c < 5; c++) {
      int x = X0 + c * 24, y = Y0 + r * 18;
      bool onAxis = bState == 0 && (rowAxis ? r == axisIdx : c == axisIdx);
      bool cur = bState == 0 && r == curR && c == curC;
      if (cur && budget > 0) {
        if (bTimer > 0) cv.fillRect(x - 3, y - 3, 17, 13, YEL);
        else            cv.drawRect(x - 3, y - 3, 17, 13, YEL);
      }
      uint16_t col = used[r][c] ? DCYN : (cur && bTimer > 0) ? BLK : onAxis ? YEL : DYEL;
      tprint(x, y, used[r][c] ? "[]" : CODES[mtx[r][c]], 1, col);
    }
  }

  // buffer e sequenze
  tprint(140, 20, "BUFFER", 1, CYN);
  for (int i = 0; i < 4; i++) {
    int x = 140 + i * 24;
    if (budget > 0) cv.drawRect(x, 30, 21, 14, DCYN);
    if (i < bufN) tprint(x + 5, 34, CODES[bufCodes[i]]);
  }
  tprint(140, 52, "SEQUENCES", 1, CYN);
  seqLine(140, 64, seq1, 2, bState == 1, ok1);
  tprint(140, 74, bState == 1 ? (ok1 ? "DATAMINE_V1 OK" : "DATAMINE_V1 X") : "DATAMINE_V1", 1, DCYN);
  seqLine(140, 90, seq2, 3, bState == 1, ok2);
  tprint(140, 100, bState == 1 ? (ok2 ? "ICEPICK OK" : "ICEPICK X") : "ICEPICK", 1, DCYN);
  cutFrame(136, 16, 101, 96, DCYN);

  // esito e registro
  int y = 136;
  if (bState == 1) {
    if (ok1 && ok2) { chroma(8, y, "BREACH", 2); chroma(8, y + 18, "SUCCESSFUL", 2); }
    else            { chroma(8, y, "BREACH", 2); chroma(8, y + 18, "PARTIAL", 2); }
    tprint(8, y + 42, ok1 ? "> DAEMON UPLOADED: DATAMINE_V1" : "> DAEMON FAILED: DATAMINE_V1", 1, ok1 ? CYN : RED);
    tprint(8, y + 52, ok2 ? "> DAEMON UPLOADED: ICEPICK" : "> DAEMON FAILED: ICEPICK", 1, ok2 ? CYN : RED);
  } else {
    tprint(8, y, "> ACCESS POINT LOCATED", 1, CYN);
    snprintf(b, sizeof(b), "> ICE DETECTED: LVL %d", (int)(mtx[4][4] % 4) + 2);
    tprint(8, y + 11, b, 1, CYN);
    snprintf(b, sizeof(b), "> BREACH IN PROGRESS... %d/4", bufN);
    tprint(8, y + 22, b, 1, YEL);
  }

  drawFooter(bState == 1 ? "> JACKING OUT..." : "> NETRUNNER: JACKED IN", CYN);
  if (budget > 0) breachStep();  // si muove solo quando la schermata e' tutta scritta
}

// ---------- Grafico delle statistiche ----------
static void drawGraph() {
  int m = curMetric();
  HistStats s = histStats(m);
  int scale = histScale(m);
  bool blink = (frameNo / 4) % 2 == 0;
  bool hot = (m <= MT_GPU && s.now >= 90) || (m == MT_TMP && s.now >= 85);
  char b[40];
  drawHeader("NETRUNNER // TELEMETRY", graphAuto ? "AUTO" : METRIC_SHORT[m]);

  // "chip" di selezione: quello attivo e' giallo pieno con l'angolo tagliato
  for (int i = 0; i < MT_COUNT; i++) {
    int x = 2 + i * 34, y = 17;
    if (budget > 0) {
      if (i == m) { cv.fillRect(x, y, 31, 12, YEL); cv.fillTriangle(x + 25, y, x + 31, y, x + 31, y + 6, BGC); }
      else        cv.drawRect(x, y, 31, 12, DCYN);
    }
    tprint(x + 7, y + 3, METRIC_SHORT[i], 1, i == m ? BLK : CYN);
  }

  // grafico: giallo per CPU, ciano per RAM / disco / rete, rosso per GPU / temperatura
  uint16_t line = YEL, fill = tft.color565(48, 45, 2);
  if (m == MT_RAM || m == MT_DSK || m == MT_NET) { line = CYN; fill = tft.color565(0, 40, 46); }
  if (m == MT_GPU || m == MT_TMP)                 { line = RED; fill = tft.color565(50, 0, 14); }
  const int GX = 34, GY = 38, GW = HIST_N, GH = 108;
  cutFrame(2, 33, 236, 118, DYEL);
  if (budget > 0)
    for (int q = 1; q < 4; q++)
      for (int x = GX; x < GX + GW; x += 6) cv.drawFastHLine(x, GY + GH * q / 4, 3, DCYN);
  snprintf(b, sizeof(b), "%d", scale);     tprint(6, GY, b, 1, DYEL);
  snprintf(b, sizeof(b), "%d", scale / 2); tprint(6, GY + GH / 2 - 4, b, 1, DYEL);
  tprint(6, GY + GH - 8, "0", 1, DYEL);
  if (!histCount) chroma(GX + 46, GY + GH / 2 - 8, "NO SIGNAL", 2);
  else if (budget > 0) histPlot(cv, GX, GY, GW, GH, m, line, fill, scale);

  // valore attuale grande + minimo / massimo / media
  snprintf(b, sizeof(b), "%d%s", s.now, METRIC_UNIT[m]);
  if (hot) tprint(8, 160, b, 3, blink ? RED : DRED);
  else     chroma(8, 160, b, 3);
  tprint(8, 188, METRIC_NAME[m], 1, DYEL);
  snprintf(b, sizeof(b), "MIN %d", s.mn);  tprint(130, 158, b, 1, CYN);
  snprintf(b, sizeof(b), "MAX %d", s.mx);  tprint(130, 169, b, 1, CYN);
  snprintf(b, sizeof(b), "AVG %d", s.avg); tprint(130, 180, b, 1, CYN);
  int tr = histTrend(m);
  tprint(130, 191, tr > 0 ? "TREND RISING" : tr < 0 ? "TREND FALLING" : "TREND STABLE", 1, YEL);

  drawFooter(hot ? "!! CRITICAL VALUE DETECTED !!" : "> TELEMETRY STREAM // 1 HZ", hot ? (blink ? RED : DRED) : CYN);
}

// ---------- Terminale seriale ----------
const int LOG_N = 12;
String logLines[LOG_N];
int logCount = 0;
const int N_SAMP = 234;
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
  drawHeader("NETRUNNER // TERMINAL", nullptr);
  int y = 20;
  for (int i = 0; i < logCount; i++) { tprint(6, y, logLines[i].c_str(), 1, CYN); y += 10; }
  if (budget > 0 && (frameNo / 12) % 2 == 0) cv.fillRect(6, y, 6, 8, YEL);

  const int PY = 146, PH = 48;  // grafico dei numeri ricevuti
  cutFrame(2, PY, 236, PH, DYEL);
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
      int x = 4 + i, yv = (int)(PY + PH - 3 - (v - mn) / (mx - mn) * (PH - 6));
      if (i > 0) cv.drawLine(px, py, x, yv, YEL);
      px = x; py = yv;
    }
  }
  drawFooter("> AWAITING INPUT", DCYN);
}

// ---------- Fotogramma ----------
void renderFrame() {
  Screen want = screenSel == SCR_AUTO ? (pcLive() ? SCR_HUD : SCR_BREACH) : screenSel;
  if (want != shown) { shown = want; resetType(); }

  budget = on(E_TYPEON) ? revealed : (1 << 30);
  if (revealed < 1000000) revealed += booting ? 6 : 16;

  cv.fillScreen(BGC);
  if (booting) drawBoot();
  else if (shown == SCR_HUD) drawHud();
  else if (shown == SCR_BREACH) drawBreach();
  else if (shown == SCR_GRAPH) drawGraph();
  else drawTerm();

  if (on(E_NOISE))
    for (int i = 0; i < 8; i++) cv.drawPixel(random(0, 240), random(0, 240), random(2) ? CYN : RED);
  bool bootGlitch = booting && bootHold > 25 && random(0, 4) == 0;
  if (on(E_GLITCH) && (bootGlitch || random(0, 40) == 0)) applyGlitch(random(1, 4));
  if (on(E_SCANLINES)) applyScanlines();

  alertOverlay(cv, RED, frameNo);
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
  if (t.startsWith("N,")) { runnerName = t.substring(2); runnerName.trim(); runnerName.toUpperCase(); Serial.println("OK " + t); return; }
  if (t.startsWith("M,")) {
    String m = t.substring(2);
    if (m == "auto") screenSel = SCR_AUTO;
    else if (m == "hud") screenSel = SCR_HUD;
    else if (m == "breach") screenSel = SCR_BREACH;
    else if (m == "term") screenSel = SCR_TERM;
    else if (m == "graph") screenSel = SCR_GRAPH;
    else if (m == "boot") startBoot();
    Serial.println("OK " + t);
    return;
  }
  if (t.equalsIgnoreCase("hud"))    { screenSel = SCR_HUD; return; }
  if (t.equalsIgnoreCase("breach")) { screenSel = SCR_BREACH; return; }
  if (t.equalsIgnoreCase("auto"))   { screenSel = SCR_AUTO; return; }
  if (t.equalsIgnoreCase("graph"))  { screenSel = SCR_GRAPH; return; }
  if (t.equalsIgnoreCase("intro"))  { startBoot(); return; }

  screenSel = SCR_TERM;
  if (t.equalsIgnoreCase("clear")) {
    logCount = 0; sCount = 0; sHead = 0;
    addLog("> MEMORY WIPED");
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
  YEL   = tft.color565(252, 238, 10);   // giallo Cyberpunk
  CYN   = tft.color565(0, 240, 255);
  RED   = tft.color565(255, 0, 60);
  DYEL  = tft.color565(100, 94, 4);
  DCYN  = tft.color565(0, 80, 92);
  DRED  = tft.color565(100, 0, 24);
  HLB   = tft.color565(44, 42, 0);      // riga/colonna attiva del Breach Protocol
  BGC   = tft.color565(6, 4, 10);
  PANEL = tft.color565(26, 22, 30);
  randomSeed(micros());
  if (!logCount) { addLog("> NETRUNNER TERMINAL READY"); addLog("> AWAITING INPUT"); }
  startBoot();
}
static void thTick() { renderFrame(); }
static void thGetState(ThemeState &s) {
  s.flags = flags;
  s.screen = (uint8_t)screenSel;
  strncpy(s.name, runnerName.c_str(), 16);
  s.name[16] = 0;
}
static void thSetState(const ThemeState &s) {
  if (s.flags) flags = s.flags & ALL_FLAGS;
  screenSel = (Screen)min<int>(s.screen, SCR_GRAPH);
  if (s.name[0]) runnerName = s.name;
}
static void thNext() { screenSel = (Screen)((screenSel + 1) % 5); }  // AUTO, HUD, BREACH, TERM, GRAPH
static void thReboot() { startBoot(); }
}  // namespace

const Theme THEME_CYBERPUNK = {"cyberpunk", thBegin, thTick, handleLine, thGetState, thSetState, thNext, thReboot};
