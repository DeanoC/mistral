// The test target disables access checks to inspect the actual generated RC
// network without changing the library headers or adding a public test API.
#include "cyclonev.h"

#include <map>
#include <set>

using CV = mistral::CycloneV;
using Sim = mistral::AnalogSim;

// Reconstruct physical positions from resistors (one ohm/unit), then compare
// capacitors with the independently specified ordered load list.
static bool check(CV &cv, const std::vector<CV::rnode_index> &targets,
                  const std::vector<float> &fixed_caps,
                  const std::vector<uint16_t> &positions, int split,
                  uint16_t split_pos, bool second, CV::edge_t edge,
                  const CV::dnode_driver *drivers,
                  CV::rnode_index source = 0xffffffff,
                  const std::set<CV::rnode_index> *active = nullptr)
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
      0.0, line, source, drivers, sim, outputs);
  // First span leaves its final capacitance for the caller to attach.
  if(!second && caps)
    sim.add_c(node, 0, caps);

  std::map<int, int> coordinates{{start, second ? split_pos : 0}};
  std::map<int, double> actual, expected;
  std::multiset<std::pair<CV::rnode_index, int>> expected_outputs;
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
      const auto &driver = drivers[ro->driver((positions[i] & 0x4000) ? 1 : 0)];
      bool connected = active && active->count(ro->ri());
      load = (connected ? driver.con : driver.coff).rf[edge];
      if(connected && (second ? int(i) >= split : int(i) < split))
        expected_outputs.emplace(ro->ri(), positions[i] & 0x3fff);
    }
    if(second ? int(i) >= split : int(i) < split)
      expected[positions[i] & 0x3fff] += load;
  }
  if(actual != expected) {
    std::fprintf(stderr, "load map differs: entries=%zu split=%d span=%d edge=%d\n",
                 positions.size(), split, second, edge);
    return false;
  }
  if(active) {
    std::multiset<std::pair<CV::rnode_index, int>> actual_outputs;
    for(const auto &output : outputs) {
      if(!coordinates.count(output.second))
        return false;
      actual_outputs.emplace(output.first, coordinates.at(output.second));
    }
    if(actual_outputs != expected_outputs) {
      std::fprintf(stderr, "active output map differs: split=%d span=%d edge=%d\n",
                   split, second, edge);
      return false;
    }
  }
  return true;
}

static int check_active_outputs(CV &cv, CV::dnode_driver *drivers)
{
  CV::rnode_index source = 0xffffffff;
  std::vector<CV::rnode_index> targets;
  // Find two programmable receivers of one real source. The fixture writes
  // their actual mux selectors, rather than relying on default/sentinel links.
  for(uint32_t rn = 0; rn != cv.rnode_index_count(); ++rn) {
    auto ro = cv.ri2ro(rn);
    if(!ro)
      continue;
    targets.clear();
    for(auto p = ro->targets_begin(); p != ro->targets_end(); ++p) {
      auto receiver = cv.ri2ro(*p);
      if(!receiver || receiver->pattern() >= 0xfe ||
         receiver->driver(0) == 0xff || receiver->driver(1) == 0xff)
        continue;
      bool reciprocal = false, alternative = false;
      for(auto s = receiver->sources_begin(); s != receiver->sources_end(); ++s) {
        reciprocal |= *s == rn;
        alternative |= *s != rn && *s != 0xffffffff;
      }
      if(reciprocal && alternative && (targets.empty() || targets.front() != *p))
        targets.push_back(*p);
      if(targets.size() == 2)
        break;
    }
    if(targets.size() == 2) {
      source = rn;
      break;
    }
  }
  if(source == 0xffffffff)
    return -1;
  for(int i = 0; i != 256; ++i)
    for(int edge = 0; edge != 2; ++edge)
      drivers[i].con.rf[edge] = drivers[i].coff.rf[edge] + 5000;

  int checked = 0;
  // Both transition directions, every split and exact split-boundary loads.
  const std::vector<uint16_t> positions{100, 0x8000 | 200, 0x4000 | 300, 0x8000 | 400};
  const std::vector<float> fixed_caps{10001, 10101};
  for(unsigned mask = 0; mask != 4; ++mask) {
    std::set<CV::rnode_index> active;
    for(size_t i = 0; i != targets.size(); ++i) {
      auto receiver = cv.ri2ro(targets[i]);
      CV::rnode_index selected = source;
      if(mask & (1u << i))
        active.insert(targets[i]);
      else {
        for(auto s = receiver->sources_begin(); s != receiver->sources_end(); ++s)
          if(*s != source && *s != 0xffffffff) {
            selected = *s;
            break;
          }
      }
      cv.rnode_link(selected, targets[i]);
      if(cv.rmux_get_source(receiver) != selected)
        return -1;
    }
    for(int split = 0; split <= 4; ++split)
      for(int equal = 0; equal != 2; ++equal)
        for(int span = 0; span != 2; ++span)
          for(int edge = 0; edge != 2; ++edge) {
            uint16_t split_pos = split * 100 + (equal ? 100 : 50);
            if(!check(cv, targets, fixed_caps, positions, split, split_pos,
                      span, CV::edge_t(edge), drivers, source, &active))
              return -1;
            ++checked;
          }
  }
  return checked;
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
  int active_checked = check_active_outputs(*cv, drivers);
  if(active_checked < 0)
    return 1;
  std::printf("%d configured receiver load/output maps passed\n", active_checked);
  return 0;
}
