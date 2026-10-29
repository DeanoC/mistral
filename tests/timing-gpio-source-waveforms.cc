#include "cyclonev.h"

#include <cmath>
#include <cstdio>
#include <memory>

using CV = mistral::CycloneV;
using Wave = mistral::AnalogSim::wave;

static bool same(const Wave &a, const Wave &b)
{
  if(a.size() != b.size())
    return false;
  for(size_t i=0; i<a.size(); i++)
    if(a[i].t != b[i].t || a[i].v != b[i].v)
      return false;
  return true;
}

int main()
{
  std::unique_ptr<CV> cv(CV::get_model("5CSEBA6U23I7"));
  // Independent physical identities used by the native SDRAM pad graph:
  // V12 is direct; AE23 reaches HMC.IOINTDQDIN.147 through the PHY bypass.
  const CV::pnode_coords direct{CV::GPIO,40,0,CV::DATAIN,0,3};
  const CV::pnode_coords bypass{CV::GPIO,82,0,CV::DATAIN,1,3};
  const auto direct_node = cv->rc2ri(CV::rnode_coords{CV::GIN,39,0,1});
  const auto bypass_node = cv->rc2ri(CV::rnode_coords{CV::GIN,81,0,14});
  if(cv->pnode_to_rnode(direct) != direct_node || cv->pnode_to_rnode(bypass) != 0xffffffff
      || cv->rnode_to_pnode(bypass_node).bt() != CV::HMC)
    return std::fprintf(stderr,"Missing independent direct/HMC source identities.\n"),1;
  std::vector<uint8_t> before,after;
  cv->rbf_save(before);
  unsigned cases=0;
  for(auto temp:{CV::T_100,CV::T_N40})
    for(auto delay:{CV::DELAY_MAX,CV::DELAY_MIN})
      for(auto edge:{CV::RF_RISE,CV::RF_FALL})
        for(auto speed:{CV::EST_SLOW,CV::EST_FAST}) {
          Wave oracle,ordinary,explicit_direct,explicit_bypass;
          cv->rnode_timing_build_input_wave(direct_node,temp,delay,edge,speed,oracle);
          cv->rnode_timing_build_input_wave(bypass_node,temp,delay,edge,speed,ordinary);
          if(oracle.size()!=2 || !ordinary.empty()
              || !cv->gpio_timing_build_input_wave(direct,direct_node,temp,delay,edge,speed,explicit_direct)
              || !cv->gpio_timing_build_input_wave(bypass,bypass_node,temp,delay,edge,speed,explicit_bypass)
              || !same(oracle,explicit_direct) || !same(oracle,explicit_bypass))
            return std::fprintf(stderr,"GPIO source wave differs at temp %d delay %d edge %d speed %d.\n",
                int(temp),int(delay),int(edge),int(speed)),1;
          for(auto point:explicit_bypass)
            if(!std::isfinite(point.t) || !std::isfinite(point.v))
              return 1;
          if(explicit_bypass[1].t<=explicit_bypass[0].t)
            return 1;
          ++cases;
        }

  auto rejected=[&](CV::pnode_coords source,CV::rnode_index node,CV::timing_slot_t temp=CV::T_100,
                    CV::delay_type_t delay=CV::DELAY_MAX,CV::edge_t edge=CV::RF_RISE,
                    CV::edge_speed_type speed=CV::EST_SLOW) {
    Wave wave{{0.0,123.0}};
    return !cv->gpio_timing_build_input_wave(source,node,temp,delay,edge,speed,wave) && wave.empty();
  };
  if(!rejected(direct,bypass_node) || !rejected(bypass,direct_node)
      || !rejected(CV::pnode_coords{CV::GPIO,40,0,CV::CLKIN,0,0},direct_node)
      || !rejected(CV::pnode_coords{CV::GPIO,40,0,CV::DATAOUT,0,0},direct_node)
      || !rejected(CV::pnode_coords{CV::GPIO,40,0,CV::DATAIN,0,4},direct_node)
      || !rejected(cv->rnode_to_pnode(bypass_node),bypass_node)
      || !rejected(CV::pnode_coords{CV::GPIO,0,0,CV::DATAIN,0,3},direct_node)
      || !rejected(direct,0xffffffff)
      || !rejected(direct,direct_node,CV::timing_slot_t(255))
      || !rejected(direct,direct_node,CV::T_100,CV::delay_type_t(255))
      || !rejected(direct,direct_node,CV::T_100,CV::DELAY_MAX,CV::edge_t(255))
      || !rejected(direct,direct_node,CV::T_100,CV::DELAY_MAX,CV::RF_RISE,CV::edge_speed_type(255)))
    return std::fprintf(stderr,"Accepted a missing, mismatched or unsupported source identity.\n"),1;
  cv->rbf_save(after);
  if(before!=after)
    return std::fprintf(stderr,"Source-wave queries changed the RBF.\n"),1;
  std::printf("%u explicit direct/bypass GPIO corner/edge/slew cases and 12 rejection checks passed.\n",cases);
  return 0;
}
