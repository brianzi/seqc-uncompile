// SPDX-License-Identifier: BSD-3-Clause
//! \file diagnostics_text.cpp
//! \brief D16: diagnostics-text helpers.
//!
//! Reconstruction notes: see `reconstructed/notes/diagnostics_text.md`
//! for per-symbol disassembly findings, regex literals, replacement
//! tables, and pseudo-C body sketches.
//!
//! Every function in this file lacks an in-recon caller and lacks a
//! difftest path through the Python bindings.  A custom diff-test
//! harness (TODO Phase E) is required for validation.

#include "zhinst/core/diagnostics_text.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>

#include <boost/algorithm/string/find_format.hpp>
#include <boost/algorithm/string/finder.hpp>
#include <boost/algorithm/string/formatter.hpp>
#include <boost/algorithm/string/regex.hpp>
#include <boost/algorithm/string/replace.hpp>
#include <boost/filesystem.hpp>
#include <boost/format.hpp>
#include <boost/locale/utf.hpp>
#include <boost/regex.hpp>
#include <boost/throw_exception.hpp>

#include "zhinst/core/exception.hpp"
#include "zhinst/device/device_type.hpp"

namespace zhinst {

namespace bfs = boost::filesystem;

// Forward declaration: lives in core/platform.cpp.
int xmlEscapeSeqToInt(std::string::const_iterator first,
                      std::string::const_iterator last);

// =============================================================
// Reconstructed function bodies follow.
//
// Binary addresses are noted next to each definition.
// Each body's correspondence with the binary is documented in
// `reconstructed/notes/diagnostics_text.md` under the matching
// `### <symbol>` heading.
// =============================================================

// === BLOCK A: entity/unescape cluster ===
// Functions: xmlUnescape, xmlUnescapeCopy, entityNumberToTxt,
//            entityNameToNumber
// Notes: see diagnostics_text.md sections starting at line ~1564.
// (Subagent A fills in below.)

// ---------------------------------------------------------------------------
// D16 block A: xmlUnescape / xmlUnescapeCopy / entityNumberToTxt /
//              entityNameToNumber
//
// Reconstruction notes (binary disassembly, .rodata literals, algorithm
// sketches) live in `reconstructed/notes/diagnostics_text.md` at the
// line ranges cited above each function.  Every literal in the entity
// tables below was copied byte-for-byte from those notes; do NOT
// "normalise" them (e.g. the `&gt` with no trailing semicolon in the
// xmlUnescape regex is intentional and matches the binary).
//
// All four functions carry \verifyme in the public header — they have
// no in-tree callers and are tested via the diff-test harness only.
// ---------------------------------------------------------------------------

// ---- xmlUnescape — binary @0x2fadd0, 5290 B ----
//
// Notes: diagnostics_text.md lines 1564..1713.
//
// Two function-local static `boost::regex` objects (at b85308 and
// b85350 in the binary) are constructed from the SAME pattern string
// at .rodata @0x90d057.  The first regex drives the in-place decoder
// here; the second belongs to the `(anonymous namespace)`
// `escapeMaliciousXmlEscapedSequences` helper invoked as a
// post-pass.  That helper has not yet been reconstructed, so this
// implementation performs the regex-driven decode + UTF-8 encode +
// surrogate-pair fold-up, then assigns the result back to `s`
// without the secondary malicious-sequence escape pass.  See
// IF-tracker for the post-pass gap (see IF-262 logged separately).
//
// Algorithm (from notes "Body sketch", lines 1631..1689):
//   1. Iterate matches of the entity regex over [s.begin, s.end).
//   2. For each match: append pre-match literal; decode the match
//      via xmlEscapeSeqToInt; if the decoded value is a high
//      surrogate (D800..DBFF), require the very next match to be
//      immediately adjacent and decode it as the low surrogate,
//      folding the pair into a supplementary-plane code point;
//      otherwise pass the BMP code point through.
//   3. UTF-8 encode the resulting code point and append.
//   4. Append the tail after the last match.
//   5. Swap the assembled buffer back into `s`.
void xmlUnescape(std::string& s) {
    // .rodata @0x90d057 — 47 bytes incl. NUL.
    static const boost::regex re(
        "&#x[0-9a-fA-F]+;|&#[0-9]+;|&amp;|&lt;|&gt|&quot;|&apos;");

    std::string out;
    auto it  = s.cbegin();
    auto end = s.cend();

    boost::match_results<std::string::const_iterator> m;
    while (boost::regex_search(it, end, m, re)) {
        // Append literal text preceding this match.
        out.append(it, m[0].first);

        unsigned cp = static_cast<unsigned>(
            xmlEscapeSeqToInt(m[0].first, m[0].second));
        auto consumed = m[0].second;

        // High surrogate?  Try to fuse with the immediately-following
        // low surrogate sourced from the *same* entity regex.
        if ((cp & 0xFFFFF800u) == 0xD800u) {
            boost::match_results<std::string::const_iterator> m2;
            if (!boost::regex_search(consumed, end, m2, re)
                || m2[0].first != consumed) {
                throw std::logic_error(
                    "xmlUnescape: dangling high surrogate");
            }
            unsigned low = static_cast<unsigned>(
                xmlEscapeSeqToInt(m2[0].first, m2[0].second));
            if ((low & 0xFFFFFC00u) != 0xDC00u) {
                throw std::logic_error(
                    "xmlUnescape: invalid low surrogate");
            }
            cp = 0x10000u + ((cp - 0xD800u) << 10) + (low - 0xDC00u);
            consumed = m2[0].second;
        }

        // UTF-8 encode and append.
        boost::locale::utf::utf_traits<char>::encode(
            cp, std::back_inserter(out));

        it = consumed;
    }
    // Tail.
    out.append(it, end);

    s.assign(out);
}

// ---- xmlUnescapeCopy — binary @0x2fcba0, 58 B ----
//
// Notes: diagnostics_text.md lines 1714..1753.  Trivial by-value
// wrapper: mutate the local copy in place, return by move.  The
// binary hand-codes a 24-byte move-out of the by-value parameter,
// which the compiler reproduces here via NRVO + move.
std::string xmlUnescapeCopy(std::string s) {
    xmlUnescape(s);
    return s;
}

// ---- entityNumberToTxt — binary @0x2f4e90, 4853 B ----
//
// Notes: diagnostics_text.md lines 1755..1838.  Eleven hand-inlined
// passes of `boost::algorithm::replace_all`, each preceded by an
// unrolled-cmpb manual `contains` check (skipped in the recon — the
// boost finder is fast enough on absent needles and the early-out
// is purely an optimisation; observable behaviour is identical).
//
// Replacement bytes are copied byte-for-byte from the table at
// notes lines 1783..1801.  Order matters in principle (an earlier
// replacement could synthesise a later needle), but none of these
// 11 substitutions create or destroy another's search pattern, so
// the order matches the binary purely for byte-for-byte parity.
std::string entityNumberToTxt(const std::string& s) {
    std::string out = s;

    static const std::pair<const char*, const char*> kTable[] = {
        {"&#8201;", " "  },   // U+2009 THIN SPACE
        {"&#178;",  "^2" },
        {"&#179;",  "^3" },
        {"&#956;",  "m"  },   // micro sign — binary uses ASCII 'm'
        {"&#60;",   "<"  },
        {"&#62;",   ">"  },
        {"&#38;",   "&"  },
        {"&#8486;", "Ohm"},
        {"&#43;",   "+"  },
        {"&#8722;", "-"  },   // U+2212 MINUS SIGN
        {"&#215;",  "x"  },   // multiplication sign — binary uses ASCII 'x'
    };

    for (auto const& e : kTable) {
        boost::algorithm::replace_all(out, e.first, e.second);
    }
    return out;
}

// ---- entityNameToNumber — binary @0x2f4290, 3061 B ----
//
// Notes: diagnostics_text.md lines 1840..1935.  Seven hand-inlined
// `replace_all` passes, same structural pattern as
// `entityNumberToTxt`.  Tables are NOT inverses: the forward
// direction (number→text) recognises 11 entities; this backward
// direction (name→number) recognises only 7 (the curated subset of
// named entities the binary chooses to canonicalise on input).
//
// The first entry (`&amp;` → `&`) appears first because boost's
// replace_all chains forward through the string each pass; later
// entries never contain `&amp;` as a substring, so order is benign
// here.  Preserved as-is for byte parity with the binary.
std::string entityNameToNumber(const std::string& s) {
    std::string out = s;

    static const std::pair<const char*, const char*> kTable[] = {
        {"&amp;",    "&"      },
        {"&Omega;",  "&#937;" },
        {"&deg;",    "&#176;" },
        {"&Theta;",  "&#952;" },
        {"&plusmn;", "&#177;" },
        {"&lt;",     "&#60;"  },
        {"&gt;",     "&#62;"  },
    };

    for (auto const& e : kTable) {
        boost::algorithm::replace_all(out, e.first, e.second);
    }
    return out;
}


// === BLOCK B: URL / Csharp / replaceUnit / browseTo / queryToLink ===
// Functions: linkToQuery, queryToLink, escapeStringForCsharp,
//            replaceUnit, browseTo
// Notes: see diagnostics_text.md sections starting at line ~906.
// (Subagent B fills in below.)

// ---- linkToQuery — binary @0x2f2f20, 4365 B ----
std::string linkToQuery(const std::string& link)
{
    // 27-entry character-encoding table; order matters: '%' is encoded
    // after the LF/TAB/CR/SP/"/#/$ block but before '&', so that the
    // %XX escapes inserted by later passes are not double-encoded.
    static constexpr struct { char c; const char* repl; } kTable[] = {
        {'\n', "%0A"}, {'\t', "%09"}, {'\r', "%0D"},
        {' ',  "%20"}, {'"',  "%22"}, {'#',  "%23"},
        {'$',  "%24"}, {'%',  "%25"}, {'&',  "%26"},
        {'+',  "%2B"}, {',',  "%2C"}, {'/',  "%2F"},
        {':',  "%3A"}, {';',  "%3B"}, {'<',  "%3C"},
        {'=',  "%3D"}, {'>',  "%3E"}, {'?',  "%3F"},
        {'@',  "%40"}, {'[',  "%5B"}, {'\\', "%5C"},
        {']',  "%5D"}, {'^',  "%5E"}, {'`',  "%60"},
        {'{',  "%7B"}, {'|',  "%7C"}, {'}',  "%7D"},
    };
    std::string out = link;
    for (const auto& e : kTable) {
        const std::string needle(1, e.c);
        const std::string replacement(e.repl);
        boost::algorithm::replace_all(out, needle, replacement);
    }
    return out;
}

// ---- queryToLink — binary @0x2f4030, 596 B ----
std::string queryToLink(const std::string& q)
{
    std::string out;
    const char* const begin = q.data();
    const char* const end   = begin + q.size();
    const char* p = begin;

    const char* hit = static_cast<const char*>(
        std::memchr(p, '%', static_cast<size_t>(end - p)));
    if (hit == nullptr) hit = end;
    out.append(p, hit);
    p = hit;

    while (p != end) {
        bool consumed = false;
        if (end - p >= 3) {
            const unsigned char h = static_cast<unsigned char>(p[1]);
            const unsigned char l = static_cast<unsigned char>(p[2]);
            const unsigned short* const ct = *__ctype_b_loc();
            if ((ct[h] & _ISxdigit) && (ct[l] & _ISxdigit)) {
                // Build a 2-char short-SSO temporary and decode via stoi.
                std::string tmp(p + 1, 2);
                const int v = std::stoi(tmp, nullptr, 16);
                out.push_back(static_cast<char>(v));
                p += 3;
                consumed = true;
            }
        }
        if (!consumed) {
            // Not a valid %HH — copy '%' and any verbatim run up to the
            // next '%'.  The verbatim-copy branch is unified with the
            // "not-enough-bytes" case in the binary.
            const char* next = static_cast<const char*>(
                std::memchr(p + 1, '%',
                            static_cast<size_t>(end - (p + 1))));
            if (next == nullptr) next = end;
            out.append(p, next);
            p = next;
            continue;
        }
        // Scan forward for the next '%' from p.
        const char* next = static_cast<const char*>(
            std::memchr(p, '%', static_cast<size_t>(end - p)));
        if (next == nullptr) next = end;
        out.append(p, next);
        p = next;
    }
    return out;
}

// ---- escapeStringForCsharp — binary @0x2f9df0, 2213 B ----
std::string escapeStringForCsharp(std::string s)
{
    // Pre-NUL passes.  Backslash MUST be first so subsequent \X
    // sequences inserted by later passes are not re-escaped.
    static constexpr struct { char c; const char* repl; } kPre[] = {
        {'\\', "\\\\"},
        {'\'', "\\'"},
        {'"',  "\\\""},
    };
    for (const auto& e : kPre) {
        const std::string needle(1, e.c);
        const std::string replacement(e.repl);
        boost::algorithm::replace_all(s, needle, replacement);
    }

    // Inline NUL handling between passes 3 and 4.  Count NULs in s; if
    // non-zero, rebuild s, mapping NUL -> the 2-byte literal "\0".
    std::size_t nuls = 0;
    for (char ch : s) {
        if (ch == '\0') ++nuls;
    }
    if (nuls != 0) {
        std::string rebuilt;
        rebuilt.reserve(s.size() + nuls);
        for (std::size_t i = 0; i < s.size(); ++i) {
            const char ch = s[i];
            if (ch == '\0') {
                rebuilt.append("\\0", 2);
            } else {
                rebuilt.append(&ch, 1);
            }
        }
        s = std::move(rebuilt);
    }

    // Post-NUL passes.
    static constexpr struct { char c; const char* repl; } kPost[] = {
        {'\a', "\\a"},
        {'\b', "\\b"},
        {'\f', "\\f"},
        {'\n', "\\n"},
        {'\r', "\\r"},
        {'\t', "\\t"},
        {'\v', "\\v"},
    };
    for (const auto& e : kPost) {
        const std::string needle(1, e.c);
        const std::string replacement(e.repl);
        boost::algorithm::replace_all(s, needle, replacement);
    }
    return s;
}

// ---- replaceUnit — binary @0x2f7ae0, 1860 B ----
std::string replaceUnit(const std::string& text,
                        const std::string& unit,
                        const std::string& replacement)
{
    // Function-local static matchSuffix at b852d8, guard at b852e8.
    static const boost::regex matchSuffix(R"((.*?)(?: *\[[0-9]+\])+$)");

    // Build per-call pattern: "(.*?) *\(\Q" + unit + "\E\)(.*)"
    std::string pattern;
    pattern.reserve(20 + unit.size());
    pattern.append("(.*?) *\\(\\Q");
    pattern.append(unit);
    pattern.append("\\E\\)(.*)");
    boost::regex re(pattern);

    // Suffix-match probe using match_partial (flag 0x400).
    boost::cmatch m;
    const bool hasIndex = boost::regex_match(
        text.data(), text.data() + text.size(), m,
        matchSuffix, boost::regex_constants::match_partial);

    // Build per-call replacement: "$1 (" + replacement + ")$2"
    std::string repl;
    repl.reserve(7 + replacement.size());
    repl.append("$1 (");
    repl.append(replacement);
    repl.append(")$2");

    if (hasIndex) {
        // Suffix-stripping streaming path.  Binary appends the *entire
        // original input* after " (", not the suffix-stripped form —
        // preserved verbatim; see \verifyme in diagnostics_text.md.
        std::string out;
        boost::regex_replace(
            std::back_inserter(out),
            text.data(), text.data() + text.size(),
            matchSuffix, std::string("$1"),
            boost::regex_constants::match_default);
        out.append(" (", 2);
        out.append(text);
        out.append(")", 1);
        return out;
    }
    return boost::regex_replace(text, re, repl,
                                boost::regex_constants::match_default);
}

// ---- browseTo — binary @0x2eb950, 1739 B ----
void browseTo(std::string url)
{
    if (url.empty()) return;

    namespace bfs = boost::filesystem;

    // Walk up while parents exist and are not files.  Loop while the
    // current parent's status <= file_not_found (status_error=0 or
    // file_not_found=1).
    while (true) {
        bfs::path p(url);
        const bfs::path parent = p.parent_path();
        if (parent.empty()) return;
        boost::system::error_code ec;
        const bfs::file_status st = bfs::status(parent, ec);
        if (st.type() > bfs::file_not_found) {
            break;
        }
        url = parent.string();
    }

    // If url itself is a regular file, browse to its containing dir.
    {
        boost::system::error_code ec;
        const bfs::file_status st = bfs::status(bfs::path(url), ec);
        if (st.type() == bfs::regular_file) {
            url = bfs::path(url).parent_path().string();
        }
    }

    // Build "xdg-open \"<path>\"" and shell out.  No quoting / escaping
    // of `url`; this is binary-faithful behaviour (\binarynote).
    std::string cmd;
    cmd.reserve(11 + url.size());
    cmd.append("xdg-open \"", 10);
    cmd.append(url);
    cmd.append("\"", 1);
    (void)std::system(cmd.c_str());
}


// === BLOCK C: filename / json / python / truncate cluster ===
// Functions: sanitizeFilename, sanitizeInvalidFilename,
//            escapeStringForJson, escapeStringForPython,
//            truncateXmlSafe, truncateUtf8Safe
// Notes: see diagnostics_text.md sections starting at line ~425.
// (Subagent C fills in below.)

// ---- sanitizeFilename — binary @0x2fcbe0, 2060 B ----
void sanitizeFilename(std::string& s) {
    // Delete path-traversal sequences and Windows-illegal filename
    // characters.  The binary issues 11 find_format_all_impl2 calls
    // each with an empty replacement; semantically equivalent to
    // erase_all.  Order is preserved from the binary.
    boost::algorithm::replace_all(s, "../", "");
    boost::algorithm::replace_all(s, "/", "");
    boost::algorithm::replace_all(s, "..\\", "");
    boost::algorithm::replace_all(s, "\\", "");
    boost::algorithm::replace_all(s, ":", "");
    boost::algorithm::replace_all(s, "*", "");
    boost::algorithm::replace_all(s, "?", "");
    boost::algorithm::replace_all(s, "\"", "");
    boost::algorithm::replace_all(s, "|", "");
    boost::algorithm::replace_all(s, "<", "");
    boost::algorithm::replace_all(s, ">", "");
}

// ---- sanitizeInvalidFilename — binary @0x2fd3f0, 829 B ----
void sanitizeInvalidFilename(std::string& s) {
    sanitizeFilename(s);
    boost::filesystem::path p(s);
    std::string stem = p.stem().string();
    static const boost::regex re("COM[1-9]|PRN", boost::regex::no_except);
    if (boost::regex_match(stem, re)) {
        boost::filesystem::path ext = p.extension();
        p.remove_filename();
        p.replace_extension(ext);
        s = p.string();
    }
}

// ---- escapeStringForJson — binary @0x2f89b0, 1663 B ----
void escapeStringForJson(std::string& s) {
    // Work on a private copy to shield the user string from
    // intermediate growth/realloc churn during the 9 passes.
    std::string tmp = s;
    boost::algorithm::replace_all(tmp, "\\", "\\\\");
    boost::algorithm::replace_all(tmp, "\"", "\\\"");
    boost::algorithm::replace_all(tmp, "\n", "\\n");
    boost::algorithm::replace_all(tmp, "\b", "\\b");
    boost::algorithm::replace_all(tmp, "\f", "\\f");
    boost::algorithm::replace_all(tmp, "\r", "\\r");
    boost::algorithm::replace_all(tmp, "\t", "\\t");
    static const boost::regex xml_quote("&((#0*34)|(#x0*22)|(quot));",
                                        boost::regex::no_except);
    boost::algorithm::replace_all_regex(tmp, xml_quote,
                                        std::string("\\\\$&"));
    boost::algorithm::replace_all(tmp, "\"", "&#0034;");
    s = std::move(tmp);
}

// ---- escapeStringForPython — binary @0x2f9780, 1644 B ----
std::string escapeStringForPython(std::string s) {
    // 10 escape pairs in binary order.
    static const struct {
        const char* from;
        const char* to;
    } table[] = {
        { "\\", "\\\\" },
        { "'",  "\\'"  },
        { "\"", "\\\"" },
        { "\a", "\\a"  },
        { "\b", "\\b"  },
        { "\f", "\\f"  },
        { "\n", "\\n"  },
        { "\r", "\\r"  },
        { "\t", "\\t"  },
        { "\v", "\\v"  },
    };
    for (const auto& e : table) {
        boost::algorithm::replace_all(s, e.from, e.to);
    }
    return s;
}

// ---- truncateXmlSafe — binary @0x2fc690, 817 B ----
void truncateXmlSafe(std::string& s, unsigned long maxBytes) {
    if (s.size() <= maxBytes) return;
    if (maxBytes == 0) { s.clear(); return; }

    const char* data = s.data();
    const char* end  = data + s.size();
    const char* searchEnd = data + maxBytes;

    // Back up to the most recent '&' to expose any partial entity.
    for (const char* p = searchEnd; p > data; --p) {
        if (p[-1] == '&') { searchEnd = p; break; }
    }

    static const boost::regex regex(
        "&#x[0-9a-fA-F]+;|&#[0-9]+;|&amp;|&lt;|&gt|&quot;|&apos;");

    boost::cmatch m;
    if (boost::regex_search(data, end, m, regex,
                            boost::regex_constants::match_default)) {
        const char* entityEnd =
            (m.size() >= 3) ? m[3].second : m[0].first;
        if (entityEnd > data + maxBytes) {
            // Partial entity straddles the cut — erase from its '&'
            // onwards.
            s.erase(static_cast<std::size_t>(m[0].first - data),
                    std::string::npos);
            return;
        }
    }
    truncateUtf8Safe(s, maxBytes);
}

// ---- truncateUtf8Safe — binary @0x2fca40, 337 B ----
void truncateUtf8Safe(std::string& s, unsigned long maxBytes) {
    if (maxBytes == 0)        { s.clear(); return; }
    if (s.size() <= maxBytes) return;

    const char* data = s.data();
    const char* cut  = data + maxBytes;

    // Fast path: if the byte at `cut` is NOT a continuation byte
    // (continuations are 0x80..0xbf), the cut already lands on a
    // codepoint boundary.
    if (static_cast<unsigned char>(*cut) <= 0xbf) {
        // *cut is a continuation byte → walk back to the leading byte
        // (any byte with value >= 0xc0).
        std::ptrdiff_t back = 0;
        unsigned char  lead = 0;
        for (std::ptrdiff_t i = 0;
             i < static_cast<std::ptrdiff_t>(maxBytes);
             ++i) {
            unsigned char b =
                static_cast<unsigned char>(cut[-1 - i]);
            if (b >= 0xc0) { lead = b; back = i + 1; break; }
        }
        if (lead) {
            std::size_t nbytes =
                ((lead & 0xe0) == 0xc0) ? 2 :
                ((lead & 0xf0) == 0xe0) ? 3 :
                ((lead & 0xf8) == 0xf0) ? 4 : 1;
            // `back` is the count of bytes-of-this-codepoint already
            // included in [data, cut).  If we have a partial
            // codepoint, push cut back to its start.
            if (nbytes > static_cast<std::size_t>(back)) {
                cut -= back;
            }
        }
    }
    s.erase(static_cast<std::size_t>(cut - data), std::string::npos);
}


// === BLOCK D: xml-escape / sfc / toCheckedString / quote ===
// Functions: xmlEscapeUtf8Critical, xmlEscapeCritical, generateSfc,
//            toCheckedString, quote
// Notes: see diagnostics_text.md sections starting at line ~39 and ~1985.
// (Subagent D fills in below.)

// ---- xmlEscapeUtf8Critical — binary @0x2faaa0, 803 B ----
void xmlEscapeUtf8Critical(std::string& s) {
    // High bytes (>=0x80) become "&#NNN;" via boost::format with a
    // SIGN-EXTENDED int, so 0xC3 → -61 → "&#-61;".  The binary uses a
    // signed load (movsbl) followed by boost::format("&#%03d;") on the
    // signed int; this is reproduced verbatim for byte-for-byte
    // compatibility.  See diagnostics_text.md \verifyme note.
    for (std::size_t i = 0; i < s.size(); /* incremented inside */) {
        signed char c = static_cast<signed char>(s[i]);
        if (c >= 0) {
            ++i;
            continue;
        }
        int v = static_cast<int>(c);  // sign-extended → negative
        std::string sub = (boost::format("&#%03d;") % v).str();
        s.replace(i, 1, sub);
        i += sub.size();
    }
}

// ---- xmlEscapeCritical — binary @0x2fa7e0, 689 B ----
void xmlEscapeCritical(std::string& s) {
    // Function-local static regex (Meyers singleton): matches a bare
    // '&' that is NOT already the start of one of the canonical
    // entity forms (gt;, lt;, amp;, quot;, #NNN;, #xHH;).
    static const boost::regex re(
        "&(?![gl]t;|amp;|quot;|#[0-9]+;|#x[0-9a-fA-F]+;)");

    // Pass 1: escape bare '&' as '&amp;'.
    std::string out;
    boost::regex_replace(
        std::back_inserter(out),
        s.begin(), s.end(),
        re,
        std::string("&amp;"),
        boost::regex_constants::match_default);

    // Passes 2 and 3: escape bare '<' and '>'.  Performed on the
    // intermediate; the regex above already protects any pre-existing
    // entity references from double-escaping.
    boost::algorithm::replace_all(out, "<", "&lt;");
    boost::algorithm::replace_all(out, ">", "&gt;");

    s = std::move(out);
}

// ---- generateSfc — binary @0x2d10b0, 788 B ----
sfc::FeaturesCode generateSfc(const std::string& devType,
                              const std::string& options) {
    DeviceFamily family = toDeviceFamily(devType);

    // Parse options string into tokens, then convert each to a
    // DeviceOption and insert.  Unknown tokens raise from
    // toDeviceOption and are silently swallowed.
    std::vector<std::string> tokens = splitDeviceOptions(options);
    DeviceOptionSet optSet(family);
    for (auto const& tok : tokens) {
        try {
            optSet.insert(toDeviceOption(tok));
        } catch (...) {
            // swallow unknown option
        }
    }

    if (family == DeviceFamily::MF) {
        return zhinst::detail::generateMfSfc(devType, optSet);
    }

    // Unsupported family → throw with boost::source_location matching
    // the original binary's literals (file/function/line/col baked into
    // .rodata at the call site).
    std::ostringstream oss;
    oss << "Request to generate SFC code for an unsupported device family (";
    oss << zhinst::toString(family);
    oss << ").";

    boost::source_location loc(
        "/builds/labone/labone/device/types/src/device_option.cpp",
        364,
        "sfc::FeaturesCode zhinst::generateSfc(const std::string &, "
        "const std::string &)",
        47);
    boost::throw_exception(zhinst::Exception(oss.str()), loc);
}

// ---- toCheckedString — binary @0x2f2700, 179 B ----
std::string toCheckedString(const char* p) {
    // Open-coded in the binary as a libc++ SSO/long-string bifurcation
    // with the standard length-overflow guard; semantically equivalent
    // to the one-liner below (the 179 B body is just the inlined
    // std::string(const char*) ctor).
    return p ? std::string(p) : std::string();
}

// ---- quote ---- already implemented in core/platform.cpp:193 ----
// `zhinst::quote(std::string&)` lives in platform.cpp as part of an
// older reconstruction pass.  The body there is the canonical
// implementation; we deliberately do NOT define it here to avoid an
// ODR / multiple-definition link error.  The platform.cpp body and
// the algorithm sketch in `notes/diagnostics_text.md` (### quote)
// agree byte-for-byte: replace `"` with `\"`, prepend `"`, append `"`.


}  // namespace zhinst
