"""Definizioni dei temi per la GUI di configurazione: elementi (bit corrispondenti a E_* nel
theme.cpp di ogni tema), schermate e grafici. Va tenuto allineato con il firmware."""

# Per ogni build: elementi (bit, chiave, etichetta, gruppo) - i bit corrispondono a E_* nel
# main.cpp della build - e schermate (comando M,<chiave>)
BUILDS = {
    "fallout": {
        "title": "RobCo Industries (fallout)",
        "name_label": "Nome amministratore:",
        "screens": [
            ("auto", "Automatica (monitor se arrivano dati, altrimenti hacking)"),
            ("home", "Monitor di sistema"),
            ("hack", "Minigioco hacking password"),
            ("term", "Terminale seriale"),
            ("graph", "Grafico statistiche"),
        ],
        "elements": [
            (16, "logo",      "Logo RobCo Industries nel boot",                  "Contenuti"),
            (0,  "header",    "Intestazione RobCo",                              "Contenuti"),
            (1,  "title",     "Titolo sezione e linea ====",                     "Contenuti"),
            (7,  "menu",      "Menu con voce evidenziata",                       "Contenuti"),
            (8,  "log",       "Righe di LOG / stato",                            "Contenuti"),
            (9,  "cursor",    "Prompt con cursore lampeggiante",                 "Contenuti"),
            (2,  "cpu",       "Riga CPU",                                        "Statistiche"),
            (3,  "ram",       "Riga RAM",                                        "Statistiche"),
            (4,  "dsk",       "Riga disco",                                      "Statistiche"),
            (5,  "gpu",       "Riga GPU",                                        "Statistiche"),
            (6,  "info",      "Info: temp GPU, rete, uptime (offline: User Log)", "Statistiche"),
            (10, "bigstats",  "Statistiche a caratteri grandi (no = barre ASCII)", "Statistiche"),
            (11, "scanlines", "Scanline (righe scure alternate)",                "Effetti CRT"),
            (12, "scanbar",   "Barra di scansione che scende",                   "Effetti CRT"),
            (13, "glow",      "Bagliore centrale / bordi scuri",                 "Effetti CRT"),
            (14, "flicker",   "Sfarfallio del fosforo",                          "Effetti CRT"),
            (15, "typeon",    "Scrittura progressiva del testo",                 "Effetti CRT"),
        ],
    },
    "skynet": {
        "title": "Skynet / T-800 (skynet)",
        "name_label": "Nome bersaglio:",
        "screens": [
            ("hud", "HUD (visione del T-800)"),
            ("term", "Terminale seriale"),
            ("graph", "Grafico statistiche"),
        ],
        "elements": [
            (0,  "header",    "Intestazione CYBERDYNE / MODEL 101",              "Contenuti"),
            (1,  "datacol",   "Colonna sinistra (codice 6502 / statistiche)",    "Contenuti"),
            (7,  "reticle",   "Mirino",                                          "Contenuti"),
            (9,  "analysis",  "Testo di analisi in alto a destra",               "Contenuti"),
            (10, "responses", "Riquadro POSSIBLE RESPONSES",                     "Contenuti"),
            (8,  "status",    "Pannello di stato in basso",                      "Contenuti"),
            (2,  "cpu",       "Riga CPU",                                        "Statistiche"),
            (3,  "ram",       "Riga RAM",                                        "Statistiche"),
            (4,  "dsk",       "Riga disco",                                      "Statistiche"),
            (5,  "gpu",       "Riga GPU",                                        "Statistiche"),
            (6,  "info",      "Info: TMP / NET / UP",                            "Statistiche"),
            (17, "bigstats",  "Statistiche a caratteri grandi",                  "Statistiche"),
            (11, "grid",      "Griglia a punti",                                 "Effetti"),
            (12, "scanbar",   "Barra di scansione",                              "Effetti"),
            (13, "noise",     "Disturbo e righe di glitch",                      "Effetti"),
            (14, "vignette",  "Visione rossa: centro chiaro, bordi scuri",       "Effetti"),
            (15, "scanlines", "Scanline",                                        "Effetti"),
            (16, "typeon",    "Scrittura progressiva del testo",                 "Effetti"),
        ],
    },
    "cyberpunk": {
        "title": "Cyberpunk 2077 (cyberpunk)",
        "name_label": "Nome netrunner:",
        "screens": [
            ("auto", "Automatica (monitor se arrivano dati, altrimenti Breach Protocol)"),
            ("hud", "Monitor di sistema (netrunner)"),
            ("breach", "Minigioco Breach Protocol"),
            ("term", "Terminale seriale"),
            ("graph", "Grafico statistiche"),
        ],
        "elements": [
            (15, "logo",       "Logo Arasaka nel boot",                          "Contenuti"),
            (0,  "header",     "Barra gialla in alto",                           "Contenuti"),
            (6,  "quickhacks", "Elenco QUICKHACKS",                              "Contenuti"),
            (7,  "scanner",    "Riquadro di scansione",                          "Contenuti"),
            (8,  "ticker",     "Notiziario N54 e riga di stato",                 "Contenuti"),
            (9,  "frames",     "Cornici ad angolo tagliato",                     "Contenuti"),
            (1,  "cpu",        "Barra CPU",                                      "Statistiche"),
            (2,  "ram",        "RAM a celle",                                    "Statistiche"),
            (3,  "gpu",        "Barra GPU",                                      "Statistiche"),
            (4,  "dsk",        "Barra disco",                                    "Statistiche"),
            (5,  "info",       "Info: temperatura, rete, uptime",                "Statistiche"),
            (10, "glitch",     "Glitch (fasce spostate)",                        "Effetti"),
            (11, "chroma",     "Separazione RGB sui titoli",                     "Effetti"),
            (12, "scanlines",  "Scanline",                                       "Effetti"),
            (13, "noise",      "Pixel di disturbo",                              "Effetti"),
            (14, "typeon",     "Scrittura progressiva del testo",                "Effetti"),
        ],
    },
    "alien": {
        "title": "Alien MU-TH-UR 6000 (alien)",
        "name_label": "Nome ufficiale:",
        "screens": [
            ("auto", "Automatica (nave se arrivano dati, altrimenti tracker)"),
            ("system", "Stato della nave"),
            ("tracker", "Motion tracker"),
            ("term", "Terminale seriale"),
            ("graph", "Grafico statistiche"),
        ],
        "elements": [
            (14, "logo",      "Logo Weyland-Yutani nel boot",                    "Contenuti"),
            (0,  "header",    "Intestazione Weyland-Yutani",                     "Contenuti"),
            (1,  "title",     "Titolo sezione e linea ====",                     "Contenuti"),
            (7,  "log",       "Conversazione / righe di stato",                  "Contenuti"),
            (8,  "cursor",    "Prompt con cursore lampeggiante",                 "Contenuti"),
            (15, "blips",     "Contatti sul motion tracker",                     "Contenuti"),
            (2,  "cpu",       "REACTOR (CPU)",                                   "Statistiche"),
            (3,  "ram",       "LIFE SUPPORT (RAM)",                              "Statistiche"),
            (4,  "dsk",       "CARGO HOLD (disco)",                              "Statistiche"),
            (5,  "gpu",       "MAIN DRIVE (GPU)",                                "Statistiche"),
            (6,  "info",      "Temperature, comunicazioni, ibernazione",         "Statistiche"),
            (9,  "scanlines", "Scanline (righe scure alternate)",                "Effetti CRT"),
            (10, "scanbar",   "Barra di scansione che scende",                   "Effetti CRT"),
            (11, "glow",      "Bagliore centrale / bordi scuri",                 "Effetti CRT"),
            (12, "flicker",   "Sfarfallio del fosforo",                          "Effetti CRT"),
            (13, "typeon",    "Scrittura progressiva del testo",                 "Effetti CRT"),
        ],
    },
}

# Grafico della schermata "graph" (comando G,<chiave>)
GRAPHS = [
    ("auto", "Automatico (a rotazione ogni 8 s)"),
    ("cpu", "CPU"),
    ("ram", "RAM"),
    ("dsk", "Disco"),
    ("gpu", "GPU"),
    ("tmp", "Temperatura GPU"),
    ("net", "Rete"),
    ("png", "Latenza (ping)"),
]


# Descrizione breve di ogni tema, mostrata nella scheda "Tema"
THEME_INFO = {
    "fallout": ("RobCo Industries", "Terminale di Fallout: boot di Fallout 4, monitor di sistema, minigioco di hacking. Verde."),
    "skynet": ("Skynet / T-800", "Visione del Terminator: HUD con mirino, codice 6502, analisi e possible responses. Rosso."),
    "cyberpunk": ("Cyberpunk 2077", "Netrunner: boot delle ottiche, quickhack, scanner, minigioco Breach Protocol. Giallo e ciano."),
    "alien": ("Alien - MU-TH-UR 6000", "Computer della Nostromo: stato della nave, motion tracker, conversazione. Ambra."),
}

METRICS = ["cpu", "ram", "dsk", "gpu", "tmp", "net", "png"]  # stesso ordine di enum Metric in common/history.h
