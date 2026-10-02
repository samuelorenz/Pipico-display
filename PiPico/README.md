# PiPico Pip-Boy Display

Firmware per **Raspberry Pi Pico (RP2040)** che pilota un display TFT **ST7789 240x240** via SPI hardware e lo trasforma in un mini **Pip-Boy** in stile *Fallout*:

- intro all'accensione con accensione CRT, logo **Vault-Tec** e righe di boot;
- schermata **STAT** con il **Vault Boy** animato, tab, barra HP / LEVEL / AP ed effetto scanline;
- modalità di test comandata da **seriale**: grafico in tempo reale dei numeri ricevuti e visualizzazione di testo libero;
- animazione extra con sinusoide verde.

Upload automatico del firmware senza premere BOOTSEL (Windows/PowerShell).

> Progetto amatoriale senza scopo commerciale. *Fallout*, *Pip-Boy*, *Vault Boy* e *Vault-Tec* sono marchi di Bethesda Softworks. Vedi la sezione [Note sui diritti](#note-sui-diritti).

## Hardware

- Raspberry Pi Pico (RP2040)
- Display TFT ST7789 240x240 (SPI)
- Cavo USB

### Collegamenti

I numeri sono GPIO, non pin fisici. Il display usa SPI0 in `SPI_MODE3`.

| Display | Pico GPIO | Pin fisico |
|---------|-----------|------------|
| SCL (SCK)  | GP18 | 24 |
| SDA (MOSI) | GP19 | 25 |
| DC         | GP16 | 21 |
| CS         | GP17 | 22 |
| RES (RST)  | GP21 | 27 |
| BL         | GP22 | 29 |
| VCC / GND  | 3V3 / GND | |

Se il tuo schermo è 240x320, cambia `tft.init(240, 240, ...)` in `tft.init(240, 320, ...)` in [src/main.cpp](src/main.cpp) (la grafica è disegnata per 240x240).

## Software

- [PlatformIO](https://platformio.org/) con core Arduino di [earlephilhower](https://github.com/earlephilhower/arduino-pico) (platform `maxgerhardt/platform-raspberrypi`)
- Librerie (installate automaticamente): `Adafruit ST7735 and ST7789 Library`, `Adafruit GFX Library`
- Per rigenerare gli sprite (opzionale): Python 3 con `pillow` e `numpy`

## Build e upload

```bash
pio run -t upload
```

Se `pio` non è nel PATH di Windows usa l'eseguibile dell'ambiente PlatformIO: `%USERPROFILE%\.platformio\penv\Scripts\pio.exe`.

L'upload usa [upload.ps1](upload.ps1):

1. Se il Pico non è già in modalità BOOTSEL, lo riavvia aprendo e chiudendo la porta seriale a **1200 baud**.
2. Aspetta il volume `RPI-RP2` (max 10 secondi).
3. Copia il file `.uf2` sul Pico.

### Porta seriale (COM)

La porta è impostata a **COM7** in [platformio.ini](platformio.ini) (`monitor_port`) e come default in [upload.ps1](upload.ps1). Windows può cambiare il numero quando ricolleghi il cavo: controlla in Gestione dispositivi (voce *Dispositivo seriale USB*) e aggiorna i due file.

Se compare "COM7 è occupata (Serial Monitor aperto?)" ma poi "Caricato su E:", l'upload è comunque riuscito. Se invece fallisce, chiudi il Serial Monitor oppure tieni premuto BOOTSEL mentre ricolleghi il cavo e rilancia.

## Cosa mostra il display

### 1. Intro (a ogni accensione)

1. Una riga luminosa che si apre, come un vecchio schermo CRT che si accende.
2. Il logo **Vault-Tec** (testo, anello con barre e slogan) in dissolvenza.
3. Le righe di boot scritte lettera per lettera: `PIP-BOY 3000 MK IV`, `INIT SPI BUS ... OK`, `LOADING VAULT BOY ... OK`, `WELCOME, DWELLER`.

### 2. Schermata STAT (modalità `pip`, predefinita)

- Tab `STAT / INV / DATA / MAP / RADIO` con la tacca sotto STAT e sottoschede `STATUS / SPECIAL / PERKS`.
- Tacche di mira attorno al personaggio.
- **Vault Boy** verde con effetto "camminata": dondolio laterale e rimbalzo a ogni passo, più leggero sfarfallio della luminosità.
- Barra inferiore `HP 90/90`, `LEVEL 1` con barra XP e `AP 70/70`.
- Barra di scansione CRT che scende continuamente.

> L'omino parte da **un solo fotogramma** (la schermata di riferimento) e il movimento è simulato con rotazione e rimbalzo. Per una vera camminata servono più fotogrammi: vedi [Sprite](#sprite-e-grafica).

### 3. Modalità sinusoide (`anim`)

Sinusoide verde che scorre con ampiezza variabile e griglia.

### 4. Modalità grafico seriale

Il display ha un'area testo in alto (ultima riga ricevuta) e un grafico a linee in basso con scala automatica sugli ultimi 238 campioni.

## Comandi da seriale

Apri il Serial Monitor a **115200 baud** (`pio device monitor`) e scrivi una riga seguita da Invio:

| Input | Effetto |
|-------|---------|
| `pip` | schermata Pip-Boy con Vault Boy |
| `intro` | rivede l'intro con logo Vault-Tec |
| `anim` | sinusoide verde animata |
| un numero (`23.5`, `-4`) | passa al grafico e aggiunge il valore (testo giallo in alto) |
| testo libero (`ciao`) | passa al grafico e mostra il testo in alto, in bianco (non viene plottato) |
| `clear` | azzera il grafico |

Qualsiasi input diverso da `pip`, `intro` e `anim` ferma le animazioni e passa alla modalità grafico.

> Se scrivi direttamente nel terminale di PowerShell invece che nel Serial Monitor, il comando non arriva al Pico.

## Prestazioni

- Per impostazione predefinita la libreria Adafruit usa SPI a **8 MHz** e un `fillScreen` completo costa circa 115 ms, per cui il display sembra lento. Il firmware imposta `tft.setSPISpeed(40000000)` (40 MHz). Se vedi pixel sporchi o schermo instabile abbassa a 32 MHz; con cavi corti puoi provare fino a 62.5 MHz.
- Le animazioni sono disegnate **fuori schermo** su un `GFXcanvas16` 240x240 (circa 115 KB di RAM) e inviate al display in un unico blocco con `drawRGBBitmap`: niente sfarfallio e circa 30-40 fps per la sinusoide.
- Il disegno degli sprite con rotazione è fatto pixel per pixel dalla CPU: ci sono circa 12.000 pixel per frame.
- Uso di memoria: RAM circa 4%, flash circa 5.7% (circa 120 KB, di cui circa 52 KB di sprite).

## Sprite e grafica

Il Vault Boy e il logo Vault-Tec sono **maschere alpha a 8 bit** (0 = trasparente, 255 = pieno) memorizzate in [src/sprites.h](src/sprites.h) e disegnate in verde Pip-Boy (RGB 40, 255, 100) da `blitAlpha()` in [src/main.cpp](src/main.cpp), con luminosità e rotazione regolabili.

| Sprite | Dimensione | Origine |
|--------|------------|---------|
| `VB_SPRITE` | 78 x 148 px | ritaglio del Vault Boy dalla schermata STAT del Pip-Boy |
| `VAULTTEC_LOGO` | 240 x 174 px | logo Vault-Tec (giallo su blu), binarizzato in base al canale rosso |

### Rigenerare gli sprite

[tools/make_sprites.py](tools/make_sprites.py) ritaglia, filtra e converte le immagini sorgente in `src/sprites.h`:

```bash
pip install pillow numpy
python tools/make_sprites.py <schermata_pipboy.png> <logo_vaulttec.png>
```

Le immagini sorgente **non sono incluse** nel repository. I riquadri di ritaglio (`VB_BOX`, `LOGO_BOX`) e la scala (`VB_SCALE`) sono costanti in cima allo script, da regolare se usi immagini di dimensioni diverse. Per avere una camminata vera: genera più sprite (un fotogramma per posa) e alterna in `drawPipFrame()` in base a `walkT`.

## Struttura del progetto

```
platformio.ini        configurazione PlatformIO (board, librerie, upload, porta)
upload.ps1            script di upload automatico (UF2 su RPI-RP2)
src/main.cpp          firmware: intro, Pip-Boy, sinusoide, grafico seriale
src/sprites.h         sprite generati (non modificare a mano)
tools/make_sprites.py generatore degli sprite
```

### Organizzazione di `main.cpp`

| Funzione | Ruolo |
|----------|-------|
| `playIntro()` | CRT, logo Vault-Tec, righe di boot |
| `drawPipFrame()` | un fotogramma della schermata STAT |
| `blitAlpha()` | disegna una maschera alpha in verde con luminosità e rotazione |
| `drawAnimFrame()` | un fotogramma della sinusoide |
| `drawPlot()` / `drawText()` | grafico e testo della modalità seriale |
| `handleLine()` | interpreta i comandi ricevuti da seriale |

## Risoluzione problemi

| Problema | Soluzione |
|----------|-----------|
| `pio` non riconosciuto | usa `%USERPROFILE%\.platformio\penv\Scripts\pio.exe` |
| Upload fallito / "RPI-RP2 non trovata" | tieni premuto BOOTSEL e ricollega il cavo; chiudi il Serial Monitor |
| La porta COM è cambiata | aggiorna `monitor_port` in `platformio.ini` e `$Port` in `upload.ps1` |
| Schermo nero | controlla BL (GP22) e che `SPI_MODE3` sia corretto per il tuo modulo |
| Pixel sporchi / immagine instabile | abbassa `setSPISpeed` a 32 MHz |
| Immagine spostata o tagliata | verifica `tft.init(240, 240, ...)` e la rotazione |
| I comandi non arrivano | scrivi nel Serial Monitor a 115200 baud, non nel terminale PowerShell |

## Note sui diritti

Gli sprite del Vault Boy e del logo Vault-Tec derivano da immagini del videogioco *Fallout* (© Bethesda Softworks). Sono usati qui per un progetto personale e amatoriale. Se pubblichi o distribuisci il repository, valuta se includere `src/sprites.h` (puoi invece lasciarlo fuori e distribuire solo lo script `tools/make_sprites.py`, in modo che ognuno lo generi con le proprie immagini). Il codice del firmware è tuo e puoi licenziarlo come preferisci.
