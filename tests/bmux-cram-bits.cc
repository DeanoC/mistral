#include "cyclonev.h"

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <map>
#include <memory>
#include <string>
#include <unistd.h>
#include <utility>
#include <vector>

namespace {

using CV = mistral::CycloneV;
using Bits = std::vector<std::pair<uint32_t, uint32_t>>;

// Above the last generated bmux_type_t. Selects and numeric values used by
// LAB/MLAB sit well under this; a miss shows up as an oracle mismatch.
const int kTypeLimit = 4096;
const int kNumLimit = 4096;

std::string capture_diff(CV &changed, const CV &baseline)
{
  char path[] = "/tmp/mistral-bmux-diff-XXXXXX";
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

void normalize(Bits &bits)
{
  std::sort(bits.begin(), bits.end());
  bits.erase(std::unique(bits.begin(), bits.end()), bits.end());
}

const char *mux_name(CV::bmux_type_t mux)
{
  const char *name = CV::bmux_type_names[mux];
  return name ? name : "?";
}

} // namespace

int main()
{
  static_assert(MISTRAL_BMUX_CRAM_BITS == 1, "LAB/MLAB block-mux CRAM query");

  std::unique_ptr<CV> model(CV::get_model("5CSEBA6U23I7"));
  std::unique_ptr<CV> changed(CV::get_model("5CSEBA6U23I7"));
  if(!model || !changed)
    return 2;
  if(model->lab_get_pos().empty() || model->mlab_get_pos().empty() || model->m10k_get_pos().empty() || model->dsp_get_pos().empty())
    return 2;

  int failures = 0;
  auto check = [&](bool value, const char *message) {
    if(!value) {
      std::fprintf(stderr, "FAIL: %s\n", message);
      ++failures;
    }
  };

  const CV::xycoords lab = model->lab_get_pos().front();
  const CV::xycoords lab_tail = model->lab_get_pos().back();
  const CV::xycoords mlab = model->mlab_get_pos().front();
  const CV::xycoords m10k = model->m10k_get_pos().front();
  const CV::xycoords dsp = model->dsp_get_pos().front();
  const CV::xycoords empty(0, 0);

  Bits scratch{{1, 2}};
  check(!model->bmux_cram_bits(CV::LAB, empty, CV::CLK0_INV, 0, scratch), "empty tile rejects");
  check(scratch.empty(), "empty tile clears");
  scratch.emplace_back(1, 2);
  check(!model->bmux_cram_bits(CV::MLAB, lab, CV::CLK0_INV, 0, scratch), "LAB tile is not an MLAB");
  check(scratch.empty(), "wrong block type clears");
  scratch.emplace_back(1, 2);
  check(!model->bmux_cram_bits(CV::LAB, mlab, CV::CLK0_INV, 0, scratch), "MLAB tile is not a LAB");
  check(scratch.empty(), "MLAB-as-LAB clears");
  scratch.emplace_back(1, 2);
  check(!model->bmux_cram_bits(CV::M10K, m10k, CV::TRUE_DUAL_PORT, 0, scratch), "M10K is outside this slice");
  check(scratch.empty(), "M10K clears");
  scratch.emplace_back(1, 2);
  check(!model->bmux_cram_bits(CV::DSP, dsp, CV::CLK0_INV, 0, scratch), "DSP is outside this slice");
  check(scratch.empty(), "DSP clears");
  scratch.emplace_back(1, 2);
  check(!model->bmux_cram_bits(CV::LAB, lab, CV::BMNONE, 0, scratch), "BMNONE is not a LAB mux");
  check(scratch.empty(), "BMNONE clears");
  scratch.emplace_back(1, 2);
  check(!model->bmux_cram_bits(CV::LAB, lab, CV::WRITE_EN, 0, scratch), "MLAB-only mux is not on a LAB");
  check(scratch.empty(), "MLAB-only mux clears");
  scratch.emplace_back(1, 2);
  check(!model->bmux_cram_bits(CV::LAB, lab, CV::CLK0_INV, 1, scratch), "span-1 mux rejects midx 1");
  check(scratch.empty(), "bad global midx clears");
  scratch.emplace_back(1, 2);
  check(!model->bmux_cram_bits(CV::LAB, lab, CV::LUT_MASK, -1, scratch), "negative midx rejects");
  check(scratch.empty(), "negative midx clears");
  scratch.emplace_back(1, 2);
  check(!model->bmux_cram_bits(CV::LAB, lab, CV::LUT_MASK, 10, scratch), "per-ALM midx 10 rejects");
  check(scratch.empty(), "past-the-end midx clears");

  check(model->bmux_cram_bits(CV::LAB, lab, CV::CLK0_INV, 0, scratch), "CLK0_INV exists");
  check(scratch.size() == 1, "CLK0_INV is one bit");
  check(model->bmux_cram_bits(CV::LAB, lab, CV::LUT_MASK, 0, scratch), "LUT_MASK midx 0 exists");
  check(scratch.size() == 64, "LUT_MASK is 64 bits");
  Bits lut0 = scratch;
  check(model->bmux_cram_bits(CV::LAB, lab, CV::LUT_MASK, 9, scratch), "LUT_MASK midx 9 exists");
  check(scratch.size() == 64, "LUT_MASK midx 9 is 64 bits");
  normalize(lut0);
  normalize(scratch);
  check(lut0 != scratch, "two ALMs do not share a LUT_MASK coordinate");
  Bits lut9 = scratch;
  std::vector<std::pair<uint32_t, uint32_t>> both = lut0;
  both.insert(both.end(), lut9.begin(), lut9.end());
  normalize(both);
  check(both.size() == lut0.size() + lut9.size(), "LUT_MASK ALMs are disjoint");

  check(model->bmux_cram_bits(CV::LAB, lab, CV::TCLK_SEL, 0, scratch), "TCLK_SEL exists");
  check(scratch.size() == 3, "TCLK_SEL is 3 bits");
  check(model->bmux_cram_bits(CV::MLAB, mlab, CV::WRITE_PULSE_LENGTH, 0, scratch), "write pulse exists");
  check(scratch.size() == 2, "write pulse is 2 bits");

  Bits write_en, mcrg;
  check(model->bmux_cram_bits(CV::MLAB, mlab, CV::WRITE_EN, 0, write_en), "WRITE_EN exists");
  check(model->bmux_cram_bits(CV::MLAB, mlab, CV::MCRG_VOLTAGE, 0, mcrg), "MCRG_VOLTAGE exists");
  normalize(write_en);
  normalize(mcrg);
  check(write_en.size() == 1 && write_en == mcrg, "WRITE_EN and MCRG_VOLTAGE share one CRAM bit");

  std::string initial = capture_diff(*changed, *model);
  check(initial.empty(), "cleared models start identical");

  struct Item {
    CV::bmux_type_t mux;
    int midx;
    int stype;
  };
  std::map<int, std::vector<int>> mux_selects;
  std::map<int, std::vector<uint32_t>> num_values;

  auto discover = [&](CV::block_type_t btype, CV::xycoords pos, CV::bmux_type_t mux, int stype) {
    if(stype == CV::MT_MUX && !mux_selects.count(mux)) {
      CV::bmux_setting_t cur;
      if(!model->bmux_get(btype, pos, mux, 0, cur))
        return;
      std::vector<int> sels;
      for(int id = 0; id < kTypeLimit; ++id) {
        CV::bmux_type_t sel = static_cast<CV::bmux_type_t>(id);
        if(!changed->bmux_m_set(btype, pos, mux, 0, sel))
          continue;
        sels.push_back(id);
        if(!changed->bmux_m_set(btype, pos, mux, 0, static_cast<CV::bmux_type_t>(cur.s)))
          check(false, "mux discovery restore");
      }
      mux_selects[mux] = sels;
    } else if(stype == CV::MT_NUM && !num_values.count(mux)) {
      CV::bmux_setting_t cur;
      if(!model->bmux_get(btype, pos, mux, 0, cur))
        return;
      std::vector<uint32_t> vals;
      for(uint32_t n = 0; n < static_cast<uint32_t>(kNumLimit); ++n) {
        if(!changed->bmux_n_set(btype, pos, mux, 0, n))
          continue;
        vals.push_back(n);
        if(!changed->bmux_n_set(btype, pos, mux, 0, cur.s))
          check(false, "num discovery restore");
      }
      num_values[mux] = vals;
    }
  };

  auto written_by = [&](CV &device, Bits &out) -> bool {
    std::string other;
    std::string text = capture_diff(device, *model);
    if(!parse_cram_diff(text, out, other)) {
      std::fprintf(stderr, "FAIL: non-cram diff line \"%s\"\n", other.c_str());
      ++failures;
      return false;
    }
    normalize(out);
    return true;
  };

  auto oracle_block = [&](CV::block_type_t btype, CV::xycoords pos, const char *label) {
    mux_selects.clear();
    num_values.clear();
    std::vector<Item> items;
    for(int midx = 0; midx < 16; ++midx) {
      for(int id = 0; id < kTypeLimit; ++id) {
        CV::bmux_type_t mux = static_cast<CV::bmux_type_t>(id);
        int stype = model->bmux_type(btype, pos, mux, midx);
        if(stype < 0)
          continue;
        items.push_back(Item{mux, midx, stype});
        if(midx == 0)
          discover(btype, pos, mux, stype);
      }
    }
    check(!items.empty(), "block has muxes");
    int checked = 0;
    for(const Item &item : items) {
      Bits query;
      if(!model->bmux_cram_bits(btype, pos, item.mux, item.midx, query)) {
        std::fprintf(stderr, "FAIL: %s %s midx %d query rejected\n", label, mux_name(item.mux), item.midx);
        ++failures;
        continue;
      }
      normalize(query);
      if(query.empty()) {
        std::fprintf(stderr, "FAIL: %s %s midx %d has no bits\n", label, mux_name(item.mux), item.midx);
        ++failures;
        continue;
      }
      CV::bmux_setting_t cur;
      if(!model->bmux_get(btype, pos, item.mux, item.midx, cur)) {
        std::fprintf(stderr, "FAIL: %s %s midx %d get\n", label, mux_name(item.mux), item.midx);
        ++failures;
        continue;
      }

      Bits written;
      bool ok = false;
      if(item.stype == CV::MT_BOOL) {
        if(!changed->bmux_b_set(btype, pos, item.mux, item.midx, cur.s == 0)) {
          check(false, "bool set");
        } else if(written_by(*changed, written)) {
          ok = written == query;
          Bits during;
          check(changed->bmux_cram_bits(btype, pos, item.mux, item.midx, during), "query while CRAM is dirty");
          normalize(during);
          check(during == query, "query does not read CRAM");
        }
        if(!changed->bmux_b_set(btype, pos, item.mux, item.midx, cur.s != 0))
          check(false, "bool restore");
      } else if(item.stype == CV::MT_RAM) {
        std::vector<uint8_t> flipped = cur.r;
        for(uint32_t b = 0; b < cur.s; ++b)
          flipped[b >> 3] = static_cast<uint8_t>(flipped[b >> 3] ^ (1u << (b & 7)));
        if(!changed->bmux_r_set(btype, pos, item.mux, item.midx, flipped)) {
          check(false, "ram set");
        } else if(written_by(*changed, written)) {
          ok = written == query;
        }
        if(!changed->bmux_r_set(btype, pos, item.mux, item.midx, cur.r))
          check(false, "ram restore");
      } else if(item.stype == CV::MT_MUX) {
        Bits accum;
        const std::vector<int> &sels = mux_selects[item.mux];
        if(sels.empty())
          check(false, "mux has no legal select");
        for(int sel : sels) {
          if(!changed->bmux_m_set(btype, pos, item.mux, item.midx, static_cast<CV::bmux_type_t>(sel))) {
            check(false, "mux set discovered select");
            continue;
          }
          Bits one;
          if(written_by(*changed, one))
            accum.insert(accum.end(), one.begin(), one.end());
          if(!changed->bmux_m_set(btype, pos, item.mux, item.midx, static_cast<CV::bmux_type_t>(cur.s)))
            check(false, "mux restore");
        }
        normalize(accum);
        written.swap(accum);
        ok = written == query;
      } else if(item.stype == CV::MT_NUM) {
        Bits accum;
        const std::vector<uint32_t> &vals = num_values[item.mux];
        if(vals.empty())
          check(false, "num has no legal value");
        for(uint32_t n : vals) {
          if(!changed->bmux_n_set(btype, pos, item.mux, item.midx, n)) {
            check(false, "num set discovered value");
            continue;
          }
          Bits one;
          if(written_by(*changed, one))
            accum.insert(accum.end(), one.begin(), one.end());
          if(!changed->bmux_n_set(btype, pos, item.mux, item.midx, cur.s))
            check(false, "num restore");
        }
        normalize(accum);
        written.swap(accum);
        ok = written == query;
      } else {
        check(false, "unknown mux stype");
      }

      if(!ok) {
        std::fprintf(stderr, "FAIL: %s %s midx %d query=%zu written=%zu\n",
                     label, mux_name(item.mux), item.midx, query.size(), written.size());
        ++failures;
      }
      ++checked;

      Bits again;
      check(changed->bmux_cram_bits(btype, pos, item.mux, item.midx, again), "query after a set still resolves");
      normalize(again);
      check(again == query, "query does not depend on CRAM state");
    }
    std::printf("%s instances=%zu oracle=%d\n", label, items.size(), checked);
  };

  oracle_block(CV::LAB, lab, "LAB");
  oracle_block(CV::MLAB, mlab, "MLAB");

  // A second LAB column, one field only, so the x_to_bx base is not a single site.
  {
    Bits query, written;
    check(model->bmux_cram_bits(CV::LAB, lab_tail, CV::CLK0_INV, 0, query), "tail LAB CLK0_INV");
    normalize(query);
    CV::bmux_setting_t cur;
    check(model->bmux_get(CV::LAB, lab_tail, CV::CLK0_INV, 0, cur), "tail LAB get");
    check(changed->bmux_b_set(CV::LAB, lab_tail, CV::CLK0_INV, 0, cur.s == 0), "tail LAB set");
    check(written_by(*changed, written) && written == query, "tail LAB oracle");
    check(changed->bmux_b_set(CV::LAB, lab_tail, CV::CLK0_INV, 0, cur.s != 0), "tail LAB restore");
    check(lab_tail != lab, "tail LAB is a different tile");
  }

  std::string restored = capture_diff(*changed, *model);
  check(restored.empty(), "CRAM matches the cleared model after the oracle");

  std::printf("LAB/MLAB block-mux CRAM: %s\n", failures ? "FAIL" : "PASS");
  return failures ? 1 : 0;
}
