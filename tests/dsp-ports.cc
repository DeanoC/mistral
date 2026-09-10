#include "cyclonev.h"

#include <cstdio>
#include <memory>

int main(int argc, char **argv)
{
  using CV = mistral::CycloneV;
  std::unique_ptr<CV> cv(CV::get_model(argc > 1 ? argv[1] : "5CSEBA6U23I7"));
  if(!cv)
    return 2;
  int failed = 0;
  int checked = 0;
  auto check = [&](CV::pnode_coords pn) {
    auto rn = cv->pnode_to_rnode(pn);
    auto back = (rn == 0xffffffff) ? CV::pnode_coords() : cv->rnode_to_pnode(rn);
    ++checked;
    if(rn == 0xffffffff || back != pn) {
      if(failed < 10)
	std::fprintf(stderr, "%s -> %s -> %s\n",
		     pn.to_string().c_str(),
		     cv->ri2rc(rn).to_string().c_str(),
		     back.to_string().c_str());
      ++failed;
    }
  };
  for(auto pos : cv->dsp_get_pos()) {
    for(int group = 0; group < 12; ++group)
      for(int bit = 0; bit < 9; ++bit)
	check(CV::pnode_coords(CV::DSP, pos, CV::DATAIN, group, bit));
    for(int bit = 0; bit < 74; ++bit)
      check(CV::pnode_coords(CV::DSP, pos, CV::RESULT, -1, bit));
  }
  std::printf("%zu DSP blocks, %d ports, %d failures\n",
	      cv->dsp_get_pos().size(), checked, failed);
  return failed ? 1 : 0;
}
