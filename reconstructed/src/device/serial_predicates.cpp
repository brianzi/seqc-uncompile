// Serial number predicates — device family identification by serial range
// Reconstructed from _seqc_compiler.so
//
// All use unsigned subtraction trick: (serial - BASE) < COUNT
// which is equivalent to serial >= BASE && serial < BASE + COUNT
// when interpreted as unsigned comparison.

#include "zhinst/device/serial_predicates.hpp"

namespace zhinst {

bool isHf2Serial(unsigned int s) {     // @0x2dfe90, 0x1b bytes
    return s < 2000u || (s - 18000u) < 2000u;
}

bool isUhfSerial(unsigned int s) {     // @0x2dfeb0, 0x24 bytes
    return (s - 2000u) < 1000u || (s - 20000u) < 2000u;
}

bool isMfSerial(unsigned int s) {      // @0x2dfee0, 0x24 bytes
    return (s - 3000u) < 5000u || (s - 30000u) < 10000u;
}

bool isHdawgSerial(unsigned int s) {   // @0x2dff10, 0x24 bytes
    return (s - 8000u) < 2000u || (s - 80000u) < 10000u;
}

bool isPqscSerial(unsigned int s) {    // @0x2dff40, 0x13 bytes
    return (s - 10000u) < 2000u;
}

bool isShfSerial(unsigned int s) {     // @0x2dff60, 0x24 bytes
    return (s - 12000u) < 1000u || (s - 120000u) < 10000u;
}

bool isShfaccSerial(unsigned int s) {  // @0x2dff90, 0x24 bytes
    return (s - 16000u) < 1000u || (s - 160000u) < 10000u;
}

bool isGhfSerial(unsigned int s) {     // @0x2dffc0, 0x24 bytes
    return (s - 22000u) < 1000u || (s - 220000u) < 10000u;
}

bool isQhubSerial(unsigned int s) {    // @0x2dfff0, 0x24 bytes
    return (s - 24000u) < 1000u || (s - 240000u) < 10000u;
}

bool isVhfSerial(unsigned int s) {     // @0x2e0020, 0x24 bytes
    return (s - 13000u) < 1000u || (s - 130000u) < 10000u;
}

} // namespace zhinst
