// Serial number predicates — device family identification by serial range
// Reconstructed from _seqc_compiler.so
//
// Each device family has a "short" serial range and a "long" serial range
// (10× the base, 5-10× the count). PQSC is the only single-range family.

#pragma once

#include <cstdint>

namespace zhinst {

bool isHf2Serial(unsigned int serial);     // @0x2dfe90 — [0,2000) ∪ [18000,20000)
bool isUhfSerial(unsigned int serial);     // @0x2dfeb0 — [2000,3000) ∪ [20000,22000)
bool isMfSerial(unsigned int serial);      // @0x2dfee0 — [3000,8000) ∪ [30000,40000)
bool isHdawgSerial(unsigned int serial);   // @0x2dff10 — [8000,10000) ∪ [80000,90000)
bool isPqscSerial(unsigned int serial);    // @0x2dff40 — [10000,12000)
bool isShfSerial(unsigned int serial);     // @0x2dff60 — [12000,13000) ∪ [120000,130000)
bool isShfaccSerial(unsigned int serial);  // @0x2dff90 — [16000,17000) ∪ [160000,170000)
bool isGhfSerial(unsigned int serial);     // @0x2dffc0 — [22000,23000) ∪ [220000,230000)
bool isQhubSerial(unsigned int serial);    // @0x2dfff0 — [24000,25000) ∪ [240000,250000)
bool isVhfSerial(unsigned int serial);     // @0x2e0020 — [13000,14000) ∪ [130000,140000)

} // namespace zhinst
