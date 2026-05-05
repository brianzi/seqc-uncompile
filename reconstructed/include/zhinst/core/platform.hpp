// Platform utility functions
// Reconstructed from _seqc_compiler.so

#pragma once

#include <string>

namespace boost { namespace filesystem { class path; } }

namespace zhinst {

// Returns "linux64" — compile-time embedded platform identifier.
// @0x2ec6e0
std::string getPlatformName();

// Checks whether the given directory is writable by attempting to create
// and remove a temporary file "Info.txt" inside it.
// @0x2cdda0
bool isDirectoryWriteable(boost::filesystem::path const& dir);

// Tests whether a path is a mount point by comparing the st_dev of
// the path itself and its parent (".."). Returns true if the device
// IDs differ (or inode numbers match), meaning a filesystem boundary.
// @0x2eb310
bool isMountPoint(std::string const& path);

// Returns true if every byte in the string has its high bit clear (< 0x80).
// @0x2f85d0
bool isPureAscii(std::string const& s);

// Iterator-range overload: validates UTF-8 byte sequences.
// Checks lead-byte patterns (0xxxxxxx, 110xxxxx, 1110xxxx, 11110xxx)
// and that continuation bytes are <= 0xBF.
// @0x2f8330
bool isValidUtf8(const char* begin, const char* end);

// String overload: delegates to the iterator-range version.
// @0x2f8470
bool isValidUtf8(std::string const& s);

// Tokenizes `list` on commas, trims whitespace from each token, and
// returns true if any token case-insensitively equals `item`.
// Uses boost::tokenizer<char_separator> and boost::algorithm::iequals.
// @0x2f2870
bool isInList(std::string const& item, std::string const& list);

// Escapes embedded double-quotes (\" → \\\") and wraps the string in "...".
// @0x2fa6a0
void quote(std::string& s);

// Converts a long to its subscript digit representation using HTML entities
// (&#8320;..&#8329; for digits 0-9). Non-digit chars in the intermediate
// string representation are skipped.
// @0x2fdb80
std::string toSubscript(long n);

// Converts each digit character in the string to its subscript HTML entity.
// Non-digit characters are skipped.
// @0x2fd960
std::string toSubscript(std::string const& s);

// Converts each digit, '+', '-', '.' in the string to its superscript
// HTML entity. Other characters are skipped.
// @0x2fd730
std::string toSuperscript(std::string const& s);

// Escapes single-quotes in a string for MATLAB by replacing ' with ''.
// @0x2f9110
std::string escapeStringForMatlab(std::string s);

} // namespace zhinst
