// ============================================================================
// Reconstructed from disassembly of _seqc_compiler.so
// AWGAssemblerImpl opcode encoding methods
//
// These methods encode assembler instructions into 32-bit binary machine words.
// Each takes an opcode base value and an AsmExpression (parse tree node with
// children vector) and produces an encoded instruction word.
//
// Binary addresses:
//   getReg:   0x2892b0 - 0x289367
//   getVal:   0x289370 - 0x2895c0
//   opcode0:  0x2895c0 - 0x289860
//   opcode1:  0x289860 - 0x289a10
//   opcode2:  0x289a10 - 0x289c90
//   opcode3:  0x289c90 - 0x28a010
//   opcode4:  0x28a010 - 0x28a610
//   opcode5:  0x28a610 - 0x28a740
// ============================================================================

#include <memory>
#include <string>
#include <vector>

#include "zhinst/codegen/awg_assembler_impl.hpp"
#include "zhinst/asm/asm_expression.hpp"
#include "zhinst/asm/assembler.hpp"
#include "zhinst/core/error_messages.hpp"
#include "zhinst/device/device_constants.hpp"

namespace zhinst {

// ==========================================================================
// AsmExpression interface (reconstructed from usage in these methods):
//
// struct AsmExpression {
//     int type;                   // offset 0x00 — 1=register, 2=label/symbol, 3=integer literal
//     std::string name;           // offset 0x08 — symbol/label name (for type==2)
//     Assembler::Command command; // offset 0x38 — the command enum of the instruction
//     int value;                  // offset 0x3C — integer value (reg number for type==1, imm for type==3)
//     std::vector<std::shared_ptr<AsmExpression>> children;  // offset 0x40..0x57
//         // children[0] at +0x00, children[1] at +0x10, children[2] at +0x20, children[3] at +0x30
// };
//
// The children vector uses 16-byte elements (shared_ptr = ptr + control_block_ptr).
// children size = (end - begin) / 16.
// ==========================================================================

// ==========================================================================
// Instruction encoding formats:
//
// opcode0: No arguments. Returns just the base opcode.
//          Format: [31:0] = opcode (but opcode == 0 since no children expected)
//          Actually returns 0 (esi on the error path; the base opcode is just returned as-is)
//
// opcode1: 1 register + 1 immediate(20-bit)
//          Format: [31:24] = reg << 24
//                  [19:0]  = val(20)
//          children: [0]=reg, [1]=imm20
//
// opcode2: 1 register + 3 immediates(8-bit each)
//          Format: [31:24] = reg << 24
//                  [23:16] = val(8) << 16
//                  [15:8]  = val(8) << 8
//                  [7:0]   = val(8)
//          children: [0]=reg, [1]=imm8, [2]=imm8, [3]=imm8
//          children size must be 4 (0x40 bytes / 16)
//
// opcode3: 2 registers + 1 immediate(20-bit) OR special formats
//          Normal (>2 children): [31:24] = reg << 24 | opcode_base
//                                [23:20] = reg2 << 20
//                                [19:0]  = val(20)
//          For opcode==0x30000001 (WVFS_H): [31:24] = (val(1) | 0x30) << 24
//                                           then reg2<<20 | val(20)
//          Special case: if opcode==1, returns 0x40000000 (ADDI literal)
//
// opcode4: Complex dispatch based on opcode and child count (0, 1, or 2 children)
//          0 children: various fixed-format instructions (TRAP, IRPT, FB, WPRF, WWVF, etc.)
//          1 child: immediate-only instructions
//            - 0xFE000000 (JMP):   val(20) | 0xFE000000
//            - 0xFD000000 (WTRIGI): val(5) | 0xFD000000
//            - others:             val(24) | opcode_base
//          2 children: reg + immediate
//            - 0xF6000000 (ST):    reg<<20 | val(20) | 0xF6000000 (with bounds check)
//            - 0xD0..0xD2:         reg<<20 | val(20) | opcode_base
//            - 0xD3 (LD variant):  reg<<20 | val(20) | opcode_base
//            - others (>0xD3):     reg<<20 | val(20) | opcode_base
//
// opcode5: 2 immediates(14-bit each)
//          Format: [31:28] = opcode top nibble
//                  [27:14] = val(14) << 14
//                  [13:0]  = val(14)
//          children size must be 2 (0x20 bytes / 16)
// ==========================================================================


// --------------------------------------------------------------------------
// getReg — extract register number from AsmExpression child
// 0x2892b0
// --------------------------------------------------------------------------
unsigned int AWGAssemblerImpl::getReg(std::shared_ptr<AsmExpression> const& expr) // 0x2892b0
{
    AsmExpression* e = expr.get();

    if (e->type != 1) {
        // Not a register expression — report error (ErrorMessage #8)
        errorMessage(ErrorMessages::get(8));  // "expected register"
        return 0;
    }

    int regNum = e->value;  // offset 0x3C
    if (regNum < 0) {
        // Negative register number — report error (ErrorMessage #3)
        errorMessage(ErrorMessages::get(3));  // "invalid register"
        return 0;
    }

    // this_->deviceConstants->registerDepth is at this[0]->offset_0x28
    DeviceConstants* dc = *(DeviceConstants**)this;
    if ((unsigned long)regNum < dc->registerDepth) {  // offset 0x28 in DeviceConstants
        return (unsigned int)regNum;
    }

    // Register out of range — report error (ErrorMessage #4, "register out of range")
    errorMessage(ErrorMessages::get(4));
    return 0;
}


// --------------------------------------------------------------------------
// getVal — extract immediate value from AsmExpression child, masked to `bits` bits
// 0x289370
// --------------------------------------------------------------------------
unsigned int AWGAssemblerImpl::getVal(std::shared_ptr<AsmExpression> const& expr, int bits) // 0x289370
{
    AsmExpression* e = expr.get();

    switch (e->type) {
    case 2: {
        // Symbol/label reference — look up in label map (bimap at this+0xD8)
        // The bimap maps string->int (label name -> address)
        auto& labelMap = labelBimap_;
        auto it = labelMap.left.find(e->name);  // e->name at offset 0x08
        if (it == labelMap.left.end()) {
            // Label not found — throw out_of_range  
            // Actually: format error message 0x78 with label name
            std::string msg = ErrorMessages::format(LabelNotFound, e->name);
            errorMessage(msg);
            return 0;
        }
        int val = it->second;  // the integer value from bimap (at node+0x18)
        if (val < 0) {
            // Negative label value — report "undefined label" error
            std::string msg = ErrorMessages::format(LabelNotFound, e->name);
            errorMessage(msg);
            return 0;
        }
        unsigned int mask = (1u << bits) - 1;
        return (unsigned int)val & mask;
    }

    case 3: {
        // Integer literal
        int val = e->value;  // offset 0x3C
        unsigned int mask = (1u << bits) - 1;
        if (val <= static_cast<int>(mask)) {
            return static_cast<unsigned int>(val) & mask;
        }
        // Value overflow — report error #5 with (val, bits)
        std::string msg = ErrorMessages::format(static_cast<ErrorMessageT>(5), val, bits);
        errorMessage(msg);
        return 0;
    }

    default:
        // Invalid expression type — report error #9
        errorMessage(ErrorMessages::get(9));
        return 0;
    }
}


// --------------------------------------------------------------------------
// opcode0 — No-argument instruction (e.g., NOP, END)
// Expects 0 children. Returns the base opcode as-is.
// 0x2895c0
// --------------------------------------------------------------------------
uint64_t AWGAssemblerImpl::opcode0(unsigned int opcode, std::shared_ptr<AsmExpression> const& expr) // 0x2895c0
{
    AsmExpression* e = expr.get();

    // Check children count == 0 (children.begin == children.end)
    if (e->children.begin() != e->children.end()) {
        // Error: unexpected arguments
        std::string cmdName = Assembler::commandToString(static_cast<Assembler::Command>(e->command));
        std::string msg = ErrorMessages::format(static_cast<ErrorMessageT>(7), cmdName, 0, 0);
        errorMessage(msg);
    }

    return opcode;  // just return the base opcode unchanged
}


// --------------------------------------------------------------------------
// opcode1 — 1 register (bits [31:24]) + 1 immediate value (20-bit, bits [19:0])
// Expects 1-2 children: child[0]=reg(optional), child[1]=val(20)
// 0x289860
// --------------------------------------------------------------------------
uint64_t AWGAssemblerImpl::opcode1(unsigned int opcode, std::shared_ptr<AsmExpression> const& expr) // 0x289860
{
    AsmExpression* e = expr.get();

    auto* children_begin = e->children.data();
    auto* children_end = children_begin + e->children.size();
    size_t byte_size = reinterpret_cast<const char*>(children_end) - reinterpret_cast<const char*>(children_begin);  // in bytes of shared_ptr (16 each)

    // Expects children byte size > 0x10 (i.e., >= 2 children)
    if (byte_size <= 0x10) {
        // Wrong number of arguments: expected 1-2 range
        std::string cmdName = Assembler::commandToString(static_cast<Assembler::Command>(e->command));
        std::string msg = ErrorMessages::format(static_cast<ErrorMessageT>(7), cmdName, 1, 2);
        errorMessage(msg);
        return 0;
    }

    // Child[0] — register (optional; may be null shared_ptr)
    if (children_begin[0].get() != nullptr) {
        unsigned int reg = getReg(children_begin[0]);
        opcode |= (reg << 24);
    } else {
        // Null register child — error #1 "expected register at position 1"
        std::string msg = ErrorMessages::format(static_cast<ErrorMessageT>(1), 1, 1);
        errorMessage(msg);
    }

    // Child[1] — immediate value, 20 bits
    if (children_begin[1].get() != nullptr) {
        unsigned int val = getVal(children_begin[1], 20);
        opcode |= val;
    } else {
        // Null value child — error #2
        std::string msg = ErrorMessages::format(static_cast<ErrorMessageT>(2), 1, 1);
        errorMessage(msg);
    }

    return opcode;
}


// --------------------------------------------------------------------------
// opcode2 — 1 register (bits [31:24]) + 3 x 8-bit immediates ([23:16], [15:8], [7:0])
// Expects exactly 4 children (0x40 bytes = 4*16)
// 0x289a10
// --------------------------------------------------------------------------
uint64_t AWGAssemblerImpl::opcode2(unsigned int opcode, std::shared_ptr<AsmExpression> const& expr) // 0x289a10
{
    AsmExpression* e = expr.get();

    auto* children_begin = e->children.data();
    size_t byte_size = reinterpret_cast<const char*>(children_begin + e->children.size()) - reinterpret_cast<const char*>(children_begin);

    // Expects exactly 0x40 bytes = 4 children
    if (byte_size != 0x40) {
        std::string cmdName = Assembler::commandToString(static_cast<Assembler::Command>(e->command));
        std::string msg = ErrorMessages::format(static_cast<ErrorMessageT>(7), cmdName, 2, 4);
        errorMessage(msg);
        return 0;
    }

    // Child[0] — register
    if (children_begin[0].get() != nullptr) {
        unsigned int reg = getReg(children_begin[0]);
        opcode |= (reg << 24);
    } else {
        std::string msg = ErrorMessages::format(static_cast<ErrorMessageT>(1), 2, 1);
        errorMessage(msg);
    }

    // Child[1] — 8-bit value at [23:16]
    if (children_begin[1].get() != nullptr) {
        unsigned int val = getVal(children_begin[1], 8);
        opcode |= (val << 16);
    } else {
        std::string msg = ErrorMessages::format(static_cast<ErrorMessageT>(2), 2, 1);
        errorMessage(msg);
    }

    // Child[2] — 8-bit value at [15:8]
    if (children_begin[2].get() != nullptr) {
        unsigned int val = getVal(children_begin[2], 8);
        opcode |= (val << 8);
    } else {
        std::string msg = ErrorMessages::format(static_cast<ErrorMessageT>(2), 2, 2);
        errorMessage(msg);
    }

    // Child[3] — 8-bit value at [7:0]
    if (children_begin[3].get() != nullptr) {
        unsigned int val = getVal(children_begin[3], 8);
        opcode |= val;
    } else {
        std::string msg = ErrorMessages::format(static_cast<ErrorMessageT>(2), 2, 3);
        errorMessage(msg);
    }

    return opcode;
}


// --------------------------------------------------------------------------
// opcode3 — 2 registers + 1 immediate(20-bit)
// Complex: handles register-register ALU ops (0x6xxxxxxx) and WVFS_H (0x30000001)
// Expects 2-3 children depending on sub-format
// 0x289c90
// --------------------------------------------------------------------------
uint64_t AWGAssemblerImpl::opcode3(unsigned int opcode, std::shared_ptr<AsmExpression> const& expr) // 0x289c90
{
    // Special case: opcode == 1 returns literal 0x40000000
    if (opcode == 1) {
        return 0x40000000;
    }

    AsmExpression* e = expr.get();
    auto* children_begin = e->children.data();
    auto* children_end = children_begin + e->children.size();
    size_t byte_size = reinterpret_cast<const char*>(children_end) - reinterpret_cast<const char*>(children_begin);

    // Need > 0x10 bytes (> 1 child)
    if (byte_size <= 0x10) {
        // Wrong number of arguments
        std::string cmdName = Assembler::commandToString(static_cast<Assembler::Command>(e->command));
        size_t nChildren = e->children.size();
        // TODO: binary uses format(4, cmdName, 3, 2, nChildren) but msg 4 has no placeholders
        errorMessage(ErrorMessages::get(4));
        return 0;
    }

    // Check if this is a register-register ALU op (0x60000000..0x60000004) or XORR (0x60000007)
    bool isRegRegALU = ((opcode - 0x60000000u) < 5) || (opcode == 0x60000007u);

    if (isRegRegALU) {
        // These require exactly 2 children (0x20 bytes) — but may have 3 if there's a quirk
        if (byte_size != 0x20) {
            // Error #6 — wrong format for reg-reg op
            errorMessage(ErrorMessages::get(6));
            // reload children after error
            e = expr.get();
            children_begin = e->children.data();
            children_end = children_begin + e->children.size();
        }
    }

    size_t remaining = reinterpret_cast<const char*>(children_end) - reinterpret_cast<const char*>(children_begin);

    if (remaining == 0x30) {
        // 3 children: special handling for WVFS_H (0x30000001)
        if (opcode == 0x30000001) {
            // Child[0] is a 1-bit value for the format selector
            unsigned int val = getVal(children_begin[0], 1);
            opcode = ((val | 0x30) << 24);
            // Then children[1] = reg, children[2] = val(20) — fall through to 2-child tail
        } else {
            // Normal 3-child: child[0] = reg1
            if (children_begin[0].get() != nullptr) {
                unsigned int reg = getReg(children_begin[0]);
                opcode |= (reg << 24);
            } else {
                std::string msg = ErrorMessages::format(static_cast<ErrorMessageT>(1), 3, 1);
                errorMessage(msg);
            }
        }

        // children[1] = reg2
        e = expr.get();
        children_begin = e->children.data();
        if (children_begin[1].get() != nullptr) {
            unsigned int reg2 = getReg(children_begin[1]);
            opcode |= (reg2 << 20);
        } else {
            std::string msg = ErrorMessages::format(static_cast<ErrorMessageT>(2), 3, 1);
            errorMessage(msg);
        }

        // children[2] = val(20)
        e = expr.get();
        children_begin = e->children.data();
        if (children_begin[2].get() != nullptr) {
            unsigned int val = getVal(children_begin[2], 20);
            return opcode | val;
        } else {
            std::string msg = ErrorMessages::format(static_cast<ErrorMessageT>(2), 3, 1);
            errorMessage(msg);
            return opcode;
        }
    }

    // 2-child (0x20 byte) path: child[0]=reg1, child[1]=reg2/val
    // Used for register-register ALU and 2-arg forms

    // child[0] = destination register
    if (children_begin[0].get() != nullptr) {
        unsigned int reg = getReg(children_begin[0]);
        opcode |= (reg << 24);
    } else {
        std::string msg = ErrorMessages::format(static_cast<ErrorMessageT>(1), 3, 1);
        errorMessage(msg);
    }

    // child[1] = source register (shifted to [23:20])
    e = expr.get();
    children_begin = e->children.data();
    if (children_begin[1].get() != nullptr) {
        unsigned int reg2 = getReg(children_begin[1]);
        opcode |= (reg2 << 20);
    } else {
        std::string msg = ErrorMessages::format(static_cast<ErrorMessageT>(1), 3, 2);
        errorMessage(msg);
    }

    // child[2] = immediate(20) — only present for 3+ child cases
    // NOTE: The binary at 0x289f49 reads children[2] unconditionally even
    // when there are only 2 children. This relies on UB (reading past the
    // vector end) that happens to find null in the binary's allocator.
    // We add an explicit bounds check to avoid the UB crash.
    e = expr.get();
    children_begin = e->children.data();
    if (e->children.size() > 2 && children_begin[2].get() != nullptr) {
        unsigned int val = getVal(children_begin[2], 20);
        return opcode | val;
    } else if (e->children.size() > 2) {
        std::string msg = ErrorMessages::format(static_cast<ErrorMessageT>(2), 3, 1);
        errorMessage(msg);
        return opcode;
    } else {
        // 2-child reg-reg: no immediate, just return the opcode with registers
        return opcode;
    }
}


// --------------------------------------------------------------------------
// opcode4 — Complex dispatch: branch/load/store/special instructions
// Child count varies: 0, 1, or 2
// 0x28a010
// --------------------------------------------------------------------------
uint64_t AWGAssemblerImpl::opcode4(unsigned int opcode, std::shared_ptr<AsmExpression> const& expr) // 0x28a010
{
    AsmExpression* e = expr.get();
    auto* children_begin = e->children.data();
    size_t nChildren = e->children.size();

    if (nChildren == 2) {
        // --- 2 children: reg + immediate ---
        // Categorize by high byte of opcode
        // kOpcodeGroup2Child maps opcode range 0xF3..0xF6 (high byte) to indices 0..3
        constexpr unsigned int kOpcodeGroup2Child = 0x0D000000u;
        unsigned int opcodeGroup = (opcode + kOpcodeGroup2Child);
        unsigned int groupRotated = (opcodeGroup << 8) | (opcodeGroup >> 24);  // rol 8

        if (groupRotated >= 3) {
            if (groupRotated == 3) {
                // opcode == 0xF6000000 (ST): reg<<20 | val(20)
                // But first: child[0] = reg
                if (children_begin[0].get() != nullptr) {
                    unsigned int reg = getReg(children_begin[0]);
                    opcode = (reg << 20) | 0xF6000000;
                } else {
                    std::string msg = ErrorMessages::format(static_cast<ErrorMessageT>(1), 4, 1);
                    errorMessage(msg);
                    opcode = 0xF6000000;
                }
                // child[1] = val(20), with memory depth bounds check
                e = expr.get();
                children_begin = e->children.data();
                if (children_begin[1].get() != nullptr) {
                    unsigned int val = getVal(children_begin[1], 20);
                    // Bounds check: val < deviceConstants->memoryDepth (offset 0x30)
                    DeviceConstants* dc = *(DeviceConstants**)this;
                    if ((unsigned long)val >= dc->memoryDepth) {
                        // Error #10: address out of range
                        std::string msg = ErrorMessages::format(static_cast<ErrorMessageT>(10), dc->memoryDepth);
                        errorMessage(msg);
                    }
                    // Reload and get val again for the actual encoding
                    e = expr.get();
                    children_begin = e->children.data();
                    unsigned int val2 = getVal(children_begin[1], 20);
                    opcode |= val2;
                }
                return opcode;
            }

            // groupRotated > 3: other 2-child opcodes (0xD3+)
            // child[0] = reg (optional for some)
            if (children_begin[0].get() != nullptr) {
                unsigned int reg = getReg(children_begin[0]);
                opcode |= (reg << 20);
                e = expr.get();
                children_begin = e->children.data();
            }
            // child[1] = val(20)
            if (children_begin[1].get() != nullptr) {
                unsigned int val = getVal(children_begin[1], 20);
                opcode |= val;
            }
            return opcode;
        }

        // groupRotated < 3: 0xD0, 0xD1, 0xD2 (LD variants)
        // child[0] = reg
        if (children_begin[0].get() != nullptr) {
            unsigned int reg = getReg(children_begin[0]);
            opcode |= (reg << 20);
        } else {
            std::string msg = ErrorMessages::format(static_cast<ErrorMessageT>(1), 4, 1);
            errorMessage(msg);
        }
        // child[1] = val(20)
        e = expr.get();
        children_begin = e->children.data();
        if (children_begin[1].get() != nullptr) {
            unsigned int val = getVal(children_begin[1], 20);
            opcode |= val;
        } else {
            std::string msg = ErrorMessages::format(static_cast<ErrorMessageT>(2), 4, 1);
            errorMessage(msg);
        }
        return opcode;

    } else if (nChildren == 1) {
        // --- 1 child: immediate-only instructions ---
        // Dispatch by opcode high byte via jump table
        // kOpcodeGroup1Child maps opcode range 0xF2..0xFF (high byte) to indices 0..0xD
        constexpr unsigned int kOpcodeGroup1Child = 0x0E000000u;
        unsigned int idx = (opcode + kOpcodeGroup1Child);
        unsigned int idxRotated = (idx << 8) | (idx >> 24);  // rol 8

        if (idxRotated > 0xD) {
            // Unknown opcode for 1-child format — error
            std::string cmdName = Assembler::commandToString(static_cast<Assembler::Command>(e->command));
            std::string msg = ErrorMessages::format(static_cast<ErrorMessageT>(7), cmdName, 4, 2);
            errorMessage(msg);
            return 0;
        }

        // Jump table dispatch (addresses from 0x95d0ac):
        //  [0]  = 0xF2 → getVal(24)     (CWVF — but actually this maps to opcode0xa2??)
        //  [4]  = 0xF6 → getReg<<20 then val(20) (but 1-child?? — actually these are getVal)
        //  [7]  = same as [4]
        //  [10] = 0xFD → getVal(5) | 0xFD000000  (WTRIGI)
        //  [11] = 0xFE → getVal(20) | 0xFE000000 (JMP)
        //  [13] = same as [0]

        switch (idxRotated) {
        case 0:  // 0xF2000000 (CWVF) — or entry [0],[13]: getVal(24)
        case 13:
            return getVal(children_begin[0], 24) | opcode;

        case 10: // 0xFD000000 (WTRIGI): val(5)
        {
            unsigned int val = getVal(children_begin[0], 5);
            return val | 0xFD000000;
        }

        case 11: // 0xFE000000 (JMP): val(20)
        {
            unsigned int val = getVal(children_begin[0], 20);
            return val | 0xFE000000;
        }

        case 4:  // 0xF6 with 1 child: getReg<<20 | val (but this is odd for 1-child)
        case 7:  // similar
        {
            // These index to 0x28a213: check child[0], getReg<<20, then fetch val(20)
            if (children_begin[0].get() != nullptr) {
                unsigned int reg = getReg(children_begin[0]);
                opcode |= (reg << 20);
            } else {
                std::string msg = ErrorMessages::format(static_cast<ErrorMessageT>(1), 4, 1);
                errorMessage(msg);
            }
            return opcode;
        }

        default: // entries [1],[2],[3],[5],[6],[8],[9]: val(20) | opcode
            return getVal(children_begin[0], 20) | opcode;
        }

    } else if (nChildren == 0) {
        // --- 0 children: no-arg instructions with specific opcodes ---
        // Check for known zero-arg opcodes
        if (opcode == 0xF0000000 || opcode == 0xF1000000 ||  // WPRF, WWVF
            opcode == 0xF7000000 || opcode == 0xF8000000 ||  // TRAP, IRPT  
            opcode == 0xFF000000) {                           // FB
            return opcode;
        }

        // Unknown opcode for 0-child format — error with child count info
        std::string cmdName = Assembler::commandToString(static_cast<Assembler::Command>(e->command));
        size_t actualChildren = e->children.size();
        std::string msg = ErrorMessages::format(static_cast<ErrorMessageT>(4), cmdName, 4, 1, actualChildren);
        errorMessage(msg);
        return 0;

    } else {
        // > 2 children: error
        std::string cmdName = Assembler::commandToString(static_cast<Assembler::Command>(e->command));
        std::string msg = ErrorMessages::format(static_cast<ErrorMessageT>(7), cmdName, 4, 2);
        errorMessage(msg);
        return 0;
    }
}


// --------------------------------------------------------------------------
// opcode5 — 2 x 14-bit immediates
// Format: opcode_base | (val1 << 14) | val2
// Expects exactly 2 children (0x20 bytes)
// 0x28a610
// --------------------------------------------------------------------------
uint64_t AWGAssemblerImpl::opcode5(unsigned int opcode, std::shared_ptr<AsmExpression> const& expr) // 0x28a610
{
    AsmExpression* e = expr.get();
    auto* children_begin = e->children.data();
    size_t byte_size = reinterpret_cast<const char*>(children_begin + e->children.size()) - reinterpret_cast<const char*>(children_begin);

    // Expects exactly 0x20 bytes = 2 children
    if (byte_size != 0x20) {
        std::string cmdName = Assembler::commandToString(static_cast<Assembler::Command>(e->command));
        std::string msg = ErrorMessages::format(static_cast<ErrorMessageT>(7), cmdName, 5, 2);
        errorMessage(msg);
        return 0;
    }

    // Child[0] — 14-bit value at [27:14]
    if (children_begin[0].get() != nullptr) {
        unsigned int val = getVal(children_begin[0], 14);
        opcode |= (val << 14);

        // Reload expression for second child
        e = expr.get();
        children_begin = e->children.data();
    }

    // Child[1] — 14-bit value at [13:0]
    if (children_begin[1].get() != nullptr) {
        unsigned int val = getVal(children_begin[1], 14);
        opcode |= val;
    }

    return opcode;
}

} // namespace zhinst
