#include "../turntable_curve.h"
#include <cassert>
#include <cstdio>
int main() {
  TurntableCurveFilter curve;
  curve.reset(0);
  assert(curve.update(4, 10, TurntableCurve::Linear) == 4);
  curve.reset(0);
  assert(curve.update(1, 10, TurntableCurve::Precision) == 0);
  assert(curve.update(2, 10, TurntableCurve::Precision) == 1);
  curve.reset(250);
  assert(curve.update(252, 2, TurntableCurve::Dynamic) == 254);
  assert(curve.update(1, 2, TurntableCurve::Dynamic) == 8);
  curve.reset(5);
  assert(curve.update(4, 2, TurntableCurve::Dynamic) == 3);
  puts("Turntable curve tests passed");
}
