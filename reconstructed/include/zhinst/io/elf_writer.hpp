// ============================================================================
// Reconstructed from disassembly of _seqc_compiler.so
// zhinst::ElfWriter — 32-bit ELF binary output writer for AWG sequencer code
//
// Source file: ElfWriter.cpp (from .debug_line / symbol table)
//
// Class size: 0x78 bytes (ELFIO::elfio base 0x70 + memoryOffset_ 0x08)
//
// ElfWriter inherits privately from ELFIO::elfio and wraps it to produce
// 32-bit ELF files targeting ZI AWG devices. The ELF header is configured
// as: ELFCLASS32, ELFDATA2LSB, EV_CURRENT, ET_EXEC, with a device-specific
// e_machine value (the `machineType` parameter).
//
// Layout:
//   +0x00..+0x6F: ELFIO::elfio base (0x70 bytes)
//     +0x00: this-ptr (Sections proxy self-ref)
//     +0x08: this-ptr (Segments proxy self-ref)
//     +0x10: elf_header* header_
//     +0x18: vector<section*> sections_
//     +0x30: vector<segment*> segments_
//     +0x48: bool  (endian converter flag)
//     +0x50: vector<char> (internal string buffer)
//     +0x68: uint64_t (layout offset tracker)
//   +0x70: uint64_t memoryOffset_
//   +0x78: END
//
// Confirmed from:
//   - Constructor @0x2934a0: zeros +0x10..+0x68, allocs header, stores +0x70=0
//   - setMemoryOffset @0x294410: stores to +0x70
//   - writeFile(ostream) @0x294030: reads +0x70 to set entry point before save
//   - ELFIO::elfio::~elfio @0x10e2e0: destructs fields up to +0x68
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

// ============================================================================
// ElfWriter — wraps ELFIO::elfio for AWG ELF output
//
// The class inherits from ELFIO::elfio (privately). In the binary, the
// first 0x70 bytes ARE the elfio base object. We model this as inheritance
// rather than composition because the constructor initializes `this` as
// an elfio object directly (stores self-pointers at +0x00, +0x08).
//
// Total size: 0x78 bytes
// ============================================================================
class ElfWriter : private ELFIO::elfio {
public:
    // --- Constructor ---
    // Initializes a 32-bit ELF with the given machine type.
    // Sets: ELFCLASS32, ELFDATA2LSB, EV_CURRENT, ET_EXEC, e_machine=machineType.
    // Calls prepareHeader(machineType) at the end.
    explicit ElfWriter(uint16_t machineType);                    // 0x2934a0

    // Destructor is the ELFIO::elfio destructor (inherited)    // via 0x10e2e0

    // --- Header configuration ---
    // Sets ELF header: e_type=ET_NONE(0), e_machine=machineType, e_flags=0
    void prepareHeader(uint16_t machineType);                    // 0x2936b0

    // --- Section builders ---

    // Adds a ".text" section (SHT_PROGBITS, SHF_ALLOC|SHF_EXECINSTR, align=64)
    // with the given opcodes. Creates a PT_LOAD segment with PF_R|PF_X and
    // align=64, virtual/physical address = memoryOffset_.
    void addCode(std::vector<uint32_t> const& opcodes);          // 0x293710

    // Adds a named section (SHT_PROGBITS, align=4) with raw data.
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
    std::unique_ptr<RawWave> addWaveform(
                           std::shared_ptr<WaveformIR> waveform,
                           SampleFormat format,
                           bool useMapped,
                           detail::AddressImpl<uint32_t> padSize); // 0x2939f0

    // --- Output ---

    // Writes the ELF to an output stream. If the header exists, sets the
    // entry point to memoryOffset_ before saving.
    void writeFile(std::ostream& os);                            // 0x294030

    // Opens a file and writes the ELF. Uses boost::filesystem::basic_ofstream
    // with ios::binary|ios::trunc mode.
    bool writeFile(std::string const& filename);                 // 0x2942a0

    // --- Configuration ---

    // Sets the memory offset used as the entry point and segment base address.
    void setMemoryOffset(uint64_t offset);                       // 0x294410

private:
    uint64_t memoryOffset_;          // +0x70, default = 0
};

} // namespace zhinst
