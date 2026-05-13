// CalVer — calendar-versioning class implementation
// Reconstructed from _seqc_compiler.so

#include "zhinst/infra/calver.hpp"
#include "zhinst/core/exception.hpp"

#include <boost/algorithm/string/classification.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/throw_exception.hpp>
#include <array>
#include <cstdint>
#include <ostream>
#include <string>
#include <vector>

namespace zhinst {

// ---------------------------------------------------------------------------
// extractVersionTriple(string const&) — @0x101570, 0x590 bytes
//
// Splits the input on '.' via boost::algorithm::split (token_compress_on),
// then parses up to 3 components via boost::lexical_cast<size_t>.
//
// Returns:
//   0 parts → {0, 0, 0}
//   1 part  → {parts[0], 0, 0}
//   2 parts → {parts[0], parts[1], 0}
//   3+ parts → {parts[0], parts[1], parts[2]}
//
// Throws boost::bad_lexical_cast if any component is not a valid integer.
// ---------------------------------------------------------------------------
std::array<size_t, 3> extractVersionTriple(std::string const& s) {
    std::vector<std::string> parts;
    boost::algorithm::split(parts, s, boost::is_any_of("."),
                            boost::token_compress_on);

    std::array<size_t, 3> result = {{0, 0, 0}};

    size_t n = parts.size();
    if (n == 0)
        return result;

    // Parse each available component
    if (n >= 1)
        result[0] = boost::lexical_cast<size_t>(parts[0]);
    if (n >= 2)
        result[1] = boost::lexical_cast<size_t>(parts[1]);
    if (n >= 3)
        result[2] = boost::lexical_cast<size_t>(parts[2]);

    return result;
}

// ---------------------------------------------------------------------------
// CalVer::CalVer(string const&) — @0x0ffdb0, 0x470 bytes
//
// Algorithm:
//   1. Zero-initialize all 4 fields.
//   2. Count '.' characters in the input string.
//      - Uses SIMD (pcmpeqb) for strings >= 4 chars, scalar fallback otherwise.
//   3. If dotCount == 3 → 4-component format "YY.MM.PATCH.BUILD":
//      a. Find last '.' by scanning backwards.
//      b. Call extractVersionTriple() on the prefix (up to last dot).
//      c. Parse the suffix after the last dot via boost::lexical_cast<size_t>
//         → build_.
//   4. If dotCount != 3 → 3-component format "YY.MM.PATCH":
//      a. Call extractVersionTriple() on the full string.
//      b. build_ = 0.
// ---------------------------------------------------------------------------
CalVer::CalVer(std::string const& s)
    : year_(0), month_(0), patch_(0), build_(0)
{
    if (s.empty())
        return;

    // Count dots
    size_t dotCount = 0;
    for (size_t i = 0; i < s.size(); ++i) {
        if (s[i] == '.')
            ++dotCount;
    }

    if (dotCount == 3) {
        // 4-component: "YY.MM.PATCH.BUILD"
        // Find last '.'
        size_t lastDot = s.size();
        while (lastDot > 0) {
            --lastDot;
            if (s[lastDot] == '.')
                break;
        }

        // extractVersionTriple on prefix
        std::string prefix = s.substr(0, lastDot);
        auto triple = extractVersionTriple(prefix);
        year_  = triple[0];
        month_ = triple[1];
        patch_ = triple[2];

        // Parse build from suffix after last dot
        std::string suffix = s.substr(lastDot + 1);
        build_ = boost::lexical_cast<size_t>(suffix);  // @0x100082..0x100154
    } else {
        // 3-component or fallback
        auto triple = extractVersionTriple(s);
        year_  = triple[0];
        month_ = triple[1];
        patch_ = triple[2];
        build_ = 0;
    }
}

// @0x100220, 9 bytes
size_t CalVer::year() const {
    return year_;
}

// @0x100230, 10 bytes
size_t CalVer::month() const {
    return month_;
}

// @0x100240, 10 bytes
size_t CalVer::patch() const {
    return patch_;
}

// @0x100250, 10 bytes
size_t CalVer::build() const {
    return build_;
}

// @0x100260, 9 bytes
// Returns *this — the caller treats the first 3 fields as array<size_t,3>.
CalVer const& CalVer::triple() const {
    return *this;
}

// ---------------------------------------------------------------------------
// getLaboneVersion() — @0x100270, 0x28 bytes
// Returns a compile-time constant CalVer.
// Disassembly:
//   mov QWORD PTR [rdi],    0x1a   → year  = 26
//   mov QWORD PTR [rdi+8],  0x01   → month = 1
//   mov QWORD PTR [rdi+16], 0x03   → patch = 3
//   mov QWORD PTR [rdi+24], 0x09   → build = 9
// ---------------------------------------------------------------------------
CalVer getLaboneVersion() {
    CalVer v;
    v.year_  = 26;
    v.month_ = 1;
    v.patch_ = 3;
    v.build_ = 9;
    return v;
}

// ---------------------------------------------------------------------------
// isSet(CalVer const&) — @0x100470, 0x18 bytes
// True if any field is non-zero.
// Disassembly: or's all 4 fields together, setne.
// ---------------------------------------------------------------------------
bool isSet(CalVer const& v) {  // @0x100470
    return (v.year_ | v.month_ | v.patch_ | v.build_) != 0;
}

// ---------------------------------------------------------------------------
// asDecimal(CalVer const&) — @0x1006e0, 0x9c bytes
//
// Packs CalVer into a single uint32_t decimal encoding:
//   result = (year%100 * 100 + month%100) * 100000
//          + (patch%10) * 10000
//          + (build%10000)
//
// The disassembly uses magic-number multiply-shift sequences for the
// modular reductions (compiler strength-reduction of %, /).
// ---------------------------------------------------------------------------
uint32_t asDecimal(CalVer const& v) {  // @0x1006e0
    uint32_t yy = static_cast<uint32_t>(v.year_  % 100);
    uint32_t mm = static_cast<uint32_t>(v.month_ % 100);
    uint32_t pp = static_cast<uint32_t>(v.patch_ % 10);
    uint32_t bb = static_cast<uint32_t>(v.build_ % 10000);
    return (yy * 100 + mm) * 100000 + pp * 10000 + bb;
}

// ---------------------------------------------------------------------------
// asBinary(CalVer const&) — @0x1007c0, 0x2b bytes
//
// Packs: year<<24 | (month & 0xFF)<<16 | (patch & 0xF)<<12 | (build & 0xFFF)
// Note: movzx cl at month step masks to 8 bits; movzx dx at patch masks to
// 16 bits (binary uses 12-bit shift so only low 4 bits of patch survive).
// ---------------------------------------------------------------------------
uint32_t asBinary(CalVer const& v) {  // @0x1007c0
    uint32_t r = static_cast<uint32_t>(v.year_) << 24;
    r |= (static_cast<uint32_t>(v.month_) & 0xFF) << 16;
    r |= (static_cast<uint32_t>(v.patch_) << 12) & 0xFFFF;
    r |= static_cast<uint32_t>(v.build_) & 0xFFF;
    return r;
}

// ---------------------------------------------------------------------------
// toString(CalVer const&) — @0x1007f0, 0x172 bytes
//
// Produces "YEAR.MONTH.PATCH" via:
//   std::to_string(year) + "." + std::to_string(month) + "." + std::to_string(patch)
//
// Static data: "." at 0x8fde52
// ---------------------------------------------------------------------------
std::string toString(CalVer const& v) {  // @0x1007f0
    return std::to_string(v.year_) + "." +
           std::to_string(v.month_) + "." +
           std::to_string(v.patch_);
}

// ---------------------------------------------------------------------------
// operator==(CalVer const&, CalVer const&) — @0x100bc0, 0x3d bytes
//
// SIMD-accelerated: compares first 24 bytes (year+month+patch) with
// pcmpeqb + pmovmskb, then scalar compare of build_.
// Semantically: all 4 fields must match.
// ---------------------------------------------------------------------------
bool operator==(CalVer const& a, CalVer const& b) {  // @0x100bc0
    return a.year_  == b.year_
        && a.month_ == b.month_
        && a.patch_ == b.patch_
        && a.build_ == b.build_;
}

// ---------------------------------------------------------------------------
// operator<(CalVer const&, CalVer const&) — @0x100c00, 0x3e bytes
//
// Lexicographic: year, then month, then patch, then build.
// Disassembly: cascading comparisons with early-exit on inequality.
// ---------------------------------------------------------------------------
bool operator<(CalVer const& a, CalVer const& b) {  // @0x100c00
    if (a.year_ != b.year_)   return a.year_  < b.year_;
    if (a.month_ != b.month_) return a.month_ < b.month_;
    if (a.patch_ != b.patch_) return a.patch_ < b.patch_;
    return a.build_ < b.build_;
}

// ---------------------------------------------------------------------------
// fromBinary(uint32_t) — @0x100780, 0x33 bytes
//
// Inverse of asBinary:
//   year  = b >> 24
//   month = (b >> 16) & 0xFF
//   patch = (b >> 12) & 0xF
//   build = b & 0xFFF
// ---------------------------------------------------------------------------
CalVer fromBinary(uint32_t b) {  // @0x100780
    CalVer v;
    v.year_  = b >> 24;
    v.month_ = (b >> 16) & 0xFF;
    v.patch_ = (b >> 12) & 0xF;
    v.build_ = b & 0xFFF;
    return v;
}

// ---------------------------------------------------------------------------
// fromDecimal(uint32_t) — @0x100490, 0x8d bytes
//
// Inverse of asDecimal. Uses magic-number division sequences.
//   year  = (d / 10000000) % 100
//   month = (d / 100000) % 100
//   patch = (d / 10000) % 10
//   build = d % 10000
// ---------------------------------------------------------------------------
CalVer fromDecimal(uint32_t d) {  // @0x100490
    CalVer v;
    v.year_  = (d / 10000000) % 100;
    v.month_ = (d / 100000)   % 100;
    v.patch_ = (d / 10000)    % 10;
    v.build_ = d % 10000;
    return v;
}

// ---------------------------------------------------------------------------
// getLaboneVersionWithCommitHash() — @0x1002a0, 0x14f bytes
//
// Returns "26.01.3.9 (203353afa6d977a08b0d4178e005ccfb3132992e)"
//
// The version string "26.01.3.9" is built inline from immediate bytes:
//   0x12 = SSO length 9; bytes "26.01.3.9\0"
// The commit hash is a 40-char hex string loaded from immediates at 0x8fecfd.
// Separator " (" at 0x8ff092, closing ")" at 0x8ff095.
// ---------------------------------------------------------------------------
std::string getLaboneVersionWithCommitHash() {  // @0x1002a0
    // Version "26.01.3.9" + " (" + 40-char commit hash + ")"
    // Hash at rodata 0x8fecfd: "203353afa6d977a08b0d4178e005ccfb3132992e"
    return std::string("26.01.3.9 (203353afa6d977a08b0d4178e005ccfb3132992e)");
}

// ---------------------------------------------------------------------------
// fromDecimal(string const&) — @0x100520, 0xc0 bytes
//
// Parses s as decimal via std::stoul, then forwards to fromDecimal(uint32_t).
// Empty string short-circuits to the all-zero CalVer.
// ---------------------------------------------------------------------------
CalVer fromDecimal(std::string const& s) {  // @0x100520
    if (s.empty()) {
        return CalVer{};
    }
    return fromDecimal(static_cast<uint32_t>(std::stoul(s, nullptr, 10)));
}

// ---------------------------------------------------------------------------
// operator<<(ostream&, CalVer const&) — @0x100b40, 0x80 bytes
//
// Equivalent to: os << toString(v).
// ---------------------------------------------------------------------------
std::ostream& operator<<(std::ostream& os, CalVer const& v) {  // @0x100b40
    return os << toString(v);
}

// ---------------------------------------------------------------------------
// toString(array<size_t,3> const&) — @0x101d80, 0x350 bytes
//
// "a[0].a[1].a[2]" via std::to_string + ".".
// ---------------------------------------------------------------------------
std::string toString(std::array<size_t, 3> const& a) {  // @0x101d80
    return std::to_string(a[0]) + "." +
           std::to_string(a[1]) + "." +
           std::to_string(a[2]);
}

// ---------------------------------------------------------------------------
// isSet(array<size_t,3> const&) — @0x1020d0, 0x18 bytes
//
// True iff any of the three components is non-zero.
// Binary form: (a[0] | a[1] | a[2]) != 0.
// ---------------------------------------------------------------------------
bool isSet(std::array<size_t, 3> const& a) {  // @0x1020d0
    return (a[0] | a[1] | a[2]) != 0;
}

} // namespace zhinst
