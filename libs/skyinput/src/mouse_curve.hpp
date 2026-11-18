#pragma once

namespace skyinput {

double curveRandom(double min, double max);
int curveSign();
int curveTremor(double probability);
int curvePoint(double t, double start, double controlA, double controlB, double end);
int curveControlPoint(double start, double end, double deviation);
int firstCurveControlPoint(double start, double end, double deviation, int sign);

}  // namespace skyinput
