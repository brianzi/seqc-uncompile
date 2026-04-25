// CalVer — calendar-versioning class
// Reconstructed from _seqc_compiler.so

#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>

namespace zhinst {

// Layout: 4 x size_t = 32 bytes (0x20) on x86-64
//   +0x00  size_t year_
//   +0x08  size_t month_
//   +0x10  size_t patch_
//   +0x18  size_t build_
struct CalVer {
    size_t year_;   // +0x00
    size_t month_;  // +0x08
    size_t patch_;  // +0x10
    size_t build_;  // +0x18

    // Default ctor — zero-initializes all fields
    CalVer() : year_(0), month_(0), patch_(0), build_(0) {}

    // Parsing ctor: parses "YY.MM.PATCH" or "YY.MM.PATCH.BUILD"
    // If exactly 3 dots → splits on last dot, calls extractVersionTriple()
    //   on the prefix, then boost::lexical_cast<size_t> on the suffix (build).
    // If != 3 dots → calls extractVersionTriple() on the whole string,
    //   sets build_ = 0.
    // @0x0ffdb0, size 0x470 bytes
    explicit CalVer(std::string const& s);

    // Accessors — trivial field reads
    size_t year()  const;  // @0x100220, 9 bytes
    size_t month() const;  // @0x100230, 10 bytes
    size_t patch() const;  // @0x100240, 10 bytes
    size_t build() const;  // @0x100250, 10 bytes

    // Returns *this by pointer cast — the first 3 fields (year_, month_, patch_)
    // are laid out identically to std::array<size_t, 3>.
    // Caller reinterprets the returned CalVer* as array<size_t,3> const&.
    // @0x100260, 9 bytes
    CalVer const& triple() const;
};

// --- Free functions ---

// Returns a compile-time-embedded CalVer {26, 1, 3, 9}
// i.e. version 26.01.3 build 9
// @0x100270, 0x28 bytes
CalVer getLaboneVersion();

// True if any field is non-zero.
// @0x100470, 0x18 bytes
bool isSet(CalVer const& v);

// Packs into decimal: (year%100 * 100 + month%100) * 100000 + patch%10 * 10000 + build%10000
// Encoding: YYMM_P_BBBB  (YY*100+MM)*100000 + patch*10000 + build
// @0x1006e0, 0x9c bytes
uint32_t asDecimal(CalVer const& v);

// Packs into binary: year<<24 | month<<16 | (patch&0xF)<<12 | (build&0xFFF)
// @0x1007c0, 0x2b bytes
uint32_t asBinary(CalVer const& v);

// Produces "YEAR.MONTH.PATCH" (3-component string via std::to_string + ".")
// @0x1007f0, 0x172 bytes
std::string toString(CalVer const& v);

// Lexicographic comparison on (year, month, patch, build)
// @0x100bc0, 0x3d bytes
bool operator==(CalVer const& a, CalVer const& b);

// @0x100c00, 0x3e bytes
bool operator<(CalVer const& a, CalVer const& b);

// Unpacks decimal → CalVer
// @0x100490
CalVer fromDecimal(uint32_t d);

// Unpacks binary → CalVer
// @0x100780
CalVer fromBinary(uint32_t b);

// Returns "26.01.3.9 (<commit_hash>)" — compile-time embedded
// @0x1002a0
std::string getLaboneVersionWithCommitHash();

} // namespace zhinst
