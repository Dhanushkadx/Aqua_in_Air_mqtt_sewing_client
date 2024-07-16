#ifndef _THERMO_h
#define _THERMO_h

#include <max6675.h>
#include <Wire.h>
#include "pinsx.h"



extern MAX6675 thermocouple;

void max_6675_setup();
float get_temperatureC();

#endif