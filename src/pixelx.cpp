#include "pixelx.h"



Adafruit_NeoPixel pixels(NUM_LEDS, PIXEL_PIN, NEO_GRB + NEO_KHZ800);



void initPixelBright(){
     pixels.begin();
  pixels.setBrightness(100);
}

void rainbow(uint8_t wait) {
  uint16_t i, j;

  for (j = 0; j < 256; j++) {
    for (i = 0; i < pixels.numPixels(); i++) {
      pixels.setPixelColor(i, Wheel((i + j) & 255));
    }
    pixels.show();
    delay(wait);
  }
}

uint32_t Wheel(byte WheelPos) {
  if (WheelPos < 85) {
    return pixels.Color(WheelPos * 3, 255 - WheelPos * 3, 0);
  } else if (WheelPos < 170) {
    WheelPos -= 85;
    return pixels.Color(255 - WheelPos * 3, 0, WheelPos * 3);
  } else {
    WheelPos -= 170;
    return pixels.Color(0, WheelPos * 3, 255 - WheelPos * 3);
  }
}

void pixel_noWifi(){
    pixels.setPixelColor(0 , 255,0,0);
    pixels.show();
    delay(250);
    pixels.clear();
    pixels.show();
    delay(250);
}

void pixel_server_unreacheble(){
    pixels.setPixelColor(0 ,0,255,0);
    pixels.show();
    delay(250);
    pixels.clear();
    pixels.show();
    delay(250);
}

void pixel_configEn(){
    pixels.setPixelColor(0 ,255,0,255);
    pixels.show();
    delay(100);
    pixels.clear();
    pixels.show();
    delay(100);
}