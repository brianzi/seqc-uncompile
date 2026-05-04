// ============================================================================
// Reconstructed implementation for zhinst/elf_reader.hpp — Phase 14d.
//
// Function addresses:
//   - ElfReader::ElfReader(string)             0x2c3110
//   - ElfReader::~ElfReader()                  0x2b18c0
//   - ElfReader::isElfFile(string)             0x2c3320
//   - ElfReader::readHeader()                  0x2c3850
//   - ElfReader::getSection(string) const      0x2c4000
//   - ElfException::ElfException(string)       0x2c7a40
//   - ElfException::~ElfException()            0x2c7b60
//   - ElfException::what()                     0x2c7b40
// ============================================================================

#include "zhinst/elf_reader.hpp"

#include <fstream>
#include <string>

#include <boost/filesystem/fstream.hpp>

namespace zhinst {

// ---------------------------------------------------------------------------
// ElfException
// ---------------------------------------------------------------------------

// 0x2c7a40 — prefixes the message with the literal "ELF Exception: ".
//
// Binary detail: the prefix is materialized at construction via three
// movabs immediates (decoded as ASCII LE):
//     0x6563784520464c45  -> "ELF Exce"
//     0x6e6f697470656378  -> "xception"
//     0x203a              -> ": "
// (15 chars total, fits in libc++ SSO inline buffer at +0x09).
ElfException::ElfException(const std::string& message)
    : message_("ELF Exception: " + message) {}

// 0x2c7b60 — chain to ~bad_exception in the binary; trivial here since
// we expose std::exception as the public base.
ElfException::~ElfException() = default;

// 0x2c7b40 — returns the SSO data pointer of message_.
const char* ElfException::what() const noexcept {
    return message_.c_str();
}

// ---------------------------------------------------------------------------
// ElfReader
// ---------------------------------------------------------------------------

// 0x2c3110
//
// Sequence (matches binary):
//   1. Default-construct the ELFIO::elfio base (this happens automatically
//      via the member-initializer list of `ElfReader : private ELFIO::elfio`).
//   2. Verify the file is a valid ELF by calling isElfFile(path); throw
//      ElfException("not a valid ELF file " + path) if not.
//   3. Call ELFIO::elfio::load(path) to parse the file into our base.
//   4. Run readHeader() to scan and classify the section table.
ElfReader::ElfReader(const std::string& path)
    : ELFIO::elfio() {
    if (!isElfFile(path)) {
        throw ElfException("not a valid ELF file " + path);
    }
    load(path);    // ELFIO::elfio::load — populates header_, sections_, etc.
    readHeader();
}

// 0x2b18c0
//
// The binary's dtor frees the vector at +0x78..+0x88 (begin/end), then
// chains to ~ELFIO::elfio. With std::vector + private inheritance, both
// happen automatically by the implicit destructor.
ElfReader::~ElfReader() = default;

// 0x2c3320
//
// Opens `path` with boost::filesystem::ifstream in binary mode, reads
// the first 4 bytes, and compares to the ELF magic 0x464c457f (\x7fELF).
// Returns false (and does NOT throw) if the file can't be opened — the
// caller (the ctor) is responsible for raising ElfException with the
// path-bearing message.
bool ElfReader::isElfFile(const std::string& path) {
    boost::filesystem::ifstream stream(path, std::ios::binary);
    if (!stream.is_open()) {
        return false;
    }
    std::uint32_t magic = 0;
    stream.read(reinterpret_cast<char*>(&magic), sizeof(magic));
    // Note: the binary explicitly sets failbit via ios_base::clear(rdstate
    // | failbit) when fewer than 4 bytes are read. For source-level
    // reconstruction it suffices to compare the read bytes — short reads
    // leave `magic` zero, which won't match.
    return magic == 0x464c457f;   // "\x7fELF"
}

// 0x2c3850
//
// Walks the loaded section table and classifies each section by name:
//   - exact match ".format"     -> formatSection_
//   - prefix match ".dd"        -> ddSections_.push_back(sec)
// Sections that match neither are ignored. The binary first checks two
// header sentinels (interpreted): ELF class == ELFCLASS32 (== 1) and
// ELF encoding == ELFDATA2LSB (== 1). If either fails, the function
// returns without touching anything — ElfWriter only ever produces
// 32-bit LE files, so reading anything else means the file is from a
// foreign producer and we conservatively ignore the section table.
void ElfReader::readHeader() {
    // Access the privately-inherited elfio base. There is no public
    // accessor for the elf_header on the base class, but `get_class()`
    // and `get_encoding()` are exposed directly on `elfio`.
    if (get_class() != ELFIO::ELFCLASS32) {
        return;
    }
    if (get_encoding() != ELFIO::ELFDATA2LSB) {
        return;
    }

    for (const auto& sec_uptr : sections) {
        ELFIO::section* sec = sec_uptr.get();
        const std::string name = sec->get_name();

        if (name == ".format") {
            formatSection_ = sec;
            continue;
        }
        // Prefix match against ".dd" (3 chars).
        //
        // Binary detail: the comparison uses bcmp(name.data(), ".dd", 3)
        // with an explicit length-cap so that names shorter than 3 chars
        // never match. std::string::compare(0, 3, ".dd") gives the same
        // semantics.
        if (name.size() >= 3 && name.compare(0, 3, ".dd") == 0) {
            ddSections_.push_back(sec);
        }
    }
}

// 0x2c4000
//
// Linear name-search across the loaded section table. Throws
// ElfException("section not found: " + name) if no section matches.
//
// This function is `const`; both the iteration and the comparison are
// purely read-only against the privately-inherited elfio base.
ELFIO::section* ElfReader::getSection(const std::string& name) const {
    for (const auto& sec_uptr : sections) {
        ELFIO::section* sec = sec_uptr.get();
        if (sec->get_name() == name) {
            return sec;
        }
    }
    throw ElfException("section not found: " + name);
}

// ElfReader::getCode() @0x2c3bc0
// Reads formatSection_ data; size aligned down to multiple of 4.
ElfReader::SectionData ElfReader::getCode() const {
    SectionData result;
    if (!formatSection_) return result;
    result.sectionType = static_cast<std::uint32_t>(formatSection_->get_type());
    auto rawSize = formatSection_->get_size();
    auto alignedSize = rawSize & ~static_cast<decltype(rawSize)>(3);  // align to 4
    if (alignedSize > 0) {
        const auto* p = reinterpret_cast<const std::uint8_t*>(
            formatSection_->get_data());
        result.data.assign(p, p + alignedSize);
    }
    return result;
}

// ElfReader::getWaveform() @0x2c3d40
// Reads ddSections_[ddSectionIndex_]; size aligned down to multiple of 2.
ElfReader::SectionData ElfReader::getWaveform() const {
    SectionData result;
    if (ddSectionIndex_ >= ddSections_.size()) return result;
    auto* sec = ddSections_[ddSectionIndex_];
    if (!sec) return result;
    result.sectionType = static_cast<std::uint32_t>(sec->get_type());
    auto rawSize = sec->get_size();
    auto alignedSize = rawSize & ~static_cast<decltype(rawSize)>(1);  // align to 2
    if (alignedSize > 0) {
        const auto* p = reinterpret_cast<const std::uint8_t*>(sec->get_data());
        result.data.assign(p, p + alignedSize);
    }
    return result;
}

// ElfReader::getLineMap() @0x2c3ef0
// Parses ".linenr" section: 16-byte records → vector<Line>.
std::vector<ElfReader::Line> ElfReader::getLineMap() const {
    std::vector<Line> lines;
    ELFIO::section* sec = getSection(".linenr");

    const auto* data = reinterpret_cast<const std::uint8_t*>(sec->get_data());
    auto size = sec->get_size();
    if (size < 4) return lines;

    auto count = size / 4;  // number of uint32 words
    for (decltype(count) i = 0; i < count; i += 4) {
        Line ln;
        std::memcpy(&ln.addr, data + (i + 1) * 4, sizeof(std::uint64_t));
        std::memcpy(&ln.line, data + (i + 3) * 4, sizeof(std::uint32_t));
        lines.push_back(ln);
    }
    return lines;
}

}  // namespace zhinst
