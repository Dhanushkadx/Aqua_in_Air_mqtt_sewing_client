#include "thermo.h"

#ifdef HAS_THERMO

MAX6675 thermocouple(thermoCLK, thermoCS, thermoDO);

void max_6675_setup() {
  // wait for MAX chip to stabilize
  delay(500);
}

float get_temperatureC(){
    float tempC = thermocouple.readCelsius();
    Serial.println(tempC);
    return tempC;
}

#else  // board has no thermocouple wired (thermo pins undefined)

void  max_6675_setup() {}
float get_temperatureC() { return 0.0f; }

#endif // HAS_THERMO
