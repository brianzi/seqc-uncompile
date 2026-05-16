// =============================================================================
// seqcc — minimal 32-bit/64-bit LE ELF section reader
//
// The SeqC compiler emits 32-bit LE ELFs (the assembler personality
// emits 64-bit ELFs from the same encoder).  This reader only needs
// to extract named sections by string-table name; it does not
// validate signatures, relocations, segments, or program headers.
//
// Format reference: reconstructed/notes/elf_format.md.
// =============================================================================
#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_map>

namespace seqcc {

//! Parsed view over an ELF byte buffer.
struct ElfSections {
    //! Map from section name (e.g. ".asm") to (offset, size) into the
    //! original buffer.  Empty when parse failed.
    struct Slice { std::uint64_t offset = 0; std::uint64_t size = 0; };
    std::unordered_map<std::string, Slice> sections;
    std::uint64_t entry = 0;
    bool valid = false;

    //! Return a string_view into `buf` for the named section, or
    //! empty view if absent.  The caller must keep `buf` alive for
    //! the duration of the view.
    std::string_view get(std::string const& name,
                         std::string_view buf) const;
};

//! Parse `buf` as either ELFCLASS32 or ELFCLASS64 LE.  Returns an
//! `ElfSections` with `valid=false` on any structural problem
//! (truncated buffer, bad e_ident, OOB headers).  Does not throw.
ElfSections parseElfSections(std::string_view buf);

}  // namespace seqcc
