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
//! \brief Bitmask flags selecting which optimisation passes
//!        `AsmOptimize` runs during `optimizePreWaveform` and
//!        `optimizePostWaveform`.
//!
//! Stored OR'd together in `AsmOptimize::optFlags_`. Each bit gates
//! one pass independently; clearing every bit disables all
//! optimisation while still draining user diagnostics through
//! `reportUserMessages`.
enum OptPassFlag : uint32_t {
    //! Enable `oneStepJumpElimination`: drop branches whose target is the immediate next reachable instruction.
    Opt_JumpElim     = 0x01,   // oneStepJumpElimination
    //! Enable `removeUnusedLabels` followed by `mergeLabels`: prune labels with no referencing branch and coalesce adjacent labels.
    Opt_LabelCleanup = 0x02,   // removeUnusedLabels + mergeLabels
    //! Enable `deadCodeElimination`: mark instructions unreachable after an unconditional transfer dead.
    Opt_DeadCode     = 0x04,   // deadCodeElimination
    //! Enable `mergeRegisterZeroing` / write-only register elimination via `simplifyAssign`.
    Opt_MergeZero    = 0x08,   // mergeRegisterZeroing
    //! Enable `removeUnusedRegs` + `registerAllocation` (with `splitConstRegisters` retry on spill).
    Opt_RegAlloc     = 0x10,   // removeUnusedRegs + registerAllocation
};

//! \brief Bitmask of `Assembler::Command` values that the
//!        register-allocation and dead-code passes treat as
//!        instruction-flow barriers and skip during forward scans.
//!
//! Indexed by `(static_cast<uint32_t>(cmd) + 1)` after the guard
//! `cmdPlus1 <= 5`.  The `+1` shift maps `Assembler::INVALID`
//! (`0xFFFFFFFF`) onto bit 0 so a single bitmap covers the
//! barrier sentinel alongside the low-numbered pseudo-ops.  Bits
//! set correspond to:
//! - bit 0 → `INVALID` — sentinel for unrecognised or unfilled cmd
//!                       slots; treated as dead.
//! - bit 3 → `LABEL`   — pseudo-instruction for label-only lines.
//! - bit 5 → `COMMENT_NOP` — synthetic comment-carrying barrier
//!                           (see `Assembler::Command::COMMENT_NOP`).
//!
//! `0x29 = 0b101001` selects bits {0, 3, 5}; no other enumerator
//! in the `cmd+1 <= 5` window falls in that set.
constexpr uint32_t kRegallocBarrierCmdMask = 0x29u;

// RegAction — return values from getNextActionForReg (A12)
//! \brief Bitmask classifying the next observed use of a register
//!        in a forward scan of `AsmList`.
//!
//! Returned by `AsmOptimize::getNextActionForReg`. The two low bits
//! are independent: bit 0 records a read, bit 1 records a write.
//! The combined value `RegAction_Both` is also produced eagerly when
//! the scan hits a branch (read of the register in a branch context)
//! or any `regAux` mention, signalling that the caller cannot reason
//! further about the register's liveness past that point.
enum RegAction : int {
    //! No use found before `asm_.end()`.
    RegAction_None    = 0,
    //! Bit 0: next use is a non-branch read of the register.
    RegAction_Read        = 1,   // bit 0
    //! Bit 1: next use is a write to the register.
    RegAction_Written     = 2,   // bit 1
    //! Both bits: read+write seen, or ambiguous (branch read / `regAux` use).
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
    //! \brief Construct with the given diagnostic message and a
    //!        zero line number.
    //! \param msg  Pre-formatted diagnostic, owned-by-copy.
    OptimizeException(const std::string& msg);
    //! \brief Construct with the given diagnostic message and
    //!        source line for error framing.
    //! \param msg         Pre-formatted diagnostic, owned-by-copy.
    //! \param lineNumber  Source line associated with the failing
    //!                    instruction; later retrievable via
    //!                    `lineNumber()`.
    OptimizeException(const std::string& msg, int lineNumber);
    //! \brief Release the embedded `message_` storage and chain to
    //!        `~std::exception`.
    ~OptimizeException() override;                  // 0x281e00
    //! \brief Return the stored message, or the canonical literal
    //!        `"Optimize Exception"` when the message is empty.
    //! \return Null-terminated diagnostic string owned by `*this`.
    const char* what() const noexcept override;     // 0x281e90
    // Returns message_.c_str() if non-empty, else "Optimize Exception"

    //! \brief Return the source line captured at construction.
    //! \return The `lineNumber` argument the exception was
    //! constructed with, or `0` for the single-argument overload.
    int lineNumber() const noexcept { return lineNumber_; }

private:
    //! \brief Diagnostic text returned by `what()`.
    std::string message_;   // +0x08
    //! \brief Source line associated with the failing instruction.
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
//! `cancel_` is observed once per outer iteration in
//! `removeUnusedRegs` and once per virtual-register iteration in
//! `registerAllocation`, so a long-running optimisation can be
//! interrupted cooperatively by the host.
class AsmOptimize {
public:
    // Constructor — builds optimizer with callbacks and device info
    // Constructed inline in Compiler::compile() at 0x120707
    //! \brief Construct the optimiser with the diagnostic callbacks,
    //!        physical-register budget, pass-selection bitmask and
    //!        cancellation handle that govern subsequent runs.
    //!
    //! \details Pure member-init: callbacks are moved into
    //! `errorCallback_` / `warningCallback_`, `numPhysicalRegs`
    //! and `flags` are stored in `numPhysicalRegs_` / `optFlags_`,
    //! the working `asm_` vector is default-constructed empty, and
    //! `cancel` is moved into `cancel_`.  The two padding slots
    //! (`pad04_`, `pad0C_`) are zeroed for layout fidelity with
    //! the binary.
    //!
    //! \param errorCallback     Invoked from `reportUserMessages`
    //!                          when an `ERROR_MSG` instruction is
    //!                          encountered, and from
    //!                          `Compiler::compile` when an
    //!                          `OptimizeException` propagates out
    //!                          of `optimizePostWaveform`.
    //! \param warningCallback   Invoked from `reportUserMessages`
    //!                          when a `MESSAGE` instruction is
    //!                          encountered.
    //! \param numPhysicalRegs   Upper bound for `registerAllocation`;
    //!                          the optimiser throws when the
    //!                          working set cannot be coloured with
    //!                          this many physical registers even
    //!                          after constant-register splitting.
    //! \param flags             `OptPassFlag` bitmask selecting the
    //!                          enabled passes.
    //! \param cancel            Cancellation handle polled by
    //!                          `removeUnusedRegs` and
    //!                          `registerAllocation`.  May be null;
    //!                          the corresponding poll sites become
    //!                          no-ops.
    AsmOptimize(std::function<void(const std::string&, int)> errorCallback,
                std::function<void(const std::string&, int)> warningCallback,
                uint32_t numPhysicalRegs,
                uint32_t flags,
                std::shared_ptr<CancelCallback> cancel);

    // Destructor — destroys callbacks, asm vector, cancel ptr
    //! \brief Release the cancellation handle, callbacks, and the
    //!        working `asm_` vector (each entry releases its
    //!        attached `Node` shared_ptr).
    //!
    //! \details Trivially defaulted body; member destructors fire
    //! in reverse declaration order.  No additional bookkeeping.
    ~AsmOptimize();                                                     // 0x123200

    // ---- Public optimization entry points ----

    // Prepare global register numbering to exceed maxRegister in the list
    //! \brief Bump `GlobalResources::regNumber` past every virtual
    //!        register number already used in `asmList`, so that
    //!        later passes (notably `splitReg`) can allocate fresh
    //!        virtual registers without collision.
    //!
    //! \details Tight loop: while
    //! `GlobalResources::regNumber <= asmList.maxRegister()`,
    //! increment.  `maxRegister()` is re-evaluated each iteration,
    //! so the post-condition holds against the snapshot of
    //! `asmList` observed at loop exit.  Operates on the
    //! thread-local `regNumber` slot only; `asm_` is untouched.
    //!
    //! \param asmList  The instruction list whose virtual-register
    //!                 numbering must be respected.  Not modified.
    //! \note  Must be called before `optimizePreWaveform` /
    //!        `optimizePostWaveform` for register allocation to
    //!        see a fresh-virtual-register cursor that is past
    //!        every existing assignment.
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
    //! \brief Test whether `instr` reads the given register in
    //!        either of its read-capable slots.
    //!
    //! \details Looks up `cmdType =
    //! Assembler::getCmdType(instr.cmd)`.  Returns true when
    //! `instr.regSrc == reg && (cmdType & 1)`, or when
    //! `instr.regAux == reg && (cmdType == 7 || cmdType == 1)`.
    //! `regDst` is intentionally not consulted — that slot is
    //! exclusively a write.  The `regAux` slot only counts as a
    //! read for the two ambiguous cmdTypes (1 and 7).
    //!
    //! \param instr  Instruction to inspect.
    //! \param reg    Register identifier to look for.
    //! \return  `true` if `reg` is read by `instr`, `false`
    //!          otherwise.
    bool isRead(const Assembler& instr, AsmRegister reg) const;    // 0x27d900

    // Check if instruction writes the given register
    // reg0 (+0x28) is written if cmdType has bit 1 set; reg1 (+0x30) is written if cmdType==7
    //! \brief Test whether `instr` writes the given register.
    //!
    //! \details Looks up `cmdType` and returns true when
    //! `instr.regDst == reg && ((cmdType >> 1) & 1)`, or when
    //! `instr.regAux == reg && cmdType == 7`.  Symmetric
    //! counterpart to `isRead`; together they encode the
    //! cmdType→slot semantics used throughout the peephole
    //! passes.
    //!
    //! \param instr  Instruction to inspect.
    //! \param reg    Register identifier to look for.
    //! \return  `true` if `reg` is written by `instr`, `false`
    //!          otherwise.
    bool isWritten(const Assembler& instr, AsmRegister reg) const; // 0x27d960

    // Check if any branch/jump instruction in [begin, it) references the given label
    // Checks commands: BRZ (0xf3), BRNZ (0xf4), BRGZ (0xf5), JMP (0xfe)
    //! \brief Determine whether any branch / jump instruction in
    //!        `[asm_.cbegin(), it)` references the given label
    //!        name.
    //!
    //! \details Forward scan from `asm_.cbegin()` up to (but not
    //! including) `it`.  Compares the label string of each
    //! `BRZ` (`0xf3`), `BRNZ` (`0xf4`), `BRGZ` (`0xf5`), and
    //! `JMP` (`0xfe`) entry against `label` and short-circuits
    //! on the first match.  Other opcodes do not own a target
    //! label string and are ignored.
    //!
    //! \param label  Label name to search for.
    //! \param it     Upper-bound iterator (exclusive).
    //! \return  `true` when a referencing branch is found,
    //!          `false` when the bounded scan completes without
    //!          a match.
    bool isLabelCalled(const std::string& label,
                       AsmList::const_iterator it);                     // 0x27d9c0

    // Scan forward from iterator, return bitmask: bit0=read, bit1=written
    // Returns 3 (both) on branch commands or reg found in dest (+0x38)
    //! \brief Look ahead from a position and report whether the
    //!        next interesting use of `reg` is a read, a write,
    //!        both, or a hard ambiguity.
    //!
    //! \details Forward scan from `it` to `asm_.end()`, skipping
    //! `INVALID` entries.  Returns `RegAction_Both` (`3`) early
    //! when the scan hits a branch whose `regSrc == reg`, or
    //! when any `regAux == reg` is found — both situations
    //! prevent the caller from reasoning about subsequent uses.
    //! Otherwise accumulates a bitmask: bit 0 set on a non-branch
    //! `regSrc == reg`, bit 1 set on `regDst == reg`.  Returns
    //! `RegAction_None` (`0`) when the end is reached without any
    //! match.
    //!
    //! \param it   Iterator at which to start the look-ahead.
    //! \param reg  Register identifier to look for.
    //! \return  A `RegAction` bitmask: `0` none, `1` read, `2`
    //!          written, `3` both / ambiguous.
    int getNextActionForReg(AsmList::const_iterator it,
                            AsmRegister reg);                           // 0x281a10

    // Check if no instruction in [start+1, end) writes the register,
    // skipping the 'exclude' iterator
    //! \brief Verify that `reg` is not written by any instruction
    //!        in the half-open forward range `(start, list.end())`,
    //!        ignoring the designated `exclude` entry.
    //!
    //! \details Returns `true` immediately when `start ==
    //! list.end()`.  Otherwise iterates `[start + 1,
    //! list.end())`.  For each entry other than `exclude`,
    //! returns `false` when `regDst == reg && ((cmdType >> 1)
    //! & 1)`, or when `regAux == reg && cmdType == 7`.  Returns
    //! `true` when the scan completes without finding a write.
    //!
    //! \param list     Instruction list to scan; taken non-const
    //!                 only for iterator type compatibility.
    //! \param reg      Register identifier to check.
    //! \param start    Lower-bound iterator; the scan begins at
    //!                 `start + 1` so `start` itself is never
    //!                 inspected.
    //! \param exclude  Iterator to skip during the scan; the
    //!                 typical use is "skip the partner
    //!                 instruction in the merge candidate".
    //! \return  `true` when no write is found, `false` otherwise.
    bool registerIsNeverWritten(AsmList& list, AsmRegister reg,
                                AsmList::const_iterator start,
                                AsmList::const_iterator exclude) const; // 0x280f50

    // ---- Optimization passes ----

    // Remove dead code after unconditional branches (sets cmd = -1)
    // Also removes Node references from dead instructions
    //! \brief Mark instructions unreachable after an unconditional
    //!        control-flow transfer as dead until a referenced
    //!        label re-establishes a live path.
    //!
    //! \details Walks `asm_` carrying an `afterBranch` flag.
    //! Outside dead regions, `JMP` flips the flag on; `BRZ` with
    //! `regSrc == R0` (a degenerate unconditional) also flips it
    //! on; any other opcode clears it.  Inside a dead region,
    //! every entry has its `cmd` set to `INVALID` and any attached
    //! `Node` is released via `Node::remove`.  When a `LABEL` is
    //! encountered in a dead region, `isLabelCalled(label, it)`
    //! decides whether the label re-enters live code: an
    //! unreferenced label stays dead, a referenced one ends the
    //! dead region.
    //!
    //! \note  No cancel check.  Uses `INVALID` (`-1`) as the
    //!        universal dead marker shared with the other passes.
    void deadCodeElimination();                                         // 0x27dbd0

    // Remove jumps that target the immediately following instruction
    //! \brief Eliminate a branch / jump whose target label is the
    //!        very next reachable instruction.
    //!
    //! \details For every branch / jump entry (`BRZ`, `BRNZ`,
    //! `BRGZ`, `JMP`) whose `noOpt` flag is clear, forward-scans
    //! the immediate successors skipping `INVALID` and `NOP`.  If
    //! a non-`LABEL` instruction is hit first, the branch stays.
    //! Otherwise compares each `LABEL`'s name to the branch's
    //! target: a match sets the branch's `cmd = INVALID`; a
    //! mismatch continues past the label so a chain of intervening
    //! labels is tolerated.
    //!
    //! \note  Only the four branch opcodes are handled.  The
    //!        target `LABEL` is left in place — `removeUnusedLabels`
    //!        / `mergeLabels` clean it up later.
    void oneStepJumpElimination();                                      // 0x27e040

    // Remove labels that are never referenced by any branch/jump
    //! \brief Mark `LABEL` entries that no branch references as
    //!        dead.
    //!
    //! \details For each `LABEL` in `asm_`, scans the **entire**
    //! `asm_` for any `BRZ` / `BRNZ` / `BRGZ` / `JMP` whose label
    //! string equals the candidate's name.  When none is found,
    //! sets `cmd = INVALID` and clears the label string.  Does
    //! not call `isLabelCalled` because that helper only inspects
    //! `[begin, it)` — `removeUnusedLabels` must also catch
    //! forward references, so it does its own full-list scan.
    //! O(n²) in the number of labels.
    void removeUnusedLabels();                                          // 0x27e1c0

    // When consecutive labels exist, replace all references to the second
    // label with the first label, then delete the second
    //! \brief Coalesce runs of consecutive `LABEL` entries into a
    //!        single name and rewrite all branches that target the
    //!        absorbed names.
    //!
    //! \details For each `LABEL`, captures `firstLabel` and walks
    //! `next = it + 1` while `next` is also a `LABEL`.  For each
    //! follower, scans the entire list and rewrites every branch
    //! whose label equals the follower's name to `firstLabel` —
    //! the candidate `LABEL` itself is skipped via address
    //! comparison (`&*scan != &*it`) so the loop never rewrites
    //! its own anchor.  The follower then gets `cmd = INVALID`
    //! and a cleared label string.  O(n²) in label density.
    void mergeLabels();                                                 // 0x27e330

    // Merge consecutive ADDI r,0 + XORR r,r,r patterns into single XORR
    // (when src1 == r0 and imm == 0, replace ADDI with NOP and set dest = r0)
    //! \brief Recognise the two-instruction pattern
    //!        `ADDI rN, R0, 0 ; ADDIU rN, rN, ...` and fold the
    //!        redundant zeroing `ADDI` by rewriting the `ADDIU`'s
    //!        source to `R0`.
    //!
    //! \details Walks pairs `(prev = it - 1, it)` from index 1,
    //! skipping entries whose `noOpt` flag is set.  Requires
    //! `prev->cmd == ADDI`, `prev->regSrc == R0`,
    //! `prev->outputs.size() == 1`, `prev->outputs.back() == 0`,
    //! and `it->cmd == ADDIU`, `it->regSrc == it->regDst`,
    //! `it->regSrc == prev->regDst`.  On match: marks the `ADDI`
    //! dead (`cmd = INVALID`) and replaces the `ADDIU`'s
    //! `regSrc` with `AsmRegister(0)`.
    //!
    //! \note  The "NOP substitution" is in fact `cmd = INVALID`
    //!        (the universal dead marker), not a real `NOP`
    //!        opcode.  Skips the merge when the `ADDIU` is marked
    //!        `noOpt`.
    void mergeRegisterZeroing();                                        // 0x27e640

    // Identify registers used only as write destinations (never read),
    // eliminate their instructions, returns max register number seen
    //! \brief Two-in-one pass: track the highest virtual register
    //!        number across `asm_`, and (when `Opt_MergeZero`
    //!        is set) eliminate writes whose destination is never
    //!        read or invoke `simplifyAssign` to fold trivial
    //!        copies.
    //!
    //! \details Locks `cancel_` to a `shared_ptr` once and polls
    //! `isCancelled()` per outer iteration.  For each non-skip
    //! entry (cmd not in bitmask `0x29` on `cmd + 1`, i.e. not
    //! `INVALID` / `LABEL` / cmd-4), runs `highestRegisterNumber`
    //! to update `maxReg`.  When `optFlags_ & 0x08` is set and
    //! the entry writes (`cmdType & 2`), picks `destSlot =
    //! regDst != R0 ? regDst : regAux` and forward-scans for
    //! the next use of that slot (using a small accumulator vector
    //! to avoid re-checking already-written-only registers).  A
    //! branch that reads the register, or any `regAux` mention,
    //! conservatively keeps the register live.  Otherwise the
    //! accumulated `(read, written)` mask drives the action: bit
    //! 0 clear ⇒ write-only ⇒ `destSlot = AsmRegister(-1)` and
    //! `cmd = INVALID`; bit 1 clear ⇒ never overwritten ⇒
    //! `simplifyAssign(it)` may further fold the assignment.
    //!
    //! \return  `maxReg`, the highest virtual register number
    //!          observed; used by `optimizePostWaveform` to size
    //!          `registerAllocation`.
    //! \note  Honours the `cancel_` callback once per outer
    //!        iteration.
    unsigned long removeUnusedRegs();                                   // 0x27e760

    // Extract user MESSAGE (cmd==3) and ERROR_MSG (cmd==5) instructions,
    // invoke error/warning callbacks, then mark as dead
    //! \brief Surface user-level `MESSAGE` (cmd `3`) and
    //!        `ERROR_MSG` (cmd `5`) instructions to the host via
    //!        the warning / error callbacks, then strike them
    //!        from the asm.
    //!
    //! \details For each instruction: when `cmd == ERROR_MSG`,
    //! takes `immediates[0]`, converts via `toString(Immediate)`,
    //! reads the source line via `it->lineNumber()` (the `Asm`
    //! field at `+0x88`, an alias of `wavetableFront` for these
    //! opcodes), invokes `errorCallback_(msg, lineNr)` when set,
    //! then sets `cmd = INVALID` provided `optFlags_` is non-zero.
    //! `MESSAGE` is treated identically with `warningCallback_`.
    //!
    //! \note  The dead-strike is gated on `optFlags_` being
    //!        non-zero — any optimisation at all causes message
    //!        removal — not on a specific flag bit.  The `Asm`
    //!        layout overlays `lineNumber` with `wavetableFront`
    //!        only for these two opcodes.
    void reportUserMessages();                                          // 0x280b60

    // Try to simplify an ADDI+XORR assign sequence at the given position
    //! \brief Fold a "compute then trivially copy" pattern: when
    //!        the next instruction is `ADDI rDst', rN, 0` and
    //!        `rN == it->regDst` is unused later, retarget the
    //!        current op's `regDst` to `rDst'` and kill the
    //!        `ADDI`.
    //!
    //! \details Returns `false` when `it + 1 == asm_.end()`,
    //! when the next entry is not `ADDI`, when its
    //! `outputs[0] != Immediate(0)`, or when its `regSrc !=
    //! it->regDst`.  Otherwise scans `it + 2 .. end`: if any
    //! subsequent `regSrc` or `regAux` references the original
    //! `it->regDst`, returns `false` (subsequent **writes** to
    //! that register are intentionally **not** blocking — once
    //! the producer is redirected the temporary is fully
    //! unused).  On success, rewrites `it->regDst = next->regDst`
    //! and sets `next->cmd = INVALID`.
    //!
    //! \param it  Iterator at the writing instruction being
    //!            inspected.  Typically arrived at via
    //!            `removeUnusedRegs`.
    //! \return  `true` when the fold was applied, `false`
    //!          otherwise.
    bool simplifyAssign(AsmList::iterator it);                          // 0x280e10

    // ---- Register allocator ----

    // Full register allocation: build live ranges, allocate physical registers,
    // spill via splitReg on failure, iterative retry with splitConstRegisters
    //! \brief Linear-scan-style allocation that maps virtual
    //!        register numbers `1..numRegs` onto a pool of
    //!        `numPhysicalRegs_` physical registers, mutating
    //!        `asm_` in place to use the chosen physical names.
    //!
    //! \details The pass proceeds in four phases over `asm_`:
    //!  - **Live ranges** — a per-register vector of instruction
    //!    indices is built by scanning every non-skip entry
    //!    (cmd not in bitmask `0x29` over `cmd + 1`, i.e. not
    //!    `INVALID` / `LABEL` / cmd-4) and recording the indices
    //!    where its `regSrc`, `regDst`, or `regAux` mention each
    //!    virtual register, with duplicate-slot suppression.
    //!  - **Backward-branch extension** — labels are mapped to
    //!    instruction indices, then for each `BRZ` / `BRNZ` /
    //!    `BRGZ` / `JMP` whose target precedes the branch, every
    //!    virtual register whose last reference falls inside the
    //!    loop body is checked: if it is read before being killed
    //!    (`regDst` write or `regAux`-with-cmdType-7 write), the
    //!    branch index is appended to its live range to make it
    //!    live across the back-edge.
    //!  - **Conflict pool** — non-empty live ranges are moved
    //!    into a per-register `vector<vector<int>>` of sub-ranges
    //!    and the corresponding indices are inserted into a
    //!    `std::set<unsigned long>` of unmerged registers.
    //!  - **Greedy merge** — for `vreg = 1..numRegs`, walks the
    //!    set of unmerged registers `≥ vreg` (skipping `vreg`
    //!    itself) and tries to fold each candidate `preg`'s sub-
    //!    range into `physRegs[vreg]` either by appending,
    //!    prepending, or fitting into a gap between consecutive
    //!    sub-vectors.  A successful merge calls `registerUpdate`
    //!    to rename `preg → vreg` across all instructions in the
    //!    moved range and erases `preg` from the conflict set.
    //!    `cancel_->isCancelled()` is polled once per outer
    //!    iteration via a locked `shared_ptr` snapshot.
    //!
    //! \param numRegs  The highest virtual-register number in
    //!                 use, as returned by `removeUnusedRegs`.
    //!                 The pass allocates `numRegs + 1` slots so
    //!                 that index `0` represents `R0`.
    //! \throws OptimizeException  When a vreg fails to fit: the
    //!                 message is the verbatim string `"run out
    //!                 of free registers, please reduce
    //!                 complexity"` and the line number is
    //!                 derived from the `lineNumber()` of the
    //!                 first asm-list entry in the offending
    //!                 register's live range.  Two distinct
    //!                 sites raise it: when `vreg ==
    //!                 totalPhysical` (numPhysical + 1) and
    //!                 unmerged conflicts remain, and when
    //!                 `vreg ≥ numPhysical` exits the merge loop
    //!                 with `physVreg` non-empty.  This is the
    //!                 trigger that `optimizePostWaveform`
    //!                 catches to attempt a `splitConstRegisters`
    //!                 retry.
    //! \note  The merge order — ascending vreg, ascending
    //!        candidate preg — is a deliberate heuristic: small
    //!        registers act as accumulators and absorb larger
    //!        ones, so the worst-case spill is pushed to the
    //!        high vreg numbers that retry can address.
    void registerAllocation(unsigned long numRegs);                     // 0x27ebb0

    // Split constant-value registers to reduce live range conflicts
    //! \brief Reduce register pressure by splitting the live
    //!        range of every constant-load `ADDI rN, R0, #imm`
    //!        whose value is reused later in the program, then
    //!        return the new highest-virtual-register number.
    //!
    //! \details Two-pass rewrite of `asm_` through a temporary
    //! `AsmList`:
    //!  - **Pass 1** copies every entry into `tmpList`.  Skip
    //!    entries (cmd in bitmask `0x29` on `cmd + 1`) are
    //!    forwarded as a single copy that retains the original
    //!    `Node`.  Every other entry is preceded by **two**
    //!    barrier slots — copies of the assembler with
    //!    `cmd = INVALID` and `regSrc = AsmRegister::magicSkipRegister()`.
    //!    Those barrier slots are pre-allocated landing pads that
    //!    `splitReg` later overwrites with the reload `ADDI` /
    //!    `ADDIU` pair.  Each emitted entry receives a fresh
    //!    `sequenceId` from `AsmList::Asm::createUniqueID(false)`,
    //!    and the carried `noOpt` flag mirrors the original via
    //!    `(cmd - 3) < 3u`.
    //!  - **Pass 2** scans `tmpList` for splittable patterns:
    //!    only entries with `cmd ∈ { INVALID, ADDI, cmd-4 }` and
    //!    `regSrc == R0` qualify.  The destination is `regDst`.
    //!    A forward scan walks subsequent entries, skipping
    //!    `cmd-4` and `INVALID`; success requires the next live
    //!    entry to be an `ADDIU` whose `regDst` matches the
    //!    destination, and (depending on the outer cmd) whose
    //!    `regSrc` matches either the destination (`ADDI` outer)
    //!    or `R0` (`INVALID` / cmd-4 outer).  Before invoking
    //!    `splitReg`, an overwrite-safety scan rejects the
    //!    candidate if any entry between the two ends — other
    //!    than the splitEnd itself — writes the destination.
    //!    A successful candidate calls `splitReg(tmpList,
    //!    destReg, it, splitEnd)` and increments `splitCount`.
    //!  - **Post-pass** clears `asm_` and copies the non-barrier
    //!    entries back: an entry is dropped when its cmd is
    //!    `INVALID` or cmd-4 **and** its `regSrc` equals
    //!    `magicSkipRegister()`.
    //!
    //! \param numRegs  Current highest virtual-register number.
    //!                 The function adds the number of fresh
    //!                 registers minted by `splitReg` and
    //!                 returns the updated maximum, which is then
    //!                 fed back into a second
    //!                 `registerAllocation` attempt.
    //! \return  `numRegs + splitCount`.
    //! \note  `magicSkipRegister()` is the agreed-upon sentinel
    //!        that distinguishes barrier slots from ordinary
    //!        `INVALID` entries left by other passes.  Pass 2's
    //!        outer filter intentionally accepts cmd `4` (the
    //!        same opcode used elsewhere as a special skip
    //!        marker) so that earlier-rewritten constant loads
    //!        can be re-split.
    unsigned long splitConstRegisters(unsigned long numRegs);           // 0x280440

    // Split a register's live range at the given boundaries
    //! \brief Insert reload instructions into the barrier slots
    //!        that precede uses of `reg` once at least ten
    //!        non-skip instructions have elapsed since `start`,
    //!        renaming the use to a freshly minted virtual
    //!        register so that allocator pressure on `reg` drops.
    //!
    //! \details Walks `[start + 1, list.cend())` carrying a
    //! per-iteration `instrCount` (bumped at every non-skip
    //! exit, never reset) and two flags `didSplit` and
    //! `allSplitOk`.  The skip predicate is the standard `cmd in
    //! { INVALID, LABEL, cmd-4 }`.  Each surviving entry has
    //! its `cmdType` looked up; a use of `reg` is detected when
    //! `(cmdType & 1) && regSrc == reg`, or when
    //! `cmdType ∈ {1, 7} && regAux == reg`.  When the same
    //! entry also writes `reg` (`(cmdType >> 1) & 1` on `regDst`
    //! or `cmdType == 7` on `regAux`), the iteration is
    //! abandoned without changing flags.  A clean use with
    //! `instrCount < 10` clears `allSplitOk` and continues; a
    //! clean use with `instrCount ≥ 10` triggers the split.
    //!
    //! Each split:
    //!  - allocates a fresh `newReg = AsmRegister(GlobalResources::regNumber++)`,
    //!  - writes **Block 1** into the slot at `iter - 2`: a clone
    //!    of `start->assembler` (cmd preserved) with
    //!    `regDst = newReg`, the current entry's `wavetableFront`,
    //!    a fresh `sequenceId`, `noOpt` derived from `start.cmd`
    //!    via `(cmd - 3) < 3u`, and a null `node`,
    //!  - writes **Block 2** into the slot at `iter - 1` (only
    //!    when `end != list.end()`): a clone of `end->assembler`
    //!    with `regDst = newReg` and `regSrc` set to `newReg`
    //!    when `start.cmd == ADDI` else `R0`, mirroring `noOpt`
    //!    derivation,
    //!  - rewrites the current entry's `regSrc` and `regAux`
    //!    from `reg` to `newReg` (the `regDst` slot is
    //!    intentionally **not** rewritten).
    //!
    //! Epilogue: when `allSplitOk && didSplit`, the original
    //! `start.assembler.cmd` is set to `INVALID`, and so is
    //! `end.assembler.cmd` if `end != list.end()`.  These
    //! marked-dead entries are stripped by the post-pass in
    //! `splitConstRegisters`.
    //!
    //! \param list   The instruction list owning the barrier
    //!               slots.  Entries are mutated in place; no
    //!               element is inserted or erased.
    //! \param reg    The over-pressured register being split.
    //! \param start  Iterator at the first barrier-equipped
    //!               entry.  The function returns immediately
    //!               when `start + 1 >= list.cend()`.
    //! \param end    Iterator at the splitEnd entry chosen by
    //!               `splitConstRegisters`, or `list.end()` to
    //!               disable Block 2.  Used as a clone source
    //!               and as an additional dead-mark target.
    //! \note  The threshold `instrCount < 10` is the heuristic
    //!        that prevents splitting registers whose uses are
    //!        already close enough not to interfere with other
    //!        live ranges.  `allSplitOk` is cleared as soon as
    //!        any below-threshold use is seen, suppressing the
    //!        epilogue's dead-mark step so `start` / `end` are
    //!        retained for those uses.
    void splitReg(AsmList& list, AsmRegister reg,
                  AsmList::const_iterator start,
                  AsmList::const_iterator end);                         // 0x281000

    // Update all register references in the given instruction indices
    // from oldReg to newReg
    //! \brief Rename every occurrence of `oldReg` to `newReg`
    //!        across the `regSrc`, `regDst`, and `regAux` slots
    //!        of the instructions identified by `indices`.
    //!
    //! \details Iterates `indices` in reverse order (matching
    //! the binary's tail-to-head walk), bounds-checks each entry
    //! against `asm_.size()` to skip out-of-range values, and
    //! independently rewrites the three register slots when they
    //! compare equal to `oldReg`.  No `cmdType` filtering is
    //! performed — the caller (`registerAllocation`) has already
    //! confirmed that the indexed slot legitimately holds the
    //! register in question.
    //!
    //! \param indices  Sorted list of `asm_` indices that
    //!                 reference the merged register; supplied
    //!                 directly from the merged sub-range that
    //!                 `registerAllocation` is folding into a
    //!                 host vreg.
    //! \param oldReg   Register currently named in the indexed
    //!                 slots.
    //! \param newReg   Replacement register.
    //! \note  All three slots are inspected on every entry so a
    //!        single instruction touching the same register in
    //!        multiple slots (e.g. `ADD rA, rA, rA`) is fully
    //!        rewritten in one visit.
    void registerUpdate(const std::vector<int>& indices,
                        AsmRegister oldReg,
                        AsmRegister newReg);                            // 0x281680

    // ---- Data members ----

    //! \brief Upper bound on physical registers available to
    //!        `registerAllocation`.
    uint32_t numPhysicalRegs_;                              // +0x00
    //! \brief Alignment padding (zero-initialised).
    uint32_t pad04_;                                        // +0x04
    //! \brief Pass-selection bitmask (`OptPassFlag` values OR'd
    //!        together).
    uint32_t optFlags_;                                        // +0x08
    //! \brief Alignment padding (zero-initialised).
    uint32_t pad0C_;                                        // +0x0C
    //! \brief Working copy of the instruction stream mutated by the
    //!        optimisation passes.
    std::vector<AsmList::Asm> asm_;                         // +0x10 (working copy)
    // padding to +0x30
    //! \brief Host-supplied sink for `ERROR_MSG` instructions and
    //!        `OptimizeException` re-emission.
    std::function<void(const std::string&, int)> errorCallback_;    // +0x30
    //! \brief Host-supplied sink for `MESSAGE` instructions emitted
    //!        during optimisation.
    std::function<void(const std::string&, int)> warningCallback_;  // +0x60
    //! \brief Cooperative-cancellation handle polled by the longer
    //!        passes; may be null.
    std::shared_ptr<CancelCallback> cancel_;                        // +0x90
};

}  // namespace zhinst
