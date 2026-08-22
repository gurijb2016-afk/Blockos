#include "../include/math.h"

namespace
{

constexpr double PI =
    3.1415926535897932384626433832795;

constexpr double TWO_PI =
    6.283185307179586476925286766559;

double wrap_angle(double x)
{
    while (x > PI)
        x -= TWO_PI;

    while (x < -PI)
        x += TWO_PI;

    return x;
}

}


extern "C"
{

double fabs(double x)
{
    return x < 0.0 ? -x : x;
}


double sqrt(double x)
{
    if (x < 0.0)
        return 0.0;

    if (x == 0.0)
        return 0.0;

    double guess =
        x > 1.0 ? x : 1.0;

    for (int i = 0; i < 32; ++i)
        guess =
            0.5 * (guess + x / guess);

    return guess;
}


double sin(double x)
{
    x = wrap_angle(x);

    const double x2 = x * x;

    return x *
        (1.0
         - x2 / 6.0
         + (x2 * x2) / 120.0
         - (x2 * x2 * x2) / 5040.0
         + (x2 * x2 * x2 * x2) / 362880.0);
}


double cos(double x)
{
    x = wrap_angle(x);

    const double x2 = x * x;

    return 1.0
         - x2 / 2.0
         + (x2 * x2) / 24.0
         - (x2 * x2 * x2) / 720.0
         + (x2 * x2 * x2 * x2) / 40320.0;
}

}
