// Dati dal PC, letti una sola volta dal firmware e visibili a tutti i temi.
//   S,cpu,ram,disco,gpu,tempGpu,netKB,uptimeOre   statistiche principali (ogni secondo)
//   X,tempCpu,batteria,inCarica                    extra: temperatura CPU (0 = n.d.), batteria % (-1 = n.d.)
//   P,nome,percentuale                             processo che usa piu' CPU
#pragma once
#include "history.h"

struct PcStats { int cpu, ram, disk, gpu, temp, net, up; unsigned long at; };
inline PcStats pc = {0, 0, 0, 0, 0, 0, 0, 0};

struct PcExtra { int cpuTemp, batt, plugged, procPct; char proc[16]; unsigned long at; };
inline PcExtra pcx = {0, -1, 0, 0, "", 0};

inline bool pcLive() { return pc.at != 0 && millis() - pc.at < 4000; }  // niente dati per 4 s = offline
inline bool pcAlarm() { return pcLive() && (pc.cpu >= 90 || pc.gpu >= 90 || pc.temp >= 85); }
inline bool extraLive() { return pcx.at != 0 && millis() - pcx.at < 4000; }

// Legge una riga S / X / P; restituisce true se la riga era di questo tipo
inline bool parseStats(const String &t) {
  if (t.length() < 3 || t[1] != ',') return false;
  char kind = t[0];
  if (kind != 'S' && kind != 'X' && kind != 'P') return false;

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
  histPush(pc.cpu, pc.ram, pc.disk, pc.gpu, pc.temp, pc.net);  // storico per la schermata Grafico
  return true;
}
