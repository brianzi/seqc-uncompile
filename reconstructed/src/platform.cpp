// Platform utility functions implementation
// Reconstructed from _seqc_compiler.so

#include "zhinst/platform.hpp"

#include <boost/algorithm/string/classification.hpp>
#include <boost/algorithm/string/predicate.hpp>
#include <boost/algorithm/string/trim.hpp>
#include <boost/filesystem/path.hpp>
#include <boost/filesystem/operations.hpp>
#include <boost/tokenizer.hpp>

#include <cstring>
#include <fstream>
#include <locale>
#include <string>
#include <sys/stat.h>

namespace zhinst {

// @0x2ec6e0 — returns SSO string "linux64" (7 chars, length tag 0x0e)
std::string getPlatformName() {
    return "linux64";
}

// ---------------------------------------------------------------------------
// canCreateFileForWriting — @0x2eb860
// Helper: tries to open `path` for writing (ios::out). If open succeeds
// (rdstate & 0x5 == 0, i.e. no failbit|badbit), removes the file and
// returns true. Otherwise returns false without removing.
// ---------------------------------------------------------------------------
namespace {
bool canCreateFileForWriting(boost::filesystem::path const& p) {
    // @0x2eb8b6: open with ios::out (mode 0x10)
    std::ofstream ofs(p.string().c_str(), std::ios::out);
    auto state = ofs.rdstate();
    ofs.close();

    bool ok = !(state & (std::ios::failbit | std::ios::badbit));
    if (ok) {
        // @0x2eb910: remove the test file on success
        boost::filesystem::remove(p);
    }
    return ok;
}
} // anonymous namespace

// ---------------------------------------------------------------------------
// isDirectoryWriteable — @0x2cdda0, ~0xDD bytes
// Appends "Info.txt" to the directory path, then calls
// canCreateFileForWriting on the result.
// ---------------------------------------------------------------------------
bool isDirectoryWriteable(boost::filesystem::path const& dir) { // @0x2cdda0
    // @0x2cddd8: "Info.txt" (0x7478742e6f666e49 little-endian)
    boost::filesystem::path testPath = dir / "Info.txt";
    return canCreateFileForWriting(testPath);
}

// ---------------------------------------------------------------------------
// isMountPoint — @0x2eb310, ~0x23D bytes
// stat(path) must succeed and be a directory (S_ISDIR).
// Then stat(path / "..") and compare st_dev and st_ino:
//   if st_dev differs OR st_ino matches → it's a mount point.
// ---------------------------------------------------------------------------
bool isMountPoint(std::string const& path) { // @0x2eb310
    struct stat st1;
    // @0x2eb37c: __xstat(1, path.c_str(), &st1)
    if (::stat(path.c_str(), &st1) != 0)
        return false;
    // @0x2eb38a: check S_ISDIR (st_mode & 0xF000 == 0x4000)
    if (!S_ISDIR(st1.st_mode))
        return false;

    // @0x2eb3f5: append ".." via boost::filesystem path_algorithms
    boost::filesystem::path parent = boost::filesystem::path(path) / "..";

    struct stat st2;
    // @0x2eb49d: __xstat(1, parent.c_str(), &st2)
    if (::stat(parent.string().c_str(), &st2) != 0)
        return false;

    // @0x2eb4b9..2eb4db: compare st_dev and st_ino
    // Different device → mount point; same device + same inode → mount point
    return (st1.st_dev != st2.st_dev) || (st1.st_ino == st2.st_ino);
}

// ---------------------------------------------------------------------------
// isPureAscii — @0x2f85d0, ~0x48 bytes
// Simple byte scan: returns false if any byte has high bit set (>= 0x80).
// Binary uses `cmpb $0x0, (%rax); js` which triggers on sign bit.
// ---------------------------------------------------------------------------
bool isPureAscii(std::string const& s) { // @0x2f85d0
    for (char c : s) {
        if (static_cast<unsigned char>(c) >= 0x80)
            return false;
    }
    return true;
}

// ---------------------------------------------------------------------------
// isValidUtf8 (iterator range) — @0x2f8330, ~0x136 bytes
// Validates UTF-8 encoding by checking lead-byte patterns and that each
// continuation byte is <= 0xBF. Accepts:
//   0xxxxxxx      → 1 byte
//   110xxxxx 10xx → 2 bytes
//   1110xxxx 10xx 10xx → 3 bytes
//   11110xxx 10xx 10xx 10xx → 4 bytes
// Returns false on any invalid sequence or truncated multi-byte.
// ---------------------------------------------------------------------------
bool isValidUtf8(const char* begin, const char* end) { // @0x2f8330
    const char* p = begin;
    while (p < end) {
        unsigned char c = static_cast<unsigned char>(*p);
        if (c < 0x80) {
            // @0x2f8375: single ASCII byte
            ++p;
            continue;
        }

        int expected;
        if ((c & 0xE0) == 0xC0) {
            // @0x2f83ba: 2-byte sequence (110xxxxx)
            expected = 2;
        } else if ((c & 0xF0) == 0xE0) {
            // @0x2f83a7: 3-byte sequence (1110xxxx)
            expected = 3;
        } else if ((c & 0xF8) == 0xF0) {
            // @0x2f83dc: 4-byte sequence (11110xxx)
            expected = 4;
        } else {
            // @0x2f83d6: invalid lead byte
            return false;
        }

        // Check continuation bytes (each must be <= 0xBF)
        for (int i = 1; i < expected; ++i) {
            if (p + i >= end)
                return (p + i == end); // partial sequence at end
            if (static_cast<signed char>(*(p + i)) > static_cast<signed char>(0xBF))
                return false;
        }
        p += expected;
    }
    return true;
}

// ---------------------------------------------------------------------------
// isValidUtf8 (string overload) — @0x2f8470, ~0x15C bytes
// Inlined copy of the iterator-range logic operating on the string's
// data()/data()+size() range. In practice identical to calling the
// iterator-range overload.
// ---------------------------------------------------------------------------
bool isValidUtf8(std::string const& s) { // @0x2f8470
    return isValidUtf8(s.data(), s.data() + s.size());
}

// ---------------------------------------------------------------------------
// isInList — @0x2f2870, ~0x590 bytes
// Tokenizes `list` on ',' using boost::tokenizer<char_separator>.
// For each token, trims whitespace (boost::algorithm::trim_copy_if with
// std::isspace classification), then compares case-insensitively
// (boost::algorithm::iequals) against `item`.
// Returns true on the first match.
// ---------------------------------------------------------------------------
bool isInList(std::string const& item, std::string const& list) { // @0x2f2870
    // @0x28a3: separator is ',' (0x2c)
    boost::char_separator<char> sep(",");
    boost::tokenizer<boost::char_separator<char>> tokens(list, sep);

    for (auto const& tok : tokens) {
        // @0x2aae: trim_copy_if with is_space classification
        std::string trimmed = boost::algorithm::trim_copy(tok);
        // @0x2ae0: boost::algorithm::iequals
        if (boost::algorithm::iequals(trimmed, item))
            return true;
    }
    return false;
}

} // namespace zhinst
