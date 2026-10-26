// A powered buffer can sharpen a slow edge and cross half-supply before its
// input. Exercise a genuine signed propagation measurement, not a fake interval.
#include "mistral-analogsim.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <memory>

struct Sharpen : mistral::AnalogSim::table2_lookup {
  Sharpen() : table2_lookup("sharpen") {}
  void lookup(double x,double y,double &v,double &dx,double &dy) const override {
    double target = std::max(0.0,std::min(1.0,3*x));
    v = (y-target)*0.01;
    dx = x > 0 && x < 1.0/3 ? -0.03 : 0.0;
    dy = 0.01;
  }
  double lookup(double x,double y) const override {
    double v,dx,dy;
    lookup(x,y,v,dx,dy);
    return v;
  }
};

int main()
{
  mistral::AnalogSim sim;
  sim.add_gnd_vdd(1.0);
  sim.set_timing_scale(0.8,1.2);
  int input = sim.gn_input("input"), output = sim.gn_g(0.0,"output");
  sim.add_buff(input,output,std::make_unique<Sharpen>());
  sim.add_r(output,0,1e9);
  sim.add_c(output,0,1e-14);
  mistral::AnalogSim::wave in{{0.0,0.0},{1e-9,1.0},{2e-9,1.0}}, out;
  mistral::AnalogSim::time_interval interval;
  sim.set_input_wave(input,in);
  sim.set_output_wave(output,out,interval);
  sim.run();
  if(!std::isfinite(interval.mi) || !std::isfinite(interval.mx)
      || interval.mi >= interval.mx || interval.mx >= -1e-10)
    return std::fprintf(stderr,"Signed interval is invalid: %g/%g\n",interval.mi,interval.mx), 1;
  std::puts("A measured negative delay retains an ordered negative derating envelope.");
  return 0;
}
