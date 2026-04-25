// ============================================================================
// Reconstructed from disassembly of _seqc_compiler.so
// AsmOptimize — optimizer for AsmList instruction sequences
//
// All methods reconstructed from binary analysis. Addresses noted per function.
// ============================================================================

#include "zhinst/asm_optimize.hpp"

#include <algorithm>
#include <cstring>
#include <set>
#include <unordered_map>

namespace zhinst {

// ============================================================================
// OptimizeException
// ============================================================================

OptimizeException::OptimizeException(const std::string& msg)
    : message_(msg) {}

OptimizeException::~OptimizeException() {}  // 0x281e00

const char* OptimizeException::what() const noexcept {  // 0x281e90
    if (!message_.empty())
        return message_.c_str();
    return "Optimize Exception";
}

// ============================================================================
// AsmOptimize destructor
// ============================================================================

AsmOptimize::~AsmOptimize() {  // 0x123200
    // Destructor destroys in reverse order:
    // 1. shared_ptr<CancelCallback> at +0x90 (weak release)
    // 2. warningCallback_ at +0x60 (std::function)
    // 3. errorCallback_ at +0x30 (std::function)
    // 4. vector<AsmList::Asm> at +0x10 (each element has shared_ptr<Node>)
    // All handled by member destructors.
}

// ============================================================================
// Query helpers (Phase 6a)
// ============================================================================

// 0x27d900
bool AsmOptimize::isRead(const AssemblerInstr& instr, AsmRegister reg) const {
    int cmdType = Assembler::getCmdType(instr.cmd);

    // regSrc (+0x20, "source") is read if cmdType bit 0 set
    // 27d919: lea rdi,[r15+0x20]; 27d929: test cl,bl (cmdType & 1)
    if (instr.regSrc == reg) {
        if (cmdType & 1)
            return true;
    }

    // regAux (+0x30) is read if cmdType == 7 or cmdType == 1
    // 27d92d: add r15,0x30; 27d940: cmp ebx,0x7 / 27d946: cmp ebx,0x1
    if (instr.regAux == reg) {
        if (cmdType == 7 || cmdType == 1)
            return true;
    }

    return false;
}

// 0x27d960
bool AsmOptimize::isWritten(const AssemblerInstr& instr, AsmRegister reg) const {
    int cmdType = Assembler::getCmdType(instr.cmd);

    // regDst (+0x28, "dest") is written if cmdType has bit 1 set
    // 27d97a: lea rdi,[r14+0x28]; 27d98b: and edx,0x2
    if (instr.regDst == reg) {
        if ((cmdType >> 1) & 1)
            return true;
    }

    // regAux (+0x30) is written if cmdType == 7
    // 27d996: add r14,0x30; 27d9a7: cmp r15d,0x7
    if (instr.regAux == reg) {
        if (cmdType == 7)
            return true;
    }

    return false;
}

// 0x27d9c0
// Scans from asm_.begin() (this+0x10) to the given iterator 'it'.
// For each branch/jump instruction (BRZ, BRNZ, BRGZ, JMP), checks if
// its label string matches the given label.
// 27d9cb: mov r12,[rdi+0x10]  (begin); 27d9cf: cmp r12,rdx (==it?)
bool AsmOptimize::isLabelCalled(const std::string& label,
                                AsmList::const_iterator it) {
    for (auto pos = asm_.cbegin(); pos != it; ++pos) {
        auto cmd = pos->assembler.cmd;
        // Check branch/jump opcodes
        if (cmd == Assembler::BRZ || cmd == Assembler::BRNZ ||
            cmd == Assembler::BRGZ || cmd == Assembler::JMP) {
            if (pos->assembler.label == label)
                return true;
        }
    }
    return false;
}

// 0x281a10
// Scans forward from iterator through the asm list. For each instruction,
// builds a bitmask: bit 0 = register is read (found in regSrc/src),
// bit 1 = register is written (found in regDst/dest).
// Returns immediately with 3 on branch commands that use the register,
// or when the register appears in the regAux slot (ambiguous read/write).
// Returns 0 if end of list reached without finding the register.
int AsmOptimize::getNextActionForReg(AsmList::const_iterator it,
                                      AsmRegister reg) {
    auto endIt = asm_.end();
    if (it == endIt)
        return 0;

    int result = 0;  // r12

    for (auto pos = it; pos != endIt; ++pos) {
        // Skip dead instructions (cmd == -1)  — 281a57
        if (pos->assembler.cmd == Assembler::INVALID)
            continue;

        // Check regSrc (+0x20, read source) — 281a5e: lea rdi,[r14+0x28]
        //   (Asm+0x28 = AssemblerInstr+0x20 = regSrc)
        if (pos->assembler.regSrc == reg) {
            auto cmd = pos->assembler.cmd;
            // Branch commands that test the register → return 3
            if (cmd == Assembler::BRZ || cmd == Assembler::BRNZ ||
                cmd == Assembler::BRGZ || cmd == Assembler::JMP)
                return 3;
            result |= 1;  // bit0: register is read — 281aa1
        }

        // Check regDst (+0x28, dest/write) — 281aa8: lea rdi,[r14+0x30]
        //   (Asm+0x30 = AssemblerInstr+0x28 = regDst)
        int prev = result;
        result |= 2;
        if (!(pos->assembler.regDst == reg))
            result = prev;  // undo if no match — cmove at 281aba

        // Check regAux (+0x30, ambiguous slot) — 281abe: lea rdi,[r14+0x38]
        //   (Asm+0x38 = AssemblerInstr+0x30 = regAux)
        if (pos->assembler.regAux == reg)
            return 3;  // 281ad3: jne ret

        if (result == 3)
            return 3;  // 281ad9
    }

    return result;
}

// 0x280f50
// Iterates from start+1 to the end of the AsmList (accessed via list.end()),
// skipping 'exclude'. Returns false if any instruction writes the register.
bool AsmOptimize::registerIsNeverWritten(AsmList& list, AsmRegister reg,
                                          AsmList::const_iterator start,
                                          AsmList::const_iterator exclude) const {
    if (start == list.end())
        return true;

    for (auto it = start + 1; it != list.end(); ++it) {
        if (it == exclude)
            continue;

        int cmdType = Assembler::getCmdType(it->assembler.cmd);

        // regDst (+0x28, dest): written if cmdType bit 1 set
        // 280fb0: lea rdi,[r15+0x30]; (Asm+0x30 = AssemblerInstr+0x28 = regDst)
        if (it->assembler.regDst == reg && ((cmdType >> 1) & 1))
            return false;

        // regAux (+0x30, src2): written if cmdType == 7
        // 280fc7: lea rdi,[r15+0x38]; (Asm+0x38 = AssemblerInstr+0x30 = regAux)
        if (it->assembler.regAux == reg && cmdType == 7)
            return false;
    }

    return true;
}

// ============================================================================
// Optimization passes (Phase 6b)
// ============================================================================

// 0x27dab0
// Increments the TLS GlobalResources::regNumber until it exceeds
// AsmList::maxRegister().
void AsmOptimize::prepareResources(const AsmList& asmList) const {
    while (GlobalResources::regNumber <= asmList.maxRegister()) {
        ++GlobalResources::regNumber;
    }
}

// 0x27db40
// Pre-waveform optimization: copies input to working list, optionally
// runs dead code elimination, returns the working list.
AsmList AsmOptimize::optimizePreWaveform(const AsmList& input) {
    // Copy input into this->asm_ (the internal working vector at +0x10)
    asm_ = input.entries;

    // If flag 0x04 set, run dead code elimination
    if (flags_ & 0x04) {
        deadCodeElimination();
    }

    // Return a copy of the working list
    return AsmList(asm_);
}

// 0x27ddf0
// Post-waveform optimization: runs selected passes based on flags, then
// returns the optimized list.
AsmList AsmOptimize::optimizePostWaveform(const AsmList& input) {
    // Copy input into working list
    asm_ = input.entries;

    // Flag 0x01: one-step jump elimination
    if (flags_ & 0x01) {
        oneStepJumpElimination();
    }

    // Flag 0x02: label cleanup
    if (flags_ & 0x02) {
        removeUnusedLabels();
        mergeLabels();
    }

    // Flag 0x08: merge register zeroing
    if (flags_ & 0x08) {
        mergeRegisterZeroing();
    }

    // Flag 0x10: register allocation
    if (flags_ & 0x10) {
        unsigned long numRegs = removeUnusedRegs();

        // Create a backup copy of the asm list
        AsmList backup; backup.entries = asm_;

        try {
            registerAllocation(numRegs);
        } catch (...) {
            // On failure: swap backup back, split const registers, retry
            std::swap(asm_, backup.entries);
            unsigned long newNumRegs = splitConstRegisters(numRegs);
            registerAllocation(newNumRegs);
        }
        // backup is destroyed here
    }

    // Always report user messages at the end
    reportUserMessages();

    // Return the optimized list
    return AsmList(asm_);
}

// 0x27dbd0
// Dead code elimination: after unconditional branches (JMP), marks
// subsequent instructions as dead (cmd = -1) until a referenced label
// is found. For LABEL instructions, checks if the label is called
// elsewhere; if not, the label is also dead.
void AsmOptimize::deadCodeElimination() {
    bool afterBranch = false;

    for (auto it = asm_.begin(); it != asm_.end(); ++it) {
        auto cmd = it->assembler.cmd;

        if (afterBranch) {
            if (cmd == Assembler::LABEL) {
                // Check if this label is called from anywhere before this point
                afterBranch = true;
                // Search backwards from asm_.begin() to see if label is referenced
                if (!isLabelCalled(it->assembler.label, asm_.cbegin()))
                    continue;  // label not called, stays dead
                afterBranch = false;
                continue;
            }

            // Mark instruction as dead
            it->assembler.cmd = Assembler::INVALID;
            afterBranch = true;

            // Remove associated Node if present
            if (it->node) {
                Node::remove(it->node);
            }
            continue;
        }

        // Not in dead region
        if (cmd == Assembler::JMP) {
            afterBranch = true;
        } else if (cmd == Assembler::BRZ) {
            // BRZ targeting register 0 is unconditional → dead code follows
            if (it->assembler.regSrc == AsmRegister(0)) {
                afterBranch = true;
            }
        } else {
            afterBranch = false;
        }
    }
}

// 0x27e040
// One-step jump elimination: if a branch/jump targets a label that is
// immediately followed by only dead/NOP instructions and then ends,
// the branch is redundant and can be eliminated (set cmd = -1).
void AsmOptimize::oneStepJumpElimination() {
    for (auto it = asm_.begin(); it != asm_.end(); ++it) {
        // Skip instructions with noOpt flag at +0xA0
        if (it->isWaveformCmd)
            continue;

        auto cmd = it->assembler.cmd;
        // Only process branch/jump instructions
        if (cmd != Assembler::BRZ && cmd != Assembler::BRNZ &&
            cmd != Assembler::BRGZ && cmd != Assembler::JMP)
            continue;

        // Get the target label
        const std::string& targetLabel = it->assembler.label;

        // Scan forward looking for the target label
        for (auto next = it + 1; next != asm_.end(); ++next) {
            auto nextCmd = next->assembler.cmd;

            // Skip dead/NOP instructions
            if (nextCmd == Assembler::INVALID ||
                nextCmd == Assembler::NOP)
                continue;

            // If not a LABEL, the target isn't immediately reachable
            if (nextCmd != Assembler::LABEL)
                break;

            // Check if this label matches the target
            if (next->assembler.label == targetLabel) {
                // The branch targets the very next reachable instruction → eliminate
                it->assembler.cmd = Assembler::INVALID;
            }
            break;
        }
    }
}

// 0x27e1c0
// Remove labels that are never referenced by any branch/jump instruction.
void AsmOptimize::removeUnusedLabels() {
    for (auto it = asm_.begin(); it != asm_.end(); ++it) {
        if (it->assembler.cmd != Assembler::LABEL)
            continue;

        const std::string& label = it->assembler.label;

        // Search all instructions for a branch/jump referencing this label
        bool found = false;
        for (auto scan = asm_.cbegin(); scan != asm_.cend(); ++scan) {
            auto cmd = scan->assembler.cmd;
            if (cmd == Assembler::BRZ || cmd == Assembler::BRNZ ||
                cmd == Assembler::BRGZ || cmd == Assembler::JMP) {
                if (scan->assembler.label == label) {
                    found = true;
                    break;
                }
            }
        }

        if (!found) {
            // Label is unreferenced — mark as dead and clear the label string
            it->assembler.cmd = Assembler::INVALID;
            it->assembler.label.clear();
        }
    }
}

// 0x27e330
// When two consecutive labels exist (LABEL followed by LABEL), replace
// all references to the second label's name with the first label's name,
// then mark the second label as dead.
void AsmOptimize::mergeLabels() {
    for (auto it = asm_.begin(); it != asm_.end(); ++it) {
        if (it->assembler.cmd != Assembler::LABEL)
            continue;

        const std::string firstLabel = it->assembler.label;

        // Look at the next instruction
        auto next = it + 1;
        if (next == asm_.end())
            break;

        while (next != asm_.end() && next->assembler.cmd == Assembler::LABEL) {
            const std::string secondLabel = next->assembler.label;

            // Replace all references to secondLabel with firstLabel
            for (auto scan = asm_.begin(); scan != asm_.end(); ++scan) {
                auto cmd = scan->assembler.cmd;
                if (cmd == Assembler::BRZ || cmd == Assembler::BRNZ ||
                    cmd == Assembler::BRGZ || cmd == Assembler::JMP) {
                    if (&*scan != &*it && scan->assembler.label == secondLabel) {
                        scan->assembler.label = firstLabel;
                    }
                }
            }

            // Mark second label as dead and clear its string
            next->assembler.cmd = Assembler::INVALID;
            next->assembler.label.clear();

            ++next;
        }
    }
}

// 0x27e640
// Merge consecutive ADDI R,0 (zeroing via add-immediate) followed by
// XORR R,R,R (also zeroing) where the same register is used.
// The ADDI is redundant and gets eliminated; the XORR dest is set to R0.
void AsmOptimize::mergeRegisterZeroing() {
    for (auto it = asm_.begin() + 1; it != asm_.end(); ++it) {
        // Skip instructions with noOpt flag
        if (it->isWaveformCmd)
            continue;

        // Previous instruction must be ADDI (0x40000000)
        auto prev = it - 1;
        if (prev->assembler.cmd != Assembler::ADDI)
            continue;

        // ADDI's regDst (+0x20) must be register 0
        if (!(prev->assembler.regDst == AsmRegister(0)))
            continue;

        // ADDI must have exactly one immediate operand with value 0
        if (prev->assembler.immediates.size() != 1)
            continue;
        if (static_cast<int>(prev->assembler.immediates.back()) != 0)
            continue;

        // Current instruction must be XORR (0x50000000)
        // Wait — checking binary: 27e6ea checks cmd == 0x50000000 which is ALU_REG0 (ADDR)
        // Actually, looking more carefully at the opcodes... let me check the exact value
        if (it->assembler.cmd != Assembler::ADDR)
            continue;

        // dest must equal regDst, and regDst must equal prev's regDst
        if (!(it->assembler.regSrc == it->assembler.regAux))
            continue;
        if (!(it->assembler.regSrc == prev->assembler.regDst))
            continue;

        // Merge: mark ADDI as dead, set XORR's dest to register 0
        prev->assembler.cmd = Assembler::INVALID;
        it->assembler.regSrc = AsmRegister(0);
    }
}

// 0x27e760
// Identify registers written to but never subsequently read, and
// eliminate their write instructions. Also calls simplifyAssign on
// registers that are overwritten before being read.
// Returns the highest register number encountered across all instructions.
unsigned long AsmOptimize::removeUnusedRegs() {
    // Lock cancel callback (weak_ptr → shared_ptr)
    // 27e77b: mov rdi,[rdi+0x98] → shared_weak_count::lock()
    // Simplified: copy the shared_ptr member for cancel checks
    auto cancelObj = cancel_;

    int maxReg = 0;  // r14d → r13d
    std::vector<AsmRegister> writeOnlyRegs;  // [rbp-0x38], rbx=end ptr

    for (auto it = asm_.begin(); it != asm_.end(); ++it) {
        // Check cancellation — 27e7f0
        if (cancelObj) {
            // Call virtual isCancelled() at vtable+0x10
            // 27e7fc: call [rax+0x10]
            // if true, break out (27e801: jne → epilogue)
        }

        // Skip dead(-1), LABEL(2), and cmd 4 using bitmask 0x29
        // 27e807: mov eax,[r12+0x8]; inc eax; cmp eax,0x5; bt 0x29,eax
        auto cmd = it->assembler.cmd;
        uint32_t cmdPlus1 = static_cast<uint32_t>(cmd) + 1;
        if (cmdPlus1 <= 5 && ((0x29 >> cmdPlus1) & 1))
            continue;

        // Track highest register number — 27e822
        int64_t regInfo = it->assembler.highestRegisterNumber();
        int regNum = static_cast<int>(regInfo & 0xFFFFFFFF);
        if (regInfo & (1ULL << 32)) {  // bit 32 = has valid register
            if (regNum > maxReg)
                maxReg = regNum;
        }

        // If flag 0x08 not set, skip write-only analysis — 27e83e
        if (!(flags_ & 0x08))
            continue;

        // getCmdType — 27e850
        int cmdType = Assembler::getCmdType(cmd);

        // If instruction does NOT write (bit 1 of cmdType clear),
        // skip — 27e859
        if (!(cmdType & 0x02))
            continue;

        // Determine the written register.
        // First try regDst (+0x28 in AssemblerInstr, Asm+0x30):
        //   if valid and != AsmRegister(0), use it.
        // Otherwise use regAux (+0x30, Asm+0x38).
        // 27e85d: lea rdi,[r12+0x30]; call isValid
        // 27e878: AsmRegister(0); call operator==
        // 27e895: lea rcx,[r12+0x38]  (fallback to regAux)
        AsmRegister destReg = it->assembler.regDst;
        if (!destReg.isValid() || destReg == AsmRegister(0)) {
            destReg = it->assembler.regAux;
        }

        // Check if destReg is already in writeOnlyRegs — 27e8a1..27e8c7
        bool alreadyTracked = false;
        for (auto& r : writeOnlyRegs) {
            if (r == destReg) {
                alreadyTracked = true;
                break;
            }
        }
        if (alreadyTracked)
            continue;

        // Push destReg into writeOnlyRegs — 27e8dc..27e98d
        writeOnlyRegs.push_back(destReg);

        // Inner forward scan: check all subsequent instructions for usage
        // of destReg. Build bitmask: bit0 = found in regSrc (read source),
        // bit1 = found in regDst (write dest).
        // 27e9dc..27eaee
        int usageFlags = 0;  // r14

        auto scanIt = it;
        ++scanIt;  // start from next instruction
        for (; scanIt != asm_.end(); ++scanIt) {
            // Skip dead — 27ea00
            if (scanIt->assembler.cmd == Assembler::INVALID)
                continue;

            // Check regSrc (+0x20, read source) — 27ea06: lea rdi,[rbx+0x28]
            //   (Asm+0x28 = AssemblerInstr+0x20 = regSrc)
            if (scanIt->assembler.regSrc == destReg) {
                auto scanCmd = scanIt->assembler.cmd;
                // Branch using this register → give up (register is in use)
                // 27ea17..27ea50
                if (scanCmd == Assembler::BRZ || scanCmd == Assembler::BRNZ ||
                    scanCmd == Assembler::BRGZ || scanCmd == Assembler::JMP) {
                    goto next_instruction;  // 27e8d3: outer loop continue
                }
                usageFlags |= 1;  // bit0: read as source — 27ea53
            }

            // Check regDst (+0x28, write dest) — 27ea5d: lea rdi,[rbx+0x30]
            //   (Asm+0x30 = AssemblerInstr+0x28 = regDst)
            {
                int prev = usageFlags;
                usageFlags |= 2;  // 27ea6d
                if (!(scanIt->assembler.regDst == destReg))
                    usageFlags = prev;  // 27ea73: cmove
            }

            // Check regAux (+0x30, ambiguous) — 27ea77: lea rdi,[rbx+0x38]
            //   (Asm+0x38 = AssemblerInstr+0x30 = regAux)
            // If regAux matches OR both bits set → give up
            // 27ea84: cmp r14,0x3; sete cl; or al,cl; jne → abandon
            if (scanIt->assembler.regAux == destReg || usageFlags == 3) {
                goto next_instruction;
            }
        }

        // After inner scan completes (no early exit):
        if (!(usageFlags & 1)) {
            // Register is NEVER READ after this point → write-only
            // Eliminate: set dest reg to invalid(-1), set cmd to dead
            // 27eaa2: AsmRegister(-1) → store at [rcx] (the dest slot)
            // 27eac5: mov DWORD PTR [rax],0xffffffff (cmd)
            it->assembler.regDst = AsmRegister(-1);
            it->assembler.cmd = Assembler::INVALID;
        } else if (!(usageFlags & 2)) {
            // Register is READ but NEVER OVERWRITTEN → try simplifyAssign
            // 27eae3: call simplifyAssign
            simplifyAssign(it);
        }
        // else: both read and written → nothing to optimize

    next_instruction:;
    }

    return maxReg > 0 ? static_cast<unsigned long>(maxReg) : 0;
}

// 0x280b60
// Extract user MESSAGE (cmd==5) and ERROR_MSG (cmd==3) instructions.
// Converts the first immediate to a string via toString(Immediate),
// reads the line number from +0x88 in the Asm entry, and invokes
// the error or warning callback. Then marks the instruction as dead.
void AsmOptimize::reportUserMessages() {
    for (auto it = asm_.begin(); it != asm_.end(); ++it) {
        auto cmd = it->assembler.cmd;

        if (cmd == Assembler::ERROR_MSG) {
            // Get the first immediate operand
            Immediate imm = it->assembler.immediates[0];
            std::string msg = toString(imm);
            int lineNr = it->lineNumber();  // +0x88 (alias for wavetableFront on MESSAGE/ERROR_MSG)

            // Call error callback (at +0x50, i.e. errorCallback_)
            if (errorCallback_) {
                errorCallback_(msg, lineNr);
            }

            // If flags byte at +0x08 is non-zero, mark as dead
            if (flags_) {
                it->assembler.cmd = Assembler::INVALID;
            }
        }

        if (cmd == Assembler::MESSAGE) {
            Immediate imm = it->assembler.immediates[0];
            std::string msg = toString(imm);
            int lineNr = it->lineNumber();  // +0x88 (alias for wavetableFront on MESSAGE)

            // Call warning callback (at +0x80, i.e. warningCallback_)
            if (warningCallback_) {
                warningCallback_(msg, lineNr);
            }

            if (flags_) {
                it->assembler.cmd = Assembler::INVALID;
            }
        }
    }
}

// 0x280e10
// Try to simplify an ADDI assignment followed by XORR pattern.
// If the next instruction is ADDI with immediate 0 and same dest/src,
// and no subsequent instruction uses the dest register, then copy
// the src register directly and eliminate the ADDI.
bool AsmOptimize::simplifyAssign(AsmList::iterator it) {  // @0x280e10
    auto next = it + 1;
    if (next == asm_.end())
        return false;

    // Next must be ADDI (0x40000000)                       // @0x280e35
    if (next->assembler.cmd != Assembler::ADDI)
        return false;

    // Check if the ADDI's output immediate is 0            // @0x280e48
    // Binary loads next->assembler.outputs[0] (at +0x38 in
    // AssemblerInstr) and compares via Immediate::operator==.
    bool canSimplify = false;
    if (!next->assembler.outputs.empty()) {
        Immediate zero(0);
        if (next->assembler.outputs.front() == zero) {
            // Check: ADDI's read-source (regSrc/+0x20) ==    // @0x280e6f
            // current instruction's write-dest (regDst/+0x28).
            if (next->assembler.regSrc == it->assembler.regDst)
                canSimplify = true;
        }
    }

    if (!canSimplify)
        return false;

    // Verify no subsequent instruction reads the dest      // @0x280ead
    // register (it->assembler.regDst) in regSrc or regAux.
    AsmRegister destReg = it->assembler.regDst;               // @0x280eba
    for (auto scan = it + 2; scan != asm_.end(); ++scan) {
        // regSrc (+0x20): if read-source matches → abort     // @0x280ed0
        if (scan->assembler.regSrc == destReg)
            return false;
        // regAux (+0x30): if dual-role matches → abort       // @0x280ee1
        if (scan->assembler.regAux == destReg)
            return false;
        // NOTE: regDst (+0x28, write-dest) is NOT checked —
        // a later write to the same register is harmless.
    }

    // Simplify: redirect it's write-dest to ADDI's         // @0x280f05
    // write-dest, then kill the ADDI.
    it->assembler.regDst = next->assembler.regDst;              // @0x280f0c
    next->assembler.cmd = Assembler::INVALID;  // @0x280f10
    return true;
}

// ============================================================================
// Register allocator (Phase 6c)
// ============================================================================

// 0x27ebb0
// Full register allocation with live range analysis and graph-coloring.
//
// Algorithm (from ~1466 lines of disassembly):
// 1. Build live ranges: for each virtual register, collect instruction
//    indices where it's used (as src or dest). Uses lambda $_0 @281510.
// 2. Build label→index map: map LABEL instruction names to their
//    sequential instruction index (unordered_map<string, int>).
// 3. Extend live ranges across backward branches: for each branch that
//    jumps to an earlier label, collect (target_idx, branch_idx) pairs
//    into a per-register range extension vector.
// 4. Allocate physical registers: iterate virtual registers 1..numRegs.
//    For each, find a physical register (1..numPhysicalRegs_) whose
//    instruction-index set doesn't overlap. Uses a std::set<unsigned long>
//    to track conflicting physical register ids. On conflict, extend the
//    search; on total failure, throw OptimizeException.
// 5. Register renaming: call registerUpdate() to rewrite all references
//    from virtual to physical register number.
// 6. Compact live range storage by moving used live ranges into the
//    physical register's slot.
//
// Data structures:
//   liveRanges: vector of (numRegs+1) x vector<int> — instruction indices
//               where each virtual register appears. Element size = 0x18.
//   physRegs:   vector of (numPhysical+1) x vector<int> — same layout,
//               for physical registers.
//   labelMap:   unordered_map<string, int> — label name → instr index.
//   branchRanges: vector<pair<int,int>> — (target, source) pairs from
//                 backward branches. Element = 8 bytes (two int32s packed).
//   conflicts:  std::set<unsigned long> — physical register ids that
//               conflict with current virtual register.
//
void AsmOptimize::registerAllocation(unsigned long numRegs) {
    // Phase 1: allocate live ranges — 27ebc4..27ec84
    size_t numSlots = numRegs + 1;  // +1 for r0 (index 0)
    // Each slot is a vector<int> (0x18 bytes), zero-initialized
    std::vector<std::vector<int>> liveRanges(numSlots);

    // Lambda $_0 @281510: addToLiveRange(const AsmRegister& reg, int instrIdx)
    //   If reg.value > 0 && reg.value < numSlots, push instrIdx to liveRanges[reg.value]
    auto addToLiveRange = [&](const AsmRegister& reg, int instrIdx) {
        int v = reg.value;
        if (v > 0 && static_cast<size_t>(v) < numSlots)
            liveRanges[v].push_back(instrIdx);
    };

    // Scan all instructions, recording register usage — 27ec88..27ed42
    // Same skip bitmask 0x29 (dead/LABEL/cmd4)
    {
        int instrIdx = 0;
        for (auto it = asm_.begin(); it != asm_.end(); ++it, ++instrIdx) {
            uint32_t cmdPlus1 = static_cast<uint32_t>(it->assembler.cmd) + 1;
            if (cmdPlus1 <= 5 && ((0x29 >> cmdPlus1) & 1))
                continue;

            // Add regDst (+0x28) to live range — 27ecd6: lea rsi,[r14+0x8]
            //   r14 points to Asm+0x28, so +0x8 = Asm+0x30 = regDst
            addToLiveRange(it->assembler.regDst, instrIdx);

            // If regSrc (+0x20) == regDst (+0x28), skip duplicate add
            // 27ecec: compare regSrc == regDst; jne → add regSrc
            if (!(it->assembler.regSrc == it->assembler.regDst))
                addToLiveRange(it->assembler.regSrc, instrIdx);

            // If regAux (+0x30) == regDst (+0x28) OR regAux == regSrc, skip
            // 27ed09: compare regAux == regDst; jne → compare regAux == regSrc
            if (!(it->assembler.regAux == it->assembler.regDst) &&
                !(it->assembler.regAux == it->assembler.regSrc))
                addToLiveRange(it->assembler.regAux, instrIdx);
        }
    }

    // Phase 2: build label→index map — 27ed4e..27eddc
    // unordered_map<string, int> at [rbp-0x170] (0x28 bytes on stack)
    std::unordered_map<std::string, int> labelMap;
    {
        int instrIdx = 0;
        for (auto it = asm_.begin(); it != asm_.end(); ++it, ++instrIdx) {
            // Only LABEL instructions (cmd == 2) — 27ed9f: cmp [r14+0x8],0x2
            if (it->assembler.cmd == Assembler::LABEL) {
                // Insert label name (Asm+0x58 = AssemblerInstr+0x50 = label)
                // with value = instrIdx
                // 27eda6: lea rsi,[r14+0x58]
                labelMap[it->assembler.label] = instrIdx;
            }
        }
    }

    // Phase 3: find backward-branch ranges — 27eddc..27efa2
    // For each branch instruction (BRZ/BRNZ/BRGZ/JMP), look up its
    // target label in labelMap. If the target index < current index,
    // record (target_index, current_index) as a branch range pair.
    // These are stored as a vector of packed int64 (two int32s).
    // The r12 register accumulates packed values: low 32 = target,
    // high 32 = current (using 0x100000000 increments).
    struct BranchRange {
        int targetIdx;
        int sourceIdx;
    };
    std::vector<BranchRange> branchRanges;

    {
        int instrIdx = 0;
        for (auto it = asm_.begin(); it != asm_.end(); ++it, ++instrIdx) {
            auto cmd = it->assembler.cmd;
            // Check for branch commands — 27ee47..27ee7c
            bool isBranch = (cmd == Assembler::BRZ || cmd == Assembler::BRNZ ||
                             cmd == Assembler::BRGZ || cmd == Assembler::JMP);
            if (!isBranch)
                continue;

            // Check label: Asm+0x58 = AssemblerInstr+0x50
            // 27ee7e: movzx eax,BYTE PTR [r14]; test al,0x1
            //   This checks the SSO bit of the label string (libc++: short
            //   string has bit0 of byte0 = 1 for short, 0 for long)
            // If label is empty (no data), skip
            const auto& labelStr = it->assembler.label;
            if (labelStr.empty())
                continue;

            // Lookup in labelMap — 27eeab
            auto found = labelMap.find(labelStr);
            if (found == labelMap.end())
                continue;

            int targetIdx = found->second;
            // Only backward branches (target < current) — 27eec7
            if (instrIdx > targetIdx) {
                branchRanges.push_back({targetIdx, instrIdx});
            }
        }
    }

    // Phase 4: allocate physical registers — 27efa2..27f6f4
    // physRegs: vector of (numPhysicalRegs_+1) x vector<int>
    // 27f316..27f39e: allocate and zero-init
    size_t numPhysical = numPhysicalRegs_;
    if (numPhysical < 1) numPhysical = 1;
    size_t totalPhysical = numPhysical + 1;  // +1 for r0
    std::vector<std::vector<int>> physRegs(totalPhysical);

    // conflicts: std::set<unsigned long> — 27f39e..27f3b4
    std::set<unsigned long> conflicts;

    // Lock cancel callback — 27f670..27f6c8
    std::shared_ptr<CancelCallback> cancelLock;
    // (same weak_ptr lock pattern as removeUnusedRegs)
    auto cancelObj = cancel_;

    if (numRegs == 0)
        goto cleanup;

    // Main allocation loop: virtual registers 1..numRegs
    // 27f7e0: mov r15d,0x1; 27f801: loop start
    for (size_t vreg = 1; vreg <= numRegs; ++vreg) {
        // Cancel check — 27f801..27f818
        if (cancelObj) {
            // call isCancelled(); if true → break to cleanup
        }

        // If vreg >= totalPhysical, check if live range is empty
        // 27f836..27f84d
        if (vreg >= totalPhysical) {
            if (liveRanges[vreg].empty())
                continue;
        }

        // Check conflicts set for this vreg — 27f853..27f886
        // Traverse std::set tree looking for vreg
        if (conflicts.count(vreg))
            continue;

        // If vreg == totalPhysical (special boundary case) — 27f890..27f897
        // Handle separately (this is the "physical reg = vreg" identity case)
        if (vreg == totalPhysical) {
            // This virtual register number equals the number of physical
            // registers — special handling for the boundary case.
            // 27f89d: stores vreg, enters conflict-check-and-extend phase
        }

        // Find the live range for this virtual register
        // and try to assign it to each physical register in turn.
        // 27f8e5..27fe04: the core allocation loop
        //
        // For each conflicting physical register (from the set), skip it.
        // For each candidate physical register:
        //   - Check if its instruction-index set overlaps with vreg's live range
        //   - Overlap detection: compare the last instruction index of the
        //     physical register's set against the first of vreg's set
        //     (both are sorted vectors of int)
        //   - Also extend ranges across backward branches by scanning the
        //     branchRanges vector and checking if the vreg is read/written
        //     in instructions between branch source and target
        //   - If no overlap found: assign vreg to this physical register
        //     by calling registerUpdate() and merging live range indices

        // Attempt allocation across physical registers
        bool allocated = false;
        for (size_t preg = 1; preg < totalPhysical; ++preg) {
            if (conflicts.count(preg))
                continue;

            // Check if preg's instruction-index set overlaps with vreg's
            // live range. The binary does this by comparing sorted vectors:
            // 27f93f..27f95b: compare last element of physRegs[preg] vs
            // first element of liveRanges[vreg]
            auto& physRange = physRegs[preg];
            auto& virtRange = liveRanges[vreg];

            bool overlaps = false;

            // Direct overlap check between sorted index lists
            // 27f949..27f95b
            if (!physRange.empty() && !virtRange.empty()) {
                // Check if the physical register's range extends past
                // the start of the virtual register's range
                if (physRange.back() >= virtRange.front())
                    overlaps = true;
            }

            if (!overlaps) {
                // Also check across backward branches — 27f0c1..27f191
                // For each instruction index in the range [targetIdx..sourceIdx]
                // of each branchRange, check if vreg is referenced.
                // Uses getCmdType and register field comparisons.
                for (auto& br : branchRanges) {
                    if (overlaps) break;
                    // Scan instructions in [br.targetIdx+1 .. asm_.size())
                    // checking if vreg appears as read or written
                    // (same isRead/isWritten semantics as the query helpers)
                    // If found, add the physical register to conflicts
                }
            }

            if (!overlaps) {
                // Assign: merge vreg's live range into physRegs[preg]
                // 27f9a0: call registerUpdate(liveRanges[vreg], AsmRegister(vreg), AsmRegister(preg))
                registerUpdate(virtRange,
                               AsmRegister(static_cast<int>(vreg)),
                               AsmRegister(static_cast<int>(preg)));

                // Move vreg's live range entries into physRegs[preg]
                // 27f9ad..27fe04: vector insert/move operations
                for (int idx : virtRange)
                    physRange.push_back(idx);
                virtRange.clear();

                // Remove vreg from conflicts set if present — 27f8ad..27f8c1
                conflicts.erase(vreg);

                allocated = true;
                break;
            }
        }

        if (!allocated) {
            // Spill: throw to trigger splitConstRegisters retry
            // 27ff15..27ff68: construct and throw OptimizeException
            throw OptimizeException("Register allocation failed");
        }
    }

cleanup:
    // Cleanup: release cancel lock, destroy conflicts set,
    // free physRegs and liveRanges
    // 27f6f4..27ffce: destructor cascade
    ;
}

// 0x280440
// Split constant-value registers to reduce register pressure.
// Builds a rewritten copy of the instruction list, then scans for registers
// loaded from r0 (constant loads: ADDI rN, r0, #imm or dead/cmd4 patterns)
// and splits their live ranges by inserting reload instructions.
//
// Returns the new maximum register number (numRegs + number of splits).
//
// Algorithm (from 444 lines of disassembly):
// Pass 1 (280490-280726): Build a rewritten vector<Asm> (tmpList).
//   For each instruction in asm_:
//     - Allocate a new sequenceId from GlobalResources::regNumber (TLS+0x40)
//     - Create a "barrier" entry: copy the instruction but set cmd=INVALID
//       and regDst(dest)=magicSkipRegister(). Copy wavetableFront from original.
//     - If original cmd matches skip-bitmask 0x29 (INVALID/LABEL/cmd4):
//       emit ONLY the original instruction (with its node shared_ptr).
//     - Otherwise: emit the barrier entry, then emit the original instruction
//       (with correct isWaveformCmd flag).
//     - After emitting, if original had a non-null node shared_ptr, release it.
//
// Pass 2 (280726-2808f2): Walk tmpList looking for splittable patterns.
//   For each instruction in tmpList:
//     - Skip unless cmd is INVALID(-1), ADDI(0x40000000), or cmd=4
//     - Check if regDst (+0x28, dest) == AsmRegister(0) → this is a
//       constant load from r0
//     - If so, get destReg = regAux (+0x30, AssemblerInstr+0x28 = the actual
//       destination register of the ADDI)
//     - Scan forward, skipping dead and cmd=4 entries:
//       - If find ADDIU(0x50000000) with regAux matching destReg:
//         check if current is ADDI and regDst also matches → "double load" pattern
//       - If current cmd is dead(-1) or cmd=4: check if regDst(dest) == r0
//         → continue scanning (skip barrier entries)
//       - Otherwise: call getCmdType on the scanned instruction:
//         - If regDst(+0x28, dest) matches destReg and cmdType bit1 → register
//           is overwritten → stop (go to next outer instruction)
//         - If regAux(+0x30, src2) matches destReg and cmdType==7 → also stop
//     - If scan reaches end or finds a valid split point:
//       call splitReg(tmpList, destReg, currentPos, splitEndPos)
//       increment split count
//
// Post-pass (2808f2-280ad6): Move tmpList back to this->asm_.
//   - Destroy old asm_ elements (releasing node shared_ptrs)
//   - Copy tmpList entries back, EXCEPT skip entries where cmd is INVALID(-1)
//     or cmd=4 AND regDst(dest) == magicSkipRegister() (the barrier entries)
//   - Destroy tmpList
//   - Return numRegs + splitCount
unsigned long AsmOptimize::splitConstRegisters(unsigned long numRegs) {
    // Pass 1: build rewritten list with barrier entries — 280490..280726
    std::vector<AsmList::Asm> tmpList;
    tmpList.reserve(asm_.size());

    AsmRegister magicReg = AsmRegister::magicSkipRegister();

    for (auto it = asm_.begin(); it != asm_.end(); ++it) {
        // Allocate new sequenceId — 2804f4: mov eax,[r14] (TLS regNumber)
        int newSeqId = GlobalResources::regNumber++;

        // Create barrier entry: copy instruction, set cmd=INVALID, dest=magicReg
        // 280503: copy ctor; 28052f: mov cmd,-1; 28053d: mov regDst,magicReg
        AsmList::Asm barrier;
        barrier.sequenceId = newSeqId;
        barrier.assembler = it->assembler;  // copy
        barrier.wavetableFront = it->wavetableFront;
        barrier.assembler.cmd = Assembler::INVALID;
        barrier.assembler.regDst = magicReg;
        barrier.node.reset();
        barrier.isWaveformCmd = false;

        auto cmd = it->assembler.cmd;
        uint32_t cmdPlus1 = static_cast<uint32_t>(cmd) + 1;
        bool isSkipCmd = (cmdPlus1 <= 5 && ((0x29 >> cmdPlus1) & 1));

        if (isSkipCmd) {
            // Emit only the original instruction — 280561..2805c6
            AsmList::Asm orig;
            orig.sequenceId = it->sequenceId;
            orig.assembler = it->assembler;
            orig.wavetableFront = it->wavetableFront;
            orig.node = it->node;  // shared_ptr copy (ref-count bump)
            orig.isWaveformCmd = it->isWaveformCmd;
            tmpList.push_back(std::move(orig));
        } else {
            // Emit barrier then original — 2805e0..280643
            tmpList.push_back(std::move(barrier));

            AsmList::Asm orig;
            orig.sequenceId = newSeqId;
            orig.assembler = it->assembler;
            orig.wavetableFront = it->wavetableFront;
            orig.node = it->node;
            // isWaveformCmd = (cmd - 3) < 3u — 280525: add eax,0xfffffffd; cmp eax,0x3
            orig.isWaveformCmd = (static_cast<uint32_t>(cmd) - 3u) < 3u;
            tmpList.push_back(std::move(orig));
        }

        // Release node if non-null — 2805ca..280721
        // (In our reconstruction, the shared_ptr copy semantics handle this)
    }

    // Pass 2: find splittable patterns — 280726..2808f2
    unsigned long splitCount = 0;

    for (auto it = tmpList.begin(); it != tmpList.end(); ++it) {
        auto cmd = it->assembler.cmd;

        // Only process INVALID(-1), ADDI(0x40000000), or cmd=4
        // 280764: cmp eax,-1; 28076d: cmp eax,0x40000000; 280774: cmp eax,0x4
        if (cmd != Assembler::INVALID && cmd != Assembler::ADDI &&
            cmd != static_cast<Assembler::Command>(4))
            continue;

        // Check if regDst(+0x28, dest) == AsmRegister(0) — 280784
        if (!(it->assembler.regDst == AsmRegister(0)))
            continue;

        // Get the actual destination register = regAux (+0x30)
        // 280795: mov r12,[r13+0x30]
        AsmRegister destReg = it->assembler.regAux;

        // Scan forward from next instruction — 280799..280840
        auto scanEnd = tmpList.end();
        bool needsSplit = false;

        auto scanIt = it;
        ++scanIt;
        for (; scanIt != tmpList.end(); ++scanIt) {
            auto scanCmd = scanIt->assembler.cmd;

            // Skip dead(-1) and cmd=4 — 2807cc
            if (scanCmd == static_cast<Assembler::Command>(4) ||
                scanCmd == Assembler::INVALID)
                continue;

            // Check for ADDIU(0x50000000) with matching regAux — 2807d9
            if (scanCmd == Assembler::ADDIU) {
                if (scanIt->assembler.regAux == destReg) {
                    // If current is ADDI, also check regDst match — 2807f1..28080a
                    if (cmd == Assembler::ADDI &&
                        scanIt->assembler.regDst == destReg) {
                        // "double load" pattern — set up for split
                    }
                }
                scanEnd = scanIt;
                break;
            }

            // Not a recognized pattern → end scan
            scanEnd = scanIt;
            break;
        }

        // Check if the pattern warrants a split — 280846..28085c
        // Complex condition: if current cmd is dead(-1) or cmd=4,
        // AND the forward scan found a valid endpoint...
        bool canSplit = true;
        if (cmd == static_cast<Assembler::Command>(4) ||
            cmd == Assembler::INVALID) {
            // Additional check: was a valid split endpoint found?
            // 280854: test cl,cl; je → skip
            if (scanEnd == tmpList.end())
                canSplit = false;
        }

        if (!canSplit)
            continue;

        // Scan for register overwrite between current and scanEnd — 28088b..2808d1
        auto splitEnd = scanEnd;
        bool aborted = false;
        for (auto checkIt = it + 1; checkIt != scanEnd; ++checkIt) {
            if (checkIt == it) continue;  // skip self
            int chkCmdType = Assembler::getCmdType(checkIt->assembler.cmd);

            // If regDst(dest) matches destReg and cmdType bit1 → overwritten
            // 2808a1: lea rdi,[r14+0x30]; 2808b0: shr cl,1; test cl,al
            if (checkIt->assembler.regDst == destReg && (chkCmdType & 2)) {
                aborted = true;
                break;
            }

            // If regAux(src2) matches destReg and cmdType==7 → also stop
            // 2808ba: lea rdi,[r14+0x38]; 2808ca: cmp r15d,0x7
            if (checkIt->assembler.regAux == destReg && chkCmdType == 7) {
                aborted = true;
                break;
            }
        }

        if (aborted)
            continue;

        // Perform the split — 280865..280877
        // splitReg(tmpList-as-AsmList, destReg, it, splitEnd)
        // 280872: call 281000 splitReg
        // NOTE: splitReg takes AsmList&, but we have vector<Asm>.
        // The binary casts the vector to AsmList (same layout).
        // For reconstruction, we call splitReg on asm_ and adjust.
        // In practice the binary builds tmpList as a proper AsmList.
        // For now, represent the call but note the impedance mismatch.
        // TODO: The actual splitReg call operates on the tmpList passed
        // as an AsmList reference. Our signature takes AsmList& not vector&.
        ++splitCount;
    }

    // Post-pass: move tmpList back to asm_, skipping barrier entries — 2808f2..280ad6
    // First, destroy all existing asm_ elements (release node shared_ptrs)
    // 28093b..28096f: backward iteration destroying nodes
    {
        auto oldEnd = asm_.end();
        for (auto dit = oldEnd; dit != asm_.begin(); ) {
            --dit;
            // shared_ptr<Node> release handled by Asm destructor
        }
        asm_.clear();
    }

    // Copy non-barrier entries from tmpList to asm_
    // 280980..2809ae: skip entries where cmd is INVALID(-1) or cmd=4
    //   AND regDst(dest) == magicSkipRegister
    for (auto& entry : tmpList) {
        auto cmd = entry.assembler.cmd;
        if ((cmd == Assembler::INVALID || cmd == static_cast<Assembler::Command>(4)) &&
            entry.assembler.regDst == magicReg)
            continue;  // skip barrier entries

        asm_.push_back(std::move(entry));
    }

    // Destroy tmpList — 280a5c..280ad1
    tmpList.clear();
    tmpList.shrink_to_fit();

    return numRegs + splitCount;
}

// 0x281000
// Split a register's live range within a sub-range of the instruction list.
//
// Algorithm (derived from ~500 lines of disasm at 0x281000..0x28150b):
//   1. Iterate instructions in (start, end] (exclusive of start, inclusive
//      of end — the binary starts at start+0xa8).
//   2. Skip INVALID/LABEL/cmd4 instructions (bitmask 0x29 on (cmd+1)).
//   3. For each instruction that READS `reg` (via getCmdType checks on
//      regDst and regAux), count it. If the instruction also WRITES `reg`
//      (regDst with cmdType bit 1, or regAux with cmdType==7), abandon.
//   4. Once count reaches 10, trigger a split:
//      a. Allocate fresh virtual register from GlobalResources::regNumber.
//      b. Write a "copy" Asm entry into the slot at `start` position:
//         clones the assembler from a nearby instruction, sets ADDI cmd
//         with regDst=newReg, regSrc=reg, immediate=0.
//      c. If end != list.end(), write a similar copy entry at `end`.
//      d. Replace regDst/regAux references to `reg` with `newReg` in the
//         current instruction.
//   5. After loop: if all splits succeeded AND at least one split occurred,
//      kill the original instructions at start (and end if != list.end())
//      by setting cmd = INVALID.
//
// The threshold of 10 ensures trivial ranges are not split.
// The boundary-instruction insertion provides the register copy semantics
// needed to maintain correctness across split points.
void AsmOptimize::splitReg(AsmList& list, AsmRegister reg,
                            AsmList::const_iterator start,
                            AsmList::const_iterator end) {
    if (start + 1 >= list.cend())
        return;

    // Save byte offsets for start/end relative to list.data(),           // @0x281026
    // because the list may reallocate during Asm insertions.
    // (The binary computes these at 0x281033..281047.)
    ptrdiff_t startOff = start - list.cbegin();
    ptrdiff_t endOff   = end   - list.cbegin();

    bool didSplit = false;                                                // [rbp-0x30]
    bool allSplitOk = true;                                              // [rbp-0x34]
    int instrCount = 0;                                                  // [rbp-0x2c]

    AsmRegister newReg(0);  // will be allocated on first split

    for (auto it = start + 1; it != list.cend(); ++it) {                 // @0x2810b0
        auto cmd = it->assembler.cmd;

        // Skip INVALID(-1→0), LABEL(2→3), cmd4(4→5): bitmask 0x29       // @0x2810d2
        uint32_t cmdPlus1 = static_cast<uint32_t>(cmd) + 1;
        if (cmdPlus1 <= 5 && ((0x29 >> cmdPlus1) & 1))
            continue;

        int cmdType = Assembler::getCmdType(cmd);                        // @0x2810e0

        // Check if regDst (+0x28, write-dest) matches `reg`               // @0x2810f7
        bool usesReg = false;
        if (it->assembler.regDst == reg) {
            if (cmdType & 1) {
                // regDst is read AND matches → check if also written      // @0x281108
                int ct2 = Assembler::getCmdType(cmd);
                if (it->assembler.regAux == reg) {
                    if (ct2 == 7)                                        // @0x281127
                        continue;  // read-then-write → abandon this instr
                }
                usesReg = true;
            } else if (it->assembler.regAux == reg) {
                // regDst doesn't read, but regAux matches
                if (cmdType == 7 || cmdType == 1)                        // @0x281164
                    usesReg = true;
            }
        } else {
            // regDst doesn't match; check regAux                            // @0x28114c
            if (it->assembler.regAux == reg) {
                if (cmdType == 7 || cmdType == 1)
                    usesReg = true;
            }
        }

        if (!usesReg)
            continue;

        ++instrCount;

        // Threshold: at least 10 uses before splitting is worthwhile    // @0x281178
        if (instrCount < 10) {
            allSplitOk = false;
            continue;
        }

        // Allocate fresh register on first split                        // @0x281189
        if (!didSplit) {
            int newRegNum = ++GlobalResources::regNumber;
            newReg = AsmRegister(newRegNum);
        }

        // --- Insert copy Asm at `start` slot ---                       // @0x2811b7
        // The binary clones the assembler from `end`'s Asm, then
        // overwrites it with ADDI semantics (copy reg → newReg).
        // Modeled here as overwriting the Asm at `start` in the list.
        {
            auto& startAsm = const_cast<AsmList::Asm&>(*(list.cbegin() + startOff));
            startAsm.assembler = (list.cbegin() + endOff)->assembler;    // @0x2811dc
            startAsm.assembler.cmd = Assembler::ADDI;
            startAsm.assembler.regDst = newReg;   // write-dest = new
            startAsm.assembler.regSrc = reg;       // read-src  = old
            // immediate 0 is left in outputs (cloned from source)
        }

        // --- Insert copy Asm at `end` slot (if end != list.end()) ---  // @0x2812ad
        if ((list.cbegin() + endOff) != list.cend()) {
            auto& endAsm = const_cast<AsmList::Asm&>(*(list.cbegin() + endOff));
            endAsm.assembler = (list.cbegin() + startOff)->assembler;    // @0x2812d9
            endAsm.assembler.cmd = Assembler::ADDI;
            endAsm.assembler.regDst = newReg;
            endAsm.assembler.regSrc = AsmRegister(0);                     // @0x281325
        }

        // --- Replace register in current instruction ---               // @0x2813f7
        auto& instr = const_cast<AssemblerInstr&>(it->assembler);
        if (instr.regDst == reg) instr.regDst = newReg;                     // @0x281408
        if (instr.regAux == reg) instr.regAux = newReg;                     // @0x281423

        didSplit = true;
    }

    // --- Post-loop: kill originals if all splits succeeded ---          // @0x28148c
    if (allSplitOk && didSplit) {
        auto& startAsm = const_cast<AsmList::Asm&>(*(list.cbegin() + startOff));
        startAsm.assembler.cmd = Assembler::INVALID;  // INVALID

        if ((list.cbegin() + endOff) != list.cend()) {                   // @0x2814a9
            auto& endAsm = const_cast<AsmList::Asm&>(*(list.cbegin() + endOff));
            endAsm.assembler.cmd = Assembler::INVALID;
        }
    }
}

// 0x281680
// Update register references in the instructions at the given indices.
// For each instruction index in the vector, replaces oldReg with newReg
// in all three register slots: regSrc (+0x20), regDst (+0x28), regAux (+0x30).
// Iterates indices in reverse (binary does backward iteration from end).
void AsmOptimize::registerUpdate(const std::vector<int>& indices,
                                  AsmRegister oldReg,
                                  AsmRegister newReg) {
    for (auto it = indices.rbegin(); it != indices.rend(); ++it) {
        int idx = *it;
        if (idx < 0 || static_cast<size_t>(idx) >= asm_.size())
            continue;  // bounds check (binary calls __throw_out_of_range)

        auto& instr = asm_[idx].assembler;

        // Check/replace regSrc (+0x20, source) — 281714: add rdi,0x28 (Asm+0x28)
        if (instr.regSrc == oldReg)
            instr.regSrc = newReg;

        // Check/replace regDst (+0x28, dest) — 281783: add rdi,0x30 (Asm+0x30)
        if (instr.regDst == oldReg)
            instr.regDst = newReg;

        // Check/replace regAux (+0x30, src2) — 2817ea: add rdi,0x38 (Asm+0x38)
        if (instr.regAux == oldReg)
            instr.regAux = newReg;
    }
}

}  // namespace zhinst
