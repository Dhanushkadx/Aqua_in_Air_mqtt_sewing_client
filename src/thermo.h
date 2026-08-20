#ifndef _THERMO_h
#define _THERMO_h

#include "pinsx.h"

// A board has a thermocouple only if its pin map defines the MAX6675 pins. Boards
// without one (e.g. IOT_OLD_PCB) leave them undefined; thermo.cpp then stubs the
// API so the firmware still links. get_temperatureC() is only actually called
// under THERMO_OK (off by default), so the stub returning 0 is harmless.
#if defined(thermoCLK) && defined(thermoCS) && defined(thermoDO)
  #define HAS_THERMO
#endif

#ifdef HAS_THERMO
#include <max6675.h>
#include <Wire.h>
extern MAX6675 thermocouple;
#endif

void  max_6675_setup();
float get_temperatureC();

#endif
