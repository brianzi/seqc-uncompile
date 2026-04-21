// ============================================================================
// Reconstructed from disassembly of _seqc_compiler.so
// zhinst::ElfWriter — all 8 methods
//
// Source file: ElfWriter.cpp (confirmed from symbol table)
//
// Uses the ELFIO library for ELF construction. ElfWriter inherits from
// ELFIO::elfio, so all elfio methods (sections, segments, set_type, etc.)
// are directly accessible on `this`.
// ============================================================================

#include "zhinst/elf_writer.hpp"
#include "zhinst/waveform_ir.hpp"
#include "zhinst/waveform.hpp"
#include "zhinst/signal.hpp"
#include "zhinst/rawwave.hpp"

#include <elfio/elfio.hpp>
#include <fstream>
#include <cstring>

namespace zhinst {

using namespace ELFIO;  // for ET_NONE, SHT_PROGBITS, PT_LOAD, PF_R, etc.

// ============================================================================
// ElfWriter::ElfWriter(uint16_t machineType)                    // 0x2934a0
//
// Binary size: 0x204 bytes (0x2934a0 - 0x2936a4)
//
// The elfio base class default ctor calls create(ELFCLASS32, ELFDATA2LSB).
// After base init, prepareHeader sets the machine type, then memoryOffset_=0.
// ============================================================================
ElfWriter::ElfWriter(uint16_t machineType)                       // 0x2934a0
    : ELFIO::elfio()
    , memoryOffset_(0)
{
    // elfio default ctor already calls create(ELFCLASS32, ELFDATA2LSB)
    // which sets up the ELF32 header and mandatory sections (.shstrtab, null).

    // Configure the header for our target device
    prepareHeader(machineType);
}

// ============================================================================
// ElfWriter::prepareHeader(uint16_t machineType)                // 0x2936b0
//
// Binary size: 0x54 bytes (0x2936b0 - 0x293704)
//
// Sets three ELF header fields:
//   e_type = ET_NONE (0)
//   e_machine = machineType
//   e_flags = 0
// ============================================================================
void ElfWriter::prepareHeader(uint16_t machineType)              // 0x2936b0
{
    set_type(ET_NONE);
    set_machine(machineType);
    set_flags(0);
}

// ============================================================================
// ElfWriter::addCode(vector<uint32_t> const& opcodes)           // 0x293710
//
// Binary size: 0x12e bytes (0x293710 - 0x29383e)
//
// Creates a ".text" section (SHT_PROGBITS, SHF_ALLOC|SHF_EXECINSTR, align=64)
// and a PT_LOAD segment (PF_R|PF_X, align=64) at memoryOffset_.
// ============================================================================
void ElfWriter::addCode(std::vector<uint32_t> const& opcodes)    // 0x293710
{
    // Create .text section
    ELFIO::section* sec = sections.add(".text");
    sec->set_type(SHT_PROGBITS);
    sec->set_flags(SHF_ALLOC | SHF_EXECINSTR);
    sec->set_addr_align(0x40);

    const char* data = reinterpret_cast<const char*>(opcodes.data());
    size_t byteSize = opcodes.size() * sizeof(uint32_t);
    sec->set_data(data, byteSize);

    // Create a PT_LOAD segment for code
    ELFIO::segment* seg = segments.add();
    seg->set_type(PT_LOAD);
    seg->set_virtual_address(memoryOffset_);
    seg->set_physical_address(memoryOffset_);
    seg->set_flags(PF_R | PF_X);
    seg->set_align(0x40);

    // Link section to segment
    seg->add_section_index(sec->get_index(), sec->get_addr_align());
}

// ============================================================================
// ElfWriter::addData(char const* data, size_t size,             // 0x293990
//                    string const& sectionName)
//
// Binary size: 0x55 bytes (0x293990 - 0x2939e5)
//
// Creates a named section (SHT_PROGBITS, align=4) with raw data.
// Note: size is truncated to 32-bit in the binary (mov %ebx,%edx).
// ============================================================================
void ElfWriter::addData(const char* data, size_t size,           // 0x293990
                        std::string const& sectionName)
{
    ELFIO::section* sec = sections.add(sectionName);
    sec->set_type(SHT_PROGBITS);
    sec->set_addr_align(4);
    sec->set_data(data, static_cast<uint32_t>(size));
}

// ============================================================================
// ElfWriter::addWaveform(shared_ptr<WaveformIR> waveform,       // 0x2939f0
//                        SampleFormat format, bool useAbsolute,
//                        AddressImpl<uint> padSize)
//
// Binary size: 0x4ce bytes (0x2939f0 - 0x293ebe)
//
// Adds waveform data to the ELF with up to two sections and one segment.
//
// Calling convention (sret):
//   rdi = sret ptr (unique_ptr<RawWave> return), rsi = this,
//   rdx = &shared_ptr<WaveformIR> (by-value, passed indirectly),
//   ecx = SampleFormat, r8d = useAbsolute, r9d = padSize.
//
// Algorithm:
//   1. Get raw waveform bytes via Signal::getRawData(format)
//   2. Create PT_LOAD segment at (addressValue - padSize)
//   3. If padSize > 0: create ".dd_<name>" zero-padding section
//   4. Create ".wf_<name>" section with waveform data
//   5. If useAbsolute && reserveOnly: NOBITS section + set_size(rawDataSize)
//      Else: PROGBITS section with actual data
//   6. Link sections to segment
//   7. Return the unique_ptr<RawWave> from getRawData
// ============================================================================
std::unique_ptr<RawWave> ElfWriter::addWaveform(                 // 0x2939f0
    std::shared_ptr<WaveformIR> waveform,
    SampleFormat format,
    bool useAbsolute,
    detail::AddressImpl<uint32_t> padSize)
{
    // Get the raw waveform bytes — the unique_ptr is returned to the caller
    WaveformIR* wfPtr = waveform.get();
    Signal& signal = wfPtr->signal;  // Waveform+0x80
    auto rawData = signal.getRawData(format);

    // 0x293a2e: save rawData->size() for later use in NOBITS path
    size_t rawDataSize = rawData->size();

    // Create a PT_LOAD segment for the waveform data
    ELFIO::segment* seg = segments.add();                        // 0x293a3a
    seg->set_type(PT_LOAD);
    seg->set_virtual_address(wfPtr->addressValue - padSize);     // 0x293a60
    seg->set_physical_address(wfPtr->addressValue - padSize);    // 0x293a73
    seg->set_flags(PF_W);

    // Alignment from WaveformIR+0xDC (bitsPerSample * channels product)
    uint32_t alignment = wfPtr->irField2;
    seg->set_align(alignment);

    if (padSize > 0) {
        // Add descriptor data section ".dd_<name>" with zero padding
        std::string wfName = wfPtr->name;
        std::string ddSectionName = ".dd_" + wfName;

        ELFIO::section* ddSec = sections.add(ddSectionName);
        ddSec->set_type(SHT_PROGBITS);
        ddSec->set_flags(SHF_ALLOC);
        ddSec->set_addr_align(alignment);

        // Create zero-filled padding buffer
        std::string padding(padSize, '\0');
        ddSec->append_data(padding.data(), padding.size());

        // Link descriptor section to segment
        seg->add_section_index(ddSec->get_index(), ddSec->get_addr_align());
    }

    // Add waveform data section ".wf_<name>"
    std::string wfName2 = wfPtr->name;
    std::string wfSectionName = ".wf_" + wfName2;

    ELFIO::section* wfSec = sections.add(wfSectionName);
    wfSec->set_flags(SHF_ALLOC);
    wfSec->set_addr_align(alignment);

    if (useAbsolute && wfPtr->signal.reserveOnly_) {             // 0x293dbc
        // Reserve-only waveform in absolute mode: NOBITS section
        // The section declares address and size metadata but contains no data.
        wfSec->set_type(SHT_NOBITS);                            // 0x293dd9
        wfSec->set_address(wfPtr->addressValue);                // 0x293de8
        wfSec->set_size(rawDataSize);                            // 0x293df7
    } else {
        // Normal waveform: PROGBITS with actual data
        wfSec->set_type(SHT_PROGBITS);                          // 0x293e0a
        wfSec->set_data(rawData->ptr(), rawData->size());        // 0x293e24
    }

    // Link waveform section to segment
    seg->add_section_index(wfSec->get_index(), wfSec->get_addr_align()); // 0x293e4c

    return rawData;
}

// ============================================================================
// ElfWriter::writeFile(ostream& os)                             // 0x294030
//
// Binary size: 0x3c bytes (0x294030 - 0x29406c)
//
// Sets entry point to memoryOffset_ then calls elfio::save(ostream).
// ============================================================================
void ElfWriter::writeFile(std::ostream& os)                      // 0x294030
{
    set_entry(memoryOffset_);
    save(os);
}

// ============================================================================
// ElfWriter::writeFile(string const& filename)                  // 0x2942a0
//
// Binary size: 0x170 bytes (0x2942a0 - 0x294410)
//
// Opens a file with ios::binary|ios::trunc, sets entry point, saves ELF.
// The binary uses boost::filesystem::basic_ofstream but std::ofstream
// is functionally equivalent.
// ============================================================================
bool ElfWriter::writeFile(std::string const& filename)           // 0x2942a0
{
    std::ofstream ofs(filename.c_str(),
                      std::ios::out | std::ios::binary | std::ios::trunc);

    set_entry(memoryOffset_);
    return save(ofs);
}

// ============================================================================
// ElfWriter::setMemoryOffset(uint64_t offset)                   // 0x294410
//
// Binary size: 0x0a bytes (0x294410 - 0x29441a)
//
// Trivial setter: mov %rsi,0x70(%rdi)
// ============================================================================
void ElfWriter::setMemoryOffset(uint64_t offset)                 // 0x294410
{
    memoryOffset_ = offset;
}

} // namespace zhinst
