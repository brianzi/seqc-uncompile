// ============================================================================
// Reconstructed from disassembly of _seqc_compiler.so
// AsmOptimize — optimizer for AsmList instruction sequences
//
// All methods reconstructed from binary analysis. Addresses noted per function.
// ============================================================================

#include "zhinst/asm/asm_optimize.hpp"
#include "zhinst/runtime/resources.hpp"

#include <algorithm>
#include <cstring>
#include <set>
#include <iostream>
#include <cstdlib>
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
// AsmOptimize constructor
// ============================================================================

// Constructed inline in Compiler::compile() at 0x120707
AsmOptimize::AsmOptimize(std::function<void(const std::string&, int)> errorCallback,
                         std::function<void(const std::string&, int)> warningCallback,
                         uint32_t numPhysicalRegs,
                         uint32_t flags,
                         std::shared_ptr<CancelCallback> cancel)
    : numPhysicalRegs_(numPhysicalRegs)
    , pad04_(0)
    , optFlags_(flags)
    , pad0C_(0)
    , asm_()
    , errorCallback_(std::move(errorCallback))
    , warningCallback_(std::move(warningCallback))
    , cancel_(std::move(cancel))
{}

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
bool AsmOptimize::isRead(const Assembler& instr, AsmRegister reg) const {
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
bool AsmOptimize::isWritten(const Assembler& instr, AsmRegister reg) const {
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
        //   (Asm+0x28 = Assembler+0x20 = regSrc)
        if (pos->assembler.regSrc == reg) {
            auto cmd = pos->assembler.cmd;
            // Branch commands that test the register → return 3
            if (cmd == Assembler::BRZ || cmd == Assembler::BRNZ ||
                cmd == Assembler::BRGZ || cmd == Assembler::JMP)
                return 3;
            result |= 1;  // bit0: register is read — 281aa1
        }

        // Check regDst (+0x28, dest/write) — 281aa8: lea rdi,[r14+0x30]
        //   (Asm+0x30 = Assembler+0x28 = regDst)
        int prev = result;
        result |= 2;
        if (!(pos->assembler.regDst == reg))
            result = prev;  // undo if no match — cmove at 281aba

        // Check regAux (+0x30, ambiguous slot) — 281abe: lea rdi,[r14+0x38]
        //   (Asm+0x38 = Assembler+0x30 = regAux)
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
        // 280fb0: lea rdi,[r15+0x30]; (Asm+0x30 = Assembler+0x28 = regDst)
        if (it->assembler.regDst == reg && ((cmdType >> 1) & 1))
            return false;

        // regAux (+0x30, src2): written if cmdType == 7
        // 280fc7: lea rdi,[r15+0x38]; (Asm+0x38 = Assembler+0x30 = regAux)
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
    if (optFlags_ & 0x04) {
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
    if (optFlags_ & 0x01) {
        oneStepJumpElimination();
    }

    // Flag 0x02: label cleanup
    if (optFlags_ & 0x02) {
        removeUnusedLabels();
        mergeLabels();
    }

    // Flag 0x08: merge register zeroing
    if (optFlags_ & 0x08) {
        mergeRegisterZeroing();
    }

    // Flag 0x10: register allocation
    if (optFlags_ & 0x10) {
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
                if (!isLabelCalled(it->assembler.label, it))
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
        if (it->noOpt)
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
        if (it->noOpt)
            continue;

        // Previous instruction must be ADDI (0x40000000)
        auto prev = it - 1;
        if (prev->assembler.cmd != Assembler::ADDI)
            continue;

        // ADDI's regSrc (+0x20) must be register 0
        // Binary at 0x27e6a8-0x27e6c4: lea -0x80(%r13) = prev.regSrc, cmp with R0
        if (!(prev->assembler.regSrc == AsmRegister(0)))
            continue;

        // ADDI must have exactly one output operand with value 0
        // Binary at 0x27e6c6-0x27e6e2: checks outputs vector size==0x20 (1 element)
        // then calls Immediate::operator int() and checks result == 0
        if (prev->assembler.outputs.size() != 1)
            continue;
        if (static_cast<int>(prev->assembler.outputs.back()) != 0)
            continue;

        // Current instruction must be ADDIU (0x50000000)
        // Binary at 0x27e6ea: cmpl $0x50000000,0x8(%r13)
        if (it->assembler.cmd != Assembler::ADDIU)
            continue;

        // current.regSrc == current.regDst
        // Binary at 0x27e6f4-0x27e706: lea 0x28(%r13)=&regSrc, mov 0x30(%r13)=regDst, eq
        if (!(it->assembler.regSrc == it->assembler.regDst))
            continue;
        // current.regSrc == prev.regDst
        // Binary at 0x27e70c-0x27e71a: mov -0x78(%r13)=prev.regDst, eq with current.regSrc
        if (!(it->assembler.regSrc == prev->assembler.regDst))
            continue;

        // Merge: mark ADDI as dead, set ADDIU's regSrc to register 0
        // Binary at 0x27e720: mov $0xffffffff, prev.cmd
        // Binary at 0x27e72e-0x27e739: AsmRegister(0) → current.regSrc (+0x28)
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
        if (!(optFlags_ & 0x08))
            continue;

        // getCmdType — 27e850
        int cmdType = Assembler::getCmdType(cmd);

        // If instruction does NOT write (bit 1 of cmdType clear),
        // skip — 27e859
        if (!(cmdType & 0x02))
            continue;

        // Determine the written register.
        // First try regDst (+0x28 in Assembler, Asm+0x30):
        //   if valid and != AsmRegister(0), use it.
        // Otherwise use regAux (+0x30, Asm+0x38).
        // 27e85d: lea rdi,[r12+0x30]; call isValid
        // 27e878: AsmRegister(0); call operator==
        // 27e895: lea rcx,[r12+0x38]  (fallback to regAux)
        // 27e85d: first try regDst; 27e895: fallback to regAux
        AsmRegister* destSlot = &it->assembler.regDst;
        AsmRegister destReg = *destSlot;
        if (!destReg.isValid() || destReg == AsmRegister(0)) {
            destSlot = &it->assembler.regAux;
            destReg = *destSlot;
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
            //   (Asm+0x28 = Assembler+0x20 = regSrc)
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
            //   (Asm+0x30 = Assembler+0x28 = regDst)
            {
                int prev = usageFlags;
                usageFlags |= 2;  // 27ea6d
                if (!(scanIt->assembler.regDst == destReg))
                    usageFlags = prev;  // 27ea73: cmove
            }

            // Check regAux (+0x30, ambiguous) — 27ea77: lea rdi,[rbx+0x38]
            //   (Asm+0x38 = Assembler+0x30 = regAux)
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
            *destSlot = AsmRegister(-1);
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
            if (optFlags_) {
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

            if (optFlags_) {
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
    // Assembler) and compares via Immediate::operator==.
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

} // namespace zhinst
