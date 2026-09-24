#include "cyclonev.h"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>
#include <unistd.h>
#include <unordered_set>
#include <utility>
#include <vector>

namespace {

using CV = mistral::CycloneV;
using Bits = std::vector<std::pair<uint32_t, uint32_t>>;

std::string capture_diff(CV &changed, const CV &baseline)
{
  char path[] = "/tmp/mistral-inv-diff-XXXXXX";
  int fd = mkstemp(path);
  if(fd < 0)
    return std::string();
  unlink(path);

  fflush(stdout);
  int saved = dup(STDOUT_FILENO);
  dup2(fd, STDOUT_FILENO);
  changed.diff(&baseline);
  fflush(stdout);
  dup2(saved, STDOUT_FILENO);
  close(saved);

  off_t end = lseek(fd, 0, SEEK_END);
  std::string out;
  if(end > 0) {
    lseek(fd, 0, SEEK_SET);
    out.resize(static_cast<size_t>(end));
    size_t got = 0;
    while(got < out.size()) {
      ssize_t n = ::read(fd, &out[got], out.size() - got);
      if(n <= 0)
        break;
      got += static_cast<size_t>(n);
    }
    out.resize(got);
  }
  close(fd);
  return out;
}

bool parse_cram_diff(const std::string &text, Bits &bits, std::string &other)
{
  bits.clear();
  other.clear();
  size_t cursor = 0;
  while(cursor < text.size()) {
    size_t eol = text.find('\n', cursor);
    if(eol == std::string::npos)
      eol = text.size();
    std::string line = text.substr(cursor, eol - cursor);
    cursor = eol < text.size() ? eol + 1 : text.size();
    if(line.empty())
      continue;
    unsigned linear = 0, x = 0, y = 0;
    if(std::sscanf(line.c_str(), "cram %u %u.%u", &linear, &x, &y) == 3)
      bits.emplace_back(x, y);
    else {
      other = line;
      return false;
    }
  }
  return true;
}

Bits union_bits(const Bits &mux, const Bits &inverter)
{
  Bits expected = mux;
  for(const auto &bit : inverter)
    if(std::find(expected.begin(), expected.end(), bit) == expected.end())
      expected.push_back(bit);
  return expected;
}

uint32_t outside_distance(const Bits &mux, uint32_t x, uint32_t y, bool &outside)
{
  uint32_t min_x = mux[0].first, max_x = mux[0].first;
  uint32_t min_y = mux[0].second, max_y = mux[0].second;
  for(const auto &bit : mux) {
    min_x = std::min(min_x, bit.first);
    max_x = std::max(max_x, bit.first);
    min_y = std::min(min_y, bit.second);
    max_y = std::max(max_y, bit.second);
  }
  uint32_t dx = x < min_x ? min_x - x : (x > max_x ? x - max_x : 0);
  uint32_t dy = y < min_y ? min_y - y : (y > max_y ? y - max_y : 0);
  outside = dx != 0 || dy != 0;
  return std::max(dx, dy);
}

} // namespace

int main(int argc, char **argv)
{
  uint32_t stride = 1;
  if(argc == 3 && std::strcmp(argv[1], "--sample") == 0) {
    stride = static_cast<uint32_t>(std::strtoul(argv[2], nullptr, 10));
    if(stride == 0)
      return 2;
  } else if(argc != 1) {
    std::fprintf(stderr, "usage: %s [--sample N]\n", argv[0]);
    return 2;
  }

  std::unique_ptr<CV> model(CV::get_model("5CSEBA6U23I7"));
  std::unique_ptr<CV> changed(CV::get_model("5CSEBA6U23I7"));
  if(!model || !changed)
    return 2;

  int failures = 0;
  auto check = [&](bool value, const char *message) {
    if(!value) {
      std::fprintf(stderr, "FAIL: %s\n", message);
      ++failures;
    }
  };

  const auto settings = model->inv_get();
  check(settings.size() == 11895, "inverter node count is 11895");

  std::unordered_set<CV::rnode_index> inverter_nodes;
  inverter_nodes.reserve(settings.size() * 2);
  for(const auto &setting : settings)
    inverter_nodes.insert(setting.node);
  check(inverter_nodes.size() == settings.size(), "inverter nodes are unique");

  Bits scratch{{1, 2}};
  const CV::rnode_coords unknown(CV::H6, 127, 127, 1023);
  check(!model->rnode_inverter_cram_bit(unknown, scratch), "unknown node rejects");
  check(scratch.empty(), "unknown node clears previous inverter result");
  scratch.emplace_back(1, 2);
  check(!model->rnode_cram_footprint(unknown, scratch), "unknown node rejects footprint");
  check(scratch.empty(), "unknown node clears previous footprint");

  const CV::rnode_coords fixed_node(CV::GCLK, 0, 36, 0);
  scratch.emplace_back(1, 2);
  check(model->rnode_inverter_cram_bit(fixed_node, scratch), "fixed node exists");
  check(scratch.empty(), "fixed node has no inverter");
  scratch.emplace_back(1, 2);
  check(model->rnode_cram_footprint(fixed_node, scratch), "fixed node footprint exists");
  check(scratch.empty(), "fixed node footprint is empty");

  std::vector<CV::rnode_coords> plain;
  for(uint32_t index = 0; index != model->rnode_index_count() && plain.size() < 8; ++index) {
    const CV::rnode_object *node = model->ri2ro(index);
    if(!node || inverter_nodes.count(index))
      continue;
    plain.push_back(node->rc());
  }
  check(plain.size() == 8, "found nodes without inverters");
  for(const auto &node : plain) {
    scratch.emplace_back(9, 9);
    check(model->rnode_inverter_cram_bit(node, scratch), "node without inverter is known");
    check(scratch.empty(), "node without inverter returns an empty result");
    Bits mux, footprint;
    check(model->rnode_mux_cram_bits(node, mux), "node without inverter has a mux query");
    check(model->rnode_cram_footprint(node, footprint), "node without inverter has a footprint");
    check(footprint == mux, "footprint of a node without an inverter is the mux bits");
  }

  std::string initial = capture_diff(*changed, *model);
  check(initial.empty(), "cleared models start with identical CRAM");

  const CV::rnode_coords gout(CV::GOUT, 1, 0, 21);
  bool saw_gout = false;
  uint32_t outside = 0;
  uint32_t max_distance = 0;
  uint32_t oracle_checked = 0;
  uint32_t mismatches = 0;
  int printed = 0;
  auto started = std::chrono::steady_clock::now();

  for(uint32_t nth = 0; nth != settings.size(); ++nth) {
    const auto &setting = settings[nth];
    const CV::rnode_object *node = model->ri2ro(setting.node);
    if(!node) {
      check(false, "inverter index is not a routing node");
      continue;
    }
    CV::rnode_coords coords = node->rc();
    Bits inverter, mux, footprint;
    check(model->rnode_inverter_cram_bit(coords, inverter), "inverter node is known");
    check(inverter.size() == 1, "inverter node has one CRAM bit");
    check(model->rnode_mux_cram_bits(coords, mux), "inverter node mux query");
    check(!mux.empty(), "inverter node has mux selector bits");
    check(model->rnode_cram_footprint(coords, footprint), "inverter node footprint");
    check(footprint == union_bits(mux, inverter), "footprint is mux bits plus the inverter bit");

    if(inverter.size() == 1 && !mux.empty()) {
      bool is_outside = false;
      uint32_t distance = outside_distance(mux, inverter[0].first, inverter[0].second, is_outside);
      if(is_outside)
        ++outside;
      max_distance = std::max(max_distance, distance);
    }

    if(coords == gout) {
      saw_gout = true;
      check(coords.to_string() == "GOUT.001.000.0021", "GOUT example name");
      check(inverter.size() == 1 && inverter[0] == std::make_pair(64u, 43u), "GOUT.001.000.0021 inverter is (64, 43)");
      if(!mux.empty()) {
        uint32_t min_x = mux[0].first, max_x = mux[0].first;
        uint32_t min_y = mux[0].second, max_y = mux[0].second;
        for(const auto &bit : mux) {
          min_x = std::min(min_x, bit.first);
          max_x = std::max(max_x, bit.first);
          min_y = std::min(min_y, bit.second);
          max_y = std::max(max_y, bit.second);
        }
        check(min_x == 55 && max_x == 64 && min_y == 40 && max_y == 42, "GOUT mux bounding box is (55..64, 40..42)");
      }
    }

    if(nth % stride != 0 || inverter.size() != 1)
      continue;

    if(!changed->inv_set(setting.node, !setting.value)) {
      check(false, "inv_set accepts an inverter node");
      continue;
    }
    Bits written;
    std::string other;
    std::string diff = capture_diff(*changed, *model);
    bool parsed = parse_cram_diff(diff, written, other);
    if(!changed->inv_set(setting.node, setting.value))
      check(false, "inv_set restores an inverter node");
    ++oracle_checked;
    if(!parsed || written.size() != 1 || written[0] != inverter[0]) {
      ++mismatches;
      if(printed < 8) {
        std::fprintf(stderr, "FAIL: oracle %s query=(%u,%u) written=%zu extra=\"%s\"\n",
                     coords.to_string().c_str(),
                     inverter[0].first, inverter[0].second,
                     written.size(), other.c_str());
        ++printed;
      }
    }
  }

  std::string restored = capture_diff(*changed, *model);
  check(restored.empty(), "CRAM matches the cleared model after the oracle");
  check(saw_gout, "GOUT.001.000.0021 is an inverter node");
  check(outside == 1987, "1987 inverter bits sit outside their mux bounding box");
  check(max_distance == 3, "farthest inverter bit is 3 CRAM positions from its mux box");
  check(mismatches == 0, "oracle mismatches");
  if(stride == 1)
    check(oracle_checked == settings.size(), "oracle covered every inverter node");
  else
    check(oracle_checked > 0, "sampled oracle covered at least one node");

  double seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - started).count();
  std::printf("Routing inverter CRAM: nodes=%zu unique=%zu outside=%u max_distance=%u oracle=%u mismatches=%u stride=%u seconds=%.2f: %s\n",
              settings.size(), inverter_nodes.size(), outside, max_distance,
              oracle_checked, mismatches, stride, seconds, failures || mismatches ? "FAIL" : "PASS");
  return failures || mismatches ? 1 : 0;
}
