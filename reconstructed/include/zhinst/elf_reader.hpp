// ============================================================================
// Reconstructed from disassembly of _seqc_compiler.so — Phase 14d.
//
// ElfReader — thin C++ wrapper around ELFIO::elfio for reading ELF artifacts
// produced by ElfWriter. Loads a file, validates the magic + class +
// data + version, then walks the section table to cache:
//   - the ".format" section (single, stored at +0x70)
//   - all sections whose names start with ".dd" (vector at +0x78..+0x90)
//
// ----------------------------------------------------------------------------
// Layout (0x98 bytes total — supersedes the earlier 0x90-byte stub):
//
//   +0x00..+0x6F  ELFIO::elfio base class (privately inherited; size 0x70)
//                 (sections-proxy self-ptr at +0x00, segments-proxy self-ptr
//                  at +0x08, header_ at +0x10, sections_ vector at +0x18,
//                  segments_ vector at +0x30, endian flag at +0x48,
//                  string_section_data buf at +0x50, layout offset at +0x68)
//
//   +0x70  ELFIO::section* formatSection_   // ".format" section (or null)
//
//   +0x78..+0x90  std::vector<ELFIO::section*> ddSections_
//                 (libc++ vector — begin/end/cap; 24B)
//
//   +0x90  uint32_t  ddSectionIndex_ / unused dword (zeroed by ctor; purpose unknown)
//
//   ----- end at +0x98 -----
//
// Reconstructed methods:
//   ElfReader(string)             0x2c3110  — opens file, validates magic,
//                                            calls ELFIO::elfio::load(path),
//                                            then readHeader().
//   ~ElfReader()                  0x2b18c0  — frees the dd-sections vector
//                                            buffer, chains ~ELFIO::elfio.
//   isElfFile(string) [static]    0x2c3320  — opens with boost.filesystem
//                                            ifstream, reads 4 bytes,
//                                            checks ELF magic 0x464c457f.
//   readHeader()                  0x2c3850  — walks sections; classifies
//                                            ".format" / ".dd*".
//   getSection(string) const      0x2c4000  — linear name-search; throws
//                                            ElfException if not found
//                                            (NOT nullptr — supersedes
//                                            earlier stub annotation).
//
// ElfException @0x2c7a40 (ctor) / 0x2c7b40 (what) / 0x2c7b60 (~) / @0x2c35e0
//   (alternate dtor used as cleanup_t in __cxa_throw).
//
//   - Inherits privately from std::bad_exception (vtable @0xb08b40).
//   - Single data field: a libc++ SSO std::string at +0x08, holding
//     "ELF Exception: " + user-supplied message.
//   - what() returns the SSO data pointer (either inline buffer at +0x09
//     or external pointer at +0x18, selected by the SSO bit at +0x08).
// ============================================================================

#pragma once

#include <cstdint>
#include <exception>
#include <string>
#include <vector>

#include <elfio/elfio.hpp>

namespace zhinst {

// ---------------------------------------------------------------------------
// ElfException — thrown by ElfReader on validation / lookup failure.
//
// Source-level reconstruction: the binary inherits privately from
// std::bad_exception (which is itself a std::exception). At source level
// we expose the simpler base (std::exception) since no consumer in the
// reconstructed source tree depends on bad_exception specifically. The
// observed `what()` behaviour and the "ELF Exception: " message prefix
// are preserved.
// ---------------------------------------------------------------------------
class ElfException : public std::exception {
public:
    // Constructs the exception with the prefix "ELF Exception: " followed
    // by the supplied message. Prefix is hard-coded in the binary via
    // movabs immediates at 0x2c7a78..0x2c7a94.
    explicit ElfException(const std::string& message);   // 0x2c7a40
    ~ElfException() override;                            // 0x2c7b60

    const char* what() const noexcept override;          // 0x2c7b40

private:
    std::string message_;   // "ELF Exception: " + msg
};

// ---------------------------------------------------------------------------
// ElfReader — privately inherits from ELFIO::elfio (matches binary). The
// base class is 0x70 bytes; ElfReader adds formatSection_ + ddSections_
// + ddSectionIndex_ for a total of 0x98 bytes.
// ---------------------------------------------------------------------------
class ElfReader : private ELFIO::elfio {
public:
    // Opens `path` and parses it as an ELF file. Throws ElfException
    // (with message "not a valid ELF file " + path) if the magic check
    // fails. After the load succeeds, runs readHeader() to populate
    // formatSection_ and ddSections_.
    explicit ElfReader(const std::string& path);   // 0x2c3110

    // Frees the dd-sections vector buffer, chains to ~ELFIO::elfio.
    ~ElfReader();                                  // 0x2b18c0

    ElfReader(const ElfReader&) = delete;
    ElfReader& operator=(const ElfReader&) = delete;

    // Linear name search across the loaded section table. Returns a
    // non-owning pointer to the matching ELFIO::section. **Throws
    // ElfException("section not found: " + name + ...)** if the section
    // is absent — the earlier stub's "or nullptr" claim was wrong.
    ELFIO::section* getSection(const std::string& name) const;   // 0x2c4000

    // Static utility: checks the first four bytes of `path` for the ELF
    // magic 0x464c457f (\x7fELF). Used by the ctor as a precondition.
    static bool isElfFile(const std::string& path);              // 0x2c3320

    // ELF section data extraction.

    // Return type for getCode() and getWaveform() — a format tag plus raw bytes.
    struct SectionData {
        std::uint32_t format = 0;        // section type from ELFIO header
        std::vector<std::uint8_t> data;  // raw bytes (size-aligned per method)
    };

    // Line map entry — 16-byte record from ".linenr" section.
    struct Line {
        std::uint64_t addr;
        std::uint32_t line;
    };

    // getCode() @0x2c3bc0 — reads formatSection_ data, size aligned to 4.
    SectionData getCode() const;

    // getWaveform() @0x2c3d40 — reads ddSections_[ddSectionIndex_], size aligned to 2.
    SectionData getWaveform() const;

    // getLineMap() @0x2c3ef0 — reads ".linenr" section, parses 16-byte records.
    std::vector<Line> getLineMap() const;

private:
    // Walks the section table and partitions sections by name:
    //   - ".format"  -> formatSection_   (single)
    //   - starts with ".dd" -> push_back into ddSections_
    // Other sections are ignored. Skips early if the ELF header reports
    // get_class() != ELFCLASS32 OR get_encoding() != ELFDATA2LSB
    // (sentinel checks observed in the binary — ElfWriter only ever
    // produces 32-bit little-endian files).
    void readHeader();                             // 0x2c3850

    // ----- Storage (immediately after the 0x70-byte ELFIO::elfio base) -----

    ELFIO::section* formatSection_ = nullptr;        // +0x70 ".format" section
    std::vector<ELFIO::section*> ddSections_;        // +0x78..+0x90 — sections
                                                     //   whose names start ".dd"

    // Trailing dword zeroed by the ctor (purpose unknown — possibly a flags
    // field for a not-yet-reconstructed feature). Kept for layout fidelity.
    std::uint32_t ddSectionIndex_ = 0;                          // +0x90
};

// ---------------------------------------------------------------------------
// Free helper: construct a std::string view of an ELFIO section's raw bytes.
//
// Many call sites (CachedParser, ElfWriter readback paths, ...) treat ELF
// section data as an opaque byte blob and want it as a std::string for easy
// slicing / hashing / memcpy. The binary inlines this pattern at every use
// site as `std::string(sec->get_data(), sec->get_size())`. Centralizing it
// here keeps the reconstructed source readable without inventing a method
// on ELFIO::section that doesn't actually exist in the upstream library.
// Returns an empty string if `sec` is null or has zero size.
inline std::string sectionAsString(const ELFIO::section* sec) {
    if (sec == nullptr) return {};
    const char* data = sec->get_data();
    const std::size_t n = static_cast<std::size_t>(sec->get_size());
    if (data == nullptr || n == 0) return {};
    return std::string(data, n);
}

}  // namespace zhinst
