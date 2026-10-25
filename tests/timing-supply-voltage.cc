// A linear RC circuit has the same half-supply propagation delay when all
// voltages scale together. Exercise the public simulator with actual model
// supplies, independently of the Cyclone V driver tables and routing database.
#include "mistral-analogsim.h"

#include <cmath>
#include <cstdio>

int main()
{
  double reference[2] = {};
  for(double vdd : {1.0, 1.006, 1.18})
    for(int edge = 0; edge != 2; ++edge) {
      const bool falling = edge != 0;
      mistral::AnalogSim sim;
      sim.add_gnd_vdd(vdd);
      sim.set_max_dv(0.002 * vdd);
      int input = sim.gn_input("input");
      int output = sim.gn_g(falling ? vdd : 0.0, "output");
      sim.add_r(input, output, 1000.0);
      sim.add_c(output, 0, 1e-12);
      const double ramp = 1e-12, tau = 1e-9;
      mistral::AnalogSim::wave in{{0.0, falling ? vdd : 0.0},
          {ramp, falling ? 0.0 : vdd}, {1e-8, falling ? 0.0 : vdd}};
      mistral::AnalogSim::wave out;
      mistral::AnalogSim::time_interval delay;
      sim.set_input_wave(input, in);
      sim.set_output_wave(output, out, delay);
      sim.run();
      // Exact ramp-driven RC half-supply delay. Allow the simulator's fixed
      // initial 20 ps integration step, while checking voltage invariance
      // separately to 1 ps. Do not bake an observed simulator delay into the test.
      const double expected = tau * std::log(2*tau/ramp*std::expm1(ramp/tau)) - ramp/2;
      if(vdd == 1.0)
        reference[edge] = delay.mi;
      bool valid = std::isfinite(delay.mi) && std::isfinite(delay.mx)
          && std::abs(delay.mi - expected) < 15e-12
          && std::abs(delay.mx - expected) < 15e-12
          && std::abs(delay.mi - reference[edge]) < 1e-12
          && std::abs(delay.mx - reference[edge]) < 1e-12;
      if(!valid) {
        std::fprintf(stderr, "VDD %.3f %s: delay %.6f/%.6f ps, expected %.6f ps, reference %.6f ps\n",
            vdd, falling ? "fall" : "rise", delay.mi*1e12, delay.mx*1e12,
            expected*1e12, reference[edge]*1e12);
        return 1;
      }
    }
  std::puts("Supply-scaled RC rise/fall timing matches the analytical envelope and 1 V reference.");
  return 0;
}
