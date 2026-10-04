// Display e canvas condivisi da tutti i temi. Il firmware unico (src/main.cpp) inizializza lo
// schermo una sola volta; ogni tema disegna sul canvas condiviso, cosi' la RAM (115 KB) si paga una volta sola.
#pragma once
#include <Arduino.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>
#include <SPI.h>

// Numeri GPIO (non i pin fisici). SPI di default: SCK=GP18, MOSI=GP19
#define TFT_DC   16  // pin fisico 21
#define TFT_CS   17  // pin fisico 22
#define TFT_RST  21  // pin fisico 27
#define TFT_BL   22  // pin fisico 29

extern Adafruit_ST7789 tft;
GFXcanvas16 &sharedCanvas();  // canvas 240x240 comune a tutti i temi
