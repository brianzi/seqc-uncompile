// Platform utility functions implementation
// Reconstructed from _seqc_compiler.so

#include "zhinst/core/platform.hpp"

#include <boost/algorithm/string/classification.hpp>
#include <boost/algorithm/string/predicate.hpp>
#include <boost/algorithm/string/replace.hpp>
#include <boost/algorithm/string/trim.hpp>
#include <boost/filesystem/path.hpp>
#include <boost/filesystem/operations.hpp>
#include <boost/tokenizer.hpp>

#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>

#include <cstring>
#include <fstream>
#include <locale>
#include <sstream>
#include <string>
#include <sys/stat.h>

namespace zhinst {

// =========================================================================
// Anonymous-namespace helpers for manifest / device-identity queries
// =========================================================================
namespace {

// ---------------------------------------------------------------------------
// readManifest(const string&) — @0x2ec210, ~0x3D0 bytes
// Reads a JSON manifest file. If the file does not exist (filesystem status
// <= 1 = file_not_found | status_error), returns an empty ptree.
// Otherwise parses via boost::property_tree::json_parser::read_json.
// ---------------------------------------------------------------------------
boost::property_tree::ptree readManifest(std::string const& path) {  // @0x2ec210
    boost::property_tree::ptree pt;
    // @0x2ec2b0: boost::filesystem::detail::status() — returns file_type enum
    // @0x2ec360: cmp $0x1, %r15d; jbe → file_not_found or status_error → return empty
    auto st = boost::filesystem::status(boost::filesystem::path(path));
    if (st.type() <= boost::filesystem::regular_file) {
        // file_not_found(1) or status_error(0) — return empty ptree
        return pt;
    }
    boost::property_tree::json_parser::read_json(path, pt);  // @0x2ec3e0
    return pt;
}

// ---------------------------------------------------------------------------
// readManifest() — @0x2ec5e0, ~0x120 bytes
// Lazy-init function-local static. Calls readManifest("/opt/zi/LabOne/manifest.json")
// once, caches in static, registers ~ptree with __cxa_atexit.
// Guard variable at 0xb852d0 in binary.
// ---------------------------------------------------------------------------
boost::property_tree::ptree const& readManifest() {  // @0x2ec5e0
    static boost::property_tree::ptree manifest =
        readManifest("/opt/zi/LabOne/manifest.json");  // @0x90ce08 in rodata
    return manifest;
}

// ---------------------------------------------------------------------------
// doIsMf(const ptree&) — @0x2ec700, ~0x1E0 bytes
// Gets ptree.get<string>("device", ""), checks length==2 and content=="mf"
// (0x666d as 16-bit compare). Uses SSO string operations.
// ---------------------------------------------------------------------------
bool doIsMf(boost::property_tree::ptree const& pt) {  // @0x2ec700
    std::string device = pt.get<std::string>("device", "");  // @0x2ec790
    return device.size() == 2 && device == "mf";             // @0x2ec7da: cmp $0x2 + cmpw $0x666d
}

// ---------------------------------------------------------------------------
// isMf(const ptree&) — @0x2ec1e0, ~0x30 bytes
// Wraps doIsMf in try/catch; returns false on any exception.
// ---------------------------------------------------------------------------
bool isMf(boost::property_tree::ptree const& pt) {  // @0x2ec1e0
    try {
        return doIsMf(pt);
    } catch (...) {
        return false;
    }
}

// ---------------------------------------------------------------------------
// isMf64(const ptree&) — @0x2ec430, ~0x1B0 bytes
// First checks doIsMf(). If not MF, returns false. Then checks
// ptree.get<string>("platform", "") has length 10 (the string "mf_dev5640").
// Same try/catch pattern as isMf.
// ---------------------------------------------------------------------------
bool isMf64(boost::property_tree::ptree const& pt) {  // @0x2ec430
    try {
        if (!doIsMf(pt))
            return false;
        std::string platform = pt.get<std::string>("platform", "");  // @0x2ec520
        return platform.size() == 10;  // @0x2ec560: cmp $0xa — "mf_dev5640" is 10 chars
    } catch (...) {
        return false;
    }
}

} // anonymous namespace

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

// ---------------------------------------------------------------------------
// quote — @0x2fa6a0, ~320 bytes
// Escapes embedded double-quotes, then wraps string in "...".
// Binary uses boost::algorithm::replace_all for the escape pass.
// ---------------------------------------------------------------------------
void quote(std::string& s) {  // @0x2fa6a0
    boost::algorithm::replace_all(s, "\"", "\\\"");
    s.insert(s.begin(), '"');
    s.push_back('"');
}

// ---------------------------------------------------------------------------
// toSubscript(string) — @0x2fd960, ~544 bytes
// Converts each digit character to its HTML subscript entity (&#8320;..&#8329;).
// Non-digit characters are skipped.
// Binary uses a jump table at 0x9646d0 mapping digit index to entity strings.
// ---------------------------------------------------------------------------
std::string toSubscript(std::string const& s) {  // @0x2fd960
    static const char* const table[] = {
        "&#8320;", "&#8321;", "&#8322;", "&#8323;", "&#8324;",
        "&#8325;", "&#8326;", "&#8327;", "&#8328;", "&#8329;"
    };
    std::ostringstream oss;
    for (char c : s) {
        unsigned d = static_cast<unsigned char>(c) - '0';
        if (d < 10) {
            oss << table[d];
        }
    }
    return oss.str();
}

// ---------------------------------------------------------------------------
// toSubscript(long) — @0x2fdb80, ~112 bytes
// Converts long to string, then delegates to toSubscript(string).
// ---------------------------------------------------------------------------
std::string toSubscript(long n) {  // @0x2fdb80
    return toSubscript(std::to_string(n));
}

// ---------------------------------------------------------------------------
// toSuperscript — @0x2fd730, ~560 bytes
// Converts digits, '+', '-', '.' to their HTML superscript entities.
// Binary uses index = (unsigned char)(c + 0xD5) with bitmask 0x7FED
// to select valid entries from a 15-element table.
// ---------------------------------------------------------------------------
std::string toSuperscript(std::string const& s) {  // @0x2fd730
    static const char* const digitTable[] = {
        "&#8304;", "&#185;",  "&#178;",  "&#179;",  "&#8308;",
        "&#8309;", "&#8310;", "&#8311;", "&#8312;", "&#8313;"
    };
    std::ostringstream oss;
    for (char c : s) {
        if (c >= '0' && c <= '9')
            oss << digitTable[c - '0'];
        else if (c == '+')
            oss << "&#8314;";
        else if (c == '-')
            oss << "&#8315;";
        else if (c == '.')
            oss << "&#183;";
    }
    return oss.str();
}

// ---------------------------------------------------------------------------
// escapeStringForMatlab — @0x2f9110, ~1648 bytes
// Escapes special characters for MATLAB string representation.
// Binary performs 10 sequential boost::algorithm::replace_all passes:
//   1. \ → \\    (backslash doubling, must be first)
//   2. ' → ''    (MATLAB single-quote doubling)
//   3. % → %%    (MATLAB percent doubling)
//   4-10. Control chars: \a, \b, \f, \n, \r, \t, \v → their C escape strings
// ---------------------------------------------------------------------------
std::string escapeStringForMatlab(std::string s) {  // @0x2f9110
    boost::algorithm::replace_all(s, "\\", "\\\\");
    boost::algorithm::replace_all(s, "'", "''");
    boost::algorithm::replace_all(s, "%", "%%");
    boost::algorithm::replace_all(s, std::string(1, '\a'), "\\a");
    boost::algorithm::replace_all(s, std::string(1, '\b'), "\\b");
    boost::algorithm::replace_all(s, std::string(1, '\f'), "\\f");
    boost::algorithm::replace_all(s, std::string(1, '\n'), "\\n");
    boost::algorithm::replace_all(s, std::string(1, '\r'), "\\r");
    boost::algorithm::replace_all(s, std::string(1, '\t'), "\\t");
    boost::algorithm::replace_all(s, std::string(1, '\v'), "\\v");
    return s;
}

// ---------------------------------------------------------------------------
// xmlEscapeSeqToInt — @0x2fc280, ~280 bytes
// Parses an XML numeric character reference from [begin, end).
// If the range contains 'x' or 'X', parses the remainder as hexadecimal;
// otherwise parses as decimal. Uses std::istringstream.
// ---------------------------------------------------------------------------
int xmlEscapeSeqToInt(std::string::const_iterator begin,
                      std::string::const_iterator end) {  // @0x2fc280
    // @0x2fc2c0: search for 'x' or 'X' in range
    auto it = begin;
    bool isHex = false;
    for (auto p = begin; p != end; ++p) {
        if (*p == 'x' || *p == 'X') {
            isHex = true;
            it = p + 1;  // skip past the 'x'/'X'
            break;
        }
    }

    int result = 0;
    std::string digits(isHex ? it : begin, end);
    std::istringstream iss(digits);
    if (isHex) {
        iss >> std::hex >> result;   // @0x2fc2f0: hex stringstream parse
    } else {
        iss >> std::dec >> result;   // @0x2fc320: decimal stringstream parse
    }
    return result;
}

} // namespace zhinst
