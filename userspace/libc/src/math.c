/* Minimal freestanding libm for BlockOS userland.
 * Accuracy is sufficient for filter-table generation and gradients;
 * not a correctly-rounded libm. */
#include "math.h"
#include <stdint.h>

double fabs(double x) { return x < 0 ? -x : x; }

double floor(double x) {
    if (x != x || x > 4503599627370496.0 || x < -4503599627370496.0) return x;
    int64_t i = (int64_t)x;
    double r = (double)i;
    if (r > x) r -= 1.0;
    return r;
}

double ceil(double x) {
    if (x != x || x > 4503599627370496.0 || x < -4503599627370496.0) return x;
    int64_t i = (int64_t)x;
    double r = (double)i;
    if (r < x) r += 1.0;
    return r;
}

double sqrt(double x) {
    if (x <= 0) return 0;
    double g = x > 1 ? x : 1;
    for (int i = 0; i < 60; i++) g = 0.5 * (g + x / g);
    return g;
}

double exp(double x) {
    /* range reduction: exp(x) = 2^k * exp(r) */
    int k = 0;
    while (x > 0.5)  { x -= 0.6931471805599453; k++; }
    while (x < -0.5) { x += 0.6931471805599453; k--; }
    double term = 1, sum = 1;
    for (int n = 1; n < 30; n++) { term *= x / n; sum += term; }
    while (k > 0) { sum *= 2; k--; }
    while (k < 0) { sum *= 0.5; k++; }
    return sum;
}

double sin(double x) {
    const double twopi = 6.283185307179586;
    while (x > M_PI)  x -= twopi;
    while (x < -M_PI) x += twopi;
    double term = x, sum = x, x2 = x * x;
    for (int n = 1; n < 20; n++) {
        term *= -x2 / ((2 * n) * (2 * n + 1));
        sum += term;
    }
    return sum;
}

double cos(double x) { return sin(x + M_PI / 2); }

double atan2(double y, double x) {
    /* atan via series on reduced argument */
    double ax = fabs(x), ay = fabs(y), a;
    if (ax == 0 && ay == 0) return 0;
    int inv = ay > ax;
    double t = inv ? ax / ay : ay / ax;
    double t2 = t * t, term = t, sum = t;
    for (int n = 1; n < 40; n++) {
        term *= -t2;
        sum += term / (2 * n + 1);
    }
    a = inv ? M_PI / 2 - sum : sum;
    if (x < 0) a = M_PI - a;
    if (y < 0) a = -a;
    return a;
}

static double log_(double x) {
    if (x <= 0) return -1e308;
    int k = 0;
    while (x > 2.0) { x *= 0.5; k++; }
    while (x < 1.0) { x *= 2.0; k--; }
    double z = (x - 1) / (x + 1), z2 = z * z, term = z, sum = 0;
    for (int n = 1; n < 80; n += 2) { sum += term / n; term *= z2; }
    return 2 * sum + k * 0.6931471805599453;
}

double pow(double x, double y) {
    if (y == 0) return 1;
    if (x == 0) return 0;
    if (y == (int64_t)y && y > 0 && y < 64) {
        double r = 1;
        for (int64_t i = 0; i < (int64_t)y; i++) r *= x;
        return r;
    }
    if (x < 0) return 0;
    return exp(y * log_(x));
}

double trunc(double x) { return x < 0 ? ceil(x) : floor(x); }
double hypot(double x, double y) { return sqrt(x * x + y * y); }
double asin(double x) {
    if (x >= 1) return M_PI / 2;
    if (x <= -1) return -M_PI / 2;
    return atan2(x, sqrt(1 - x * x));
}
