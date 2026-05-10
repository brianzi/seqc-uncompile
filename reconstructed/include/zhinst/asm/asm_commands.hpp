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

    //! \brief Emit `PRF` — opcode `0x10000000`, prefetch a
    //!        waveform into the playback cache.
    //!
    //! \details Three-register format (`opcodeType=3`,
    //! `regOrder=3`): `reg1` selects the Hirzel-cache slot,
    //! `reg2` the Cervino-cache slot, and `intArg` carries
    //! the 20-bit length immediate.  Validates both
    //! registers; on failure raises
    //! `ResourcesException(InvalidRegister, "prf")`.
    //! Emitted by the prefetch family in `prefetch_*.cpp`.
    AsmList::Asm prf(AsmRegister reg1, AsmRegister reg2, int intArg) const;
    //! \brief Emit `WPRF` — **wait for prefetch** barrier
    //!        (opcode `0xF0000000`, no operands).
    //!
    //! \details The `w` prefix denotes "wait for" (see opcode
    //! naming conventions); `wprf` pairs with `prf` to
    //! synchronise the prefetch pipeline.  Cervino emits the
    //! real opcode; Hirzel substitutes the no-op sentinel
    //! `0xFFFFFFFF` because its hardware has no separate
    //! prefetch-completion signal.  Device-specific encoding
    //! is delegated to `impl_`.
    AsmList::Asm wprf() const;
    //! \brief Emit `WWVFQ` — **wait for waveform queue**
    //!        barrier (opcode alias of `WPRF` at
    //!        `0xF0000000`, no operands).
    //!
    //! \binarynote Hirzel-only.  Cervino's
    //! `AsmCommandsImplCervino::wwvfq` throws
    //! `ResourcesException(UnsupportedOp, "wwvfq")` because
    //! UHF-family devices have no playback queue.  Emitted
    //! by `waitPlayQueueEmpty`.
    AsmList::Asm wwvfq() const;
    //! \brief Emit `WWVF` — **wait for waveform** barrier
    //!        (opcode `0xF1000000`, no operands).
    //!
    //! \details Appended by `waitWave` and by the program
    //! trailer in `compiler.cpp` to drain in-flight waveform
    //! playback before program end.
    AsmList::Asm wwvf() const;
    //! \brief Emit `WVF` — **play waveform** (opcode
    //!        `0x20000000`).
    //!
    //! \details Three-register format on Cervino (waveform
    //! address in `dstReg`, marker source in `reg`, length
    //! immediate).  On Hirzel, when `dstReg == R0` the
    //! factory falls through to the extended-opcode form
    //! `WVFE` (`0xFA000000`, single register) instead.
    //! Validates `reg`; on failure raises
    //! `ResourcesException(InvalidRegister, "wvf")`.
    //! Device-specific encoding is delegated to `impl_`.
    AsmList::Asm wvf(AsmRegister reg, AsmRegister dstReg, int length) const;
    //! \brief Emit `WVFI` — play waveform, **indexed**
    //!        (opcode `0x30000000`).
    //!
    //! \details The `i` suffix means "indexed" (the address
    //! comes from an index register), **not** "interleaved".
    //! Cervino-only — Hirzel's `wvfi` impl throws
    //! `UnsupportedOp` (the equivalent extended-opcode
    //! `WVFEI` exists in the parser but is never emitted).
    //! Validates `reg`; raises
    //! `ResourcesException(InvalidRegister, "wvfi")` on
    //! failure.  Delegates to `impl_`.
    AsmList::Asm wvfi(AsmRegister reg, AsmRegister dstReg, int length) const;
    //! \brief Emit `WVFS` — set up a waveform-fetch
    //!        descriptor (opcode `0x30000001`).
    //!
    //! \details `type` is a `PlayDummyType` selector that
    //! occupies a 1-bit slot; valid values are `0` and `1`,
    //! anything else raises `ResourcesException(ValueOverflow,
    //! "wvfs")`.  The remaining bits hold a 20-bit offset.
    //! The chosen length register is the higher of `reg` and
    //! `R0` so callers may pass `R0` to mean "no register".
    //! Emitted by `Prefetch::wvfs` to install the
    //! prefetch-descriptor pair (`addi` + `wvfs`) that
    //! precedes a waveform-fetch.
    //!
    //! \binarynote Hirzel-only.  Cervino's
    //! `AsmCommandsImplCervino::wvfs` throws
    //! `ResourcesException(UnsupportedOp, "wvfs")`.
    AsmList::Asm wvfs(Assembler::PlayDummyType type, AsmRegister reg, int length) const;
    //! \brief Emit `WVFT` — execute a waveform-table entry
    //!        (Hirzel extended-opcode `WVFET` = `0xFC000000`).
    //!
    //! \details Single-register form: `reg` carries the
    //! table-entry index, `length` the immediate.  Emitted
    //! by `playWaveDIO`, `playZSync`, and `executeTableEntry`
    //! to dispatch a precomposed table entry.
    //!
    //! \binarynote Hirzel-only.  Cervino's
    //! `AsmCommandsImplCervino::wvft` throws
    //! `ResourcesException(UnsupportedOp, "wvft")`.
    AsmList::Asm wvft(AsmRegister reg, int length) const;
    //! \brief Emit `CWVF` — **configure waveform**, immediate
    //!        form (opcode `0xF2000000`).
    //!
    //! \details Writes the PlayConfig register (rate,
    //! suppress flag, marker bits, channel mask) packed by
    //! `PlayConfig::encodeCwvf()` into the 24-bit
    //! immediate.  When the packed value would not fit, the
    //! caller falls back to the register-operand spillover
    //! form `cwvfr`.
    AsmList::Asm cwvf(int value) const;
    //! \brief Emit `CWVFR` — configure waveform from
    //!        register-supplied PlayConfig word
    //!        (opcode `0xF9000000`).
    //!
    //! \details Spillover form of `cwvf` for packed
    //! PlayConfig values that exceed the 24-bit immediate
    //! capacity.
    //!
    //! \binarynote Hirzel-only — gated by
    //! `impl_->isCWVFRSupported()`.  This same flag also
    //! gates marker-bit computation in `genPlayConfig` and
    //! the Hirzel-only `wvfs` / `wvft` / `wwvfq` factories.
    AsmList::Asm cwvfr(AsmRegister reg) const;

    // =====================================================================
    // Branch
    // =====================================================================

    //! \brief Emit an unconditional branch to `label`.
    //!
    //! \details Encoded as `BRZ R0, label` (branch on a
    //! register that is always zero).  On Hirzel this falls
    //! through `AsmCommandsImplHirzel::brz`'s `reg == R0`
    //! branch and produces the dedicated `JMP` opcode
    //! `0xFE000000` instead of `BRZ`'s `0xF3000000`; on
    //! Cervino the branch remains a `BRZ` carrying `R0`.
    //!
    //! \param label  Target label name (resolved at link).
    //! \param noOpt  When `true`, mark the branch as
    //!               non-optimisable so later passes preserve
    //!               it verbatim.
    AsmList::Asm br(const std::string& label, bool noOpt) const;
    //! \brief Emit `BRZ` — branch if `reg == 0`
    //!        (opcode `0xF3000000`, 3-cycle branch penalty).
    //!
    //! \details Validates `reg`; raises
    //! `ResourcesException(InvalidRegister, "brz")` on
    //! failure.  Device-specific encoding via `impl_` (Hirzel
    //! diverts the `reg == R0` case to `JMP`; see `br`).
    AsmList::Asm brz(AsmRegister reg, const std::string& label, bool noOpt) const;
    //! \brief Emit `BRNZ` — branch if `reg != 0`
    //!        (opcode `0xF4000000`, 3-cycle branch penalty).
    //!
    //! \details Validates `reg`; raises
    //! `ResourcesException(InvalidRegister, "brnz")` on
    //! failure.  Encoded directly (not via `impl_`) — same
    //! encoding on both device families.
    AsmList::Asm brnz(AsmRegister reg, const std::string& label, bool noOpt) const;
    //! \brief Emit `BRGZ` — branch if `reg > 0`
    //!        (opcode `0xF5000000`, 3-cycle branch penalty).
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
    //!        `XNORR`, `XORR`).
    //!
    //! \details The `r` suffix on these mnemonics means
    //! "**r**egister operand" (3-register form, `dst = dst OP
    //! src`).  Opcodes share the `0x60000000` base with the
    //! low nibble selecting the operation: `ADDR=0x..00`,
    //! `SUBR=0x..01`, `ANDR=0x..02`, `ORR=0x..03`,
    //! `XNORR=0x..04`, `XORR=0x..07`.  Validates both `dst`
    //! and `src`; raises
    //! `ResourcesException(InvalidRegister,
    //! commandToString(cmd))` on failure.  The named
    //! convenience wrappers below dispatch through this
    //! function.
    AsmList::Asm alur(Assembler::Command cmd, AsmRegister dst, AsmRegister src) const;
    //! \brief `dst += src` — opcode `0x60000000`.
    //!        Convenience for `alur(ADDR, …)`.
    AsmList::Asm addr(AsmRegister dst, AsmRegister src) const;
    //! \brief `dst -= src` — opcode `0x60000001`.
    //!        Convenience for `alur(SUBR, …)`.
    AsmList::Asm subr(AsmRegister dst, AsmRegister src) const;
    //! \brief `dst &= src` — opcode `0x60000002`.
    //!        Convenience for `alur(ANDR, …)`.
    AsmList::Asm andr(AsmRegister dst, AsmRegister src) const;
    //! \brief `dst |= src` — opcode `0x60000003`.
    //!        Convenience for `alur(ORR, …)`.
    AsmList::Asm orr(AsmRegister dst, AsmRegister src) const;
    //! \brief `dst = ~(dst ^ src)` — opcode `0x60000004`.
    //!        Convenience for `alur(XNORR, …)`.
    AsmList::Asm xnorr(AsmRegister dst, AsmRegister src) const;

    // =====================================================================
    // Shift (delegates to impl_)
    // =====================================================================

    //! \brief Emit `SSL` — single-bit shift left of `reg`
    //!        (opcode `0x60000005`).
    //!
    //! \details No count operand: shift amount is fixed at
    //! 1.  Encoding differs by family — Cervino places `reg`
    //! in both the destination and source slots; Hirzel
    //! places `reg` in the destination and `R0` in the
    //! source.  Used by prefetch address arithmetic
    //! (`Prefetch::wvfRegImpl`, `prefetch_splitplay`,
    //! `prefetch_placesingle`) between `addi` and
    //! `wvf`/`wvfi`.  Delegates to `impl_`; raises
    //! `ResourcesException(InvalidRegister, "ssl")` on
    //! register validation failure.
    AsmList::Asm ssl(AsmRegister reg) const;
    //! \brief Emit `SSR` — single-bit shift right of `reg`
    //!        (opcode `0x60000006`).
    //!
    //! \details Same encoding-pattern split as `ssl` (Cervino
    //! reg-in-both-slots vs Hirzel reg/R0).  Delegates to
    //! `impl_`; raises
    //! `ResourcesException(InvalidRegister, "ssr")` on
    //! register validation failure.
    AsmList::Asm ssr(AsmRegister reg) const;

    // =====================================================================
    // ALU immediate, upper word (single instruction)
    // =====================================================================

    //! \brief Emit a generic immediate-upper-word ALU op
    //!        (`ADDIU`, `ANDIU`, `ORIU`, `XNORIU`).
    //!
    //! \details The `u` suffix means "**u**pper word", **not**
    //! "unsigned": the 12-bit immediate is implicitly shifted
    //! left by 12 before being applied.  These mnemonics are
    //! chained after the matching `i`-suffix low-bits form
    //! to encode wide constants — `addi` followed by `addiu`
    //! reconstructs a 24-bit immediate split across the
    //! low 12 bits and the upper word.  Opcode bases:
    //! `ADDIU=0x50000000`, `ANDIU=0x80000000`,
    //! `ORIU=0xA0000000`, `XNORIU=0xC0000000`.
    //!
    //! `imm` is stored as an `outputs` immediate (not the
    //! `immediates` slot).  Validates `dst`/`src`; raises
    //! `ResourcesException(InvalidRegister, commandToString(cmd))`
    //! on failure.
    AsmList::Asm aluiu(Assembler::Command cmd, AsmRegister dst,
                   AsmRegister src, Immediate imm) const;
    //! \brief `dst = src + (imm << 12)` — opcode
    //!        `0x50000000`.  Upper-word complement to
    //!        `addi`; chain after `addi` to encode wide
    //!        immediates.
    AsmList::Asm addiu(AsmRegister dst, AsmRegister src, Immediate imm) const;
    //! \brief `dst = src & (imm << 12)` — opcode
    //!        `0x80000000`.  Upper-word complement to
    //!        `andi`.
    AsmList::Asm andiu(AsmRegister dst, AsmRegister src, Immediate imm) const;
    //! \brief `dst = src | (imm << 12)` — opcode
    //!        `0xA0000000`.  Upper-word complement to
    //!        `ori`.
    AsmList::Asm oriu(AsmRegister dst, AsmRegister src, Immediate imm) const;
    //! \brief `dst = ~(src ^ (imm << 12))` — opcode
    //!        `0xC0000000`.  Upper-word complement to
    //!        `xnori`.
    AsmList::Asm xnoriu(AsmRegister dst, AsmRegister src, Immediate imm) const;

    // =====================================================================
    // ALU immediate-signed (may emit multiple instructions)
    // =====================================================================

    //! \brief Emit a signed-immediate ALU op
    //!        (`ADDI`/`ANDI`/`ORI`/`XNORI`), expanding to
    //!        multiple instructions when `imm` does not fit
    //!        the signed low-12-bit immediate field.
    //!
    //! \details The `i` suffix means "**i**mmediate operand"
    //! and addresses the low 12 bits of the constant; wide
    //! values are extended with the matching `iu` upper-word
    //! form.  Opcode bases: `ADDI=0x40000000`,
    //! `ANDI=0x70000000`, `ORI=0x90000000`,
    //! `XNORI=0xB0000000`.
    //!
    //! For an immediate in the `[-2^18, 2^18 - 1]` range,
    //! emits a single instruction with `imm` in the outputs
    //! slot.  Otherwise:
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

    //! \brief Emit `LD reg, addr` — load from memory-mapped
    //!        register `addr` into `reg`
    //!        (opcode `0xD0000000`).
    //!
    //! \details Validates `reg`; raises
    //! `ResourcesException(InvalidRegister, "ld")` on
    //! failure.  `addr.value` is stored in the outputs slot.
    //! See the special-register map for the meaning of
    //! individual addresses (DIO `0x20`, trigger `0x22`,
    //! ZSync `0x6A..0x6C`, PRNG `0x77`, …).
    AsmList::Asm ld(AsmRegister reg, detail::AddressImpl<unsigned int> addr) const;
    //! \brief Emit `ST reg, addr` — store `reg` to
    //!        memory-mapped register `addr`
    //!        (opcode `0xF6000000`).
    //!
    //! \details Validates `reg`; raises
    //! `ResourcesException(InvalidRegister, "st")` on
    //! failure.  `addr` carries an internal bounds check
    //! against `DeviceConstants::memoryDepth`.
    AsmList::Asm st(AsmRegister reg, detail::AddressImpl<unsigned int> addr) const;
    //! \brief Read the DIO bus into `reg`.
    //!
    //! \details Convenience wrapping `ld(reg, 0x20)` for the
    //! low bank (`highBank == false`) or `ld(reg, 0x1FE)`
    //! for the high bank (`highBank == true`).  The high
    //! bank is only meaningful on SHFLI / VHFLI / GHFLI;
    //! other devices alias both addresses or reject the high
    //! bank at the caller layer.  Raises
    //! `ResourcesException(InvalidRegister, "ldio")` on
    //! register validation failure.
    AsmList::Asm ldio(AsmRegister reg, bool highBank) const;
    //! \brief Store `reg` to the DIO bus.
    //!
    //! \details Convenience wrapping `st(reg, 0x20)` /
    //! `st(reg, 0x1FE)` selected by `highBank` (see
    //! `ldio`).  Raises
    //! `ResourcesException(InvalidRegister, "sdio")` on
    //! register validation failure.
    AsmList::Asm sdio(AsmRegister reg, bool highBank) const;
    //! \brief Load from the user-register address space.
    //!
    //! \details Same opcode as `ld`; the distinct mnemonic
    //! is preserved in the IR for diagnostic clarity but
    //! emits the same `LD` instruction.  Used for the
    //! 0x000–0x3FF user-register page (`getUserReg`,
    //! `getPRNGValue`, …).  Raises
    //! `ResourcesException(InvalidRegister, "luser")` on
    //! register validation failure.
    AsmList::Asm luser(AsmRegister reg, detail::AddressImpl<unsigned int> addr) const;
    //! \brief Store to the user-register address space.
    //!
    //! \details Same opcode as `st`; see `luser` for the
    //! aliasing rationale.  Used for the 0x000–0x3FF user
    //! page plus the named control registers in the 0x40+
    //! range (sync, oscillator phase, PRNG, sweep, QA, …).
    //! Raises `ResourcesException(InvalidRegister, "suser")`
    //! on register validation failure.
    AsmList::Asm suser(AsmRegister reg, detail::AddressImpl<unsigned int> addr) const;

    // =====================================================================
    // Trigger
    // =====================================================================

    //! \brief Read the trigger register into `reg` —
    //!        `ld(reg, 0x22)`.
    //!
    //! \details Used by `getTrigger` / `getAnaTrigger` /
    //! `getDigTrigger`.  Raises
    //! `ResourcesException(InvalidRegister, "ltrig")` on
    //! register validation failure.
    AsmList::Asm ltrig(AsmRegister reg) const;
    //! \brief Write `reg` to the trigger register —
    //!        `st(reg, 0x22)`.
    //!
    //! \details Pre-SHFLI devices only; used by
    //! `setTrigger`, `startQAResult`, `startQAMonitor`.
    //! Raises `ResourcesException(InvalidRegister, "strig")`
    //! on register validation failure.
    AsmList::Asm strig(AsmRegister reg) const;
    //! \brief Write `reg` to the internal-trigger register —
    //!        `st(reg, 0x23)`.
    //!
    //! \details LI-family only; used by `setInternalTrigger`.
    //! Raises `ResourcesException(InvalidRegister, "sinttrig")`
    //! on register validation failure.
    AsmList::Asm sinttrig(AsmRegister reg) const;
    //! \brief Emit `WTRIG r1, r2` — wait for trigger
    //!        condition (opcode `0xE0000000`).
    //!
    //! \details Three-register format: `r2` populates the
    //! source slot and `r1` the aux slot.  The trigger value
    //! is typically pre-loaded into a register via `addi`
    //! before this barrier.  Validates both; raises
    //! `ResourcesException(InvalidRegister, "wtrig")` on
    //! failure.
    AsmList::Asm wtrig(AsmRegister r1, AsmRegister r2) const;
    //! \brief Emit `WTRIGI value` — wait for trigger,
    //!        immediate operand (opcode `0xFD000000`).
    //!
    //! \details The `i` suffix denotes the immediate-operand
    //! variant; `value` occupies the low 5 bits of the
    //! instruction word.  No current frontend factory emits
    //! this — present for completeness with the assembler
    //! parser.
    AsmList::Asm wtrigi(int value) const;

    // =====================================================================
    // Misc I/O
    // =====================================================================

    //! \brief Write `reg` to the device-id register
    //!        (`st(reg, 0x21)` low bank or
    //!        `st(reg, 0x1FF)` high bank, selected by
    //!        `highBank`).
    //!
    //! \details Hirzel-only; high bank is reserved for
    //! SHFLI / VHFLI / GHFLI.  Raises
    //! `ResourcesException(InvalidRegister, "sid")` on
    //! register validation failure.
    AsmList::Asm sid(AsmRegister reg, bool highBank) const;
    //! \brief Emit a 3-instruction `smap` sequence —
    //!        command-table mapping write.
    //!
    //! \details Returns the concatenation of `alui(ADDI, r1,
    //! R0, value)` (which itself may be 1 or 2 instructions
    //! depending on the magnitude of `value`), followed by
    //! `st(r1, 0x62)` (map key) and `st(r2, 0x63)` (map
    //! value).  Validates `r1` and `r2`; raises
    //! `ResourcesException(InvalidRegister, "smap")` on
    //! failure.
    //!
    //! \binarynote Address `0x62` is overloaded: it is the
    //! `smap` map-key register on most devices, but UHFQA
    //! reuses the same address for
    //! `resetRTLoggerTimestamp()`.  The hardware
    //! disambiguates by device.
    std::vector<AsmList::Asm> smap(AsmRegister r1, AsmRegister r2, int value) const;
    //! \brief Emit `LDIOTRIG` — load I/O trigger value into
    //!        `reg`.
    //!
    //! \details Encoded as `LD reg, 0x60` on Cervino and
    //! `LD reg, 0x68` on Hirzel — the I/O-trigger register
    //! lives at different addresses in the two families.
    //! Used by `getDIOTriggered`, `getZSyncData(RAW)`, and
    //! `getFeedback(RAW)`.  Delegates to `impl_`.
    AsmList::Asm ldiotrig(AsmRegister reg) const;
    //! \brief Emit `LCNT reg, addr` — load HW loop counter
    //!        (`ld(reg, 0x64 + addr.value)`).
    //!
    //! \details Adds the loop-counter base `0x64` to
    //! `addr.value` before delegating to `ld`, placing the
    //! access into the loop-counter bank `0x64..0x65`
    //! (counter index 0 or 1).  Used by `getCnt(idx)`.
    //!
    //! \binarynote HDAWG-only.  No other device exposes
    //! hardware loop counters at this register window.
    //! Raises `ResourcesException(InvalidRegister, "lcnt")`
    //! on register validation failure.
    AsmList::Asm lcnt(AsmRegister reg, detail::AddressImpl<unsigned int> addr) const;

    // =====================================================================
    // Control flow / special
    // =====================================================================

    //! \brief Emit `TRAP` — debugger trap / breakpoint
    //!        (opcode `0xF7000000`, no operands).
    //!
    //! \details Emitted by the `play` validation paths in
    //! `custom_functions_play.cpp` to abort the sequencer on
    //! a runtime contract violation.
    AsmList::Asm trap() const;
    //! \brief Emit `IRPT` — raise interrupt
    //!        (opcode `0xF8000000`, no operands).
    //!
    //! \details No current frontend factory emits this;
    //! present for completeness with the assembler parser.
    AsmList::Asm irpt() const;
    //! \brief Emit `END` — program terminator
    //!        (opcode `0x00000000`, no operands).
    AsmList::Asm end() const;
    //! \brief Emit `NOP` — no-operation, one cycle
    //!        (opcode `0x00000001`, no operands).
    //!
    //! \details Used by the wait family and the program
    //! trailer in `compiler.cpp` for fixed-cycle padding.
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
    //! mask `0x400000`, and the user-register banks at
    //! addresses `0x44`/`0x45` so that every AWG core in a
    //! sync group has reached the barrier before any
    //! continues.  `flag == true` selects the
    //! 8-instruction primary path (uses `0x44`); `flag ==
    //! false` selects the 7-instruction shorter path (uses
    //! `0x45`).  The two scratch registers `reg1`/`reg2` are
    //! clobbered.
    //!
    //! \binarynote Cervino-only.  Hirzel devices use the
    //! single-instruction `asmSyncHirzel` because their sync
    //! protocol is implemented entirely in hardware once the
    //! user-register write at `0x6E` occurs.
    //!
    //! \param reg1  First scratch register.
    //! \param reg2  Second scratch register.
    //! \param flag  Selects between the two sync flavours
    //!              (see above).
    //! \return  Multi-instruction sequence as an `AsmList`.
    AsmList syncCervino(AsmRegister reg1, AsmRegister reg2, bool flag) const;
    //! \brief Emit the 2-instruction Cervino unsync
    //!        sequence — clears user registers `0x44` and
    //!        `0x45`.
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
    //!        synchronisation barrier — `suser(R0, 0x6E)`.
    //!
    //! \details Hirzel devices implement sync entirely in
    //! hardware once the user-register write at address
    //! `0x6E` occurs, so the multi-instruction Cervino
    //! sequence is unnecessary.
    //!
    //! \binarynote Hirzel-only counterpart of `syncCervino`.
    AsmList::Asm asmSyncHirzel() const;

    // =====================================================================
    // Pseudo-instructions / directives
    // =====================================================================

    //! \brief Zero `reg` via `ADDI reg, R0, 0` (opcode
    //!        `0x40000000`).
    AsmList::Asm asmZero(AsmRegister reg) const;
    //! \brief Set `reg` to `1` via `ADDI reg, R0, 1`
    //!        (opcode `0x40000000`).
    AsmList::Asm asmOne(AsmRegister reg) const;
    //! \brief Emit a `LABEL` directive carrying `label`
    //!        (pseudo-opcode `0x00000002`).
    //!
    //! \details Pure pseudo-instruction — `LABEL` is **not**
    //! present in the `cmdMap` parser table; it never
    //! produces a machine word.  The entry's
    //! `wavetableFront` is forced to `0` so labels do not
    //! inherit the surrounding per-waveform context — they
    //! are global to the `AsmList`.
    AsmList::Asm asmLabel(const std::string& label) const;
    //! \brief Emit a diagnostic `MESSAGE` (`isError ==
    //!        false`, opcode `0x00000003`) or `ERROR_MSG`
    //!        (`isError == true`, opcode `0x00000005`)
    //!        pseudo-instruction carrying a string payload.
    //!
    //! \details The message text is stored in the
    //! `immediates` slot as an `Immediate(string)`.
    //! `wavetableFront` is forced to `0` so the diagnostic
    //! is not bound to any per-waveform context.
    //!
    //! \binarynote These two opcodes are normally only
    //! produced by the standalone `AWGAssemblerImpl` parsing
    //! a hand-written `.asm` source — the SeqC compiler
    //! frontend does not emit them.  They are consumed by
    //! `AsmOptimize::reportUserMessages` and routed to the
    //! warning / error callbacks.
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

    //! \brief Emit a `WTRIG-LS` placeholder as
    //!        `ST R0, (value + 0x40)`.
    //!
    //! \details Encodes a wait-trigger as a store to address
    //! `0x40 + N`, where `N` is resolved at compile time
    //! from a resource constant such as
    //! `AWG_MAP_TRIGGER_INDEX`, `AWG_ZSYNC_TRIGGER_INDEX`,
    //! `AWG_DIG_TRIGGER{1,2}_INDEX`, etc.  Used by
    //! `waitDIOTrigger`, `waitZSyncTrigger`,
    //! `waitDigTrigger(idx)`, `waitCntTrigger(idx)`,
    //! `waitOnGrid`, and `waitSineOscPhase(osc)`.
    AsmList::Asm asmWtrigLSPlaceholder(int value);
    //! \brief Emit `FB value` — configure feedback
    //!        processing pipeline (opcode `0xFF000000`).
    //!
    //! \details `value` is the 23-bit packed immediate
    //! produced by `configureFeedbackProcessing(source,
    //! shift, numBits, threshold)` in `custom_functions_io.cpp`.
    //! Bit layout: `mode` (2 bits at 22:21, derived from
    //! `source`), `shift` (5 bits at 20:16), `numBits − 1`
    //! (4 bits at 15:12, encoded as `((numBits << 12) +
    //! 0xf000) & 0xffff`), and `threshold` (12 bits at
    //! 11:0).
    //!
    //! \binarynote Hirzel-only.  Only the processed feedback
    //! channels are valid sources — `ZSYNC_DATA_RAW` /
    //! `QA_DATA_RAW` are accepted by `getFeedback()` but
    //! rejected here.
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
