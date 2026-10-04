// Interfaccia di un tema grafico. Ogni tema (fallout, skynet, cyberpunk, alien) definisce una
// costante Theme; src/main.cpp sceglie quello attivo, gli passa i comandi e ne salva lo stato.
#pragma once
#include <Arduino.h>

// Stato di un tema che sopravvive allo spegnimento (salvato nella flash dal firmware)
struct ThemeState {
  uint32_t flags;   // elementi visibili (bit E_* del tema); 0 = non impostato
  uint8_t screen;   // schermata corrente (numero specifico del tema)
  char name[17];    // nome mostrato (amministratore / bersaglio / netrunner)
};

struct Theme {
  const char *id;                            // "fallout", "skynet", ...
  void (*begin)();                           // colori e boot, chiamata quando il tema diventa attivo
  void (*tick)();                            // disegna un fotogramma
  void (*handle)(const String &);            // comando o testo ricevuto da seriale
  void (*getState)(ThemeState &);
  void (*setState)(const ThemeState &);
  void (*nextScreen)();                      // pulsante: schermata successiva
  void (*reboot)();                          // rivede il boot
};

extern const Theme THEME_FALLOUT;
extern const Theme THEME_SKYNET;
extern const Theme THEME_CYBERPUNK;
extern const Theme THEME_ALIEN;
