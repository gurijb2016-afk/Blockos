#pragma once
typedef int nl_item;
#define CODESET 14
#define D_T_FMT 2
#define D_FMT 1
#define T_FMT 3
#define T_FMT_AMPM 5
#define AM_STR 4
#define PM_STR 5
const char* nl_langinfo(nl_item);
