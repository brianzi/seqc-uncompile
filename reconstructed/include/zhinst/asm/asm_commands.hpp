// ============================================================================
// Reconstructed from disassembly of _seqc_compiler.so
// AsmCommands — main assembler code generator class
// ============================================================================
#pragma once

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "zhinst/codegen/awg_compiler_config.hpp"
#include "zhinst/asm/asm_commands_impl.hpp"
#include "zhinst/asm/asm_list.hpp"
#include "zhinst/asm/asm_register.hpp"
#include "zhinst/asm/assembler.hpp"
#include "zhinst/waveform/play_config.hpp"
#include "zhinst/ast/value.hpp"
#include "zhinst/waveform/waveform_front.hpp"
#include "zhinst/waveform/wavetable_front.hpp"

namespace zhinst {

// AsmCommands — generates assembler instruction records for the AWG processor.
//
// Offset  Size  Type                                    Name
// +0x00   8     (padding / no explicit base)
// +0x08   8     (padding / reserved)
// +0x10   8     std::unique_ptr<AsmCommandsImpl>        impl_
// +0x18   8     (padding — unique_ptr is 8 bytes; shared_ptr begins at +0x20)
// +0x20   16    std::shared_ptr<WavetableFront>         wavetable_     (approx)
// +0x30   16    (padding to +0x40)
// +0x40   32    std::function<void(const string&)>      errorHandler_
// +0x50   4     int                                     wavetableFrontIndex_
// +0x54   4     int                                     numChannelGroups_
// +0x58         END (sizeof(AsmCommands) = 0x58, approximate)
//
// Notes: +0x20 and +0x40 are approximate; confirmed: +0x10, +0x50, +0x54.
//! \brief Façade that emits AWG-processor instruction records.
//!
//! `AsmCommands` is the code-generation surface used by the rest of the
//! compiler: every method on this class corresponds to one assembler
//! mnemonic (`addr`, `brz`, `wvf`, `ld`, `cwvf`, …) and returns a
//! freshly built `AsmList::Asm` ready for appending to the active
//! `AsmList`.  Device-specific instruction encodings are delegated to
//! the polymorphic `AsmCommandsImpl` (Cervino vs Hirzel) held as a
//! pimpl, so the public API is uniform across hardware families.
//!
//! The class also exposes helpers for higher-level constructs that
//! expand into multiple instructions or carry IR-side metadata:
//! `asmPlay` / `asmTable` build playback nodes referencing concrete
//! waveforms, `asmPrefetch` and `asm*Placeholder` create the
//! placeholder entries the prefetch pass later fills in, and the
//! `genPlayConfig` helper assembles a `PlayConfig` from waveform and
//! channel-grouping context.
//!
//! Each emitted entry inherits the current `wavetableFrontIndex_`
//! (settable via `setWavetableFrontIndex`) and a fresh sequence ID,
//! so the surrounding `AsmList` retains stable insertion ordering
//! even across the multi-instruction expansions.  Register-bearing
//! mnemonics validate their operands via `isValid` and throw
//! `ResourcesException(ErrorMessageT::InvalidRegister, "<mnemonic>")`
//! on failure; the diagnostic name matches the public method name.
class AsmCommands {
public:
    //! \brief Construct the code-generator façade for one
    //!        compilation.
    //!
    //! \details Picks a device-appropriate `AsmCommandsImpl`
    //! subclass via `AsmCommandsImpl::getInstance(config)`
    //! (Cervino vs Hirzel) and stashes the wavetable handle and
    //! diagnostic sink for the lifetime of the object.
    //! `wavetableFrontIndex_` and `numChannelGroups_` start at
    //! `0`; the latter is later seeded from the device config
    //! by the surrounding `Compiler`.
    //!
    //! \param config        Compiler configuration for the
    //!                      current AWG core; selects the
    //!                      device-specific impl.
    //! \param wavetable     Shared wavetable front used by
    //!                      table-emitting instructions.
    //! \param errorHandler  Callback invoked when a Value→int
    //!                      conversion overflows or
    //!                      underflows in `toInt32`.
    AsmCommands(const AWGCompilerConfig& config,
                std::shared_ptr<WavetableFront> wavetable,
                std::function<void(const std::string&)> errorHandler);

    // =====================================================================
    // Waveform playback
    // =====================================================================

    //! \brief Emit `PRF` (waveform prefetch with two register
    //!        operands plus a 20-bit immediate index).
    //!
    //! \details Encodes `reg1` into the high register slot,
    //! `reg2` into the secondary slot, and `intArg` into a
    //! 20-bit immediate output.  Validates both registers; on
    //! failure raises `ResourcesException(InvalidRegister,
    //! "prf")`.
    AsmList::Asm prf(AsmRegister reg1, AsmRegister reg2, int intArg) const;
    //! \brief Emit `WPRF` (wait for waveform prefetch).
    //!        Device-specific encoding; delegates to `impl_`.
    AsmList::Asm wprf() const;
    //! \brief Emit `WWVFQ` (wait waveform queue).
    //!        Device-specific encoding; delegates to `impl_`.
    AsmList::Asm wwvfq() const;
    //! \brief Emit `WWVF` (write-waveform trigger,
    //!        opcode `0xF1000000`). No operands.
    AsmList::Asm wwvf() const;
    //! \brief Emit `WVF` (play waveform from register source
    //!        with destination register and explicit length).
    //!
    //! \details Validates `reg`; on failure raises
    //! `ResourcesException(InvalidRegister, "wvf")`.
    //! Device-specific encoding is delegated to `impl_`.
    AsmList::Asm wvf(AsmRegister reg, AsmRegister dstReg, int length) const;
    //! \brief Emit `WVFI` (play waveform indexed; like `wvf`
    //!        but reads the index from `reg`).
    //!
    //! \details Validates `reg`; raises
    //! `ResourcesException(InvalidRegister, "wvfi")` on
    //! failure.  Delegates to `impl_`.
    AsmList::Asm wvfi(AsmRegister reg, AsmRegister dstReg, int length) const;
    //! \brief Emit `WVFS` (play synthetic / dummy waveform
    //!        such as `playZero` or `playHold`).
    //!
    //! \details `type` selects the synthetic-waveform variant
    //! (`PlayDummyType` enum); valid values are `0` and `1`.
    //! Any other value raises `ResourcesException(ValueOverflow,
    //! "wvfs")`.  The chosen length register is the *higher*
    //! of `reg` and `R0` so callers may pass `R0` to mean
    //! "no register".  Delegates to `impl_`.
    AsmList::Asm wvfs(Assembler::PlayDummyType type, AsmRegister reg, int length) const;
    //! \brief Emit `WVFT` (play waveform from table by
    //!        register, with explicit length).  Delegates
    //!        to `impl_`.
    AsmList::Asm wvft(AsmRegister reg, int length) const;
    //! \brief Emit `CWVF` (configure waveform; immediate
    //!        encoded play-config word).
    AsmList::Asm cwvf(int value) const;
    //! \brief Emit `CWVFR` (configure waveform from
    //!        register-supplied play-config word).
    AsmList::Asm cwvfr(AsmRegister reg) const;

    // =====================================================================
    // Branch
    // =====================================================================

    //! \brief Emit unconditional branch — encoded as a
    //!        `BRZ R0, label` (always-true branch on `R0`).
    //!
    //! \param label  Target label name (resolved at link).
    //! \param noOpt  When `true`, mark the branch as
    //!               non-optimisable so later passes preserve
    //!               it verbatim.
    AsmList::Asm br(const std::string& label, bool noOpt) const;
    //! \brief Emit `BRZ` (branch if `reg == 0`).
    //!
    //! \details Validates `reg`; raises
    //! `ResourcesException(InvalidRegister, "brz")` on
    //! failure.  Device-specific encoding via `impl_`.
    AsmList::Asm brz(AsmRegister reg, const std::string& label, bool noOpt) const;
    //! \brief Emit `BRNZ` (branch if `reg != 0`).
    //!
    //! \details Validates `reg`; raises
    //! `ResourcesException(InvalidRegister, "brnz")` on
    //! failure.  Encoded directly (not via `impl_`).
    AsmList::Asm brnz(AsmRegister reg, const std::string& label, bool noOpt) const;
    //! \brief Emit `BRGZ` (branch if `reg > 0`,
    //!        opcode `0xF5000000`).
    //!
    //! \details Validates `reg`; raises
    //! `ResourcesException(InvalidRegister, "brgz")` on
    //! failure.  Encoded directly.
    AsmList::Asm brgz(AsmRegister reg, const std::string& label, bool noOpt) const;

    // =====================================================================
    // ALU register-register
    // =====================================================================

    //! \brief Emit a generic ALU register-register
    //!        instruction (`ADDR`, `SUBR`, `ANDR`, `ORR`,
    //!        `XNORR`, …).
    //!
    //! \details Validates both `dst` and `src`; raises
    //! `ResourcesException(InvalidRegister,
    //! commandToString(cmd))` on failure.  The named
    //! convenience wrappers below dispatch through this
    //! function.
    AsmList::Asm alur(Assembler::Command cmd, AsmRegister dst, AsmRegister src) const;
    //! \brief `dst += src`. Convenience for `alur(ADDR, …)`.
    AsmList::Asm addr(AsmRegister dst, AsmRegister src) const;
    //! \brief `dst -= src`. Convenience for `alur(SUBR, …)`.
    AsmList::Asm subr(AsmRegister dst, AsmRegister src) const;
    //! \brief `dst &= src`. Convenience for `alur(ANDR, …)`.
    AsmList::Asm andr(AsmRegister dst, AsmRegister src) const;
    //! \brief `dst |= src`. Convenience for `alur(ORR, …)`.
    AsmList::Asm orr(AsmRegister dst, AsmRegister src) const;
    //! \brief `dst = ~(dst ^ src)`. Convenience for
    //!        `alur(XNORR, …)`.
    AsmList::Asm xnorr(AsmRegister dst, AsmRegister src) const;

    // =====================================================================
    // Shift (delegates to impl_)
    // =====================================================================

    //! \brief Emit `SSL` (shift register `reg` single-bit
    //!        left).  Delegates to `impl_`; raises
    //!        `ResourcesException(InvalidRegister, "ssl")`
    //!        on register validation failure.
    AsmList::Asm ssl(AsmRegister reg) const;
    //! \brief Emit `SSR` (shift register `reg` single-bit
    //!        right).  Delegates to `impl_`; raises
    //!        `ResourcesException(InvalidRegister, "ssr")`
    //!        on register validation failure.
    AsmList::Asm ssr(AsmRegister reg) const;

    // =====================================================================
    // ALU immediate-unsigned (single instruction)
    // =====================================================================

    //! \brief Emit a generic single-instruction
    //!        immediate-unsigned ALU op (`ADDIU`, `ANDIU`,
    //!        `ORIU`, `XNORIU`).
    //!
    //! \details The 20-bit unsigned `imm` is stored as an
    //! `outputs` immediate (not the `immediates` slot).
    //! Validates `dst`/`src`; raises
    //! `ResourcesException(InvalidRegister, commandToString(cmd))`
    //! on failure.
    AsmList::Asm aluiu(Assembler::Command cmd, AsmRegister dst,
                   AsmRegister src, Immediate imm) const;
    //! \brief `dst = src + imm` (unsigned 20-bit immediate).
    AsmList::Asm addiu(AsmRegister dst, AsmRegister src, Immediate imm) const;
    //! \brief `dst = src & imm` (unsigned 20-bit immediate).
    AsmList::Asm andiu(AsmRegister dst, AsmRegister src, Immediate imm) const;
    //! \brief `dst = src | imm` (unsigned 20-bit immediate).
    AsmList::Asm oriu(AsmRegister dst, AsmRegister src, Immediate imm) const;
    //! \brief `dst = ~(src ^ imm)` (unsigned 20-bit immediate).
    AsmList::Asm xnoriu(AsmRegister dst, AsmRegister src, Immediate imm) const;

    // =====================================================================
    // ALU immediate-signed (may emit multiple instructions)
    // =====================================================================

    //! \brief Emit a signed-immediate ALU op
    //!        (`ADDI`/`ANDI`/`ORI`/`XNORI`), expanding to
    //!        multiple instructions when `imm` does not fit
    //!        the 19-bit signed immediate field.
    //!
    //! \details For an immediate in the
    //! `[-2^18, 2^18 - 1]` range, emits a single instruction
    //! with `imm` in the outputs slot.  Otherwise:
    //! - `ADDI` is split into `ADDI dst, src, low12` followed
    //!   by `ADDIU dst, dst, upper>>12`.
    //! - `ANDI` / `ORI` / `XNORI` first load the constant
    //!   into `dst` via the same low12+upper expansion (with
    //!   `src=R0`), then emit a register-register version of
    //!   the same op (`ANDR`/`ORR`/`XNORR`) operating on
    //!   `dst, src`.
    //! - Any other command raises
    //!   `ResourcesException(ErrorMessageT(0xD8),
    //!   commandToString(cmd))`.
    //!
    //! Validates `dst`/`src`; raises
    //! `ResourcesException(InvalidRegister, commandToString(cmd))`
    //! on failure.
    std::vector<AsmList::Asm> alui(Assembler::Command cmd, AsmRegister dst,
                               AsmRegister src, Immediate imm) const;
    //! \brief `dst = src + imm`. May expand to two
    //!        instructions when `imm` exceeds the 19-bit
    //!        signed range — see `alui`.
    std::vector<AsmList::Asm> addi(AsmRegister dst, AsmRegister src, Immediate imm) const;
    //! \brief Always emit a 2-instruction ADDI sequence
    //!        carrying a full 32-bit signed immediate.
    //!
    //! \details Unlike `addi`, no small-immediate fast path
    //! is taken: this always emits `ADDI dst, src, imm&0xFFF`
    //! followed by `ADDIU dst, dst, imm>>12`, and forces both
    //! entries' `noOpt` flag so later passes do not collapse
    //! the pair.  Used when the caller relies on the exact
    //! two-instruction layout (e.g. patchable immediates).
    //! Validates `dst`/`src`; raises
    //! `ResourcesException(InvalidRegister, "addi32")` on
    //! failure.
    std::vector<AsmList::Asm> addi32(AsmRegister dst, AsmRegister src, Immediate imm) const;
    //! \brief `dst = src & imm`. Multi-instruction expansion
    //!        for large `imm` — see `alui`.
    std::vector<AsmList::Asm> andi(AsmRegister dst, AsmRegister src, Immediate imm) const;
    //! \brief `dst = src | imm`. Multi-instruction expansion
    //!        for large `imm` — see `alui`.
    std::vector<AsmList::Asm> ori(AsmRegister dst, AsmRegister src, Immediate imm) const;
    //! \brief `dst = ~(src ^ imm)`. Multi-instruction
    //!        expansion for large `imm` — see `alui`.
    std::vector<AsmList::Asm> xnori(AsmRegister dst, AsmRegister src, Immediate imm) const;

    // =====================================================================
    // ALU with Value (runtime variant) — converts to int, then delegates
    // =====================================================================

    //! \brief Convert a SeqC `Value` to `int32_t`, surfacing
    //!        out-of-range conditions through `errorHandler_`.
    //!
    //! \details Calls `Value::toInt`.  On `std::overflow_error`
    //! reports `ErrorMessageT::ValueOutOfRange` (formatted
    //! with the original double and the bit-width 32) via
    //! `errorHandler_` and returns `INT_MAX`; the symmetric
    //! `std::underflow_error` path reports the same and
    //! returns `INT_MIN`.  Used as the front door for every
    //! `Value`-overload below so that a single error path
    //! handles arithmetic-overflow diagnostics uniformly.
    //!
    //! \param val  Source value to clamp into `int32_t`.
    //! \return  Clamped integer (or the original `toInt`
    //!          result when in range).
    int toInt32(Value val) const;

    //! \brief `Value`-overload of `addi`; converts `val`
    //!        through `toInt32` then dispatches.
    std::vector<AsmList::Asm> addi(AsmRegister dst, AsmRegister src, Value val) const;
    //! \brief `Value`-overload of `addiu`; converts `val`
    //!        through `toInt32` then dispatches.
    AsmList::Asm addiu(AsmRegister dst, AsmRegister src, Value val) const;
    //! \brief `Value`-overload of `andi`; converts `val`
    //!        through `toInt32` then dispatches.
    std::vector<AsmList::Asm> andi(AsmRegister dst, AsmRegister src, Value val) const;
    //! \brief `Value`-overload of `andiu`; converts `val`
    //!        through `toInt32` then dispatches.
    AsmList::Asm andiu(AsmRegister dst, AsmRegister src, Value val) const;
    //! \brief `Value`-overload of `ori`; converts `val`
    //!        through `toInt32` then dispatches.
    std::vector<AsmList::Asm> ori(AsmRegister dst, AsmRegister src, Value val) const;
    //! \brief `Value`-overload of `oriu`; converts `val`
    //!        through `toInt32` then dispatches.
    AsmList::Asm oriu(AsmRegister dst, AsmRegister src, Value val) const;
    //! \brief `Value`-overload of `xnori`; converts `val`
    //!        through `toInt32` then dispatches.
    std::vector<AsmList::Asm> xnori(AsmRegister dst, AsmRegister src, Value val) const;
    //! \brief `Value`-overload of `xnoriu`; converts `val`
    //!        through `toInt32` then dispatches.
    AsmList::Asm xnoriu(AsmRegister dst, AsmRegister src, Value val) const;

    // =====================================================================
    // Load / Store
    // =====================================================================

    //! \brief Emit `LD reg, addr` (load from data-memory
    //!        address `addr` into `reg`).
    //!
    //! \details Validates `reg`; raises
    //! `ResourcesException(InvalidRegister, "ld")` on
    //! failure.  `addr.value` is stored in the outputs slot.
    AsmList::Asm ld(AsmRegister reg, detail::AddressImpl<unsigned int> addr) const;
    //! \brief Emit `ST reg, addr` (store `reg` to data-memory
    //!        address `addr`).
    //!
    //! \details Validates `reg`; raises
    //! `ResourcesException(InvalidRegister, "st")` on
    //! failure.
    AsmList::Asm st(AsmRegister reg, detail::AddressImpl<unsigned int> addr) const;
    //! \brief Load DIO bus value into `reg`.
    //!
    //! \details Convenience for `ld(reg, kDioAddrLow)` or
    //! `ld(reg, kDioAddrHigh)` selected by `highBank`.
    //! Raises `ResourcesException(InvalidRegister, "ldio")`
    //! on register validation failure.
    AsmList::Asm ldio(AsmRegister reg, bool highBank) const;
    //! \brief Store `reg` to the DIO bus.
    //!
    //! \details Convenience for `st(reg, kDioAddrLow)` or
    //! `st(reg, kDioAddrHigh)` selected by `highBank`.
    //! Raises `ResourcesException(InvalidRegister, "sdio")`
    //! on register validation failure.
    AsmList::Asm sdio(AsmRegister reg, bool highBank) const;
    //! \brief Load from the user-register address space.
    //!
    //! \details Same opcode as `ld`; the distinct mnemonic
    //! is preserved in the IR for diagnostic clarity but
    //! emits the same `LD` instruction.  Raises
    //! `ResourcesException(InvalidRegister, "luser")` on
    //! register validation failure.
    AsmList::Asm luser(AsmRegister reg, detail::AddressImpl<unsigned int> addr) const;
    //! \brief Store to the user-register address space.
    //!
    //! \details Same opcode as `st`; see `luser` for the
    //! aliasing rationale.  Raises
    //! `ResourcesException(InvalidRegister, "suser")` on
    //! register validation failure.
    AsmList::Asm suser(AsmRegister reg, detail::AddressImpl<unsigned int> addr) const;

    // =====================================================================
    // Trigger
    // =====================================================================

    //! \brief Read the trigger register into `reg`
    //!        (`ld(reg, kAddrTrigger)`).  Raises
    //!        `ResourcesException(InvalidRegister, "ltrig")`
    //!        on register validation failure.
    AsmList::Asm ltrig(AsmRegister reg) const;
    //! \brief Write `reg` to the trigger register
    //!        (`st(reg, kAddrTrigger)`).  Raises
    //!        `ResourcesException(InvalidRegister, "strig")`
    //!        on register validation failure.
    AsmList::Asm strig(AsmRegister reg) const;
    //! \brief Write `reg` to the internal-trigger register
    //!        (`st(reg, kAddrInternalTrig)`).  Raises
    //!        `ResourcesException(InvalidRegister, "sinttrig")`
    //!        on register validation failure.
    AsmList::Asm sinttrig(AsmRegister reg) const;
    //! \brief Emit `WTRIG r1, r2` (wait for trigger
    //!        condition encoded across two registers).
    //!
    //! \details `r2` populates the source slot and `r1` the
    //! aux slot.  Validates both; raises
    //! `ResourcesException(InvalidRegister, "wtrig")` on
    //! failure.
    AsmList::Asm wtrig(AsmRegister r1, AsmRegister r2) const;
    //! \brief Emit `WTRIGI value` (wait for trigger with
    //!        immediate mask).
    AsmList::Asm wtrigi(int value) const;

    // =====================================================================
    // Misc I/O
    // =====================================================================

    //! \brief Write `reg` to the device-id register
    //!        (`st(reg, kIdAddrLow|kIdAddrHigh)` selected
    //!        by `highBank`).  Raises
    //!        `ResourcesException(InvalidRegister, "sid")`
    //!        on register validation failure.
    AsmList::Asm sid(AsmRegister reg, bool highBank) const;
    //! \brief Emit a 3-instruction `smap` sequence
    //!        (mapping-table write).
    //!
    //! \details Returns the concatenation of `alui(ADDI, r1,
    //! R0, value)` (which itself may be 1 or 2 instructions
    //! depending on the magnitude of `value`), followed by
    //! `st(r1, 0x62)` and `st(r2, 0x63)`.  Validates `r1`
    //! and `r2`; raises
    //! `ResourcesException(InvalidRegister, "smap")` on
    //! failure.
    std::vector<AsmList::Asm> smap(AsmRegister r1, AsmRegister r2, int value) const;
    //! \brief Emit a DIO-trigger load (`LDIOTRIG`).
    //!        Device-specific encoding; delegates to
    //!        `impl_`.
    AsmList::Asm ldiotrig(AsmRegister reg) const;
    //! \brief Emit `LCNT reg, addr` (load loop counter).
    //!
    //! \details Adds `0x64` (100) to `addr.value` before
    //! delegating to `ld`, placing the access into the
    //! loop-counter bank that overlaps the upper register
    //! address space.  Raises
    //! `ResourcesException(InvalidRegister, "lcnt")` on
    //! register validation failure.
    AsmList::Asm lcnt(AsmRegister reg, detail::AddressImpl<unsigned int> addr) const;

    // =====================================================================
    // Control flow / special
    // =====================================================================

    //! \brief Emit `TRAP` (debugger trap / breakpoint).
    AsmList::Asm trap() const;
    //! \brief Emit `IRPT` (raise interrupt).
    AsmList::Asm irpt() const;
    //! \brief Emit `END` (program terminator).
    AsmList::Asm end() const;
    //! \brief Emit `NOP` (no-operation; one cycle).
    AsmList::Asm nop() const;

    // =====================================================================
    // Sync
    // =====================================================================

    //! \brief Emit the multi-instruction Cervino-family
    //!        synchronisation barrier.
    //!
    //! \details Builds a 7- or 8-instruction sequence
    //! coordinating two scratch registers, the
    //! sync trigger mask `0x800000`, the per-core sync
    //! mask `0x400000`, and the user-register banks
    //! `0x44`/`0x45` so that every AWG core in a sync
    //! group has reached the barrier before any continues.
    //! `flag == true` selects the 8-instruction primary
    //! path (uses `0x44`); `flag == false` selects the
    //! 7-instruction shorter path (uses `0x45`).  The two
    //! scratch registers `reg1`/`reg2` are clobbered.
    //!
    //! \param reg1  First scratch register.
    //! \param reg2  Second scratch register.
    //! \param flag  Selects between the two sync flavours
    //!              (see above).
    //! \return  Multi-instruction sequence as an `AsmList`.
    AsmList syncCervino(AsmRegister reg1, AsmRegister reg2, bool flag) const;
    //! \brief Emit the 2-instruction Cervino unsync
    //!        sequence (clears user registers `0x44` and
    //!        `0x45`).
    //!
    //! \return  Two `ST R0, 0x44` / `ST R0, 0x45` entries
    //!          packaged as an `AsmList`.
    AsmList unsyncCervino() const;
    //! \brief Emit the Cervino sync placeholder node — a
    //!        `SyncPlaceholderCervino` node that the
    //!        prefetch / sync pass later replaces with the
    //!        full `syncCervino` expansion once the
    //!        barrier participants are known.
    AsmList::Asm asmSyncPlaceholderCervino() const;
    //! \brief Emit the single-instruction Hirzel-family
    //!        synchronisation barrier.
    //!
    //! \details Equivalent to `suser(R0, kSuserSyncHirzel)`;
    //! Hirzel devices implement sync entirely in hardware
    //! once the user-register write occurs, so the
    //! multi-instruction Cervino sequence is unnecessary.
    AsmList::Asm asmSyncHirzel() const;

    // =====================================================================
    // Pseudo-instructions / directives
    // =====================================================================

    //! \brief Zero `reg` via `ADDI reg, R0, 0`.
    AsmList::Asm asmZero(AsmRegister reg) const;
    //! \brief Set `reg` to `1` via `ADDI reg, R0, 1`.
    AsmList::Asm asmOne(AsmRegister reg) const;
    //! \brief Emit a `LABEL` directive carrying `label`.
    //!
    //! \details The entry's `wavetableFront` is forced to
    //! `0` so labels do not inherit the surrounding
    //! per-waveform context — they are global to the
    //! `AsmList`.
    AsmList::Asm asmLabel(const std::string& label) const;
    //! \brief Emit a diagnostic `MESSAGE` (`isError == false`)
    //!        or `ERROR` (`isError == true`) pseudo-instruction
    //!        carrying a string payload.
    //!
    //! \details The opcode is `3` for messages and `5` for
    //! errors; the message text is stored in the
    //! `immediates` slot as an `Immediate(string)`.
    //! `wavetableFront` is forced to `0`.
    AsmList::Asm asmMessage(const std::string& msg, bool isError) const;

    // =====================================================================
    // Node-creating commands
    // =====================================================================

    //! \brief Emit a `Branch` IR node entry — placeholder for
    //!        a branch target the IR pass will resolve.
    AsmList::Asm asmBranchNode() const;
    //! \brief Emit a `Loop` IR node entry — placeholder for
    //!        loop boundary metadata used by later passes.
    AsmList::Asm asmLoopNode() const;
    //! \brief Emit a `Rate` IR node carrying the global
    //!        sample-rate `rate` (written into
    //!        `Node::globalRate` at `+0x100`).
    AsmList::Asm asmRate(int rate) const;
    //! \brief Emit a `SetPrecomp` IR node carrying the
    //!        default pre-compensation `flags` bitmask
    //!        (written into `Node::defaultPrecompFlags`
    //!        at `+0x104`).
    AsmList::Asm asmSetPrecompFlags(unsigned int flags) const;

    // =====================================================================
    // Placeholder / waveform management (non-const)
    // =====================================================================

    //! \brief Emit a `SetVar` placeholder node parameterised
    //!        by length-register `reg`.
    //!
    //! \details Stores `reg` into `Node::lengthReg`.  Used
    //! to defer the resolution of variable-length playback
    //! until the surrounding context is fully analysed.
    AsmList::Asm asmSetVarPlaceholder(AsmRegister reg);
    //! \brief Emit a `LockPlaceholder` node binding waveform
    //!        `wvf` to slot `index` for later locking.
    //!
    //! \details Stores `wvf->name` into
    //! `node->wavesPerDev[index]` and sets
    //! `node->deviceIndex = index`.  The lock pass uses
    //! these to drive `LD`/`ST` accesses to the per-device
    //! waveform memory.
    AsmList::Asm asmLockPlaceholder(std::shared_ptr<WaveformFront> wvf, int index);
    //! \brief Emit an `UnlockPlaceholder` node releasing the
    //!        slot bound by an earlier `asmLockPlaceholder`.
    //!
    //! \details Sets `node->deviceIndex = index`.  `wvf` is
    //! retained for symmetry with `asmLockPlaceholder` but
    //! the unlock node only needs the index.
    AsmList::Asm asmUnlockPlaceholder(std::shared_ptr<WaveformFront> wvf, int index);
    //! \brief Emit a `Load` placeholder — a pass-through
    //!        marker the prefetch pass replaces with a
    //!        concrete load sequence.
    AsmList::Asm asmLoadPlaceholder();
    //! \brief Emit a `PlainLoad` prefetch node binding
    //!        waveform `wvf` to register / length metadata.
    //!
    //! \details Stores `regVal` as `node->lengthReg` and
    //! `extraVal` as `node->length`; when `wvf` is
    //! non-null, marks it as `used` and records its name in
    //! `node->wavesPerDev[nameIndex]`.  `node->deviceIndex`
    //! is always set to `nameIndex`.
    //!
    //! \param wvf        Waveform to prefetch (may be null
    //!                   for synthetic prefetches).
    //! \param nameIndex  Per-device slot index.
    //! \param regVal     Length-register value to embed.
    //! \param extraVal   Explicit length to embed.
    AsmList::Asm asmPrefetch(std::shared_ptr<WaveformFront> wvf,
                         int nameIndex, int regVal, int extraVal);

    // =====================================================================
    // Play / Table
    // =====================================================================

    //! \brief Build a `PlayConfig` packing playback metadata
    //!        for one waveform invocation.
    //!
    //! \details Populates `rate`, `suppress`, `is4Channel`,
    //! `trigger`, `precompFlags = 0`, `now = playNow`, and
    //! `hold` from the corresponding parameters.  Marker
    //! processing (computing `markerBits`) and channel-mask
    //! derivation depend on `wvf`:
    //! - When `wvf` is null, `channelMask = 0`,
    //!   `markerBits = 0`, and `dummy = true`.
    //! - Otherwise `dummy = false`, `channelMask` is derived
    //!   from `wvf->signal.channels_` (single-channel
    //!   waveforms map to `0b10` when `isHold`, otherwise
    //!   the full mask), and `markerBits` is computed from
    //!   `wvf->signal.markerBits_` only when
    //!   `impl_->isCWVFRSupported()` returns `true` — UHF
    //!   (Cervino) devices skip marker processing and leave
    //!   `markerBits == 0`.
    //!
    //! \binarynote Two `bool` arguments duplicate
    //!             playback-mode information: `fourChannel`
    //!             is unused inside `genPlayConfig` itself
    //!             (it is consumed by the surrounding
    //!             `asmPlay` call), and `is4Channel`
    //!             populates `PlayConfig::is4Channel`
    //!             directly.
    PlayConfig genPlayConfig(const std::shared_ptr<WaveformFront>& wvf,
                             bool isHold, bool fourChannel, bool playNow,
                             bool hold, int rate, unsigned int suppress,
                             bool is4Channel, unsigned int trigger) const;

    //! \brief Emit a `Play` IR node referencing one or more
    //!        concrete waveforms.
    //!
    //! \details Allocates a `Play` node and, when
    //! `waveforms` is non-empty, populates
    //! `node->wavesPerDev` with each waveform's name (or
    //! `nullopt` for null entries) and sets
    //! `node->deviceIndex = deviceIndex`.  The dummy-play
    //! path (empty `waveforms`) leaves `wavesPerDev` at its
    //! constructor-assigned `nullopt` slots and
    //! `deviceIndex` at `-1`, which the downstream
    //! `getWaveformByName` lookup uses to set
    //! `PlayConfig::dummy = true`.  The waveform at
    //! `waveforms[deviceIndex]` (if in range) feeds
    //! `genPlayConfig`; the resulting `PlayConfig` is also
    //! encoded back into `WaveformFront::playConfig` via
    //! `encodeCwvf(-1)` and the waveform is marked `used`.
    AsmList::Asm asmPlay(std::vector<std::shared_ptr<WaveformFront>> waveforms,
                     int deviceIndex, bool isHold, bool fourChannel, bool hold,
                     int rate, unsigned int suppress, bool is4Channel,
                     AsmRegister lengthReg, int length, AsmRegister reg2,
                     unsigned int trigger);

    //! \brief Emit a `Table` IR node referencing a single
    //!        waveform indexed via the wavetable.
    //!
    //! \details Computes a `PlayConfig` via `genPlayConfig`,
    //! sets `lengthReg` / `length`, and records
    //! `tableIndex` plus `deviceIndex` on the node.  When
    //! `wvf` is non-null, marks it `used`.  The
    //! `playNow`/`hold` arguments to `genPlayConfig` are
    //! pinned to `fourChannel` / `is4Channel` and the
    //! trailing `is4Channel`/`trigger` arguments are pinned
    //! to `false` / `0` — table playback never carries
    //! a "play now" flag or external-trigger context at
    //! this layer.
    AsmList::Asm asmTable(int tableIndex, std::shared_ptr<WaveformFront> wvf,
                      int deviceIndex, bool isHold, bool fourChannel,
                      int rate, unsigned int suppress, bool is4Channel,
                      AsmRegister lengthReg, int length);

    // =====================================================================
    // Misc
    // =====================================================================

    //! \brief Emit a `WTRIG-LS` placeholder as a `ST R0,
    //!        (value + 0x40)` write.
    //!
    //! \details The `+0x40` offset places the store in the
    //! late-sync trigger bank.  The placeholder is later
    //! patched by the trigger-resolution pass.
    AsmList::Asm asmWtrigLSPlaceholder(int value);
    //! \brief Emit `FB value` (feedback / future-broadcast
    //!        immediate instruction).
    AsmList::Asm fb(int value) const;

    // Setter for the dual-purpose wavetable-front / source-line index at
    // +0x50. Used by Prefetch::placeSingleCommand (0x1d79bf) to propagate
    // the placeholder's `wavetableFront` value into the active assembler
    // context before emitting commands.
    //! \brief Set the per-instruction `wavetableFront` index
    //!        propagated into every entry emitted from now
    //!        on.
    //!
    //! \details Updates the field at `+0x50`, also used by
    //! `Prefetch::placeSingleCommand` to propagate a
    //! placeholder's index into the active assembler
    //! context before emitting commands.  The value is
    //! copied into each `AsmList::Asm::wavetableFront`
    //! produced by subsequent emit calls (except `asmLabel`
    //! and `asmMessage`, which force `0`).
    void setWavetableFrontIndex(int value) { wavetableFrontIndex_ = value; }
    //! \brief Read the current `wavetableFront` index — see
    //!        `setWavetableFrontIndex`.
    int  wavetableFrontIndex() const { return wavetableFrontIndex_; }

private:
    std::unique_ptr<AsmCommandsImpl> impl_;                      // +0x10
    std::shared_ptr<WavetableFront> wavetable_;                  // ~+0x20
    std::function<void(const std::string&)> errorHandler_;       // ~+0x40
    int wavetableFrontIndex_ = 0;                                // +0x50
    int numChannelGroups_ = 0;                                   // +0x54

    // Helper: build an AsmList::Asm from a local Assembler
    AsmList::Asm emitEntry(const Assembler& instr) const;
    AsmList::Asm emitEntry(const Assembler& instr, int overrideWavetableFront) const;

    // Helper: build an AsmList::Asm with a Node
    AsmList::Asm emitNodeEntry(NodeType type) const;
};

} // namespace zhinst
