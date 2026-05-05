// ============================================================================
// Reconstructed from disassembly of _seqc_compiler.so
// Assembler method implementations
//
// Binary addresses noted in comments next to each function.
// ============================================================================

#include "zhinst/asm/assembler.hpp"

#include <algorithm>
#include <iomanip>
#include <map>
#include <sstream>

#include <boost/algorithm/string.hpp>

namespace zhinst {

// ============================================================================
// Global cmdMap — std::map<string, Assembler::Command>
// Located at 0xb84c20 in .bss, initialized by static constructor.
//
// 43 entries. The map is keyed by lowercase string; commandToString does
// a reverse (O(n)) linear scan to find the string for a given Command.
// ============================================================================
static const std::map<std::string, Assembler::Command>& getCmdMap() {
    static const std::map<std::string, Assembler::Command> cmdMap = {
        {"end",     Assembler::END},
        {"nop",     Assembler::NOP},
        {"msg",     Assembler::MESSAGE},
        {"rer",     Assembler::ERROR_MSG},
        {"prf",     Assembler::PRF},
        {"wvf",     Assembler::WVF},
        {"wvfi",    Assembler::WVFI},
        {"wvfs",    Assembler::WVFS_H},
        {"addi",    Assembler::ADDI},
        {"addiu",   Assembler::ADDIU},
        {"addr",    Assembler::ADDR},
        {"subr",    Assembler::SUBR},
        {"andr",    Assembler::ANDR},
        {"orr",     Assembler::ORR},
        {"xnorr",   Assembler::XNORR},
        {"ssl",     Assembler::SSL},
        {"ssr",     Assembler::SSR},
        {"xorr",    Assembler::XORR},
        {"andi",    Assembler::ANDI},
        {"andiu",   Assembler::ANDIU},
        {"ori",     Assembler::ORI},
        {"oriu",    Assembler::ORIU},
        {"xnori",   Assembler::XNORI},
        {"xnoriu",  Assembler::XNORIU},
        {"ld",      Assembler::LD},
        {"wtrig",   Assembler::WTRIG},
        {"wprf",    Assembler::WPRF},
        {"wwvfq",   Assembler::WPRF},      // alias — both map to 0xF0000000
        {"wwvf",    Assembler::WWVF},
        {"cwvf",    Assembler::CWVF},
        {"brz",     Assembler::BRZ},
        {"brnz",    Assembler::BRNZ},
        {"brgz",    Assembler::BRGZ},
        {"st",      Assembler::ST},
        {"trap",    Assembler::TRAP},
        {"irpt",    Assembler::IRPT},
        {"cwvfr",   Assembler::CWVFR},
        {"wvfe",    Assembler::WVFE},
        {"wvfei",   Assembler::WVFEI},
        {"wvfet",   Assembler::WVFET},
        {"wtrigi",  Assembler::WTRIGI},
        {"jmp",     Assembler::JMP},
        {"fb",      Assembler::FB},
    };
    return cmdMap;
}

// 0x28f7f0 — Linear scan of cmdMap (reverse lookup: Command → string).
std::string Assembler::commandToString(Command cmd) {
    const auto& map = getCmdMap();
    for (const auto& [name, val] : map) {
        if (val == cmd)
            return name;
    }
    return "";
}

// 0x2902f0 — Forward lookup: string → Command.
Assembler::Command Assembler::commandFromString(const std::string& name) {
    std::string lower = boost::to_lower_copy(name);
    const auto& map = getCmdMap();
    auto it = map.find(lower);
    if (it != map.end())
        return it->second;
    return INVALID;
}

// 0x28f8a0 — Classify opcode into scheduling categories.
int Assembler::getOpcodeType(Command cmd) {
    switch (cmd) {
    case NOP:
    case PRF:
    case WVF:
    case WVFI:
    case WVFS_H:
    case ADDI:
    case ADDIU:
    case ADDR:
    case SUBR:
    case ANDR:
    case ORR:
    case XNORR:
    case SSL:
    case SSR:
    case XORR:
    case ANDI:
    case ANDIU:
    case ORI:
    case ORIU:
    case XNORI:
    case XNORIU:
    case WTRIG:
        return 3;
    case LD:
        return 1;
    case WPRF:
    case WWVF:
    case CWVF:
    case BRZ:
    case BRNZ:
    case BRGZ:
    case ST:
    case TRAP:
    case IRPT:
    case CWVFR:
    case WVFE:
    case WVFEI:
    case WVFET:
    case WTRIGI:
    case JMP:
    case FB:
        return 4;
    default:
        return 0;
    }
}

// 0x28fac0 — Return cycle count for instruction.
int Assembler::getCycles(Command cmd) {
    switch (cmd) {
    case NOP:
    case PRF:
    case WVF:
    case WVFI:
    case WVFS_H:
    case ADDI:
    case ADDIU:
    case ADDR:
    case SUBR:
    case ANDR:
    case ORR:
    case XNORR:
    case SSL:
    case SSR:
    case XORR:
    case ANDI:
    case ANDIU:
    case ORI:
    case ORIU:
    case XNORI:
    case XNORIU:
    case LD:
    case WTRIG:
    case WPRF:
    case WWVF:
    case CWVF:
    case ST:
    case TRAP:
    case IRPT:
    case CWVFR:
    case WVFE:
    case WVFEI:
    case WVFET:
    case WTRIGI:
    case JMP:
    case FB:
        return 1;
    case BRZ:
    case BRNZ:
    case BRGZ:
        return 3;
    default:
        return 0;
    }
}

// 0x28fce0 — Return command type category.
int Assembler::getCmdType(Command cmd) {
    switch (cmd) {
    case PRF:
    case WVF:
    case WVFI:
    case WVFS_H:
    case BRZ:
    case BRNZ:
    case ST:
    case WTRIG:
    case CWVFR:
    case WVFE:
    case WVFEI:
    case WVFET:
        return 1;
    case LD:
        return 2;
    case ADDI:
    case ADDIU:
    case ANDI:
    case ANDIU:
    case ORI:
    case ORIU:
    case XNORI:
    case XNORIU:
        return 3;
    case ADDR:
    case SUBR:
    case ANDR:
    case ORR:
    case XNORR:
    case SSL:
    case SSR:
    case XORR:
        return 7;
    default:
        return 0;
    }
}

// 0x28fe70 — Return register ordering info.
int Assembler::getRegisterOrder(Command cmd) {
    switch (cmd) {
    case ADDR:
    case SUBR:
    case ANDR:
    case ORR:
    case XNORR:
    case SSL:
    case SSR:
    case XORR:
    case PRF:
    case WVF:
    case WVFI:
    case WTRIG:
        return 3;
     case WVFS_H:
    case BRZ:
    case BRNZ:
    case BRGZ:
    case ST:
    case CWVFR:
    case WVFE:
    case WVFET:
        return 1;
    case LD:
        return 2;
    case ADDI:
    case ADDIU:
    case ANDI:
    case ANDIU:
    case ORI:
    case ORIU:
    case XNORI:
    case XNORIU:
        return 4;
    default:
        return 0;
    }
}

// ============================================================================
// Assembler instance methods
// ============================================================================

// 0x28ffe0 — Returns packed int64_t: (1LL << 32) | regIndex if any valid
// registers, else 0. Checks regSrc, regDst, regAux and returns the highest.
int64_t Assembler::highestRegisterNumber() const {
    int maxReg = -1;
    bool found = false;

    if (regSrc.valid) {
        maxReg = regSrc.value;
        found = true;
    }
    if (regDst.valid && regDst.value > maxReg) {
        maxReg = regDst.value;
        found = true;
    }
    if (regAux.valid && regAux.value > maxReg) {
        maxReg = regAux.value;
        found = true;
    }

    if (!found)
        return 0;
    return (1LL << 32) | static_cast<uint8_t>(maxReg);
}

// 0x28ebd0 — Produce disassembly string.
std::string Assembler::str(bool verbose) const {
    // Special case: LABEL pseudo-instruction
    if (cmd == LABEL) {
        return label + ":";
    }

    std::ostringstream ss;
    ss << "\t" << commandToString(cmd);

    bool needComma = false;

    // Input immediates
    for (const auto& imm : immediates) {
        ss << " " << toString(imm);
        needComma = true;
        if (needComma) ss << ",";
    }

    // Registers: regDst, regAux, regSrc (printed in this order)
    auto emitReg = [&](const AsmRegister& r) {
        if (r.valid) {
            ss << " R" << r.value << ",";
            needComma = true;
        }
    };
    emitReg(regDst);
    emitReg(regAux);
    emitReg(regSrc);

    // Output immediates
    for (const auto& imm : outputs) {
        ss << " " << toString(imm) << ",";
        needComma = true;
    }

    // Label (branch target)
    if (!label.empty()) {
        ss << " " << label;
        needComma = false;  // label is last, no trailing comma
    }

    // Strip trailing comma
    std::string result = ss.str();
    if (!result.empty() && result.back() == ',') {
        result.pop_back();
    }

    // Verbose: pad and append comment
    if (verbose && !comment.empty()) {
        if (result.size() < 25) {
            result.append(25 - result.size(), ' ');
        }
        result += " // " + comment;
    }

    return result;
}

// ============================================================================
// Assembler destructor — 0x103980
// ============================================================================
Assembler::~Assembler() = default;

// Assembler::operator=(const Assembler&) @0x125e80
Assembler& Assembler::operator=(const Assembler& other) {
    if (this != &other) {
        cmd = other.cmd;
        immediates = other.immediates;
        regSrc = other.regSrc;
        regDst = other.regDst;
        regAux = other.regAux;
        outputs = other.outputs;
        label = other.label;
        comment = other.comment;
    }
    return *this;
}

// Assembler::operator=(Assembler&&) @0x125ab0
Assembler& Assembler::operator=(Assembler&& other) noexcept {
    if (this != &other) {
        cmd = other.cmd;
        immediates = std::move(other.immediates);
        regSrc = other.regSrc;
        regDst = other.regDst;
        regAux = other.regAux;
        outputs = std::move(other.outputs);
        label = std::move(other.label);
        comment = std::move(other.comment);
    }
    return *this;
}

// str(AsmOperationType) @0x28d280 — operand type to string
std::string str(AsmOperationType t) {
    switch (t) {
        case AsmOperationType::Cmd:   return "cmd";
        case AsmOperationType::Name:  return "name";
        case AsmOperationType::Value: return "value";
        case AsmOperationType::Reg:   return "reg";
        default:                      return "?";
    }
}

} // namespace zhinst
