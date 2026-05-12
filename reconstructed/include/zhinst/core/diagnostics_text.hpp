// SPDX-License-Identifier: BSD-3-Clause
//! \file diagnostics_text.hpp
//! \brief User-facing text helpers: HTML/XML/JSON/CSV escaping, URL
//!        rewriting, filename sanitisation, UTF-8 truncation.
//!
//! \details
//! Reconstructed in phase D16.  None of these functions have callers
//! inside the reconstructed compiler — they exist for downstream
//! tooling that loads the shared object directly for diagnostics-only
//! purposes (e.g. AWG status panels, error-report formatters).
//!
//! Because the compiler test suite cannot reach any of these symbols
//! through the Python bindings, a separate diff-test harness
//! (`tests/diff_unreachable/harness.py`, see TODO Phase E) was built
//! to validate them against the original binary.  17 of the 19
//! callable functions in this header have been verified by that
//! harness across 640 input cases; remaining `\verifyme` markers
//! identify the symbols still pending harness coverage.

#pragma once

#include <string>

#include "zhinst/device/device_type.hpp"  // zhinst::FeaturesCode

namespace zhinst {

//! \brief Replace HTML/XML character-reference escapes in-place with
//!        their UTF-8 codepoints.
//!
//! Decodes numeric escapes (`&#NNN;`, `&#xHH;`) plus the five canonical
//! named entities (`&amp;`, `&lt;`, `&gt`, `&quot;`, `&apos;`) — note
//! that `&gt` (no semicolon) is intentional and matches the binary's
//! regex literal exactly.  Surrogate pairs (D800..DFFF) are combined
//! into the corresponding 4-byte UTF-8 sequence.
//!
//! \param s  String to decode in place.
void xmlUnescape(std::string& s);

//! \brief Copy-returning wrapper around xmlUnescape.
//! \param s  Input string (consumed by value).
//! \return Decoded copy of \p s.
std::string xmlUnescapeCopy(std::string s);

//! \brief Replace numeric HTML entities (`&#NNN;`) with short symbolic
//!        equivalents (e.g. `&#8486;` → `Ohm`, `&#178;` → `^2`).
//!
//! Hand-coded fixed table of 11 replacements.  Unmatched entities are
//! left in place.
//!
//! \param s  Input string.
//! \return Copy of \p s with numeric entities substituted.
std::string entityNumberToTxt(const std::string& s);

//! \brief Replace seven named HTML entities (`&amp;`, `&Omega;`,
//!        `&deg;`, `&Theta;`, `&plusmn;`, `&lt;`, `&gt;`) with their
//!        numeric forms.
//!
//! Not the inverse of entityNumberToTxt — the forward direction
//! is wider.
//!
//! \param s  Input string.
//! \return Copy of \p s with named entities converted to numeric form.
std::string entityNameToNumber(const std::string& s);

//! \brief URL-decode the query portion of an external link (turns
//!        `%XX` triples back into raw bytes).
//! \param link  URL-encoded query string.
//! \return Decoded copy.
std::string linkToQuery(const std::string& link);

//! \brief URL-encode reserved characters (`+`, `,`, `/`, CR, LF) into
//!        `%XX` triples.  Inverse of linkToQuery for the small
//!        character set actually escaped.
//! \param q  Query string to encode.
//! \return URL-encoded copy.
std::string queryToLink(const std::string& q);

//! \brief Escape \p s as a C# string literal body (`\\`, `\"`, `\n`,
//!        ...).  Does not add surrounding quotes.
//! \param s  Input string (consumed by value).
//! \return Escaped copy.
std::string escapeStringForCsharp(std::string s);

//! \brief Escape \p s as a JSON string literal body in-place (without
//!        surrounding quotes), with a defence-in-depth pass that also
//!        replaces XML-encoded quote entities `&quot;` / `&#34;` /
//!        `&#x22;` with `\\$&` and a final `"` → `&#0034;` swap.
//! \param s  String to escape in place.
void escapeStringForJson(std::string& s);

//! \brief Escape \p s as a Python string literal body.  Handles ten
//!        C escape characters (`\\`, `\'`, `\"`, `\a`, `\b`, `\f`,
//!        `\n`, `\r`, `\t`, `\v`).  Does **not** emit `\xNN` for
//!        non-printables.
//! \param s  Input string (consumed by value).
//! \return Escaped copy.
std::string escapeStringForPython(std::string s);

//! \brief Replace filesystem-reserved characters and `../` traversal
//!        patterns with empty strings.
//!
//! Removes `/`, `\\`, `:`, `*`, `?`, `"`, `|`, `<`, `>`, and the
//! traversal sequences `../` and `..\\`.  A lone `.` is preserved
//! (only `..` followed by a path separator is stripped).
//!
//! \param s  Filename string to sanitise in place.
void sanitizeFilename(std::string& s);

//! \brief Apply sanitizeFilename and additionally rename Windows
//!        reserved stems (e.g. `COM1..COM9`, `PRN`).
//!
//! \binarynote The pattern in the binary is `COM[1-9]|PRN` with no
//!             anchors; classical reserved names `LPT[1-9]`, `AUX`,
//!             `CON`, `NUL` are NOT covered, and substring matches
//!             (e.g. `"COM1.txt"` → `"COMx.txt"`) trigger.  Confirmed
//!             by the diff-test harness across 24 cases including all
//!             eight reserved-stem families and `COM1.txt` /
//!             `PRN.log`.  Surprising for callers who expect the full
//!             Win32 reserved-name set — preserved verbatim.
//!
//! \param s  Filename string to sanitise in place.
void sanitizeInvalidFilename(std::string& s);

//! \brief Replace each unit substring in \p text matching \p unit with
//!        \p replacement, ignoring occurrences inside parenthesised
//!        regex `\\E)` markers.
//! \param text         Input text.
//! \param unit         Unit substring to find (treated as a literal
//!                     inside a `\\Q...\\E` regex bracket).
//! \param replacement  Replacement string.
//! \return Copy of \p text with substitutions applied.
//! \verifyme
std::string replaceUnit(const std::string& text,
                        const std::string& unit,
                        const std::string& replacement);

//! \brief Open \p url in the host's default browser.
//!
//! On Linux this shells out to `xdg-open`.  Other platforms have
//! their own equivalents (`open` on macOS, ShellExecute on Windows);
//! the reconstructed body only contains the Linux branch observed in
//! the analysed binary.
//!
//! \param url  Target URL or local path (consumed by value).
//! \verifyme
void browseTo(std::string url);

//! \brief Truncate \p s to at most \p maxBytes bytes, taking care to
//!        cut on a UTF-8 codepoint boundary.
//! \param s         String to truncate in place.
//! \param maxBytes  Maximum permitted byte length.
void truncateUtf8Safe(std::string& s, unsigned long maxBytes);

//! \brief Truncate \p s like truncateUtf8Safe but additionally avoid
//!        cutting inside an XML character/entity reference.
//! \param s         String to truncate in place.
//! \param maxBytes  Maximum permitted byte length.
void truncateXmlSafe(std::string& s, unsigned long maxBytes);

//! \brief In-place escape of bytes for inclusion as XML character data:
//!        every byte ≥ 0x80 is replaced with `&#NNN;` (decimal).
//!
//! \binarynote The binary uses a signed-extension fold before the
//!             `boost::format("&#%03d;")` call, which means high bytes
//!             render as **negative** decimals (`&#-NNN;`) — invalid
//!             XML but preserved here for byte-for-byte compatibility.
//!             Confirmed by the diff-test harness across high-byte
//!             inputs (`\x80`, `\xa0\xff`, etc.).
//!
//! \param s  String to escape in place.
void xmlEscapeUtf8Critical(std::string& s);

//! \brief In-place escape of the three structurally critical XML
//!        characters (`&`, `<`, `>`).
//!
//! Idempotent: ampersands that already form a valid character or named
//! entity reference are left unchanged.  `&quot;` is recognised in the
//! lookahead but never produced (double quotes pass through).
//!
//! \param s  String to escape in place.
void xmlEscapeCritical(std::string& s);

//! \brief Generate the SFC FeaturesCode for an MF-family device.
//!
//! \param devType  Device-type string (e.g. `"MF-DEV4123"`).
//! \param options  Comma-separated option list.
//! \return Composed feature code on success.
//! \throws zhinst::Exception if the device family resolves to anything
//!         other than MF (the only family for which an SFC table
//!         exists in this binary).
//! \verifyme
sfc::FeaturesCode generateSfc(const std::string& devType,
                              const std::string& options);

//! \brief Wrap a possibly-null C string into a `std::string`,
//!        returning the empty string for nullptr.
//! \param p  C string pointer (may be null).
//! \return `std::string(p)` if \p p is non-null, otherwise an empty
//!         string.
std::string toCheckedString(const char* p);

}  // namespace zhinst
