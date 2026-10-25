#ifndef BLOCKOS_MATH_H
#define BLOCKOS_MATH_H
#define M_PI 3.14159265358979323846
#define M_E  2.7182818284590452354
#define HUGE_VAL (__builtin_huge_val())
#define INFINITY (__builtin_inff())
#define NAN (__builtin_nanf(""))
double fabs(double x);
double floor(double x);
double ceil(double x);
double sqrt(double x);
double exp(double x);
double sin(double x);
double cos(double x);
double atan2(double y, double x);
double pow(double x, double y);
#endif
double trunc(double x);
double hypot(double x, double y);
double asin(double x);
#define isfinite(x) ((x) - (x) == 0)
#define isnan(x) ((x) != (x))
double acos(double x);
double cbrt(double x);
float sqrtf(float x);
