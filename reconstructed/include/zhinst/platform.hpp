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

} // namespace zhinst
