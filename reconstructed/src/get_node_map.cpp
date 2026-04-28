// get_node_map.cpp — GetNodeMap<AwgDeviceType>::get() specializations
// Auto-extracted from _seqc_compiler.so binary data tables.
//
// 8 device-specific node maps totalling 1081 entries.
// UHFLI @0x1948d0, HDAWG @0x1ad9a0, UHFQA @0x1b1470, SHFQA @0x1ba3d0,
// SHFSG @0x1bae40, SHFLI @0x1bbcb0, GHFLI @0x1bc030, VHFLI @0x1bc3b0.
// Dispatcher @0x1ba360, initNodeMap @0x16b740.

#include "zhinst/custom_functions.hpp"
#include "zhinst/node_map_data.hpp"
#include "zhinst/types.hpp"

#include <map>
#include <memory>
#include <string>
#include <vector>

namespace zhinst {

// ============================================================================
// GetNodeMap — template class with 8 explicit specializations
// ============================================================================

template <AwgDeviceType T>
class GetNodeMap {
public:
    static std::unique_ptr<NodeMap> get();
};

// ---------------------------------------------------------------------------
// Helpers to reduce per-entry boilerplate
// ---------------------------------------------------------------------------
namespace {

using Map = std::map<std::string, NodeMapItem>;

void addDirect(Map& m, const char* key, uint32_t addr, int32_t typeIdx) {
    auto* data = new DirectAddrNodeMapData;
    data->addr_ = addr;
    NodeMapItem item{};
    item.data = data;
    item.typeIdx = typeIdx;
    m[key] = item;
}

void addVirt(Map& m, const char* key, const char* name,
             std::vector<int32_t> addresses, int32_t typeIdx) {
    auto* data = new VirtAddrNodeMapData;
    data->name_.assign(name);
    data->addresses_ = std::move(addresses);
    NodeMapItem item{};
    item.data = data;
    item.typeIdx = typeIdx;
    m[key] = item;
}

} // anonymous namespace

// ---------------------------------------------------------------------------
// UHFLI — 566 entries @0x1948d0
// ---------------------------------------------------------------------------
template <>
std::unique_ptr<NodeMap> GetNodeMap<AwgDeviceType::UHFLI>::get() {  // @0x1948d0
    auto nm = std::make_unique<NodeMap>();
    auto& m = nm->entries_;

    addDirect(m, "_/dios/0/output", 0x1c0c, 0);
    addDirect(m, "aucarts/0/enable", 0xe804, 0);
    addDirect(m, "aucarts/0/mode", 0xe800, 0);
    addDirect(m, "aucarts/0/ops/0/coeff", 0xe818, 0);
    addDirect(m, "aucarts/0/ops/0/demodselect", 0xe810, 0);
    addDirect(m, "aucarts/0/ops/0/scale", 0xe81c, 2);
    addDirect(m, "aucarts/0/ops/0/value", 0xe814, 0);
    addDirect(m, "aucarts/0/ops/1/coeff", 0xe828, 0);
    addDirect(m, "aucarts/0/ops/1/demodselect", 0xe820, 0);
    addDirect(m, "aucarts/0/ops/1/scale", 0xe82c, 2);
    addDirect(m, "aucarts/0/ops/1/value", 0xe824, 0);
    addDirect(m, "aucarts/0/rate", 0xe808, 2);
    addDirect(m, "aucarts/1/enable", 0xe844, 0);
    addDirect(m, "aucarts/1/mode", 0xe840, 0);
    addDirect(m, "aucarts/1/ops/0/coeff", 0xe858, 0);
    addDirect(m, "aucarts/1/ops/0/demodselect", 0xe850, 0);
    addDirect(m, "aucarts/1/ops/0/scale", 0xe85c, 2);
    addDirect(m, "aucarts/1/ops/0/value", 0xe854, 0);
    addDirect(m, "aucarts/1/ops/1/coeff", 0xe868, 0);
    addDirect(m, "aucarts/1/ops/1/demodselect", 0xe860, 0);
    addDirect(m, "aucarts/1/ops/1/scale", 0xe86c, 2);
    addDirect(m, "aucarts/1/ops/1/value", 0xe864, 0);
    addDirect(m, "aucarts/1/rate", 0xe848, 2);
    addDirect(m, "aupolars/0/enable", 0xea04, 0);
    addDirect(m, "aupolars/0/flags", 0xea34, 0);
    addDirect(m, "aupolars/0/mode", 0xea00, 0);
    addDirect(m, "aupolars/0/ops/0/coeff", 0xea18, 0);
    addDirect(m, "aupolars/0/ops/0/demodselect", 0xea10, 0);
    addDirect(m, "aupolars/0/ops/0/scale", 0xea1c, 2);
    addDirect(m, "aupolars/0/ops/0/value", 0xea14, 0);
    addDirect(m, "aupolars/0/ops/1/coeff", 0xea28, 0);
    addDirect(m, "aupolars/0/ops/1/demodselect", 0xea20, 0);
    addDirect(m, "aupolars/0/ops/1/scale", 0xea2c, 2);
    addDirect(m, "aupolars/0/ops/1/value", 0xea24, 0);
    addDirect(m, "aupolars/0/rate", 0xea08, 2);
    addDirect(m, "aupolars/0/reserved1", 0xea0c, 0);
    addDirect(m, "aupolars/1/enable", 0xea44, 0);
    addDirect(m, "aupolars/1/flags", 0xea74, 0);
    addDirect(m, "aupolars/1/mode", 0xea40, 0);
    addDirect(m, "aupolars/1/ops/0/coeff", 0xea58, 0);
    addDirect(m, "aupolars/1/ops/0/demodselect", 0xea50, 0);
    addDirect(m, "aupolars/1/ops/0/scale", 0xea5c, 2);
    addDirect(m, "aupolars/1/ops/0/value", 0xea54, 0);
    addDirect(m, "aupolars/1/ops/1/coeff", 0xea68, 0);
    addDirect(m, "aupolars/1/ops/1/demodselect", 0xea60, 0);
    addDirect(m, "aupolars/1/ops/1/scale", 0xea6c, 2);
    addDirect(m, "aupolars/1/ops/1/value", 0xea64, 0);
    addDirect(m, "aupolars/1/rate", 0xea48, 2);
    addDirect(m, "aupolars/1/reserved1", 0xea4c, 0);
    addDirect(m, "auxouts/0/demodselect", 0xd738, 0);
    addDirect(m, "auxouts/0/limitlower", 0xd720, 2);
    addDirect(m, "auxouts/0/limitupper", 0xd724, 2);
    addDirect(m, "auxouts/0/offset", 0xd708, 2);
    addDirect(m, "auxouts/0/outputselect", 0xd734, 0);
    addDirect(m, "auxouts/0/preoffset", 0xd700, 3);
    addDirect(m, "auxouts/0/scale", 0xd710, 2);
    addDirect(m, "auxouts/1/demodselect", 0xd778, 0);
    addDirect(m, "auxouts/1/limitlower", 0xd760, 2);
    addDirect(m, "auxouts/1/limitupper", 0xd764, 2);
    addDirect(m, "auxouts/1/offset", 0xd748, 2);
    addDirect(m, "auxouts/1/outputselect", 0xd774, 0);
    addDirect(m, "auxouts/1/preoffset", 0xd740, 3);
    addDirect(m, "auxouts/1/scale", 0xd750, 2);
    addDirect(m, "auxouts/2/demodselect", 0xd7b8, 0);
    addDirect(m, "auxouts/2/limitlower", 0xd7a0, 2);
    addDirect(m, "auxouts/2/limitupper", 0xd7a4, 2);
    addDirect(m, "auxouts/2/offset", 0xd788, 2);
    addDirect(m, "auxouts/2/outputselect", 0xd7b4, 0);
    addDirect(m, "auxouts/2/preoffset", 0xd780, 3);
    addDirect(m, "auxouts/2/scale", 0xd790, 2);
    addDirect(m, "auxouts/3/demodselect", 0xd7f8, 0);
    addDirect(m, "auxouts/3/limitlower", 0xd7e0, 2);
    addDirect(m, "auxouts/3/limitupper", 0xd7e4, 2);
    addDirect(m, "auxouts/3/offset", 0xd7c8, 2);
    addDirect(m, "auxouts/3/outputselect", 0xd7f4, 0);
    addDirect(m, "auxouts/3/preoffset", 0xd7c0, 3);
    addDirect(m, "auxouts/3/scale", 0xd7d0, 2);
    addDirect(m, "awgs/0/auxtriggers/0/channel", 0x5300, 0);
    addDirect(m, "awgs/0/auxtriggers/0/falling", 0x5308, 0);
    addDirect(m, "awgs/0/auxtriggers/0/rising", 0x5304, 0);
    addDirect(m, "awgs/0/auxtriggers/0/state", 0x530c, 0);
    addDirect(m, "awgs/0/auxtriggers/1/channel", 0x5310, 0);
    addDirect(m, "awgs/0/auxtriggers/1/falling", 0x5318, 0);
    addDirect(m, "awgs/0/auxtriggers/1/rising", 0x5314, 0);
    addDirect(m, "awgs/0/auxtriggers/1/state", 0x531c, 0);
    addDirect(m, "awgs/0/enable", 0x5000, 0);
    addDirect(m, "awgs/0/outputs/0/amplitude", 0x5500, 2);
    addDirect(m, "awgs/0/outputs/0/mode", 0x5504, 0);
    addDirect(m, "awgs/0/outputs/1/amplitude", 0x5540, 2);
    addDirect(m, "awgs/0/outputs/1/mode", 0x5544, 0);
    addDirect(m, "awgs/0/single", 0x5004, 0);
    addDirect(m, "awgs/0/time", 0x5008, 0);
    addDirect(m, "awgs/0/triggers/0/channel", 0x5218, 0);
    addDirect(m, "awgs/0/triggers/0/falling", 0x5214, 0);
    addDirect(m, "awgs/0/triggers/0/force", 0x521c, 0);
    addDirect(m, "awgs/0/triggers/0/gate/enable", 0x5224, 0);
    addDirect(m, "awgs/0/triggers/0/gate/inputselect", 0x5228, 0);
    addDirect(m, "awgs/0/triggers/0/hysteresis/absolute", 0x5204, 2);
    addDirect(m, "awgs/0/triggers/0/hysteresis/mode", 0x520c, 0);
    addDirect(m, "awgs/0/triggers/0/hysteresis/relative", 0x5208, 2);
    addDirect(m, "awgs/0/triggers/0/level", 0x5200, 2);
    addDirect(m, "awgs/0/triggers/0/rising", 0x5210, 0);
    addDirect(m, "awgs/0/triggers/0/state", 0x5220, 0);
    addDirect(m, "awgs/0/triggers/1/channel", 0x5258, 0);
    addDirect(m, "awgs/0/triggers/1/falling", 0x5254, 0);
    addDirect(m, "awgs/0/triggers/1/force", 0x525c, 0);
    addDirect(m, "awgs/0/triggers/1/gate/enable", 0x5264, 0);
    addDirect(m, "awgs/0/triggers/1/gate/inputselect", 0x5268, 0);
    addDirect(m, "awgs/0/triggers/1/hysteresis/absolute", 0x5244, 2);
    addDirect(m, "awgs/0/triggers/1/hysteresis/mode", 0x524c, 0);
    addDirect(m, "awgs/0/triggers/1/hysteresis/relative", 0x5248, 2);
    addDirect(m, "awgs/0/triggers/1/level", 0x5240, 2);
    addDirect(m, "awgs/0/triggers/1/rising", 0x5250, 0);
    addDirect(m, "awgs/0/triggers/1/state", 0x5260, 0);
    addDirect(m, "awgs/0/userregs/0", 0x5060, 0);
    addDirect(m, "awgs/0/userregs/1", 0x5064, 0);
    addDirect(m, "awgs/0/userregs/10", 0x5088, 0);
    addDirect(m, "awgs/0/userregs/11", 0x508c, 0);
    addDirect(m, "awgs/0/userregs/12", 0x5090, 0);
    addDirect(m, "awgs/0/userregs/13", 0x5094, 0);
    addDirect(m, "awgs/0/userregs/14", 0x5098, 0);
    addDirect(m, "awgs/0/userregs/15", 0x509c, 0);
    addDirect(m, "awgs/0/userregs/2", 0x5068, 0);
    addDirect(m, "awgs/0/userregs/3", 0x506c, 0);
    addDirect(m, "awgs/0/userregs/4", 0x5070, 0);
    addDirect(m, "awgs/0/userregs/5", 0x5074, 0);
    addDirect(m, "awgs/0/userregs/6", 0x5078, 0);
    addDirect(m, "awgs/0/userregs/7", 0x507c, 0);
    addDirect(m, "awgs/0/userregs/8", 0x5080, 0);
    addDirect(m, "awgs/0/userregs/9", 0x5084, 0);
    addDirect(m, "boxcars/0/baseline/enable", 0x4230, 0);
    addDirect(m, "boxcars/0/baseline/windowstart", 0x4234, 2);
    addDirect(m, "boxcars/0/enable", 0x4200, 0);
    addDirect(m, "boxcars/0/insel", 0x4218, 0);
    addDirect(m, "boxcars/0/limitrate", 0x422c, 2);
    addDirect(m, "boxcars/0/oscsel", 0x421c, 0);
    addDirect(m, "boxcars/0/periods", 0x4204, 0);
    addDirect(m, "boxcars/0/windowsize", 0x423c, 2);
    addDirect(m, "boxcars/0/windowstart", 0x4208, 2);
    addDirect(m, "boxcars/1/baseline/enable", 0x42b0, 0);
    addDirect(m, "boxcars/1/baseline/windowstart", 0x42b4, 2);
    addDirect(m, "boxcars/1/enable", 0x4280, 0);
    addDirect(m, "boxcars/1/insel", 0x4298, 0);
    addDirect(m, "boxcars/1/limitrate", 0x42ac, 2);
    addDirect(m, "boxcars/1/oscsel", 0x429c, 0);
    addDirect(m, "boxcars/1/periods", 0x4284, 0);
    addDirect(m, "boxcars/1/windowsize", 0x42bc, 2);
    addDirect(m, "boxcars/1/windowstart", 0x4288, 2);
    addDirect(m, "demods/0/adcselect", 0x1500, 0);
    addDirect(m, "demods/0/bypass", 0x1520, 0);
    addDirect(m, "demods/0/enable", 0x1528, 0);
    addDirect(m, "demods/0/harmonic", 0x1514, 0);
    addDirect(m, "demods/0/order", 0x1504, 0);
    addDirect(m, "demods/0/oscselect", 0x1510, 0);
    addDirect(m, "demods/0/phaseshift", 0x1518, 0);
    addDirect(m, "demods/0/rate", 0x150c, 2);
    addDirect(m, "demods/0/sinc", 0x151c, 0);
    addDirect(m, "demods/0/timeconstant", 0x1524, 2);
    addDirect(m, "demods/1/adcselect", 0x1580, 0);
    addDirect(m, "demods/1/bypass", 0x15a0, 0);
    addDirect(m, "demods/1/enable", 0x15a8, 0);
    addDirect(m, "demods/1/harmonic", 0x1594, 0);
    addDirect(m, "demods/1/order", 0x1584, 0);
    addDirect(m, "demods/1/oscselect", 0x1590, 0);
    addDirect(m, "demods/1/phaseshift", 0x1598, 0);
    addDirect(m, "demods/1/rate", 0x158c, 2);
    addDirect(m, "demods/1/sinc", 0x159c, 0);
    addDirect(m, "demods/1/timeconstant", 0x15a4, 2);
    addDirect(m, "demods/2/adcselect", 0x1600, 0);
    addDirect(m, "demods/2/bypass", 0x1620, 0);
    addDirect(m, "demods/2/enable", 0x1628, 0);
    addDirect(m, "demods/2/harmonic", 0x1614, 0);
    addDirect(m, "demods/2/order", 0x1604, 0);
    addDirect(m, "demods/2/oscselect", 0x1610, 0);
    addDirect(m, "demods/2/phaseshift", 0x1618, 0);
    addDirect(m, "demods/2/rate", 0x160c, 2);
    addDirect(m, "demods/2/sinc", 0x161c, 0);
    addDirect(m, "demods/2/timeconstant", 0x1624, 2);
    addDirect(m, "demods/3/adcselect", 0x1680, 0);
    addDirect(m, "demods/3/bypass", 0x16a0, 0);
    addDirect(m, "demods/3/enable", 0x16a8, 0);
    addDirect(m, "demods/3/harmonic", 0x1694, 0);
    addDirect(m, "demods/3/order", 0x1684, 0);
    addDirect(m, "demods/3/oscselect", 0x1690, 0);
    addDirect(m, "demods/3/phaseshift", 0x1698, 0);
    addDirect(m, "demods/3/rate", 0x168c, 2);
    addDirect(m, "demods/3/sinc", 0x169c, 0);
    addDirect(m, "demods/3/timeconstant", 0x16a4, 2);
    addDirect(m, "demods/4/adcselect", 0x1700, 0);
    addDirect(m, "demods/4/bypass", 0x1720, 0);
    addDirect(m, "demods/4/enable", 0x1728, 0);
    addDirect(m, "demods/4/harmonic", 0x1714, 0);
    addDirect(m, "demods/4/order", 0x1704, 0);
    addDirect(m, "demods/4/oscselect", 0x1710, 0);
    addDirect(m, "demods/4/phaseshift", 0x1718, 0);
    addDirect(m, "demods/4/rate", 0x170c, 2);
    addDirect(m, "demods/4/sinc", 0x171c, 0);
    addDirect(m, "demods/4/timeconstant", 0x1724, 2);
    addDirect(m, "demods/5/adcselect", 0x1780, 0);
    addDirect(m, "demods/5/bypass", 0x17a0, 0);
    addDirect(m, "demods/5/enable", 0x17a8, 0);
    addDirect(m, "demods/5/harmonic", 0x1794, 0);
    addDirect(m, "demods/5/order", 0x1784, 0);
    addDirect(m, "demods/5/oscselect", 0x1790, 0);
    addDirect(m, "demods/5/phaseshift", 0x1798, 0);
    addDirect(m, "demods/5/rate", 0x178c, 2);
    addDirect(m, "demods/5/sinc", 0x179c, 0);
    addDirect(m, "demods/5/timeconstant", 0x17a4, 2);
    addDirect(m, "demods/6/adcselect", 0x1800, 0);
    addDirect(m, "demods/6/bypass", 0x1820, 0);
    addDirect(m, "demods/6/enable", 0x1828, 0);
    addDirect(m, "demods/6/harmonic", 0x1814, 0);
    addDirect(m, "demods/6/order", 0x1804, 0);
    addDirect(m, "demods/6/oscselect", 0x1810, 0);
    addDirect(m, "demods/6/phaseshift", 0x1818, 0);
    addDirect(m, "demods/6/rate", 0x180c, 2);
    addDirect(m, "demods/6/sinc", 0x181c, 0);
    addDirect(m, "demods/6/timeconstant", 0x1824, 2);
    addDirect(m, "demods/7/adcselect", 0x1880, 0);
    addDirect(m, "demods/7/bypass", 0x18a0, 0);
    addDirect(m, "demods/7/enable", 0x18a8, 0);
    addDirect(m, "demods/7/harmonic", 0x1894, 0);
    addDirect(m, "demods/7/order", 0x1884, 0);
    addDirect(m, "demods/7/oscselect", 0x1890, 0);
    addDirect(m, "demods/7/phaseshift", 0x1898, 0);
    addDirect(m, "demods/7/rate", 0x188c, 2);
    addDirect(m, "demods/7/sinc", 0x189c, 0);
    addDirect(m, "demods/7/timeconstant", 0x18a4, 2);
    addDirect(m, "inputpwas/0/enable", 0x4400, 0);
    addDirect(m, "inputpwas/0/harmonic", 0x4414, 0);
    addDirect(m, "inputpwas/0/holdoff", 0x4428, 2);
    addDirect(m, "inputpwas/0/insel", 0x440c, 0);
    addDirect(m, "inputpwas/0/mode", 0x4410, 0);
    addDirect(m, "inputpwas/0/oscselect", 0x4408, 0);
    addDirect(m, "inputpwas/0/samplecount", 0x4420, 1);
    addDirect(m, "inputpwas/0/shift", 0x4418, 2);
    addDirect(m, "inputpwas/0/single", 0x4404, 0);
    addDirect(m, "inputpwas/0/termination", 0x441c, 0);
    addDirect(m, "inputpwas/1/enable", 0x4480, 0);
    addDirect(m, "inputpwas/1/harmonic", 0x4494, 0);
    addDirect(m, "inputpwas/1/holdoff", 0x44a8, 2);
    addDirect(m, "inputpwas/1/insel", 0x448c, 0);
    addDirect(m, "inputpwas/1/mode", 0x4490, 0);
    addDirect(m, "inputpwas/1/oscselect", 0x4488, 0);
    addDirect(m, "inputpwas/1/samplecount", 0x44a0, 1);
    addDirect(m, "inputpwas/1/shift", 0x4498, 2);
    addDirect(m, "inputpwas/1/single", 0x4484, 0);
    addDirect(m, "inputpwas/1/termination", 0x449c, 0);
    addDirect(m, "mods/0/carrier/harmonic", 0x2260, 0);
    addDirect(m, "mods/0/carrier/inputselect", 0x2230, 0);
    addDirect(m, "mods/0/carrier/order", 0x223c, 0);
    addDirect(m, "mods/0/carrier/oscselect", 0x2254, 0);
    addDirect(m, "mods/0/carrier/phaseshift", 0x226c, 2);
    addDirect(m, "mods/0/carrier/timeconstant", 0x2248, 2);
    addDirect(m, "mods/0/carrier_enable", 0x2278, 0);
    addDirect(m, "mods/0/carrier_gain", 0x2284, 2);
    addDirect(m, "mods/0/enable", 0x2200, 0);
    addDirect(m, "mods/0/freqdev", 0x2218, 2);
    addDirect(m, "mods/0/freqdevenable", 0x2214, 0);
    addDirect(m, "mods/0/index", 0x221c, 2);
    addDirect(m, "mods/0/mode", 0x2208, 0);
    addDirect(m, "mods/0/operation", 0x2224, 0);
    addDirect(m, "mods/0/output", 0x2204, 0);
    addDirect(m, "mods/0/rate", 0x2228, 2);
    addDirect(m, "mods/0/sampleenable", 0x2220, 0);
    addDirect(m, "mods/0/sideband0_gain", 0x2288, 2);
    addDirect(m, "mods/0/sideband1_gain", 0x228c, 2);
    addDirect(m, "mods/0/sidebands/0/enable", 0x227c, 0);
    addDirect(m, "mods/0/sidebands/0/harmonic", 0x2264, 0);
    addDirect(m, "mods/0/sidebands/0/inputselect", 0x2234, 0);
    addDirect(m, "mods/0/sidebands/0/mode", 0x220c, 0);
    addDirect(m, "mods/0/sidebands/0/order", 0x2240, 0);
    addDirect(m, "mods/0/sidebands/0/oscselect", 0x2258, 0);
    addDirect(m, "mods/0/sidebands/0/phaseshift", 0x2270, 2);
    addDirect(m, "mods/0/sidebands/0/timeconstant", 0x224c, 2);
    addDirect(m, "mods/0/sidebands/1/enable", 0x2280, 0);
    addDirect(m, "mods/0/sidebands/1/harmonic", 0x2268, 0);
    addDirect(m, "mods/0/sidebands/1/inputselect", 0x2238, 0);
    addDirect(m, "mods/0/sidebands/1/mode", 0x2210, 0);
    addDirect(m, "mods/0/sidebands/1/order", 0x2244, 0);
    addDirect(m, "mods/0/sidebands/1/oscselect", 0x225c, 0);
    addDirect(m, "mods/0/sidebands/1/phaseshift", 0x2274, 2);
    addDirect(m, "mods/0/sidebands/1/timeconstant", 0x2250, 2);
    addDirect(m, "mods/0/trigger", 0x222c, 0);
    addDirect(m, "mods/1/carrier/harmonic", 0x2360, 0);
    addDirect(m, "mods/1/carrier/inputselect", 0x2330, 0);
    addDirect(m, "mods/1/carrier/order", 0x233c, 0);
    addDirect(m, "mods/1/carrier/oscselect", 0x2354, 0);
    addDirect(m, "mods/1/carrier/phaseshift", 0x236c, 2);
    addDirect(m, "mods/1/carrier/timeconstant", 0x2348, 2);
    addDirect(m, "mods/1/carrier_enable", 0x2378, 0);
    addDirect(m, "mods/1/carrier_gain", 0x2384, 2);
    addDirect(m, "mods/1/enable", 0x2300, 0);
    addDirect(m, "mods/1/freqdev", 0x2318, 2);
    addDirect(m, "mods/1/freqdevenable", 0x2314, 0);
    addDirect(m, "mods/1/index", 0x231c, 2);
    addDirect(m, "mods/1/mode", 0x2308, 0);
    addDirect(m, "mods/1/operation", 0x2324, 0);
    addDirect(m, "mods/1/output", 0x2304, 0);
    addDirect(m, "mods/1/rate", 0x2328, 2);
    addDirect(m, "mods/1/sampleenable", 0x2320, 0);
    addDirect(m, "mods/1/sideband0_gain", 0x2388, 2);
    addDirect(m, "mods/1/sideband1_gain", 0x238c, 2);
    addDirect(m, "mods/1/sidebands/0/enable", 0x237c, 0);
    addDirect(m, "mods/1/sidebands/0/harmonic", 0x2364, 0);
    addDirect(m, "mods/1/sidebands/0/inputselect", 0x2334, 0);
    addDirect(m, "mods/1/sidebands/0/mode", 0x230c, 0);
    addDirect(m, "mods/1/sidebands/0/order", 0x2340, 0);
    addDirect(m, "mods/1/sidebands/0/oscselect", 0x2358, 0);
    addDirect(m, "mods/1/sidebands/0/phaseshift", 0x2370, 2);
    addDirect(m, "mods/1/sidebands/0/timeconstant", 0x234c, 2);
    addDirect(m, "mods/1/sidebands/1/enable", 0x2380, 0);
    addDirect(m, "mods/1/sidebands/1/harmonic", 0x2368, 0);
    addDirect(m, "mods/1/sidebands/1/inputselect", 0x2338, 0);
    addDirect(m, "mods/1/sidebands/1/mode", 0x2310, 0);
    addDirect(m, "mods/1/sidebands/1/order", 0x2344, 0);
    addDirect(m, "mods/1/sidebands/1/oscselect", 0x235c, 0);
    addDirect(m, "mods/1/sidebands/1/phaseshift", 0x2374, 2);
    addDirect(m, "mods/1/sidebands/1/timeconstant", 0x2350, 2);
    addDirect(m, "mods/1/trigger", 0x232c, 0);
    addDirect(m, "oscs/0/freq", 0x1400, 4);
    addDirect(m, "oscs/1/freq", 0x1408, 4);
    addDirect(m, "oscs/2/freq", 0x1410, 4);
    addDirect(m, "oscs/3/freq", 0x1418, 4);
    addDirect(m, "oscs/4/freq", 0x1420, 4);
    addDirect(m, "oscs/5/freq", 0x1428, 4);
    addDirect(m, "oscs/6/freq", 0x1430, 4);
    addDirect(m, "oscs/7/freq", 0x1438, 4);
    addDirect(m, "outputpwas/0/enable", 0x4300, 0);
    addDirect(m, "outputpwas/0/harmonic", 0x4314, 0);
    addDirect(m, "outputpwas/0/holdoff", 0x4328, 2);
    addDirect(m, "outputpwas/0/insel", 0x430c, 0);
    addDirect(m, "outputpwas/0/mode", 0x4310, 0);
    addDirect(m, "outputpwas/0/oscselect", 0x4308, 0);
    addDirect(m, "outputpwas/0/progress", 0x432c, 2);
    addDirect(m, "outputpwas/0/samplecount", 0x4320, 1);
    addDirect(m, "outputpwas/0/shift", 0x4318, 2);
    addDirect(m, "outputpwas/0/single", 0x4304, 0);
    addDirect(m, "outputpwas/0/status", 0x4330, 0);
    addDirect(m, "outputpwas/0/termination", 0x431c, 0);
    addDirect(m, "outputpwas/1/enable", 0x4380, 0);
    addDirect(m, "outputpwas/1/harmonic", 0x4394, 0);
    addDirect(m, "outputpwas/1/holdoff", 0x43a8, 2);
    addDirect(m, "outputpwas/1/insel", 0x438c, 0);
    addDirect(m, "outputpwas/1/mode", 0x4390, 0);
    addDirect(m, "outputpwas/1/oscselect", 0x4388, 0);
    addDirect(m, "outputpwas/1/progress", 0x43ac, 2);
    addDirect(m, "outputpwas/1/samplecount", 0x43a0, 1);
    addDirect(m, "outputpwas/1/shift", 0x4398, 2);
    addDirect(m, "outputpwas/1/single", 0x4384, 0);
    addDirect(m, "outputpwas/1/status", 0x43b0, 0);
    addDirect(m, "outputpwas/1/termination", 0x439c, 0);
    addDirect(m, "pids/0/center", 0x4874, 3);
    addDirect(m, "pids/0/d", 0x4828, 2);
    addDirect(m, "pids/0/demod/adcselect", 0x4804, 0);
    addDirect(m, "pids/0/demod/harmonic", 0x482c, 0);
    addDirect(m, "pids/0/demod/order", 0x4808, 0);
    addDirect(m, "pids/0/demod/timeconstant", 0x480c, 2);
    addDirect(m, "pids/0/dlimittimeconstant", 0x4814, 2);
    addDirect(m, "pids/0/enable", 0x4800, 0);
    addDirect(m, "pids/0/i", 0x4824, 2);
    addDirect(m, "pids/0/inputchannel", 0x4870, 0);
    addDirect(m, "pids/0/inputsource", 0x486c, 0);
    addDirect(m, "pids/0/limitlower", 0x481c, 2);
    addDirect(m, "pids/0/limitupper", 0x4818, 2);
    addDirect(m, "pids/0/mode", 0x4868, 0);
    addDirect(m, "pids/0/output", 0x4838, 0);
    addDirect(m, "pids/0/outputchannel", 0x4834, 0);
    addDirect(m, "pids/0/p", 0x4820, 2);
    addDirect(m, "pids/0/phaseunwrap", 0x4848, 0);
    addDirect(m, "pids/0/pll/automode", 0x4894, 0);
    addDirect(m, "pids/0/rate", 0x4864, 2);
    addDirect(m, "pids/0/setpoint", 0x4810, 2);
    addDirect(m, "pids/0/stream/rate", 0x485c, 2);
    addDirect(m, "pids/1/center", 0x4974, 3);
    addDirect(m, "pids/1/d", 0x4928, 2);
    addDirect(m, "pids/1/demod/adcselect", 0x4904, 0);
    addDirect(m, "pids/1/demod/harmonic", 0x492c, 0);
    addDirect(m, "pids/1/demod/order", 0x4908, 0);
    addDirect(m, "pids/1/demod/timeconstant", 0x490c, 2);
    addDirect(m, "pids/1/dlimittimeconstant", 0x4914, 2);
    addDirect(m, "pids/1/enable", 0x4900, 0);
    addDirect(m, "pids/1/i", 0x4924, 2);
    addDirect(m, "pids/1/inputchannel", 0x4970, 0);
    addDirect(m, "pids/1/inputsource", 0x496c, 0);
    addDirect(m, "pids/1/limitlower", 0x491c, 2);
    addDirect(m, "pids/1/limitupper", 0x4918, 2);
    addDirect(m, "pids/1/mode", 0x4968, 0);
    addDirect(m, "pids/1/output", 0x4938, 0);
    addDirect(m, "pids/1/outputchannel", 0x4934, 0);
    addDirect(m, "pids/1/p", 0x4920, 2);
    addDirect(m, "pids/1/phaseunwrap", 0x4948, 0);
    addDirect(m, "pids/1/pll/automode", 0x4994, 0);
    addDirect(m, "pids/1/rate", 0x4964, 2);
    addDirect(m, "pids/1/setpoint", 0x4910, 2);
    addDirect(m, "pids/1/stream/rate", 0x495c, 2);
    addDirect(m, "pids/2/center", 0x4a74, 3);
    addDirect(m, "pids/2/d", 0x4a28, 2);
    addDirect(m, "pids/2/demod/adcselect", 0x4a04, 0);
    addDirect(m, "pids/2/demod/harmonic", 0x4a2c, 0);
    addDirect(m, "pids/2/demod/order", 0x4a08, 0);
    addDirect(m, "pids/2/demod/timeconstant", 0x4a0c, 2);
    addDirect(m, "pids/2/dlimittimeconstant", 0x4a14, 2);
    addDirect(m, "pids/2/enable", 0x4a00, 0);
    addDirect(m, "pids/2/i", 0x4a24, 2);
    addDirect(m, "pids/2/inputchannel", 0x4a70, 0);
    addDirect(m, "pids/2/inputsource", 0x4a6c, 0);
    addDirect(m, "pids/2/limitlower", 0x4a1c, 2);
    addDirect(m, "pids/2/limitupper", 0x4a18, 2);
    addDirect(m, "pids/2/mode", 0x4a68, 0);
    addDirect(m, "pids/2/output", 0x4a38, 0);
    addDirect(m, "pids/2/outputchannel", 0x4a34, 0);
    addDirect(m, "pids/2/p", 0x4a20, 2);
    addDirect(m, "pids/2/phaseunwrap", 0x4a48, 0);
    addDirect(m, "pids/2/pll/automode", 0x4a94, 0);
    addDirect(m, "pids/2/rate", 0x4a64, 2);
    addDirect(m, "pids/2/setpoint", 0x4a10, 2);
    addDirect(m, "pids/2/stream/rate", 0x4a5c, 2);
    addDirect(m, "pids/3/center", 0x4b74, 3);
    addDirect(m, "pids/3/d", 0x4b28, 2);
    addDirect(m, "pids/3/demod/adcselect", 0x4b04, 0);
    addDirect(m, "pids/3/demod/harmonic", 0x4b2c, 0);
    addDirect(m, "pids/3/demod/order", 0x4b08, 0);
    addDirect(m, "pids/3/demod/timeconstant", 0x4b0c, 2);
    addDirect(m, "pids/3/dlimittimeconstant", 0x4b14, 2);
    addDirect(m, "pids/3/enable", 0x4b00, 0);
    addDirect(m, "pids/3/i", 0x4b24, 2);
    addDirect(m, "pids/3/inputchannel", 0x4b70, 0);
    addDirect(m, "pids/3/inputsource", 0x4b6c, 0);
    addDirect(m, "pids/3/limitlower", 0x4b1c, 2);
    addDirect(m, "pids/3/limitupper", 0x4b18, 2);
    addDirect(m, "pids/3/mode", 0x4b68, 0);
    addDirect(m, "pids/3/output", 0x4b38, 0);
    addDirect(m, "pids/3/outputchannel", 0x4b34, 0);
    addDirect(m, "pids/3/p", 0x4b20, 2);
    addDirect(m, "pids/3/phaseunwrap", 0x4b48, 0);
    addDirect(m, "pids/3/pll/automode", 0x4b94, 0);
    addDirect(m, "pids/3/rate", 0x4b64, 2);
    addDirect(m, "pids/3/setpoint", 0x4b10, 2);
    addDirect(m, "pids/3/stream/rate", 0x4b5c, 2);
    addDirect(m, "scopes/0/channel", 0x1a3c, 0);
    addDirect(m, "scopes/0/channels/0/bwlimit", 0x1a2c, 0);
    addDirect(m, "scopes/0/channels/0/inputselect", 0x1a28, 0);
    addDirect(m, "scopes/0/channels/0/limitlower", 0x1aa0, 2);
    addDirect(m, "scopes/0/channels/0/limitupper", 0x1aa4, 2);
    addDirect(m, "scopes/0/channels/1/bwlimit", 0x1a34, 0);
    addDirect(m, "scopes/0/channels/1/inputselect", 0x1a30, 0);
    addDirect(m, "scopes/0/channels/1/limitlower", 0x1aac, 2);
    addDirect(m, "scopes/0/channels/1/limitupper", 0x1ab0, 2);
    addDirect(m, "scopes/0/cont", 0x1a54, 0);
    addDirect(m, "scopes/0/enable", 0x1a00, 0);
    addDirect(m, "scopes/0/length", 0x1a18, 0);
    addDirect(m, "scopes/0/segments/count", 0x1a38, 0);
    addDirect(m, "scopes/0/segments/counts/enable", 0x1a6c, 0);
    addDirect(m, "scopes/0/single", 0x1a40, 0);
    addDirect(m, "scopes/0/stream/enables/0", 0xec00, 0);
    addDirect(m, "scopes/0/stream/enables/1", 0xec04, 0);
    addDirect(m, "scopes/0/stream/rate", 0xec08, 0);
    addDirect(m, "scopes/0/time", 0x1a04, 0);
    addDirect(m, "scopes/0/trigchannel", 0x1a24, 0);
    addDirect(m, "scopes/0/trigdelay", 0x1a8c, 2);
    addDirect(m, "scopes/0/trigenable", 0x1a08, 0);
    addDirect(m, "scopes/0/trigfalling", 0x1a10, 0);
    addDirect(m, "scopes/0/trigforce", 0x1a64, 0);
    addDirect(m, "scopes/0/triggate/enable", 0x1a80, 0);
    addDirect(m, "scopes/0/triggate/inputselect", 0x1a84, 0);
    addDirect(m, "scopes/0/trigholdoff", 0x1a1c, 2);
    addDirect(m, "scopes/0/trigholdofftrigger", 0x1a94, 0);
    addDirect(m, "scopes/0/trigholdofftriggerenable", 0x1a90, 0);
    addDirect(m, "scopes/0/trighysteresis/absolute", 0x1a50, 2);
    addDirect(m, "scopes/0/trighysteresis/mode", 0x1a60, 0);
    addDirect(m, "scopes/0/trighysteresis/relative", 0x1a5c, 2);
    addDirect(m, "scopes/0/triglevel", 0x1a14, 2);
    addDirect(m, "scopes/0/trigreference", 0x1a88, 2);
    addDirect(m, "scopes/0/trigrising", 0x1a0c, 0);
    addDirect(m, "scopes/0/trigstate", 0x1a78, 0);
    addDirect(m, "sigins/0/ac", 0x1d04, 0);
    addDirect(m, "sigins/0/autorange", 0x1d40, 0);
    addDirect(m, "sigins/0/bw", 0x1d20, 0);
    addDirect(m, "sigins/0/diff", 0x1d3c, 0);
    addDirect(m, "sigins/0/imp50", 0x1d08, 0);
    addDirect(m, "sigins/0/on", 0x1d24, 0);
    addDirect(m, "sigins/0/range", 0x1d28, 0);
    addDirect(m, "sigins/0/scaling", 0x1d44, 2);
    addDirect(m, "sigins/1/ac", 0x1d84, 0);
    addDirect(m, "sigins/1/autorange", 0x1dc0, 0);
    addDirect(m, "sigins/1/bw", 0x1da0, 0);
    addDirect(m, "sigins/1/diff", 0x1dbc, 0);
    addDirect(m, "sigins/1/imp50", 0x1d88, 0);
    addDirect(m, "sigins/1/on", 0x1da4, 0);
    addDirect(m, "sigins/1/range", 0x1da8, 0);
    addDirect(m, "sigins/1/scaling", 0x1dc4, 2);
    addDirect(m, "sigouts/0/amplitudes/0", 0x2004, 2);
    addDirect(m, "sigouts/0/amplitudes/1", 0x200c, 2);
    addDirect(m, "sigouts/0/amplitudes/2", 0x2014, 2);
    addDirect(m, "sigouts/0/amplitudes/3", 0x201c, 2);
    addDirect(m, "sigouts/0/amplitudes/4", 0x2024, 2);
    addDirect(m, "sigouts/0/amplitudes/5", 0x202c, 2);
    addDirect(m, "sigouts/0/amplitudes/6", 0x2034, 2);
    addDirect(m, "sigouts/0/amplitudes/7", 0x203c, 2);
    addDirect(m, "sigouts/0/autorange", 0x1e20, 0);
    addDirect(m, "sigouts/0/enables/0", 0x2000, 0);
    addDirect(m, "sigouts/0/enables/1", 0x2008, 0);
    addDirect(m, "sigouts/0/enables/2", 0x2010, 0);
    addDirect(m, "sigouts/0/enables/3", 0x2018, 0);
    addDirect(m, "sigouts/0/enables/4", 0x2020, 0);
    addDirect(m, "sigouts/0/enables/5", 0x2028, 0);
    addDirect(m, "sigouts/0/enables/6", 0x2030, 0);
    addDirect(m, "sigouts/0/enables/7", 0x2038, 0);
    addDirect(m, "sigouts/0/imp50", 0x1e14, 0);
    addDirect(m, "sigouts/0/offset", 0x1e30, 2);
    addDirect(m, "sigouts/0/on", 0x1e00, 0);
    addDirect(m, "sigouts/0/range", 0x1e04, 2);
    addDirect(m, "sigouts/1/amplitudes/0", 0x2044, 2);
    addDirect(m, "sigouts/1/amplitudes/1", 0x204c, 2);
    addDirect(m, "sigouts/1/amplitudes/2", 0x2054, 2);
    addDirect(m, "sigouts/1/amplitudes/3", 0x205c, 2);
    addDirect(m, "sigouts/1/amplitudes/4", 0x2064, 2);
    addDirect(m, "sigouts/1/amplitudes/5", 0x206c, 2);
    addDirect(m, "sigouts/1/amplitudes/6", 0x2074, 2);
    addDirect(m, "sigouts/1/amplitudes/7", 0x207c, 2);
    addDirect(m, "sigouts/1/autorange", 0x1e60, 0);
    addDirect(m, "sigouts/1/enables/0", 0x2040, 0);
    addDirect(m, "sigouts/1/enables/1", 0x2048, 0);
    addDirect(m, "sigouts/1/enables/2", 0x2050, 0);
    addDirect(m, "sigouts/1/enables/3", 0x2058, 0);
    addDirect(m, "sigouts/1/enables/4", 0x2060, 0);
    addDirect(m, "sigouts/1/enables/5", 0x2068, 0);
    addDirect(m, "sigouts/1/enables/6", 0x2070, 0);
    addDirect(m, "sigouts/1/enables/7", 0x2078, 0);
    addDirect(m, "sigouts/1/imp50", 0x1e54, 0);
    addDirect(m, "sigouts/1/offset", 0x1e70, 2);
    addDirect(m, "sigouts/1/on", 0x1e40, 0);
    addDirect(m, "sigouts/1/range", 0x1e44, 2);
    addDirect(m, "triggers/in/0/dcc_fedge", 0xd910, 0);
    addDirect(m, "triggers/in/0/imp50", 0xd900, 0);
    addDirect(m, "triggers/in/0/level", 0xd904, 0);
    addDirect(m, "triggers/in/1/dcc_fedge", 0xd930, 0);
    addDirect(m, "triggers/in/1/imp50", 0xd920, 0);
    addDirect(m, "triggers/in/1/level", 0xd924, 0);
    addDirect(m, "triggers/in/2/dcc_fedge", 0xd950, 0);
    addDirect(m, "triggers/in/2/imp50", 0xd940, 0);
    addDirect(m, "triggers/in/2/level", 0xd944, 0);
    addDirect(m, "triggers/in/3/dcc_fedge", 0xd970, 0);
    addDirect(m, "triggers/in/3/imp50", 0xd960, 0);
    addDirect(m, "triggers/in/3/level", 0xd964, 0);
    addDirect(m, "triggers/out/0/delay", 0xda14, 2);
    addDirect(m, "triggers/out/0/dir", 0xda04, 0);
    addDirect(m, "triggers/out/0/hold", 0xda10, 0);
    addDirect(m, "triggers/out/0/srcsel", 0xda00, 0);
    addDirect(m, "triggers/out/0/static_value", 0xda08, 0);
    addDirect(m, "triggers/out/1/delay", 0xda34, 2);
    addDirect(m, "triggers/out/1/dir", 0xda24, 0);
    addDirect(m, "triggers/out/1/hold", 0xda30, 0);
    addDirect(m, "triggers/out/1/srcsel", 0xda20, 0);
    addDirect(m, "triggers/out/1/static_value", 0xda28, 0);
    addDirect(m, "triggers/out/2/delay", 0xda54, 2);
    addDirect(m, "triggers/out/2/dir", 0xda44, 0);
    addDirect(m, "triggers/out/2/hold", 0xda50, 0);
    addDirect(m, "triggers/out/2/srcsel", 0xda40, 0);
    addDirect(m, "triggers/out/2/static_value", 0xda48, 0);
    addDirect(m, "triggers/out/3/delay", 0xda74, 2);
    addDirect(m, "triggers/out/3/dir", 0xda64, 0);
    addDirect(m, "triggers/out/3/hold", 0xda70, 0);
    addDirect(m, "triggers/out/3/srcsel", 0xda60, 0);
    addDirect(m, "triggers/out/3/static_value", 0xda68, 0);

    return nm;
}

// ---------------------------------------------------------------------------
// HDAWG — 186 entries @0x1ad9a0
// ---------------------------------------------------------------------------
template <>
std::unique_ptr<NodeMap> GetNodeMap<AwgDeviceType::HDAWG>::get() {  // @0x1ad9a0
    auto nm = std::make_unique<NodeMap>();
    auto& m = nm->entries_;

    addVirt(m, "_/dios/0/output", "DIOOUTPUT", {0}, 0);
    addVirt(m, "awgs/0/outputs/0/gains/0", "AWGOUTGAIN", {0, 0, 0}, 2);
    addVirt(m, "awgs/0/outputs/0/gains/1", "AWGOUTGAIN", {0, 0, 1}, 2);
    addVirt(m, "awgs/0/outputs/0/modulation/carriers/0/harmonic", "AWGMODHARM", {0, 0, 0}, 0);
    addVirt(m, "awgs/0/outputs/0/modulation/carriers/0/oscselect", "AWGMODOSCSEL", {0, 0, 0}, 0);
    addVirt(m, "awgs/0/outputs/0/modulation/carriers/0/phaseshift", "AWGMODPHASE", {0, 0, 0}, 2);
    addVirt(m, "awgs/0/outputs/0/modulation/carriers/1/harmonic", "AWGMODHARM", {0, 0, 1}, 0);
    addVirt(m, "awgs/0/outputs/0/modulation/carriers/1/oscselect", "AWGMODOSCSEL", {0, 0, 1}, 0);
    addVirt(m, "awgs/0/outputs/0/modulation/carriers/1/phaseshift", "AWGMODPHASE", {0, 0, 1}, 2);
    addVirt(m, "awgs/0/outputs/0/modulation/carriers/2/harmonic", "AWGMODHARM", {0, 0, 2}, 0);
    addVirt(m, "awgs/0/outputs/0/modulation/carriers/2/oscselect", "AWGMODOSCSEL", {0, 0, 2}, 0);
    addVirt(m, "awgs/0/outputs/0/modulation/carriers/2/phaseshift", "AWGMODPHASE", {0, 0, 2}, 2);
    addVirt(m, "awgs/0/outputs/0/modulation/carriers/3/harmonic", "AWGMODHARM", {0, 0, 3}, 0);
    addVirt(m, "awgs/0/outputs/0/modulation/carriers/3/oscselect", "AWGMODOSCSEL", {0, 0, 3}, 0);
    addVirt(m, "awgs/0/outputs/0/modulation/carriers/3/phaseshift", "AWGMODPHASE", {0, 0, 3}, 2);
    addVirt(m, "awgs/0/outputs/1/gains/0", "AWGOUTGAIN", {0, 1, 0}, 2);
    addVirt(m, "awgs/0/outputs/1/gains/1", "AWGOUTGAIN", {0, 1, 1}, 2);
    addVirt(m, "awgs/0/outputs/1/modulation/carriers/0/harmonic", "AWGMODHARM", {0, 1, 0}, 0);
    addVirt(m, "awgs/0/outputs/1/modulation/carriers/0/oscselect", "AWGMODOSCSEL", {0, 1, 0}, 0);
    addVirt(m, "awgs/0/outputs/1/modulation/carriers/0/phaseshift", "AWGMODPHASE", {0, 1, 0}, 2);
    addVirt(m, "awgs/0/outputs/1/modulation/carriers/1/harmonic", "AWGMODHARM", {0, 1, 1}, 0);
    addVirt(m, "awgs/0/outputs/1/modulation/carriers/1/oscselect", "AWGMODOSCSEL", {0, 1, 1}, 0);
    addVirt(m, "awgs/0/outputs/1/modulation/carriers/1/phaseshift", "AWGMODPHASE", {0, 1, 1}, 2);
    addVirt(m, "awgs/0/outputs/1/modulation/carriers/2/harmonic", "AWGMODHARM", {0, 1, 2}, 0);
    addVirt(m, "awgs/0/outputs/1/modulation/carriers/2/oscselect", "AWGMODOSCSEL", {0, 1, 2}, 0);
    addVirt(m, "awgs/0/outputs/1/modulation/carriers/2/phaseshift", "AWGMODPHASE", {0, 1, 2}, 2);
    addVirt(m, "awgs/0/outputs/1/modulation/carriers/3/harmonic", "AWGMODHARM", {0, 1, 3}, 0);
    addVirt(m, "awgs/0/outputs/1/modulation/carriers/3/oscselect", "AWGMODOSCSEL", {0, 1, 3}, 0);
    addVirt(m, "awgs/0/outputs/1/modulation/carriers/3/phaseshift", "AWGMODPHASE", {0, 1, 3}, 2);
    addVirt(m, "awgs/1/outputs/0/gains/0", "AWGOUTGAIN", {1, 0, 0}, 2);
    addVirt(m, "awgs/1/outputs/0/gains/1", "AWGOUTGAIN", {1, 0, 1}, 2);
    addVirt(m, "awgs/1/outputs/0/modulation/carriers/0/harmonic", "AWGMODHARM", {1, 0, 0}, 0);
    addVirt(m, "awgs/1/outputs/0/modulation/carriers/0/oscselect", "AWGMODOSCSEL", {1, 0, 0}, 0);
    addVirt(m, "awgs/1/outputs/0/modulation/carriers/0/phaseshift", "AWGMODPHASE", {1, 0, 0}, 2);
    addVirt(m, "awgs/1/outputs/0/modulation/carriers/1/harmonic", "AWGMODHARM", {1, 0, 1}, 0);
    addVirt(m, "awgs/1/outputs/0/modulation/carriers/1/oscselect", "AWGMODOSCSEL", {1, 0, 1}, 0);
    addVirt(m, "awgs/1/outputs/0/modulation/carriers/1/phaseshift", "AWGMODPHASE", {1, 0, 1}, 2);
    addVirt(m, "awgs/1/outputs/0/modulation/carriers/2/harmonic", "AWGMODHARM", {1, 0, 2}, 0);
    addVirt(m, "awgs/1/outputs/0/modulation/carriers/2/oscselect", "AWGMODOSCSEL", {1, 0, 2}, 0);
    addVirt(m, "awgs/1/outputs/0/modulation/carriers/2/phaseshift", "AWGMODPHASE", {1, 0, 2}, 2);
    addVirt(m, "awgs/1/outputs/0/modulation/carriers/3/harmonic", "AWGMODHARM", {1, 0, 3}, 0);
    addVirt(m, "awgs/1/outputs/0/modulation/carriers/3/oscselect", "AWGMODOSCSEL", {1, 0, 3}, 0);
    addVirt(m, "awgs/1/outputs/0/modulation/carriers/3/phaseshift", "AWGMODPHASE", {1, 0, 3}, 2);
    addVirt(m, "awgs/1/outputs/1/gains/0", "AWGOUTGAIN", {1, 1, 0}, 2);
    addVirt(m, "awgs/1/outputs/1/gains/1", "AWGOUTGAIN", {1, 1, 1}, 2);
    addVirt(m, "awgs/1/outputs/1/modulation/carriers/0/harmonic", "AWGMODHARM", {1, 1, 0}, 0);
    addVirt(m, "awgs/1/outputs/1/modulation/carriers/0/oscselect", "AWGMODOSCSEL", {1, 1, 0}, 0);
    addVirt(m, "awgs/1/outputs/1/modulation/carriers/0/phaseshift", "AWGMODPHASE", {1, 1, 0}, 2);
    addVirt(m, "awgs/1/outputs/1/modulation/carriers/1/harmonic", "AWGMODHARM", {1, 1, 1}, 0);
    addVirt(m, "awgs/1/outputs/1/modulation/carriers/1/oscselect", "AWGMODOSCSEL", {1, 1, 1}, 0);
    addVirt(m, "awgs/1/outputs/1/modulation/carriers/1/phaseshift", "AWGMODPHASE", {1, 1, 1}, 2);
    addVirt(m, "awgs/1/outputs/1/modulation/carriers/2/harmonic", "AWGMODHARM", {1, 1, 2}, 0);
    addVirt(m, "awgs/1/outputs/1/modulation/carriers/2/oscselect", "AWGMODOSCSEL", {1, 1, 2}, 0);
    addVirt(m, "awgs/1/outputs/1/modulation/carriers/2/phaseshift", "AWGMODPHASE", {1, 1, 2}, 2);
    addVirt(m, "awgs/1/outputs/1/modulation/carriers/3/harmonic", "AWGMODHARM", {1, 1, 3}, 0);
    addVirt(m, "awgs/1/outputs/1/modulation/carriers/3/oscselect", "AWGMODOSCSEL", {1, 1, 3}, 0);
    addVirt(m, "awgs/1/outputs/1/modulation/carriers/3/phaseshift", "AWGMODPHASE", {1, 1, 3}, 2);
    addVirt(m, "awgs/2/outputs/0/gains/0", "AWGOUTGAIN", {2, 0, 0}, 2);
    addVirt(m, "awgs/2/outputs/0/gains/1", "AWGOUTGAIN", {2, 0, 1}, 2);
    addVirt(m, "awgs/2/outputs/0/modulation/carriers/0/harmonic", "AWGMODHARM", {2, 0, 0}, 0);
    addVirt(m, "awgs/2/outputs/0/modulation/carriers/0/oscselect", "AWGMODOSCSEL", {2, 0, 0}, 0);
    addVirt(m, "awgs/2/outputs/0/modulation/carriers/0/phaseshift", "AWGMODPHASE", {2, 0, 0}, 2);
    addVirt(m, "awgs/2/outputs/0/modulation/carriers/1/harmonic", "AWGMODHARM", {2, 0, 1}, 0);
    addVirt(m, "awgs/2/outputs/0/modulation/carriers/1/oscselect", "AWGMODOSCSEL", {2, 0, 1}, 0);
    addVirt(m, "awgs/2/outputs/0/modulation/carriers/1/phaseshift", "AWGMODPHASE", {2, 0, 1}, 2);
    addVirt(m, "awgs/2/outputs/0/modulation/carriers/2/harmonic", "AWGMODHARM", {2, 0, 2}, 0);
    addVirt(m, "awgs/2/outputs/0/modulation/carriers/2/oscselect", "AWGMODOSCSEL", {2, 0, 2}, 0);
    addVirt(m, "awgs/2/outputs/0/modulation/carriers/2/phaseshift", "AWGMODPHASE", {2, 0, 2}, 2);
    addVirt(m, "awgs/2/outputs/0/modulation/carriers/3/harmonic", "AWGMODHARM", {2, 0, 3}, 0);
    addVirt(m, "awgs/2/outputs/0/modulation/carriers/3/oscselect", "AWGMODOSCSEL", {2, 0, 3}, 0);
    addVirt(m, "awgs/2/outputs/0/modulation/carriers/3/phaseshift", "AWGMODPHASE", {2, 0, 3}, 2);
    addVirt(m, "awgs/2/outputs/1/gains/0", "AWGOUTGAIN", {2, 1, 0}, 2);
    addVirt(m, "awgs/2/outputs/1/gains/1", "AWGOUTGAIN", {2, 1, 1}, 2);
    addVirt(m, "awgs/2/outputs/1/modulation/carriers/0/harmonic", "AWGMODHARM", {2, 1, 0}, 0);
    addVirt(m, "awgs/2/outputs/1/modulation/carriers/0/oscselect", "AWGMODOSCSEL", {2, 1, 0}, 0);
    addVirt(m, "awgs/2/outputs/1/modulation/carriers/0/phaseshift", "AWGMODPHASE", {2, 1, 0}, 2);
    addVirt(m, "awgs/2/outputs/1/modulation/carriers/1/harmonic", "AWGMODHARM", {2, 1, 1}, 0);
    addVirt(m, "awgs/2/outputs/1/modulation/carriers/1/oscselect", "AWGMODOSCSEL", {2, 1, 1}, 0);
    addVirt(m, "awgs/2/outputs/1/modulation/carriers/1/phaseshift", "AWGMODPHASE", {2, 1, 1}, 2);
    addVirt(m, "awgs/2/outputs/1/modulation/carriers/2/harmonic", "AWGMODHARM", {2, 1, 2}, 0);
    addVirt(m, "awgs/2/outputs/1/modulation/carriers/2/oscselect", "AWGMODOSCSEL", {2, 1, 2}, 0);
    addVirt(m, "awgs/2/outputs/1/modulation/carriers/2/phaseshift", "AWGMODPHASE", {2, 1, 2}, 2);
    addVirt(m, "awgs/2/outputs/1/modulation/carriers/3/harmonic", "AWGMODHARM", {2, 1, 3}, 0);
    addVirt(m, "awgs/2/outputs/1/modulation/carriers/3/oscselect", "AWGMODOSCSEL", {2, 1, 3}, 0);
    addVirt(m, "awgs/2/outputs/1/modulation/carriers/3/phaseshift", "AWGMODPHASE", {2, 1, 3}, 2);
    addVirt(m, "awgs/3/outputs/0/gains/0", "AWGOUTGAIN", {3, 0, 0}, 2);
    addVirt(m, "awgs/3/outputs/0/gains/1", "AWGOUTGAIN", {3, 0, 1}, 2);
    addVirt(m, "awgs/3/outputs/0/modulation/carriers/0/harmonic", "AWGMODHARM", {3, 0, 0}, 0);
    addVirt(m, "awgs/3/outputs/0/modulation/carriers/0/oscselect", "AWGMODOSCSEL", {3, 0, 0}, 0);
    addVirt(m, "awgs/3/outputs/0/modulation/carriers/0/phaseshift", "AWGMODPHASE", {3, 0, 0}, 2);
    addVirt(m, "awgs/3/outputs/0/modulation/carriers/1/harmonic", "AWGMODHARM", {3, 0, 1}, 0);
    addVirt(m, "awgs/3/outputs/0/modulation/carriers/1/oscselect", "AWGMODOSCSEL", {3, 0, 1}, 0);
    addVirt(m, "awgs/3/outputs/0/modulation/carriers/1/phaseshift", "AWGMODPHASE", {3, 0, 1}, 2);
    addVirt(m, "awgs/3/outputs/0/modulation/carriers/2/harmonic", "AWGMODHARM", {3, 0, 2}, 0);
    addVirt(m, "awgs/3/outputs/0/modulation/carriers/2/oscselect", "AWGMODOSCSEL", {3, 0, 2}, 0);
    addVirt(m, "awgs/3/outputs/0/modulation/carriers/2/phaseshift", "AWGMODPHASE", {3, 0, 2}, 2);
    addVirt(m, "awgs/3/outputs/0/modulation/carriers/3/harmonic", "AWGMODHARM", {3, 0, 3}, 0);
    addVirt(m, "awgs/3/outputs/0/modulation/carriers/3/oscselect", "AWGMODOSCSEL", {3, 0, 3}, 0);
    addVirt(m, "awgs/3/outputs/0/modulation/carriers/3/phaseshift", "AWGMODPHASE", {3, 0, 3}, 2);
    addVirt(m, "awgs/3/outputs/1/gains/0", "AWGOUTGAIN", {3, 1, 0}, 2);
    addVirt(m, "awgs/3/outputs/1/gains/1", "AWGOUTGAIN", {3, 1, 1}, 2);
    addVirt(m, "awgs/3/outputs/1/modulation/carriers/0/harmonic", "AWGMODHARM", {3, 1, 0}, 0);
    addVirt(m, "awgs/3/outputs/1/modulation/carriers/0/oscselect", "AWGMODOSCSEL", {3, 1, 0}, 0);
    addVirt(m, "awgs/3/outputs/1/modulation/carriers/0/phaseshift", "AWGMODPHASE", {3, 1, 0}, 2);
    addVirt(m, "awgs/3/outputs/1/modulation/carriers/1/harmonic", "AWGMODHARM", {3, 1, 1}, 0);
    addVirt(m, "awgs/3/outputs/1/modulation/carriers/1/oscselect", "AWGMODOSCSEL", {3, 1, 1}, 0);
    addVirt(m, "awgs/3/outputs/1/modulation/carriers/1/phaseshift", "AWGMODPHASE", {3, 1, 1}, 2);
    addVirt(m, "awgs/3/outputs/1/modulation/carriers/2/harmonic", "AWGMODHARM", {3, 1, 2}, 0);
    addVirt(m, "awgs/3/outputs/1/modulation/carriers/2/oscselect", "AWGMODOSCSEL", {3, 1, 2}, 0);
    addVirt(m, "awgs/3/outputs/1/modulation/carriers/2/phaseshift", "AWGMODPHASE", {3, 1, 2}, 2);
    addVirt(m, "awgs/3/outputs/1/modulation/carriers/3/harmonic", "AWGMODHARM", {3, 1, 3}, 0);
    addVirt(m, "awgs/3/outputs/1/modulation/carriers/3/oscselect", "AWGMODOSCSEL", {3, 1, 3}, 0);
    addVirt(m, "awgs/3/outputs/1/modulation/carriers/3/phaseshift", "AWGMODPHASE", {3, 1, 3}, 2);
    addVirt(m, "oscs/0/freq", "OSCFREQ", {0}, 4);
    addVirt(m, "oscs/1/freq", "OSCFREQ", {1}, 4);
    addVirt(m, "oscs/10/freq", "OSCFREQ", {10}, 4);
    addVirt(m, "oscs/11/freq", "OSCFREQ", {11}, 4);
    addVirt(m, "oscs/12/freq", "OSCFREQ", {12}, 4);
    addVirt(m, "oscs/13/freq", "OSCFREQ", {13}, 4);
    addVirt(m, "oscs/14/freq", "OSCFREQ", {14}, 4);
    addVirt(m, "oscs/15/freq", "OSCFREQ", {15}, 4);
    addVirt(m, "oscs/2/freq", "OSCFREQ", {2}, 4);
    addVirt(m, "oscs/3/freq", "OSCFREQ", {3}, 4);
    addVirt(m, "oscs/4/freq", "OSCFREQ", {4}, 4);
    addVirt(m, "oscs/5/freq", "OSCFREQ", {5}, 4);
    addVirt(m, "oscs/6/freq", "OSCFREQ", {6}, 4);
    addVirt(m, "oscs/7/freq", "OSCFREQ", {7}, 4);
    addVirt(m, "oscs/8/freq", "OSCFREQ", {8}, 4);
    addVirt(m, "oscs/9/freq", "OSCFREQ", {9}, 4);
    addVirt(m, "oscs/phasereset", "OSCPHASERST", {}, 0);
    addVirt(m, "sines/0/amplitudes/0", "SINEAMPL", {0, 0}, 2);
    addVirt(m, "sines/0/amplitudes/1", "SINEAMPL", {0, 1}, 2);
    addVirt(m, "sines/0/enables/0", "SINEENABLE", {0, 0}, 0);
    addVirt(m, "sines/0/enables/1", "SINEENABLE", {0, 1}, 0);
    addVirt(m, "sines/0/harmonic", "SINEHARM", {0}, 0);
    addVirt(m, "sines/0/oscselect", "SINEOSCSEL", {0}, 0);
    addVirt(m, "sines/0/phaseshift", "SINEPHASE", {0}, 5);
    addVirt(m, "sines/1/amplitudes/0", "SINEAMPL", {1, 0}, 2);
    addVirt(m, "sines/1/amplitudes/1", "SINEAMPL", {1, 1}, 2);
    addVirt(m, "sines/1/enables/0", "SINEENABLE", {1, 0}, 0);
    addVirt(m, "sines/1/enables/1", "SINEENABLE", {1, 1}, 0);
    addVirt(m, "sines/1/harmonic", "SINEHARM", {1}, 0);
    addVirt(m, "sines/1/oscselect", "SINEOSCSEL", {1}, 0);
    addVirt(m, "sines/1/phaseshift", "SINEPHASE", {1}, 5);
    addVirt(m, "sines/2/amplitudes/0", "SINEAMPL", {2, 0}, 2);
    addVirt(m, "sines/2/amplitudes/1", "SINEAMPL", {2, 1}, 2);
    addVirt(m, "sines/2/enables/0", "SINEENABLE", {2, 0}, 0);
    addVirt(m, "sines/2/enables/1", "SINEENABLE", {2, 1}, 0);
    addVirt(m, "sines/2/harmonic", "SINEHARM", {2}, 0);
    addVirt(m, "sines/2/oscselect", "SINEOSCSEL", {2}, 0);
    addVirt(m, "sines/2/phaseshift", "SINEPHASE", {2}, 5);
    addVirt(m, "sines/3/amplitudes/0", "SINEAMPL", {3, 0}, 2);
    addVirt(m, "sines/3/amplitudes/1", "SINEAMPL", {3, 1}, 2);
    addVirt(m, "sines/3/enables/0", "SINEENABLE", {3, 0}, 0);
    addVirt(m, "sines/3/enables/1", "SINEENABLE", {3, 1}, 0);
    addVirt(m, "sines/3/harmonic", "SINEHARM", {3}, 0);
    addVirt(m, "sines/3/oscselect", "SINEOSCSEL", {3}, 0);
    addVirt(m, "sines/3/phaseshift", "SINEPHASE", {3}, 5);
    addVirt(m, "sines/4/amplitudes/0", "SINEAMPL", {4, 0}, 2);
    addVirt(m, "sines/4/amplitudes/1", "SINEAMPL", {4, 1}, 2);
    addVirt(m, "sines/4/enables/0", "SINEENABLE", {4, 0}, 0);
    addVirt(m, "sines/4/enables/1", "SINEENABLE", {4, 1}, 0);
    addVirt(m, "sines/4/harmonic", "SINEHARM", {4}, 0);
    addVirt(m, "sines/4/oscselect", "SINEOSCSEL", {4}, 0);
    addVirt(m, "sines/4/phaseshift", "SINEPHASE", {4}, 5);
    addVirt(m, "sines/5/amplitudes/0", "SINEAMPL", {5, 0}, 2);
    addVirt(m, "sines/5/amplitudes/1", "SINEAMPL", {5, 1}, 2);
    addVirt(m, "sines/5/enables/0", "SINEENABLE", {5, 0}, 0);
    addVirt(m, "sines/5/enables/1", "SINEENABLE", {5, 1}, 0);
    addVirt(m, "sines/5/harmonic", "SINEHARM", {5}, 0);
    addVirt(m, "sines/5/oscselect", "SINEOSCSEL", {5}, 0);
    addVirt(m, "sines/5/phaseshift", "SINEPHASE", {5}, 5);
    addVirt(m, "sines/6/amplitudes/0", "SINEAMPL", {6, 0}, 2);
    addVirt(m, "sines/6/amplitudes/1", "SINEAMPL", {6, 1}, 2);
    addVirt(m, "sines/6/enables/0", "SINEENABLE", {6, 0}, 0);
    addVirt(m, "sines/6/enables/1", "SINEENABLE", {6, 1}, 0);
    addVirt(m, "sines/6/harmonic", "SINEHARM", {6}, 0);
    addVirt(m, "sines/6/oscselect", "SINEOSCSEL", {6}, 0);
    addVirt(m, "sines/6/phaseshift", "SINEPHASE", {6}, 5);
    addVirt(m, "sines/7/amplitudes/0", "SINEAMPL", {7, 0}, 2);
    addVirt(m, "sines/7/amplitudes/1", "SINEAMPL", {7, 1}, 2);
    addVirt(m, "sines/7/enables/0", "SINEENABLE", {7, 0}, 0);
    addVirt(m, "sines/7/enables/1", "SINEENABLE", {7, 1}, 0);
    addVirt(m, "sines/7/harmonic", "SINEHARM", {7}, 0);
    addVirt(m, "sines/7/oscselect", "SINEOSCSEL", {7}, 0);
    addVirt(m, "sines/7/phaseshift", "SINEPHASE", {7}, 5);

    return nm;
}

// ---------------------------------------------------------------------------
// UHFQA — 209 entries @0x1b1470
// ---------------------------------------------------------------------------
template <>
std::unique_ptr<NodeMap> GetNodeMap<AwgDeviceType::UHFQA>::get() {  // @0x1b1470
    auto nm = std::make_unique<NodeMap>();
    auto& m = nm->entries_;

    addDirect(m, "auxouts/0/demodselect", 0xd738, 0);
    addDirect(m, "auxouts/0/limitlower", 0xd720, 2);
    addDirect(m, "auxouts/0/limitupper", 0xd724, 2);
    addDirect(m, "auxouts/0/offset", 0xd708, 2);
    addDirect(m, "auxouts/0/outputselect", 0xd734, 0);
    addDirect(m, "auxouts/0/preoffset", 0xd700, 3);
    addDirect(m, "auxouts/0/scale", 0xd710, 2);
    addDirect(m, "auxouts/1/demodselect", 0xd778, 0);
    addDirect(m, "auxouts/1/limitlower", 0xd760, 2);
    addDirect(m, "auxouts/1/limitupper", 0xd764, 2);
    addDirect(m, "auxouts/1/offset", 0xd748, 2);
    addDirect(m, "auxouts/1/outputselect", 0xd774, 0);
    addDirect(m, "auxouts/1/preoffset", 0xd740, 3);
    addDirect(m, "auxouts/1/scale", 0xd750, 2);
    addDirect(m, "auxouts/2/demodselect", 0xd7b8, 0);
    addDirect(m, "auxouts/2/limitlower", 0xd7a0, 2);
    addDirect(m, "auxouts/2/limitupper", 0xd7a4, 2);
    addDirect(m, "auxouts/2/offset", 0xd788, 2);
    addDirect(m, "auxouts/2/outputselect", 0xd7b4, 0);
    addDirect(m, "auxouts/2/preoffset", 0xd780, 3);
    addDirect(m, "auxouts/2/scale", 0xd790, 2);
    addDirect(m, "auxouts/3/demodselect", 0xd7f8, 0);
    addDirect(m, "auxouts/3/limitlower", 0xd7e0, 2);
    addDirect(m, "auxouts/3/limitupper", 0xd7e4, 2);
    addDirect(m, "auxouts/3/offset", 0xd7c8, 2);
    addDirect(m, "auxouts/3/outputselect", 0xd7f4, 0);
    addDirect(m, "auxouts/3/preoffset", 0xd7c0, 3);
    addDirect(m, "auxouts/3/scale", 0xd7d0, 2);
    addDirect(m, "awgs/0/auxtriggers/0/channel", 0x5300, 0);
    addDirect(m, "awgs/0/auxtriggers/0/falling", 0x5308, 0);
    addDirect(m, "awgs/0/auxtriggers/0/rising", 0x5304, 0);
    addDirect(m, "awgs/0/auxtriggers/0/state", 0x530c, 0);
    addDirect(m, "awgs/0/auxtriggers/1/channel", 0x5310, 0);
    addDirect(m, "awgs/0/auxtriggers/1/falling", 0x5318, 0);
    addDirect(m, "awgs/0/auxtriggers/1/rising", 0x5314, 0);
    addDirect(m, "awgs/0/auxtriggers/1/state", 0x531c, 0);
    addDirect(m, "awgs/0/enable", 0x5000, 0);
    addDirect(m, "awgs/0/outputs/0/amplitude", 0x5500, 2);
    addDirect(m, "awgs/0/outputs/0/mode", 0x5504, 0);
    addDirect(m, "awgs/0/outputs/1/amplitude", 0x5540, 2);
    addDirect(m, "awgs/0/outputs/1/mode", 0x5544, 0);
    addDirect(m, "awgs/0/single", 0x5004, 0);
    addDirect(m, "awgs/0/time", 0x5008, 0);
    addDirect(m, "awgs/0/triggers/0/channel", 0x5218, 0);
    addDirect(m, "awgs/0/triggers/0/falling", 0x5214, 0);
    addDirect(m, "awgs/0/triggers/0/force", 0x521c, 0);
    addDirect(m, "awgs/0/triggers/0/gate/enable", 0x5224, 0);
    addDirect(m, "awgs/0/triggers/0/gate/inputselect", 0x5228, 0);
    addDirect(m, "awgs/0/triggers/0/hysteresis/absolute", 0x5204, 2);
    addDirect(m, "awgs/0/triggers/0/hysteresis/mode", 0x520c, 0);
    addDirect(m, "awgs/0/triggers/0/hysteresis/relative", 0x5208, 2);
    addDirect(m, "awgs/0/triggers/0/level", 0x5200, 2);
    addDirect(m, "awgs/0/triggers/0/rising", 0x5210, 0);
    addDirect(m, "awgs/0/triggers/0/state", 0x5220, 0);
    addDirect(m, "awgs/0/triggers/1/channel", 0x5258, 0);
    addDirect(m, "awgs/0/triggers/1/falling", 0x5254, 0);
    addDirect(m, "awgs/0/triggers/1/force", 0x525c, 0);
    addDirect(m, "awgs/0/triggers/1/gate/enable", 0x5264, 0);
    addDirect(m, "awgs/0/triggers/1/gate/inputselect", 0x5268, 0);
    addDirect(m, "awgs/0/triggers/1/hysteresis/absolute", 0x5244, 2);
    addDirect(m, "awgs/0/triggers/1/hysteresis/mode", 0x524c, 0);
    addDirect(m, "awgs/0/triggers/1/hysteresis/relative", 0x5248, 2);
    addDirect(m, "awgs/0/triggers/1/level", 0x5240, 2);
    addDirect(m, "awgs/0/triggers/1/rising", 0x5250, 0);
    addDirect(m, "awgs/0/triggers/1/state", 0x5260, 0);
    addDirect(m, "awgs/0/userregs/0", 0x5060, 0);
    addDirect(m, "awgs/0/userregs/1", 0x5064, 0);
    addDirect(m, "awgs/0/userregs/10", 0x5088, 0);
    addDirect(m, "awgs/0/userregs/11", 0x508c, 0);
    addDirect(m, "awgs/0/userregs/12", 0x5090, 0);
    addDirect(m, "awgs/0/userregs/13", 0x5094, 0);
    addDirect(m, "awgs/0/userregs/14", 0x5098, 0);
    addDirect(m, "awgs/0/userregs/15", 0x509c, 0);
    addDirect(m, "awgs/0/userregs/2", 0x5068, 0);
    addDirect(m, "awgs/0/userregs/3", 0x506c, 0);
    addDirect(m, "awgs/0/userregs/4", 0x5070, 0);
    addDirect(m, "awgs/0/userregs/5", 0x5074, 0);
    addDirect(m, "awgs/0/userregs/6", 0x5078, 0);
    addDirect(m, "awgs/0/userregs/7", 0x507c, 0);
    addDirect(m, "awgs/0/userregs/8", 0x5080, 0);
    addDirect(m, "awgs/0/userregs/9", 0x5084, 0);
    addDirect(m, "oscs/0/freq", 0x1400, 4);
    addDirect(m, "qas/0/integration/length", 0x673c, 0);
    addDirect(m, "scopes/0/channel", 0x1a3c, 0);
    addDirect(m, "scopes/0/channels/0/bwlimit", 0x1a2c, 0);
    addDirect(m, "scopes/0/channels/0/inputselect", 0x1a28, 0);
    addDirect(m, "scopes/0/channels/0/limitlower", 0x1aa0, 2);
    addDirect(m, "scopes/0/channels/0/limitupper", 0x1aa4, 2);
    addDirect(m, "scopes/0/channels/1/bwlimit", 0x1a34, 0);
    addDirect(m, "scopes/0/channels/1/inputselect", 0x1a30, 0);
    addDirect(m, "scopes/0/channels/1/limitlower", 0x1aac, 2);
    addDirect(m, "scopes/0/channels/1/limitupper", 0x1ab0, 2);
    addDirect(m, "scopes/0/cont", 0x1a54, 0);
    addDirect(m, "scopes/0/enable", 0x1a00, 0);
    addDirect(m, "scopes/0/length", 0x1a18, 0);
    addDirect(m, "scopes/0/segments/count", 0x1a38, 0);
    addDirect(m, "scopes/0/segments/counts/enable", 0x1a6c, 0);
    addDirect(m, "scopes/0/single", 0x1a40, 0);
    addDirect(m, "scopes/0/stream/enables/0", 0xec00, 0);
    addDirect(m, "scopes/0/stream/enables/1", 0xec04, 0);
    addDirect(m, "scopes/0/stream/rate", 0xec08, 0);
    addDirect(m, "scopes/0/time", 0x1a04, 0);
    addDirect(m, "scopes/0/trigchannel", 0x1a24, 0);
    addDirect(m, "scopes/0/trigdelay", 0x1a8c, 2);
    addDirect(m, "scopes/0/trigenable", 0x1a08, 0);
    addDirect(m, "scopes/0/trigfalling", 0x1a10, 0);
    addDirect(m, "scopes/0/trigforce", 0x1a64, 0);
    addDirect(m, "scopes/0/triggate/enable", 0x1a80, 0);
    addDirect(m, "scopes/0/triggate/inputselect", 0x1a84, 0);
    addDirect(m, "scopes/0/trigholdoff", 0x1a1c, 2);
    addDirect(m, "scopes/0/trigholdofftrigger", 0x1a94, 0);
    addDirect(m, "scopes/0/trigholdofftriggerenable", 0x1a90, 0);
    addDirect(m, "scopes/0/trighysteresis/absolute", 0x1a50, 2);
    addDirect(m, "scopes/0/trighysteresis/mode", 0x1a60, 0);
    addDirect(m, "scopes/0/trighysteresis/relative", 0x1a5c, 2);
    addDirect(m, "scopes/0/triglevel", 0x1a14, 2);
    addDirect(m, "scopes/0/trigreference", 0x1a88, 2);
    addDirect(m, "scopes/0/trigrising", 0x1a0c, 0);
    addDirect(m, "scopes/0/trigstate", 0x1a78, 0);
    addDirect(m, "sigins/0/ac", 0x1d04, 0);
    addDirect(m, "sigins/0/autorange", 0x1d40, 0);
    addDirect(m, "sigins/0/bw", 0x1d20, 0);
    addDirect(m, "sigins/0/diff", 0x1d3c, 0);
    addDirect(m, "sigins/0/imp50", 0x1d08, 0);
    addDirect(m, "sigins/0/on", 0x1d24, 0);
    addDirect(m, "sigins/0/range", 0x1d28, 0);
    addDirect(m, "sigins/0/scaling", 0x1d44, 2);
    addDirect(m, "sigins/1/ac", 0x1d84, 0);
    addDirect(m, "sigins/1/autorange", 0x1dc0, 0);
    addDirect(m, "sigins/1/bw", 0x1da0, 0);
    addDirect(m, "sigins/1/diff", 0x1dbc, 0);
    addDirect(m, "sigins/1/imp50", 0x1d88, 0);
    addDirect(m, "sigins/1/on", 0x1da4, 0);
    addDirect(m, "sigins/1/range", 0x1da8, 0);
    addDirect(m, "sigins/1/scaling", 0x1dc4, 2);
    addDirect(m, "sigouts/0/amplitudes/0", 0x2004, 2);
    addDirect(m, "sigouts/0/amplitudes/1", 0x200c, 2);
    addDirect(m, "sigouts/0/amplitudes/2", 0x2014, 2);
    addDirect(m, "sigouts/0/amplitudes/3", 0x201c, 2);
    addDirect(m, "sigouts/0/amplitudes/4", 0x2024, 2);
    addDirect(m, "sigouts/0/amplitudes/5", 0x202c, 2);
    addDirect(m, "sigouts/0/amplitudes/6", 0x2034, 2);
    addDirect(m, "sigouts/0/amplitudes/7", 0x203c, 2);
    addDirect(m, "sigouts/0/autorange", 0x1e20, 0);
    addDirect(m, "sigouts/0/enables/0", 0x2000, 0);
    addDirect(m, "sigouts/0/enables/1", 0x2008, 0);
    addDirect(m, "sigouts/0/enables/2", 0x2010, 0);
    addDirect(m, "sigouts/0/enables/3", 0x2018, 0);
    addDirect(m, "sigouts/0/enables/4", 0x2020, 0);
    addDirect(m, "sigouts/0/enables/5", 0x2028, 0);
    addDirect(m, "sigouts/0/enables/6", 0x2030, 0);
    addDirect(m, "sigouts/0/enables/7", 0x2038, 0);
    addDirect(m, "sigouts/0/imp50", 0x1e14, 0);
    addDirect(m, "sigouts/0/offset", 0x1e30, 2);
    addDirect(m, "sigouts/0/on", 0x1e00, 0);
    addDirect(m, "sigouts/0/range", 0x1e04, 2);
    addDirect(m, "sigouts/1/amplitudes/0", 0x2044, 2);
    addDirect(m, "sigouts/1/amplitudes/1", 0x204c, 2);
    addDirect(m, "sigouts/1/amplitudes/2", 0x2054, 2);
    addDirect(m, "sigouts/1/amplitudes/3", 0x205c, 2);
    addDirect(m, "sigouts/1/amplitudes/4", 0x2064, 2);
    addDirect(m, "sigouts/1/amplitudes/5", 0x206c, 2);
    addDirect(m, "sigouts/1/amplitudes/6", 0x2074, 2);
    addDirect(m, "sigouts/1/amplitudes/7", 0x207c, 2);
    addDirect(m, "sigouts/1/autorange", 0x1e60, 0);
    addDirect(m, "sigouts/1/enables/0", 0x2040, 0);
    addDirect(m, "sigouts/1/enables/1", 0x2048, 0);
    addDirect(m, "sigouts/1/enables/2", 0x2050, 0);
    addDirect(m, "sigouts/1/enables/3", 0x2058, 0);
    addDirect(m, "sigouts/1/enables/4", 0x2060, 0);
    addDirect(m, "sigouts/1/enables/5", 0x2068, 0);
    addDirect(m, "sigouts/1/enables/6", 0x2070, 0);
    addDirect(m, "sigouts/1/enables/7", 0x2078, 0);
    addDirect(m, "sigouts/1/imp50", 0x1e54, 0);
    addDirect(m, "sigouts/1/offset", 0x1e70, 2);
    addDirect(m, "sigouts/1/on", 0x1e40, 0);
    addDirect(m, "sigouts/1/range", 0x1e44, 2);
    addDirect(m, "triggers/in/0/dcc_fedge", 0xd910, 0);
    addDirect(m, "triggers/in/0/imp50", 0xd900, 0);
    addDirect(m, "triggers/in/0/level", 0xd904, 0);
    addDirect(m, "triggers/in/1/dcc_fedge", 0xd930, 0);
    addDirect(m, "triggers/in/1/imp50", 0xd920, 0);
    addDirect(m, "triggers/in/1/level", 0xd924, 0);
    addDirect(m, "triggers/in/2/dcc_fedge", 0xd950, 0);
    addDirect(m, "triggers/in/2/imp50", 0xd940, 0);
    addDirect(m, "triggers/in/2/level", 0xd944, 0);
    addDirect(m, "triggers/in/3/dcc_fedge", 0xd970, 0);
    addDirect(m, "triggers/in/3/imp50", 0xd960, 0);
    addDirect(m, "triggers/in/3/level", 0xd964, 0);
    addDirect(m, "triggers/out/0/delay", 0xda14, 2);
    addDirect(m, "triggers/out/0/dir", 0xda04, 0);
    addDirect(m, "triggers/out/0/hold", 0xda10, 0);
    addDirect(m, "triggers/out/0/srcsel", 0xda00, 0);
    addDirect(m, "triggers/out/0/static_value", 0xda08, 0);
    addDirect(m, "triggers/out/1/delay", 0xda34, 2);
    addDirect(m, "triggers/out/1/dir", 0xda24, 0);
    addDirect(m, "triggers/out/1/hold", 0xda30, 0);
    addDirect(m, "triggers/out/1/srcsel", 0xda20, 0);
    addDirect(m, "triggers/out/1/static_value", 0xda28, 0);
    addDirect(m, "triggers/out/2/delay", 0xda54, 2);
    addDirect(m, "triggers/out/2/dir", 0xda44, 0);
    addDirect(m, "triggers/out/2/hold", 0xda50, 0);
    addDirect(m, "triggers/out/2/srcsel", 0xda40, 0);
    addDirect(m, "triggers/out/2/static_value", 0xda48, 0);
    addDirect(m, "triggers/out/3/delay", 0xda74, 2);
    addDirect(m, "triggers/out/3/dir", 0xda64, 0);
    addDirect(m, "triggers/out/3/hold", 0xda70, 0);
    addDirect(m, "triggers/out/3/srcsel", 0xda60, 0);
    addDirect(m, "triggers/out/3/static_value", 0xda68, 0);

    return nm;
}

// ---------------------------------------------------------------------------
// SHFQA — 24 entries @0x1ba3d0
// ---------------------------------------------------------------------------
template <>
std::unique_ptr<NodeMap> GetNodeMap<AwgDeviceType::SHFQA>::get() {  // @0x1ba3d0
    auto nm = std::make_unique<NodeMap>();
    auto& m = nm->entries_;

    addVirt(m, "qachannels/0/oscs/0/freq", "QAOSCFREQ", {0, 0}, 4);
    addVirt(m, "qachannels/0/oscs/1/freq", "QAOSCFREQ", {0, 1}, 4);
    addVirt(m, "qachannels/0/oscs/2/freq", "QAOSCFREQ", {0, 2}, 4);
    addVirt(m, "qachannels/0/oscs/3/freq", "QAOSCFREQ", {0, 3}, 4);
    addVirt(m, "qachannels/0/oscs/4/freq", "QAOSCFREQ", {0, 4}, 4);
    addVirt(m, "qachannels/0/oscs/5/freq", "QAOSCFREQ", {0, 5}, 4);
    addVirt(m, "qachannels/1/oscs/0/freq", "QAOSCFREQ", {1, 0}, 4);
    addVirt(m, "qachannels/1/oscs/1/freq", "QAOSCFREQ", {1, 1}, 4);
    addVirt(m, "qachannels/1/oscs/2/freq", "QAOSCFREQ", {1, 2}, 4);
    addVirt(m, "qachannels/1/oscs/3/freq", "QAOSCFREQ", {1, 3}, 4);
    addVirt(m, "qachannels/1/oscs/4/freq", "QAOSCFREQ", {1, 4}, 4);
    addVirt(m, "qachannels/1/oscs/5/freq", "QAOSCFREQ", {1, 5}, 4);
    addVirt(m, "qachannels/2/oscs/0/freq", "QAOSCFREQ", {2, 0}, 4);
    addVirt(m, "qachannels/2/oscs/1/freq", "QAOSCFREQ", {2, 1}, 4);
    addVirt(m, "qachannels/2/oscs/2/freq", "QAOSCFREQ", {2, 2}, 4);
    addVirt(m, "qachannels/2/oscs/3/freq", "QAOSCFREQ", {2, 3}, 4);
    addVirt(m, "qachannels/2/oscs/4/freq", "QAOSCFREQ", {2, 4}, 4);
    addVirt(m, "qachannels/2/oscs/5/freq", "QAOSCFREQ", {2, 5}, 4);
    addVirt(m, "qachannels/3/oscs/0/freq", "QAOSCFREQ", {3, 0}, 4);
    addVirt(m, "qachannels/3/oscs/1/freq", "QAOSCFREQ", {3, 1}, 4);
    addVirt(m, "qachannels/3/oscs/2/freq", "QAOSCFREQ", {3, 2}, 4);
    addVirt(m, "qachannels/3/oscs/3/freq", "QAOSCFREQ", {3, 3}, 4);
    addVirt(m, "qachannels/3/oscs/4/freq", "QAOSCFREQ", {3, 4}, 4);
    addVirt(m, "qachannels/3/oscs/5/freq", "QAOSCFREQ", {3, 5}, 4);

    // Note: binary's SHFQA node map does NOT include DIOOUTPUT.
    // setDIO's lookupNode("_/dios/0/output") throws; caught upstream.

    return nm;
}

// ---------------------------------------------------------------------------
// SHFSG — 72 entries @0x1bae40
// ---------------------------------------------------------------------------
template <>
std::unique_ptr<NodeMap> GetNodeMap<AwgDeviceType::SHFSG>::get() {  // @0x1bae40
    auto nm = std::make_unique<NodeMap>();
    auto& m = nm->entries_;

    addVirt(m, "sgchannels/0/oscs/0/freq", "SGOSCFREQ", {0, 0}, 4);
    addVirt(m, "sgchannels/0/oscs/1/freq", "SGOSCFREQ", {0, 1}, 4);
    addVirt(m, "sgchannels/0/oscs/2/freq", "SGOSCFREQ", {0, 2}, 4);
    addVirt(m, "sgchannels/0/oscs/3/freq", "SGOSCFREQ", {0, 3}, 4);
    addVirt(m, "sgchannels/0/oscs/4/freq", "SGOSCFREQ", {0, 4}, 4);
    addVirt(m, "sgchannels/0/oscs/5/freq", "SGOSCFREQ", {0, 5}, 4);
    addVirt(m, "sgchannels/0/oscs/6/freq", "SGOSCFREQ", {0, 6}, 4);
    addVirt(m, "sgchannels/0/oscs/7/freq", "SGOSCFREQ", {0, 7}, 4);
    addVirt(m, "sgchannels/0/sines/0/phaseshift", "SGSINEPHASE", {0, 0}, 5);
    addVirt(m, "sgchannels/1/oscs/0/freq", "SGOSCFREQ", {1, 0}, 4);
    addVirt(m, "sgchannels/1/oscs/1/freq", "SGOSCFREQ", {1, 1}, 4);
    addVirt(m, "sgchannels/1/oscs/2/freq", "SGOSCFREQ", {1, 2}, 4);
    addVirt(m, "sgchannels/1/oscs/3/freq", "SGOSCFREQ", {1, 3}, 4);
    addVirt(m, "sgchannels/1/oscs/4/freq", "SGOSCFREQ", {1, 4}, 4);
    addVirt(m, "sgchannels/1/oscs/5/freq", "SGOSCFREQ", {1, 5}, 4);
    addVirt(m, "sgchannels/1/oscs/6/freq", "SGOSCFREQ", {1, 6}, 4);
    addVirt(m, "sgchannels/1/oscs/7/freq", "SGOSCFREQ", {1, 7}, 4);
    addVirt(m, "sgchannels/1/sines/0/phaseshift", "SGSINEPHASE", {1, 0}, 5);
    addVirt(m, "sgchannels/2/oscs/0/freq", "SGOSCFREQ", {2, 0}, 4);
    addVirt(m, "sgchannels/2/oscs/1/freq", "SGOSCFREQ", {2, 1}, 4);
    addVirt(m, "sgchannels/2/oscs/2/freq", "SGOSCFREQ", {2, 2}, 4);
    addVirt(m, "sgchannels/2/oscs/3/freq", "SGOSCFREQ", {2, 3}, 4);
    addVirt(m, "sgchannels/2/oscs/4/freq", "SGOSCFREQ", {2, 4}, 4);
    addVirt(m, "sgchannels/2/oscs/5/freq", "SGOSCFREQ", {2, 5}, 4);
    addVirt(m, "sgchannels/2/oscs/6/freq", "SGOSCFREQ", {2, 6}, 4);
    addVirt(m, "sgchannels/2/oscs/7/freq", "SGOSCFREQ", {2, 7}, 4);
    addVirt(m, "sgchannels/2/sines/0/phaseshift", "SGSINEPHASE", {2, 0}, 5);
    addVirt(m, "sgchannels/3/oscs/0/freq", "SGOSCFREQ", {3, 0}, 4);
    addVirt(m, "sgchannels/3/oscs/1/freq", "SGOSCFREQ", {3, 1}, 4);
    addVirt(m, "sgchannels/3/oscs/2/freq", "SGOSCFREQ", {3, 2}, 4);
    addVirt(m, "sgchannels/3/oscs/3/freq", "SGOSCFREQ", {3, 3}, 4);
    addVirt(m, "sgchannels/3/oscs/4/freq", "SGOSCFREQ", {3, 4}, 4);
    addVirt(m, "sgchannels/3/oscs/5/freq", "SGOSCFREQ", {3, 5}, 4);
    addVirt(m, "sgchannels/3/oscs/6/freq", "SGOSCFREQ", {3, 6}, 4);
    addVirt(m, "sgchannels/3/oscs/7/freq", "SGOSCFREQ", {3, 7}, 4);
    addVirt(m, "sgchannels/3/sines/0/phaseshift", "SGSINEPHASE", {3, 0}, 5);
    addVirt(m, "sgchannels/4/oscs/0/freq", "SGOSCFREQ", {4, 0}, 4);
    addVirt(m, "sgchannels/4/oscs/1/freq", "SGOSCFREQ", {4, 1}, 4);
    addVirt(m, "sgchannels/4/oscs/2/freq", "SGOSCFREQ", {4, 2}, 4);
    addVirt(m, "sgchannels/4/oscs/3/freq", "SGOSCFREQ", {4, 3}, 4);
    addVirt(m, "sgchannels/4/oscs/4/freq", "SGOSCFREQ", {4, 4}, 4);
    addVirt(m, "sgchannels/4/oscs/5/freq", "SGOSCFREQ", {4, 5}, 4);
    addVirt(m, "sgchannels/4/oscs/6/freq", "SGOSCFREQ", {4, 6}, 4);
    addVirt(m, "sgchannels/4/oscs/7/freq", "SGOSCFREQ", {4, 7}, 4);
    addVirt(m, "sgchannels/4/sines/0/phaseshift", "SGSINEPHASE", {4, 0}, 5);
    addVirt(m, "sgchannels/5/oscs/0/freq", "SGOSCFREQ", {5, 0}, 4);
    addVirt(m, "sgchannels/5/oscs/1/freq", "SGOSCFREQ", {5, 1}, 4);
    addVirt(m, "sgchannels/5/oscs/2/freq", "SGOSCFREQ", {5, 2}, 4);
    addVirt(m, "sgchannels/5/oscs/3/freq", "SGOSCFREQ", {5, 3}, 4);
    addVirt(m, "sgchannels/5/oscs/4/freq", "SGOSCFREQ", {5, 4}, 4);
    addVirt(m, "sgchannels/5/oscs/5/freq", "SGOSCFREQ", {5, 5}, 4);
    addVirt(m, "sgchannels/5/oscs/6/freq", "SGOSCFREQ", {5, 6}, 4);
    addVirt(m, "sgchannels/5/oscs/7/freq", "SGOSCFREQ", {5, 7}, 4);
    addVirt(m, "sgchannels/5/sines/0/phaseshift", "SGSINEPHASE", {5, 0}, 5);
    addVirt(m, "sgchannels/6/oscs/0/freq", "SGOSCFREQ", {6, 0}, 4);
    addVirt(m, "sgchannels/6/oscs/1/freq", "SGOSCFREQ", {6, 1}, 4);
    addVirt(m, "sgchannels/6/oscs/2/freq", "SGOSCFREQ", {6, 2}, 4);
    addVirt(m, "sgchannels/6/oscs/3/freq", "SGOSCFREQ", {6, 3}, 4);
    addVirt(m, "sgchannels/6/oscs/4/freq", "SGOSCFREQ", {6, 4}, 4);
    addVirt(m, "sgchannels/6/oscs/5/freq", "SGOSCFREQ", {6, 5}, 4);
    addVirt(m, "sgchannels/6/oscs/6/freq", "SGOSCFREQ", {6, 6}, 4);
    addVirt(m, "sgchannels/6/oscs/7/freq", "SGOSCFREQ", {6, 7}, 4);
    addVirt(m, "sgchannels/6/sines/0/phaseshift", "SGSINEPHASE", {6, 0}, 5);
    addVirt(m, "sgchannels/7/oscs/0/freq", "SGOSCFREQ", {7, 0}, 4);
    addVirt(m, "sgchannels/7/oscs/1/freq", "SGOSCFREQ", {7, 1}, 4);
    addVirt(m, "sgchannels/7/oscs/2/freq", "SGOSCFREQ", {7, 2}, 4);
    addVirt(m, "sgchannels/7/oscs/3/freq", "SGOSCFREQ", {7, 3}, 4);
    addVirt(m, "sgchannels/7/oscs/4/freq", "SGOSCFREQ", {7, 4}, 4);
    addVirt(m, "sgchannels/7/oscs/5/freq", "SGOSCFREQ", {7, 5}, 4);
    addVirt(m, "sgchannels/7/oscs/6/freq", "SGOSCFREQ", {7, 6}, 4);
    addVirt(m, "sgchannels/7/oscs/7/freq", "SGOSCFREQ", {7, 7}, 4);
    addVirt(m, "sgchannels/7/sines/0/phaseshift", "SGSINEPHASE", {7, 0}, 5);

    return nm;
}

// ---------------------------------------------------------------------------
// SHFLI — 8 entries @0x1bbcb0
// ---------------------------------------------------------------------------
template <>
std::unique_ptr<NodeMap> GetNodeMap<AwgDeviceType::SHFLI>::get() {  // @0x1bbcb0
    auto nm = std::make_unique<NodeMap>();
    auto& m = nm->entries_;

    addVirt(m, "oscs/0/freq", "LIOSCFREQ", {0}, 4);
    addVirt(m, "oscs/1/freq", "LIOSCFREQ", {1}, 4);
    addVirt(m, "oscs/2/freq", "LIOSCFREQ", {2}, 4);
    addVirt(m, "oscs/3/freq", "LIOSCFREQ", {3}, 4);
    addVirt(m, "oscs/4/freq", "LIOSCFREQ", {4}, 4);
    addVirt(m, "oscs/5/freq", "LIOSCFREQ", {5}, 4);
    addVirt(m, "oscs/6/freq", "LIOSCFREQ", {6}, 4);
    addVirt(m, "oscs/7/freq", "LIOSCFREQ", {7}, 4);

    return nm;
}

// ---------------------------------------------------------------------------
// GHFLI — 8 entries @0x1bc030
// ---------------------------------------------------------------------------
template <>
std::unique_ptr<NodeMap> GetNodeMap<AwgDeviceType::GHFLI>::get() {  // @0x1bc030
    auto nm = std::make_unique<NodeMap>();
    auto& m = nm->entries_;

    addVirt(m, "oscs/0/freq", "LIOSCFREQ", {0}, 4);
    addVirt(m, "oscs/1/freq", "LIOSCFREQ", {1}, 4);
    addVirt(m, "oscs/2/freq", "LIOSCFREQ", {2}, 4);
    addVirt(m, "oscs/3/freq", "LIOSCFREQ", {3}, 4);
    addVirt(m, "oscs/4/freq", "LIOSCFREQ", {4}, 4);
    addVirt(m, "oscs/5/freq", "LIOSCFREQ", {5}, 4);
    addVirt(m, "oscs/6/freq", "LIOSCFREQ", {6}, 4);
    addVirt(m, "oscs/7/freq", "LIOSCFREQ", {7}, 4);

    return nm;
}

// ---------------------------------------------------------------------------
// VHFLI — 8 entries @0x1bc3b0
// ---------------------------------------------------------------------------
template <>
std::unique_ptr<NodeMap> GetNodeMap<AwgDeviceType::VHFLI>::get() {  // @0x1bc3b0
    auto nm = std::make_unique<NodeMap>();
    auto& m = nm->entries_;

    addVirt(m, "oscs/0/freq", "LIOSCFREQ", {0}, 4);
    addVirt(m, "oscs/1/freq", "LIOSCFREQ", {1}, 4);
    addVirt(m, "oscs/2/freq", "LIOSCFREQ", {2}, 4);
    addVirt(m, "oscs/3/freq", "LIOSCFREQ", {3}, 4);
    addVirt(m, "oscs/4/freq", "LIOSCFREQ", {4}, 4);
    addVirt(m, "oscs/5/freq", "LIOSCFREQ", {5}, 4);
    addVirt(m, "oscs/6/freq", "LIOSCFREQ", {6}, 4);
    addVirt(m, "oscs/7/freq", "LIOSCFREQ", {7}, 4);

    return nm;
}

// ---------------------------------------------------------------------------
// GetNodeMapDispatcher — dispatches to GetNodeMap<N>::get() for N > 4
// Template: GetNodeMapDispatcher<integer_sequence<AwgDeviceType, 8,16,32,64,128,256>>
// ---------------------------------------------------------------------------
namespace {

std::unique_ptr<NodeMap> dispatchHighDevType(AwgDeviceType devType) {  // @0x1ba360
    switch (static_cast<int>(devType)) {
        case 8:   return GetNodeMap<AwgDeviceType::SHFQA>::get();
        case 16:  return GetNodeMap<AwgDeviceType::SHFSG>::get();
        // case 32: SHFQC_SG — no GetNodeMap specialization in binary
        case 64:  return GetNodeMap<AwgDeviceType::SHFLI>::get();
        case 128: return GetNodeMap<AwgDeviceType::GHFLI>::get();
        case 256: return GetNodeMap<AwgDeviceType::VHFLI>::get();
        default:  return GetNodeMap<AwgDeviceType::VHFLI>::get();  // fallthrough in binary
    }
}

} // anonymous namespace

// Full dispatch matching initNodeMap @0x16b740 switch table.
std::unique_ptr<NodeMap> getNodeMapForDevice(AwgDeviceType devType) {
    switch (static_cast<int>(devType)) {
        case 0:  return nullptr;
        case 1:  return GetNodeMap<AwgDeviceType::UHFLI>::get();
        case 2:  return GetNodeMap<AwgDeviceType::HDAWG>::get();
        case 4:  return GetNodeMap<AwgDeviceType::UHFQA>::get();
        default: return dispatchHighDevType(devType);
    }
}

// Explicit instantiations (all 8 specializations defined above)
template class GetNodeMap<AwgDeviceType::UHFLI>;
template class GetNodeMap<AwgDeviceType::HDAWG>;
template class GetNodeMap<AwgDeviceType::UHFQA>;
template class GetNodeMap<AwgDeviceType::SHFQA>;
template class GetNodeMap<AwgDeviceType::SHFSG>;
template class GetNodeMap<AwgDeviceType::SHFLI>;
template class GetNodeMap<AwgDeviceType::GHFLI>;
template class GetNodeMap<AwgDeviceType::VHFLI>;

} // namespace zhinst

