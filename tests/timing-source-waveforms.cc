#include "cyclonev.h"

#include <cmath>
#include <cstdio>
#include <map>
#include <memory>

using CV = mistral::CycloneV;

static CV::bmux_type_t selector(CV::port_type_t port)
{
  switch(port) {
  case CV::FFT0: return CV::TDFF0;
  case CV::FFT1: return CV::TDFF1;
  case CV::FFT1L: return CV::TDFF1L;
  case CV::FFB0: return CV::BDFF0;
  case CV::FFB1: return CV::BDFF1;
  default: return CV::BDFF1L;
  }
}

int main()
{
  std::unique_ptr<CV> cv(CV::get_model("5CSEBA6U23I7"));
  if(!cv)
    return 2;
  std::map<std::pair<CV::block_type_t,CV::port_type_t>,CV::rnode_index> roots;
  for(uint32_t rn = 0; rn != cv->rnode_index_count() && roots.size() != 12; ++rn) {
    if(!cv->ri2ro(rn))
      continue;
    auto p = cv->rnode_to_pnode(rn);
    if(p.bt() != CV::LAB && p.bt() != CV::MLAB)
      continue;
    if(p.pt() != CV::FFT0 && p.pt() != CV::FFT1 && p.pt() != CV::FFT1L
        && p.pt() != CV::FFB0 && p.pt() != CV::FFB1 && p.pt() != CV::FFB1L)
      continue;
    roots.emplace(std::make_pair(p.bt(),p.pt()),rn);
  }
  if(roots.size() != 12)
    return std::fprintf(stderr, "Missing LAB/MLAB source-port inventory: %zu\n",roots.size()), 1;
  unsigned cases = 0;
  for(auto root : roots) {
    auto p = cv->rnode_to_pnode(root.second);
    for(auto mode : {CV::REG,CV::NLUT}) {
      if(!cv->bmux_m_set(p.bt(),p.p(),selector(p.pt()),p.bi(),mode))
        return 1;
      for(auto temp : {CV::T_100,CV::T_N40})
        for(auto delay : {CV::DELAY_MAX,CV::DELAY_MIN})
          for(auto edge : {CV::RF_RISE,CV::RF_FALL}) {
            auto index = cv->dn_lookup->index_sg[cv->model->speed_grade][temp][delay];
            double supply = cv->dn_info[index].vdd;
            // Stored voltage ordinates are rounded to five decimal places.
            const double rail_tolerance = 1e-5;
            mistral::AnalogSim::wave wave;
            cv->rnode_timing_build_input_wave(root.second,temp,delay,edge,CV::EST_SLOW,wave);
            if(wave.size() != 10)
              return 1;
            double previous = -1e300;
            for(auto point : wave) {
              if(!std::isfinite(point.t) || !std::isfinite(point.v) || point.t < previous
                  || point.v < -rail_tolerance || point.v > supply+rail_tolerance)
                return std::fprintf(stderr, "Source %s mode %d temp %d delay %d edge %d: time %g previous %g volts %g supply %g\n",
                    p.to_string().c_str(),int(mode),int(temp),int(delay),int(edge),point.t,previous,point.v,supply), 1;
              previous = point.t;
            }
            double initial = edge == CV::RF_RISE ? 0.0 : supply;
            double final = edge == CV::RF_RISE ? supply : 0.0;
            if(std::abs(wave.front().v-initial) > rail_tolerance || std::abs(wave.back().v-final) > rail_tolerance)
              return std::fprintf(stderr, "Source endpoints do not match supply %g\n",supply), 1;
            ++cases;
          }
    }
  }
  std::printf("%u actual LAB/MLAB source waveforms match their selected supplies.\n",cases);
  return 0;
}
