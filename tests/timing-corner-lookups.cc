// Independently sampled linear functions expose voltage units and derivatives.
#include "cyclonev.h"

#include <cmath>
#include <cstdio>

using CV = mistral::CycloneV;

static bool close(double actual, double expected)
{
  return std::isfinite(actual) && std::abs(actual - expected) < 2e-4;
}

int main()
{
  for(double supply : {1.0, 1.006, 1.18}) {
    CV::dnode_table2 grid2{};
    CV::dnode_table3 grid3{};
    grid3.start = 0.85;
    for(int x = 0; x != 11; ++x)
      for(int y = 0; y != 11; ++y) {
        double vx = x*supply/10, vy = y*supply/10;
        grid2.value[x*11+y] = 7 + 2*vx + 3*vy;
        for(int z = 0; z != 11; ++z)
          grid3.value[x*121+y*11+z] = 7 + 2*vx + 3*(double(grid3.start)+vy) + 5*z*supply/10;
      }
    CV::t2_lookup two("linear2", &grid2, supply);
    CV::t3_lookup three("linear3", &grid3, supply);
    for(double fx : {-0.1, 0.0, 0.23, 0.7, 1.0, 1.15})
      for(double fy : {-0.1, 0.0, 0.37, 0.7, 1.0, 1.15}) {
        double x = fx*supply, y = fy*supply, value, dx, dy, dz;
        two.lookup(x, y, value, dx, dy);
        if(!close(value, 7+2*x+3*y) || !close(two.lookup(x,y), value) || !close(dx,2) || !close(dy,3))
          return std::fprintf(stderr, "2D lookup units/derivatives fail at supply %g\n", supply), 1;
        for(double fz : {0.0, 0.71, 1.0}) {
          double gate = double(grid3.start)+y, z = fz*supply;
          three.lookup(x, gate, z, value, dx, dy, dz);
          if(!close(value, 7+2*x+3*gate+5*z) || !close(three.lookup(x,gate,z),value)
              || !close(dx,2) || !close(dy,3) || !close(dz,5))
            return std::fprintf(stderr, "3D lookup units/origin/derivatives fail at supply %g\n", supply), 1;
        }
      }
  }
  std::puts("Supply-scaled interpolation, physical gate origins and derivatives pass.");
  return 0;
}
