// Reconstructed from _seqc_compiler.so
//
// ZiFolder method implementations.
// Original source: /builds/labone/labone/utils/filesystem/src/zi_folder.linux.cpp

#include "zhinst/zi_folder.hpp"
#include "zhinst/exception.hpp"
#include "zhinst/format_time.hpp"

#include <boost/filesystem/path.hpp>
#include <boost/filesystem/operations.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/throw_exception.hpp>

#include <cstdlib>   // getenv
#include <cstring>   // strlen
#include <sstream>
#include <stdexcept>

#include <unistd.h>  // readlink, getuid
#include <pwd.h>     // getpwuid_r

// Forward-declared helpers from the binary (used by sessionSaveDirectoryName/ziFolder).
namespace zhinst {
bool runningOnMfDevice();
} // namespace zhinst

namespace zhinst {

// --------------------------------------------------------------------------
// ZiFolder::ZiFolder                                             @0x2ce2c0
// --------------------------------------------------------------------------
// The ctor is a plain move of the string argument into basePath_.
// Binary: movups (%rsi),%xmm0 / movups %xmm0,(%rdi) / mov 0x10(%rsi) -> 0x10(%rdi)
// then zero out source (standard libc++ string move).
ZiFolder::ZiFolder(std::string basePath)
    : basePath_(std::move(basePath))
{
}

// --------------------------------------------------------------------------
// ZiFolder::folderPath                                           @0x2ce2f0
// --------------------------------------------------------------------------
// Builds: basePath / [optional "$Zurich Instruments"] / "LabOne" / serial / extra
//
// The binary constructs a boost::filesystem::path from basePath_ (via strlen +
// new string), then:
//   1. Checks subdir length:
//      - 9 => compare with "/settings" => if match, append "$Zurich Instruments"
//      - 5 => compare with "/data"     => if match, append "$Zurich Instruments"
//      - else => append "$Zurich Instruments" always? No — the jne at 0x2ce440
//        leads to the append at 0x2ce444. Actually the else-branch *does* append
//        the literal "$Zurich Instruments".
//      Correction: the binary appends "$Zurich Instruments" for /data and /settings
//      matches, and also for non-matching subdirs (the jne at 0x2ce39b and 0x2ce43e
//      both fall through to the append). Re-reading: the jne at 0x2ce378 (len!=5)
//      jumps to 0x2ce440 which *is* the append block. So it always appends
//      "$Zurich Instruments" regardless.
//   2. Appends "LabOne".
//   3. Appends deviceSerial (the second const string& arg = `this` pointer's
//      caller passes serial). Actually looking at the prototype, the args
//      are (subdir, extra), and `this` contains basePath_.
//   4. Appends extra if non-empty.
//
// Simplified reconstruction:
std::string ZiFolder::folderPath(const std::string& subdir,
                                 const std::string& extra) const
{
    namespace fs = boost::filesystem;

    // Binary builds: subdir / "$Zurich Instruments" / "LabOne" / basePath_ / extra
    // Initial path from subdir (rdx); basePath_ is the device serial.
    fs::path result(subdir);
    result /= "$Zurich Instruments";
    result /= "LabOne";
    if (!basePath_.empty()) {
        result /= basePath_;
    }
    if (!extra.empty()) {
        result /= extra;
    }
    return result.string();
}

// --------------------------------------------------------------------------
// ZiFolder::sessionSaveDirectoryName                             @0x2ce620
// --------------------------------------------------------------------------
// Builds: "session_" + formatTime(now, true) + "_" + suffix + serial
// where suffix = "0" if serial.size()==1, else "".
//
// Binary flow:
//   1. time() + localtime_r() => tm => boost create_time => ptime
//   2. Check serial.size() == 1 => suffix = "0" (from rodata at 0x90b505)
//   3. ostringstream << "session_" << formatTime(ptime, true) << "_" << suffix << serial
//   4. return oss.str()
std::string ZiFolder::sessionSaveDirectoryName(const std::string& serial)
{
    time_t now;
    ::time(&now);
    struct tm tmBuf;
    struct tm* tmResult = ::localtime_r(&now, &tmBuf);
    if (!tmResult) {
        throw std::runtime_error(
            "could not convert calendar time to local time");
    }

    auto ptime = boost::posix_time::ptime_from_tm(tmBuf);

    // suffix is "0" if serial length == 1, else empty
    std::string suffix;
    if (serial.size() == 1) {
        suffix = "0";
    }

    std::ostringstream oss;
    oss << "session_"                     // 8 chars at 0x90b507
        << formatTime(ptime, true)
        << "_"                            // 1 char at 0x90a347
        << suffix
        << serial;

    return oss.str();
}

// --------------------------------------------------------------------------
// ZiFolder::ziFolder                                             @0x2cf0c0
// --------------------------------------------------------------------------
// Factory that resolves a well-known directory:
//
//   DirectoryType::Executable (2):
//     readlink("/proc/self/exe"), then call find_parent_path_size twice
//     to get the grandparent directory of the executable.
//
//   DirectoryType::Data (0) / Settings (1):
//     If runningOnMfDevice() => return "/data" or "/settings" directly.
//     Otherwise:
//       1. Try $HOME env var; fall back to getpwuid_r(getuid()).
//       2. Walk up parent directories of that path multiple times
//          (looking for an install-root marker).
//       3. Throw zhinst::Exception if HOME is not set and getpwuid_r fails.
//
//   Unknown type (>=3): throws zhinst::Exception("Unknown directory type.")
//
ZiFolder ZiFolder::ziFolder(DirectoryType type)
{
    // Helper: HOME-based fallback used by both the Executable readlink-failure
    // path and the Data/Settings non-MF path. Originally a `goto resolve_home;`
    // tail (binary jmp 0x2cf4eb); extracted into a lambda to keep the function
    // goto-free without changing the binary's execution shape.
    auto resolveHomeFolder = []() -> ZiFolder {
        // Try $HOME, fall back to getpwuid_r
        const char* homeDir = ::getenv("HOME");
        if (!homeDir) {
            struct passwd pw;
            struct passwd* result = nullptr;
            char pwBuf[1024];
            int rc = ::getpwuid_r(::getuid(), &pw, pwBuf, sizeof(pwBuf), &result);
            if (rc != 0) {
                BOOST_THROW_EXCEPTION(
                    Exception("Could not identify the user directory. "
                              "Explicit definition of the paths is needed."));
            }
            homeDir = pw.pw_dir;
        }

        // The binary constructs a path from homeDir and walks up parent
        // directories (multiple find_parent_path_size calls) to locate
        // the install root. It then returns the parent of the parent
        // (or grandparent depending on depth) as a ZiFolder.
        boost::filesystem::path homePath(homeDir);

        // Walk up parent directories to find install root
        // The binary calls find_parent_path_size up to 4 times in sequence,
        // checking at each level whether a non-empty parent exists.
        boost::filesystem::path root = homePath.parent_path();
        return ZiFolder(root.string());
    };

    if (type == DirectoryType::Executable) {
        // readlink("/proc/self/exe") into 4096-byte buffer
        char buf[4096];
        ::memset(buf, 0, sizeof(buf));
        ssize_t len = ::readlink("/proc/self/exe", buf, sizeof(buf));
        if (len <= 0) {
            // Fall through to HOME-based resolution (binary jumps to 0x2cf4eb)
            return resolveHomeFolder();
        }

        // Build a boost path from the readlink result, then take parent_path
        // twice (grandparent of the executable).
        boost::filesystem::path exePath(buf);
        boost::filesystem::path installRoot = exePath.parent_path().parent_path();
        return ZiFolder(installRoot.string());
    }

    if (static_cast<int>(type) >= 2) {
        // Unknown directory type — the binary throws at 0x2cf738
        BOOST_THROW_EXCEPTION(
            Exception("Unknown directory type."));
    }

    // Data (0) or Settings (1)
    if (runningOnMfDevice()) {
        // On MF device: return "/data" (type==0) or "/settings" (type==1)
        // Binary: lea 0x90b4db="/data", lea 0x90b4e1="/settings"
        //         cmove selects "/data" when type==0
        if (type == DirectoryType::Data) {
            return ZiFolder(std::string("/data"));
        } else {
            return ZiFolder(std::string("/settings"));
        }
    }

    // Non-MF Data/Settings → fall through to HOME resolution
    return resolveHomeFolder();
}

} // namespace zhinst
