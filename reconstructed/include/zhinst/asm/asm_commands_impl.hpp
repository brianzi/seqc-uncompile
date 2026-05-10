// ============================================================================
// Reconstructed from disassembly of _seqc_compiler.so
// AsmCommandsImpl — virtual base + Cervino/Hirzel declarations
// ============================================================================
#pragma once

#include <memory>
#include <string>

#include "zhinst/asm/asm_list.hpp"
#include "zhinst/asm/asm_register.hpp"
#include "zhinst/asm/assembler.hpp"
#include "zhinst/core/types.hpp"

namespace zhinst {

// Abstract base class for device-specific instruction encoding.
// Used as a pimpl inside AsmCommands.
//
// Vtable layout (13 slots):
//   [0]  ~AsmCommandsImpl() (deleting dtor)
//   [1]  ~AsmCommandsImpl() (complete dtor)
//   [2]  isCWVFRSupported() const -> bool
//   [3]  wwvfq(int) const
//   [4]  wprf(int) const
//   [5]  wvf(AsmRegister, AsmRegister, int, int) const
//   [6]  wvfi(AsmRegister, AsmRegister, int, int) const
//   [7]  wvfs(PlayDummyType, AsmRegister, int, int) const
//   [8]  wvft(AsmRegister, int, int) const
//   [9]  brz(AsmRegister, const string&, bool, int) const
//   [10] ssl(AsmRegister, int) const
//   [11] ssr(AsmRegister, int) const
//   [12] ldiotrig(AsmRegister, int) const
//! \brief Device-specific instruction-encoding back end for `AsmCommands`.
//!
//! `AsmCommandsImpl` is the polymorphic pimpl behind `AsmCommands`:
//! each AWG hardware family (Cervino, Hirzel) implements the abstract
//! virtuals here with the opcode bit-patterns and operand layouts its
//! processor expects.  The `getInstance(deviceType)` factory selects
//! the right concrete subclass based on `AwgDeviceType`.
//!
//! Methods that have no encoding on a given family (for example
//! Cervino lacks `wwvfq`) throw an exception when called on that
//! subclass.  `isCWVFRSupported()` is a per-family capability flag
//! consulted by `AsmCommands` to gate optional code-generation paths
//! such as the marker-bit computation in `genPlayConfig`.
class AsmCommandsImpl {
public:
    //! \brief Virtual destructor so deleting through a base pointer
    //!        invokes the derived destructor.
    virtual ~AsmCommandsImpl();

    //! \brief Capability flag indicating whether the back end
    //!        supports the CWVFR (compact-waveform) instruction
    //!        family.
    //! \details Consulted by `AsmCommands` to gate code-generation
    //! paths such as the marker-bit computation in
    //! `genPlayConfig`.  Returns `true` for the Hirzel back end
    //! and `false` for Cervino.
    //! \return `true` when CWVFR encodings are available.
    virtual bool isCWVFRSupported() const = 0;

    //! \brief Emit the `wwvfq` (write-wave waveform-queue) opcode.
    //! \param lineNumber  Source line tagged on the emitted entry
    //! for diagnostics.
    //! \return The encoded assembler entry.
    virtual AsmList::Asm wwvfq(int lineNumber) const = 0;
    //! \brief Emit the `wprf` (write-prefetch) opcode used by the
    //!        prefetcher to bind a waveform slot to a destination
    //!        register.
    //! \param lineNumber  Source line tagged on the emitted entry.
    //! \return The encoded assembler entry.
    virtual AsmList::Asm wprf(int lineNumber) const = 0;

    //! \brief Emit the standard `wvf` (write-waveform) opcode that
    //!        triggers playback of the waveform addressed by
    //!        `waveReg` into the channel selected by `dstReg`.
    //! \param waveReg    Register holding the waveform descriptor.
    //! \param dstReg     Destination channel register (`R0`
    //!                   selects the back-end's default slot).
    //! \param length     Sample count of the waveform.
    //! \param lineNumber Source line tagged on the emitted entry.
    //! \return The encoded assembler entry.
    virtual AsmList::Asm wvf(AsmRegister waveReg, AsmRegister dstReg,
                         int length, int lineNumber) const = 0;
    //! \brief Emit the `wvfi` (write-waveform-immediate) opcode.
    //! \param waveReg    Register holding the waveform descriptor.
    //! \param dstReg     Destination channel register.
    //! \param length     Sample count of the waveform.
    //! \param lineNumber Source line tagged on the emitted entry.
    //! \return The encoded assembler entry.
    //! \binarynote Hirzel throws `AssemblerException` for this
    //! opcode; only Cervino encodes it.
    virtual AsmList::Asm wvfi(AsmRegister waveReg, AsmRegister dstReg,
                          int length, int lineNumber) const = 0;
    //! \brief Emit the `wvfs` (write-waveform-special) opcode used
    //!        for `playZero` / `playHold` placeholder playback.
    //! \param dummyType  Sub-opcode discriminator
    //!                   (`Assembler::PlayDummyType`).
    //! \param reg        Destination register (channel slot).
    //! \param length     Sample count.
    //! \param lineNumber Source line tagged on the emitted entry.
    //! \return The encoded assembler entry.
    //! \binarynote Cervino throws for this opcode; only Hirzel
    //! encodes it.
    virtual AsmList::Asm wvfs(Assembler::PlayDummyType dummyType,
                          AsmRegister reg, int length, int lineNumber) const = 0;
    //! \brief Emit the `wvft` (write-waveform-trigger) opcode that
    //!        latches the trigger output for `length` cycles.
    //! \param reg        Destination register.
    //! \param length     Trigger duration in samples.
    //! \param lineNumber Source line tagged on the emitted entry.
    //! \return The encoded assembler entry.
    //! \binarynote Cervino throws for this opcode; only Hirzel
    //! encodes it.
    virtual AsmList::Asm wvft(AsmRegister reg, int length, int lineNumber) const = 0;

    //! \brief Emit the `brz` (branch-if-register-zero) opcode that
    //!        jumps to `label` when `reg` is zero.
    //! \param reg         Register tested against zero.
    //! \param label       Target label name.
    //! \param noOpt       When `true`, marks the branch so the
    //!                    optimiser leaves it in place even if it
    //!                    appears redundant.
    //! \param lineNumber  Source line tagged on the emitted entry.
    //! \return The encoded assembler entry.
    virtual AsmList::Asm brz(AsmRegister reg, const std::string& label,
                         bool noOpt, int lineNumber) const = 0;

    //! \brief Emit the `ssl` (signed-shift-left) opcode.
    //! \param reg         Register to shift in place.
    //! \param lineNumber  Source line tagged on the emitted entry.
    //! \return The encoded assembler entry.
    virtual AsmList::Asm ssl(AsmRegister reg, int lineNumber) const = 0;
    //! \brief Emit the `ssr` (signed-shift-right) opcode.
    //! \param reg         Register to shift in place.
    //! \param lineNumber  Source line tagged on the emitted entry.
    //! \return The encoded assembler entry.
    virtual AsmList::Asm ssr(AsmRegister reg, int lineNumber) const = 0;

    //! \brief Emit the `ldiotrig` (load-IO-trigger) opcode that
    //!        latches the device's external trigger byte into
    //!        `reg`.
    //! \param reg         Destination register.
    //! \param lineNumber  Source line tagged on the emitted entry.
    //! \return The encoded assembler entry.
    virtual AsmList::Asm ldiotrig(AsmRegister reg, int lineNumber) const = 0;

    //! \brief Factory: instantiate the concrete back end appropriate
    //!        for `deviceType`.
    //! \details Selects `AsmCommandsImplHirzel` for HDAWG / SHF /
    //! UHF families and `AsmCommandsImplCervino` for the remaining
    //! UHFLI / UHFQA / legacy devices.
    //! \param deviceType  Target AWG device family.
    //! \return Owning pointer to a freshly constructed back end.
    static std::unique_ptr<AsmCommandsImpl> getInstance(AwgDeviceType deviceType);
};

// Cervino implementation — older/FPGA devices.
//! \brief `AsmCommandsImpl` back end for the older Cervino sequencer family.
//!
//! Encodes the subset of instructions supported by Cervino, and throws
//! for the Hirzel-only forms (`wwvfq`, `wvfs`, `wvft`).  Reports
//! `isCWVFRSupported() == false`.  Selected by `getInstance()` for the
//! `AwgDeviceType` values not assigned to Hirzel.
class AsmCommandsImplCervino : public AsmCommandsImpl {
public:
    //! \brief Defaulted destructor.
    ~AsmCommandsImplCervino() override;

    //! \copydoc AsmCommandsImpl::isCWVFRSupported
    //! \details Cervino has no CWVFR support; reports `false`.
    bool isCWVFRSupported() const override;  // returns false

    //! \copydoc AsmCommandsImpl::wwvfq
    //! \details Cervino does not support `wwvfq`; always throws.
    //! \binarynote Throws `AssemblerException` whose message names
    //! the unsupported opcode.
    AsmList::Asm wwvfq(int lineNumber) const override;  // throws (unsupported)
    //! \copydoc AsmCommandsImpl::wprf
    //! \details Cervino encodes `wprf` as opcode 0xF0000000.
    AsmList::Asm wprf(int lineNumber) const override;    // opcode 0xF0000000

    //! \copydoc AsmCommandsImpl::wvf
    //! \details Cervino encodes `wvf` as opcode 0x20000000 with
    //! `waveReg` and `dstReg` in their respective fields.
    AsmList::Asm wvf(AsmRegister waveReg, AsmRegister dstReg,
                 int length, int lineNumber) const override;   // 0x20000000
    //! \copydoc AsmCommandsImpl::wvfi
    //! \details Encoded as opcode 0x30000000; throws when the
    //! operand combination is unsupported.
    AsmList::Asm wvfi(AsmRegister waveReg, AsmRegister dstReg,
                  int length, int lineNumber) const override;  // 0x30000000 or throws
    //! \copydoc AsmCommandsImpl::wvfs
    //! \details Cervino has no `wvfs` opcode; always throws.
    AsmList::Asm wvfs(Assembler::PlayDummyType dummyType, AsmRegister reg,
                  int length, int lineNumber) const override;   // throws
    //! \copydoc AsmCommandsImpl::wvft
    //! \details Cervino has no `wvft` opcode; always throws.
    AsmList::Asm wvft(AsmRegister reg, int length, int lineNumber) const override;  // throws

    //! \copydoc AsmCommandsImpl::brz
    //! \details Cervino encodes `brz` as opcode 0xF3000000.
    AsmList::Asm brz(AsmRegister reg, const std::string& label,
                 bool noOpt, int lineNumber) const override;   // 0xF3000000

    //! \copydoc AsmCommandsImpl::ssl
    //! \details Cervino encodes `ssl` as opcode 0x60000005.
    AsmList::Asm ssl(AsmRegister reg, int lineNumber) const override;  // 0x60000005
    //! \copydoc AsmCommandsImpl::ssr
    //! \details Cervino encodes `ssr` as opcode 0x60000006.
    AsmList::Asm ssr(AsmRegister reg, int lineNumber) const override;  // 0x60000006

    //! \copydoc AsmCommandsImpl::ldiotrig
    //! \details Cervino loads the IO trigger byte as 0x60.
    AsmList::Asm ldiotrig(AsmRegister reg, int lineNumber) const override;  // LD 0x60
};

// Hirzel implementation — HDAWG, UHF, SHF devices.
//! \brief `AsmCommandsImpl` back end for the Hirzel sequencer family.
//!
//! Encodes the Hirzel instruction set, including the `wwvfq`, `wvfs`,
//! and `wvft` forms that have no Cervino equivalent.  Reports
//! `isCWVFRSupported() == true`.  Selected by `getInstance()` for the
//! `AwgDeviceType` values picked out by its bitmask.
class AsmCommandsImplHirzel : public AsmCommandsImpl {
public:
    //! \brief Defaulted destructor.
    ~AsmCommandsImplHirzel() override;

    //! \copydoc AsmCommandsImpl::isCWVFRSupported
    //! \details Hirzel supports CWVFR; reports `true`.
    bool isCWVFRSupported() const override;  // returns true

    //! \copydoc AsmCommandsImpl::wwvfq
    //! \details Hirzel encodes `wwvfq` as opcode 0xF0000000.
    AsmList::Asm wwvfq(int lineNumber) const override;   // 0xF0000000
    //! \copydoc AsmCommandsImpl::wprf
    //! \details Hirzel emits a sentinel `INVALID` entry — the
    //! `wprf` semantics are folded directly into the surrounding
    //! `wvf` / `wwvfq` slots.
    AsmList::Asm wprf(int lineNumber) const override;     // INVALID (sentinel/no-op)

    //! \copydoc AsmCommandsImpl::wvf
    //! \details Hirzel selects opcode 0xFA000000 when
    //! `dstReg == R0` and 0x20000000 otherwise.
    AsmList::Asm wvf(AsmRegister waveReg, AsmRegister dstReg,
                 int length, int lineNumber) const override;
        // dstReg==R0: 0xFA000000; else: 0x20000000
    //! \copydoc AsmCommandsImpl::wvfi
    //! \details Hirzel does not encode `wvfi`; always throws.
    AsmList::Asm wvfi(AsmRegister waveReg, AsmRegister dstReg, int length,
                  int lineNumber) const override;  // always throws
    //! \copydoc AsmCommandsImpl::wvfs
    //! \details Hirzel encodes `wvfs` as opcode 0x30000001 with
    //! the play-dummy sub-opcode selected by `dummyType`.
    AsmList::Asm wvfs(Assembler::PlayDummyType dummyType,
                  AsmRegister reg, int length, int lineNumber) const override;
        // 0x30000001
    //! \copydoc AsmCommandsImpl::wvft
    //! \details Hirzel encodes `wvft` as opcode 0xFC000000 that
    //! latches the trigger output for `length` cycles.
    AsmList::Asm wvft(AsmRegister reg, int length, int lineNumber) const override;
        // 0xFC000000

    //! \copydoc AsmCommandsImpl::brz
    //! \details Hirzel uses opcode 0xFE000000 when `reg == R0`
    //! (treated as unconditional jump) and 0xF3000000 otherwise.
    AsmList::Asm brz(AsmRegister reg, const std::string& label,
                 bool noOpt, int lineNumber) const override;
        // reg==R0: 0xFE000000; else: 0xF3000000

    //! \copydoc AsmCommandsImpl::ssl
    //! \details Hirzel encodes `ssl` as opcode 0x60000005 with
    //! `R0` in the second slot.
    AsmList::Asm ssl(AsmRegister reg, int lineNumber) const override;
        // 0x60000005, second slot = R0
    //! \copydoc AsmCommandsImpl::ssr
    //! \details Hirzel encodes `ssr` as opcode 0x60000006 with
    //! `R0` in the second slot.
    AsmList::Asm ssr(AsmRegister reg, int lineNumber) const override;
        // 0x60000006, second slot = R0

    //! \copydoc AsmCommandsImpl::ldiotrig
    //! \details Hirzel loads the IO trigger byte as 0x68.
    AsmList::Asm ldiotrig(AsmRegister reg, int lineNumber) const override;
        // LD 0x68
};

} // namespace zhinst
