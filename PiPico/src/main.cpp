#include <Arduino.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>
#include <SPI.h>

// Numeri GPIO (non i pin fisici). SPI di default: SCK=GP18, MOSI=GP19
#define TFT_DC   16  // pin fisico 21
#define TFT_CS   17  // pin fisico 22
#define TFT_RST  21  // pin fisico 27
#define TFT_BL   22  // pin fisico 29

// SPI software: usa direttamente SDA=GP19, SCL=GP18
Adafruit_ST7789 tft(TFT_CS, TFT_DC, 19, 18, TFT_RST);

void setup() {
  Serial.begin(115200);
  pinMode(TFT_BL, OUTPUT);
  digitalWrite(TFT_BL, HIGH);

  tft.init(240, 240, SPI_MODE3);  // se il tuo schermo e' 240x320 cambia in init(240, 320)
  tft.setRotation(0);

  tft.fillScreen(ST77XX_RED);   delay(500);
  tft.fillScreen(ST77XX_GREEN); delay(500);
  tft.fillScreen(ST77XX_BLUE);  delay(500);
  tft.fillScreen(ST77XX_BLACK);

  tft.setTextColor(ST77XX_WHITE);
  tft.setTextSize(3);
  tft.setCursor(20, 20);
  tft.println("Ciao Samu!");
  tft.setTextSize(2);
  tft.setCursor(20, 70);
  tft.println("Upload auto OK!");
}

int n = 0;

void loop() {
  tft.fillRect(20, 120, 200, 40, ST77XX_BLACK);
  tft.setTextColor(ST77XX_YELLOW);
  tft.setTextSize(3);
  tft.setCursor(20, 120);
  tft.print(n++);
  delay(500);
}
