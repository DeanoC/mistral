#include "cyclonev.h"

#include <cstdio>
#include <fstream>
#include <iterator>
#include <memory>
#include <string>
#include <vector>

// Multi-bit boolean fields are redundant bit groups that Quartus writes
// together.  Check that true sets every bit of each such field on this die,
// then compare against a Quartus fast/slow slew-rate pair when given.

namespace {

using CV = mistral::CycloneV;

int count_diff_lines(CV &changed, const CV &baseline)
{
  FILE *fp = std::tmpfile();
  if(!fp)
    return -1;
  changed.diff(&baseline, fp);
  std::fflush(fp);
  std::rewind(fp);
  int lines = 0;
  for(int c = std::fgetc(fp); c != EOF; c = std::fgetc(fp))
    if(c == '\n')
      ++lines;
  std::fclose(fp);
  return lines;
}

bool load(CV &cv, const char *path)
{
  std::ifstream file(path, std::ios::binary);
  if(!file)
    return false;
  std::vector<uint8_t> data((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
  cv.rbf_load(data.data(), data.size());
  return cv.opt_r_set(CV::JTAG_ID, 0);
}

}

int main(int argc, char **argv)
{
  static_assert(MISTRAL_MULTIBIT_BOOL_SET == 1, "multi-bit boolean setter");
  if(argc != 1 && argc != 3) {
    std::fprintf(stderr, "usage: multibit-bool [slew-fast.rbf slew-slow.rbf]\n");
    return 2;
  }

  std::unique_ptr<CV> baseline(CV::get_model("5CSEBA6U23I7"));
  std::unique_ptr<CV> changed(CV::get_model("5CSEBA6U23I7"));
  if(!baseline || !changed || baseline->m10k_get_pos().empty() || baseline->dsp_get_pos().empty())
    return 2;
  const CV::pin_info_t *pin = baseline->pin_find_name("AD26");
  if(!pin)
    return 2;
  const CV::xycoords gpio(pin->pad & 0x3fff);
  const int pad = pin->pad >> 14;

  struct Case { const char *name; CV::block_type_t block; CV::xycoords pos; CV::bmux_type_t mux; int idx; int bits; };
  const Case cases[] = {
    {"GPIO SLEW_RATE_SLOW", CV::GPIO, gpio, CV::SLEW_RATE_SLOW, pad, 2},
    {"M10K PR_EN", CV::M10K, baseline->m10k_get_pos().front(), CV::PR_EN, 0, 2},
    {"DSP PARTIAL_RECONFIG_EN", CV::DSP, baseline->dsp_get_pos().front(), CV::PARTIAL_RECONFIG_EN, 0, 3},
  };

  int failures = 0;
  for(const Case &c : cases) {
    changed->clear();
    baseline->clear();
    CV::bmux_setting_t s;
    if(!changed->bmux_b_set(c.block, c.pos, c.mux, c.idx, true) ||
       !changed->bmux_get(c.block, c.pos, c.mux, c.idx, s) || !s.s) {
      std::fprintf(stderr, "FAIL: %s cannot be set\n", c.name);
      ++failures;
      continue;
    }
    int got = count_diff_lines(*changed, *baseline);
    if(got != c.bits) {
      std::fprintf(stderr, "FAIL: %s true changed %d bits, expected %d\n", c.name, got, c.bits);
      ++failures;
    }
    changed->bmux_b_set(c.block, c.pos, c.mux, c.idx, false);
    if(count_diff_lines(*changed, *baseline) != 0) {
      std::fprintf(stderr, "FAIL: %s false does not restore the default\n", c.name);
      ++failures;
    }
  }

  if(argc == 3) {
    std::unique_ptr<CV> fast(CV::get_model("5CSEBA6U23I7"));
    std::unique_ptr<CV> slow(CV::get_model("5CSEBA6U23I7"));
    if(!load(*fast, argv[1]) || !load(*slow, argv[2]))
      return 2;
    if(!fast->bmux_b_set(CV::GPIO, gpio, CV::SLEW_RATE_SLOW, pad, true))
      return 2;
    std::vector<uint8_t> got, expected;
    fast->rbf_save(got);
    slow->rbf_save(expected);
    if(got != expected) {
      std::fprintf(stderr, "FAIL: SLEW_RATE_SLOW does not reproduce the Quartus slow-slew bitstream\n");
      ++failures;
    }
  }

  if(failures)
    return 1;
  std::puts(argc == 3 ? "PASS: multi-bit booleans set every bit; SLEW_RATE_SLOW reproduces the Quartus slow-slew RBF"
		      : "PASS: multi-bit booleans set every bit");
  return 0;
}
