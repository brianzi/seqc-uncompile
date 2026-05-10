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
//! Calendar-versioning value: a four-component LabOne version number
//! (`year`, `month`, `patch`, `build`) with parsing, comparison, and
//! pack/unpack helpers.
//!
//! The string form is `"YY.MM.PATCH"` or `"YY.MM.PATCH.BUILD"`; missing
//! `build` parses as zero. Two pack formats are provided: `asDecimal()`
//! produces a human-readable `YYMM_P_BBBB` integer, while `asBinary()`
//! produces a tightly bit-packed `uint32_t`. Lexicographic ordering
//! across all four fields is exposed via `operator==` and `operator<`.
//!
//! Use `getLaboneVersion()` to obtain the build's compile-time-embedded
//! version, or `getLaboneVersionWithCommitHash()` for the same value
//! formatted alongside the source commit hash.
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

//! \brief Returns the build's embedded LabOne version.
//! \details Yields a `CalVer` whose four fields are baked in at compile
//! time (currently `year=26`, `month=1`, `patch=3`, `build=9`,
//! i.e. version `26.01.3.9`). The value is hard-coded — no runtime
//! configuration, environment, or file is consulted.
//! \return The compile-time `CalVer` for this build.
// Returns a compile-time-embedded CalVer {26, 1, 3, 9}
// i.e. version 26.01.3 build 9
// @0x100270, 0x28 bytes
CalVer getLaboneVersion();

//! \brief Tests whether a `CalVer` carries any non-zero component.
//! \details Returns `true` if at least one of `year`, `month`, `patch`,
//! or `build` is non-zero; returns `false` only for the all-zero value
//! produced by the default constructor. Useful as a "has been
//! initialised / parsed" check.
//! \param v Version value to test.
//! \return `true` if any component is non-zero, `false` otherwise.
// True if any field is non-zero.
// @0x100470, 0x18 bytes
bool isSet(CalVer const& v);

//! \brief Packs a `CalVer` into a human-readable decimal `uint32_t`.
//! \details Encoding is `YYMMPBBBB`, computed as
//! `((year % 100) * 100 + (month % 100)) * 100000 + (patch % 10) * 10000
//! + (build % 10000)`. Each component is reduced modulo its field width
//! before packing, so values that overflow their slot wrap silently —
//! `patch` is single-digit and `build` is four-digit in this encoding.
//! Inverse of `fromDecimal()`.
//! \param v Version value to encode.
//! \return Packed decimal representation.
// Packs into decimal: (year%100 * 100 + month%100) * 100000 + patch%10 * 10000 + build%10000
// Encoding: YYMM_P_BBBB  (YY*100+MM)*100000 + patch*10000 + build
// @0x1006e0, 0x9c bytes
uint32_t asDecimal(CalVer const& v);

//! \brief Packs a `CalVer` into a bit-packed `uint32_t`.
//! \details Layout is
//! `(year << 24) | ((month & 0xFF) << 16) | ((patch & 0xF) << 12) | (build & 0xFFF)`.
//! `year` occupies the top 8 bits without masking; `month` is masked to
//! 8 bits, `patch` to 4 bits, `build` to 12 bits. Inverse of
//! `fromBinary()`.
//! \param v Version value to encode.
//! \return Bit-packed binary representation.
// Packs into binary: year<<24 | month<<16 | (patch&0xF)<<12 | (build&0xFFF)
// @0x1007c0, 0x2b bytes
uint32_t asBinary(CalVer const& v);

//! \brief Formats a `CalVer` as the three-component string `"year.month.patch"`.
//! \details Concatenates `std::to_string(year) + "." +
//! std::to_string(month) + "." + std::to_string(patch)`. The `build`
//! field is **not** included, so the output is lossy whenever
//! `build_ != 0`. Components are formatted without zero-padding (e.g.
//! month `1` renders as `"1"`, not `"01"`).
//! \param v Version value to format.
//! \return `"year.month.patch"` string.
// Produces "YEAR.MONTH.PATCH" (3-component string via std::to_string + ".")
// @0x1007f0, 0x172 bytes
std::string toString(CalVer const& v);

//! \brief Field-wise equality across all four `CalVer` components.
//! \details Returns `true` iff `year`, `month`, `patch`, and `build`
//! all match between `a` and `b`.
//! \param a Left-hand operand.
//! \param b Right-hand operand.
//! \return `true` if every component compares equal.
// Lexicographic comparison on (year, month, patch, build)
// @0x100bc0, 0x3d bytes
bool operator==(CalVer const& a, CalVer const& b);

//! \brief Lexicographic ordering on `(year, month, patch, build)`.
//! \details Compares `year` first, then `month`, then `patch`, then
//! `build`; returns at the first differing field. Defines a strict
//! weak order consistent with `operator==`.
//! \param a Left-hand operand.
//! \param b Right-hand operand.
//! \return `true` if `a` orders strictly before `b`.
// @0x100c00, 0x3e bytes
bool operator<(CalVer const& a, CalVer const& b);

//! \brief Unpacks a decimal-packed `uint32_t` into a `CalVer`.
//! \details Inverse of `asDecimal()`. Extracts
//! `year  = (d / 10000000) % 100`,
//! `month = (d / 100000) % 100`,
//! `patch = (d / 10000) % 10`,
//! `build = d % 10000`. Inputs whose digit groups exceed the per-field
//! width are truncated by the modulo reductions rather than rejected.
//! \param d Packed decimal value.
//! \return Decoded `CalVer`.
// Unpacks decimal → CalVer
// @0x100490
CalVer fromDecimal(uint32_t d);

//! \brief Unpacks a bit-packed `uint32_t` into a `CalVer`.
//! \details Inverse of `asBinary()`. Extracts `year = b >> 24`,
//! `month = (b >> 16) & 0xFF`, `patch = (b >> 12) & 0xF`,
//! `build = b & 0xFFF`.
//! \param b Bit-packed binary value.
//! \return Decoded `CalVer`.
// Unpacks binary → CalVer
// @0x100780
CalVer fromBinary(uint32_t b);

//! \brief Returns the build's LabOne version paired with its source commit hash.
//! \details Formats the compile-time-embedded version and commit hash
//! as `"year.month.patch.build (<40-char hex hash>)"`. Both the version
//! string and the hash are baked into the build; no runtime lookup is
//! performed.
//! \return Human-readable version-plus-hash string.
// Returns "26.01.3.9 (<commit_hash>)" — compile-time embedded
// @0x1002a0
std::string getLaboneVersionWithCommitHash();

//! \brief Splits a dotted version string into up to three numeric components.
//! \details Tokenises `s` on `'.'` (with adjacent dots compressed),
//! then converts the first up to three tokens via
//! `boost::lexical_cast<size_t>`. Missing trailing components default
//! to zero, so `""` yields `{0,0,0}`, `"1"` yields `{1,0,0}`, `"1.2"`
//! yields `{1,2,0}`, and any extra tokens beyond the third are ignored.
//! \param s Dotted version string (e.g. `"26.01.3"`).
//! \return Three-element array `{year, month, patch}`.
//! \throws boost::bad_lexical_cast if a present token is not a valid
//! unsigned integer.
// Splits a version string on '.' and parses up to 3 numeric components.
// @0x101570, 0x590 bytes
std::array<size_t, 3> extractVersionTriple(std::string const& s);

} // namespace zhinst
