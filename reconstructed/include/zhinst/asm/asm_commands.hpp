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
    //!
    //! \param reg1    Hirzel-cache slot register; validated and rejected
    //!                as `InvalidRegister` on failure.
    //! \param reg2    Cervino-cache slot register; validated and rejected
    //!                as `InvalidRegister` on failure.
    //! \param intArg  20-bit length immediate encoded into the instruction
    //!                word.
    //! \return  The emitted assembler-instruction descriptor.
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
    //!
    //! \return  The emitted assembler-instruction descriptor.
    AsmList::Asm wprf() const;
    //! \brief Emit `WWVFQ` — **wait for waveform queue**
    //!        barrier (opcode alias of `WPRF` at
    //!        `0xF0000000`, no operands).
    //!
    //! \note Supported only on HDAWG/SHF (Hirzel) sequencers; UHF-family
    //! back ends throw `ResourcesException(UnsupportedOp, "wwvfq")`
    //! because they have no playback queue.  Emitted by
    //! `waitPlayQueueEmpty`.
    //!
    //! \return  The emitted assembler-instruction descriptor.
    AsmList::Asm wwvfq() const;
    //! \brief Emit `WWVF` — **wait for waveform** barrier
    //!        (opcode `0xF1000000`, no operands).
    //!
    //! \details Appended by `waitWave` and by the program
    //! trailer in `compiler.cpp` to drain in-flight waveform
    //! playback before program end.
    //!
    //! \return  The emitted assembler-instruction descriptor.
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
    //!
    //! \param reg     Marker-source register; validated and rejected
    //!                as `InvalidRegister` on failure.
    //! \param dstReg  Waveform-address register; on Hirzel, `R0` selects
    //!                the extended-opcode `WVFE` form.
    //! \param length  Length immediate encoded into the instruction word.
    //! \return  The emitted assembler-instruction descriptor.
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
    //!
    //! \param reg     Index register; validated and rejected as
    //!                `InvalidRegister` on failure.
    //! \param dstReg  Destination / address register.
    //! \param length  Length immediate encoded into the instruction word.
    //! \return  The emitted assembler-instruction descriptor.
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
    //! \note Supported only on HDAWG/SHF (Hirzel) sequencers; UHF-family
    //! back ends throw `ResourcesException(UnsupportedOp, "wvfs")`.
    //!
    //! \param type    `PlayDummyType` selector occupying a 1-bit slot;
    //!                must be `0` or `1`, otherwise `ValueOverflow` is
    //!                raised.
    //! \param reg     Length register; pass `R0` to mean "no register".
    //! \param length  20-bit offset immediate encoded into the
    //!                instruction word.
    //! \return  The emitted assembler-instruction descriptor.
    AsmList::Asm wvfs(Assembler::PlayDummyType type, AsmRegister reg, int length) const;
    //! \brief Emit `WVFT` — execute a waveform-table entry
    //!        (Hirzel extended-opcode `WVFET` = `0xFC000000`).
    //!
    //! \details Single-register form: `reg` carries the
    //! table-entry index, `length` the immediate.  Emitted
    //! by `playWaveDIO`, `playZSync`, and `executeTableEntry`
    //! to dispatch a precomposed table entry.
    //!
    //! \note Supported only on HDAWG/SHF (Hirzel) sequencers; UHF-family
    //! back ends throw `ResourcesException(UnsupportedOp, "wvft")`.
    //!
    //! \param reg     Table-entry index register.
    //! \param length  Length immediate encoded into the instruction word.
    //! \return  The emitted assembler-instruction descriptor.
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
    //!
    //! \param value  Packed PlayConfig word (24-bit immediate payload).
    //! \return  The emitted assembler-instruction descriptor.
    AsmList::Asm cwvf(int value) const;
    //! \brief Emit `CWVFR` — configure waveform from
    //!        register-supplied PlayConfig word
    //!        (opcode `0xF9000000`).
    //!
    //! \details Spillover form of `cwvf` for packed
    //! PlayConfig values that exceed the 24-bit immediate
    //! capacity.
    //!
    //! \note Available only when `impl_->isCWVFRSupported()`; the same
    //! flag also gates marker-bit computation in `genPlayConfig` and
    //! the device-restricted `wvfs` / `wvft` / `wwvfq` factories.
    //!
    //! \param reg  Register holding the packed PlayConfig word.
    //! \return  The emitted assembler-instruction descriptor.
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
    //! \return  The emitted assembler-instruction descriptor.
    AsmList::Asm br(const std::string& label, bool noOpt) const;
    //! \brief Emit `BRZ` — branch if `reg == 0`
    //!        (opcode `0xF3000000`, 3-cycle branch penalty).
    //!
    //! \details Validates `reg`; raises
    //! `ResourcesException(InvalidRegister, "brz")` on
    //! failure.  Device-specific encoding via `impl_` (Hirzel
    //! diverts the `reg == R0` case to `JMP`; see `br`).
    //!
    //! \param reg    Operand register tested against zero; validated and
    //!               rejected as `InvalidRegister` on failure.
    //! \param label  Target label name resolved at link time.
    //! \param noOpt  When `true`, marks the emitted instruction as
    //!               non-optimisable so later passes preserve it verbatim.
    //! \return  The emitted assembler-instruction descriptor.
    AsmList::Asm brz(AsmRegister reg, const std::string& label, bool noOpt) const;
    //! \brief Emit `BRNZ` — branch if `reg != 0`
    //!        (opcode `0xF4000000`, 3-cycle branch penalty).
    //!
    //! \details Validates `reg`; raises
    //! `ResourcesException(InvalidRegister, "brnz")` on
    //! failure.  Encoded directly (not via `impl_`) — same
    //! encoding on both device families.
    //!
    //! \param reg    Operand register tested against zero; validated and
    //!               rejected as `InvalidRegister` on failure.
    //! \param label  Target label name resolved at link time.
    //! \param noOpt  When `true`, marks the emitted instruction as
    //!               non-optimisable so later passes preserve it verbatim.
    //! \return  The emitted assembler-instruction descriptor.
    AsmList::Asm brnz(AsmRegister reg, const std::string& label, bool noOpt) const;
    //! \brief Emit `BRGZ` — branch if `reg > 0`
    //!        (opcode `0xF5000000`, 3-cycle branch penalty).
    //!
    //! \details Validates `reg`; raises
    //! `ResourcesException(InvalidRegister, "brgz")` on
    //! failure.  Encoded directly.
    //!
    //! \param reg    Operand register tested for positivity; validated
    //!               and rejected as `InvalidRegister` on failure.
    //! \param label  Target label name resolved at link time.
    //! \param noOpt  When `true`, marks the emitted instruction as
    //!               non-optimisable so later passes preserve it verbatim.
    //! \return  The emitted assembler-instruction descriptor.
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
    //!
    //! \param cmd  ALU operation selector (`ADDR`, `SUBR`, `ANDR`, `ORR`,
    //!             `XNORR`, `XORR`).
    //! \param dst  Destination register; also acts as the left operand.
    //! \param src  Source register supplying the right operand.
    //! \return  The emitted assembler-instruction descriptor.
    AsmList::Asm alur(Assembler::Command cmd, AsmRegister dst, AsmRegister src) const;
    //! \brief `dst += src` — opcode `0x60000000`.
    //!        Convenience for `alur(ADDR, …)`.
    //!
    //! \param dst  Destination register; also acts as the left operand.
    //! \param src  Source register supplying the right operand.
    //! \return  The emitted assembler-instruction descriptor.
    AsmList::Asm addr(AsmRegister dst, AsmRegister src) const;
    //! \brief `dst -= src` — opcode `0x60000001`.
    //!        Convenience for `alur(SUBR, …)`.
    //!
    //! \param dst  Destination register; also acts as the left operand.
    //! \param src  Source register supplying the right operand.
    //! \return  The emitted assembler-instruction descriptor.
    AsmList::Asm subr(AsmRegister dst, AsmRegister src) const;
    //! \brief `dst &= src` — opcode `0x60000002`.
    //!        Convenience for `alur(ANDR, …)`.
    //!
    //! \param dst  Destination register; also acts as the left operand.
    //! \param src  Source register supplying the right operand.
    //! \return  The emitted assembler-instruction descriptor.
    AsmList::Asm andr(AsmRegister dst, AsmRegister src) const;
    //! \brief `dst |= src` — opcode `0x60000003`.
    //!        Convenience for `alur(ORR, …)`.
    //!
    //! \param dst  Destination register; also acts as the left operand.
    //! \param src  Source register supplying the right operand.
    //! \return  The emitted assembler-instruction descriptor.
    AsmList::Asm orr(AsmRegister dst, AsmRegister src) const;
    //! \brief `dst = ~(dst ^ src)` — opcode `0x60000004`.
    //!        Convenience for `alur(XNORR, …)`.
    //!
    //! \param dst  Destination register; also acts as the left operand.
    //! \param src  Source register supplying the right operand.
    //! \return  The emitted assembler-instruction descriptor.
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
    //!
    //! \param reg  Register shifted in place; validated and rejected as
    //!             `InvalidRegister` on failure.
    //! \return  The emitted assembler-instruction descriptor.
    AsmList::Asm ssl(AsmRegister reg) const;
    //! \brief Emit `SSR` — single-bit shift right of `reg`
    //!        (opcode `0x60000006`).
    //!
    //! \details Same encoding-pattern split as `ssl` (Cervino
    //! reg-in-both-slots vs Hirzel reg/R0).  Delegates to
    //! `impl_`; raises
    //! `ResourcesException(InvalidRegister, "ssr")` on
    //! register validation failure.
    //!
    //! \param reg  Register shifted in place; validated and rejected as
    //!             `InvalidRegister` on failure.
    //! \return  The emitted assembler-instruction descriptor.
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
    //!
    //! \param cmd  ALU operation selector (`ADDIU`, `ANDIU`, `ORIU`,
    //!             `XNORIU`).
    //! \param dst  Destination register.
    //! \param src  Source register supplying the left operand.
    //! \param imm  12-bit immediate, implicitly shifted left by 12 before
    //!             being applied.
    //! \return  The emitted assembler-instruction descriptor.
    AsmList::Asm aluiu(Assembler::Command cmd, AsmRegister dst,
                   AsmRegister src, Immediate imm) const;
    //! \brief `dst = src + (imm << 12)` — opcode
    //!        `0x50000000`.  Upper-word complement to
    //!        `addi`; chain after `addi` to encode wide
    //!        immediates.
    //!
    //! \param dst  Destination register.
    //! \param src  Source register supplying the left operand.
    //! \param imm  12-bit immediate, implicitly shifted left by 12.
    //! \return  The emitted assembler-instruction descriptor.
    AsmList::Asm addiu(AsmRegister dst, AsmRegister src, Immediate imm) const;
    //! \brief `dst = src & (imm << 12)` — opcode
    //!        `0x80000000`.  Upper-word complement to
    //!        `andi`.
    //!
    //! \param dst  Destination register.
    //! \param src  Source register supplying the left operand.
    //! \param imm  12-bit immediate, implicitly shifted left by 12.
    //! \return  The emitted assembler-instruction descriptor.
    AsmList::Asm andiu(AsmRegister dst, AsmRegister src, Immediate imm) const;
    //! \brief `dst = src | (imm << 12)` — opcode
    //!        `0xA0000000`.  Upper-word complement to
    //!        `ori`.
    //!
    //! \param dst  Destination register.
    //! \param src  Source register supplying the left operand.
    //! \param imm  12-bit immediate, implicitly shifted left by 12.
    //! \return  The emitted assembler-instruction descriptor.
    AsmList::Asm oriu(AsmRegister dst, AsmRegister src, Immediate imm) const;
    //! \brief `dst = ~(src ^ (imm << 12))` — opcode
    //!        `0xC0000000`.  Upper-word complement to
    //!        `xnori`.
    //!
    //! \param dst  Destination register.
    //! \param src  Source register supplying the left operand.
    //! \param imm  12-bit immediate, implicitly shifted left by 12.
    //! \return  The emitted assembler-instruction descriptor.
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
    //!
    //! \param cmd  ALU operation selector (`ADDI`, `ANDI`, `ORI`,
    //!             `XNORI`); any other command raises
    //!             `ResourcesException(ErrorMessageT(0xD8), …)`.
    //! \param dst  Destination register.
    //! \param src  Source register supplying the left operand.
    //! \param imm  Signed immediate; values outside the 19-bit signed
    //!             range trigger the multi-instruction expansion.
    //! \return  Sequence of one or more assembler-instruction descriptors.
    std::vector<AsmList::Asm> alui(Assembler::Command cmd, AsmRegister dst,
                               AsmRegister src, Immediate imm) const;
    //! \brief `dst = src + imm`. May expand to two
    //!        instructions when `imm` exceeds the 19-bit
    //!        signed range — see `alui`.
    //!
    //! \param dst  Destination register.
    //! \param src  Source register supplying the left operand.
    //! \param imm  Signed immediate; wide values trigger expansion.
    //! \return  Sequence of one or two assembler-instruction descriptors.
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
    //!
    //! \param dst  Destination register.
    //! \param src  Source register supplying the left operand.
    //! \param imm  Full 32-bit signed immediate, split across the
    //!             low-12 and upper-word halves.
    //! \return  Two-element `ADDI`/`ADDIU` sequence flagged as
    //!          non-optimisable.
    std::vector<AsmList::Asm> addi32(AsmRegister dst, AsmRegister src, Immediate imm) const;
    //! \brief `dst = src & imm`. Multi-instruction expansion
    //!        for large `imm` — see `alui`.
    //!
    //! \param dst  Destination register.
    //! \param src  Source register supplying the left operand.
    //! \param imm  Signed immediate; wide values trigger expansion.
    //! \return  Sequence of one or more assembler-instruction descriptors.
    std::vector<AsmList::Asm> andi(AsmRegister dst, AsmRegister src, Immediate imm) const;
    //! \brief `dst = src | imm`. Multi-instruction expansion
    //!        for large `imm` — see `alui`.
    //!
    //! \param dst  Destination register.
    //! \param src  Source register supplying the left operand.
    //! \param imm  Signed immediate; wide values trigger expansion.
    //! \return  Sequence of one or more assembler-instruction descriptors.
    std::vector<AsmList::Asm> ori(AsmRegister dst, AsmRegister src, Immediate imm) const;
    //! \brief `dst = ~(src ^ imm)`. Multi-instruction
    //!        expansion for large `imm` — see `alui`.
    //!
    //! \param dst  Destination register.
    //! \param src  Source register supplying the left operand.
    //! \param imm  Signed immediate; wide values trigger expansion.
    //! \return  Sequence of one or more assembler-instruction descriptors.
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
    //!
    //! \param dst  Destination register.
    //! \param src  Source register supplying the left operand.
    //! \param val  Source value clamped through `toInt32` then forwarded
    //!             to the integer overload.
    //! \return  Sequence of one or more assembler-instruction descriptors.
    std::vector<AsmList::Asm> addi(AsmRegister dst, AsmRegister src, Value val) const;
    //! \brief `Value`-overload of `addiu`; converts `val`
    //!        through `toInt32` then dispatches.
    //!
    //! \param dst  Destination register.
    //! \param src  Source register supplying the left operand.
    //! \param val  Source value clamped through `toInt32` then forwarded
    //!             to the integer overload.
    //! \return  The emitted assembler-instruction descriptor.
    AsmList::Asm addiu(AsmRegister dst, AsmRegister src, Value val) const;
    //! \brief `Value`-overload of `andi`; converts `val`
    //!        through `toInt32` then dispatches.
    //!
    //! \param dst  Destination register.
    //! \param src  Source register supplying the left operand.
    //! \param val  Source value clamped through `toInt32` then forwarded
    //!             to the integer overload.
    //! \return  Sequence of one or more assembler-instruction descriptors.
    std::vector<AsmList::Asm> andi(AsmRegister dst, AsmRegister src, Value val) const;
    //! \brief `Value`-overload of `andiu`; converts `val`
    //!        through `toInt32` then dispatches.
    //!
    //! \param dst  Destination register.
    //! \param src  Source register supplying the left operand.
    //! \param val  Source value clamped through `toInt32` then forwarded
    //!             to the integer overload.
    //! \return  The emitted assembler-instruction descriptor.
    AsmList::Asm andiu(AsmRegister dst, AsmRegister src, Value val) const;
    //! \brief `Value`-overload of `ori`; converts `val`
    //!        through `toInt32` then dispatches.
    //!
    //! \param dst  Destination register.
    //! \param src  Source register supplying the left operand.
    //! \param val  Source value clamped through `toInt32` then forwarded
    //!             to the integer overload.
    //! \return  Sequence of one or more assembler-instruction descriptors.
    std::vector<AsmList::Asm> ori(AsmRegister dst, AsmRegister src, Value val) const;
    //! \brief `Value`-overload of `oriu`; converts `val`
    //!        through `toInt32` then dispatches.
    //!
    //! \param dst  Destination register.
    //! \param src  Source register supplying the left operand.
    //! \param val  Source value clamped through `toInt32` then forwarded
    //!             to the integer overload.
    //! \return  The emitted assembler-instruction descriptor.
    AsmList::Asm oriu(AsmRegister dst, AsmRegister src, Value val) const;
    //! \brief `Value`-overload of `xnori`; converts `val`
    //!        through `toInt32` then dispatches.
    //!
    //! \param dst  Destination register.
    //! \param src  Source register supplying the left operand.
    //! \param val  Source value clamped through `toInt32` then forwarded
    //!             to the integer overload.
    //! \return  Sequence of one or more assembler-instruction descriptors.
    std::vector<AsmList::Asm> xnori(AsmRegister dst, AsmRegister src, Value val) const;
    //! \brief `Value`-overload of `xnoriu`; converts `val`
    //!        through `toInt32` then dispatches.
    //!
    //! \param dst  Destination register.
    //! \param src  Source register supplying the left operand.
    //! \param val  Source value clamped through `toInt32` then forwarded
    //!             to the integer overload.
    //! \return  The emitted assembler-instruction descriptor.
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
    //!
    //! \param reg   Destination register; validated and rejected as
    //!              `InvalidRegister` on failure.
    //! \param addr  Source memory-mapped register address; encoded into
    //!              the outputs slot.
    //! \return  The emitted assembler-instruction descriptor.
    AsmList::Asm ld(AsmRegister reg, detail::AddressImpl<unsigned int> addr) const;
    //! \brief Emit `ST reg, addr` — store `reg` to
    //!        memory-mapped register `addr`
    //!        (opcode `0xF6000000`).
    //!
    //! \details Validates `reg`; raises
    //! `ResourcesException(InvalidRegister, "st")` on
    //! failure.  `addr` carries an internal bounds check
    //! against `DeviceConstants::memoryDepth`.
    //!
    //! \param reg   Source register; validated and rejected as
    //!              `InvalidRegister` on failure.
    //! \param addr  Destination memory-mapped register address; bounds-
    //!              checked against `DeviceConstants::memoryDepth`.
    //! \return  The emitted assembler-instruction descriptor.
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
    //!
    //! \param reg       Destination register; validated and rejected as
    //!                  `InvalidRegister` on failure.
    //! \param highBank  Selects the high (`0x1FE`) vs low (`0x20`) DIO
    //!                  bank.
    //! \return  The emitted assembler-instruction descriptor.
    AsmList::Asm ldio(AsmRegister reg, bool highBank) const;
    //! \brief Store `reg` to the DIO bus.
    //!
    //! \details Convenience wrapping `st(reg, 0x20)` /
    //! `st(reg, 0x1FE)` selected by `highBank` (see
    //! `ldio`).  Raises
    //! `ResourcesException(InvalidRegister, "sdio")` on
    //! register validation failure.
    //!
    //! \param reg       Source register; validated and rejected as
    //!                  `InvalidRegister` on failure.
    //! \param highBank  Selects the high (`0x1FE`) vs low (`0x20`) DIO
    //!                  bank.
    //! \return  The emitted assembler-instruction descriptor.
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
    //!
    //! \param reg   Destination register; validated and rejected as
    //!              `InvalidRegister` on failure.
    //! \param addr  Source user-register address.
    //! \return  The emitted assembler-instruction descriptor.
    AsmList::Asm luser(AsmRegister reg, detail::AddressImpl<unsigned int> addr) const;
    //! \brief Store to the user-register address space.
    //!
    //! \details Same opcode as `st`; see `luser` for the
    //! aliasing rationale.  Used for the 0x000–0x3FF user
    //! page plus the named control registers in the 0x40+
    //! range (sync, oscillator phase, PRNG, sweep, QA, …).
    //! Raises `ResourcesException(InvalidRegister, "suser")`
    //! on register validation failure.
    //!
    //! \param reg   Source register; validated and rejected as
    //!              `InvalidRegister` on failure.
    //! \param addr  Destination user-register address.
    //! \return  The emitted assembler-instruction descriptor.
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
    //!
    //! \param reg  Destination register; validated and rejected as
    //!             `InvalidRegister` on failure.
    //! \return  The emitted assembler-instruction descriptor.
    AsmList::Asm ltrig(AsmRegister reg) const;
    //! \brief Write `reg` to the trigger register —
    //!        `st(reg, 0x22)`.
    //!
    //! \details Pre-SHFLI devices only; used by
    //! `setTrigger`, `startQAResult`, `startQAMonitor`.
    //! Raises `ResourcesException(InvalidRegister, "strig")`
    //! on register validation failure.
    //!
    //! \param reg  Source register; validated and rejected as
    //!             `InvalidRegister` on failure.
    //! \return  The emitted assembler-instruction descriptor.
    AsmList::Asm strig(AsmRegister reg) const;
    //! \brief Write `reg` to the internal-trigger register —
    //!        `st(reg, 0x23)`.
    //!
    //! \details LI-family only; used by `setInternalTrigger`.
    //! Raises `ResourcesException(InvalidRegister, "sinttrig")`
    //! on register validation failure.
    //!
    //! \param reg  Source register; validated and rejected as
    //!             `InvalidRegister` on failure.
    //! \return  The emitted assembler-instruction descriptor.
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
    //!
    //! \param r1  Aux-slot register; validated and rejected as
    //!            `InvalidRegister` on failure.
    //! \param r2  Source-slot register; validated and rejected as
    //!            `InvalidRegister` on failure.
    //! \return  The emitted assembler-instruction descriptor.
    AsmList::Asm wtrig(AsmRegister r1, AsmRegister r2) const;
    //! \brief Emit `WTRIGI value` — wait for trigger,
    //!        immediate operand (opcode `0xFD000000`).
    //!
    //! \details The `i` suffix denotes the immediate-operand
    //! variant; `value` occupies the low 5 bits of the
    //! instruction word.  No current frontend factory emits
    //! this — present for completeness with the assembler
    //! parser.
    //!
    //! \param value  5-bit immediate payload encoded into the instruction
    //!               word.
    //! \return  The emitted assembler-instruction descriptor.
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
    //!
    //! \param reg       Source register; validated and rejected as
    //!                  `InvalidRegister` on failure.
    //! \param highBank  Selects the high (`0x1FF`) vs low (`0x21`)
    //!                  device-id register.
    //! \return  The emitted assembler-instruction descriptor.
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
    //! \note Address `0x62` is overloaded: it is the
    //! `smap` map-key register on most devices, but UHFQA
    //! reuses the same address for
    //! `resetRTLoggerTimestamp()`.  The hardware
    //! disambiguates by device.
    //!
    //! \param r1     Scratch register loaded with `value` and stored to
    //!               the map-key address.
    //! \param r2     Source register stored to the map-value address.
    //! \param value  Map-key immediate loaded into `r1`.
    //! \return  Sequence of two or three assembler-instruction
    //!          descriptors.
    std::vector<AsmList::Asm> smap(AsmRegister r1, AsmRegister r2, int value) const;
    //! \brief Emit `LDIOTRIG` — load I/O trigger value into
    //!        `reg`.
    //!
    //! \details Encoded as `LD reg, 0x60` on Cervino and
    //! `LD reg, 0x68` on Hirzel — the I/O-trigger register
    //! lives at different addresses in the two families.
    //! Used by `getDIOTriggered`, `getZSyncData(RAW)`, and
    //! `getFeedback(RAW)`.  Delegates to `impl_`.
    //!
    //! \param reg  Destination register receiving the I/O trigger value.
    //! \return  The emitted assembler-instruction descriptor.
    AsmList::Asm ldiotrig(AsmRegister reg) const;
    //! \brief Emit `LCNT reg, addr` — load HW loop counter
    //!        (`ld(reg, 0x64 + addr.value)`).
    //!
    //! \details Adds the loop-counter base `0x64` to
    //! `addr.value` before delegating to `ld`, placing the
    //! access into the loop-counter bank `0x64..0x65`
    //! (counter index 0 or 1).  Used by `getCnt(idx)`.
    //!
    //! \note HDAWG-only; no other device exposes hardware loop counters
    //! at this register window.  Raises
    //! `ResourcesException(InvalidRegister, "lcnt")` on register
    //! validation failure.
    //!
    //! \param reg   Destination register receiving the counter value.
    //! \param addr  Counter index (0 or 1) added to the base `0x64`.
    //! \return  The emitted assembler-instruction descriptor.
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
    //!
    //! \return  The emitted assembler-instruction descriptor.
    AsmList::Asm trap() const;
    //! \brief Emit `IRPT` — raise interrupt
    //!        (opcode `0xF8000000`, no operands).
    //!
    //! \details No current frontend factory emits this;
    //! present for completeness with the assembler parser.
    //!
    //! \return  The emitted assembler-instruction descriptor.
    AsmList::Asm irpt() const;
    //! \brief Emit `END` — program terminator
    //!        (opcode `0x00000000`, no operands).
    //!
    //! \return  The emitted assembler-instruction descriptor.
    AsmList::Asm end() const;
    //! \brief Emit `NOP` — no-operation, one cycle
    //!        (opcode `0x00000001`, no operands).
    //!
    //! \details Used by the wait family and the program
    //! trailer in `compiler.cpp` for fixed-cycle padding.
    //!
    //! \return  The emitted assembler-instruction descriptor.
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
    //! \note UHF-family devices only; HDAWG/SHF sequencers use the
    //! single-instruction `asmSyncHirzel` because their sync protocol
    //! is implemented entirely in hardware once the user-register
    //! write at `0x6E` occurs.
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
    //!
    //! \return  The emitted assembler-instruction descriptor.
    AsmList::Asm asmSyncPlaceholderCervino() const;
    //! \brief Emit the single-instruction Hirzel-family
    //!        synchronisation barrier — `suser(R0, 0x6E)`.
    //!
    //! \details Hirzel devices implement sync entirely in
    //! hardware once the user-register write at address
    //! `0x6E` occurs, so the multi-instruction Cervino
    //! sequence is unnecessary.
    //!
    //! \return  The emitted assembler-instruction descriptor.
    AsmList::Asm asmSyncHirzel() const;

    // =====================================================================
    // Pseudo-instructions / directives
    // =====================================================================

    //! \brief Zero `reg` via `ADDI reg, R0, 0` (opcode
    //!        `0x40000000`).
    //!
    //! \param reg  Destination register set to zero.
    //! \return  The emitted assembler-instruction descriptor.
    AsmList::Asm asmZero(AsmRegister reg) const;
    //! \brief Set `reg` to `1` via `ADDI reg, R0, 1`
    //!        (opcode `0x40000000`).
    //!
    //! \param reg  Destination register set to one.
    //! \return  The emitted assembler-instruction descriptor.
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
    //!
    //! \param label  Label name carried by the directive.
    //! \return  The emitted assembler-instruction descriptor.
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
    // Provenance: these two opcodes are normally produced only by the
    // standalone AWGAssemblerImpl parsing a hand-written .asm source —
    // the SeqC compiler frontend does not emit them.  They are consumed
    // by AsmOptimize::reportUserMessages and routed to the warning /
    // error callbacks.
    //!
    //! \param msg      Diagnostic text payload stored in the immediates
    //!                 slot.
    //! \param isError  Selects `ERROR_MSG` (`true`) or `MESSAGE`
    //!                 (`false`).
    //! \return  The emitted assembler-instruction descriptor.
    AsmList::Asm asmMessage(const std::string& msg, bool isError) const;

    // =====================================================================
    // Node-creating commands
    // =====================================================================

    //! \brief Emit a `Branch` IR node entry — placeholder for
    //!        a branch target the IR pass will resolve.
    //!
    //! \return  The emitted assembler-instruction descriptor.
    AsmList::Asm asmBranchNode() const;
    //! \brief Emit a `Loop` IR node entry — placeholder for
    //!        loop boundary metadata used by later passes.
    //!
    //! \return  The emitted assembler-instruction descriptor.
    AsmList::Asm asmLoopNode() const;
    //! \brief Emit a `Rate` IR node carrying the global
    //!        sample-rate `rate` (written into
    //!        `Node::globalRate` at `+0x100`).
    //!
    //! \param rate  Global sample-rate written into `Node::globalRate`.
    //! \return  The emitted assembler-instruction descriptor.
    AsmList::Asm asmRate(int rate) const;
    //! \brief Emit a `SetPrecomp` IR node carrying the
    //!        default pre-compensation `flags` bitmask
    //!        (written into `Node::defaultPrecompFlags`
    //!        at `+0x104`).
    //!
    //! \param flags  Default pre-compensation flags bitmask written into
    //!               `Node::defaultPrecompFlags`.
    //! \return  The emitted assembler-instruction descriptor.
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
    //!
    //! \param reg  Length register stored into `Node::lengthReg`.
    //! \return  The emitted assembler-instruction descriptor.
    AsmList::Asm asmSetVarPlaceholder(AsmRegister reg);
    //! \brief Emit a `LockPlaceholder` node binding waveform
    //!        `wvf` to slot `index` for later locking.
    //!
    //! \details Stores `wvf->name` into
    //! `node->wavesPerDev[index]` and sets
    //! `node->deviceIndex = index`.  The lock pass uses
    //! these to drive `LD`/`ST` accesses to the per-device
    //! waveform memory.
    //!
    //! \param wvf    Waveform whose name is recorded in
    //!               `node->wavesPerDev[index]`.
    //! \param index  Per-device slot index also stored as
    //!               `node->deviceIndex`.
    //! \return  The emitted assembler-instruction descriptor.
    AsmList::Asm asmLockPlaceholder(std::shared_ptr<WaveformFront> wvf, int index);
    //! \brief Emit an `UnlockPlaceholder` node releasing the
    //!        slot bound by an earlier `asmLockPlaceholder`.
    //!
    //! \details Sets `node->deviceIndex = index`.  `wvf` is
    //! retained for symmetry with `asmLockPlaceholder` but
    //! the unlock node only needs the index.
    //!
    //! \param wvf    Waveform retained for symmetry with
    //!               `asmLockPlaceholder`; not stored on the node.
    //! \param index  Per-device slot index stored as
    //!               `node->deviceIndex`.
    //! \return  The emitted assembler-instruction descriptor.
    AsmList::Asm asmUnlockPlaceholder(std::shared_ptr<WaveformFront> wvf, int index);
    //! \brief Emit a `Load` placeholder — a pass-through
    //!        marker the prefetch pass replaces with a
    //!        concrete load sequence.
    //!
    //! \return  The emitted assembler-instruction descriptor.
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
    //! \return  The emitted assembler-instruction descriptor.
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
    //!
    //! \param wvf          Source waveform; when null, `dummy = true`
    //!                     and channel/marker fields are zeroed.
    //! \param isHold       When `true`, single-channel waveforms map to
    //!                     `channelMask = 0b10`.
    //! \param fourChannel  Unused inside `genPlayConfig`; consumed by
    //!                     the surrounding `asmPlay` call.
    //! \param playNow      Populates `PlayConfig::now`.
    //! \param hold         Populates `PlayConfig::hold`.
    //! \param rate         Populates `PlayConfig::rate`.
    //! \param suppress     Populates `PlayConfig::suppress`.
    //! \param is4Channel   Populates `PlayConfig::is4Channel`.
    //! \param trigger      Populates `PlayConfig::trigger`.
    //! \return  Fully populated `PlayConfig` record.
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
    //!
    //! \param waveforms    Per-device waveform list; an empty list selects
    //!                     the dummy-play path.
    //! \param deviceIndex  Active device-slot index; `-1` for dummy play.
    //! \param isHold       Hold-mode flag forwarded to `genPlayConfig`.
    //! \param fourChannel  Four-channel flag forwarded to `genPlayConfig`.
    //! \param hold         Populates `PlayConfig::hold`.
    //! \param rate         Populates `PlayConfig::rate`.
    //! \param suppress     Populates `PlayConfig::suppress`.
    //! \param is4Channel   Populates `PlayConfig::is4Channel`.
    //! \param lengthReg    Length register stored on the node.
    //! \param length       Explicit length stored on the node.
    //! \param reg2         Auxiliary register stored on the node.
    //! \param trigger      Populates `PlayConfig::trigger`.
    //! \return  The emitted assembler-instruction descriptor.
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
    //!
    //! \param tableIndex   Wavetable entry index recorded on the node.
    //! \param wvf          Source waveform; marked `used` when non-null.
    //! \param deviceIndex  Active device-slot index recorded on the node.
    //! \param isHold       Hold-mode flag forwarded to `genPlayConfig`.
    //! \param fourChannel  Four-channel flag forwarded to `genPlayConfig`
    //!                     (also pinned as the `playNow` argument).
    //! \param rate         Populates `PlayConfig::rate`.
    //! \param suppress     Populates `PlayConfig::suppress`.
    //! \param is4Channel   Populates `PlayConfig::is4Channel` (also pinned
    //!                     as the `hold` argument).
    //! \param lengthReg    Length register stored on the node.
    //! \param length       Explicit length stored on the node.
    //! \return  The emitted assembler-instruction descriptor.
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
    //!
    //! \param value  Resource-constant offset added to the base `0x40`
    //!               to form the destination address.
    //! \return  The emitted assembler-instruction descriptor.
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
    //! \note HDAWG/SHF only.  Only processed feedback channels are
    //! valid sources here: `ZSYNC_DATA_RAW` and `QA_DATA_RAW` are
    //! accepted by `getFeedback()` but rejected by this factory.
    //!
    //! \param value  23-bit packed immediate produced by
    //!               `configureFeedbackProcessing`.
    //! \return  The emitted assembler-instruction descriptor.
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
    //!
    //! \param value  New `wavetableFront` index propagated into every
    //!               subsequently emitted entry.
    void setWavetableFrontIndex(int value) { wavetableFrontIndex_ = value; }
    //! \brief Read the current `wavetableFront` index — see
    //!        `setWavetableFrontIndex`.
    //!
    //! \return  The current `wavetableFront` index.
    int  wavetableFrontIndex() const { return wavetableFrontIndex_; }

private:
    //! \brief Pimpl pointer holding the queued instruction list and
    //! the per-channel-group accounting state.
    std::unique_ptr<AsmCommandsImpl> impl_;                      // +0x10
    //! \brief Wavetable currently active for newly-emitted entries;
    //! used to stamp wavetable references on emitted assemblers.
    std::shared_ptr<WavetableFront> wavetable_;                  // ~+0x20
    //! \brief Callback invoked with the error message when an
    //! emitter helper detects a misuse (out-of-range index,
    //! incompatible operand, etc.).
    std::function<void(const std::string&)> errorHandler_;       // ~+0x40
    //! \brief Index of the wavetable-front entry stamped onto every
    //! subsequent `AsmList::Asm`; updated via
    //! `setWavetableFrontIndex`.
    int wavetableFrontIndex_ = 0;                                // +0x50
    //! \brief Number of channel groups the surrounding compilation
    //! targets; used by waveform-emitting helpers to size per-group
    //! payloads.
    int numChannelGroups_ = 0;                                   // +0x54

    // Helper: build an AsmList::Asm from a local Assembler
    //! \brief Wraps `instr` in an `AsmList::Asm`, stamping the
    //! current `wavetableFrontIndex_` and the active source-line
    //! annotation.
    //! \param instr Concrete assembler instruction to wrap.
    //! \return Newly-built `AsmList::Asm` entry.
    AsmList::Asm emitEntry(const Assembler& instr) const;
    //! \brief Like `emitEntry(instr)` but stamps
    //! `overrideWavetableFront` instead of the current
    //! `wavetableFrontIndex_` — used by helpers that emit
    //! cross-wavetable references.
    //! \param instr                   Concrete assembler
    //!                                instruction to wrap.
    //! \param overrideWavetableFront  Wavetable-front index forced
    //!                                onto the entry.
    //! \return Newly-built `AsmList::Asm` entry.
    AsmList::Asm emitEntry(const Assembler& instr, int overrideWavetableFront) const;

    // Helper: build an AsmList::Asm with a Node
    //! \brief Builds an `AsmList::Asm` whose payload is a fresh IR
    //! `Node` of the given `type` (used by emitters that attach a
    //! lowered-IR node to the assembler stream).
    //! \param type Node-type discriminator for the new IR node.
    //! \return Newly-built `AsmList::Asm` carrying the `Node`.
    AsmList::Asm emitNodeEntry(NodeType type) const;
};

} // namespace zhinst
