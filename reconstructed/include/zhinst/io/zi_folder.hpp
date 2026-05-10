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

//! Resolves Zurich Instruments LabOne directory paths anchored to a
//! base path.
//!
//! Wraps a single base-path string and provides helpers for locating
//! the canonical LabOne subdirectories (`data`, `settings`,
//! `executable`) and for composing per-device session directory
//! names. The static factory `ziFolder(DirectoryType)` returns a
//! ready-made instance for the well-known directories: on MF devices
//! these are fixed paths under `/data` or `/settings`; on host
//! systems they are derived from `$HOME` or the running executable's
//! location.
//!
//! `folderPath()` composes a full
//! `<base>/<subdir>/$Zurich Instruments/LabOne/<deviceSerial>/<extra>`
//! path (the `$Zurich Instruments` segment is inserted only for the
//! standard `data` and `settings` subdirs). `sessionSaveDirectoryName()`
//! produces a timestamped, per-device directory name suitable for
//! storing one session's output.
class ZiFolder {
public:
    // DirectoryType selects which well-known ZI directory to resolve.
    //
    // Values determined from ziFolder() at 0x2cf0c0:
    //   - cmp $0x2,%edx => DirectoryType::Executable  (special: readlink /proc/self/exe)
    //   - edx == 0      => string "/data"  (5 chars)
    //   - edx == 1      => string "/settings" (9 chars, but stored as "/setti" + "ngs")
    // The cmp $0x2 / jne pattern at 0x2cf0d7 and the setne+cmove at
    // 0x2cf166-0x2cf17b confirm three enum values: 0=Data, 1=Settings,
    // 2=Executable.
    //! \brief Selector for the well-known LabOne directory
    //! `ziFolder()` should resolve: user-data root, settings root,
    //! or the running executable's directory.
    enum class DirectoryType : int {
        Data       = 0,   // resolves to "<base>/data"
        Settings   = 1,   // resolves to "<base>/settings"
        Executable = 2,   // resolves via readlink("/proc/self/exe")
    };

    // Move-constructs from a base path string.                 @0x2ce2c0
    //! \brief Constructs a folder rooted at `basePath`; the
    //! argument is move-constructed into `basePath_`.
    //! \param basePath Filesystem path used as the prefix for all
    //!                 paths derived from this instance.
    explicit ZiFolder(std::string basePath);

    // Build a folder path:  <basePath> / <subdir> / "LabOne" / <deviceSerial> / <extra>
    // where <subdir> is determined by the value of `subdir`:
    //   - "/settings" (len 9)  => "$Zurich Instruments" is appended first
    //   - "/data"     (len 5)  => "$Zurich Instruments" is appended first
    //   - anything else        => no intermediate "$Zurich Instruments" component
    // Then "LabOne" is always appended, followed by `deviceSerial` (if non-empty)
    // and `extra` (if non-empty).
    //
    // Returns a boost::filesystem::path encoded as std::string. @0x2ce2f0
    //! \brief Composes
    //! `<basePath_>/<subdir>[/$Zurich Instruments]/LabOne/[<extra>]`
    //! where the `$Zurich Instruments` segment is inserted only when
    //! `subdir` is the canonical `"/data"` or `"/settings"` literal.
    //! \param subdir Sub-directory marker; the literal strings
    //!               `"/data"` and `"/settings"` trigger the vendor
    //!               segment, any other value is appended verbatim.
    //! \param extra  Optional tail segment appended after `LabOne`
    //!               (e.g. device serial); empty value is skipped.
    //! \return Composed path encoded as `std::string`.
    std::string folderPath(const std::string& subdir,
                           const std::string& extra) const;

    // Build a session-save directory name from a device serial.
    //   format: "session_" + formatTime(now, true) + "_" + suffix + serial
    // where `suffix` is "0" if serial.size() == 1, else empty.
    //
    // Static function.                                          @0x2ce620
    //! \brief Builds a per-session, per-device directory name of
    //! the form `"session_<timestamp>_<padding><serial>"` (with a
    //! single `"0"` zero-pad when the serial is one character).
    //! \param serial Device serial number to embed in the name.
    //! \return Directory-name suitable for storing one session's
    //! data.
    static std::string sessionSaveDirectoryName(const std::string& serial);

    // Factory: resolve a well-known ZI directory to a ZiFolder.
    //
    // - Executable: readlink("/proc/self/exe"), then parent_path twice.
    // - Data/Settings: if runningOnMfDevice(), returns "/data" or "/settings".
    //   Otherwise falls back to $HOME (or getpwuid_r) and walks up
    //   parent directories to find the install root.
    //
    // Static function.                                          @0x2cf0c0
    //! \brief Resolves the well-known LabOne directory selected by
    //! `type` and returns a `ZiFolder` rooted at it.
    //! \details `Executable` uses `readlink("/proc/self/exe")` and
    //! walks two parent directories up; `Data` / `Settings` return
    //! the fixed `/data` / `/settings` paths on MF devices and fall
    //! back to a `$HOME`-derived install root on host systems.
    //! \param type Which well-known directory to locate.
    //! \return A `ZiFolder` whose `basePath_` is the resolved
    //! directory.
    static ZiFolder ziFolder(DirectoryType type);

private:
    //! \brief Filesystem root all `folderPath` invocations are
    //! anchored to.
    std::string basePath_;  // offset 0x00, size 24
};

} // namespace zhinst
