# PiPico Display

Firmware per **Raspberry Pi Pico (RP2040)** che pilota un display TFT **ST7789 240x240** via SPI hardware e lo trasforma in un monitor per il PC a tema cinematografico/videoludico. Un **firmware unico** contiene tre temi e si cambia tema al volo, da seriale, dalla GUI o con un pulsante:

| Tema (`T,<tema>`) | Descrizione | Colore |
|-------------------|-------------|--------|
| `fallout` | Terminale **RobCo Industries** di *Fallout*: boot di Fallout 4, monitor di sistema, minigioco di hacking, terminale seriale | verde |
| `skynet`  | Visione del **T-800** / terminale **Skynet** di *Terminator*: intro con logo, HUD con mirino, codice 6502, analisi, possible responses, terminale seriale | rosso |
| `cyberpunk` | **Netrunner** di *Cyberpunk 2077*: boot delle ottiche, monitor con quickhack e scanner, minigioco Breach Protocol, terminale seriale | giallo / ciano / rosso |

Tutti i temi hanno una modalità di test da **seriale** (grafico dei numeri ricevuti e testo libero), la schermata **Grafico** delle statistiche e mostrano le **statistiche del PC** in tempo reale (vedi [Statistiche del PC](#statistiche-del-pc-companion)). Il Pico **ricorda** tema, elementi visibili, schermata, nome e grafico anche dopo lo spegnimento. L'upload è automatico, senza premere BOOTSEL (Windows/PowerShell).

> Progetto amatoriale senza scopo commerciale. *Fallout*, *RobCo* (Bethesda Softworks), *Terminator*, *Skynet* e *Cyberdyne Systems* sono marchi dei rispettivi proprietari, come *Cyberpunk 2077*, *Night City*, *Kiroshi* e *Netwatch* (CD PROJEKT). Vedi la sezione [Note sui diritti](#note-sui-diritti).

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
- Per gli script in `tools/`: Python 3 e `pip install -r tools/requirements.txt` (`psutil`, `pyserial`, `pillow`, `numpy`)

## Build e upload

```bash
pio run -t upload               # firmware unico con tutti i temi (env "all", predefinito)
pio run -e fallout   -t upload  # solo RobCo Industries (firmware piu' piccolo)
pio run -e skynet    -t upload  # solo Skynet / T-800
pio run -e cyberpunk -t upload  # solo Cyberpunk 2077
pio run                         # solo compilazione
```

Il firmware `all` pesa circa 180 KB su 2 MB di flash e usa circa 8% della RAM statica più un canvas condiviso da 115 KB. Gli ambienti con un solo tema compilano il tema con `-DONLY_<TEMA>`; il resto del comportamento (comandi, salvataggio, pulsanti) è identico.

In VS Code puoi scegliere l'ambiente dalla barra di PlatformIO in basso e poi premere la freccia di upload. Se `pio` non è nel PATH di Windows usa l'eseguibile dell'ambiente PlatformIO: `%USERPROFILE%\.platformio\penv\Scripts\pio.exe`.

L'upload usa [upload.ps1](upload.ps1):

1. Ferma `pc_stats.py` e la GUI se sono in esecuzione (tengono aperta la seriale).
2. Se il Pico non è già in modalità BOOTSEL, lo riavvia aprendo e chiudendo la porta seriale a **1200 baud**.
3. Aspetta il volume `RPI-RP2` (max 10 secondi).
4. Copia il file `.uf2` sul Pico.
5. **Rilancia** da solo ciò che aveva fermato (`pc_stats.py` nascosto, la GUI in una finestra) dopo 6 secondi, quando il Pico è ripartito.

### Cambiare tema e schermata

| Come | Effetto |
|------|---------|
| comando `T,fallout` / `T,skynet` / `T,cyberpunk` / `T,next` | cambia tema (il boot del nuovo tema parte subito) |
| menu *Build* della [GUI](#gui-di-debug) | come sopra |
| pulsante su **GP14** (pin fisico 19) verso GND | tema successivo |
| pulsante su **GP15** (pin fisico 20) verso GND | schermata successiva del tema attivo |

I pulsanti sono facoltativi: i pin usano la resistenza di pull-up interna e non fanno nulla se non sono collegati. Cambiare tema non cancella lo stato degli altri: ognuno mantiene i propri elementi, la propria schermata e il proprio nome.

### Impostazioni salvate nella flash

Il Pico salva da solo, 3 secondi dopo l'ultimo cambiamento, **il tema attivo, gli elementi visibili, la schermata e il nome di ogni tema, il grafico scelto e il tempo di spegnimento dello schermo**. Al riavvio riparte esattamente da lì. I dati stanno in un settore riservato della flash (emulazione EEPROM) che l'upload di un nuovo firmware **non cancella**.

| Comando | Effetto |
|---------|---------|
| `D` | il Pico risponde `STATE,<tema>,<elementi>,<schermata>,<spegnimento>,<metrica>,<auto>,<nome>` con lo stato attuale (la [GUI](#gui-di-debug) lo usa per impostare le caselle) |
| `K,reset` | cancella le impostazioni salvate e riavvia: tutto torna ai valori di fabbrica |

### Spegnimento dello schermo

Se per `B` secondi non arriva nulla dalla seriale (nessuna statistica, nessun comando, nessun pulsante), la retroilluminazione si spegne; riparte appena arriva un dato. Per questo con il PC spento o in standby lo schermo si spegne da solo. Il valore predefinito è 300 secondi (5 minuti); `B,<secondi>` lo cambia (`B,0` = mai) e viene salvato nella flash.

### Porta seriale (COM)

[upload.ps1](upload.ps1) **trova da solo la porta del Pico** (cerca il dispositivo USB con VID `2E8A`), quindi il numero COM non conta per l'upload. Windows può cambiarlo a ogni ricollegamento. Per il Serial Monitor invece `monitor_port` in [platformio.ini](platformio.ini) è fisso (ora `COM7`): se cambia, aggiornalo (Gestione dispositivi, voce *Dispositivo seriale USB*) oppure rimuovi la riga e PlatformIO la cerca da solo.

Aprire la porta a 1200 baud fa riavviare il Pico in BOOTSEL: .NET segnala quindi "dispositivo inesistente", ma è normale e lo script lo ignora. Se compare un avviso diverso, o l'unità `RPI-RP2` non appare, chiudi il Serial Monitor oppure tieni premuto BOOTSEL mentre ricolleghi il cavo e rilancia.

## Statistiche del PC (companion)

Tutti i temi mostrano in tempo reale CPU, RAM, disco, rete, uptime e, con una scheda NVIDIA, GPU e temperatura; in più, se disponibili, temperatura della CPU, batteria e il processo che usa più CPU. Un piccolo script sul PC le legge e le manda al Pico via USB; il Pico le integra nella grafica già presente, senza una schermata separata.

### Avvio

```bash
pip install -r tools/requirements.txt
python tools/pc_stats.py              # trova da solo il Pico (VID 2E8A)
python tools/pc_stats.py --port COM7  # porta indicata a mano
python tools/pc_stats.py --test       # stampa i dati senza inviarli
```

Lo script invia i dati una volta al secondo e si riconnette da solo se il Pico viene scollegato o ricaricato. Se non arrivano dati per 4 secondi, il display torna alla schermata normale (modalità offline).

**Avvio automatico con Windows:**

```bash
powershell -ExecutionPolicy Bypass -File tools\autostart.ps1            # installa
powershell -ExecutionPolicy Bypass -File tools\autostart.ps1 -Status    # controlla
powershell -ExecutionPolicy Bypass -File tools\autostart.ps1 -Remove    # rimuove
```

Crea il collegamento `PiPico Display Stats` nella cartella Esecuzione automatica dell'utente: avvia `pc_stats.py` con `pythonw.exe` (senza finestra) a ogni accesso. Quando apri la GUI, questa ferma lo script e lo rilancia alla chiusura.

### Protocollo seriale

Righe di testo a 115200 baud, una per messaggio:

```
S,cpu,ram,disco,gpu,tempGpu,netKB,uptimeOre     S,34,62,48,21,55,120,5
X,tempCpu,batteria,inCarica                     X,62,85,1
P,processo,cpu                                  P,CHROME,23
```

| Riga | Significato |
|------|-------------|
| `S` | percentuali 0-100, temperatura GPU in °C, rete in KB/s e uptime in ore (tutti interi); alimenta anche i [grafici](#grafici-delle-statistiche) |
| `X` | temperatura CPU in °C (`0` = non disponibile), batteria in % (`-1` = non disponibile, per esempio sul PC fisso), `1` se in carica |
| `P` | nome (maiuscolo, senza virgole, max 15 caratteri) e percentuale CPU del processo più pesante |

Le righe `S`, `X` e `P` non cambiano schermata e non compaiono nel log del terminale. Chiunque può quindi scrivere un proprio sender in qualsiasi linguaggio. Il codice che le legge è in [src/common/stats.h](src/common/stats.h), letto una volta sola per tutti i temi.

### Come compaiono

| | `fallout` (RobCo) | `skynet` (T-800) |
|-|-------------------|-------------------|
| CPU, RAM, disco, GPU | quattro righe con barra ASCII a 20 caratteri (oppure caratteri grandi con barra a 10 segmenti) e percentuale | pannello a sinistra con testo grande e barra sotto ogni valore |
| Temperatura GPU, rete, uptime | righe `GPU TEMP`, `NET`, `UPTIME` sotto la linea di `=` | `TMP`, `NET`, `UP` nel pannello a sinistra |
| Stato | `> LOG: all systems nominal`, menu con voce evidenziata | `LOAD`, `GPU TEMP` e `THREAT` (`LOW`, `MODERATE`, `HIGH`, `CRITICAL`) |
| Allarme (CPU o GPU ≥ 90% oppure temperatura ≥ 85 °C) | i segmenti dei valori critici lampeggiano e compare `> WARNING: SYSTEM OVERLOAD` | `THREAT: CRITICAL` lampeggia e la cornice si accende |
| Reazione al carico | — | il mirino diventa più nervoso con il carico (e più piccolo, nella metà destra) |

### Grafici delle statistiche

Ogni build ha una schermata **Grafico** con lo storico di **una statistica alla volta**: CPU, RAM, disco, GPU, temperatura GPU o rete. In alternativa c'è la rotazione automatica, che cambia grafico ogni 8 secondi. Il Pico tiene in memoria gli ultimi **200 campioni** (circa 3 minuti e 20 secondi, uno per ogni riga `S,...`) per tutte e sei le statistiche, quindi cambiando grafico la curva è subito piena.

| Comando | Effetto |
|---------|---------|
| `M,graph` (oppure `graph`) | mostra la schermata Grafico |
| `G,cpu` / `G,ram` / `G,dsk` / `G,gpu` / `G,tmp` / `G,net` | sceglie la statistica |
| `G,auto` | rotazione automatica ogni 8 secondi |

Il fondo scala è 100 per le percentuali, almeno 100 °C per la temperatura e automatico per la rete (arrotondato a 1, 2 o 5 × 10ⁿ KB/s).

| Tema | Stile del grafico |
|-------|-------------------|
| `fallout` | `-RobCo Telemetry: CPU LOAD-`, selettore `CPU RAM DSK GPU TMP NET` con la voce in negativo, curva verde con area piena e quarti tratteggiati, righe `NOW / MIN / MAX / AVG` e `> LOG: trend rising/falling/stable`, prompt `> run:// telemetry -cpu` |
| `skynet` | `TELEMETRY // CPU LOAD`, curva rossa su griglia a punti con un piccolo mirino bianco sull'ultimo valore, `CURRENT / PEAK / MEAN / SAMPLES`, `ANALYSIS: TREND RISING`, `MODE: LOCKED ON CPU` o `AUTO CYCLE` |
| `cyberpunk` | `NETRUNNER // TELEMETRY`, chip di selezione gialli con l'angolo tagliato, curva gialla (CPU), ciano (RAM, disco, rete) o rossa (GPU, temperatura) con area piena, valore attuale grande con separazione RGB, `MIN / MAX / AVG / TREND` |

Il codice comune (storico, scala, statistiche, tendenza, disegno della curva, comando `G,`) sta in [src/common/history.h](src/common/history.h), incluso da ogni tema. Il resto della grafica è nello stile di ciascun tema.

### Note

- GPU e temperatura GPU si leggono con `nvidia-smi` (driver NVIDIA). Senza, restano a 0.
- **Temperatura CPU:** Windows non la espone in modo semplice. Apri [LibreHardwareMonitor](https://github.com/LibreHardwareMonitor/LibreHardwareMonitor) e attiva *Options → Remote Web Server → Run* (porta 8085): `pc_stats.py` la legge da `http://127.0.0.1:8085/data.json`. Se non c'è, riprova ogni 30 secondi e la temperatura resta a 0 (non mostrata).
- **Batteria:** compare solo sui portatili.
- **Processo più pesante:** la percentuale è normalizzata sul numero di core (100% = tutta la CPU). Si vede nel tema RobCo (`> TOP: ...`), in Skynet (nel testo di analisi e come `TARGET`) e in Cyberpunk (sotto lo scanner).
- Lo script tiene aperta la porta seriale. [upload.ps1](upload.ps1) lo ferma e lo rilancia da solo a ogni upload. Per usare il Serial Monitor chiudi lo script.

## Build `fallout`: terminale RobCo Industries

Ricrea il terminale RobCo dei videogiochi *Fallout*: testo verde monospazio che viene "battuto" carattere per carattere con il cursore a blocco, voci di menu evidenziate in negativo ed effetti da monitor a tubo catodico. Ogni elemento si accende e si spegne in tempo reale dalla [GUI di debug](#gui-di-debug).

### Boot (a ogni accensione)

La sequenza di avvio dei terminali di *Fallout 4*, scritta lettera per lettera: `WELCOME TO ROBCO INDUSTRIES (TM) TERMLINK`, `>SET TERMINAL/INQUIRE`, `RIT-V300`, `>SET FILE/PROTECTION=OWNER:RWED ACCOUNTS.F`, `>SET HALT RESTART/MAINT`, `Initializing Robco Industries(TM) MF Boot Agent v2.3.0`, `RETROS BIOS`, `RBIOS-4.02.08.00 52EE5.E7.E8`, `Uppermem: 64 KB`, `Root (5A8)`, `Maintenance Mode`, `>RUN DEBUG/ACCOUNTS.F`. Le righe più lunghe di 40 caratteri vengono strette (passo di 5 pixel invece di 6). Alla fine compare il **logo RobCo Industries** (quello consumato di *Fallout 76*) in verde fosforo con dissolvenza, con sotto `TERMLINK PROTOCOL v2.3.0`.

### Schermate

| Schermata | Contenuto |
|-----------|-----------|
| **Monitor di sistema** (`home`) | Intestazione `ROBCO INDUSTRIES UNIFIED OPERATING SYSTEM` / `COPYRIGHT 2075-2077 ROBCO INDUSTRIES` / `-Server 76-`, titolo `-RobCo System Monitor-` con linea `====`, righe CPU / RAM / DSK / GPU (barre ASCII con caratteri pieni e ombreggiati, oppure caratteri grandi con barra a segmenti), temperatura GPU, rete, uptime, menu `> View System Log` / `> Network Diagnostics` / `> Logout` con la voce selezionata in negativo che cambia da sola, riga di stato e prompt. Senza dati dal PC mostra `-RobCo Trespasser Management System-` con lo `User Log` e `> LOG: waiting for host link...` |
| **Hacking** (`hack`) | Il minigioco della password: `ROBCO INDUSTRIES (TM) TERMLINK PROTOCOL`, `ENTER PASSWORD NOW`, `4 ATTEMPT(S) LEFT` con i blocchi, due colonne di memoria (`0xF4A0` + 12 caratteri di "spazzatura") con parole nascoste. Un selettore si muove da solo verso una parola, la evidenzia e risponde `>Entry denied.` / `>Likeness=n` (lettere giuste al posto giusto) finché non trova la password: `>Exact match!`, `ACCESS GRANTED` e nuova partita |
| **Terminale** (`term`) | Log delle righe ricevute da seriale, con prompt e cursore, e grafico dei numeri in basso |
| **Automatica** (`auto`, predefinita) | Monitor se arrivano le statistiche, altrimenti hacking |

### Effetti CRT

| Effetto | Come è fatto |
|---------|--------------|
| Bagliore | Sfondo a 6 anelli ellittici precalcolati: centro più chiaro, angoli scuri |
| Scanline | Le righe dispari vengono portate al 75% di luminosità (operazione sui bit RGB565) |
| Barra di scansione | Fascia più chiara che scende di continuo |
| Sfarfallio | Ogni tanto un fotogramma con il testo meno luminoso |
| Scrittura progressiva | Ogni schermata viene "battuta" a circa 14 caratteri per fotogramma (5 nel boot), con il cursore a blocco alla fine |

### Comandi seriali

| Comando | Effetto |
|---------|---------|
| `C,<maschera>` | elementi visibili, bit per bit (vedi tabella sotto); il Pico risponde `OK ...` |
| `M,auto` / `M,home` / `M,hack` / `M,term` / `M,graph` | schermata |
| `G,<metrica>` | grafico da mostrare (vedi [Grafici](#grafici-delle-statistiche)) |
| `M,boot` oppure `intro` | rivede il boot |
| `N,<nome>` | nome dell'amministratore nello User Log |
| `?` | il Pico risponde `ID,fallout` |
| `home`, `hack`, `auto` | come `M,...` |
| un numero (`23.5`) | riga `> DATA: 23.5` nel terminale e punto nel grafico |
| testo libero | riga `> testo` nel terminale |
| `clear` | cancella log e grafico |

| Bit | Elemento | Bit | Elemento |
|-----|----------|-----|----------|
| 0 | intestazione RobCo | 8 | righe `> LOG:` / stato |
| 1 | titolo e linea `====` | 9 | prompt con cursore |
| 2 | riga CPU | 10 | statistiche a caratteri grandi |
| 3 | riga RAM | 11 | scanline |
| 4 | riga disco | 12 | barra di scansione |
| 5 | riga GPU | 13 | bagliore |
| 6 | info (temp, rete, uptime / User Log) | 14 | sfarfallio |
| 7 | menu | 15 | scrittura progressiva |
| | | 16 | logo RobCo nel boot |

Esempio: `C,131071` accende tutto (predefinito), `C,66559` toglie tutti gli effetti CRT e le statistiche grandi ma tiene il logo.

## GUI di debug

[tools/display_gui.py](tools/display_gui.py) è una finestra sul PC per provare dal vivo cosa mostrare sul display, per tutti i temi:

```bash
python tools/display_gui.py
```

- **Sincronizzata con il Pico:** quando si collega legge lo stato salvato sul Pico (comandi `?` e `D`) e imposta da sola tema, caselle, schermata, nome, grafico e tempo di spegnimento. Quello che vedi nella GUI è quello che c'è sul display.
- **Build:** il menu cambia tema sul Pico (`T,<tema>`); le caselle si aggiornano quando il Pico risponde.
- **Schermata:** quelle del tema (RobCo: automatica, monitor, hacking, terminale, grafico; Skynet: HUD, terminale, grafico; Cyberpunk: automatica, monitor, Breach Protocol, terminale, grafico), più i pulsanti *Rivedi il boot* e *Pulisci terminale*.
- **Grafico:** menu con la statistica da mostrare (o la rotazione automatica). Scegliendola, il display passa alla schermata Grafico.
- **Elementi:** una casella per ogni contenuto, statistica ed effetto. Ogni modifica arriva subito al display. Ci sono anche i pulsanti *Tutto acceso* e *Tutto spento*.
- **Dati:** nome (amministratore per RobCo, bersaglio per Skynet, netrunner per Cyberpunk), **tempo di spegnimento dello schermo**, invio delle statistiche del PC (sostituisce `pc_stats.py`; mostra anche temperatura CPU, batteria e processo più pesante) e una riga libera da inviare al Pico.
- **Note per Claude:** spazio per scrivere cosa ti piace e cosa cambiare.
- **Salva configurazione:** scrive `tools/display_config.json` (una sezione per tema) e `tools/display_config.txt`.
- **Copia per Claude:** copia negli appunti un riepilogo (tema, schermata, grafico, maschera, elementi da TENERE e da TOGLIERE, note) da incollare in chat, così i valori scelti possono diventare quelli predefiniti del firmware.
- **Risposte del Pico:** mostra le conferme `OK ...` ricevute.

Le scelte fatte nella GUI vengono salvate **dal Pico** nella flash: non serve rimandarle. La GUI usa la porta seriale come `pc_stats.py`: all'apertura ferma lo script e lo rilancia alla chiusura; `upload.ps1` ferma e rilancia entrambi. Durante l'intro Skynet (circa 8 secondi) il Pico non legge la seriale: i comandi arrivano subito dopo. Richiede Python con Tkinter (incluso nell'installer di Python per Windows; il Python interno di PlatformIO non lo ha).

## Build `skynet`: visione del T-800 / terminale Skynet

Ricrea la visione rossa del Terminator: testo che compare battuto, codice assembly 6502 che scorre (lo stesso tipo di codice che si vede nel film), testo di analisi bianco rosato, mirino che aggancia il bersaglio e il riquadro `POSSIBLE RESPONSES`. Ogni elemento si accende e si spegne in tempo reale dalla [GUI di debug](#gui-di-debug).

### Intro (a ogni accensione)

1. Lampi rossi di accensione.
2. Righe di boot scritte lettera per lettera: `CYBERDYNE SYSTEMS CORPORATION`, `SKYNET NEURAL NET PROCESSOR`, `MODEL 101 SERIES 800`, `CPU: 6502 ... OK`, `> SKYNET BECOMES SELF-AWARE`, `> 29 AUG 1997 02:14 EDT`.
3. Il **logo Skynet** (piramidi a righe con la Y, scritta `SKYNET`, `NEURAL NET-BASED ARTIFICIAL INTELLIGENCE`, `CYBERDYNE SYSTEMS CORPORATION`) in dissolvenza rossa con effetto glitch a righe, poi fisso.

### HUD (`hud`, predefinita)

| Elemento | Contenuto |
|----------|-----------|
| Intestazione | `CYBERDYNE SYSTEMS` / `MODEL 101` |
| Colonna sinistra | senza dati: codice 6502 che scorre (`3A LDA #$00`, `JSR $FDED`, `STA ($08),Y`…); con le statistiche: CPU / RAM / DSK / GPU con barra, `TMP`, `NET`, `UP` (a caratteri grandi oppure piccoli) |
| Mirino | ciclo di circa 7 secondi: `SCANNING` (vaga) → `LOCKING` (si restringe e rallenta) → `TARGET ACQUIRED` (fermo e lampeggiante); si adatta allo spazio lasciato dalla colonna e diventa più nervoso con il carico del PC |
| Analisi | in alto a destra, gruppi di righe battute in bianco rosato. Senza dati: `ANALYSIS: / SCAN MODE 43984 / SIZE ASSESSMENT / ASSESSMENT COMPLETE`, `MATCH SEARCH: / PATTERN 0.86 ACCEPTED / FIT PROBABILITY 0.99`, `THREAT ASSESSMENT`, `VISUAL OVERRIDE`. Con i dati: analisi di CPU, temperatura, memoria e minaccia |
| Possible responses | ogni 20 secondi circa compare un riquadro con 5 risposte. La selezione scende fino a quella scelta, che lampeggia. Senza dati: `YES/NO`, `OR WHAT?`, `GO AWAY`, `PLEASE COME BACK LATER`, `I'LL BE BACK`; con i dati risposte da monitoraggio, in allarme `REDUCE LOAD`, `CLOSE PROCESSES`, `INCREASE COOLING`… |
| Pannello di stato | `STATUS`, `MATCH`, `RANGE` e `THREAT`; in fase `TARGET ACQUIRED` compare `TARGET: <nome>`. Con i dati: `LOAD`, `GPU TEMP`, `THREAT` (`LOW`, `MODERATE`, `HIGH`, `CRITICAL`, con la cornice che lampeggia) |
| Effetti | griglia a punti, barra di scansione, disturbo e righe di glitch, vignettatura rossa (centro più chiaro), scanline, scrittura progressiva |

### Comandi seriali

| Comando | Effetto |
|---------|---------|
| `C,<maschera>` | elementi visibili, bit per bit (vedi tabella sotto); il Pico risponde `OK ...` |
| `M,hud` / `M,term` / `M,graph` | schermata |
| `G,<metrica>` | grafico da mostrare (vedi [Grafici](#grafici-delle-statistiche)) |
| `M,boot` oppure `intro` | rivede l'intro |
| `N,<nome>` | nome del bersaglio (`TARGET: <nome>`) |
| `?` | il Pico risponde `ID,skynet` |
| `hud` | torna all'HUD |
| un numero (`23.5`) | riga `> DATA: 23.5` nel terminale e punto nel grafico |
| testo libero | riga `> TESTO` nel terminale, in maiuscolo |
| `clear` | cancella log e grafico |

| Bit | Elemento | Bit | Elemento |
|-----|----------|-----|----------|
| 0 | intestazione | 9 | testo di analisi |
| 1 | colonna sinistra (6502 / statistiche) | 10 | possible responses |
| 2 | riga CPU | 11 | griglia |
| 3 | riga RAM | 12 | barra di scansione |
| 4 | riga disco | 13 | disturbo / glitch |
| 5 | riga GPU | 14 | vignettatura rossa |
| 6 | info TMP / NET / UP | 15 | scanline |
| 7 | mirino | 16 | scrittura progressiva |
| 8 | pannello di stato | 17 | statistiche a caratteri grandi |

Esempio: `C,262143` accende tutto (predefinito).

## Build `cyberpunk`: netrunner di Cyberpunk 2077

Interfaccia ispirata a *Cyberpunk 2077*: giallo (RGB 252, 238, 10), ciano e rosso su fondo quasi nero, pannelli con l'angolo tagliato, titoli con separazione RGB e fasce di glitch. Ogni elemento si accende e si spegne in tempo reale dalla [GUI di debug](#gui-di-debug).

### Boot (a ogni accensione)

Avvio delle ottiche: `KIROSHI OPTICS FW 2.0.77`, `> NEURAL LINK ... OK`, `> CYBERDECK ... ONLINE`, `> ICEBREAKER ... LOADED`, `> DAEMONS ... 3 READY`, `> NETWATCH PROXY ... SPOOFED`, `> RAM UNITS ... 10`, `WELCOME BACK, <nome>`. Segue il **logo Arasaka** (stemma rotondo grande e scritta sotto, nel rosso Arasaka con una copia ciano spostata per la separazione RGB, più i glitch) con `NIGHT CITY // 2077`. Se il logo è spento compare solo il titolo `NIGHT CITY`.

### Schermate

| Schermata | Contenuto |
|-----------|-----------|
| **Monitor** (`hud`) | Barra gialla `NETRUNNER // SYS.MONITOR` con l'uptime. A sinistra CPU (barra gialla a 20 segmenti), RAM come le celle di RAM dei quickhack (`RAM 6/10`), GPU (barra rossa), disco e righe temperatura / rete / uptime; i valori critici lampeggiano. A destra l'elenco `QUICKHACKS` (`PING`, `REBOOT OPTICS`, `SHORT CIRCUIT`, `OVERHEAT`, `SYSTEM RESET`, con il costo in RAM) con la voce selezionata che cambia da sola (in allarme resta su `OVERHEAT`, in rosso), e uno scanner con staffe gialle, `SCANNING xx%` e poi `THREAT: LOW / MODERATE / HIGH / EXTREME`. Senza dati: `NO SIGNAL` |
| **Breach Protocol** (`breach`) | Il minigioco di hacking che si gioca da solo: matrice 5x5 di codici (`1C`, `55`, `BD`, `E9`, `7A`, `FF`), riga o colonna attiva evidenziata, cursore che scorre e seleziona alternando riga e colonna, buffer da 4, due sequenze da caricare (`DATAMINE_V1`, `ICEPICK`), tempo che scende. Alla fine `BREACH SUCCESSFUL` oppure, una partita su quattro circa, `BREACH PARTIAL` con un demone fallito; poi nuova partita |
| **Terminale** (`term`) | Log delle righe ricevute da seriale in ciano e grafico dei numeri in giallo |
| **Automatica** (`auto`, predefinita) | Monitor se arrivano le statistiche, altrimenti Breach Protocol |

In fondo a tutte le schermate scorre il notiziario `N54 NEWS` con sotto la riga di stato (`> SYS NOMINAL // LINK STABLE`, in allarme `!! CRITICAL: SYSTEM OVERHEAT !!`).

### Comandi seriali

| Comando | Effetto |
|---------|---------|
| `C,<maschera>` | elementi visibili, bit per bit (vedi tabella sotto); il Pico risponde `OK ...` |
| `M,auto` / `M,hud` / `M,breach` / `M,term` / `M,graph` | schermata |
| `G,<metrica>` | grafico da mostrare (vedi [Grafici](#grafici-delle-statistiche)) |
| `M,boot` oppure `intro` | rivede il boot |
| `N,<nome>` | nome del netrunner (`WELCOME BACK, <nome>`) |
| `?` | il Pico risponde `ID,cyberpunk` |
| `hud`, `breach`, `auto` | come `M,...` |
| un numero (`23.5`) | riga `> DATA: 23.5` nel terminale e punto nel grafico |
| testo libero | riga `> TESTO` nel terminale, in maiuscolo |
| `clear` | cancella log e grafico |

| Bit | Elemento | Bit | Elemento |
|-----|----------|-----|----------|
| 0 | barra gialla in alto | 8 | notiziario e riga di stato |
| 1 | barra CPU | 9 | cornici ad angolo tagliato |
| 2 | RAM a celle | 10 | glitch |
| 3 | barra GPU | 11 | separazione RGB |
| 4 | barra disco | 12 | scanline |
| 5 | info temperatura / rete / uptime | 13 | pixel di disturbo |
| 6 | quickhack | 14 | scrittura progressiva |
| 7 | scanner | 15 | logo Arasaka nel boot |

Esempio: `C,65535` accende tutto (predefinito).

## Prestazioni

- Per impostazione predefinita la libreria Adafruit usa SPI a **8 MHz** e un `fillScreen` completo costa circa 115 ms, per cui il display sembra lento. I firmware impostano `tft.setSPISpeed(40000000)` (40 MHz). Se vedi pixel sporchi o schermo instabile abbassa a 32 MHz; con cavi corti puoi provare fino a 62.5 MHz.
- Le animazioni sono disegnate **fuori schermo** su un `GFXcanvas16` 240x240 (circa 115 KB di RAM) e inviate al display in un unico blocco con `drawRGBBitmap`: niente sfarfallio e circa 30-40 fps.
- Uso di memoria: firmware `all` circa 180 KB di flash su 2 MB (8,5%) e circa 20 KB di RAM statica, più il canvas condiviso da 115 KB allocato all'avvio. Un solo tema: circa 96 KB (`cyberpunk`), 101 KB (`fallout`) o 120 KB (`skynet`).
- Le impostazioni salvate occupano un settore della flash (emulazione EEPROM, 256 byte usati). La scrittura blocca il Pico per qualche decina di millisecondi, per questo avviene solo 3 secondi dopo un cambiamento e non a ogni fotogramma.

## Loghi

Ogni build mostra nel boot il logo del proprio mondo, salvato come **maschera alpha a 8 bit** (0 = trasparente, 255 = pieno) e colorato dal firmware nel colore del tema:

| Build | Logo | File | Dimensione | Colore |
|-------|------|------|------------|--------|
| `fallout` | RobCo Industries (versione di *Fallout 76*) | [src/fallout/logo.h](src/fallout/logo.h) | 220 x 104 | verde fosforo |
| `skynet` | Skynet / Cyberdyne Systems | [src/skynet/logo.h](src/skynet/logo.h) | 236 x 165 | rosso, con glitch |
| `cyberpunk` | Arasaka: scritta e stemma separati | [src/cyberpunk/logo.h](src/cyberpunk/logo.h) | 232 x 25 e 96 x 95 | rosso Arasaka + copia ciano |

I loghi RobCo e Arasaka vengono dai wiki di Fallout e Cyberpunk 2077 (file `FO76 Robco Logo.png` e `Arasaka Logo CP2077.png`). Il logo Skynet viene da un'immagine fornita a mano.

### Rigenerare un logo

[tools/make_logo.py](tools/make_logo.py) converte qualsiasi immagine. Prende la trasparenza dal canale alpha, oppure dalla luminosità se l'immagine non è trasparente, toglie i margini e riduce alla dimensione massima:

```bash
python tools/make_logo.py robco.png   src/fallout/logo.h   ROBCO_LOGO     220 110
python tools/make_logo.py arasaka.png src/cyberpunk/logo.h ARASAKA_LOGO   232 40
python tools/make_logo.py arasaka.png src/cyberpunk/logo.h ARASAKA_EMBLEM 96 96 --crop 0,425,230,655 --append
python tools/make_skynet_logo.py <logo_skynet.png>
```

`--crop` ritaglia prima una parte dell'immagine (qui lo stemma Arasaka) e `--append` aggiunge la maschera allo stesso file `.h`. Le immagini sorgente non sono incluse nel repository. Per cambiare logo basta rigenerare il `.h` con lo stesso nome e ricompilare. Nella GUI la casella *Logo* lo mostra o lo nasconde (`fallout` e `cyberpunk`).

## Struttura del progetto

```
platformio.ini             env all (tutti i temi) e uno per ogni tema (-DONLY_<TEMA>)
upload.ps1                 upload automatico (UF2 su RPI-RP2); ferma e rilancia pc_stats.py e la GUI
src/main.cpp               firmware unico: seriale, comandi comuni, scelta del tema, pulsanti, flash, retroilluminazione
src/common/shared.h|cpp    display (tft) e canvas 240x240 condiviso
src/common/theme.h         interfaccia di un tema (begin, tick, handle, stato, schermata successiva, boot)
src/common/stats.h         dati dal PC (righe S, X, P), letti una volta per tutti i temi
src/common/history.h       storico delle statistiche e disegno dei grafici
src/fallout/theme.cpp      tema RobCo: boot, monitor, hacking, terminale, grafico
src/fallout/logo.h         logo RobCo Industries (generato)
src/skynet/theme.cpp       tema Skynet: intro, HUD, terminale, grafico
src/skynet/logo.h          logo Skynet (generato)
src/cyberpunk/theme.cpp    tema Cyberpunk: boot, monitor, Breach Protocol, terminale, grafico
src/cyberpunk/logo.h       logo e stemma Arasaka (generati)
tools/pc_stats.py          invia le statistiche del PC al Pico
tools/autostart.ps1        installa/rimuove l'avvio automatico di pc_stats.py con Windows
tools/display_gui.py       GUI di debug (elementi, schermate, statistiche)
tools/display_config.*     configurazione salvata dalla GUI (generata)
tools/make_logo.py         generatore generico di loghi (RobCo, Arasaka)
tools/make_skynet_logo.py  generatore del logo Skynet
tools/requirements.txt     dipendenze Python degli script in tools/
```

Ogni tema è un file `theme.cpp` con tutto il suo codice in un `namespace` anonimo (così i temi non si disturbano tra loro) e una costante `Theme` in fondo (`THEME_FALLOUT`, ...) che `src/main.cpp` usa per avviarlo, passargli i comandi, salvarne lo stato e disegnarlo. Tutti i temi disegnano sullo stesso canvas condiviso, quindi aggiungerne uno non costa RAM.

**Aggiungere un nuovo tema:** crea `src/<nome>/theme.cpp` copiando la struttura di uno esistente (funzioni `thBegin`, `thTick`, `thGetState`, `thSetState`, `thNext`, `thReboot` e la costante `Theme`), dichiara `extern const Theme THEME_<NOME>` in [src/common/theme.h](src/common/theme.h), aggiungilo all'elenco `THEMES` in `src/main.cpp`, nello `slotOf()` (massimo 3 temi nello stato salvato: aumenta `Saved::st`) e un `[env:<nome>]` in `platformio.ini`.

### Organizzazione di `src/main.cpp`

| Funzione | Ruolo |
|----------|-------|
| `setup()` / `loop()` | inizializza lo schermo, carica le impostazioni, attiva il tema; legge seriale e pulsanti, spegne la retroilluminazione, salva, chiama `tick()` del tema |
| `handleLine()` | comandi comuni (`?`, `D`, `T,`, `B,`, `G,`, `K,reset`, righe S/X/P); il resto va al tema attivo |
| `activate()` | cambia tema e risponde `ID,<tema>` |
| `snapshot()` / `checkDirty()` / `flushSave()` / `loadSettings()` | stato salvato nella flash (emulazione EEPROM) |

### Organizzazione di `src/fallout/theme.cpp`

| Funzione | Ruolo |
|----------|-------|
| `renderFrame()` | sceglie la schermata, prepara sfondo ed effetti, disegna e invia il fotogramma |
| `drawBoot()` / `blitMask()` | sequenza di boot di Fallout 4 e logo RobCo |
| `drawHome()` / `statRow()` | monitor di sistema e righe delle statistiche |
| `drawGraph()` | schermata Grafico (RobCo Telemetry) |
| `drawHack()` / `hackNew()` / `hackStep()` | minigioco di hacking: disegno, nuova partita, animazione del selettore |
| `drawTerm()` / `addLog()` | terminale seriale con log a scorrimento e grafico |
| `tprint()` / `tcenter()` / `tinverse()` | testo con scrittura progressiva, centrato, evidenziato in negativo |
| `drawBackground()` / `applyScanlines()` | bagliore, barra di scansione e scanline |
| `handleLine()` | interpreta i comandi ricevuti da seriale (`C,`, `M,`, `N,`, testo) |
| `parseStats()` / `pcLive()` / `pcAlarm()` | legge le righe `S,...`, indica se i dati sono recenti e se c'è sovraccarico |

> Le costanti degli elementi si chiamano `E_*` e non `F_*` perché `F_CPU` è già una macro del core Arduino (frequenza della CPU).

### Organizzazione di `src/skynet/theme.cpp`

| Funzione | Ruolo |
|----------|-------|
| `playIntro()` | lampi, righe di boot, logo Skynet con glitch |
| `drawLogo()` | disegna la maschera alpha del logo in rosso, con luminosità e glitch a righe |
| `renderFrame()` | prepara sfondo ed effetti, disegna HUD o terminale e invia il fotogramma |
| `drawHudFrame()` | un fotogramma dell'HUD: colonna sinistra, mirino, analisi, pannello di stato |
| `drawAsmColumn()` / `statsColumn()` | colonna sinistra: codice 6502 che scorre o statistiche del PC |
| `drawAnalysis()` / `drawResponses()` | testo di analisi e riquadro POSSIBLE RESPONSES |
| `drawReticle()` | mirino con cerchio, croce e staffe d'angolo |
| `drawGraph()` | schermata Grafico (TELEMETRY) |
| `drawTerm()` / `addLog()` | terminale seriale con log a scorrimento e grafico |
| `tprint()` | testo con scrittura progressiva |
| `drawBackground()` / `applyScanlines()` | vignettatura rossa, barra di scansione e scanline |
| `handleLine()` | interpreta i comandi del tema (`C,`, `M,`, `N,`, testo); i comandi comuni sono in `src/main.cpp` |
| `parseStats()` / `pcLive()` / `pcAlarm()` | come sopra |

### Organizzazione di `src/cyberpunk/theme.cpp`

| Funzione | Ruolo |
|----------|-------|
| `renderFrame()` | sceglie la schermata, disegna, applica disturbo, glitch e scanline e invia il fotogramma |
| `drawBoot()` / `blitMask()` / `chromaMask()` | avvio delle ottiche e logo Arasaka con separazione RGB |
| `drawHud()` | monitor: statistiche, quickhack, scanner |
| `drawGraph()` | schermata Grafico (NETRUNNER // TELEMETRY) |
| `drawBreach()` / `breachNew()` / `breachStep()` | Breach Protocol: disegno, nuova partita con percorso risolvibile, mosse del cursore |
| `drawTerm()` / `addLog()` | terminale seriale con log e grafico |
| `drawHeader()` / `drawFooter()` | barra gialla in alto, notiziario e riga di stato in basso |
| `cutFrame()` / `segBar()` / `chroma()` | cornice ad angolo tagliato, barra a segmenti, titolo con separazione RGB |
| `applyGlitch()` / `applyScanlines()` | fasce orizzontali spostate e scanline sul fotogramma |
| `handleLine()` | interpreta i comandi del tema (`C,`, `M,`, `N,`, testo); i comandi comuni sono in `src/main.cpp` |

## Risoluzione problemi

| Problema | Soluzione |
|----------|-----------|
| `pio` non riconosciuto | usa `%USERPROFILE%\.platformio\penv\Scripts\pio.exe` |
| Upload fallito / "RPI-RP2 non trovata" | tieni premuto BOOTSEL e ricollega il cavo; chiudi il Serial Monitor |
| La porta COM è cambiata | l'upload la trova da solo; per il monitor aggiorna `monitor_port` in `platformio.ini` |
| Le statistiche non compaiono | controlla che `pc_stats.py` sia in esecuzione (Gestione attività, `pythonw.exe`); l'upload lo rilancia da solo. Il display torna offline dopo 4 secondi senza dati |
| Lo schermo è spento | si è spento per inattività (`B,<secondi>`): manda un comando o premi un pulsante. `B,0` disattiva lo spegnimento |
| Il Pico parte con impostazioni strane | `K,reset` dalla seriale ripristina i valori di fabbrica |
| La temperatura CPU non compare | serve LibreHardwareMonitor con il Remote Web Server attivo (vedi [Note](#note)) |
| La GUI dice "Impossibile aprire COM..." | è aperto `pc_stats.py` o il Serial Monitor: chiudili e premi *Collega* |
| `No module named 'tkinter'` | avvia la GUI con il Python installato da python.org, non con quello di PlatformIO |
| Schermo nero | controlla BL (GP22) e che `SPI_MODE3` sia corretto per il tuo modulo |
| Pixel sporchi / immagine instabile | abbassa `setSPISpeed` a 32 MHz |
| Immagine spostata o tagliata | verifica `tft.init(240, 240, ...)` e la rotazione |
| I comandi non arrivano | scrivi nel Serial Monitor a 115200 baud, non nel terminale PowerShell |

## Note sui diritti

Le interfacce sono disegnate dal codice. I loghi (RobCo Industries, Skynet, Arasaka) derivano invece da immagini dei videogiochi e del film: *Fallout* © Bethesda Softworks, *Cyberpunk 2077* © CD PROJEKT, *Terminator* © dei rispettivi titolari. I testi sono solo riferimenti alle opere. Tutto è usato per un progetto personale e amatoriale, senza scopo commerciale.

I file generati con le immagini (`src/fallout/logo.h`, `src/skynet/logo.h`, `src/cyberpunk/logo.h`) sono **esclusi dal repository** tramite `.gitignore`, perché derivano da opere protette. Restano sul tuo PC; chi clona il repository deve generarli con gli script in `tools/` e le proprie immagini prima di compilare (vedi [Loghi](#loghi)). Per includerli comunque, togli quelle righe da `.gitignore`. Il codice del firmware è tuo e puoi licenziarlo come preferisci.
