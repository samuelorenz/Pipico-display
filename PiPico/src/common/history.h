// Storico delle statistiche del PC per la schermata Grafico, condiviso da tutte le build.
// Un campione per ogni riga "S,..." ricevuta (pc_stats.py e la GUI ne mandano una al secondo).
// Comando seriale "G,<metrica>": cpu | ram | dsk | gpu | tmp | net | auto (a rotazione).
// E' un header da includere in un solo main.cpp per build: non va compilato a parte.
#pragma once
#include <Arduino.h>
#include <Adafruit_GFX.h>

enum Metric { MT_CPU, MT_RAM, MT_DSK, MT_GPU, MT_TMP, MT_NET, MT_COUNT };
static const char *const METRIC_KEY[MT_COUNT]   = {"cpu", "ram", "dsk", "gpu", "tmp", "net"};
static const char *const METRIC_SHORT[MT_COUNT] = {"CPU", "RAM", "DSK", "GPU", "TMP", "NET"};
static const char *const METRIC_NAME[MT_COUNT]  = {"CPU LOAD", "RAM USAGE", "DISK USAGE", "GPU LOAD", "GPU TEMP", "NETWORK"};
static const char *const METRIC_UNIT[MT_COUNT]  = {"%", "%", "%", "%", "C", "K"};

const int HIST_N = 200;  // circa 3 minuti e 20 secondi
inline uint16_t histV[MT_COUNT][HIST_N];
inline int histCount = 0, histHead = 0;
inline int graphMetric = MT_CPU;
inline bool graphAuto = false;
static const unsigned long GRAPH_AUTO_MS = 8000;  // durata di ogni grafico in rotazione

inline void histPush(int cpu, int ram, int dsk, int gpu, int tmp, int net) {
  int v[MT_COUNT] = {cpu, ram, dsk, gpu, tmp, net};
  for (int m = 0; m < MT_COUNT; m++) histV[m][histHead] = (uint16_t)constrain(v[m], 0, 65535);
  histHead = (histHead + 1) % HIST_N;
  if (histCount < HIST_N) histCount++;
}

// i = 0 il campione piu' vecchio, histCount - 1 il piu' recente
inline int histAt(int m, int i) { return histV[m][(histHead - histCount + i + HIST_N) % HIST_N]; }

inline int curMetric() { return graphAuto ? (int)((millis() / GRAPH_AUTO_MS) % MT_COUNT) : graphMetric; }

// Fondo scala: 100 per le percentuali, almeno 100 C per la temperatura, automatico per la rete
inline int histScale(int m) {
  if (m <= MT_GPU) return 100;
  int mx = m == MT_TMP ? 100 : 10;
  for (int i = 0; i < histCount; i++) mx = max(mx, histAt(m, i));
  if (m == MT_NET) {  // arrotonda a 1 / 2 / 5 x 10^n
    int p = 1;
    while (p * 10 < mx) p *= 10;
    mx = mx <= p * 2 ? p * 2 : mx <= p * 5 ? p * 5 : p * 10;
  }
  return mx;
}

struct HistStats { int now, mn, mx, avg; };
inline HistStats histStats(int m) {
  HistStats s = {0, 0, 0, 0};
  if (!histCount) return s;
  long sum = 0;
  s.mn = 65535;
  for (int i = 0; i < histCount; i++) {
    int v = histAt(m, i);
    s.mn = min(s.mn, v); s.mx = max(s.mx, v); sum += v;
  }
  s.now = histAt(m, histCount - 1);
  s.avg = sum / histCount;
  return s;
}

// Tendenza: media degli ultimi 10 campioni contro i 10 precedenti (+1 sale, -1 scende, 0 stabile)
inline int histTrend(int m) {
  if (histCount < 20) return 0;
  long a = 0, b = 0;
  for (int i = 0; i < 10; i++) { a += histAt(m, histCount - 1 - i); b += histAt(m, histCount - 11 - i); }
  long d = (a - b) / 10, th = max(2, histScale(m) / 20);
  return d > th ? 1 : d < -th ? -1 : 0;
}

// Curva: il campione piu' recente a destra; fill = colore dell'area sotto (0 = nessuna)
inline void histPlot(GFXcanvas16 &c, int x, int y, int w, int h, int m, uint16_t line, uint16_t fill, int scale) {
  int n = min(histCount, w);
  int px = 0, py = 0;
  for (int k = 0; k < n; k++) {
    int v = histAt(m, histCount - n + k);
    int xx = x + w - n + k, yy = y + h - 1 - (int)((long)min(v, scale) * (h - 1) / scale);
    if (fill) c.drawFastVLine(xx, yy, y + h - yy, fill);
    if (k > 0) c.drawLine(px, py, xx, yy, line);
    px = xx; py = yy;
  }
}

// Gestisce "G,<metrica>"; restituisce true se la riga era un comando G
inline bool parseGraphCmd(const String &t) {
  if (!t.startsWith("G,")) return false;
  String k = t.substring(2);
  k.trim();
  k.toLowerCase();
  if (k == "auto") graphAuto = true;
  for (int m = 0; m < MT_COUNT; m++)
    if (k == METRIC_KEY[m]) { graphMetric = m; graphAuto = false; }
  Serial.println("OK " + t);
  return true;
}
