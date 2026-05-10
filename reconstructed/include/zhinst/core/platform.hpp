// Platform utility functions
// Reconstructed from _seqc_compiler.so

#pragma once

#include <string>

namespace boost { namespace filesystem { class path; } }

namespace zhinst {

//! \brief Return the compile-time embedded platform identifier
//!        for the build that produced this shared object.
//!
//! \details The string is hard-coded into the binary at build
//! time and stamped into compiler output where the host platform
//! must be recorded (e.g. JSON metadata of the emitted ELF).
//! The Linux 64-bit build always returns `"linux64"`; other
//! builds (if they exist) would return their own identifier.
//!
//! \return  Platform identifier string (e.g. `"linux64"`).
std::string getPlatformName();

//! \brief Test whether `dir` is writable by attempting a probe
//!        file create-and-delete cycle.
//!
//! \details Creates a file named `Info.txt` inside `dir`, then
//! removes it.  Returns `true` if both operations succeed,
//! `false` if either fails (typically permission denied or
//! read-only filesystem).  The probe name is fixed regardless
//! of caller — concurrent calls into the same directory may
//! race on `Info.txt`.
//!
//! \binarynote The probe filename is not parameterised and not
//!             unique-per-process.  In practice the only caller
//!             uses this once at startup against the cache /
//!             temp directory, so the race window is benign.
//!
//! \param dir  Directory to probe.
//! \return  `true` iff the probe write/delete pair succeeds.
bool isDirectoryWriteable(boost::filesystem::path const& dir);

//! \brief Detect whether `path` sits exactly on a filesystem
//!        mount point.
//!
//! \details Compares `stat(path).st_dev` against
//! `stat(path "/..").st_dev`.  When the two device IDs differ
//! the path is the root of its filesystem and therefore a mount
//! point; the inode-equality fallback (root inode == its
//! parent's inode) catches the special case of `/`.
//!
//! \param path  Filesystem path to test.
//! \return  `true` iff `path` is the root of its filesystem.
bool isMountPoint(std::string const& path);

//! \brief Test whether every byte in `s` is plain 7-bit ASCII.
//!
//! \details Returns `true` iff every byte has its high bit
//! clear (`< 0x80`).  An empty string is pure ASCII.  Used as
//! a fast pre-check before deciding whether to invoke
//! `isValidUtf8` on user-supplied source text.
//!
//! \param s  Byte string to inspect.
//! \return  `true` iff all bytes are `< 0x80`.
bool isPureAscii(std::string const& s);

//! \brief Validate that the byte range `[begin, end)` is
//!        well-formed UTF-8.
//!
//! \details Walks the range checking the lead-byte pattern
//! against the four legal UTF-8 forms (`0xxxxxxx`,
//! `110xxxxx`, `1110xxxx`, `11110xxx`) and that each
//! continuation byte falls in the `0x80..0xBF` range.  Does
//! not check overlong encodings or surrogate-pair values
//! beyond the lead-byte mask.  Returns `false` on the first
//! invalid sequence.
//!
//! \param begin  First byte of the range.
//! \param end    One past the last byte.
//! \return  `true` iff the range is well-formed UTF-8 to the
//!          extent of the lead/continuation byte structure.
bool isValidUtf8(const char* begin, const char* end);

//! \brief String-overload convenience for the iterator-range
//!        `isValidUtf8`.
//!
//! \details Forwards to `isValidUtf8(s.data(), s.data() +
//! s.size())`; same semantics.
bool isValidUtf8(std::string const& s);

//! \brief Test whether `item` appears in the comma-separated
//!        list `list`, ignoring whitespace and case.
//!
//! \details Tokenises `list` on commas via
//! `boost::tokenizer<char_separator>`, trims surrounding
//! whitespace from each token, and returns `true` if any
//! token compares equal to `item` under
//! `boost::algorithm::iequals` (locale-independent
//! ASCII-case-insensitive compare).  An empty `list` always
//! yields `false`.
//!
//! \param item  Lookup token.
//! \param list  Comma-separated list of candidate values.
//! \return  `true` iff `item` matches any list entry.
bool isInList(std::string const& item, std::string const& list);

//! \brief Quote `s` in-place: escape embedded double-quotes
//!        and wrap the result in `"..."`.
//!
//! \details Replaces every `"` inside `s` with `\"`, then
//! prepends and appends a `"`.  Mutates `s` in place; useful
//! when emitting strings into JSON or other quoted-string
//! formats.  Does not handle other JSON escapes (backslash,
//! control characters); only the double-quote escape is
//! applied.
//!
//! \param s  String to quote in place.
void quote(std::string& s);

//! \brief Render `n` as a string of Unicode subscript-digit
//!        HTML entities.
//!
//! \details Converts `n` to its decimal string form via
//! `std::to_string`, then maps each digit `'0'..'9'` to its
//! HTML entity `&#8320;..&#8329;`.  Used for pretty-printing
//! channel/index suffixes in human-readable diagnostics.
//! Negative numbers carry their `-` sign through unchanged
//! (the `-` is non-digit and is dropped — see overload
//! below).
//!
//! \binarynote Non-digit characters in the intermediate
//!             string (notably the leading `-` for negative
//!             values) are silently skipped, so a negative
//!             integer renders without a sign.
//!
//! \param n  Integer to render.
//! \return  HTML-entity-encoded subscript representation.
std::string toSubscript(long n);

//! \brief Render each digit of `s` as its Unicode subscript
//!        HTML entity; non-digit characters are dropped.
//!
//! \details Maps `'0'..'9'` to `&#8320;..&#8329;`; any other
//! character is silently skipped.  Used to format index /
//! channel suffixes embedded in mixed text.
//!
//! \param s  Input text; usually a digit run or short
//!           identifier.
//! \return  HTML-entity-encoded subscript representation
//!          containing only the digit positions of `s`.
std::string toSubscript(std::string const& s);

//! \brief Render each digit, `+`, `-`, and `.` of `s` as its
//!        Unicode superscript HTML entity; other characters
//!        are dropped.
//!
//! \details Maps `'0'..'9'`, `'+'`, `'-'`, and `'.'` to their
//! Unicode superscript code-point HTML entities; any other
//! character is silently skipped.  Used to render exponents
//! and signed superscripts in diagnostics.
//!
//! \param s  Input text containing the characters to lift.
//! \return  HTML-entity-encoded superscript representation
//!          containing only the supported character classes.
std::string toSuperscript(std::string const& s);

//! \brief Escape `s` so it can appear inside a single-quoted
//!        MATLAB string literal.
//!
//! \details Replaces every embedded single quote `'` with the
//! MATLAB-doubled form `''`.  No other characters are touched
//! and no surrounding quotes are added — the caller wraps
//! the result.  Used when emitting MATLAB-syntax sections in
//! cross-language metadata.
//!
//! \param s  String to escape (passed by value, returned
//!           transformed).
//! \return  Escaped copy of `s`.
std::string escapeStringForMatlab(std::string s);

} // namespace zhinst
