#include "locale.h"
#include "langinfo.h"
static struct lconv g = { ".", "", "", "", "", ".", "", "", "+", "-", 127,127,127,127,127,127,127,127 };
static const char codeset[] = "UTF-8";
char *setlocale(int category, const char *locale) { (void)category; if (locale && locale[0] && locale[0] != 'C' && locale[0] != '.') return 0; return (char*)"C.UTF-8"; }
struct lconv *localeconv(void) { return &g; }
const char *nl_langinfo(nl_item item) { return item == CODESET ? codeset : ""; }
