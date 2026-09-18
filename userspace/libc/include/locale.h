#pragma once
struct lconv { char *decimal_point; char *thousands_sep; char *grouping; char *int_curr_symbol; char *currency_symbol; char *mon_decimal_point; char *mon_thousands_sep; char *mon_grouping; char *positive_sign; char *negative_sign; char int_frac_digits, frac_digits; char p_cs_precedes,n_cs_precedes,p_sep_by_space,n_sep_by_space; char p_sign_posn,n_sign_posn; };
#define LC_ALL 6
#define LC_COLLATE 3
#define LC_CTYPE 0
#define LC_MONETARY 4
#define LC_NUMERIC 1
#define LC_TIME 2
char *setlocale(int, const char*);
struct lconv *localeconv(void);
