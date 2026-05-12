// ============================================================================
// Reconstructed from disassembly of _seqc_compiler.so
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
//! Exception thrown by `ElfReader` when an ELF artifact fails to
//! load or a requested section is missing.
//!
//! All messages are prefixed with `"ELF Exception: "`, so the
//! `what()` text identifies the source even when the exception is
//! caught as a generic `std::exception`.
class ElfException : public std::exception {
public:
    //! \brief Constructs the exception with the prefix
    //!        `"ELF Exception: "` followed by `message`.
    //! \details The prefix is fixed at construction time; subsequent
    //! `what()` reads it back via the SSO `std::string` storage.
    //! \param message  User-supplied diagnostic text; copied.
    // Prefix is hard-coded in the binary via movabs immediates at
    // 0x2c7a78..0x2c7a94.
    explicit ElfException(const std::string& message);   // 0x2c7a40
    //! \brief Release the embedded `message_` storage and chain to
    //!        `~std::exception`.
    ~ElfException() override;                            // 0x2c7b60

    //! \brief Return the prefixed diagnostic text.
    //! \return `message_.c_str()` — never null; valid for the
    //! lifetime of `*this`.
    const char* what() const noexcept override;          // 0x2c7b40

private:
    //! \brief `"ELF Exception: "` + caller-supplied message.
    std::string message_;   // "ELF Exception: " + msg
};

// ---------------------------------------------------------------------------
// ElfReader — privately inherits from ELFIO::elfio (matches binary). The
// base class is 0x70 bytes; ElfReader adds formatSection_ + ddSections_
// + ddSectionIndex_ for a total of 0x98 bytes.
// ---------------------------------------------------------------------------
//! Loads an ELF artifact produced by `ElfWriter` and exposes the
//! sections the SeqC toolchain consumes.
//!
//! The constructor opens the file, validates the ELF magic / class /
//! encoding, and walks the section table once to cache references
//! to the `.format` section and every `.dd*` waveform-descriptor
//! section. Section data is then served by name through
//! `getSection()` (which throws `ElfException` on a missing name) or
//! through the typed convenience accessors `getCode()`,
//! `getWaveform()`, and `getLineMap()`.
//!
//! Only ELFCLASS32 / ELFDATA2LSB inputs are accepted; other ELF
//! variants are silently treated as if they contained no sections.
class ElfReader : private ELFIO::elfio {
public:
    //! \brief Open `path` and parse it as an ELF artifact, caching
    //!        the `.format` section and every `.dd*` section for
    //!        later retrieval.
    //! \details Validates the ELF magic via `isElfFile`, then loads
    //! the file through `ELFIO::elfio::load(path)` and runs
    //! `readHeader()` to populate `formatSection_` and
    //! `ddSections_`.
    //! \param path  Filesystem path to the ELF artifact.
    //! \throws ElfException  with message `"not a valid ELF
    //! file " + path` when the magic check fails.
    explicit ElfReader(const std::string& path);   // 0x2c3110

    //! \brief Release the cached section vector and chain to the
    //!        `ELFIO::elfio` destructor.
    ~ElfReader();                                  // 0x2b18c0

    //! \brief Non-copyable: copying an open ELF reader would
    //!        double-own the underlying `ELFIO::elfio` state.
    ElfReader(const ElfReader&) = delete;
    //! \brief Non-copyable: copy-assignment is deleted for the
    //!        same reason as the copy constructor.
    ElfReader& operator=(const ElfReader&) = delete;

    //! \brief Look up an ELF section by exact name.
    //! \details Linear scan over the section table.  Returns the
    //! matching section pointer; raises `ElfException` when no
    //! section matches the requested name.
    //! \param name  Section name (e.g. `".text"`, `".linenr"`).
    //! \return Non-owning pointer to the matching section.
    //! \throws ElfException  When the section is absent.
    ELFIO::section* getSection(const std::string& name) const;   // 0x2c4000

    //! \brief Cheap precondition check: peek at the first four
    //!        bytes of `path` and test the ELF magic 0x464c457f
    //!        (`\x7fELF`).
    //! \details Opens via `boost::filesystem::ifstream`; failure to
    //! open is treated as "not an ELF file".  Used by the
    //! `ElfReader` constructor before the full ELFIO load.
    //! \param path  Filesystem path to test.
    //! \return `true` when the magic matches, `false` otherwise.
    static bool isElfFile(const std::string& path);              // 0x2c3320

    // ELF section data extraction.

    //! \brief Return type for `getCode()` and `getWaveform()`:
    //!        section-type tag plus raw bytes.
    struct SectionData {
        //! \brief Section type as reported by the ELFIO header.
        std::uint32_t sectionType = 0;   // section type from ELFIO header
        //! \brief Raw section bytes (size-aligned per accessor).
        std::vector<std::uint8_t> data;  // raw bytes (size-aligned per method)
    };

    //! \brief Line-map record (a 16-byte entry from the `.linenr`
    //!        section, see notes/elf_reader.md).
    struct Line {
        //! \brief Instruction address the record pins.
        std::uint64_t addr;
        //! \brief Source line associated with `addr`.
        std::uint32_t line;
    };

    //! \brief Return the `.format` section payload (4-byte aligned).
    //! \return A `SectionData` whose `data` holds the raw bytes of
    //! the cached `formatSection_`.
    SectionData getCode() const;                            // 0x2c3bc0

    //! \brief Return the currently selected `.dd*` waveform-
    //!        descriptor section (2-byte aligned).
    //! \return A `SectionData` for `ddSections_[ddSectionIndex_]`.
    SectionData getWaveform() const;                        // 0x2c3d40

    //! \brief Parse the `.linenr` section into a vector of 16-byte
    //!        `Line` records.
    //! \return Sequence of address → line mappings in section order.
    std::vector<Line> getLineMap() const;                   // 0x2c3ef0

private:
    //! \brief Walk the section table once and classify each entry
    //!        into the `formatSection_` / `ddSections_` caches.
    //! \details Early-exits without populating either cache when
    //! the ELF header reports `get_class() != ELFCLASS32` or
    //! `get_encoding() != ELFDATA2LSB`, matching the
    //! 32-bit-little-endian-only constraint that
    //! `ElfWriter` produces.
    void readHeader();                             // 0x2c3850

    // ----- Storage (immediately after the 0x70-byte ELFIO::elfio base) -----

    //! \brief Cached pointer to the `.format` section, or `nullptr`
    //!        when absent.
    ELFIO::section* formatSection_ = nullptr;        // +0x70 ".format" section
    //! \brief Cached pointers to every section whose name starts
    //!        with `".dd"`, in section-table order.
    std::vector<ELFIO::section*> ddSections_;        // +0x78..+0x90 — sections
                                                     //   whose names start ".dd"

    //! \brief Current selector into `ddSections_` used by
    //!        `getWaveform()`.
    //!
    //! Trailing dword zeroed by the constructor; the binary's
    //! purpose for this slot is not yet established (see
    //! `notes/elf_reader.md`).
    //! \unverifiable Unverifiable from SeqC: the field is read by
    //! `getWaveform()` as the index into `ddSections_`, but no
    //! reconstructed code path writes it to a non-zero value and no
    //! `compile_seqc` input drives `getWaveform()` to observe a
    //! non-zero selector.  The semantic role of a non-zero value
    //! (single fixed slot vs rotating selector vs flag) therefore
    //! cannot be established from current observations.
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
//! \brief Return the contents of an ELFIO section as a `std::string`
//!        of raw bytes, safely handling null sections and empty
//!        payloads.
//! \details Equivalent to the inlined `std::string(sec->get_data(),
//! sec->get_size())` pattern used throughout the reader; returns an
//! empty string when `sec` is `nullptr`, when `get_data()` returns
//! null, or when `get_size()` is zero. The returned string is a
//! copy and may contain embedded NULs (sections are not C-strings).
//! \param sec Pointer to an ELFIO section, or `nullptr`.
//! \return Section bytes as `std::string`; empty on any of the
//!         null / zero-size conditions above.
inline std::string sectionAsString(const ELFIO::section* sec) {
    if (sec == nullptr) return {};
    const char* data = sec->get_data();
    const std::size_t n = static_cast<std::size_t>(sec->get_size());
    if (data == nullptr || n == 0) return {};
    return std::string(data, n);
}

}  // namespace zhinst
