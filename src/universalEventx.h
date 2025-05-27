
#ifndef _UNI_EVENTX_H
#define _UNI_EVENTX_H

#include <ArduinoJson.h>
#include "statments.h"
#include "Arduino.h"
#include "typex.h"


byte universal_event_hadler(const char* smsbuffer);

extern uint32_t prev_prdcount;
#endif