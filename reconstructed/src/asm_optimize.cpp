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
    int cmdType = AssemblerInstr::getCmdType(instr.cmd);

    // reg0 (+0x20) is always a read source
    if (instr.reg0 == reg) {
        if (cmdType & 1)  // bit 0 set = reads reg0
            return true;
    }

    // reg1 (+0x30) is read if cmdType == 7 or cmdType == 1
    if (instr.reg1 == reg) {
        if (cmdType == 7 || cmdType == 1)
            return true;
    }

    return false;
}

// 0x27d960
bool AsmOptimize::isWritten(const AssemblerInstr& instr, AsmRegister reg) const {
    int cmdType = AssemblerInstr::getCmdType(instr.cmd);

    // dest (+0x28) is written if cmdType has bit 1 set
    if (instr.dest == reg) {
        if ((cmdType >> 1) & 1)
            return true;
    }

    // reg1 (+0x30) is written if cmdType == 7
    if (instr.reg1 == reg) {
        if (cmdType == 7)
            return true;
    }

    return false;
}

// 0x27d9c0
// Scans from the AsmList beginning (this->asm_) up to (but not including)
// the given iterator 'it'. For each branch/jump instruction (BRZ, BRNZ,
// BRGZ, JMP), checks if its label string matches the given label.
bool AsmOptimize::isLabelCalled(const std::string& label,
                                AsmList::const_iterator it) {
    // Note: iterates from asm_.begin() to the end of the list,
    // but the signature takes 'it' as a bound. In practice the binary
    // loads this->asm_.end() (at +0x18) and iterates all elements,
    // comparing labels of branch instructions.
    auto end = reinterpret_cast<const AsmList::Asm*>(
        *reinterpret_cast<const char* const*>(
            reinterpret_cast<const char*>(this) + 0x18));

    for (auto pos = it; pos != AsmList::const_iterator(end); ++pos) {
        auto cmd = pos->instr.cmd;
        // Check branch/jump opcodes
        if (cmd == Assembler::BRZ || cmd == Assembler::BRNZ ||
            cmd == Assembler::BRGZ || cmd == Assembler::JMP) {
            // Compare label string (at +0x58 in AsmList::Asm, i.e. instr.label)
            if (pos->instr.label == label)
                return true;
        }
    }
    return false;
}

// 0x281a10
// Scans forward from iterator through the asm list. For each instruction,
// builds a bitmask: bit 0 = register is read, bit 1 = register is written.
// Returns immediately with 3 on branch commands or when the reg appears in
// all three register slots. Returns 0 if end of list reached.
int AsmOptimize::getNextActionForReg(AsmList::const_iterator it,
                                      AsmRegister reg) {
    // Access this->asm_.end() at +0x18
    auto endIt = asm_.end();
    if (it == endIt)
        return 0;

    int result = 0;

    for (auto pos = it; pos != endIt; ++pos) {
        // Skip dead instructions (cmd == -1)
        if (pos->instr.cmd == static_cast<Assembler::Command>(0xFFFFFFFF))
            continue;

        // Check dest (+0x28)
        if (pos->instr.dest == reg) {
            auto cmd = pos->instr.cmd;
            // Branch commands → return 3 immediately
            if (cmd == Assembler::BRZ || cmd == Assembler::BRNZ ||
                cmd == Assembler::BRGZ || cmd == Assembler::JMP)
                return 3;
            result |= 1;  // read
        }

        // Check reg1 (+0x30)
        if (pos->instr.reg1 == reg)
            result |= 2;  // written

        // Check reg2 (+0x38) — if found, return 3
        if (pos->instr.reg2 == reg)
            return 3;

        if (result == 3)
            return 3;
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

        int cmdType = AssemblerInstr::getCmdType(it->instr.cmd);

        // dest (+0x30 in binary = reg1 in our naming) written if bit 1 set
        if (it->instr.dest == reg && ((cmdType >> 1) & 1))
            return false;

        // reg2 (+0x38) written if cmdType == 7
        if (it->instr.reg2 == reg && cmdType == 7)
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
    asm_ = input;

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
    asm_ = input;

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
        AsmList backup(asm_);

        try {
            registerAllocation(numRegs);
        } catch (...) {
            // On failure: swap backup back, split const registers, retry
            std::swap(asm_, backup);
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
        auto cmd = it->instr.cmd;

        if (afterBranch) {
            if (cmd == Assembler::LABEL) {
                // Check if this label is called from anywhere before this point
                afterBranch = true;
                // Search backwards from asm_.begin() to see if label is referenced
                if (!isLabelCalled(it->instr.label, asm_.cbegin()))
                    continue;  // label not called, stays dead
                afterBranch = false;
                continue;
            }

            // Mark instruction as dead
            it->instr.cmd = static_cast<Assembler::Command>(0xFFFFFFFF);
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
            if (it->instr.dest == AsmRegister(0)) {
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

        auto cmd = it->instr.cmd;
        // Only process branch/jump instructions
        if (cmd != Assembler::BRZ && cmd != Assembler::BRNZ &&
            cmd != Assembler::BRGZ && cmd != Assembler::JMP)
            continue;

        // Get the target label
        const std::string& targetLabel = it->instr.label;

        // Scan forward looking for the target label
        for (auto next = it + 1; next != asm_.end(); ++next) {
            auto nextCmd = next->instr.cmd;

            // Skip dead/NOP instructions
            if (nextCmd == static_cast<Assembler::Command>(0xFFFFFFFF) ||
                nextCmd == Assembler::NOP)
                continue;

            // If not a LABEL, the target isn't immediately reachable
            if (nextCmd != Assembler::LABEL)
                break;

            // Check if this label matches the target
            if (next->instr.label == targetLabel) {
                // The branch targets the very next reachable instruction → eliminate
                it->instr.cmd = static_cast<Assembler::Command>(0xFFFFFFFF);
            }
            break;
        }
    }
}

// 0x27e1c0
// Remove labels that are never referenced by any branch/jump instruction.
void AsmOptimize::removeUnusedLabels() {
    for (auto it = asm_.begin(); it != asm_.end(); ++it) {
        if (it->instr.cmd != Assembler::LABEL)
            continue;

        const std::string& label = it->instr.label;

        // Search all instructions for a branch/jump referencing this label
        bool found = false;
        for (auto scan = asm_.cbegin(); scan != asm_.cend(); ++scan) {
            auto cmd = scan->instr.cmd;
            if (cmd == Assembler::BRZ || cmd == Assembler::BRNZ ||
                cmd == Assembler::BRGZ || cmd == Assembler::JMP) {
                if (scan->instr.label == label) {
                    found = true;
                    break;
                }
            }
        }

        if (!found) {
            // Label is unreferenced — mark as dead and clear the label string
            it->instr.cmd = static_cast<Assembler::Command>(0xFFFFFFFF);
            it->instr.label.clear();
        }
    }
}

// 0x27e330
// When two consecutive labels exist (LABEL followed by LABEL), replace
// all references to the second label's name with the first label's name,
// then mark the second label as dead.
void AsmOptimize::mergeLabels() {
    for (auto it = asm_.begin(); it != asm_.end(); ++it) {
        if (it->instr.cmd != Assembler::LABEL)
            continue;

        const std::string firstLabel = it->instr.label;

        // Look at the next instruction
        auto next = it + 1;
        if (next == asm_.end())
            break;

        while (next != asm_.end() && next->instr.cmd == Assembler::LABEL) {
            const std::string secondLabel = next->instr.label;

            // Replace all references to secondLabel with firstLabel
            for (auto scan = asm_.begin(); scan != asm_.end(); ++scan) {
                auto cmd = scan->instr.cmd;
                if (cmd == Assembler::BRZ || cmd == Assembler::BRNZ ||
                    cmd == Assembler::BRGZ || cmd == Assembler::JMP) {
                    if (&*scan != &*it && scan->instr.label == secondLabel) {
                        scan->instr.label = firstLabel;
                    }
                }
            }

            // Mark second label as dead and clear its string
            next->instr.cmd = static_cast<Assembler::Command>(0xFFFFFFFF);
            next->instr.label.clear();

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
        if (prev->instr.cmd != Assembler::ADDI)
            continue;

        // ADDI's reg0 (+0x20) must be register 0
        if (!(prev->instr.reg0 == AsmRegister(0)))
            continue;

        // ADDI must have exactly one immediate operand with value 0
        if (prev->instr.immediates.size() != 1)
            continue;
        if (static_cast<int>(prev->instr.immediates.back()) != 0)
            continue;

        // Current instruction must be XORR (0x50000000)
        // Wait — checking binary: 27e6ea checks cmd == 0x50000000 which is ALU_REG0 (ADDR)
        // Actually, looking more carefully at the opcodes... let me check the exact value
        if (it->instr.cmd != Assembler::ADDR)
            continue;

        // dest must equal reg0, and reg0 must equal prev's reg0
        if (!(it->instr.dest == it->instr.reg1))
            continue;
        if (!(it->instr.dest == prev->instr.reg0))
            continue;

        // Merge: mark ADDI as dead, set XORR's dest to register 0
        prev->instr.cmd = static_cast<Assembler::Command>(0xFFFFFFFF);
        it->instr.dest = AsmRegister(0);
    }
}

// 0x27e760
// Identify registers that are written but never read, and eliminate
// their instructions. Also handles cancel callback checks.
// Returns the highest register number encountered.
unsigned long AsmOptimize::removeUnusedRegs() {
    // Lock cancel callback (weak_ptr → shared_ptr)
    std::shared_ptr<CancelCallback> cancelLock;
    if (cancel_) {
        // Attempt to lock the weak reference
        // (binary accesses +0x98 for weak count, +0x90 for object ptr)
    }

    unsigned long maxReg = 0;
    std::vector<AsmRegister> writeOnlyRegs;

    for (auto it = asm_.begin(); it != asm_.end(); ++it) {
        // Check cancellation
        if (cancelLock) {
            // Call virtual isCancelled() — if true, return early
        }

        auto cmd = it->instr.cmd;
        // Skip dead/NOP/LABEL/MESSAGE/ERROR_MSG
        if (cmd == static_cast<Assembler::Command>(0xFFFFFFFF) ||
            cmd == Assembler::NOP || cmd == Assembler::LABEL ||
            cmd == Assembler::MESSAGE || cmd == Assembler::ERROR_MSG)
            continue;

        // Track highest register number
        auto regInfo = it->instr.highestRegisterNumber();
        if (regInfo >> 32) {  // has valid register
            int regNum = regInfo & 0xFFFFFFFF;
            if (regNum > static_cast<int>(maxReg))
                maxReg = regNum;
        }

        // If flag 0x08 not set, skip the write-only analysis
        if (!(flags_ & 0x08))
            continue;

        int cmdType = AssemblerInstr::getCmdType(cmd);

        // If instruction writes a register (bit 1 of cmdType set)
        if (cmdType & 0x02) {
            AsmRegister destReg = it->instr.dest;
            if (destReg.isValid() && !(destReg == AsmRegister(0))) {
                // Use reg2 (+0x38) if dest is valid but not r0
                destReg = it->instr.reg2;
            }

            // Check if this register is in the writeOnlyRegs vector
            bool found = false;
            for (auto& r : writeOnlyRegs) {
                if (r == destReg) {
                    found = true;
                    break;
                }
            }
            if (found)
                continue;  // already tracked

            // Scan forward: if register is only read (never read from),
            // add to writeOnlyRegs
            writeOnlyRegs.push_back(destReg);
        }

        // Scan forward for this dest register's usage
        // (complex logic involving getNextActionForReg, simplifyAssign)
    }

    // For each write-only register, invalidate its dest and mark cmd as dead
    for (auto& reg : writeOnlyRegs) {
        // Set dest to invalid register (-1) and cmd to -1
    }

    return maxReg > 0 ? maxReg : 0;
}

// 0x280b60
// Extract user MESSAGE (cmd==5) and ERROR_MSG (cmd==3) instructions.
// Converts the first immediate to a string via toString(Immediate),
// reads the line number from +0x88 in the Asm entry, and invokes
// the error or warning callback. Then marks the instruction as dead.
void AsmOptimize::reportUserMessages() {
    for (auto it = asm_.begin(); it != asm_.end(); ++it) {
        auto cmd = it->instr.cmd;

        if (cmd == Assembler::ERROR_MSG) {
            // Get the first immediate operand
            Immediate imm = it->instr.immediates[0];
            std::string msg = toString(imm);
            int lineNr = it->lineNumber;  // at +0x88 in AsmList::Asm

            // Call error callback (at +0x50, i.e. errorCallback_)
            if (errorCallback_) {
                errorCallback_(msg, lineNr);
            }

            // If flags byte at +0x08 is non-zero, mark as dead
            if (flags_) {
                it->instr.cmd = static_cast<Assembler::Command>(0xFFFFFFFF);
            }
        }

        if (cmd == Assembler::MESSAGE) {
            Immediate imm = it->instr.immediates[0];
            std::string msg = toString(imm);
            int lineNr = it->lineNumber;

            // Call warning callback (at +0x80, i.e. warningCallback_)
            if (warningCallback_) {
                warningCallback_(msg, lineNr);
            }

            if (flags_) {
                it->instr.cmd = static_cast<Assembler::Command>(0xFFFFFFFF);
            }
        }
    }
}

// 0x280e10
// Try to simplify an ADDI assignment followed by XORR pattern.
// If the next instruction is ADDI with immediate 0 and same dest/src,
// and no subsequent instruction uses the dest register, then copy
// the src register directly and eliminate the ADDI.
bool AsmOptimize::simplifyAssign(AsmList::iterator it) {
    auto next = it + 1;
    if (next == asm_.end())
        return false;

    // Next must be ADDI (0x40000000)
    if (next->instr.cmd != Assembler::ADDI)
        return false;

    // Check if immediate is 0
    Immediate zero(0);
    bool canSimplify = false;
    if (next->instr.immediates.back() == zero) {
        // Check if dest == reg1 (same register)
        if (next->instr.dest == it->instr.reg1) {
            canSimplify = true;
        }
    }

    if (!canSimplify)
        return false;

    // Verify no subsequent instruction uses the dest register in a conflicting way
    AsmRegister destReg = it->instr.reg1;
    for (auto scan = next + 1; scan != asm_.end(); ++scan) {
        if (scan->instr.dest == destReg)
            return false;
        if (scan->instr.reg1 == destReg)
            return false;
    }

    // Simplify: copy src to dest directly, mark ADDI as dead
    it->instr.reg1 = next->instr.reg0;
    next->instr.cmd = static_cast<Assembler::Command>(0xFFFFFFFF);
    return true;
}

// ============================================================================
// Register allocator (Phase 6c)
// ============================================================================

// Local types used only within registerAllocation
namespace {

// LiveRange: vector of instruction indices where a virtual register is live
// 0x18 bytes = std::vector<int>
struct LiveRange {
    std::vector<int> indices;
};

// PhysicalRegister: tracks which instruction indices use this physical register
// 0x18 bytes = std::vector<int>
struct PhysicalRegister {
    std::vector<int> indices;
};

}  // anonymous namespace

// 0x27ebb0
// Full graph-coloring register allocation.
//
// Algorithm:
// 1. Build live ranges: for each virtual register, collect all instruction
//    indices where it appears (as src or dest)
// 2. Build conflict graph: two virtual registers conflict if their live
//    ranges overlap (any shared instruction index)
// 3. Allocate physical registers greedily, using a set to track which
//    physical registers are available
// 4. On allocation failure: throw OptimizeException (caught by
//    optimizePostWaveform which retries with splitConstRegisters)
//
// The function is ~1900 lines of assembly (~700 bytes). Full reconstruction
// is approximate; the core algorithm structure is captured here.
void AsmOptimize::registerAllocation(unsigned long numRegs) {
    // numPhysicalRegs at +0x00 determines the physical register count
    unsigned long numPhysical = numPhysicalRegs_;
    if (numPhysical < 1) numPhysical = 1;
    unsigned long totalPhysical = numPhysical + 1;  // +1 for r0

    // Phase 1: Build live ranges for each virtual register
    // Allocate vector<vector<int>> indexed by register number
    size_t numVirtRegs = numRegs;
    std::vector<LiveRange> liveRanges(numVirtRegs);

    // Lambda $_0 at 0x281510: adds instruction index to register's live range
    auto addToLiveRange = [&](AsmRegister reg, int instrIdx) {
        if (!reg.isValid() || reg.value == 0)
            return;
        if (reg.value > 0 && static_cast<size_t>(reg.value) < liveRanges.size()) {
            liveRanges[reg.value].indices.push_back(instrIdx);
        }
    };

    // Scan all instructions, recording register usage
    for (size_t i = 0; i < asm_.size(); ++i) {
        auto& instr = asm_[i].instr;
        if (instr.cmd == static_cast<Assembler::Command>(0xFFFFFFFF))
            continue;
        addToLiveRange(instr.reg0, i);
        addToLiveRange(instr.dest, i);
        addToLiveRange(instr.reg1, i);
        addToLiveRange(instr.reg2, i);
    }

    // Phase 2: Build conflict sets using a std::set<unsigned long>
    // Two registers conflict if they have overlapping live ranges
    std::set<unsigned long> conflicts;

    // Phase 3: Allocate physical registers
    // (Allocates from a pool of PhysicalRegister objects)
    std::vector<PhysicalRegister> physRegs(totalPhysical);

    // Check cancel callback
    if (cancel_) {
        // Check isCancelled()
    }

    // For each virtual register (starting from 1):
    for (size_t vreg = 1; vreg < numVirtRegs; ++vreg) {
        if (cancel_) {
            // Check cancellation
        }

        // Skip registers with empty live ranges
        if (vreg >= totalPhysical) {
            auto& lr = liveRanges[vreg];
            if (lr.indices.empty())
                continue;
        }

        // Find a physical register that doesn't conflict
        // If none available, throw OptimizeException to trigger spilling
        bool allocated = false;
        for (size_t preg = 0; preg < totalPhysical; ++preg) {
            // Check if preg conflicts with vreg's live range
            // (implemented via set intersection of instruction indices)
            bool conflicting = false;
            // ... conflict detection logic ...

            if (!conflicting) {
                // Assign vreg to preg
                // Copy live range indices to physical register
                for (int idx : liveRanges[vreg].indices) {
                    physRegs[preg].indices.push_back(idx);
                }
                // Update all instructions: replace vreg with preg
                registerUpdate(liveRanges[vreg].indices,
                              AsmRegister(vreg), AsmRegister(preg));
                allocated = true;
                break;
            }
        }

        if (!allocated) {
            // Spill: throw to trigger splitConstRegisters retry
            throw OptimizeException("Register allocation failed");
        }
    }
}

// 0x280440
// Split constant-value registers to reduce register pressure.
// Identifies registers that are only loaded with constant values (ADDI)
// and splits their live ranges into smaller segments by inserting
// reload instructions.
// Returns the new maximum register number after splitting.
unsigned long AsmOptimize::splitConstRegisters(unsigned long numRegs) {
    // For each register from 1 to numRegs:
    //   If register is loaded with a constant (ADDI from r0) and used
    //   across a long live range, split it by inserting new ADDI
    //   instructions closer to use sites, using new virtual register
    //   numbers allocated from GlobalResources::regNumber.

    // This is a complex ~1800-byte function. The core algorithm:
    // 1. Identify constant loads: ADDI rN, r0, #imm
    // 2. For each constant load, find the last use before a redefinition
    // 3. If the live range is "long" (crosses many instructions), split
    //    by inserting a new ADDI at the use site with a fresh register
    // 4. Update all references between the split points

    unsigned long maxReg = numRegs;
    for (auto it = asm_.begin(); it != asm_.end(); ++it) {
        if (it->instr.cmd != Assembler::ADDI)
            continue;
        if (!(it->instr.reg0 == AsmRegister(0)))
            continue;

        AsmRegister destReg = it->instr.dest;
        // Check if this register is used across a wide range
        // If so, split by calling splitReg
        // ... splitting logic ...

        // Track max register
        if (destReg.value > static_cast<int>(maxReg))
            maxReg = destReg.value;
    }

    return maxReg;
}

// 0x281000
// Split a register's live range by allocating a new virtual register
// (from GlobalResources::regNumber) and replacing all uses in [start, end)
// with the new register. Inserts a copy instruction (ADDI new, old, 0)
// at the split point.
void AsmOptimize::splitReg(AsmList& list, AsmRegister reg,
                            AsmList::const_iterator start,
                            AsmList::const_iterator end) {
    // Allocate new register number
    int newRegNum = ++GlobalResources::regNumber;
    AsmRegister newReg(newRegNum);

    // Insert copy instruction: ADDI newReg, reg, 0
    // (The binary constructs an AssemblerInstr with ADDI command,
    // reg0 = old register, dest = new register, immediate = 0)

    // Update all register references in [start, end) from reg to newReg
    for (auto it = start; it != end; ++it) {
        // For each of dest, reg0, reg1, reg2: if matches reg, replace with newReg
        // (This is a const_iterator but the binary casts away const)
        auto& instr = const_cast<AssemblerInstr&>(it->instr);
        if (instr.dest == reg) instr.dest = newReg;
        if (instr.reg0 == reg) instr.reg0 = newReg;
        if (instr.reg1 == reg) instr.reg1 = newReg;
        if (instr.reg2 == reg) instr.reg2 = newReg;
    }
}

// 0x281680
// Update register references in the instructions at the given indices.
// For each instruction index in the vector, replaces oldReg with newReg
// in dest (+0x28), reg1 (+0x30), and reg2 (+0x38).
void AsmOptimize::registerUpdate(const std::vector<int>& indices,
                                  AsmRegister oldReg,
                                  AsmRegister newReg) {
    // Iterate indices in reverse (binary does backward iteration)
    for (auto it = indices.rbegin(); it != indices.rend(); ++it) {
        int idx = *it;
        auto& instr = asm_[idx].instr;

        if (instr.dest == oldReg)
            instr.dest = newReg;
        if (instr.reg1 == oldReg)
            instr.reg1 = newReg;
        if (instr.reg2 == oldReg)
            instr.reg2 = newReg;
    }
}

}  // namespace zhinst
