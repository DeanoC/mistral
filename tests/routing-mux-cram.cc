#include "cyclonev.h"

#include <algorithm>
#include <cstdio>
#include <memory>
#include <utility>
#include <vector>

int main()
{
  using CV = mistral::CycloneV;
  using Bits = std::vector<std::pair<uint32_t, uint32_t>>;
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
  struct Case { CV::rnode_t node; uint32_t x, y; bool inside; };
  const Case cases[] = {
    {CV::rnode(CV::H6, 31, 8, 31), 2976, 772, false},
    {CV::rnode(CV::H6, 21, 16, 12), 1743, 1456, false},
    {CV::rnode(CV::V2, 20, 14, 19), 1743, 1452, false},
    {CV::rnode(CV::H6, 21, 14, 3), 1749, 1284, false},
    {CV::rnode(CV::H6, 21, 15, 8), 1749, 1338, false},
    {CV::rnode(CV::H6, 32, 14, 20), 3041, 1280, false},
    {CV::rnode(CV::H6, 32, 17, 34), 3035, 1488, false},
    {CV::rnode(CV::V2, 25, 12, 2), 2083, 1030, true},
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
    check(actual == expected, CV::rn2s(item.node).c_str());
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
  check(!cv->rnode_mux_cram_bits(CV::rnode(CV::H6, 127, 127, 1023), bits), "unknown node rejects");
  check(bits.empty(), "unknown node clears previous result");
  bits.emplace_back(1, 2);
  check(cv->rnode_mux_cram_bits(CV::rnode(CV::GCLK, 0, 36, 0), bits), "fixed node exists");
  check(bits.empty(), "fixed connection has no programmable bits");
  std::printf("Routing mux CRAM coordinates: 7 outside muxes, 1 inside mux, boundaries, unknown/fixed: %s\n",
              failures ? "FAIL" : "PASS");
  return failures ? 1 : 0;
}
