#include "csv_token.h"
#include <string.h>

int csv_split(const char* tok, char* work, size_t worklen, const char** fields, int maxf) {
    if (!tok || worklen == 0) return 0;
    strncpy(work, tok, worklen - 1);
    work[worklen - 1] = '\0';

    int   n    = 0;
    char* save = nullptr;
    for (char* p = strtok_r(work, ",", &save); p && n < maxf;
         p = strtok_r(nullptr, ",", &save)) {
        fields[n++] = p;
    }
    return n;
}
