// Dati dal PC, letti una sola volta dal firmware e visibili a tutti i temi.
//   S,cpu,ram,disco,gpu,tempGpu,netKB,uptimeOre   statistiche principali (ogni secondo)
//   X,tempCpu,batteria,inCarica                    extra: temperatura CPU (0 = n.d.), batteria % (-1 = n.d.)
//   P,nome,percentuale                             processo che usa piu' CPU
//   Y,giu,su,discoLett,discoScritt,ping,processi   rete in KB/s, disco in KB/s, latenza in ms (-1 = n.d.)
#pragma once
#include "history.h"

struct PcStats { int cpu, ram, disk, gpu, temp, net, up; unsigned long at; };
inline PcStats pc = {0, 0, 0, 0, 0, 0, 0, 0};

struct PcExtra {
  int cpuTemp, batt, plugged, procPct; char proc[16]; unsigned long at;
  int down, up, dr, dw, ping, procs; unsigned long netAt;
};
inline PcExtra pcx = {0, -1, 0, 0, "", 0, 0, 0, 0, 0, -1, 0, 0};

// Soglie di allarme (comando A,<cpu>,<gpu>,<temp>,<cicalino>; 0 = soglia disattivata)
inline int alarmCpu = 90, alarmGpu = 90, alarmTemp = 85;
inline bool alarmBeep = true;

inline bool pcLive() { return pc.at != 0 && millis() - pc.at < 4000; }  // niente dati per 4 s = offline
inline bool pcAlarm() {
  return pcLive() && ((alarmCpu && pc.cpu >= alarmCpu) || (alarmGpu && pc.gpu >= alarmGpu) || (alarmTemp && pc.temp >= alarmTemp));
}
inline bool extraLive() { return pcx.at != 0 && millis() - pcx.at < 4000; }
inline bool netExtraLive() { return pcx.netAt != 0 && millis() - pcx.netAt < 4000; }

// 1234 -> "1.2M", 56 -> "56K"
inline void fmtRate(char *o, size_t n, int kb) {
  if (kb >= 1000) snprintf(o, n, "%d.%dM", kb / 1000, (kb % 1000) / 100);
  else snprintf(o, n, "%dK", kb);
}

// Bordo e angoli che lampeggiano a schermo intero quando scatta un allarme (chiamata prima di inviare il fotogramma)
inline void alertOverlay(GFXcanvas16 &c, uint16_t col, int frame) {
  if (!pcAlarm() || (frame / 5) % 2) return;
  for (int i = 0; i < 3; i++) c.drawRect(i, i, 240 - 2 * i, 240 - 2 * i, col);
  c.fillTriangle(0, 0, 22, 0, 0, 22, col);       c.fillTriangle(239, 0, 217, 0, 239, 22, col);
  c.fillTriangle(0, 239, 22, 239, 0, 217, col);  c.fillTriangle(239, 239, 217, 239, 239, 217, col);
}

// Legge una riga S / X / P; restituisce true se la riga era di questo tipo
inline bool parseStats(const String &t) {
  if (t.length() < 3 || t[1] != ',') return false;
  char kind = t[0];
  if (kind != 'S' && kind != 'X' && kind != 'P' && kind != 'Y') return false;

  if (kind == 'P') {
    int q = t.indexOf(',', 2);
    if (q < 0) return true;
    String name = t.substring(2, q);
    strncpy(pcx.proc, name.c_str(), sizeof(pcx.proc) - 1);
    pcx.proc[sizeof(pcx.proc) - 1] = 0;
    pcx.procPct = constrain(t.substring(q + 1).toInt(), 0, 100);
    pcx.at = millis();
    return true;
  }

  int v[7], n = 0, p = 2;
  while (n < 7 && p <= (int)t.length()) {
    int q = t.indexOf(',', p);
    if (q < 0) q = t.length();
    v[n++] = t.substring(p, q).toInt();
    p = q + 1;
  }
  if (kind == 'Y') {
    if (n < 6) return true;
    pcx.down = max(v[0], 0); pcx.up = max(v[1], 0);
    pcx.dr = max(v[2], 0); pcx.dw = max(v[3], 0);
    pcx.ping = v[4]; pcx.procs = max(v[5], 0);
    pcx.netAt = millis();
    return true;
  }
  if (kind == 'X') {
    if (n < 3) return true;
    pcx.cpuTemp = constrain(v[0], 0, 150);
    pcx.batt = v[1] < 0 ? -1 : constrain(v[1], 0, 100);
    pcx.plugged = v[2] ? 1 : 0;
    pcx.at = millis();
    return true;
  }
  if (n < 7) return true;  // riga incompleta: ignorata
  pc.cpu = constrain(v[0], 0, 100); pc.ram = constrain(v[1], 0, 100);
  pc.disk = constrain(v[2], 0, 100); pc.gpu = constrain(v[3], 0, 100);
  pc.temp = constrain(v[4], 0, 150); pc.net = max(v[5], 0); pc.up = max(v[6], 0);
  pc.at = millis();
  histPush(pc.cpu, pc.ram, pc.disk, pc.gpu, pc.temp, pc.net, netExtraLive() ? max(pcx.ping, 0) : 0);  // storico per la schermata Grafico
  return true;
}
