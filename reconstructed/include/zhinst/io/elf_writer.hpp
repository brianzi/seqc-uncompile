// ============================================================================
// Reconstructed from disassembly of _seqc_compiler.so
// zhinst::ElfWriter — 32-bit ELF binary output writer for AWG sequencer code
//
// ElfWriter inherits privately from ELFIO::elfio and wraps it to produce
// 32-bit ELF files targeting ZI AWG devices. The ELF header is configured
// as: ELFCLASS32, ELFDATA2LSB, EV_CURRENT, ET_EXEC, with a device-specific
// e_machine value (the `machineType` parameter).
// ============================================================================
#pragma once

#include <cstdint>

#include <elfio/elfio.hpp>
#include <memory>
#include <ostream>
#include <string>
#include <vector>

#include "zhinst/waveform/rawwave.hpp"
#include "zhinst/asm/address_impl.hpp"
#include "zhinst/waveform/signal.hpp"        // for SampleFormat

namespace zhinst {

struct WaveformIR;  // forward declaration
struct DeviceConstants;  // forward declaration

// Offset  Size  Type              Name           Notes
// +0x00   8     T*                               Sections proxy self-ref (ELFIO base)
// +0x08   8     T*                               Segments proxy self-ref (ELFIO base)
// +0x10   8     elf_header*                      header_ (ELFIO base)
// +0x18   24    vector<section*>                 sections_ (ELFIO base)
// +0x30   24    vector<segment*>                 segments_ (ELFIO base)
// +0x48   8     bool + padding                   endian flag (ELFIO base)
// +0x50   24    vector<char>                     string buffer (ELFIO base)
// +0x68   8     uint64_t                         layout offset (ELFIO base)
// +0x70   8     uint64_t          memoryOffset_  AWG instruction memory base
// sizeof(ElfWriter) = 0x78
//
// Note: fields +0x00..+0x68 are the ELFIO::elfio base (private inheritance).
// The only ZI-owned field is memoryOffset_ at +0x70.
//! Builds the 32-bit little-endian ELF firmware container that the
//! AWG sequencer consumes.
//!
//! Wraps `ELFIO::elfio` and exposes a focused API for the sections
//! the SeqC compiler emits: a single `.text` code segment plus
//! per-waveform `.dd_<name>` / `.wf_<name>` pairs and arbitrary named
//! data sections. The class fixes the ELF class and encoding
//! (ELFCLASS32, ELFDATA2LSB, ET_EXEC) so callers only need to choose
//! the device-specific `e_machine` value at construction time.
//!
//! Use `setMemoryOffset()` to pick the AWG instruction-memory base;
//! it becomes both the entry point (`e_entry`) and the virtual /
//! physical address of the `.text` PT_LOAD segment. After all
//! sections have been added, `writeFile()` emits the final ELF to a
//! stream or filesystem path.
class ElfWriter : private ELFIO::elfio {
public:
    // --- Constructor ---
    // Initializes a 32-bit ELF with the given machine type.
    // Sets: ELFCLASS32, ELFDATA2LSB, EV_CURRENT, ET_EXEC, e_machine=machineType.
    // Calls prepareHeader(machineType) at the end.
    //! \brief Initialises a fresh 32-bit little-endian ET_EXEC ELF
    //! and calls `prepareHeader(machineType)` to install the AWG
    //! header layout.
    //! \param machineType ELF `e_machine` value identifying the
    //! target AWG family.
    explicit ElfWriter(uint16_t machineType);                    // 0x2934a0

    // Destructor is the ELFIO::elfio destructor (inherited)    // via 0x10e2e0

    // --- Header configuration ---
    // Sets ELF header: e_type=ET_NONE(0), e_machine=machineType, e_flags=0
    //! \brief Rewrites the ELF header so the file becomes ET_NONE
    //! with `e_machine = machineType` and `e_flags = 0`; called from
    //! the constructor and re-callable if the machine type needs to
    //! be switched.
    //! \param machineType ELF `e_machine` value to install.
    void prepareHeader(uint16_t machineType);                    // 0x2936b0

    // --- Section builders ---

    // Adds a ".text" section (SHT_PROGBITS, SHF_ALLOC|SHF_EXECINSTR, align=64)
    // with the given opcodes. Creates a PT_LOAD segment with PF_R|PF_X and
    // align=64, virtual/physical address = memoryOffset_.
    //! \brief Emits the `.text` code section (executable, alloc,
    //! 64-byte aligned) and wraps it in a `PT_LOAD` segment based
    //! at `memoryOffset_` with PF_R | PF_X permissions.
    //! \param opcodes 32-bit instruction stream to install as
    //! `.text` payload.
    void addCode(std::vector<uint32_t> const& opcodes);          // 0x293710

    // Adds a named section (SHT_PROGBITS, align=4) with raw data.
    //! \brief Appends a 4-byte-aligned `SHT_PROGBITS` section
    //! named `sectionName` carrying the bytes at `[data, data +
    //! size)`.
    //! \param data        Pointer to the raw bytes to copy in.
    //! \param size        Number of bytes available at `data`.
    //! \param sectionName ELF section name (e.g. `.waveforms`,
    //!                    `.channels`, `.asm`).
    void addData(const char* data, size_t size,
                 std::string const& sectionName);                // 0x293990

    // Adds waveform data as ".dd_<name>" (descriptor data) and ".wf_<name>"
    // (raw waveform) sections. Creates a PT_LOAD segment for the waveform.
    //
    // Parameters:
    //   waveform    - shared_ptr to the WaveformIR
    //   format      - SampleFormat (Cervino=0 or Hirzel16=1)
    //   useMapped - if true AND waveform is reserveOnly, sets segment
    //                 as NOBITS with address metadata
    //   padSize     - AddressImpl<uint> padding/alignment size
    //
    // Returns the unique_ptr<RawWave> produced by Signal::getRawData().
    // The caller can query size() on the returned object to know how many
    // bytes of waveform data were written (used by writeWavesToElfAbsolute
    // to track cumulative offset).
    //! \brief Emits the `.dd_<name>` descriptor section and the
    //! `.wf_<name>` raw-sample section for `waveform`, wraps them in
    //! a dedicated `PT_LOAD` segment, and returns the
    //! `RawWave` so the caller can track the bytes consumed.
    //! \param waveform   Waveform to serialise; the function reads
    //!                   `signal` and `name` to derive section
    //!                   names and payload.
    //! \param format     Sample-encoding format (`Cervino` or
    //!                   `Hirzel16`) selecting word width and
    //!                   interleave.
    //! \param useMapped  When true and `waveform->signal` is
    //!                   `reserveOnly`, emits the segment as
    //!                   `SHT_NOBITS` carrying only address
    //!                   metadata.
    //! \param padSize    Padding / alignment quantum applied to the
    //!                   waveform segment.
    //! \return The `RawWave` produced by `Signal::getRawData`;
    //! `RawWave::size()` reports the bytes written to the section.
    std::unique_ptr<RawWave> addWaveform(
                           std::shared_ptr<WaveformIR> waveform,
                           SampleFormat format,
                           bool useMapped,
                           detail::AddressImpl<uint32_t> padSize); // 0x2939f0

    // --- Output ---

    // Writes the ELF to an output stream. If the header exists, sets the
    // entry point to memoryOffset_ before saving.
    //! \brief Writes the populated ELF to `os`; sets the entry
    //! point to `memoryOffset_` first when a header is present.
    //! \param os Destination stream (typically opened in binary
    //!           mode).
    void writeFile(std::ostream& os);                            // 0x294030

    // Opens a file and writes the ELF. Uses boost::filesystem::basic_ofstream
    // with ios::binary|ios::trunc mode.
    //! \brief Convenience overload: opens `filename` in
    //! `binary | trunc` mode and forwards to the stream overload.
    //! \param filename Filesystem path the ELF is written to.
    //! \return `true` on success, `false` when the file could not
    //! be opened or the underlying write failed.
    bool writeFile(std::string const& filename);                 // 0x2942a0

    // --- Configuration ---

    // Sets the memory offset used as the entry point and segment base address.
    //! \brief Sets the AWG instruction-memory base address used as
    //! `e_entry` and as the virtual/physical address of the
    //! `.text` `PT_LOAD` segment.
    //! \param offset Memory base address (device-specific; e.g.
    //!               `0x80000000` for HDAWG / SHF).
    void setMemoryOffset(uint64_t offset);                       // 0x294410

private:
    //! \brief AWG instruction-memory base address; populated by
    //! `setMemoryOffset` and consumed by `addCode` / `writeFile`.
    uint64_t memoryOffset_;          // +0x70, default = 0
};

} // namespace zhinst
