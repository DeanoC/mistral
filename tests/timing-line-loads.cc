// The test target disables access checks to inspect the actual generated RC
// network without changing the library headers or adding a public test API.
#include "cyclonev.h"

#include <map>

using CV = mistral::CycloneV;
using Sim = mistral::AnalogSim;

// Reconstruct physical positions from resistors (one ohm/unit), then compare
// capacitors with the independently specified ordered load list.
static bool check(CV &cv, const std::vector<CV::rnode_index> &targets,
                  const std::vector<float> &fixed_caps,
                  const std::vector<uint16_t> &positions, int split,
                  uint16_t split_pos, bool second, CV::edge_t edge,
                  const CV::dnode_driver *drivers)
{
  CV::rnode_line_information line{};
  line.length = (positions.size() + 2) * 100;
  Sim sim;
  sim.add_gnd_vdd(1.0);
  int node = sim.gn_g(0.0);
  const int start = node;
  double caps = 0;
  std::vector<std::pair<CV::rnode_index, int>> outputs;
  cv.rnode_timing_generate_line(targets.data(), fixed_caps.data(), positions.data(),
      split, positions.size(), split_pos, second, 1, caps, node, 1.0, edge,
      0.0, line, 0xffffffff, drivers, sim, outputs);
  // First span leaves its final capacitance for the caller to attach.
  if(!second && caps)
    sim.add_c(node, 0, caps);

  std::map<int, int> coordinates{{start, second ? split_pos : 0}};
  std::map<int, double> actual, expected;
  for(const auto &c : sim.components) {
    if(c.type == Sim::C_R) {
      if(!coordinates.count(c.nodes[0]) || coordinates.count(c.nodes[1]))
        return false;
      coordinates[c.nodes[1]] = coordinates.at(c.nodes[0]) + int(c.param);
    } else if(c.type == Sim::C_C) {
      if(c.nodes[1] != 0 || !coordinates.count(c.nodes[0]))
        return false;
      if(c.param)
        actual[coordinates.at(c.nodes[0])] += c.param;
    } else
      return false;
  }
  size_t receiver = 0, fixed = 0;
  for(size_t i = 0; i != positions.size(); ++i) {
    double load;
    if(positions[i] & 0x8000)
      load = fixed_caps.at(fixed++);
    else {
      auto ro = cv.ri2ro(targets.at(receiver++));
      load = drivers[ro->driver((positions[i] & 0x4000) ? 1 : 0)].coff.rf[edge];
    }
    if(second ? int(i) >= split : int(i) < split)
      expected[positions[i] & 0x3fff] += load;
  }
  if(actual != expected) {
    std::fprintf(stderr, "load map differs: entries=%zu split=%d span=%d edge=%d\n",
                 positions.size(), split, second, edge);
    return false;
  }
  return true;
}

int main()
{
  std::unique_ptr<CV> cv(CV::get_model("5CSEBA6U23I7"));
  if(!cv)
    return 2;
  // Real receivers with different driver types make skipped entries visible.
  std::vector<CV::rnode_index> receivers;
  for(uint32_t rn = 0; rn != cv->rnode_index_count(); ++rn) {
    auto ro = cv->ri2ro(rn);
    if(!ro || ro->pattern() >= 0xfe || ro->driver(0) == 0xff || ro->driver(1) == 0xff)
      continue;
    if(receivers.empty() || ro->driver(0) != cv->ri2ro(receivers.front())->driver(0))
      receivers.push_back(rn);
    if(receivers.size() == 2)
      break;
  }
  if(receivers.size() != 2)
    return 2;
  CV::dnode_driver drivers[256]{};
  for(int i = 0; i != 256; ++i)
    for(int edge = 0; edge != 2; ++edge)
      drivers[i].con.rf[edge] = drivers[i].coff.rf[edge] = 10 * (i + 1) + edge;

  int checked = 0;
  // Empty/homogeneous lines, both mixed transitions, every split prefix and
  // final entry. Exact-size arrays also expose end overreads under ASan.
  for(int count = 0; count <= 6; ++count)
    for(unsigned mask = 0; mask != (1u << count); ++mask) {
      std::vector<uint16_t> positions;
      std::vector<CV::rnode_index> targets;
      std::vector<float> fixed_caps;
      for(int i = 0; i != count; ++i) {
        bool fixed = mask & (1u << i);
        positions.push_back((i + 1) * 100 | (fixed ? 0x8000 : (i & 1 ? 0x4000 : 0)));
        if(fixed)
          fixed_caps.push_back(10001 + 100 * fixed_caps.size());
        else
          targets.push_back(receivers[targets.size() & 1]);
      }
      positions.shrink_to_fit();
      targets.shrink_to_fit();
      fixed_caps.shrink_to_fit();
      for(int split = 0; split <= count; ++split)
        for(int equal = 0; equal != 2; ++equal)
          for(int span = 0; span != 2; ++span)
            for(int edge = 0; edge != 2; ++edge) {
              uint16_t split_pos = split * 100 + (equal ? 100 : 50);
              if(!check(*cv, targets, fixed_caps, positions, split, split_pos,
                        span, CV::edge_t(edge), drivers))
                return 1;
              ++checked;
            }
    }
  std::printf("%d split-line RC load maps passed\n", checked);
  return 0;
}
