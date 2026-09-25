#include "cyclonev.h"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

namespace {

using CV = mistral::CycloneV;
using Bits = std::vector<std::pair<uint32_t, uint32_t>>;

struct Anchor {
  CV::rnode_type_t type;
  uint32_t x, y, z;
  const char *name;
  uint32_t bit_x, bit_y;
  uint32_t box_min_x, box_max_x, box_min_y, box_max_y;
};

struct Expect {
  const char *model;
  uint32_t cram_sx, cram_sy;
  size_t nodes;
  uint32_t outside;
  uint32_t max_distance;
  const Anchor *anchor;
  bool fixed_gclk;
};

const Anchor sx120f_gout = {
  CV::GOUT, 1, 0, 21, "GOUT.001.000.0021",
  64, 43, 55, 64, 40, 42
};

const Expect expects[] = {
  {"5CSEBA6U23I7", 7605, 7024, 11895, 1987, 3, &sx120f_gout, true},
  {"5CGXFC3B6F23C6", 3856, 3412, 5922, 1430, 3, nullptr, false},
};

bool capture_diff(CV &changed, const CV &baseline, std::string &out)
{
  out.clear();
  FILE *fp = std::tmpfile();
  if(!fp)
    return false;
  changed.diff(&baseline, fp);
  std::fflush(fp);
  std::rewind(fp);
  char buf[4096];
  while(size_t n = std::fread(buf, 1, sizeof buf, fp))
    out.append(buf, n);
  std::fclose(fp);
  return true;
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

const Expect *find_expect(const char *model)
{
  for(const Expect &item : expects)
    if(std::strcmp(item.model, model) == 0)
      return &item;
  return nullptr;
}

int run_model(const Expect &expect, uint32_t stride)
{
  std::unique_ptr<CV> model(CV::get_model(expect.model));
  std::unique_ptr<CV> changed(CV::get_model(expect.model));
  if(!model || !changed)
    return 2;

  int failures = 0;
  auto check = [&](bool value, const char *message) {
    if(!value) {
      std::fprintf(stderr, "FAIL: %s\n", message);
      ++failures;
    }
  };

  char msg[160];
  std::snprintf(msg, sizeof msg, "%s CRAM dimensions", expect.model);
  check(model->get_cram_sx() == expect.cram_sx && model->get_cram_sy() == expect.cram_sy, msg);

  const auto settings = model->inv_get();
  std::snprintf(msg, sizeof msg, "%s inverter node count is %zu (got %zu)",
                expect.model, expect.nodes, settings.size());
  check(settings.size() == expect.nodes, msg);

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

  if(expect.fixed_gclk) {
    const CV::rnode_coords fixed_node(CV::GCLK, 0, 36, 0);
    scratch.emplace_back(1, 2);
    check(model->rnode_inverter_cram_bit(fixed_node, scratch), "fixed node exists");
    check(scratch.empty(), "fixed node has no inverter");
    scratch.emplace_back(1, 2);
    check(model->rnode_cram_footprint(fixed_node, scratch), "fixed node footprint exists");
    check(scratch.empty(), "fixed node footprint is empty");
  }

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

  std::string initial;
  check(capture_diff(*changed, *model, initial), "capture diff");
  check(initial.empty(), "cleared models start with identical CRAM");

  bool saw_anchor = false;
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

    if(expect.anchor && coords == CV::rnode_coords(expect.anchor->type, expect.anchor->x, expect.anchor->y, expect.anchor->z)) {
      const Anchor &anchor = *expect.anchor;
      saw_anchor = true;
      check(coords.to_string() == anchor.name, "anchor example name");
      check(inverter.size() == 1 && inverter[0] == std::make_pair(anchor.bit_x, anchor.bit_y), "anchor inverter coordinate");
      if(!mux.empty()) {
        uint32_t min_x = mux[0].first, max_x = mux[0].first;
        uint32_t min_y = mux[0].second, max_y = mux[0].second;
        for(const auto &bit : mux) {
          min_x = std::min(min_x, bit.first);
          max_x = std::max(max_x, bit.first);
          min_y = std::min(min_y, bit.second);
          max_y = std::max(max_y, bit.second);
        }
        std::snprintf(msg, sizeof msg, "%s mux bounding box", anchor.name);
        check(min_x == anchor.box_min_x && max_x == anchor.box_max_x &&
              min_y == anchor.box_min_y && max_y == anchor.box_max_y, msg);
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
    std::string diff;
    bool captured = capture_diff(*changed, *model, diff);
    bool parsed = captured && parse_cram_diff(diff, written, other);
    if(!changed->inv_set(setting.node, setting.value))
      check(false, "inv_set restores an inverter node");
    ++oracle_checked;
    if(!parsed || written.size() != 1 || written[0] != inverter[0]) {
      ++mismatches;
      if(printed < 8) {
        std::fprintf(stderr, "FAIL: oracle %s query=(%u,%u) written=%zu extra=\"%s\"\n",
                     coords.to_string().c_str(),
                     inverter.empty() ? 0 : inverter[0].first,
                     inverter.empty() ? 0 : inverter[0].second,
                     written.size(), other.c_str());
        ++printed;
      }
    }
  }

  std::string restored;
  check(capture_diff(*changed, *model, restored), "capture restored diff");
  check(restored.empty(), "CRAM matches the cleared model after the oracle");
  if(expect.anchor)
    check(saw_anchor, "anchor inverter node is present");
  std::snprintf(msg, sizeof msg, "%s outside count is %u (got %u)", expect.model, expect.outside, outside);
  check(outside == expect.outside, msg);
  std::snprintf(msg, sizeof msg, "%s max distance is %u (got %u)", expect.model, expect.max_distance, max_distance);
  check(max_distance == expect.max_distance, msg);
  check(mismatches == 0, "oracle mismatches");
  if(stride == 1)
    check(oracle_checked == settings.size(), "oracle covered every inverter node");
  else
    check(oracle_checked > 0, "sampled oracle covered at least one node");

  double seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - started).count();
  std::printf("Routing inverter CRAM %s: nodes=%zu unique=%zu outside=%u max_distance=%u oracle=%u mismatches=%u stride=%u seconds=%.2f: %s\n",
              expect.model, settings.size(), inverter_nodes.size(), outside, max_distance,
              oracle_checked, mismatches, stride, seconds, failures || mismatches ? "FAIL" : "PASS");
  return failures || mismatches ? 1 : 0;
}

} // namespace

int main(int argc, char **argv)
{
  uint32_t stride = 1;
  const char *model_name = "5CSEBA6U23I7";
  int argi = 1;
  if(argi < argc && std::strcmp(argv[argi], "--sample") == 0) {
    if(argi + 1 >= argc)
      goto usage;
    stride = static_cast<uint32_t>(std::strtoul(argv[argi + 1], nullptr, 10));
    if(stride == 0)
      return 2;
    argi += 2;
  }
  if(argi < argc)
    model_name = argv[argi++];
  if(argi != argc) {
  usage:
    std::fprintf(stderr, "usage: %s [--sample N] [model]\n", argv[0]);
    return 2;
  }

  const Expect *expect = find_expect(model_name);
  if(!expect) {
    std::fprintf(stderr, "no locked expectations for %s\n", model_name);
    return 2;
  }
  return run_model(*expect, stride);
}
