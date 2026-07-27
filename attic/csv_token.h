#ifndef CSV_TOKEN_H
#define CSV_TOKEN_H

#include <Arduino.h>

// Split a comma-separated token (e.g. "5,1,33,0") into field pointers.
// Copies `tok` into the caller's `work` buffer (worklen must be > strlen(tok)),
// replaces each comma with a NUL, and fills fields[] with pointers into `work`.
// Returns the number of fields found (capped at maxf). Caller converts each
// field with strtol (decimal) or strtoull(...,16) (hex mask) as appropriate.
//
// Used by the token-array config contract (valves/sensors/pumps/rules) so each
// row is ONE JSON value instead of a multi-key object — keeps the ArduinoJson
// parse tree tiny (1 node per row).
int csv_split(const char* tok, char* work, size_t worklen, const char** fields, int maxf);

#endif // CSV_TOKEN_H
