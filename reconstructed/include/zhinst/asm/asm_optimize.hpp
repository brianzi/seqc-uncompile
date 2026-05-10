// ============================================================================
// Reconstructed from disassembly of _seqc_compiler.so
// AsmOptimize — optimizer for AsmList instruction sequences
//
// Optimization flags (bitmask at +0x08):
//   0x01 = oneStepJumpElimination
//   0x02 = removeUnusedLabels + mergeLabels
//   0x04 = deadCodeElimination
//   0x08 = mergeRegisterZeroing
//   0x10 = removeUnusedRegs + registerAllocation
// ============================================================================
#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "zhinst/asm/asm_list.hpp"
#include "zhinst/asm/asm_register.hpp"
#include "zhinst/asm/assembler.hpp"

namespace zhinst {

// Optimization pass flag bits for AsmOptimize::optFlags_ (A5)
enum OptPassFlag : uint32_t {
    Opt_JumpElim     = 0x01,   // oneStepJumpElimination
    Opt_LabelCleanup = 0x02,   // removeUnusedLabels + mergeLabels
    Opt_DeadCode     = 0x04,   // deadCodeElimination
    Opt_MergeZero    = 0x08,   // mergeRegisterZeroing
    Opt_RegAlloc     = 0x10,   // removeUnusedRegs + registerAllocation
};

// RegAction — return values from getNextActionForReg (A12)
enum RegAction : int {
    RegAction_None    = 0,
    RegAction_Read        = 1,   // bit 0
    RegAction_Written     = 2,   // bit 1
    RegAction_Both = 3,   // both bits / branch
};

// Forward declaration
class CancelCallback;

// Exception thrown by optimization passes
//   +0x00  vtable
//   +0x08  std::string message_   (libc++ 24-byte SSO)
//   +0x20  int         lineNumber_ (source line for error framing)
//   +0x28  total size
//! \brief Exception raised when register allocation cannot be satisfied.
//!
//! Thrown from `AsmOptimize::registerAllocation` (and its inner
//! coalescing loop) when no further physical register can be assigned
//! to a virtual register even after constant-register splitting and
//! live-range spilling — the message is always a fixed
//! "run out of free registers, please reduce complexity" string with
//! the source line of the offending instruction in `lineNumber_`.
//! Caught by `Compiler::compile` around `optimizePreWaveform` and
//! `optimizePostWaveform`, which re-emits it through
//! `messages_.errorMessage(...)` so the standard
//! "Compiler Error (line: N): " prefix is applied before re-throwing.
class OptimizeException : public std::exception {
public:
    OptimizeException(const std::string& msg);
    OptimizeException(const std::string& msg, int lineNumber);
    ~OptimizeException() override;                  // 0x281e00
    const char* what() const noexcept override;     // 0x281e90
    // Returns message_.c_str() if non-empty, else "Optimize Exception"

    int lineNumber() const noexcept { return lineNumber_; }

private:
    std::string message_;   // +0x08
    int lineNumber_{0};     // +0x20
};

// GlobalResources::regNumber is a thread-local static member of the
// GlobalResources class (declared in resources.hpp, defined in
// global_resources.cpp). Forward-declare the class here to avoid
// pulling in the full resources.hpp header.
class GlobalResources;

// Offset  Size  Type                                    Name
// +0x00   4     uint32_t                                numPhysicalRegs_
// +0x04   4     (padding)
// +0x08   4     uint32_t                                optFlags_
// +0x0C   4     (padding)
// +0x10   24    std::vector<AsmList::Asm>               asm_
// +0x28   8     (padding to 0x30)
// +0x30   48    std::function<void(string,int)>         errorCallback_
// +0x60   48    std::function<void(string,int)>         warningCallback_
// +0x90   16    std::shared_ptr<CancelCallback>         cancel_
// sizeof(AsmOptimize) = 0xA0
//! \brief Driver for the post-codegen optimisation passes over an `AsmList`.
//!
//! Owns a working copy of the instruction stream and runs a fixed set
//! of peephole / liveness / register-allocation passes selected by the
//! `optFlags_` bitmask (see `OptPassFlag`).  Two public entry points
//! split the work around waveform planning:
//!
//! - `optimizePreWaveform` — invoked before waveforms are emitted; runs
//!   only `deadCodeElimination` (when `Opt_DeadCode` is set) so that
//!   unreachable instructions don't influence waveform lowering.
//! - `optimizePostWaveform` — invoked after waveform planning; runs
//!   the remaining passes (`oneStepJumpElimination`, label cleanup,
//!   register-zeroing fusion, register allocation) plus user
//!   message/error reporting via `reportUserMessages`.
//!
//! `prepareResources` must be called once before either entry point;
//! it bumps `GlobalResources::regNumber` past any virtual register
//! number already used in the input so the allocator can mint fresh
//! virtuals during spilling.  Diagnostics from `reportUserMessages`
//! and from a failed `registerAllocation` (which throws
//! `OptimizeException`) are routed through the `errorCallback_` /
//! `warningCallback_` `std::function`s supplied at construction.
//! `cancel_` is checked by the long-running register-allocation loop
//! to support user-initiated cancellation.
class AsmOptimize {
public:
    // Constructor — builds optimizer with callbacks and device info
    // Constructed inline in Compiler::compile() at 0x120707
    AsmOptimize(std::function<void(const std::string&, int)> errorCallback,
                std::function<void(const std::string&, int)> warningCallback,
                uint32_t numPhysicalRegs,
                uint32_t flags,
                std::shared_ptr<CancelCallback> cancel);

    // Destructor — destroys callbacks, asm vector, cancel ptr
    ~AsmOptimize();                                                     // 0x123200

    // ---- Public optimization entry points ----

    // Prepare global register numbering to exceed maxRegister in the list
    void prepareResources(const AsmList& asmList) const;                // 0x27dab0

    // Pre-waveform optimization pass (copies input, runs deadCodeElimination if flag 0x04)
    // Returns optimized AsmList by sret
    /*! \brief Run the optimisation passes that must execute before
     *  waveform planning, returning the rewritten instruction stream.
     *
     *  \details Copies `input.entries` into the internal working
     *  vector `asm_`, then conditionally runs
     *  `deadCodeElimination()` when `Opt_DeadCode` is set in
     *  `optFlags_`.  No other pass is executed at this stage.  The
     *  rationale is that unreachable instructions (e.g. dead branches
     *  emitted by the lowering frontend) must be pruned before the
     *  prefetcher inspects waveform usage, so dead `play*` calls
     *  don't keep waveforms alive in `WavetableIR`.  All other
     *  passes are deferred to `optimizePostWaveform` because they
     *  depend on register allocation and label cleanup that must
     *  happen after waveform-load placeholders have been expanded
     *  by `Prefetch::fillInPlaceholders`.
     *
     *  \param input  The pre-waveform instruction list as produced
     *                by the lowering frontend.  Not modified.
     *  \return       A copy of `asm_` after the (optional) dead-code
     *                pass has run.
     */
    AsmList optimizePreWaveform(const AsmList& input);                  // 0x27db40

    // Post-waveform optimization pass (all other passes based on flags)
    // Returns optimized AsmList by sret
    /*! \brief Run the optimisation passes that must execute after
     *  waveform planning, returning the final instruction stream
     *  ready for assembly.
     *
     *  \details Copies `input.entries` into `asm_`, then runs each
     *  pass guarded by its `optFlags_` bit:
     *    1. `Opt_JumpElim`     — `oneStepJumpElimination`.
     *    2. `Opt_LabelCleanup` — `removeUnusedLabels` followed by
     *       `mergeLabels`.
     *    3. `Opt_MergeZero`    — `mergeRegisterZeroing`.
     *    4. `Opt_RegAlloc`     — `removeUnusedRegs` to compute the
     *       virtual-register count, then `registerAllocation` on a
     *       backup of `asm_`.  When that throws (the spill heuristic
     *       failed to fit the working set into `numPhysicalRegs_`),
     *       the backup is restored, `splitConstRegisters` is run to
     *       break long-lived constant-bearing live ranges into
     *       shorter ones, and `registerAllocation` is retried with
     *       the new virtual-register count.  A second failure is
     *       not caught and propagates as `OptimizeException`.
     *
     *  Independently of the flags, `reportUserMessages` is always
     *  invoked at the end to drain any user-emitted `MESSAGE`
     *  (warning) and `ERROR_MSG` (error) instructions through the
     *  `warningCallback_` / `errorCallback_` `std::function`s and
     *  mark them dead so they do not reach the assembler.
     *
     *  \param input  The post-waveform instruction list as produced
     *                by `Prefetch::fillInPlaceholders`.  Not modified.
     *  \return       A copy of `asm_` after every enabled pass has
     *                run and user diagnostics have been reported.
     *  \throws OptimizeException  When `registerAllocation` fails
     *                a second time after `splitConstRegisters`, i.e.
     *                the program cannot be expressed in
     *                `numPhysicalRegs_` physical registers even
     *                after constant-register splitting.
     */
    AsmList optimizePostWaveform(const AsmList& input);                 // 0x27ddf0

private:
    // ---- Query helpers ----

    // Check if instruction reads the given register
    // reg2 (+0x20) is read if cmdType bit 0 set; reg1 (+0x30) is read if cmdType==7 or cmdType==1
    bool isRead(const Assembler& instr, AsmRegister reg) const;    // 0x27d900

    // Check if instruction writes the given register
    // reg0 (+0x28) is written if cmdType has bit 1 set; reg1 (+0x30) is written if cmdType==7
    bool isWritten(const Assembler& instr, AsmRegister reg) const; // 0x27d960

    // Check if any branch/jump instruction after 'it' references the given label
    // Checks commands: BRZ (0xf3), BRNZ (0xf4), BRGZ (0xf5), JMP (0xfe)
    bool isLabelCalled(const std::string& label,
                       AsmList::const_iterator it);                     // 0x27d9c0

    // Scan forward from iterator, return bitmask: bit0=read, bit1=written
    // Returns 3 (both) on branch commands or reg found in dest (+0x38)
    int getNextActionForReg(AsmList::const_iterator it,
                            AsmRegister reg);                           // 0x281a10

    // Check if no instruction in [start+1, end) writes the register,
    // skipping the 'exclude' iterator
    bool registerIsNeverWritten(AsmList& list, AsmRegister reg,
                                AsmList::const_iterator start,
                                AsmList::const_iterator exclude) const; // 0x280f50

    // ---- Optimization passes ----

    // Remove dead code after unconditional branches (sets cmd = -1)
    // Also removes Node references from dead instructions
    void deadCodeElimination();                                         // 0x27dbd0

    // Remove jumps that target the immediately following instruction
    void oneStepJumpElimination();                                      // 0x27e040

    // Remove labels that are never referenced by any branch/jump
    void removeUnusedLabels();                                          // 0x27e1c0

    // When consecutive labels exist, replace all references to the second
    // label with the first label, then delete the second
    void mergeLabels();                                                 // 0x27e330

    // Merge consecutive ADDI r,0 + XORR r,r,r patterns into single XORR
    // (when src1 == r0 and imm == 0, replace ADDI with NOP and set dest = r0)
    void mergeRegisterZeroing();                                        // 0x27e640

    // Identify registers used only as write destinations (never read),
    // eliminate their instructions, returns max register number seen
    unsigned long removeUnusedRegs();                                   // 0x27e760

    // Extract user MESSAGE (cmd==3) and ERROR_MSG (cmd==5) instructions,
    // invoke error/warning callbacks, then mark as dead
    void reportUserMessages();                                          // 0x280b60

    // Try to simplify an ADDI+XORR assign sequence at the given position
    bool simplifyAssign(AsmList::iterator it);                          // 0x280e10

    // ---- Register allocator ----

    // Full register allocation: build live ranges, allocate physical registers,
    // spill via splitReg on failure, iterative retry with splitConstRegisters
    void registerAllocation(unsigned long numRegs);                     // 0x27ebb0

    // Split constant-value registers to reduce live range conflicts
    unsigned long splitConstRegisters(unsigned long numRegs);           // 0x280440

    // Split a register's live range at the given boundaries
    void splitReg(AsmList& list, AsmRegister reg,
                  AsmList::const_iterator start,
                  AsmList::const_iterator end);                         // 0x281000

    // Update all register references in the given instruction indices
    // from oldReg to newReg
    void registerUpdate(const std::vector<int>& indices,
                        AsmRegister oldReg,
                        AsmRegister newReg);                            // 0x281680

    // ---- Data members ----

    uint32_t numPhysicalRegs_;                              // +0x00
    uint32_t pad04_;                                        // +0x04
    uint32_t optFlags_;                                        // +0x08
    uint32_t pad0C_;                                        // +0x0C
    std::vector<AsmList::Asm> asm_;                         // +0x10 (working copy)
    // padding to +0x30
    std::function<void(const std::string&, int)> errorCallback_;    // +0x30
    std::function<void(const std::string&, int)> warningCallback_;  // +0x60
    std::shared_ptr<CancelCallback> cancel_;                        // +0x90
};

}  // namespace zhinst
