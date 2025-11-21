
#ifndef PIXELX_H_
#define PIXEL_H_
#include "config.h"
#include "pinsx.h"
#include "Arduino.h"
#include "statments.h"
#include <Adafruit_NeoPixel.h>
#include "config.h"


void rainbow(uint8_t wait);
uint32_t Wheel(byte WheelPos);
void initPixelBright();
void pixel_noWifi();
void pixel_server_unreacheble();
void pixel_configEn();
void pixel_no_ip();

#endif