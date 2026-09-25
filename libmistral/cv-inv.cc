#include "cyclonev.h"

#include <algorithm>

int mistral::CycloneV::inv_get_default(const inverter_info &inf) const
{
  switch(inf.pos_and_def & inverter_info::DEF_MASK) {
  case inverter_info::DEF_0: return 0; break;
  case inverter_info::DEF_1: return 1; break;

  case inverter_info::DEF_GP: {
    pnode_coords pnode = rnode_to_pnode(inf.node);
    if(pnode.pt() != DATAOUT && pnode.pt() != OEIN)
      return 0;

    bool is_wired = pin_find_pos(pnode.p(), pnode.bi());
    if(pnode.pt() == DATAOUT || pnode.pi() == 0)
      return is_wired ? 0 : 1;
    return is_wired ? 1 : 0;
  }

  case inverter_info::DEF_HMC: {
    pnode_coords pnode = rnode_to_pnode(inf.node);
    pnode = hmc_get_bypass(pnode);
    if(!pnode)
      return 0;

    auto gpio = p2p_to(pnode);
    if(!gpio) {
      auto gpiol = p2p_from(pnode);
      for(pnode_coords gp : gpiol)
	if(gp.bt() == GPIO) {
	  gpio = gp;
	  break;
	}
    }
    if(!gpio)
      return 0;

    if(gpio.pt() != OEIN && gpio.pt() != DATAOUT)
      return 0;

    bool is_wired = pin_find_pos(gpio.p(), gpio.bi());
    if(gpio.pt() == DATAOUT || !gpio)
      return is_wired ? 0 : 1;
    return is_wired ? 1 : 0;
  }
  }

  return -1;
}

std::vector<mistral::CycloneV::inv_setting_t> mistral::CycloneV::inv_get() const
{
  std::vector<inv_setting_t> res;
  for(uint32_t i = 0; i != dhead->count_inv; i++) {
    const auto &inf = inverter_infos[i];
    uint32_t pos = inf.pos_and_def & ~inverter_info::DEF_MASK;
    bool value = (cram[pos >> 3] >> (pos & 7)) & 1;
    int def = inv_get_default(inf);
    res.emplace_back(inv_setting_t{inf.node, value, int(value) == def});
  }
  return res;
}

void mistral::CycloneV::inv_default_set()
{
  for(uint32_t i = 0; i != dhead->count_inv; i++) {
    const auto &inf = inverter_infos[i];
    uint32_t pos = inf.pos_and_def & ~inverter_info::DEF_MASK;
    bool def = inv_get_default(inf) == 1;

    if(def)
      cram[pos >> 3] |= 1 << (pos & 7);
    else
      cram[pos >> 3] &= ~(1 << (pos & 7));
  }
}

void mistral::CycloneV::build_inverter_index()
{
  inverter_order.resize(dhead->count_inv);
  for(uint32_t i = 0; i != dhead->count_inv; i++)
    inverter_order[i] = i;
  // The baked table is ordered by pre-index coordinates, not by rnode_index.
  // stable_sort keeps the original entry first when two rows share a node,
  // which is the row inv_set's linear walk returns.
  std::stable_sort(inverter_order.begin(), inverter_order.end(),
		   [this](uint32_t a, uint32_t b) {
		     return inverter_infos[a].node < inverter_infos[b].node;
		   });
}

const mistral::CycloneV::inverter_info *mistral::CycloneV::inverter_find(rnode_index node) const
{
  auto it = std::lower_bound(inverter_order.begin(), inverter_order.end(), node,
			      [this](uint32_t index, rnode_index key) {
				return inverter_infos[index].node < key;
			      });
  if(it == inverter_order.end() || inverter_infos[*it].node != node)
    return nullptr;
  return &inverter_infos[*it];
}

bool mistral::CycloneV::rnode_inverter_cram_bit(rnode_coords rn, std::vector<std::pair<uint32_t, uint32_t>> &bits) const
{
  bits.clear();
  const rnode_object *r = rc2ro(rn);
  if(!r)
    return false;

  // At most one entry per node; routing-inverter-cram asserts that. The
  // index returns the earliest table entry, which is the entry inv_set writes.
  // pos = pos_and_def & ~DEF_MASK, then (x, y) = (pos % cram_sx, pos / cram_sx).
  if(const inverter_info *inf = inverter_find(r->ri())) {
    uint32_t pos = inf->pos_and_def & ~inverter_info::DEF_MASK;
    bits.emplace_back(pos % di.cram_sx, pos / di.cram_sx);
  }
  return true;
}

bool mistral::CycloneV::inv_set(rnode_index node, bool value)
{
  for(uint32_t i = 0; i != dhead->count_inv; i++) {
    const auto &inf = inverter_infos[i];
    if(inf.node == node) {
      uint32_t pos = inf.pos_and_def & ~inverter_info::DEF_MASK;
      if(value)
	cram[pos >> 3] |= 1 << (pos & 7);
      else
	cram[pos >> 3] &= ~(1 << (pos & 7));
      return true;
    }
  }
  return false;
}

mistral::CycloneV::invert_t mistral::CycloneV::rnode_is_inverting(rnode_index ri) const
{
  const rnode_object *ro = ri2ro(ri);
  rnode_coords rn = ro->rc();
  if(rn.t() == WM)
    return INV_NO;

  for(uint32_t i = 0; i != dhead->count_inv; i++) {
    const auto &inf = inverter_infos[i];
    if(inf.node == ri)
      return INV_PROGRAMMABLE;
  }

  if(ro->driver(0) == 0xff)
    return INV_UNKNOWN;

  return dn_info[dn_lookup->index_si[SI_TT][T_85]].drivers[ro->driver(0)].invert ? INV_YES : INV_NO;
}

