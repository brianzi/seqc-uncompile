// get_node_map.cpp — GetNodeMap<AwgDeviceType>::get() specializations
// Auto-extracted from _seqc_compiler.so binary data tables.
//
// 8 device-specific node maps totalling 1081 entries.
// UHFLI @0x1948d0, HDAWG @0x1ad9a0, UHFQA @0x1b1470, SHFQA @0x1ba3d0,
// SHFSG @0x1bae40, SHFLI @0x1bbcb0, GHFLI @0x1bc030, VHFLI @0x1bc3b0.
// Dispatcher @0x1ba360, initNodeMap @0x16b740.

#include "zhinst/runtime/custom_functions.hpp"
#include "zhinst/runtime/node_map_data.hpp"
#include "zhinst/core/types.hpp"

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

void addDirect(Map& m, const char* key, uint32_t addr, NodeTypeIdx typeIdx,
               bool hasFast = false, uint32_t fastAddr = 0) {
    auto* data = new DirectAddrNodeMapData;
    data->addr_ = addr;
    NodeMapItem item{};
    item.data = data;
    item.typeIdx = typeIdx;
    item.hasFast = hasFast;
    item.fastAddr = fastAddr;
    m[key] = item;
}

void addVirt(Map& m, const char* key, const char* name,
             std::vector<int32_t> addresses, NodeTypeIdx typeIdx,
             bool hasFast = false, uint32_t fastAddr = 0) {
    auto* data = new VirtAddrNodeMapData;
    data->name_.assign(name);
    data->addresses_ = std::move(addresses);
    NodeMapItem item{};
    item.data = data;
    item.typeIdx = typeIdx;
    item.hasFast = hasFast;
    item.fastAddr = fastAddr;
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

    addDirect(m, "_/dios/0/output", 0x1c0c, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "aucarts/0/enable", 0xe804, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "aucarts/0/mode", 0xe800, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "aucarts/0/ops/0/coeff", 0xe818, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "aucarts/0/ops/0/demodselect", 0xe810, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "aucarts/0/ops/0/scale", 0xe81c, NodeTypeIdx::FloatBits);
    addDirect(m, "aucarts/0/ops/0/value", 0xe814, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "aucarts/0/ops/1/coeff", 0xe828, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "aucarts/0/ops/1/demodselect", 0xe820, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "aucarts/0/ops/1/scale", 0xe82c, NodeTypeIdx::FloatBits);
    addDirect(m, "aucarts/0/ops/1/value", 0xe824, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "aucarts/0/rate", 0xe808, NodeTypeIdx::FloatBits);
    addDirect(m, "aucarts/1/enable", 0xe844, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "aucarts/1/mode", 0xe840, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "aucarts/1/ops/0/coeff", 0xe858, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "aucarts/1/ops/0/demodselect", 0xe850, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "aucarts/1/ops/0/scale", 0xe85c, NodeTypeIdx::FloatBits);
    addDirect(m, "aucarts/1/ops/0/value", 0xe854, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "aucarts/1/ops/1/coeff", 0xe868, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "aucarts/1/ops/1/demodselect", 0xe860, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "aucarts/1/ops/1/scale", 0xe86c, NodeTypeIdx::FloatBits);
    addDirect(m, "aucarts/1/ops/1/value", 0xe864, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "aucarts/1/rate", 0xe848, NodeTypeIdx::FloatBits);
    addDirect(m, "aupolars/0/enable", 0xea04, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "aupolars/0/flags", 0xea34, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "aupolars/0/mode", 0xea00, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "aupolars/0/ops/0/coeff", 0xea18, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "aupolars/0/ops/0/demodselect", 0xea10, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "aupolars/0/ops/0/scale", 0xea1c, NodeTypeIdx::FloatBits);
    addDirect(m, "aupolars/0/ops/0/value", 0xea14, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "aupolars/0/ops/1/coeff", 0xea28, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "aupolars/0/ops/1/demodselect", 0xea20, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "aupolars/0/ops/1/scale", 0xea2c, NodeTypeIdx::FloatBits);
    addDirect(m, "aupolars/0/ops/1/value", 0xea24, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "aupolars/0/rate", 0xea08, NodeTypeIdx::FloatBits);
    addDirect(m, "aupolars/0/reserved1", 0xea0c, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "aupolars/1/enable", 0xea44, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "aupolars/1/flags", 0xea74, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "aupolars/1/mode", 0xea40, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "aupolars/1/ops/0/coeff", 0xea58, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "aupolars/1/ops/0/demodselect", 0xea50, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "aupolars/1/ops/0/scale", 0xea5c, NodeTypeIdx::FloatBits);
    addDirect(m, "aupolars/1/ops/0/value", 0xea54, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "aupolars/1/ops/1/coeff", 0xea68, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "aupolars/1/ops/1/demodselect", 0xea60, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "aupolars/1/ops/1/scale", 0xea6c, NodeTypeIdx::FloatBits);
    addDirect(m, "aupolars/1/ops/1/value", 0xea64, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "aupolars/1/rate", 0xea48, NodeTypeIdx::FloatBits);
    addDirect(m, "aupolars/1/reserved1", 0xea4c, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "auxouts/0/demodselect", 0xd738, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "auxouts/0/limitlower", 0xd720, NodeTypeIdx::FloatBits);
    addDirect(m, "auxouts/0/limitupper", 0xd724, NodeTypeIdx::FloatBits);
    addDirect(m, "auxouts/0/offset", 0xd708, NodeTypeIdx::FloatBits);
    addDirect(m, "auxouts/0/outputselect", 0xd734, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "auxouts/0/preoffset", 0xd700, NodeTypeIdx::RawDoubleLow32);
    addDirect(m, "auxouts/0/scale", 0xd710, NodeTypeIdx::FloatBits);
    addDirect(m, "auxouts/1/demodselect", 0xd778, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "auxouts/1/limitlower", 0xd760, NodeTypeIdx::FloatBits);
    addDirect(m, "auxouts/1/limitupper", 0xd764, NodeTypeIdx::FloatBits);
    addDirect(m, "auxouts/1/offset", 0xd748, NodeTypeIdx::FloatBits);
    addDirect(m, "auxouts/1/outputselect", 0xd774, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "auxouts/1/preoffset", 0xd740, NodeTypeIdx::RawDoubleLow32);
    addDirect(m, "auxouts/1/scale", 0xd750, NodeTypeIdx::FloatBits);
    addDirect(m, "auxouts/2/demodselect", 0xd7b8, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "auxouts/2/limitlower", 0xd7a0, NodeTypeIdx::FloatBits);
    addDirect(m, "auxouts/2/limitupper", 0xd7a4, NodeTypeIdx::FloatBits);
    addDirect(m, "auxouts/2/offset", 0xd788, NodeTypeIdx::FloatBits);
    addDirect(m, "auxouts/2/outputselect", 0xd7b4, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "auxouts/2/preoffset", 0xd780, NodeTypeIdx::RawDoubleLow32);
    addDirect(m, "auxouts/2/scale", 0xd790, NodeTypeIdx::FloatBits);
    addDirect(m, "auxouts/3/demodselect", 0xd7f8, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "auxouts/3/limitlower", 0xd7e0, NodeTypeIdx::FloatBits);
    addDirect(m, "auxouts/3/limitupper", 0xd7e4, NodeTypeIdx::FloatBits);
    addDirect(m, "auxouts/3/offset", 0xd7c8, NodeTypeIdx::FloatBits);
    addDirect(m, "auxouts/3/outputselect", 0xd7f4, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "auxouts/3/preoffset", 0xd7c0, NodeTypeIdx::RawDoubleLow32);
    addDirect(m, "auxouts/3/scale", 0xd7d0, NodeTypeIdx::FloatBits);
    addDirect(m, "awgs/0/auxtriggers/0/channel", 0x5300, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "awgs/0/auxtriggers/0/falling", 0x5308, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "awgs/0/auxtriggers/0/rising", 0x5304, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "awgs/0/auxtriggers/0/state", 0x530c, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "awgs/0/auxtriggers/1/channel", 0x5310, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "awgs/0/auxtriggers/1/falling", 0x5318, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "awgs/0/auxtriggers/1/rising", 0x5314, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "awgs/0/auxtriggers/1/state", 0x531c, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "awgs/0/enable", 0x5000, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "awgs/0/outputs/0/amplitude", 0x5500, NodeTypeIdx::FloatBits);
    addDirect(m, "awgs/0/outputs/0/mode", 0x5504, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "awgs/0/outputs/1/amplitude", 0x5540, NodeTypeIdx::FloatBits);
    addDirect(m, "awgs/0/outputs/1/mode", 0x5544, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "awgs/0/single", 0x5004, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "awgs/0/time", 0x5008, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "awgs/0/triggers/0/channel", 0x5218, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "awgs/0/triggers/0/falling", 0x5214, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "awgs/0/triggers/0/force", 0x521c, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "awgs/0/triggers/0/gate/enable", 0x5224, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "awgs/0/triggers/0/gate/inputselect", 0x5228, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "awgs/0/triggers/0/hysteresis/absolute", 0x5204, NodeTypeIdx::FloatBits);
    addDirect(m, "awgs/0/triggers/0/hysteresis/mode", 0x520c, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "awgs/0/triggers/0/hysteresis/relative", 0x5208, NodeTypeIdx::FloatBits);
    addDirect(m, "awgs/0/triggers/0/level", 0x5200, NodeTypeIdx::FloatBits);
    addDirect(m, "awgs/0/triggers/0/rising", 0x5210, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "awgs/0/triggers/0/state", 0x5220, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "awgs/0/triggers/1/channel", 0x5258, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "awgs/0/triggers/1/falling", 0x5254, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "awgs/0/triggers/1/force", 0x525c, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "awgs/0/triggers/1/gate/enable", 0x5264, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "awgs/0/triggers/1/gate/inputselect", 0x5268, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "awgs/0/triggers/1/hysteresis/absolute", 0x5244, NodeTypeIdx::FloatBits);
    addDirect(m, "awgs/0/triggers/1/hysteresis/mode", 0x524c, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "awgs/0/triggers/1/hysteresis/relative", 0x5248, NodeTypeIdx::FloatBits);
    addDirect(m, "awgs/0/triggers/1/level", 0x5240, NodeTypeIdx::FloatBits);
    addDirect(m, "awgs/0/triggers/1/rising", 0x5250, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "awgs/0/triggers/1/state", 0x5260, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "awgs/0/userregs/0", 0x5060, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "awgs/0/userregs/1", 0x5064, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "awgs/0/userregs/10", 0x5088, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "awgs/0/userregs/11", 0x508c, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "awgs/0/userregs/12", 0x5090, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "awgs/0/userregs/13", 0x5094, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "awgs/0/userregs/14", 0x5098, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "awgs/0/userregs/15", 0x509c, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "awgs/0/userregs/2", 0x5068, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "awgs/0/userregs/3", 0x506c, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "awgs/0/userregs/4", 0x5070, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "awgs/0/userregs/5", 0x5074, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "awgs/0/userregs/6", 0x5078, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "awgs/0/userregs/7", 0x507c, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "awgs/0/userregs/8", 0x5080, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "awgs/0/userregs/9", 0x5084, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "boxcars/0/baseline/enable", 0x4230, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "boxcars/0/baseline/windowstart", 0x4234, NodeTypeIdx::FloatBits);
    addDirect(m, "boxcars/0/enable", 0x4200, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "boxcars/0/insel", 0x4218, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "boxcars/0/limitrate", 0x422c, NodeTypeIdx::FloatBits);
    addDirect(m, "boxcars/0/oscsel", 0x421c, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "boxcars/0/periods", 0x4204, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "boxcars/0/windowsize", 0x423c, NodeTypeIdx::FloatBits);
    addDirect(m, "boxcars/0/windowstart", 0x4208, NodeTypeIdx::FloatBits);
    addDirect(m, "boxcars/1/baseline/enable", 0x42b0, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "boxcars/1/baseline/windowstart", 0x42b4, NodeTypeIdx::FloatBits);
    addDirect(m, "boxcars/1/enable", 0x4280, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "boxcars/1/insel", 0x4298, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "boxcars/1/limitrate", 0x42ac, NodeTypeIdx::FloatBits);
    addDirect(m, "boxcars/1/oscsel", 0x429c, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "boxcars/1/periods", 0x4284, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "boxcars/1/windowsize", 0x42bc, NodeTypeIdx::FloatBits);
    addDirect(m, "boxcars/1/windowstart", 0x4288, NodeTypeIdx::FloatBits);
    addDirect(m, "demods/0/adcselect", 0x1500, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "demods/0/bypass", 0x1520, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "demods/0/enable", 0x1528, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "demods/0/harmonic", 0x1514, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "demods/0/order", 0x1504, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "demods/0/oscselect", 0x1510, NodeTypeIdx::IntegerPassthrough, true, 0x30000040);
    addDirect(m, "demods/0/phaseshift", 0x1518, NodeTypeIdx::IntegerPassthrough, true, 0x3000008c);
    addDirect(m, "demods/0/rate", 0x150c, NodeTypeIdx::FloatBits);
    addDirect(m, "demods/0/sinc", 0x151c, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "demods/0/timeconstant", 0x1524, NodeTypeIdx::FloatBits);
    addDirect(m, "demods/1/adcselect", 0x1580, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "demods/1/bypass", 0x15a0, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "demods/1/enable", 0x15a8, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "demods/1/harmonic", 0x1594, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "demods/1/order", 0x1584, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "demods/1/oscselect", 0x1590, NodeTypeIdx::IntegerPassthrough, true, 0x30000044);
    addDirect(m, "demods/1/phaseshift", 0x1598, NodeTypeIdx::IntegerPassthrough, true, 0x300000cc);
    addDirect(m, "demods/1/rate", 0x158c, NodeTypeIdx::FloatBits);
    addDirect(m, "demods/1/sinc", 0x159c, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "demods/1/timeconstant", 0x15a4, NodeTypeIdx::FloatBits);
    addDirect(m, "demods/2/adcselect", 0x1600, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "demods/2/bypass", 0x1620, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "demods/2/enable", 0x1628, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "demods/2/harmonic", 0x1614, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "demods/2/order", 0x1604, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "demods/2/oscselect", 0x1610, NodeTypeIdx::IntegerPassthrough, true, 0x30000048);
    addDirect(m, "demods/2/phaseshift", 0x1618, NodeTypeIdx::IntegerPassthrough, true, 0x3000010c);
    addDirect(m, "demods/2/rate", 0x160c, NodeTypeIdx::FloatBits);
    addDirect(m, "demods/2/sinc", 0x161c, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "demods/2/timeconstant", 0x1624, NodeTypeIdx::FloatBits);
    addDirect(m, "demods/3/adcselect", 0x1680, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "demods/3/bypass", 0x16a0, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "demods/3/enable", 0x16a8, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "demods/3/harmonic", 0x1694, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "demods/3/order", 0x1684, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "demods/3/oscselect", 0x1690, NodeTypeIdx::IntegerPassthrough, true, 0x3000004c);
    addDirect(m, "demods/3/phaseshift", 0x1698, NodeTypeIdx::IntegerPassthrough, true, 0x3000014c);
    addDirect(m, "demods/3/rate", 0x168c, NodeTypeIdx::FloatBits);
    addDirect(m, "demods/3/sinc", 0x169c, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "demods/3/timeconstant", 0x16a4, NodeTypeIdx::FloatBits);
    addDirect(m, "demods/4/adcselect", 0x1700, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "demods/4/bypass", 0x1720, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "demods/4/enable", 0x1728, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "demods/4/harmonic", 0x1714, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "demods/4/order", 0x1704, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "demods/4/oscselect", 0x1710, NodeTypeIdx::IntegerPassthrough, true, 0x30000050);
    addDirect(m, "demods/4/phaseshift", 0x1718, NodeTypeIdx::IntegerPassthrough, true, 0x3000018c);
    addDirect(m, "demods/4/rate", 0x170c, NodeTypeIdx::FloatBits);
    addDirect(m, "demods/4/sinc", 0x171c, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "demods/4/timeconstant", 0x1724, NodeTypeIdx::FloatBits);
    addDirect(m, "demods/5/adcselect", 0x1780, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "demods/5/bypass", 0x17a0, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "demods/5/enable", 0x17a8, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "demods/5/harmonic", 0x1794, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "demods/5/order", 0x1784, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "demods/5/oscselect", 0x1790, NodeTypeIdx::IntegerPassthrough, true, 0x30000054);
    addDirect(m, "demods/5/phaseshift", 0x1798, NodeTypeIdx::IntegerPassthrough, true, 0x300001cc);
    addDirect(m, "demods/5/rate", 0x178c, NodeTypeIdx::FloatBits);
    addDirect(m, "demods/5/sinc", 0x179c, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "demods/5/timeconstant", 0x17a4, NodeTypeIdx::FloatBits);
    addDirect(m, "demods/6/adcselect", 0x1800, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "demods/6/bypass", 0x1820, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "demods/6/enable", 0x1828, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "demods/6/harmonic", 0x1814, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "demods/6/order", 0x1804, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "demods/6/oscselect", 0x1810, NodeTypeIdx::IntegerPassthrough, true, 0x30000058);
    addDirect(m, "demods/6/phaseshift", 0x1818, NodeTypeIdx::IntegerPassthrough, true, 0x3000020c);
    addDirect(m, "demods/6/rate", 0x180c, NodeTypeIdx::FloatBits);
    addDirect(m, "demods/6/sinc", 0x181c, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "demods/6/timeconstant", 0x1824, NodeTypeIdx::FloatBits);
    addDirect(m, "demods/7/adcselect", 0x1880, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "demods/7/bypass", 0x18a0, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "demods/7/enable", 0x18a8, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "demods/7/harmonic", 0x1894, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "demods/7/order", 0x1884, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "demods/7/oscselect", 0x1890, NodeTypeIdx::IntegerPassthrough, true, 0x3000005c);
    addDirect(m, "demods/7/phaseshift", 0x1898, NodeTypeIdx::IntegerPassthrough, true, 0x3000024c);
    addDirect(m, "demods/7/rate", 0x188c, NodeTypeIdx::FloatBits);
    addDirect(m, "demods/7/sinc", 0x189c, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "demods/7/timeconstant", 0x18a4, NodeTypeIdx::FloatBits);
    addDirect(m, "inputpwas/0/enable", 0x4400, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "inputpwas/0/harmonic", 0x4414, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "inputpwas/0/holdoff", 0x4428, NodeTypeIdx::FloatBits);
    addDirect(m, "inputpwas/0/insel", 0x440c, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "inputpwas/0/mode", 0x4410, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "inputpwas/0/oscselect", 0x4408, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "inputpwas/0/samplecount", 0x4420, NodeTypeIdx::SinePair);
    addDirect(m, "inputpwas/0/shift", 0x4418, NodeTypeIdx::FloatBits);
    addDirect(m, "inputpwas/0/single", 0x4404, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "inputpwas/0/termination", 0x441c, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "inputpwas/1/enable", 0x4480, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "inputpwas/1/harmonic", 0x4494, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "inputpwas/1/holdoff", 0x44a8, NodeTypeIdx::FloatBits);
    addDirect(m, "inputpwas/1/insel", 0x448c, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "inputpwas/1/mode", 0x4490, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "inputpwas/1/oscselect", 0x4488, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "inputpwas/1/samplecount", 0x44a0, NodeTypeIdx::SinePair);
    addDirect(m, "inputpwas/1/shift", 0x4498, NodeTypeIdx::FloatBits);
    addDirect(m, "inputpwas/1/single", 0x4484, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "inputpwas/1/termination", 0x449c, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "mods/0/carrier/harmonic", 0x2260, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "mods/0/carrier/inputselect", 0x2230, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "mods/0/carrier/order", 0x223c, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "mods/0/carrier/oscselect", 0x2254, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "mods/0/carrier/phaseshift", 0x226c, NodeTypeIdx::FloatBits);
    addDirect(m, "mods/0/carrier/timeconstant", 0x2248, NodeTypeIdx::FloatBits);
    addDirect(m, "mods/0/carrier_enable", 0x2278, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "mods/0/carrier_gain", 0x2284, NodeTypeIdx::FloatBits);
    addDirect(m, "mods/0/enable", 0x2200, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "mods/0/freqdev", 0x2218, NodeTypeIdx::FloatBits);
    addDirect(m, "mods/0/freqdevenable", 0x2214, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "mods/0/index", 0x221c, NodeTypeIdx::FloatBits);
    addDirect(m, "mods/0/mode", 0x2208, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "mods/0/operation", 0x2224, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "mods/0/output", 0x2204, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "mods/0/rate", 0x2228, NodeTypeIdx::FloatBits);
    addDirect(m, "mods/0/sampleenable", 0x2220, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "mods/0/sideband0_gain", 0x2288, NodeTypeIdx::FloatBits);
    addDirect(m, "mods/0/sideband1_gain", 0x228c, NodeTypeIdx::FloatBits);
    addDirect(m, "mods/0/sidebands/0/enable", 0x227c, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "mods/0/sidebands/0/harmonic", 0x2264, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "mods/0/sidebands/0/inputselect", 0x2234, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "mods/0/sidebands/0/mode", 0x220c, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "mods/0/sidebands/0/order", 0x2240, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "mods/0/sidebands/0/oscselect", 0x2258, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "mods/0/sidebands/0/phaseshift", 0x2270, NodeTypeIdx::FloatBits);
    addDirect(m, "mods/0/sidebands/0/timeconstant", 0x224c, NodeTypeIdx::FloatBits);
    addDirect(m, "mods/0/sidebands/1/enable", 0x2280, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "mods/0/sidebands/1/harmonic", 0x2268, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "mods/0/sidebands/1/inputselect", 0x2238, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "mods/0/sidebands/1/mode", 0x2210, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "mods/0/sidebands/1/order", 0x2244, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "mods/0/sidebands/1/oscselect", 0x225c, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "mods/0/sidebands/1/phaseshift", 0x2274, NodeTypeIdx::FloatBits);
    addDirect(m, "mods/0/sidebands/1/timeconstant", 0x2250, NodeTypeIdx::FloatBits);
    addDirect(m, "mods/0/trigger", 0x222c, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "mods/1/carrier/harmonic", 0x2360, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "mods/1/carrier/inputselect", 0x2330, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "mods/1/carrier/order", 0x233c, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "mods/1/carrier/oscselect", 0x2354, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "mods/1/carrier/phaseshift", 0x236c, NodeTypeIdx::FloatBits);
    addDirect(m, "mods/1/carrier/timeconstant", 0x2348, NodeTypeIdx::FloatBits);
    addDirect(m, "mods/1/carrier_enable", 0x2378, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "mods/1/carrier_gain", 0x2384, NodeTypeIdx::FloatBits);
    addDirect(m, "mods/1/enable", 0x2300, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "mods/1/freqdev", 0x2318, NodeTypeIdx::FloatBits);
    addDirect(m, "mods/1/freqdevenable", 0x2314, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "mods/1/index", 0x231c, NodeTypeIdx::FloatBits);
    addDirect(m, "mods/1/mode", 0x2308, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "mods/1/operation", 0x2324, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "mods/1/output", 0x2304, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "mods/1/rate", 0x2328, NodeTypeIdx::FloatBits);
    addDirect(m, "mods/1/sampleenable", 0x2320, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "mods/1/sideband0_gain", 0x2388, NodeTypeIdx::FloatBits);
    addDirect(m, "mods/1/sideband1_gain", 0x238c, NodeTypeIdx::FloatBits);
    addDirect(m, "mods/1/sidebands/0/enable", 0x237c, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "mods/1/sidebands/0/harmonic", 0x2364, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "mods/1/sidebands/0/inputselect", 0x2334, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "mods/1/sidebands/0/mode", 0x230c, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "mods/1/sidebands/0/order", 0x2340, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "mods/1/sidebands/0/oscselect", 0x2358, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "mods/1/sidebands/0/phaseshift", 0x2370, NodeTypeIdx::FloatBits);
    addDirect(m, "mods/1/sidebands/0/timeconstant", 0x234c, NodeTypeIdx::FloatBits);
    addDirect(m, "mods/1/sidebands/1/enable", 0x2380, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "mods/1/sidebands/1/harmonic", 0x2368, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "mods/1/sidebands/1/inputselect", 0x2338, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "mods/1/sidebands/1/mode", 0x2310, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "mods/1/sidebands/1/order", 0x2344, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "mods/1/sidebands/1/oscselect", 0x235c, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "mods/1/sidebands/1/phaseshift", 0x2374, NodeTypeIdx::FloatBits);
    addDirect(m, "mods/1/sidebands/1/timeconstant", 0x2350, NodeTypeIdx::FloatBits);
    addDirect(m, "mods/1/trigger", 0x232c, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "oscs/0/freq", 0x1400, NodeTypeIdx::Frequency, true, 0x00);
    addDirect(m, "oscs/1/freq", 0x1408, NodeTypeIdx::Frequency, true, 0x08);
    addDirect(m, "oscs/2/freq", 0x1410, NodeTypeIdx::Frequency, true, 0x10);
    addDirect(m, "oscs/3/freq", 0x1418, NodeTypeIdx::Frequency, true, 0x18);
    addDirect(m, "oscs/4/freq", 0x1420, NodeTypeIdx::Frequency, true, 0x20);
    addDirect(m, "oscs/5/freq", 0x1428, NodeTypeIdx::Frequency, true, 0x28);
    addDirect(m, "oscs/6/freq", 0x1430, NodeTypeIdx::Frequency, true, 0x30);
    addDirect(m, "oscs/7/freq", 0x1438, NodeTypeIdx::Frequency, true, 0x38);
    addDirect(m, "outputpwas/0/enable", 0x4300, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "outputpwas/0/harmonic", 0x4314, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "outputpwas/0/holdoff", 0x4328, NodeTypeIdx::FloatBits);
    addDirect(m, "outputpwas/0/insel", 0x430c, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "outputpwas/0/mode", 0x4310, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "outputpwas/0/oscselect", 0x4308, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "outputpwas/0/progress", 0x432c, NodeTypeIdx::FloatBits);
    addDirect(m, "outputpwas/0/samplecount", 0x4320, NodeTypeIdx::SinePair);
    addDirect(m, "outputpwas/0/shift", 0x4318, NodeTypeIdx::FloatBits);
    addDirect(m, "outputpwas/0/single", 0x4304, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "outputpwas/0/status", 0x4330, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "outputpwas/0/termination", 0x431c, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "outputpwas/1/enable", 0x4380, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "outputpwas/1/harmonic", 0x4394, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "outputpwas/1/holdoff", 0x43a8, NodeTypeIdx::FloatBits);
    addDirect(m, "outputpwas/1/insel", 0x438c, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "outputpwas/1/mode", 0x4390, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "outputpwas/1/oscselect", 0x4388, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "outputpwas/1/progress", 0x43ac, NodeTypeIdx::FloatBits);
    addDirect(m, "outputpwas/1/samplecount", 0x43a0, NodeTypeIdx::SinePair);
    addDirect(m, "outputpwas/1/shift", 0x4398, NodeTypeIdx::FloatBits);
    addDirect(m, "outputpwas/1/single", 0x4384, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "outputpwas/1/status", 0x43b0, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "outputpwas/1/termination", 0x439c, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "pids/0/center", 0x4874, NodeTypeIdx::RawDoubleLow32);
    addDirect(m, "pids/0/d", 0x4828, NodeTypeIdx::FloatBits);
    addDirect(m, "pids/0/demod/adcselect", 0x4804, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "pids/0/demod/harmonic", 0x482c, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "pids/0/demod/order", 0x4808, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "pids/0/demod/timeconstant", 0x480c, NodeTypeIdx::FloatBits);
    addDirect(m, "pids/0/dlimittimeconstant", 0x4814, NodeTypeIdx::FloatBits);
    addDirect(m, "pids/0/enable", 0x4800, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "pids/0/i", 0x4824, NodeTypeIdx::FloatBits);
    addDirect(m, "pids/0/inputchannel", 0x4870, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "pids/0/inputsource", 0x486c, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "pids/0/limitlower", 0x481c, NodeTypeIdx::FloatBits);
    addDirect(m, "pids/0/limitupper", 0x4818, NodeTypeIdx::FloatBits);
    addDirect(m, "pids/0/mode", 0x4868, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "pids/0/output", 0x4838, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "pids/0/outputchannel", 0x4834, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "pids/0/p", 0x4820, NodeTypeIdx::FloatBits);
    addDirect(m, "pids/0/phaseunwrap", 0x4848, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "pids/0/pll/automode", 0x4894, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "pids/0/rate", 0x4864, NodeTypeIdx::FloatBits);
    addDirect(m, "pids/0/setpoint", 0x4810, NodeTypeIdx::FloatBits);
    addDirect(m, "pids/0/stream/rate", 0x485c, NodeTypeIdx::FloatBits);
    addDirect(m, "pids/1/center", 0x4974, NodeTypeIdx::RawDoubleLow32);
    addDirect(m, "pids/1/d", 0x4928, NodeTypeIdx::FloatBits);
    addDirect(m, "pids/1/demod/adcselect", 0x4904, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "pids/1/demod/harmonic", 0x492c, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "pids/1/demod/order", 0x4908, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "pids/1/demod/timeconstant", 0x490c, NodeTypeIdx::FloatBits);
    addDirect(m, "pids/1/dlimittimeconstant", 0x4914, NodeTypeIdx::FloatBits);
    addDirect(m, "pids/1/enable", 0x4900, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "pids/1/i", 0x4924, NodeTypeIdx::FloatBits);
    addDirect(m, "pids/1/inputchannel", 0x4970, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "pids/1/inputsource", 0x496c, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "pids/1/limitlower", 0x491c, NodeTypeIdx::FloatBits);
    addDirect(m, "pids/1/limitupper", 0x4918, NodeTypeIdx::FloatBits);
    addDirect(m, "pids/1/mode", 0x4968, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "pids/1/output", 0x4938, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "pids/1/outputchannel", 0x4934, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "pids/1/p", 0x4920, NodeTypeIdx::FloatBits);
    addDirect(m, "pids/1/phaseunwrap", 0x4948, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "pids/1/pll/automode", 0x4994, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "pids/1/rate", 0x4964, NodeTypeIdx::FloatBits);
    addDirect(m, "pids/1/setpoint", 0x4910, NodeTypeIdx::FloatBits);
    addDirect(m, "pids/1/stream/rate", 0x495c, NodeTypeIdx::FloatBits);
    addDirect(m, "pids/2/center", 0x4a74, NodeTypeIdx::RawDoubleLow32);
    addDirect(m, "pids/2/d", 0x4a28, NodeTypeIdx::FloatBits);
    addDirect(m, "pids/2/demod/adcselect", 0x4a04, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "pids/2/demod/harmonic", 0x4a2c, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "pids/2/demod/order", 0x4a08, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "pids/2/demod/timeconstant", 0x4a0c, NodeTypeIdx::FloatBits);
    addDirect(m, "pids/2/dlimittimeconstant", 0x4a14, NodeTypeIdx::FloatBits);
    addDirect(m, "pids/2/enable", 0x4a00, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "pids/2/i", 0x4a24, NodeTypeIdx::FloatBits);
    addDirect(m, "pids/2/inputchannel", 0x4a70, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "pids/2/inputsource", 0x4a6c, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "pids/2/limitlower", 0x4a1c, NodeTypeIdx::FloatBits);
    addDirect(m, "pids/2/limitupper", 0x4a18, NodeTypeIdx::FloatBits);
    addDirect(m, "pids/2/mode", 0x4a68, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "pids/2/output", 0x4a38, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "pids/2/outputchannel", 0x4a34, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "pids/2/p", 0x4a20, NodeTypeIdx::FloatBits);
    addDirect(m, "pids/2/phaseunwrap", 0x4a48, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "pids/2/pll/automode", 0x4a94, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "pids/2/rate", 0x4a64, NodeTypeIdx::FloatBits);
    addDirect(m, "pids/2/setpoint", 0x4a10, NodeTypeIdx::FloatBits);
    addDirect(m, "pids/2/stream/rate", 0x4a5c, NodeTypeIdx::FloatBits);
    addDirect(m, "pids/3/center", 0x4b74, NodeTypeIdx::RawDoubleLow32);
    addDirect(m, "pids/3/d", 0x4b28, NodeTypeIdx::FloatBits);
    addDirect(m, "pids/3/demod/adcselect", 0x4b04, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "pids/3/demod/harmonic", 0x4b2c, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "pids/3/demod/order", 0x4b08, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "pids/3/demod/timeconstant", 0x4b0c, NodeTypeIdx::FloatBits);
    addDirect(m, "pids/3/dlimittimeconstant", 0x4b14, NodeTypeIdx::FloatBits);
    addDirect(m, "pids/3/enable", 0x4b00, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "pids/3/i", 0x4b24, NodeTypeIdx::FloatBits);
    addDirect(m, "pids/3/inputchannel", 0x4b70, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "pids/3/inputsource", 0x4b6c, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "pids/3/limitlower", 0x4b1c, NodeTypeIdx::FloatBits);
    addDirect(m, "pids/3/limitupper", 0x4b18, NodeTypeIdx::FloatBits);
    addDirect(m, "pids/3/mode", 0x4b68, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "pids/3/output", 0x4b38, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "pids/3/outputchannel", 0x4b34, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "pids/3/p", 0x4b20, NodeTypeIdx::FloatBits);
    addDirect(m, "pids/3/phaseunwrap", 0x4b48, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "pids/3/pll/automode", 0x4b94, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "pids/3/rate", 0x4b64, NodeTypeIdx::FloatBits);
    addDirect(m, "pids/3/setpoint", 0x4b10, NodeTypeIdx::FloatBits);
    addDirect(m, "pids/3/stream/rate", 0x4b5c, NodeTypeIdx::FloatBits);
    addDirect(m, "scopes/0/channel", 0x1a3c, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "scopes/0/channels/0/bwlimit", 0x1a2c, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "scopes/0/channels/0/inputselect", 0x1a28, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "scopes/0/channels/0/limitlower", 0x1aa0, NodeTypeIdx::FloatBits);
    addDirect(m, "scopes/0/channels/0/limitupper", 0x1aa4, NodeTypeIdx::FloatBits);
    addDirect(m, "scopes/0/channels/1/bwlimit", 0x1a34, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "scopes/0/channels/1/inputselect", 0x1a30, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "scopes/0/channels/1/limitlower", 0x1aac, NodeTypeIdx::FloatBits);
    addDirect(m, "scopes/0/channels/1/limitupper", 0x1ab0, NodeTypeIdx::FloatBits);
    addDirect(m, "scopes/0/cont", 0x1a54, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "scopes/0/enable", 0x1a00, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "scopes/0/length", 0x1a18, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "scopes/0/segments/count", 0x1a38, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "scopes/0/segments/counts/enable", 0x1a6c, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "scopes/0/single", 0x1a40, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "scopes/0/stream/enables/0", 0xec00, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "scopes/0/stream/enables/1", 0xec04, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "scopes/0/stream/rate", 0xec08, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "scopes/0/time", 0x1a04, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "scopes/0/trigchannel", 0x1a24, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "scopes/0/trigdelay", 0x1a8c, NodeTypeIdx::FloatBits);
    addDirect(m, "scopes/0/trigenable", 0x1a08, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "scopes/0/trigfalling", 0x1a10, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "scopes/0/trigforce", 0x1a64, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "scopes/0/triggate/enable", 0x1a80, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "scopes/0/triggate/inputselect", 0x1a84, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "scopes/0/trigholdoff", 0x1a1c, NodeTypeIdx::FloatBits);
    addDirect(m, "scopes/0/trigholdofftrigger", 0x1a94, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "scopes/0/trigholdofftriggerenable", 0x1a90, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "scopes/0/trighysteresis/absolute", 0x1a50, NodeTypeIdx::FloatBits);
    addDirect(m, "scopes/0/trighysteresis/mode", 0x1a60, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "scopes/0/trighysteresis/relative", 0x1a5c, NodeTypeIdx::FloatBits);
    addDirect(m, "scopes/0/triglevel", 0x1a14, NodeTypeIdx::FloatBits);
    addDirect(m, "scopes/0/trigreference", 0x1a88, NodeTypeIdx::FloatBits);
    addDirect(m, "scopes/0/trigrising", 0x1a0c, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "scopes/0/trigstate", 0x1a78, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "sigins/0/ac", 0x1d04, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "sigins/0/autorange", 0x1d40, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "sigins/0/bw", 0x1d20, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "sigins/0/diff", 0x1d3c, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "sigins/0/imp50", 0x1d08, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "sigins/0/on", 0x1d24, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "sigins/0/range", 0x1d28, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "sigins/0/scaling", 0x1d44, NodeTypeIdx::FloatBits);
    addDirect(m, "sigins/1/ac", 0x1d84, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "sigins/1/autorange", 0x1dc0, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "sigins/1/bw", 0x1da0, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "sigins/1/diff", 0x1dbc, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "sigins/1/imp50", 0x1d88, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "sigins/1/on", 0x1da4, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "sigins/1/range", 0x1da8, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "sigins/1/scaling", 0x1dc4, NodeTypeIdx::FloatBits);
    addDirect(m, "sigouts/0/amplitudes/0", 0x2004, NodeTypeIdx::FloatBits);
    addDirect(m, "sigouts/0/amplitudes/1", 0x200c, NodeTypeIdx::FloatBits);
    addDirect(m, "sigouts/0/amplitudes/2", 0x2014, NodeTypeIdx::FloatBits);
    addDirect(m, "sigouts/0/amplitudes/3", 0x201c, NodeTypeIdx::FloatBits);
    addDirect(m, "sigouts/0/amplitudes/4", 0x2024, NodeTypeIdx::FloatBits);
    addDirect(m, "sigouts/0/amplitudes/5", 0x202c, NodeTypeIdx::FloatBits);
    addDirect(m, "sigouts/0/amplitudes/6", 0x2034, NodeTypeIdx::FloatBits);
    addDirect(m, "sigouts/0/amplitudes/7", 0x203c, NodeTypeIdx::FloatBits);
    addDirect(m, "sigouts/0/autorange", 0x1e20, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "sigouts/0/enables/0", 0x2000, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "sigouts/0/enables/1", 0x2008, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "sigouts/0/enables/2", 0x2010, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "sigouts/0/enables/3", 0x2018, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "sigouts/0/enables/4", 0x2020, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "sigouts/0/enables/5", 0x2028, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "sigouts/0/enables/6", 0x2030, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "sigouts/0/enables/7", 0x2038, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "sigouts/0/imp50", 0x1e14, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "sigouts/0/offset", 0x1e30, NodeTypeIdx::FloatBits);
    addDirect(m, "sigouts/0/on", 0x1e00, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "sigouts/0/range", 0x1e04, NodeTypeIdx::FloatBits);
    addDirect(m, "sigouts/1/amplitudes/0", 0x2044, NodeTypeIdx::FloatBits);
    addDirect(m, "sigouts/1/amplitudes/1", 0x204c, NodeTypeIdx::FloatBits);
    addDirect(m, "sigouts/1/amplitudes/2", 0x2054, NodeTypeIdx::FloatBits);
    addDirect(m, "sigouts/1/amplitudes/3", 0x205c, NodeTypeIdx::FloatBits);
    addDirect(m, "sigouts/1/amplitudes/4", 0x2064, NodeTypeIdx::FloatBits);
    addDirect(m, "sigouts/1/amplitudes/5", 0x206c, NodeTypeIdx::FloatBits);
    addDirect(m, "sigouts/1/amplitudes/6", 0x2074, NodeTypeIdx::FloatBits);
    addDirect(m, "sigouts/1/amplitudes/7", 0x207c, NodeTypeIdx::FloatBits);
    addDirect(m, "sigouts/1/autorange", 0x1e60, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "sigouts/1/enables/0", 0x2040, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "sigouts/1/enables/1", 0x2048, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "sigouts/1/enables/2", 0x2050, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "sigouts/1/enables/3", 0x2058, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "sigouts/1/enables/4", 0x2060, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "sigouts/1/enables/5", 0x2068, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "sigouts/1/enables/6", 0x2070, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "sigouts/1/enables/7", 0x2078, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "sigouts/1/imp50", 0x1e54, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "sigouts/1/offset", 0x1e70, NodeTypeIdx::FloatBits);
    addDirect(m, "sigouts/1/on", 0x1e40, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "sigouts/1/range", 0x1e44, NodeTypeIdx::FloatBits);
    addDirect(m, "triggers/in/0/dcc_fedge", 0xd910, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "triggers/in/0/imp50", 0xd900, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "triggers/in/0/level", 0xd904, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "triggers/in/1/dcc_fedge", 0xd930, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "triggers/in/1/imp50", 0xd920, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "triggers/in/1/level", 0xd924, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "triggers/in/2/dcc_fedge", 0xd950, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "triggers/in/2/imp50", 0xd940, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "triggers/in/2/level", 0xd944, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "triggers/in/3/dcc_fedge", 0xd970, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "triggers/in/3/imp50", 0xd960, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "triggers/in/3/level", 0xd964, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "triggers/out/0/delay", 0xda14, NodeTypeIdx::FloatBits);
    addDirect(m, "triggers/out/0/dir", 0xda04, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "triggers/out/0/hold", 0xda10, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "triggers/out/0/srcsel", 0xda00, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "triggers/out/0/static_value", 0xda08, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "triggers/out/1/delay", 0xda34, NodeTypeIdx::FloatBits);
    addDirect(m, "triggers/out/1/dir", 0xda24, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "triggers/out/1/hold", 0xda30, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "triggers/out/1/srcsel", 0xda20, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "triggers/out/1/static_value", 0xda28, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "triggers/out/2/delay", 0xda54, NodeTypeIdx::FloatBits);
    addDirect(m, "triggers/out/2/dir", 0xda44, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "triggers/out/2/hold", 0xda50, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "triggers/out/2/srcsel", 0xda40, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "triggers/out/2/static_value", 0xda48, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "triggers/out/3/delay", 0xda74, NodeTypeIdx::FloatBits);
    addDirect(m, "triggers/out/3/dir", 0xda64, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "triggers/out/3/hold", 0xda70, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "triggers/out/3/srcsel", 0xda60, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "triggers/out/3/static_value", 0xda68, NodeTypeIdx::IntegerPassthrough);

    return nm;
}

// ---------------------------------------------------------------------------
// HDAWG — 186 entries @0x1ad9a0
// ---------------------------------------------------------------------------
template <>
std::unique_ptr<NodeMap> GetNodeMap<AwgDeviceType::HDAWG>::get() {  // @0x1ad9a0
    auto nm = std::make_unique<NodeMap>();
    auto& m = nm->entries_;

    addVirt(m, "_/dios/0/output", "DIOOUTPUT", {0}, NodeTypeIdx::IntegerPassthrough);
    addVirt(m, "awgs/0/outputs/0/gains/0", "AWGOUTGAIN", {0, 0, 0}, NodeTypeIdx::FloatBits);
    addVirt(m, "awgs/0/outputs/0/gains/1", "AWGOUTGAIN", {0, 0, 1}, NodeTypeIdx::FloatBits);
    addVirt(m, "awgs/0/outputs/0/modulation/carriers/0/harmonic", "AWGMODHARM", {0, 0, 0}, NodeTypeIdx::IntegerPassthrough);
    addVirt(m, "awgs/0/outputs/0/modulation/carriers/0/oscselect", "AWGMODOSCSEL", {0, 0, 0}, NodeTypeIdx::IntegerPassthrough);
    addVirt(m, "awgs/0/outputs/0/modulation/carriers/0/phaseshift", "AWGMODPHASE", {0, 0, 0}, NodeTypeIdx::FloatBits);
    addVirt(m, "awgs/0/outputs/0/modulation/carriers/1/harmonic", "AWGMODHARM", {0, 0, 1}, NodeTypeIdx::IntegerPassthrough);
    addVirt(m, "awgs/0/outputs/0/modulation/carriers/1/oscselect", "AWGMODOSCSEL", {0, 0, 1}, NodeTypeIdx::IntegerPassthrough);
    addVirt(m, "awgs/0/outputs/0/modulation/carriers/1/phaseshift", "AWGMODPHASE", {0, 0, 1}, NodeTypeIdx::FloatBits);
    addVirt(m, "awgs/0/outputs/0/modulation/carriers/2/harmonic", "AWGMODHARM", {0, 0, 2}, NodeTypeIdx::IntegerPassthrough);
    addVirt(m, "awgs/0/outputs/0/modulation/carriers/2/oscselect", "AWGMODOSCSEL", {0, 0, 2}, NodeTypeIdx::IntegerPassthrough);
    addVirt(m, "awgs/0/outputs/0/modulation/carriers/2/phaseshift", "AWGMODPHASE", {0, 0, 2}, NodeTypeIdx::FloatBits);
    addVirt(m, "awgs/0/outputs/0/modulation/carriers/3/harmonic", "AWGMODHARM", {0, 0, 3}, NodeTypeIdx::IntegerPassthrough);
    addVirt(m, "awgs/0/outputs/0/modulation/carriers/3/oscselect", "AWGMODOSCSEL", {0, 0, 3}, NodeTypeIdx::IntegerPassthrough);
    addVirt(m, "awgs/0/outputs/0/modulation/carriers/3/phaseshift", "AWGMODPHASE", {0, 0, 3}, NodeTypeIdx::FloatBits);
    addVirt(m, "awgs/0/outputs/1/gains/0", "AWGOUTGAIN", {0, 1, 0}, NodeTypeIdx::FloatBits);
    addVirt(m, "awgs/0/outputs/1/gains/1", "AWGOUTGAIN", {0, 1, 1}, NodeTypeIdx::FloatBits);
    addVirt(m, "awgs/0/outputs/1/modulation/carriers/0/harmonic", "AWGMODHARM", {0, 1, 0}, NodeTypeIdx::IntegerPassthrough);
    addVirt(m, "awgs/0/outputs/1/modulation/carriers/0/oscselect", "AWGMODOSCSEL", {0, 1, 0}, NodeTypeIdx::IntegerPassthrough);
    addVirt(m, "awgs/0/outputs/1/modulation/carriers/0/phaseshift", "AWGMODPHASE", {0, 1, 0}, NodeTypeIdx::FloatBits);
    addVirt(m, "awgs/0/outputs/1/modulation/carriers/1/harmonic", "AWGMODHARM", {0, 1, 1}, NodeTypeIdx::IntegerPassthrough);
    addVirt(m, "awgs/0/outputs/1/modulation/carriers/1/oscselect", "AWGMODOSCSEL", {0, 1, 1}, NodeTypeIdx::IntegerPassthrough);
    addVirt(m, "awgs/0/outputs/1/modulation/carriers/1/phaseshift", "AWGMODPHASE", {0, 1, 1}, NodeTypeIdx::FloatBits);
    addVirt(m, "awgs/0/outputs/1/modulation/carriers/2/harmonic", "AWGMODHARM", {0, 1, 2}, NodeTypeIdx::IntegerPassthrough);
    addVirt(m, "awgs/0/outputs/1/modulation/carriers/2/oscselect", "AWGMODOSCSEL", {0, 1, 2}, NodeTypeIdx::IntegerPassthrough);
    addVirt(m, "awgs/0/outputs/1/modulation/carriers/2/phaseshift", "AWGMODPHASE", {0, 1, 2}, NodeTypeIdx::FloatBits);
    addVirt(m, "awgs/0/outputs/1/modulation/carriers/3/harmonic", "AWGMODHARM", {0, 1, 3}, NodeTypeIdx::IntegerPassthrough);
    addVirt(m, "awgs/0/outputs/1/modulation/carriers/3/oscselect", "AWGMODOSCSEL", {0, 1, 3}, NodeTypeIdx::IntegerPassthrough);
    addVirt(m, "awgs/0/outputs/1/modulation/carriers/3/phaseshift", "AWGMODPHASE", {0, 1, 3}, NodeTypeIdx::FloatBits);
    addVirt(m, "awgs/1/outputs/0/gains/0", "AWGOUTGAIN", {1, 0, 0}, NodeTypeIdx::FloatBits);
    addVirt(m, "awgs/1/outputs/0/gains/1", "AWGOUTGAIN", {1, 0, 1}, NodeTypeIdx::FloatBits);
    addVirt(m, "awgs/1/outputs/0/modulation/carriers/0/harmonic", "AWGMODHARM", {1, 0, 0}, NodeTypeIdx::IntegerPassthrough);
    addVirt(m, "awgs/1/outputs/0/modulation/carriers/0/oscselect", "AWGMODOSCSEL", {1, 0, 0}, NodeTypeIdx::IntegerPassthrough);
    addVirt(m, "awgs/1/outputs/0/modulation/carriers/0/phaseshift", "AWGMODPHASE", {1, 0, 0}, NodeTypeIdx::FloatBits);
    addVirt(m, "awgs/1/outputs/0/modulation/carriers/1/harmonic", "AWGMODHARM", {1, 0, 1}, NodeTypeIdx::IntegerPassthrough);
    addVirt(m, "awgs/1/outputs/0/modulation/carriers/1/oscselect", "AWGMODOSCSEL", {1, 0, 1}, NodeTypeIdx::IntegerPassthrough);
    addVirt(m, "awgs/1/outputs/0/modulation/carriers/1/phaseshift", "AWGMODPHASE", {1, 0, 1}, NodeTypeIdx::FloatBits);
    addVirt(m, "awgs/1/outputs/0/modulation/carriers/2/harmonic", "AWGMODHARM", {1, 0, 2}, NodeTypeIdx::IntegerPassthrough);
    addVirt(m, "awgs/1/outputs/0/modulation/carriers/2/oscselect", "AWGMODOSCSEL", {1, 0, 2}, NodeTypeIdx::IntegerPassthrough);
    addVirt(m, "awgs/1/outputs/0/modulation/carriers/2/phaseshift", "AWGMODPHASE", {1, 0, 2}, NodeTypeIdx::FloatBits);
    addVirt(m, "awgs/1/outputs/0/modulation/carriers/3/harmonic", "AWGMODHARM", {1, 0, 3}, NodeTypeIdx::IntegerPassthrough);
    addVirt(m, "awgs/1/outputs/0/modulation/carriers/3/oscselect", "AWGMODOSCSEL", {1, 0, 3}, NodeTypeIdx::IntegerPassthrough);
    addVirt(m, "awgs/1/outputs/0/modulation/carriers/3/phaseshift", "AWGMODPHASE", {1, 0, 3}, NodeTypeIdx::FloatBits);
    addVirt(m, "awgs/1/outputs/1/gains/0", "AWGOUTGAIN", {1, 1, 0}, NodeTypeIdx::FloatBits);
    addVirt(m, "awgs/1/outputs/1/gains/1", "AWGOUTGAIN", {1, 1, 1}, NodeTypeIdx::FloatBits);
    addVirt(m, "awgs/1/outputs/1/modulation/carriers/0/harmonic", "AWGMODHARM", {1, 1, 0}, NodeTypeIdx::IntegerPassthrough);
    addVirt(m, "awgs/1/outputs/1/modulation/carriers/0/oscselect", "AWGMODOSCSEL", {1, 1, 0}, NodeTypeIdx::IntegerPassthrough);
    addVirt(m, "awgs/1/outputs/1/modulation/carriers/0/phaseshift", "AWGMODPHASE", {1, 1, 0}, NodeTypeIdx::FloatBits);
    addVirt(m, "awgs/1/outputs/1/modulation/carriers/1/harmonic", "AWGMODHARM", {1, 1, 1}, NodeTypeIdx::IntegerPassthrough);
    addVirt(m, "awgs/1/outputs/1/modulation/carriers/1/oscselect", "AWGMODOSCSEL", {1, 1, 1}, NodeTypeIdx::IntegerPassthrough);
    addVirt(m, "awgs/1/outputs/1/modulation/carriers/1/phaseshift", "AWGMODPHASE", {1, 1, 1}, NodeTypeIdx::FloatBits);
    addVirt(m, "awgs/1/outputs/1/modulation/carriers/2/harmonic", "AWGMODHARM", {1, 1, 2}, NodeTypeIdx::IntegerPassthrough);
    addVirt(m, "awgs/1/outputs/1/modulation/carriers/2/oscselect", "AWGMODOSCSEL", {1, 1, 2}, NodeTypeIdx::IntegerPassthrough);
    addVirt(m, "awgs/1/outputs/1/modulation/carriers/2/phaseshift", "AWGMODPHASE", {1, 1, 2}, NodeTypeIdx::FloatBits);
    addVirt(m, "awgs/1/outputs/1/modulation/carriers/3/harmonic", "AWGMODHARM", {1, 1, 3}, NodeTypeIdx::IntegerPassthrough);
    addVirt(m, "awgs/1/outputs/1/modulation/carriers/3/oscselect", "AWGMODOSCSEL", {1, 1, 3}, NodeTypeIdx::IntegerPassthrough);
    addVirt(m, "awgs/1/outputs/1/modulation/carriers/3/phaseshift", "AWGMODPHASE", {1, 1, 3}, NodeTypeIdx::FloatBits);
    addVirt(m, "awgs/2/outputs/0/gains/0", "AWGOUTGAIN", {2, 0, 0}, NodeTypeIdx::FloatBits);
    addVirt(m, "awgs/2/outputs/0/gains/1", "AWGOUTGAIN", {2, 0, 1}, NodeTypeIdx::FloatBits);
    addVirt(m, "awgs/2/outputs/0/modulation/carriers/0/harmonic", "AWGMODHARM", {2, 0, 0}, NodeTypeIdx::IntegerPassthrough);
    addVirt(m, "awgs/2/outputs/0/modulation/carriers/0/oscselect", "AWGMODOSCSEL", {2, 0, 0}, NodeTypeIdx::IntegerPassthrough);
    addVirt(m, "awgs/2/outputs/0/modulation/carriers/0/phaseshift", "AWGMODPHASE", {2, 0, 0}, NodeTypeIdx::FloatBits);
    addVirt(m, "awgs/2/outputs/0/modulation/carriers/1/harmonic", "AWGMODHARM", {2, 0, 1}, NodeTypeIdx::IntegerPassthrough);
    addVirt(m, "awgs/2/outputs/0/modulation/carriers/1/oscselect", "AWGMODOSCSEL", {2, 0, 1}, NodeTypeIdx::IntegerPassthrough);
    addVirt(m, "awgs/2/outputs/0/modulation/carriers/1/phaseshift", "AWGMODPHASE", {2, 0, 1}, NodeTypeIdx::FloatBits);
    addVirt(m, "awgs/2/outputs/0/modulation/carriers/2/harmonic", "AWGMODHARM", {2, 0, 2}, NodeTypeIdx::IntegerPassthrough);
    addVirt(m, "awgs/2/outputs/0/modulation/carriers/2/oscselect", "AWGMODOSCSEL", {2, 0, 2}, NodeTypeIdx::IntegerPassthrough);
    addVirt(m, "awgs/2/outputs/0/modulation/carriers/2/phaseshift", "AWGMODPHASE", {2, 0, 2}, NodeTypeIdx::FloatBits);
    addVirt(m, "awgs/2/outputs/0/modulation/carriers/3/harmonic", "AWGMODHARM", {2, 0, 3}, NodeTypeIdx::IntegerPassthrough);
    addVirt(m, "awgs/2/outputs/0/modulation/carriers/3/oscselect", "AWGMODOSCSEL", {2, 0, 3}, NodeTypeIdx::IntegerPassthrough);
    addVirt(m, "awgs/2/outputs/0/modulation/carriers/3/phaseshift", "AWGMODPHASE", {2, 0, 3}, NodeTypeIdx::FloatBits);
    addVirt(m, "awgs/2/outputs/1/gains/0", "AWGOUTGAIN", {2, 1, 0}, NodeTypeIdx::FloatBits);
    addVirt(m, "awgs/2/outputs/1/gains/1", "AWGOUTGAIN", {2, 1, 1}, NodeTypeIdx::FloatBits);
    addVirt(m, "awgs/2/outputs/1/modulation/carriers/0/harmonic", "AWGMODHARM", {2, 1, 0}, NodeTypeIdx::IntegerPassthrough);
    addVirt(m, "awgs/2/outputs/1/modulation/carriers/0/oscselect", "AWGMODOSCSEL", {2, 1, 0}, NodeTypeIdx::IntegerPassthrough);
    addVirt(m, "awgs/2/outputs/1/modulation/carriers/0/phaseshift", "AWGMODPHASE", {2, 1, 0}, NodeTypeIdx::FloatBits);
    addVirt(m, "awgs/2/outputs/1/modulation/carriers/1/harmonic", "AWGMODHARM", {2, 1, 1}, NodeTypeIdx::IntegerPassthrough);
    addVirt(m, "awgs/2/outputs/1/modulation/carriers/1/oscselect", "AWGMODOSCSEL", {2, 1, 1}, NodeTypeIdx::IntegerPassthrough);
    addVirt(m, "awgs/2/outputs/1/modulation/carriers/1/phaseshift", "AWGMODPHASE", {2, 1, 1}, NodeTypeIdx::FloatBits);
    addVirt(m, "awgs/2/outputs/1/modulation/carriers/2/harmonic", "AWGMODHARM", {2, 1, 2}, NodeTypeIdx::IntegerPassthrough);
    addVirt(m, "awgs/2/outputs/1/modulation/carriers/2/oscselect", "AWGMODOSCSEL", {2, 1, 2}, NodeTypeIdx::IntegerPassthrough);
    addVirt(m, "awgs/2/outputs/1/modulation/carriers/2/phaseshift", "AWGMODPHASE", {2, 1, 2}, NodeTypeIdx::FloatBits);
    addVirt(m, "awgs/2/outputs/1/modulation/carriers/3/harmonic", "AWGMODHARM", {2, 1, 3}, NodeTypeIdx::IntegerPassthrough);
    addVirt(m, "awgs/2/outputs/1/modulation/carriers/3/oscselect", "AWGMODOSCSEL", {2, 1, 3}, NodeTypeIdx::IntegerPassthrough);
    addVirt(m, "awgs/2/outputs/1/modulation/carriers/3/phaseshift", "AWGMODPHASE", {2, 1, 3}, NodeTypeIdx::FloatBits);
    addVirt(m, "awgs/3/outputs/0/gains/0", "AWGOUTGAIN", {3, 0, 0}, NodeTypeIdx::FloatBits);
    addVirt(m, "awgs/3/outputs/0/gains/1", "AWGOUTGAIN", {3, 0, 1}, NodeTypeIdx::FloatBits);
    addVirt(m, "awgs/3/outputs/0/modulation/carriers/0/harmonic", "AWGMODHARM", {3, 0, 0}, NodeTypeIdx::IntegerPassthrough);
    addVirt(m, "awgs/3/outputs/0/modulation/carriers/0/oscselect", "AWGMODOSCSEL", {3, 0, 0}, NodeTypeIdx::IntegerPassthrough);
    addVirt(m, "awgs/3/outputs/0/modulation/carriers/0/phaseshift", "AWGMODPHASE", {3, 0, 0}, NodeTypeIdx::FloatBits);
    addVirt(m, "awgs/3/outputs/0/modulation/carriers/1/harmonic", "AWGMODHARM", {3, 0, 1}, NodeTypeIdx::IntegerPassthrough);
    addVirt(m, "awgs/3/outputs/0/modulation/carriers/1/oscselect", "AWGMODOSCSEL", {3, 0, 1}, NodeTypeIdx::IntegerPassthrough);
    addVirt(m, "awgs/3/outputs/0/modulation/carriers/1/phaseshift", "AWGMODPHASE", {3, 0, 1}, NodeTypeIdx::FloatBits);
    addVirt(m, "awgs/3/outputs/0/modulation/carriers/2/harmonic", "AWGMODHARM", {3, 0, 2}, NodeTypeIdx::IntegerPassthrough);
    addVirt(m, "awgs/3/outputs/0/modulation/carriers/2/oscselect", "AWGMODOSCSEL", {3, 0, 2}, NodeTypeIdx::IntegerPassthrough);
    addVirt(m, "awgs/3/outputs/0/modulation/carriers/2/phaseshift", "AWGMODPHASE", {3, 0, 2}, NodeTypeIdx::FloatBits);
    addVirt(m, "awgs/3/outputs/0/modulation/carriers/3/harmonic", "AWGMODHARM", {3, 0, 3}, NodeTypeIdx::IntegerPassthrough);
    addVirt(m, "awgs/3/outputs/0/modulation/carriers/3/oscselect", "AWGMODOSCSEL", {3, 0, 3}, NodeTypeIdx::IntegerPassthrough);
    addVirt(m, "awgs/3/outputs/0/modulation/carriers/3/phaseshift", "AWGMODPHASE", {3, 0, 3}, NodeTypeIdx::FloatBits);
    addVirt(m, "awgs/3/outputs/1/gains/0", "AWGOUTGAIN", {3, 1, 0}, NodeTypeIdx::FloatBits);
    addVirt(m, "awgs/3/outputs/1/gains/1", "AWGOUTGAIN", {3, 1, 1}, NodeTypeIdx::FloatBits);
    addVirt(m, "awgs/3/outputs/1/modulation/carriers/0/harmonic", "AWGMODHARM", {3, 1, 0}, NodeTypeIdx::IntegerPassthrough);
    addVirt(m, "awgs/3/outputs/1/modulation/carriers/0/oscselect", "AWGMODOSCSEL", {3, 1, 0}, NodeTypeIdx::IntegerPassthrough);
    addVirt(m, "awgs/3/outputs/1/modulation/carriers/0/phaseshift", "AWGMODPHASE", {3, 1, 0}, NodeTypeIdx::FloatBits);
    addVirt(m, "awgs/3/outputs/1/modulation/carriers/1/harmonic", "AWGMODHARM", {3, 1, 1}, NodeTypeIdx::IntegerPassthrough);
    addVirt(m, "awgs/3/outputs/1/modulation/carriers/1/oscselect", "AWGMODOSCSEL", {3, 1, 1}, NodeTypeIdx::IntegerPassthrough);
    addVirt(m, "awgs/3/outputs/1/modulation/carriers/1/phaseshift", "AWGMODPHASE", {3, 1, 1}, NodeTypeIdx::FloatBits);
    addVirt(m, "awgs/3/outputs/1/modulation/carriers/2/harmonic", "AWGMODHARM", {3, 1, 2}, NodeTypeIdx::IntegerPassthrough);
    addVirt(m, "awgs/3/outputs/1/modulation/carriers/2/oscselect", "AWGMODOSCSEL", {3, 1, 2}, NodeTypeIdx::IntegerPassthrough);
    addVirt(m, "awgs/3/outputs/1/modulation/carriers/2/phaseshift", "AWGMODPHASE", {3, 1, 2}, NodeTypeIdx::FloatBits);
    addVirt(m, "awgs/3/outputs/1/modulation/carriers/3/harmonic", "AWGMODHARM", {3, 1, 3}, NodeTypeIdx::IntegerPassthrough);
    addVirt(m, "awgs/3/outputs/1/modulation/carriers/3/oscselect", "AWGMODOSCSEL", {3, 1, 3}, NodeTypeIdx::IntegerPassthrough);
    addVirt(m, "awgs/3/outputs/1/modulation/carriers/3/phaseshift", "AWGMODPHASE", {3, 1, 3}, NodeTypeIdx::FloatBits);
    addVirt(m, "oscs/0/freq", "OSCFREQ", {0}, NodeTypeIdx::Frequency, true, 0x00);
    addVirt(m, "oscs/1/freq", "OSCFREQ", {1}, NodeTypeIdx::Frequency, true, 0x08);
    addVirt(m, "oscs/10/freq", "OSCFREQ", {10}, NodeTypeIdx::Frequency, true, 0x50);
    addVirt(m, "oscs/11/freq", "OSCFREQ", {11}, NodeTypeIdx::Frequency, true, 0x58);
    addVirt(m, "oscs/12/freq", "OSCFREQ", {12}, NodeTypeIdx::Frequency, true, 0x60);
    addVirt(m, "oscs/13/freq", "OSCFREQ", {13}, NodeTypeIdx::Frequency, true, 0x68);
    addVirt(m, "oscs/14/freq", "OSCFREQ", {14}, NodeTypeIdx::Frequency, true, 0x70);
    addVirt(m, "oscs/15/freq", "OSCFREQ", {15}, NodeTypeIdx::Frequency, true, 0x78);
    addVirt(m, "oscs/2/freq", "OSCFREQ", {2}, NodeTypeIdx::Frequency, true, 0x10);
    addVirt(m, "oscs/3/freq", "OSCFREQ", {3}, NodeTypeIdx::Frequency, true, 0x18);
    addVirt(m, "oscs/4/freq", "OSCFREQ", {4}, NodeTypeIdx::Frequency, true, 0x20);
    addVirt(m, "oscs/5/freq", "OSCFREQ", {5}, NodeTypeIdx::Frequency, true, 0x28);
    addVirt(m, "oscs/6/freq", "OSCFREQ", {6}, NodeTypeIdx::Frequency, true, 0x30);
    addVirt(m, "oscs/7/freq", "OSCFREQ", {7}, NodeTypeIdx::Frequency, true, 0x38);
    addVirt(m, "oscs/8/freq", "OSCFREQ", {8}, NodeTypeIdx::Frequency, true, 0x40);
    addVirt(m, "oscs/9/freq", "OSCFREQ", {9}, NodeTypeIdx::Frequency, true, 0x48);
    addVirt(m, "oscs/phasereset", "OSCPHASERST", {}, NodeTypeIdx::IntegerPassthrough, true, 0x88);
    addVirt(m, "sines/0/amplitudes/0", "SINEAMPL", {0, 0}, NodeTypeIdx::FloatBits);
    addVirt(m, "sines/0/amplitudes/1", "SINEAMPL", {0, 1}, NodeTypeIdx::FloatBits);
    addVirt(m, "sines/0/enables/0", "SINEENABLE", {0, 0}, NodeTypeIdx::IntegerPassthrough);
    addVirt(m, "sines/0/enables/1", "SINEENABLE", {0, 1}, NodeTypeIdx::IntegerPassthrough);
    addVirt(m, "sines/0/harmonic", "SINEHARM", {0}, NodeTypeIdx::IntegerPassthrough, true, 0xa4);
    addVirt(m, "sines/0/oscselect", "SINEOSCSEL", {0}, NodeTypeIdx::IntegerPassthrough, true, 0x9c);
    addVirt(m, "sines/0/phaseshift", "SINEPHASE", {0}, NodeTypeIdx::Phase, true, 0xa0);
    addVirt(m, "sines/1/amplitudes/0", "SINEAMPL", {1, 0}, NodeTypeIdx::FloatBits);
    addVirt(m, "sines/1/amplitudes/1", "SINEAMPL", {1, 1}, NodeTypeIdx::FloatBits);
    addVirt(m, "sines/1/enables/0", "SINEENABLE", {1, 0}, NodeTypeIdx::IntegerPassthrough);
    addVirt(m, "sines/1/enables/1", "SINEENABLE", {1, 1}, NodeTypeIdx::IntegerPassthrough);
    addVirt(m, "sines/1/harmonic", "SINEHARM", {1}, NodeTypeIdx::IntegerPassthrough, true, 0xb0);
    addVirt(m, "sines/1/oscselect", "SINEOSCSEL", {1}, NodeTypeIdx::IntegerPassthrough, true, 0xa8);
    addVirt(m, "sines/1/phaseshift", "SINEPHASE", {1}, NodeTypeIdx::Phase, true, 0xac);
    addVirt(m, "sines/2/amplitudes/0", "SINEAMPL", {2, 0}, NodeTypeIdx::FloatBits);
    addVirt(m, "sines/2/amplitudes/1", "SINEAMPL", {2, 1}, NodeTypeIdx::FloatBits);
    addVirt(m, "sines/2/enables/0", "SINEENABLE", {2, 0}, NodeTypeIdx::IntegerPassthrough);
    addVirt(m, "sines/2/enables/1", "SINEENABLE", {2, 1}, NodeTypeIdx::IntegerPassthrough);
    addVirt(m, "sines/2/harmonic", "SINEHARM", {2}, NodeTypeIdx::IntegerPassthrough, true, 0xa4);
    addVirt(m, "sines/2/oscselect", "SINEOSCSEL", {2}, NodeTypeIdx::IntegerPassthrough, true, 0x9c);
    addVirt(m, "sines/2/phaseshift", "SINEPHASE", {2}, NodeTypeIdx::Phase, true, 0xa0);
    addVirt(m, "sines/3/amplitudes/0", "SINEAMPL", {3, 0}, NodeTypeIdx::FloatBits);
    addVirt(m, "sines/3/amplitudes/1", "SINEAMPL", {3, 1}, NodeTypeIdx::FloatBits);
    addVirt(m, "sines/3/enables/0", "SINEENABLE", {3, 0}, NodeTypeIdx::IntegerPassthrough);
    addVirt(m, "sines/3/enables/1", "SINEENABLE", {3, 1}, NodeTypeIdx::IntegerPassthrough);
    addVirt(m, "sines/3/harmonic", "SINEHARM", {3}, NodeTypeIdx::IntegerPassthrough, true, 0xb0);
    addVirt(m, "sines/3/oscselect", "SINEOSCSEL", {3}, NodeTypeIdx::IntegerPassthrough, true, 0xa8);
    addVirt(m, "sines/3/phaseshift", "SINEPHASE", {3}, NodeTypeIdx::Phase, true, 0xac);
    addVirt(m, "sines/4/amplitudes/0", "SINEAMPL", {4, 0}, NodeTypeIdx::FloatBits);
    addVirt(m, "sines/4/amplitudes/1", "SINEAMPL", {4, 1}, NodeTypeIdx::FloatBits);
    addVirt(m, "sines/4/enables/0", "SINEENABLE", {4, 0}, NodeTypeIdx::IntegerPassthrough);
    addVirt(m, "sines/4/enables/1", "SINEENABLE", {4, 1}, NodeTypeIdx::IntegerPassthrough);
    addVirt(m, "sines/4/harmonic", "SINEHARM", {4}, NodeTypeIdx::IntegerPassthrough, true, 0xa4);
    addVirt(m, "sines/4/oscselect", "SINEOSCSEL", {4}, NodeTypeIdx::IntegerPassthrough, true, 0x9c);
    addVirt(m, "sines/4/phaseshift", "SINEPHASE", {4}, NodeTypeIdx::Phase, true, 0xa0);
    addVirt(m, "sines/5/amplitudes/0", "SINEAMPL", {5, 0}, NodeTypeIdx::FloatBits);
    addVirt(m, "sines/5/amplitudes/1", "SINEAMPL", {5, 1}, NodeTypeIdx::FloatBits);
    addVirt(m, "sines/5/enables/0", "SINEENABLE", {5, 0}, NodeTypeIdx::IntegerPassthrough);
    addVirt(m, "sines/5/enables/1", "SINEENABLE", {5, 1}, NodeTypeIdx::IntegerPassthrough);
    addVirt(m, "sines/5/harmonic", "SINEHARM", {5}, NodeTypeIdx::IntegerPassthrough, true, 0xb0);
    addVirt(m, "sines/5/oscselect", "SINEOSCSEL", {5}, NodeTypeIdx::IntegerPassthrough, true, 0xa8);
    addVirt(m, "sines/5/phaseshift", "SINEPHASE", {5}, NodeTypeIdx::Phase, true, 0xac);
    addVirt(m, "sines/6/amplitudes/0", "SINEAMPL", {6, 0}, NodeTypeIdx::FloatBits);
    addVirt(m, "sines/6/amplitudes/1", "SINEAMPL", {6, 1}, NodeTypeIdx::FloatBits);
    addVirt(m, "sines/6/enables/0", "SINEENABLE", {6, 0}, NodeTypeIdx::IntegerPassthrough);
    addVirt(m, "sines/6/enables/1", "SINEENABLE", {6, 1}, NodeTypeIdx::IntegerPassthrough);
    addVirt(m, "sines/6/harmonic", "SINEHARM", {6}, NodeTypeIdx::IntegerPassthrough, true, 0xa4);
    addVirt(m, "sines/6/oscselect", "SINEOSCSEL", {6}, NodeTypeIdx::IntegerPassthrough, true, 0x9c);
    addVirt(m, "sines/6/phaseshift", "SINEPHASE", {6}, NodeTypeIdx::Phase, true, 0xa0);
    addVirt(m, "sines/7/amplitudes/0", "SINEAMPL", {7, 0}, NodeTypeIdx::FloatBits);
    addVirt(m, "sines/7/amplitudes/1", "SINEAMPL", {7, 1}, NodeTypeIdx::FloatBits);
    addVirt(m, "sines/7/enables/0", "SINEENABLE", {7, 0}, NodeTypeIdx::IntegerPassthrough);
    addVirt(m, "sines/7/enables/1", "SINEENABLE", {7, 1}, NodeTypeIdx::IntegerPassthrough);
    addVirt(m, "sines/7/harmonic", "SINEHARM", {7}, NodeTypeIdx::IntegerPassthrough, true, 0xb0);
    addVirt(m, "sines/7/oscselect", "SINEOSCSEL", {7}, NodeTypeIdx::IntegerPassthrough, true, 0xa8);
    addVirt(m, "sines/7/phaseshift", "SINEPHASE", {7}, NodeTypeIdx::Phase, true, 0xac);

    return nm;
}

// ---------------------------------------------------------------------------
// UHFQA — 209 entries @0x1b1470
// ---------------------------------------------------------------------------
template <>
std::unique_ptr<NodeMap> GetNodeMap<AwgDeviceType::UHFQA>::get() {  // @0x1b1470
    auto nm = std::make_unique<NodeMap>();
    auto& m = nm->entries_;

    addDirect(m, "auxouts/0/demodselect", 0xd738, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "auxouts/0/limitlower", 0xd720, NodeTypeIdx::FloatBits);
    addDirect(m, "auxouts/0/limitupper", 0xd724, NodeTypeIdx::FloatBits);
    addDirect(m, "auxouts/0/offset", 0xd708, NodeTypeIdx::FloatBits);
    addDirect(m, "auxouts/0/outputselect", 0xd734, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "auxouts/0/preoffset", 0xd700, NodeTypeIdx::RawDoubleLow32);
    addDirect(m, "auxouts/0/scale", 0xd710, NodeTypeIdx::FloatBits);
    addDirect(m, "auxouts/1/demodselect", 0xd778, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "auxouts/1/limitlower", 0xd760, NodeTypeIdx::FloatBits);
    addDirect(m, "auxouts/1/limitupper", 0xd764, NodeTypeIdx::FloatBits);
    addDirect(m, "auxouts/1/offset", 0xd748, NodeTypeIdx::FloatBits);
    addDirect(m, "auxouts/1/outputselect", 0xd774, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "auxouts/1/preoffset", 0xd740, NodeTypeIdx::RawDoubleLow32);
    addDirect(m, "auxouts/1/scale", 0xd750, NodeTypeIdx::FloatBits);
    addDirect(m, "auxouts/2/demodselect", 0xd7b8, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "auxouts/2/limitlower", 0xd7a0, NodeTypeIdx::FloatBits);
    addDirect(m, "auxouts/2/limitupper", 0xd7a4, NodeTypeIdx::FloatBits);
    addDirect(m, "auxouts/2/offset", 0xd788, NodeTypeIdx::FloatBits);
    addDirect(m, "auxouts/2/outputselect", 0xd7b4, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "auxouts/2/preoffset", 0xd780, NodeTypeIdx::RawDoubleLow32);
    addDirect(m, "auxouts/2/scale", 0xd790, NodeTypeIdx::FloatBits);
    addDirect(m, "auxouts/3/demodselect", 0xd7f8, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "auxouts/3/limitlower", 0xd7e0, NodeTypeIdx::FloatBits);
    addDirect(m, "auxouts/3/limitupper", 0xd7e4, NodeTypeIdx::FloatBits);
    addDirect(m, "auxouts/3/offset", 0xd7c8, NodeTypeIdx::FloatBits);
    addDirect(m, "auxouts/3/outputselect", 0xd7f4, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "auxouts/3/preoffset", 0xd7c0, NodeTypeIdx::RawDoubleLow32);
    addDirect(m, "auxouts/3/scale", 0xd7d0, NodeTypeIdx::FloatBits);
    addDirect(m, "awgs/0/auxtriggers/0/channel", 0x5300, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "awgs/0/auxtriggers/0/falling", 0x5308, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "awgs/0/auxtriggers/0/rising", 0x5304, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "awgs/0/auxtriggers/0/state", 0x530c, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "awgs/0/auxtriggers/1/channel", 0x5310, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "awgs/0/auxtriggers/1/falling", 0x5318, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "awgs/0/auxtriggers/1/rising", 0x5314, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "awgs/0/auxtriggers/1/state", 0x531c, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "awgs/0/enable", 0x5000, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "awgs/0/outputs/0/amplitude", 0x5500, NodeTypeIdx::FloatBits);
    addDirect(m, "awgs/0/outputs/0/mode", 0x5504, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "awgs/0/outputs/1/amplitude", 0x5540, NodeTypeIdx::FloatBits);
    addDirect(m, "awgs/0/outputs/1/mode", 0x5544, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "awgs/0/single", 0x5004, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "awgs/0/time", 0x5008, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "awgs/0/triggers/0/channel", 0x5218, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "awgs/0/triggers/0/falling", 0x5214, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "awgs/0/triggers/0/force", 0x521c, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "awgs/0/triggers/0/gate/enable", 0x5224, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "awgs/0/triggers/0/gate/inputselect", 0x5228, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "awgs/0/triggers/0/hysteresis/absolute", 0x5204, NodeTypeIdx::FloatBits);
    addDirect(m, "awgs/0/triggers/0/hysteresis/mode", 0x520c, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "awgs/0/triggers/0/hysteresis/relative", 0x5208, NodeTypeIdx::FloatBits);
    addDirect(m, "awgs/0/triggers/0/level", 0x5200, NodeTypeIdx::FloatBits);
    addDirect(m, "awgs/0/triggers/0/rising", 0x5210, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "awgs/0/triggers/0/state", 0x5220, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "awgs/0/triggers/1/channel", 0x5258, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "awgs/0/triggers/1/falling", 0x5254, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "awgs/0/triggers/1/force", 0x525c, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "awgs/0/triggers/1/gate/enable", 0x5264, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "awgs/0/triggers/1/gate/inputselect", 0x5268, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "awgs/0/triggers/1/hysteresis/absolute", 0x5244, NodeTypeIdx::FloatBits);
    addDirect(m, "awgs/0/triggers/1/hysteresis/mode", 0x524c, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "awgs/0/triggers/1/hysteresis/relative", 0x5248, NodeTypeIdx::FloatBits);
    addDirect(m, "awgs/0/triggers/1/level", 0x5240, NodeTypeIdx::FloatBits);
    addDirect(m, "awgs/0/triggers/1/rising", 0x5250, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "awgs/0/triggers/1/state", 0x5260, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "awgs/0/userregs/0", 0x5060, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "awgs/0/userregs/1", 0x5064, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "awgs/0/userregs/10", 0x5088, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "awgs/0/userregs/11", 0x508c, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "awgs/0/userregs/12", 0x5090, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "awgs/0/userregs/13", 0x5094, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "awgs/0/userregs/14", 0x5098, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "awgs/0/userregs/15", 0x509c, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "awgs/0/userregs/2", 0x5068, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "awgs/0/userregs/3", 0x506c, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "awgs/0/userregs/4", 0x5070, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "awgs/0/userregs/5", 0x5074, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "awgs/0/userregs/6", 0x5078, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "awgs/0/userregs/7", 0x507c, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "awgs/0/userregs/8", 0x5080, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "awgs/0/userregs/9", 0x5084, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "oscs/0/freq", 0x1400, NodeTypeIdx::Frequency, true, 0x00);
    addDirect(m, "qas/0/integration/length", 0x673c, NodeTypeIdx::IntegerPassthrough, true, 0x34);
    addDirect(m, "scopes/0/channel", 0x1a3c, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "scopes/0/channels/0/bwlimit", 0x1a2c, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "scopes/0/channels/0/inputselect", 0x1a28, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "scopes/0/channels/0/limitlower", 0x1aa0, NodeTypeIdx::FloatBits);
    addDirect(m, "scopes/0/channels/0/limitupper", 0x1aa4, NodeTypeIdx::FloatBits);
    addDirect(m, "scopes/0/channels/1/bwlimit", 0x1a34, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "scopes/0/channels/1/inputselect", 0x1a30, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "scopes/0/channels/1/limitlower", 0x1aac, NodeTypeIdx::FloatBits);
    addDirect(m, "scopes/0/channels/1/limitupper", 0x1ab0, NodeTypeIdx::FloatBits);
    addDirect(m, "scopes/0/cont", 0x1a54, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "scopes/0/enable", 0x1a00, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "scopes/0/length", 0x1a18, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "scopes/0/segments/count", 0x1a38, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "scopes/0/segments/counts/enable", 0x1a6c, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "scopes/0/single", 0x1a40, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "scopes/0/stream/enables/0", 0xec00, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "scopes/0/stream/enables/1", 0xec04, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "scopes/0/stream/rate", 0xec08, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "scopes/0/time", 0x1a04, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "scopes/0/trigchannel", 0x1a24, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "scopes/0/trigdelay", 0x1a8c, NodeTypeIdx::FloatBits);
    addDirect(m, "scopes/0/trigenable", 0x1a08, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "scopes/0/trigfalling", 0x1a10, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "scopes/0/trigforce", 0x1a64, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "scopes/0/triggate/enable", 0x1a80, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "scopes/0/triggate/inputselect", 0x1a84, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "scopes/0/trigholdoff", 0x1a1c, NodeTypeIdx::FloatBits);
    addDirect(m, "scopes/0/trigholdofftrigger", 0x1a94, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "scopes/0/trigholdofftriggerenable", 0x1a90, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "scopes/0/trighysteresis/absolute", 0x1a50, NodeTypeIdx::FloatBits);
    addDirect(m, "scopes/0/trighysteresis/mode", 0x1a60, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "scopes/0/trighysteresis/relative", 0x1a5c, NodeTypeIdx::FloatBits);
    addDirect(m, "scopes/0/triglevel", 0x1a14, NodeTypeIdx::FloatBits);
    addDirect(m, "scopes/0/trigreference", 0x1a88, NodeTypeIdx::FloatBits);
    addDirect(m, "scopes/0/trigrising", 0x1a0c, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "scopes/0/trigstate", 0x1a78, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "sigins/0/ac", 0x1d04, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "sigins/0/autorange", 0x1d40, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "sigins/0/bw", 0x1d20, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "sigins/0/diff", 0x1d3c, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "sigins/0/imp50", 0x1d08, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "sigins/0/on", 0x1d24, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "sigins/0/range", 0x1d28, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "sigins/0/scaling", 0x1d44, NodeTypeIdx::FloatBits);
    addDirect(m, "sigins/1/ac", 0x1d84, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "sigins/1/autorange", 0x1dc0, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "sigins/1/bw", 0x1da0, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "sigins/1/diff", 0x1dbc, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "sigins/1/imp50", 0x1d88, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "sigins/1/on", 0x1da4, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "sigins/1/range", 0x1da8, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "sigins/1/scaling", 0x1dc4, NodeTypeIdx::FloatBits);
    addDirect(m, "sigouts/0/amplitudes/0", 0x2004, NodeTypeIdx::FloatBits);
    addDirect(m, "sigouts/0/amplitudes/1", 0x200c, NodeTypeIdx::FloatBits);
    addDirect(m, "sigouts/0/amplitudes/2", 0x2014, NodeTypeIdx::FloatBits);
    addDirect(m, "sigouts/0/amplitudes/3", 0x201c, NodeTypeIdx::FloatBits);
    addDirect(m, "sigouts/0/amplitudes/4", 0x2024, NodeTypeIdx::FloatBits);
    addDirect(m, "sigouts/0/amplitudes/5", 0x202c, NodeTypeIdx::FloatBits);
    addDirect(m, "sigouts/0/amplitudes/6", 0x2034, NodeTypeIdx::FloatBits);
    addDirect(m, "sigouts/0/amplitudes/7", 0x203c, NodeTypeIdx::FloatBits);
    addDirect(m, "sigouts/0/autorange", 0x1e20, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "sigouts/0/enables/0", 0x2000, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "sigouts/0/enables/1", 0x2008, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "sigouts/0/enables/2", 0x2010, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "sigouts/0/enables/3", 0x2018, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "sigouts/0/enables/4", 0x2020, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "sigouts/0/enables/5", 0x2028, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "sigouts/0/enables/6", 0x2030, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "sigouts/0/enables/7", 0x2038, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "sigouts/0/imp50", 0x1e14, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "sigouts/0/offset", 0x1e30, NodeTypeIdx::FloatBits);
    addDirect(m, "sigouts/0/on", 0x1e00, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "sigouts/0/range", 0x1e04, NodeTypeIdx::FloatBits);
    addDirect(m, "sigouts/1/amplitudes/0", 0x2044, NodeTypeIdx::FloatBits);
    addDirect(m, "sigouts/1/amplitudes/1", 0x204c, NodeTypeIdx::FloatBits);
    addDirect(m, "sigouts/1/amplitudes/2", 0x2054, NodeTypeIdx::FloatBits);
    addDirect(m, "sigouts/1/amplitudes/3", 0x205c, NodeTypeIdx::FloatBits);
    addDirect(m, "sigouts/1/amplitudes/4", 0x2064, NodeTypeIdx::FloatBits);
    addDirect(m, "sigouts/1/amplitudes/5", 0x206c, NodeTypeIdx::FloatBits);
    addDirect(m, "sigouts/1/amplitudes/6", 0x2074, NodeTypeIdx::FloatBits);
    addDirect(m, "sigouts/1/amplitudes/7", 0x207c, NodeTypeIdx::FloatBits);
    addDirect(m, "sigouts/1/autorange", 0x1e60, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "sigouts/1/enables/0", 0x2040, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "sigouts/1/enables/1", 0x2048, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "sigouts/1/enables/2", 0x2050, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "sigouts/1/enables/3", 0x2058, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "sigouts/1/enables/4", 0x2060, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "sigouts/1/enables/5", 0x2068, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "sigouts/1/enables/6", 0x2070, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "sigouts/1/enables/7", 0x2078, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "sigouts/1/imp50", 0x1e54, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "sigouts/1/offset", 0x1e70, NodeTypeIdx::FloatBits);
    addDirect(m, "sigouts/1/on", 0x1e40, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "sigouts/1/range", 0x1e44, NodeTypeIdx::FloatBits);
    addDirect(m, "triggers/in/0/dcc_fedge", 0xd910, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "triggers/in/0/imp50", 0xd900, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "triggers/in/0/level", 0xd904, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "triggers/in/1/dcc_fedge", 0xd930, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "triggers/in/1/imp50", 0xd920, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "triggers/in/1/level", 0xd924, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "triggers/in/2/dcc_fedge", 0xd950, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "triggers/in/2/imp50", 0xd940, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "triggers/in/2/level", 0xd944, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "triggers/in/3/dcc_fedge", 0xd970, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "triggers/in/3/imp50", 0xd960, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "triggers/in/3/level", 0xd964, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "triggers/out/0/delay", 0xda14, NodeTypeIdx::FloatBits);
    addDirect(m, "triggers/out/0/dir", 0xda04, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "triggers/out/0/hold", 0xda10, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "triggers/out/0/srcsel", 0xda00, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "triggers/out/0/static_value", 0xda08, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "triggers/out/1/delay", 0xda34, NodeTypeIdx::FloatBits);
    addDirect(m, "triggers/out/1/dir", 0xda24, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "triggers/out/1/hold", 0xda30, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "triggers/out/1/srcsel", 0xda20, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "triggers/out/1/static_value", 0xda28, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "triggers/out/2/delay", 0xda54, NodeTypeIdx::FloatBits);
    addDirect(m, "triggers/out/2/dir", 0xda44, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "triggers/out/2/hold", 0xda50, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "triggers/out/2/srcsel", 0xda40, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "triggers/out/2/static_value", 0xda48, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "triggers/out/3/delay", 0xda74, NodeTypeIdx::FloatBits);
    addDirect(m, "triggers/out/3/dir", 0xda64, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "triggers/out/3/hold", 0xda70, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "triggers/out/3/srcsel", 0xda60, NodeTypeIdx::IntegerPassthrough);
    addDirect(m, "triggers/out/3/static_value", 0xda68, NodeTypeIdx::IntegerPassthrough);

    return nm;
}

// ---------------------------------------------------------------------------
// SHFQA — 24 entries @0x1ba3d0
// ---------------------------------------------------------------------------
template <>
std::unique_ptr<NodeMap> GetNodeMap<AwgDeviceType::SHFQA>::get() {  // @0x1ba3d0
    auto nm = std::make_unique<NodeMap>();
    auto& m = nm->entries_;

    addVirt(m, "qachannels/0/oscs/0/freq", "QAOSCFREQ", {0, 0}, NodeTypeIdx::Frequency);
    addVirt(m, "qachannels/0/oscs/1/freq", "QAOSCFREQ", {0, 1}, NodeTypeIdx::Frequency);
    addVirt(m, "qachannels/0/oscs/2/freq", "QAOSCFREQ", {0, 2}, NodeTypeIdx::Frequency);
    addVirt(m, "qachannels/0/oscs/3/freq", "QAOSCFREQ", {0, 3}, NodeTypeIdx::Frequency);
    addVirt(m, "qachannels/0/oscs/4/freq", "QAOSCFREQ", {0, 4}, NodeTypeIdx::Frequency);
    addVirt(m, "qachannels/0/oscs/5/freq", "QAOSCFREQ", {0, 5}, NodeTypeIdx::Frequency);
    addVirt(m, "qachannels/1/oscs/0/freq", "QAOSCFREQ", {1, 0}, NodeTypeIdx::Frequency);
    addVirt(m, "qachannels/1/oscs/1/freq", "QAOSCFREQ", {1, 1}, NodeTypeIdx::Frequency);
    addVirt(m, "qachannels/1/oscs/2/freq", "QAOSCFREQ", {1, 2}, NodeTypeIdx::Frequency);
    addVirt(m, "qachannels/1/oscs/3/freq", "QAOSCFREQ", {1, 3}, NodeTypeIdx::Frequency);
    addVirt(m, "qachannels/1/oscs/4/freq", "QAOSCFREQ", {1, 4}, NodeTypeIdx::Frequency);
    addVirt(m, "qachannels/1/oscs/5/freq", "QAOSCFREQ", {1, 5}, NodeTypeIdx::Frequency);
    addVirt(m, "qachannels/2/oscs/0/freq", "QAOSCFREQ", {2, 0}, NodeTypeIdx::Frequency);
    addVirt(m, "qachannels/2/oscs/1/freq", "QAOSCFREQ", {2, 1}, NodeTypeIdx::Frequency);
    addVirt(m, "qachannels/2/oscs/2/freq", "QAOSCFREQ", {2, 2}, NodeTypeIdx::Frequency);
    addVirt(m, "qachannels/2/oscs/3/freq", "QAOSCFREQ", {2, 3}, NodeTypeIdx::Frequency);
    addVirt(m, "qachannels/2/oscs/4/freq", "QAOSCFREQ", {2, 4}, NodeTypeIdx::Frequency);
    addVirt(m, "qachannels/2/oscs/5/freq", "QAOSCFREQ", {2, 5}, NodeTypeIdx::Frequency);
    addVirt(m, "qachannels/3/oscs/0/freq", "QAOSCFREQ", {3, 0}, NodeTypeIdx::Frequency);
    addVirt(m, "qachannels/3/oscs/1/freq", "QAOSCFREQ", {3, 1}, NodeTypeIdx::Frequency);
    addVirt(m, "qachannels/3/oscs/2/freq", "QAOSCFREQ", {3, 2}, NodeTypeIdx::Frequency);
    addVirt(m, "qachannels/3/oscs/3/freq", "QAOSCFREQ", {3, 3}, NodeTypeIdx::Frequency);
    addVirt(m, "qachannels/3/oscs/4/freq", "QAOSCFREQ", {3, 4}, NodeTypeIdx::Frequency);
    addVirt(m, "qachannels/3/oscs/5/freq", "QAOSCFREQ", {3, 5}, NodeTypeIdx::Frequency);

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

    addVirt(m, "sgchannels/0/oscs/0/freq", "SGOSCFREQ", {0, 0}, NodeTypeIdx::Frequency);
    addVirt(m, "sgchannels/0/oscs/1/freq", "SGOSCFREQ", {0, 1}, NodeTypeIdx::Frequency);
    addVirt(m, "sgchannels/0/oscs/2/freq", "SGOSCFREQ", {0, 2}, NodeTypeIdx::Frequency);
    addVirt(m, "sgchannels/0/oscs/3/freq", "SGOSCFREQ", {0, 3}, NodeTypeIdx::Frequency);
    addVirt(m, "sgchannels/0/oscs/4/freq", "SGOSCFREQ", {0, 4}, NodeTypeIdx::Frequency);
    addVirt(m, "sgchannels/0/oscs/5/freq", "SGOSCFREQ", {0, 5}, NodeTypeIdx::Frequency);
    addVirt(m, "sgchannels/0/oscs/6/freq", "SGOSCFREQ", {0, 6}, NodeTypeIdx::Frequency);
    addVirt(m, "sgchannels/0/oscs/7/freq", "SGOSCFREQ", {0, 7}, NodeTypeIdx::Frequency);
    addVirt(m, "sgchannels/0/sines/0/phaseshift", "SGSINEPHASE", {0, 0}, NodeTypeIdx::Phase);
    addVirt(m, "sgchannels/1/oscs/0/freq", "SGOSCFREQ", {1, 0}, NodeTypeIdx::Frequency);
    addVirt(m, "sgchannels/1/oscs/1/freq", "SGOSCFREQ", {1, 1}, NodeTypeIdx::Frequency);
    addVirt(m, "sgchannels/1/oscs/2/freq", "SGOSCFREQ", {1, 2}, NodeTypeIdx::Frequency);
    addVirt(m, "sgchannels/1/oscs/3/freq", "SGOSCFREQ", {1, 3}, NodeTypeIdx::Frequency);
    addVirt(m, "sgchannels/1/oscs/4/freq", "SGOSCFREQ", {1, 4}, NodeTypeIdx::Frequency);
    addVirt(m, "sgchannels/1/oscs/5/freq", "SGOSCFREQ", {1, 5}, NodeTypeIdx::Frequency);
    addVirt(m, "sgchannels/1/oscs/6/freq", "SGOSCFREQ", {1, 6}, NodeTypeIdx::Frequency);
    addVirt(m, "sgchannels/1/oscs/7/freq", "SGOSCFREQ", {1, 7}, NodeTypeIdx::Frequency);
    addVirt(m, "sgchannels/1/sines/0/phaseshift", "SGSINEPHASE", {1, 0}, NodeTypeIdx::Phase);
    addVirt(m, "sgchannels/2/oscs/0/freq", "SGOSCFREQ", {2, 0}, NodeTypeIdx::Frequency);
    addVirt(m, "sgchannels/2/oscs/1/freq", "SGOSCFREQ", {2, 1}, NodeTypeIdx::Frequency);
    addVirt(m, "sgchannels/2/oscs/2/freq", "SGOSCFREQ", {2, 2}, NodeTypeIdx::Frequency);
    addVirt(m, "sgchannels/2/oscs/3/freq", "SGOSCFREQ", {2, 3}, NodeTypeIdx::Frequency);
    addVirt(m, "sgchannels/2/oscs/4/freq", "SGOSCFREQ", {2, 4}, NodeTypeIdx::Frequency);
    addVirt(m, "sgchannels/2/oscs/5/freq", "SGOSCFREQ", {2, 5}, NodeTypeIdx::Frequency);
    addVirt(m, "sgchannels/2/oscs/6/freq", "SGOSCFREQ", {2, 6}, NodeTypeIdx::Frequency);
    addVirt(m, "sgchannels/2/oscs/7/freq", "SGOSCFREQ", {2, 7}, NodeTypeIdx::Frequency);
    addVirt(m, "sgchannels/2/sines/0/phaseshift", "SGSINEPHASE", {2, 0}, NodeTypeIdx::Phase);
    addVirt(m, "sgchannels/3/oscs/0/freq", "SGOSCFREQ", {3, 0}, NodeTypeIdx::Frequency);
    addVirt(m, "sgchannels/3/oscs/1/freq", "SGOSCFREQ", {3, 1}, NodeTypeIdx::Frequency);
    addVirt(m, "sgchannels/3/oscs/2/freq", "SGOSCFREQ", {3, 2}, NodeTypeIdx::Frequency);
    addVirt(m, "sgchannels/3/oscs/3/freq", "SGOSCFREQ", {3, 3}, NodeTypeIdx::Frequency);
    addVirt(m, "sgchannels/3/oscs/4/freq", "SGOSCFREQ", {3, 4}, NodeTypeIdx::Frequency);
    addVirt(m, "sgchannels/3/oscs/5/freq", "SGOSCFREQ", {3, 5}, NodeTypeIdx::Frequency);
    addVirt(m, "sgchannels/3/oscs/6/freq", "SGOSCFREQ", {3, 6}, NodeTypeIdx::Frequency);
    addVirt(m, "sgchannels/3/oscs/7/freq", "SGOSCFREQ", {3, 7}, NodeTypeIdx::Frequency);
    addVirt(m, "sgchannels/3/sines/0/phaseshift", "SGSINEPHASE", {3, 0}, NodeTypeIdx::Phase);
    addVirt(m, "sgchannels/4/oscs/0/freq", "SGOSCFREQ", {4, 0}, NodeTypeIdx::Frequency);
    addVirt(m, "sgchannels/4/oscs/1/freq", "SGOSCFREQ", {4, 1}, NodeTypeIdx::Frequency);
    addVirt(m, "sgchannels/4/oscs/2/freq", "SGOSCFREQ", {4, 2}, NodeTypeIdx::Frequency);
    addVirt(m, "sgchannels/4/oscs/3/freq", "SGOSCFREQ", {4, 3}, NodeTypeIdx::Frequency);
    addVirt(m, "sgchannels/4/oscs/4/freq", "SGOSCFREQ", {4, 4}, NodeTypeIdx::Frequency);
    addVirt(m, "sgchannels/4/oscs/5/freq", "SGOSCFREQ", {4, 5}, NodeTypeIdx::Frequency);
    addVirt(m, "sgchannels/4/oscs/6/freq", "SGOSCFREQ", {4, 6}, NodeTypeIdx::Frequency);
    addVirt(m, "sgchannels/4/oscs/7/freq", "SGOSCFREQ", {4, 7}, NodeTypeIdx::Frequency);
    addVirt(m, "sgchannels/4/sines/0/phaseshift", "SGSINEPHASE", {4, 0}, NodeTypeIdx::Phase);
    addVirt(m, "sgchannels/5/oscs/0/freq", "SGOSCFREQ", {5, 0}, NodeTypeIdx::Frequency);
    addVirt(m, "sgchannels/5/oscs/1/freq", "SGOSCFREQ", {5, 1}, NodeTypeIdx::Frequency);
    addVirt(m, "sgchannels/5/oscs/2/freq", "SGOSCFREQ", {5, 2}, NodeTypeIdx::Frequency);
    addVirt(m, "sgchannels/5/oscs/3/freq", "SGOSCFREQ", {5, 3}, NodeTypeIdx::Frequency);
    addVirt(m, "sgchannels/5/oscs/4/freq", "SGOSCFREQ", {5, 4}, NodeTypeIdx::Frequency);
    addVirt(m, "sgchannels/5/oscs/5/freq", "SGOSCFREQ", {5, 5}, NodeTypeIdx::Frequency);
    addVirt(m, "sgchannels/5/oscs/6/freq", "SGOSCFREQ", {5, 6}, NodeTypeIdx::Frequency);
    addVirt(m, "sgchannels/5/oscs/7/freq", "SGOSCFREQ", {5, 7}, NodeTypeIdx::Frequency);
    addVirt(m, "sgchannels/5/sines/0/phaseshift", "SGSINEPHASE", {5, 0}, NodeTypeIdx::Phase);
    addVirt(m, "sgchannels/6/oscs/0/freq", "SGOSCFREQ", {6, 0}, NodeTypeIdx::Frequency);
    addVirt(m, "sgchannels/6/oscs/1/freq", "SGOSCFREQ", {6, 1}, NodeTypeIdx::Frequency);
    addVirt(m, "sgchannels/6/oscs/2/freq", "SGOSCFREQ", {6, 2}, NodeTypeIdx::Frequency);
    addVirt(m, "sgchannels/6/oscs/3/freq", "SGOSCFREQ", {6, 3}, NodeTypeIdx::Frequency);
    addVirt(m, "sgchannels/6/oscs/4/freq", "SGOSCFREQ", {6, 4}, NodeTypeIdx::Frequency);
    addVirt(m, "sgchannels/6/oscs/5/freq", "SGOSCFREQ", {6, 5}, NodeTypeIdx::Frequency);
    addVirt(m, "sgchannels/6/oscs/6/freq", "SGOSCFREQ", {6, 6}, NodeTypeIdx::Frequency);
    addVirt(m, "sgchannels/6/oscs/7/freq", "SGOSCFREQ", {6, 7}, NodeTypeIdx::Frequency);
    addVirt(m, "sgchannels/6/sines/0/phaseshift", "SGSINEPHASE", {6, 0}, NodeTypeIdx::Phase);
    addVirt(m, "sgchannels/7/oscs/0/freq", "SGOSCFREQ", {7, 0}, NodeTypeIdx::Frequency);
    addVirt(m, "sgchannels/7/oscs/1/freq", "SGOSCFREQ", {7, 1}, NodeTypeIdx::Frequency);
    addVirt(m, "sgchannels/7/oscs/2/freq", "SGOSCFREQ", {7, 2}, NodeTypeIdx::Frequency);
    addVirt(m, "sgchannels/7/oscs/3/freq", "SGOSCFREQ", {7, 3}, NodeTypeIdx::Frequency);
    addVirt(m, "sgchannels/7/oscs/4/freq", "SGOSCFREQ", {7, 4}, NodeTypeIdx::Frequency);
    addVirt(m, "sgchannels/7/oscs/5/freq", "SGOSCFREQ", {7, 5}, NodeTypeIdx::Frequency);
    addVirt(m, "sgchannels/7/oscs/6/freq", "SGOSCFREQ", {7, 6}, NodeTypeIdx::Frequency);
    addVirt(m, "sgchannels/7/oscs/7/freq", "SGOSCFREQ", {7, 7}, NodeTypeIdx::Frequency);
    addVirt(m, "sgchannels/7/sines/0/phaseshift", "SGSINEPHASE", {7, 0}, NodeTypeIdx::Phase);

    return nm;
}

// ---------------------------------------------------------------------------
// SHFLI — 8 entries @0x1bbcb0
// ---------------------------------------------------------------------------
template <>
std::unique_ptr<NodeMap> GetNodeMap<AwgDeviceType::SHFLI>::get() {  // @0x1bbcb0
    auto nm = std::make_unique<NodeMap>();
    auto& m = nm->entries_;

    addVirt(m, "oscs/0/freq", "LIOSCFREQ", {0}, NodeTypeIdx::Frequency);
    addVirt(m, "oscs/1/freq", "LIOSCFREQ", {1}, NodeTypeIdx::Frequency);
    addVirt(m, "oscs/2/freq", "LIOSCFREQ", {2}, NodeTypeIdx::Frequency);
    addVirt(m, "oscs/3/freq", "LIOSCFREQ", {3}, NodeTypeIdx::Frequency);
    addVirt(m, "oscs/4/freq", "LIOSCFREQ", {4}, NodeTypeIdx::Frequency);
    addVirt(m, "oscs/5/freq", "LIOSCFREQ", {5}, NodeTypeIdx::Frequency);
    addVirt(m, "oscs/6/freq", "LIOSCFREQ", {6}, NodeTypeIdx::Frequency);
    addVirt(m, "oscs/7/freq", "LIOSCFREQ", {7}, NodeTypeIdx::Frequency);

    return nm;
}

// ---------------------------------------------------------------------------
// GHFLI — 8 entries @0x1bc030
// ---------------------------------------------------------------------------
template <>
std::unique_ptr<NodeMap> GetNodeMap<AwgDeviceType::GHFLI>::get() {  // @0x1bc030
    auto nm = std::make_unique<NodeMap>();
    auto& m = nm->entries_;

    addVirt(m, "oscs/0/freq", "LIOSCFREQ", {0}, NodeTypeIdx::Frequency);
    addVirt(m, "oscs/1/freq", "LIOSCFREQ", {1}, NodeTypeIdx::Frequency);
    addVirt(m, "oscs/2/freq", "LIOSCFREQ", {2}, NodeTypeIdx::Frequency);
    addVirt(m, "oscs/3/freq", "LIOSCFREQ", {3}, NodeTypeIdx::Frequency);
    addVirt(m, "oscs/4/freq", "LIOSCFREQ", {4}, NodeTypeIdx::Frequency);
    addVirt(m, "oscs/5/freq", "LIOSCFREQ", {5}, NodeTypeIdx::Frequency);
    addVirt(m, "oscs/6/freq", "LIOSCFREQ", {6}, NodeTypeIdx::Frequency);
    addVirt(m, "oscs/7/freq", "LIOSCFREQ", {7}, NodeTypeIdx::Frequency);

    return nm;
}

// ---------------------------------------------------------------------------
// VHFLI — 8 entries @0x1bc3b0
// ---------------------------------------------------------------------------
template <>
std::unique_ptr<NodeMap> GetNodeMap<AwgDeviceType::VHFLI>::get() {  // @0x1bc3b0
    auto nm = std::make_unique<NodeMap>();
    auto& m = nm->entries_;

    addVirt(m, "oscs/0/freq", "LIOSCFREQ", {0}, NodeTypeIdx::Frequency);
    addVirt(m, "oscs/1/freq", "LIOSCFREQ", {1}, NodeTypeIdx::Frequency);
    addVirt(m, "oscs/2/freq", "LIOSCFREQ", {2}, NodeTypeIdx::Frequency);
    addVirt(m, "oscs/3/freq", "LIOSCFREQ", {3}, NodeTypeIdx::Frequency);
    addVirt(m, "oscs/4/freq", "LIOSCFREQ", {4}, NodeTypeIdx::Frequency);
    addVirt(m, "oscs/5/freq", "LIOSCFREQ", {5}, NodeTypeIdx::Frequency);
    addVirt(m, "oscs/6/freq", "LIOSCFREQ", {6}, NodeTypeIdx::Frequency);
    addVirt(m, "oscs/7/freq", "LIOSCFREQ", {7}, NodeTypeIdx::Frequency);

    return nm;
}

// ---------------------------------------------------------------------------
// GetNodeMapDispatcher — dispatches to GetNodeMap<N>::get() for N > 4
// Template: GetNodeMapDispatcher<integer_sequence<AwgDeviceType, 8,16,32,64,128,256>>
// ---------------------------------------------------------------------------
namespace {

std::unique_ptr<NodeMap> dispatchHighDevType(AwgDeviceType devType) {  // @0x1ba360
    // Verified from disassembly: jump table at 0x9598f8 maps
    //   index 0 (SHFSG=16)    -> GetNodeMap<SHFSG>::get()    @0x1ba393
    //   index 1 (SHFQC_SG=32) -> GetNodeMap<SHFSG>::get()    @0x1ba393  (same!)
    //   index 3 (SHFLI=64)    -> GetNodeMap<SHFLI>::get()    @0x1ba39d
    //   index 7 (GHFLI=128)   -> GetNodeMap<GHFLI>::get()    @0x1ba3a7
    //   indices 2,4,5,6 + default -> GetNodeMap<VHFLI>::get() @0x1ba3b1
    // Note: SHFQA(8) is handled by an early-out at 0x1ba36c; everything
    // else (incl. 256/VHFLI) falls into the VHFLI branch.
    switch (static_cast<int>(devType)) {
        case 8:   return GetNodeMap<AwgDeviceType::SHFQA>::get();
        case 16:  return GetNodeMap<AwgDeviceType::SHFSG>::get();
        case 32:  return GetNodeMap<AwgDeviceType::SHFSG>::get();  // SHFQC_SG reuses SHFSG node tree (binary jump-table idx1 → 0x1ba393)
        case 64:  return GetNodeMap<AwgDeviceType::SHFLI>::get();
        case 128: return GetNodeMap<AwgDeviceType::GHFLI>::get();
        default:  return GetNodeMap<AwgDeviceType::VHFLI>::get();  // VHFLI(256) and fallthrough
    }
}

} // anonymous namespace

// Full dispatch matching initNodeMap @0x16b740 switch table.
//! \brief Return a freshly-allocated `NodeMap` populated with the
//!        parameter-tree schema for the given AWG device type.
//!
//! \details Dispatches on the numeric `AwgDeviceType` code. Family
//! codes 1, 2, 4 (UHFLI, HDAWG, UHFQA) map directly; higher codes
//! (8, 16, 32, 64, 128, 256) fan out through
//! `dispatchHighDevType`, which selects between SHFQA / SHFSG /
//! SHFLI / GHFLI / VHFLI specialisations and reuses the SHFSG
//! node tree for `SHFQC_SG` (code 32). Code 0 returns `nullptr`;
//! any code that falls off the high-dispatch switch is treated as
//! VHFLI as a last-resort fallback.
//! \param devType AWG device family enumerator.
//! \return Owning pointer to a populated `NodeMap`, or `nullptr`
//!         when `devType == 0` (no device).
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

