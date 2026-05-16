// =============================================================================
// seqcc — ELF section reader implementation.  See header for scope.
// =============================================================================

#include "elf_reader.hpp"

#include <cstring>

namespace seqcc {

namespace {

template <typename T>
bool readLE(std::string_view buf, std::size_t offset, T& out) {
    if (offset + sizeof(T) > buf.size()) return false;
    std::memcpy(&out, buf.data() + offset, sizeof(T));
    return true;
}

}  // namespace

std::string_view ElfSections::get(std::string const& name,
                                  std::string_view buf) const {
    auto it = sections.find(name);
    if (it == sections.end()) return {};
    auto const& s = it->second;
    if (s.offset + s.size > buf.size()) return {};
    return buf.substr(static_cast<std::size_t>(s.offset),
                      static_cast<std::size_t>(s.size));
}

ElfSections parseElfSections(std::string_view buf) {
    ElfSections out;
    if (buf.size() < 52) return out;
    if (buf[0] != 0x7f || buf[1] != 'E' || buf[2] != 'L' || buf[3] != 'F') {
        return out;
    }
    std::uint8_t ei_class = static_cast<std::uint8_t>(buf[4]);
    std::uint8_t ei_data  = static_cast<std::uint8_t>(buf[5]);
    if (ei_data != 1) return out;  // we only handle little-endian

    std::uint64_t e_entry = 0, e_shoff = 0;
    std::uint16_t e_shentsize = 0, e_shnum = 0, e_shstrndx = 0;

    if (ei_class == 1) {  // ELFCLASS32
        std::uint32_t entry32 = 0, shoff32 = 0;
        if (!readLE(buf, 24, entry32)) return out;
        if (!readLE(buf, 32, shoff32)) return out;
        if (!readLE(buf, 46, e_shentsize)) return out;
        if (!readLE(buf, 48, e_shnum))     return out;
        if (!readLE(buf, 50, e_shstrndx))  return out;
        e_entry = entry32;
        e_shoff = shoff32;
    } else if (ei_class == 2) {  // ELFCLASS64
        std::uint64_t entry64 = 0, shoff64 = 0;
        if (!readLE(buf, 24, entry64)) return out;
        if (!readLE(buf, 40, shoff64)) return out;
        if (!readLE(buf, 58, e_shentsize)) return out;
        if (!readLE(buf, 60, e_shnum))     return out;
        if (!readLE(buf, 62, e_shstrndx))  return out;
        e_entry = entry64;
        e_shoff = shoff64;
    } else {
        return out;
    }

    if (e_shnum == 0 || e_shentsize == 0) return out;
    if (e_shoff + static_cast<std::uint64_t>(e_shnum) * e_shentsize > buf.size()) {
        return out;
    }
    if (e_shstrndx >= e_shnum) return out;

    // Section header fields we need: sh_name (offset 0, 4 bytes both
    // ELF32/64), sh_offset, sh_size.  Their byte offsets within the
    // section-header entry depend on EI_CLASS.
    auto readShdr = [&](std::uint64_t shoff,
                        std::uint32_t& sh_name,
                        std::uint64_t& sh_offset,
                        std::uint64_t& sh_size) -> bool {
        if (!readLE(buf, static_cast<std::size_t>(shoff), sh_name)) return false;
        if (ei_class == 1) {
            std::uint32_t off32 = 0, size32 = 0;
            if (!readLE(buf, static_cast<std::size_t>(shoff + 16), off32))  return false;
            if (!readLE(buf, static_cast<std::size_t>(shoff + 20), size32)) return false;
            sh_offset = off32;
            sh_size   = size32;
        } else {
            if (!readLE(buf, static_cast<std::size_t>(shoff + 24), sh_offset)) return false;
            if (!readLE(buf, static_cast<std::size_t>(shoff + 32), sh_size))   return false;
        }
        return true;
    };

    // First, locate .shstrtab via e_shstrndx and read it into a span.
    std::uint64_t strtab_shoff = e_shoff + static_cast<std::uint64_t>(e_shstrndx) * e_shentsize;
    std::uint32_t strtab_name = 0;
    std::uint64_t strtab_off = 0, strtab_size = 0;
    if (!readShdr(strtab_shoff, strtab_name, strtab_off, strtab_size)) return out;
    if (strtab_off + strtab_size > buf.size()) return out;
    std::string_view strtab = buf.substr(static_cast<std::size_t>(strtab_off),
                                          static_cast<std::size_t>(strtab_size));

    for (std::uint16_t i = 0; i < e_shnum; ++i) {
        std::uint64_t shoff = e_shoff + static_cast<std::uint64_t>(i) * e_shentsize;
        std::uint32_t sh_name = 0;
        std::uint64_t sh_offset = 0, sh_size = 0;
        if (!readShdr(shoff, sh_name, sh_offset, sh_size)) return out;
        if (sh_name >= strtab.size()) continue;
        auto end = strtab.find('\0', sh_name);
        std::string name(strtab.substr(sh_name,
                                       end == std::string_view::npos
                                           ? strtab.size() - sh_name
                                           : end - sh_name));
        if (name.empty()) continue;
        if (sh_offset + sh_size > buf.size()) continue;
        out.sections.emplace(std::move(name),
                             ElfSections::Slice{sh_offset, sh_size});
    }

    out.entry = e_entry;
    out.valid = true;
    return out;
}

}  // namespace seqcc
