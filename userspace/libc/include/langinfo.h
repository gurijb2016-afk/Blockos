#ifdef __cplusplus
extern "C" {
#endif
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

#ifdef __cplusplus
}
#endif

#ifndef BOS_NL_ITEMS
#define BOS_NL_ITEMS
#define ABDAY_1 100
#define ABDAY_2 101
#define ABDAY_3 102
#define ABDAY_4 103
#define ABDAY_5 104
#define ABDAY_6 105
#define ABDAY_7 106
#define DAY_1 107
#define DAY_2 108
#define DAY_3 109
#define DAY_4 110
#define DAY_5 111
#define DAY_6 112
#define DAY_7 113
#define ABMON_1 114
#define ABMON_2 115
#define ABMON_3 116
#define ABMON_4 117
#define ABMON_5 118
#define ABMON_6 119
#define ABMON_7 120
#define ABMON_8 121
#define ABMON_9 122
#define ABMON_10 123
#define ABMON_11 124
#define ABMON_12 125
#define MON_1 126
#define MON_2 127
#define MON_3 128
#define MON_4 129
#define MON_5 130
#define MON_6 131
#define MON_7 132
#define MON_8 133
#define MON_9 134
#define MON_10 135
#define MON_11 136
#define MON_12 137
#define ALTMON_1 138
#define ALTMON_2 139
#define ALTMON_3 140
#define ALTMON_4 141
#define ALTMON_5 142
#define ALTMON_6 143
#define ALTMON_7 144
#define ALTMON_8 145
#define ALTMON_9 146
#define ALTMON_10 147
#define ALTMON_11 148
#define ALTMON_12 149
#define AM_STR 150
#define PM_STR 151
#define D_T_FMT 152
#define D_FMT 153
#define T_FMT 154
#define T_FMT_AMPM 155
#define ERA 156
#define CODESET 157
#define RADIXCHAR 158
#define THOUSEP 159
#define YESEXPR 160
#define NOEXPR 161
#endif
