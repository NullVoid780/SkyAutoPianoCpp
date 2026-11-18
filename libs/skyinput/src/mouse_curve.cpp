#include "mouse_curve.hpp"

#include <cmath>
#include <random>

namespace skyinput {

namespace {

double unitRandom()
{
    thread_local std::mt19937_64 engine{std::random_device{}()};
    return std::uniform_real_distribution<double>(0.0, 1.0)(engine);
}

double jsRound(double value)
{
    return std::floor(value + 0.5);
}

}  // namespace

double curveRandom(double min, double max)
{
    return std::floor(unitRandom() * (max + 1 - min)) + min;
}

int curveSign()
{
    return unitRandom() > 0.5 ? 1 : -1;
}

int curveTremor(double probability)
{
    return unitRandom() <= probability ? curveSign() : 0;
}

int curvePoint(double t, double start, double controlA, double controlB, double end)
{
    const double invert = 1 - t;
    const double invert2 = invert * invert;
    const double t2 = t * t;
    return static_cast<int>(std::floor(invert2 * invert * start + 3 * invert2 * t * controlA +
                                       3 * invert * t2 * controlB + t2 * t * end));
}

int curveControlPoint(double start, double end, double deviation)
{
    return static_cast<int>(jsRound(start + (end - start) * (0.5 + curveSign() * deviation)));
}

int firstCurveControlPoint(double start, double end, double deviation, int sign)
{
    return static_cast<int>(jsRound(start + sign * (end - start) * deviation));
}

}  // namespace skyinput
