#include "thermo.h"


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