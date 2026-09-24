#include "cyclonev.h"

#include <cstdio>
#include <fstream>
#include <iterator>
#include <memory>
#include <string>
#include <vector>

// Compare fixed-placement Quartus references that differ only in clock polarity.
// Normalizing JTAG_ID removes the synthesis-generated design identifier.
int main(int argc, char **argv)
{
  using CV = mistral::CycloneV;
  if(argc < 3 || argc > 5) {
    std::fprintf(stderr, "usage: lab-clock-oracle positive.rbf negative.rbf [LAB|MLAB] [1|3]\n");
    return 2;
  }
  std::unique_ptr<CV> positive(CV::get_model("5CSEBA6U23I7"));
  std::unique_ptr<CV> negative(CV::get_model("5CSEBA6U23I7"));
  CV *models[] = {positive.get(), negative.get()};
  for(int i = 0; i != 2; ++i) {
    std::ifstream file(argv[i + 1], std::ios::binary);
    if(!file)
      return 2;
    std::vector<uint8_t> data((std::istreambuf_iterator<char>(file)),
			      std::istreambuf_iterator<char>());
    models[i]->rbf_load(data.data(), data.size());
    if(!models[i]->opt_r_set(CV::JTAG_ID, 0))
      return 2;
  }
  bool lab = argc > 3 && std::string(argv[3]) == "LAB";
  if(argc > 3 && !lab && std::string(argv[3]) != "MLAB")
    return 2;
  int clocks = argc > 4 && std::string(argv[4]) == "3" ? 3 : 1;
  if(argc > 4 && std::string(argv[4]) != "1" && std::string(argv[4]) != "3")
    return 2;
  const CV::bmux_type_t inversion[] = {CV::CLK0_INV, CV::CLK1_INV, CV::CLK2_INV};
  CV::xycoords pos(lab ? 7 : 8, 32);
  for(int i = 0; i < clocks; ++i)
    if(!positive->bmux_b_set(lab ? CV::LAB : CV::MLAB, pos, inversion[i], 0, true))
      return 2;
  std::vector<uint8_t> got, expected;
  positive->rbf_save(got);
  negative->rbf_save(expected);
  if(got != expected) {
    std::fprintf(stderr, "FAIL: CLKx_INV does not reproduce the Quartus falling-edge bitstream\n");
    return 1;
  }
  std::puts("PASS: CLKx_INV reproduces the fixed-placement Quartus falling-edge configuration");
  return 0;
}
