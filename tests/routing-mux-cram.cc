#include "cyclonev.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace {

using CV = mistral::CycloneV;
using Bits = std::vector<std::pair<uint32_t, uint32_t>>;

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

int check_sx120f()
{
  std::unique_ptr<CV> cv(CV::get_model("5CSEBA6U23I7"));
  if(!cv)
    return 2;
  int failures = 0;
  auto check = [&](bool value, const char *message) {
    if(!value) {
      std::fprintf(stderr, "FAIL: %s\n", message);
      ++failures;
    }
  };
  check(cv->get_cram_sx() == 7605 && cv->get_cram_sy() == 7024, "device CRAM dimensions");
  // The seven mux destinations isolated from a real ZX81 composition's
  // outside-slot RBF difference. Several logical destinations are in columns
  // 21..33, while their programmable mux bits are physically outside it.
  struct Case { CV::rnode_coords node; uint32_t x, y; bool inside; };
  const Case cases[] = {
    {CV::rnode_coords(CV::H6, 31, 8, 31), 2976, 772, false},
    {CV::rnode_coords(CV::H6, 21, 16, 12), 1743, 1456, false},
    {CV::rnode_coords(CV::V2, 20, 14, 19), 1743, 1452, false},
    {CV::rnode_coords(CV::H6, 21, 14, 3), 1749, 1284, false},
    {CV::rnode_coords(CV::H6, 21, 15, 8), 1749, 1338, false},
    {CV::rnode_coords(CV::H6, 32, 14, 20), 3041, 1280, false},
    {CV::rnode_coords(CV::H6, 32, 17, 34), 3035, 1488, false},
    {CV::rnode_coords(CV::V2, 25, 12, 2), 2083, 1030, true},
  };
  auto contained = [](const Bits &bits, uint32_t x0, uint32_t y0, uint32_t x1, uint32_t y1) {
    return std::all_of(bits.begin(), bits.end(), [&](const std::pair<uint32_t, uint32_t> &bit) {
      return bit.first >= x0 && bit.first < x1 && bit.second >= y0 && bit.second < y1;
    });
  };
  for(const auto &item : cases) {
    Bits actual{{99999, 99999}}, expected;
    for(uint32_t x = item.x; x < item.x + 2; ++x)
      for(uint32_t y = item.y; y < item.y + 4; ++y)
        expected.emplace_back(x, y);
    check(cv->rnode_mux_cram_bits(item.node, actual), "known destination");
    check(actual == expected, item.node.to_string().c_str());
    check(contained(actual, 1769, 32, 2806, 7024) == item.inside, "fixed slot containment");
    check(contained(actual, item.x, item.y, item.x + 2, item.y + 4), "inclusive lower bounds");
    check(!contained(actual, item.x, item.y, item.x + 1, item.y + 4), "exclusive upper X bound");
    check(!contained(actual, item.x, item.y, item.x + 2, item.y + 3), "exclusive upper Y bound");
  }
  // Independently observed non-CRC CRAM differences from the real base/cart
  // RBF pair: every changed outside bit belongs to one of the seven muxes.
  const Bits observed{{2977,772},{2977,774},{3042,1282},{3041,1283},
                      {1749,1284},{1750,1285},{1749,1340},{1749,1341},
                      {1743,1454},{1743,1455},{1744,1458},{1743,1459},
                      {3035,1489},{3035,1491}};
  for(const auto &changed : observed) {
    bool found = false;
    for(const auto &item : cases) {
      if(item.inside)
        continue;
      Bits coordinates;
      cv->rnode_mux_cram_bits(item.node, coordinates);
      found |= std::find(coordinates.begin(), coordinates.end(), changed) != coordinates.end();
    }
    check(found, "observed outside bit covered by offending mux");
  }
  Bits bits{{1, 2}};
  check(!cv->rnode_mux_cram_bits(CV::rnode_coords(CV::H6, 127, 127, 1023), bits), "unknown node rejects");
  check(bits.empty(), "unknown node clears previous result");
  bits.emplace_back(1, 2);
  check(cv->rnode_mux_cram_bits(CV::rnode_coords(CV::GCLK, 0, 36, 0), bits), "fixed node exists");
  check(bits.empty(), "fixed connection has no programmable bits");
  std::printf("Routing mux CRAM coordinates: 7 outside muxes, 1 inside mux, boundaries, unknown/fixed: %s\n",
              failures ? "FAIL" : "PASS");
  return failures ? 1 : 0;
}

int check_gx25f()
{
  const char *model_name = "5CGXFC3B6F23C6";
  std::unique_ptr<CV> cv(CV::get_model(model_name));
  std::unique_ptr<CV> changed(CV::get_model(model_name));
  if(!cv || !changed)
    return 2;
  int failures = 0;
  auto check = [&](bool value, const char *message) {
    if(!value) {
      std::fprintf(stderr, "FAIL: %s\n", message);
      ++failures;
    }
  };
  check(cv->get_cram_sx() == 3856 && cv->get_cram_sy() == 3412, "gx25f CRAM dimensions");
  Bits bits{{1, 2}};
  check(!cv->rnode_mux_cram_bits(CV::rnode_coords(CV::H6, 127, 127, 1023), bits), "gx25f unknown node rejects");
  check(bits.empty(), "gx25f unknown node clears previous result");

  const uint32_t sx = cv->get_cram_sx();
  const uint32_t sy = cv->get_cram_sy();
  int in_grid = 0;
  int linked = 0;
  for(uint32_t index = 0; index != cv->rnode_index_count() && (in_grid < 32 || linked < 4); ++index) {
    const CV::rnode_object *node = cv->ri2ro(index);
    if(!node || node->pattern() >= 0xfe || node->sources_count() == 0)
      continue;
    Bits footprint;
    if(!cv->rnode_mux_cram_bits(node->rc(), footprint) || footprint.empty()) {
      check(false, "gx25f programmable mux has bits");
      continue;
    }
    bool inside = std::all_of(footprint.begin(), footprint.end(), [&](const std::pair<uint32_t, uint32_t> &bit) {
      return bit.first < sx && bit.second < sy;
    });
    if(in_grid < 32) {
      check(inside, "gx25f mux bits lie in the CRAM grid");
      ++in_grid;
    }
    if(linked >= 4 || !inside)
      continue;
    bool wrote = false;
    for(uint32_t s = 0; s != node->sources_count() && !wrote; ++s) {
      changed->clear();
      changed->rnode_link(node->sources_begin()[s], node->ri());
      std::string text, other;
      Bits written;
      if(!capture_diff(*changed, *cv, text) || !parse_cram_diff(text, written, other)) {
        check(false, "gx25f link diff is cram coordinates");
        wrote = true;
        break;
      }
      if(written.empty())
        continue;
      bool covered = std::all_of(written.begin(), written.end(), [&](const std::pair<uint32_t, uint32_t> &bit) {
        return std::find(footprint.begin(), footprint.end(), bit) != footprint.end();
      });
      check(covered, "gx25f diff bits are inside the mux footprint");
      wrote = true;
      ++linked;
    }
  }
  check(in_grid == 32, "gx25f found programmable muxes");
  check(linked == 4, "gx25f linked non-default mux sources");
  changed->clear();
  std::string restored;
  check(capture_diff(*changed, *cv, restored) && restored.empty(), "gx25f CRAM restored after links");
  std::printf("Routing mux CRAM gx25f: grid=%d linked=%d: %s\n",
              in_grid, linked, failures ? "FAIL" : "PASS");
  return failures ? 1 : 0;
}

} // namespace

int main()
{
  int sx = check_sx120f();
  int gx = check_gx25f();
  if(sx == 2 || gx == 2)
    return 2;
  return sx || gx ? 1 : 0;
}
