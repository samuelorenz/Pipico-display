// Firmware unico: inizializza lo schermo e sceglie quale tema mostrare (fallout, skynet,
// cyberpunk). Si occupa di tutto cio' che non dipende dal tema:
//   - lettura della seriale, statistiche del PC e comandi comuni
//   - scelta del tema (comando T, oppure pulsante)
//   - schermata successiva (pulsante)
//   - salvataggio nella flash di tema, elementi visibili, schermate, nome e grafico
//   - spegnimento della retroilluminazione dopo un po' di inattivita'
//
// Comandi comuni (gli altri vengono passati al tema attivo):
//   ?                 risponde "ID,<tema>"
//   D                 risponde "STATE,<tema>,<elementi>,<schermata>,<spegnimento>,<metrica>,<auto>,<nome>"
//   T,<tema>|next     cambia tema (fallout | skynet | cyberpunk)
//   B,<secondi>       spegne la retroilluminazione dopo N secondi senza dati (0 = mai)
//   K,reset           cancella le impostazioni salvate e riavvia
//   G,<metrica>       grafico: cpu | ram | dsk | gpu | tmp | net | auto
//   S,... X,... P,... dati dal PC (vedi common/stats.h)
//
// Pulsanti opzionali verso GND (con pull-up interno): GP14 = tema successivo, GP15 = schermata successiva.
#include <EEPROM.h>
#include "common/shared.h"
#include "common/theme.h"
#include "common/stats.h"

#define BTN_THEME   14   // pin fisico 19
#define BTN_SCREEN  15   // pin fisico 20

// Con -DONLY_<TEMA> (env fallout, skynet, ...) il firmware contiene un solo tema
#if defined(ONLY_FALLOUT)
const Theme *const THEMES[] = {&THEME_FALLOUT};
#elif defined(ONLY_SKYNET)
const Theme *const THEMES[] = {&THEME_SKYNET};
#elif defined(ONLY_CYBERPUNK)
const Theme *const THEMES[] = {&THEME_CYBERPUNK};
#else
const Theme *const THEMES[] = {&THEME_FALLOUT, &THEME_SKYNET, &THEME_CYBERPUNK};
#endif
const int NTHEMES = sizeof(THEMES) / sizeof(THEMES[0]);

// Posizione fissa di ogni tema nella flash, indipendente da quali temi sono compilati
static int slotOf(const Theme *t) {
  const char *ids[3] = {"fallout", "skynet", "cyberpunk"};
  for (int i = 0; i < 3; i++) if (!strcmp(t->id, ids[i])) return i;
  return 0;
}

// ---------- Impostazioni salvate ----------
struct Saved {
  uint32_t magic;
  uint8_t theme, gMetric, gAuto, pad;
  uint16_t offSec, pad2;
  ThemeState st[3];
};
const uint32_t MAGIC = 0x50494332;  // "PIC2" (cambiato quando e' stato tolto il tema pipboy: le vecchie impostazioni si ignorano)
Saved cur;                  // ultimo stato noto (quello salvato o da salvare)
bool savePending = false;
unsigned long saveAt = 0;
uint16_t offSec = 300;      // spegnimento retroilluminazione (0 = mai)

int active = 0;
unsigned long lastActivity = 0, lastCheck = 0;
bool backlightOn = true;

static void snapshot(Saved &s) {
  s = cur;  // mantiene gli slot dei temi non compilati
  s.magic = MAGIC;
  for (int i = 0; i < NTHEMES; i++) {
    ThemeState st;
    memset(&st, 0, sizeof(st));
    THEMES[i]->getState(st);
    s.st[slotOf(THEMES[i])] = st;
  }
  s.theme = slotOf(THEMES[active]);
  s.gMetric = (uint8_t)graphMetric;
  s.gAuto = graphAuto ? 1 : 0;
  s.offSec = offSec;
}

// Dopo ogni cambiamento: se lo stato e' diverso da quello salvato, lo salva fra 3 secondi
static void checkDirty() {
  Saved s;
  memset(&s, 0, sizeof(s));
  snapshot(s);
  if (memcmp(&s, &cur, sizeof(s)) != 0) {
    cur = s;
    savePending = true;
    saveAt = millis() + 3000;
  }
}

static void flushSave() {
  if (!savePending || millis() < saveAt) return;
  savePending = false;
  EEPROM.put(0, cur);
  EEPROM.commit();  // scrive la flash: blocca il Pico per qualche decina di ms
}

static void loadSettings() {
  EEPROM.begin(256);
  Saved s;
  EEPROM.get(0, s);
  memset(&cur, 0, sizeof(cur));
  if (s.magic != MAGIC) { cur.magic = MAGIC; return; }
  cur = s;
  for (int i = 0; i < NTHEMES; i++) THEMES[i]->setState(s.st[slotOf(THEMES[i])]);
  graphMetric = min<int>(s.gMetric, MT_COUNT - 1);
  graphAuto = s.gAuto;
  offSec = s.offSec;
  for (int i = 0; i < NTHEMES; i++) if (slotOf(THEMES[i]) == s.theme) active = i;
}

// ---------- Tema e retroilluminazione ----------
static void activate(int i) {
  active = (i % NTHEMES + NTHEMES) % NTHEMES;
  THEMES[active]->begin();
  Serial.println(String("ID,") + THEMES[active]->id);
  checkDirty();
}

static void wake() {
  lastActivity = millis();
  if (!backlightOn) { backlightOn = true; digitalWrite(TFT_BL, HIGH); }
}

// ---------- Comandi ----------
static void handleLine(const String &s) {
  String t = s;
  t.trim();
  if (t.length() == 0) return;

  if (parseStats(t)) return;  // S / X / P: dati del PC
  if (t == "?") { Serial.println(String("ID,") + THEMES[active]->id); return; }
  if (t == "D") {  // stato corrente, per la GUI: STATE,tema,elementi,schermata,spegnimento,metrica,auto,nome
    ThemeState st;
    memset(&st, 0, sizeof(st));
    THEMES[active]->getState(st);
    Serial.println(String("STATE,") + THEMES[active]->id + "," + st.flags + "," + st.screen + "," + offSec + "," +
                   graphMetric + "," + (graphAuto ? 1 : 0) + "," + st.name);
    return;
  }
  if (parseGraphCmd(t)) { checkDirty(); return; }
  if (t.startsWith("T,")) {
    String id = t.substring(2);
    id.trim();
    if (id == "next") { activate(active + 1); return; }
    for (int i = 0; i < NTHEMES; i++) if (id == THEMES[i]->id) { activate(i); return; }
    Serial.println("ERR tema sconosciuto: " + id);
    return;
  }
  if (t.startsWith("B,")) {
    offSec = (uint16_t)constrain(t.substring(2).toInt(), 0, 65000);
    Serial.println("OK " + t);
    checkDirty();
    return;
  }
  if (t == "K,reset") {
    EEPROM.put(0, Saved{});
    EEPROM.commit();
    Serial.println("OK impostazioni cancellate, riavvio");
    delay(100);
    rp2040.reboot();
  }
  THEMES[active]->handle(t);
  checkDirty();
}

// ---------- Setup / loop ----------
String line;

void setup() {
  Serial.begin(115200);
  SPI.setSCK(18);
  SPI.setTX(19);
  pinMode(TFT_BL, OUTPUT);
  digitalWrite(TFT_BL, HIGH);
  pinMode(BTN_THEME, INPUT_PULLUP);
  pinMode(BTN_SCREEN, INPUT_PULLUP);

  tft.init(240, 240, SPI_MODE3);  // se il tuo schermo e' 240x240; per 240x320 cambia in init(240, 320)
  tft.setSPISpeed(40000000);      // default Adafruit = 8 MHz; il ST7789 regge 40-62 MHz
  tft.setRotation(0);
  tft.fillScreen(ST77XX_BLACK);

  loadSettings();
  activate(active);
  lastActivity = millis();
}

void loop() {
  while (Serial.available()) {
    char c = Serial.read();
    wake();
    if (c == '\n' || c == '\r') {
      handleLine(line);
      line = "";
    } else if (line.length() < 64) {
      line += c;
    }
  }

  // pulsanti (attivi bassi), con 250 ms di antirimbalzo
  static unsigned long lastBtn = 0;
  if (millis() - lastBtn > 250) {
    if (digitalRead(BTN_THEME) == LOW)       { lastBtn = millis(); wake(); if (NTHEMES > 1) activate(active + 1); }
    else if (digitalRead(BTN_SCREEN) == LOW) { lastBtn = millis(); wake(); THEMES[active]->nextScreen(); checkDirty(); }
  }

  // retroilluminazione: si spegne se non arriva nulla per offSec secondi
  if (backlightOn && offSec && millis() - lastActivity > (unsigned long)offSec * 1000UL) {
    backlightOn = false;
    digitalWrite(TFT_BL, LOW);
  }

  if (millis() - lastCheck > 1000) { lastCheck = millis(); checkDirty(); }
  flushSave();

  if (backlightOn) THEMES[active]->tick();
  else delay(50);
}
