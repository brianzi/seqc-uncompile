// Reconstructed from _seqc_compiler.so
//
// zhinst::ZiFolder — thin wrapper around a std::string base path, providing
// directory-resolution helpers for Zurich Instruments LabOne paths.
//
// Original source path (from source_location in ziFolder):
//   /builds/labone/labone/utils/filesystem/src/zi_folder.linux.cpp
//
// Binary symbols:
//   ZiFolder::ZiFolder(string)                     @0x2ce2c0
//   ZiFolder::folderPath(string const&, string const&) const  @0x2ce2f0
//   ZiFolder::sessionSaveDirectoryName(string const&)         @0x2ce620
//   ZiFolder::ziFolder(DirectoryType)              @0x2cf0c0
//
// Layout: 24 bytes — a single std::string member (libc++ SSO layout).

#pragma once

#include <string>

namespace zhinst {

class ZiFolder {
public:
    /// DirectoryType selects which well-known ZI directory to resolve.
    ///
    /// Values determined from ziFolder() at 0x2cf0c0:
    ///   - cmp $0x2,%edx => DirectoryType::Executable  (special: readlink /proc/self/exe)
    ///   - edx == 0      => string "/data"  (5 chars)
    ///   - edx == 1      => string "/settings" (9 chars, but stored as "/setti" + "ngs")
    /// The cmp $0x2 / jne pattern at 0x2cf0d7 and the setne+cmove at
    /// 0x2cf166-0x2cf17b confirm three enum values: 0=Data, 1=Settings,
    /// 2=Executable.
    enum class DirectoryType : int {
        Data       = 0,   // resolves to "<base>/data"
        Settings   = 1,   // resolves to "<base>/settings"
        Executable = 2,   // resolves via readlink("/proc/self/exe")
    };

    /// Move-constructs from a base path string.                 @0x2ce2c0
    explicit ZiFolder(std::string basePath);

    /// Build a folder path:  <basePath> / <subdir> / "LabOne" / <deviceSerial> / <extra>
    /// where <subdir> is determined by the value of `subdir`:
    ///   - "/settings" (len 9)  => "$Zurich Instruments" is appended first
    ///   - "/data"     (len 5)  => "$Zurich Instruments" is appended first
    ///   - anything else        => no intermediate "$Zurich Instruments" component
    /// Then "LabOne" is always appended, followed by `deviceSerial` (if non-empty)
    /// and `extra` (if non-empty).
    ///
    /// Returns a boost::filesystem::path encoded as std::string. @0x2ce2f0
    std::string folderPath(const std::string& subdir,
                           const std::string& extra) const;

    /// Build a session-save directory name from a device serial.
    ///   format: "session_" + formatTime(now, true) + "_" + suffix + serial
    /// where `suffix` is "0" if serial.size() == 1, else empty.
    ///
    /// Static function.                                          @0x2ce620
    static std::string sessionSaveDirectoryName(const std::string& serial);

    /// Factory: resolve a well-known ZI directory to a ZiFolder.
    ///
    /// - Executable: readlink("/proc/self/exe"), then parent_path twice.
    /// - Data/Settings: if runningOnMfDevice(), returns "/data" or "/settings".
    ///   Otherwise falls back to $HOME (or getpwuid_r) and walks up
    ///   parent directories to find the install root.
    ///
    /// Static function.                                          @0x2cf0c0
    static ZiFolder ziFolder(DirectoryType type);

private:
    std::string basePath_;  // offset 0x00, size 24
};

} // namespace zhinst
