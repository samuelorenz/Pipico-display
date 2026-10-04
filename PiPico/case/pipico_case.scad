// Custodia parametrica per Raspberry Pi Pico + display ST7789 240x240 (1.54").
// Apri il file con OpenSCAD (https://openscad.org), scegli la parte con "part", premi F6 e poi
// "Export as STL". NON e' stata renderizzata ne' stampata: misura il tuo modulo con un calibro e
// correggi i valori qui sotto prima di stampare (i moduli ST7789 variano di qualche millimetro).
//
// Layout: il display sta sul fronte (verticale), il Pico e' dietro, con la porta USB verso il basso.
// Dietro ci sono due fori per i pulsanti (tema / schermata, GP14 e GP15) e un foro per il cicalino (GP13).

part = "both";        // "front" | "back" | "both"

// ---- Display ----
disp_w = 33.0;        // larghezza del circuito stampato del display
disp_h = 35.0;        // altezza del circuito stampato
disp_t = 4.0;         // spessore totale (PCB + vetro)
view_w = 27.6;        // area visibile
view_h = 27.6;
view_top = 3.0;       // distanza dal bordo alto del PCB all'area visibile
pins_h = 6.0;         // spazio sotto il display per il connettore a pin

// ---- Raspberry Pi Pico ----
pico_l = 51.0;        // lunghezza
pico_w = 21.0;        // larghezza
pico_t = 3.6;         // spessore con i componenti (connettore USB incluso)
hole_dx = 47.0;       // distanza tra i fori di fissaggio (lungo)
hole_dy = 11.4;       // distanza tra i fori di fissaggio (corto)
usb_w = 9.0;          // foro per il cavo micro-USB
usb_h = 4.0;

// ---- Custodia ----
wall = 2.4;           // spessore delle pareti
tol = 0.4;            // gioco attorno ai componenti
screw_d = 2.8;        // foro per viti autofilettanti M3 (corpo 2.8 mm)
btn_d = 6.2;          // foro dei pulsanti (pulsanti da 6 mm)
buzz_d = 2.0;         // foro del cicalino

inner_w = max(disp_w, pico_w) + 2 * tol;
inner_h = pico_l + 2 * tol;
outer_w = inner_w + 2 * wall;
outer_h = inner_h + 2 * wall;
front_d = wall + disp_t + 1.0;     // parte frontale: pannello + tasca del display
back_d = pico_t + 6.0;             // parte posteriore: Pico + spazio per i fili

module rbox(w, h, d, r = 2.5) {    // parallelepipedo con spigoli verticali arrotondati
  hull() for (x = [r, w - r], y = [r, h - r]) translate([x, y, 0]) cylinder(r = r, h = d, $fn = 32);
}

module screw_posts() {             // quattro colonnine agli angoli per le viti
  for (x = [wall + 2.2, outer_w - wall - 2.2], y = [wall + 2.2, outer_h - wall - 2.2])
    translate([x, y, 0]) difference() {
      cylinder(d = 6, h = front_d, $fn = 24);
      translate([0, 0, 1]) cylinder(d = screw_d, h = front_d, $fn = 24);
    }
}

module front() {
  difference() {
    union() {
      difference() {
        rbox(outer_w, outer_h, front_d);
        translate([wall, wall, wall]) rbox(inner_w, inner_h, front_d, 1.5);   // vano interno
      }
      screw_posts();
    }
    // finestra sul display (area visibile), centrata in larghezza e vicina al bordo alto
    translate([(outer_w - view_w) / 2, outer_h - wall - tol - view_top - view_h, -1])
      cube([view_w, view_h, wall + 2]);
  }
  // cornice che trattiene il circuito stampato del display
  difference() {
    translate([(outer_w - disp_w - 2 * tol) / 2 - 1.2, outer_h - wall - disp_h - 2 * tol - 1.2, wall])
      cube([disp_w + 2 * tol + 2.4, disp_h + 2 * tol + 2.4, disp_t]);
    translate([(outer_w - disp_w - 2 * tol) / 2, outer_h - wall - disp_h - 2 * tol, wall - 0.1])
      cube([disp_w + 2 * tol, disp_h + 2 * tol, disp_t + 1]);
  }
}

module back() {
  difference() {
    union() {
      difference() {
        rbox(outer_w, outer_h, back_d);
        translate([wall, wall, wall]) rbox(inner_w, inner_h, back_d, 1.5);
      }
      // colonnine su cui appoggia il Pico (fori di fissaggio M2)
      for (x = [0, hole_dy], y = [0, hole_dx])
        translate([(outer_w - hole_dy) / 2 + x, wall + tol + (pico_l - hole_dx) / 2 + y, wall])
          difference() { cylinder(d = 4.5, h = 2.5, $fn = 24); cylinder(d = 2.0, h = 3, $fn = 24); }
    }
    // foro USB sulla parete bassa
    translate([(outer_w - usb_w) / 2, -1, wall + 2.5 + 0.2]) cube([usb_w, wall + 2, usb_h]);
    // due fori per i pulsanti sul lato destro
    for (y = [outer_h * 0.35, outer_h * 0.55])
      translate([outer_w - wall - 1, y, back_d / 2]) rotate([0, 90, 0]) cylinder(d = btn_d, h = wall + 2, $fn = 24);
    // foro per il cicalino sul fondo
    translate([outer_w / 2, outer_h * 0.7, -1]) cylinder(d = buzz_d, h = wall + 2, $fn = 24);
    // fori per le viti nel retro
    for (x = [wall + 2.2, outer_w - wall - 2.2], y = [wall + 2.2, outer_h - wall - 2.2])
      translate([x, y, -1]) cylinder(d = 3.2, h = wall + 2, $fn = 24);
  }
}

if (part == "front" || part == "both") front();
if (part == "back" || part == "both") translate([outer_w + 8, 0, 0]) back();
