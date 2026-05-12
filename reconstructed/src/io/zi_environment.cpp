// Reconstructed from _seqc_compiler.so
//
// zhinst::-namespace runtime-environment helpers: MF / MF64 detection
// (manifest-driven) and a small handful of filesystem helpers used by
// the LabOne ZiFolder code path.  See the companion header
// `zhinst/io/zi_environment.hpp` for the public-API documentation.
//
// Original source path inferred from string literals in the binary:
//   /builds/labone/labone/utils/filesystem/src/zi_folder.cpp        (makeDirectories)
//
// History: prior to this file the symbol `runningOnMfDevice()` was a
// constant-`false` stub at `core/stubs.cpp:34` (see IF-253); that
// stub is removed in the same change-set that introduces this file.

#include "zhinst/io/zi_environment.hpp"

#include "zhinst/core/exception.hpp"
#include "zhinst/core/platform.hpp"

#include <boost/filesystem/operations.hpp>
#include <boost/filesystem/path.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/regex.hpp>
#include <boost/throw_exception.hpp>

#include <sstream>
#include <string>

namespace zhinst {

// =========================================================================
// Anonymous-namespace helpers (mirrored from binary anon namespace).
//
// In the binary these live as
// `zhinst::(anonymous namespace)::readManifest`, `::isMf`, `::doIsMf`,
// `::isMf64`.
// =========================================================================
namespace {

// ---------------------------------------------------------------------------
// readManifest(string const&)                                    @0x2ec210
//
// Reads the JSON manifest at `path`.  If the file does not exist
// (filesystem status type <= 1, i.e. `status_error` or
// `file_not_found`), returns an empty ptree without attempting to
// parse it.  Otherwise calls
// `boost::property_tree::json_parser::read_json` and returns the
// parsed tree.  No exception swallowing — a malformed file
// propagates the parser's exception out.
// ---------------------------------------------------------------------------
boost::property_tree::ptree readManifestImpl(std::string const& path)  // @0x2ec210
{
    boost::property_tree::ptree pt;
    auto st = boost::filesystem::status(boost::filesystem::path(path));
    if (st.type() <= boost::filesystem::regular_file) {
        // status_error(0) or file_not_found(1) — empty tree.
        return pt;
    }
    boost::property_tree::json_parser::read_json(path, pt);
    return pt;
}

// ---------------------------------------------------------------------------
// readManifest()                                                 @0x2ec5e0
//
// Lazy-init function-local static.  Calls
// `readManifestImpl("/opt/zi/LabOne/manifest.json")` once on first
// invocation, caches the result in a Meyers singleton, and registers
// `~ptree` with `__cxa_atexit`.  The literal manifest path is at
// rodata 0x90ce08 and the guard variable at 0xb852d0 in the binary.
// ---------------------------------------------------------------------------
boost::property_tree::ptree const& laboneManifest()  // @0x2ec5e0
{
    static boost::property_tree::ptree manifest =
        readManifestImpl("/opt/zi/LabOne/manifest.json");
    return manifest;
}

// ---------------------------------------------------------------------------
// doIsMf(ptree const&)                                           @0x2ec700
//
// Reads the `"device"` key as `string` (default empty) and returns
// true iff the value has length 2 and equals "mf".  The binary uses
// SSO short-string ops and a single 16-bit compare against `0x666d`
// ("mf" little-endian).
// ---------------------------------------------------------------------------
bool doIsMf(boost::property_tree::ptree const& pt)  // @0x2ec700
{
    std::string device = pt.get<std::string>("device", "");
    return device.size() == 2 && device == "mf";
}

// ---------------------------------------------------------------------------
// isMf(ptree const&)                                             @0x2ec1e0
//
// Wraps `doIsMf` in `try { ... } catch(...) { return false; }`.
// Used by the cached `runningOnMfDevice()` path.
// ---------------------------------------------------------------------------
bool isMf(boost::property_tree::ptree const& pt)  // @0x2ec1e0
{
    try {
        return doIsMf(pt);
    } catch (...) {
        return false;
    }
}

// ---------------------------------------------------------------------------
// isMf64(ptree const&)                                           @0x2ec430
//
// Returns true iff `doIsMf(pt)` AND the `"platform"` key (read as
// `string`, default empty) has length 10 and matches "linuxARM64"
// byte-for-byte.  The binary verifies the 10 bytes via two XOR
// compares: an 8-byte XOR against `"linuxARM"` (= 0x4d524178756e696c)
// followed by a 2-byte XOR against `"64"` (= 0x3436).  The whole
// body is wrapped in `try { ... } catch(...) { return false; }`.
// ---------------------------------------------------------------------------
bool isMf64(boost::property_tree::ptree const& pt)  // @0x2ec430
{
    try {
        if (!doIsMf(pt)) {
            return false;
        }
        std::string platform = pt.get<std::string>("platform", "");
        return platform.size() == 10 && platform == "linuxARM64";
    } catch (...) {
        return false;
    }
}

} // anonymous namespace

// =========================================================================
// Public API — MF / MF64 detection
// =========================================================================

// ---------------------------------------------------------------------------
// runningOnMfDevice()                                            @0x2ec580
//
// Caches `isMf(laboneManifest())` in a Meyers-singleton bool guarded
// by `__cxa_guard`.  On a workstation the manifest is normally
// absent, so the cached value is `false`; on an MF instrument the
// `"device"` key reads `"mf"` and the cached value is `true`.
// ---------------------------------------------------------------------------
bool runningOnMfDevice()  // @0x2ec580
{
    static bool runningOnMf = isMf(laboneManifest());
    return runningOnMf;
}

// ---------------------------------------------------------------------------
// runningOnMfDevice(string const&)                               @0x2ec160
//
// Path-arg variant: reads `manifestPath` fresh (no cache), calls
// `doIsMf` directly, and wraps the whole thing in
// `try { ... } catch(...) { return false; }` so any parse error or
// I/O failure surfaces as a `false` result.
// ---------------------------------------------------------------------------
bool runningOnMfDevice(std::string const& manifestPath)  // @0x2ec160
{
    try {
        return doIsMf(readManifestImpl(manifestPath));
    } catch (...) {
        return false;
    }
}

// ---------------------------------------------------------------------------
// runningOnMf64Device()                                          @0x2ec680
//
// Caches `isMf64(laboneManifest())` in its own Meyers-singleton bool
// (separate guard from `runningOnMfDevice`'s).  Both 0-arg cached
// forms share the same underlying `laboneManifest()` static, so the
// JSON file is read at most once per process.
// ---------------------------------------------------------------------------
bool runningOnMf64Device()  // @0x2ec680
{
    static bool runningOnMf64 = isMf64(laboneManifest());
    return runningOnMf64;
}

// ---------------------------------------------------------------------------
// runningOnMf64Device(string const&)                             @0x2ec3d0
//
// Path-arg variant: reads `manifestPath` fresh (no cache) and calls
// `isMf64` (which has its own internal try/catch).  Unlike the
// `runningOnMfDevice(string)` overload there is no outer try/catch
// here — the binary relies entirely on `isMf64`'s wrapper.
// ---------------------------------------------------------------------------
bool runningOnMf64Device(std::string const& manifestPath)  // @0x2ec3d0
{
    return isMf64(readManifestImpl(manifestPath));
}

// =========================================================================
// Public API — filesystem helpers
// =========================================================================

// ---------------------------------------------------------------------------
// hasMediaDevNode(string const&)                                 @0x2eb550
//
// Two-stage check:
//   1. Match `p` against the function-local-static regex
//      `^/media/sd[a-z][0-9]+$` (a Meyers singleton; the regex
//      object lives at 0xb85278 with guard at 0xb85288 in the
//      binary).  If no match, return false.
//   2. Append "/dev" to `p`, call `boost::filesystem::status` on the
//      result, and return true only if the status call set no error
//      AND the file-type equals `character_file` (5).
//
// In short: "is `p` a `/media/sdX1` mount whose `/dev` child is a
// character device?".
// ---------------------------------------------------------------------------
bool hasMediaDevNode(std::string const& p)  // @0x2eb550
{
    static const boost::regex reMedia("^/media/sd[a-z][0-9]+$");
    if (!boost::regex_match(p, reMedia)) {
        return false;
    }

    boost::filesystem::path devPath = boost::filesystem::path(p) / "dev";
    boost::system::error_code ec;
    auto st = boost::filesystem::status(devPath, ec);
    if (ec) {
        return false;
    }
    return st.type() == boost::filesystem::character_file;
}

// ---------------------------------------------------------------------------
// makeDirectories(boost::filesystem::path const&)                @0x2cdef0
//
// Recursively creates `dir` and any missing parent components, with
// richer error reporting than bare `boost::filesystem::create_directories`.
// Flow:
//   1. `boost::filesystem::create_directories(dir, ec)` — error_code
//      overload, never throws.
//   2. `isDirectoryWriteable(dir)` — if true, return cleanly.
//   3. Otherwise build the message
//      `"Could not access directory '" + dir.string() + "'."` and
//      throw `zhinst::Exception(0x8011, msg)` via
//      `boost::throw_exception` (carries source-location info).
//   4. Any exception thrown out of step 1/3 is caught at the
//      outer `__cxa_begin_catch` and translated into a fresh
//      `zhinst::Exception(0x8011, "Could not create directory '" +
//      dir.string() + "'." + inner.what())` which is then
//      re-thrown.  The error-code constant 0x8011 corresponds to
//      ZIIOException-class semantics in the ZI API enum.
// ---------------------------------------------------------------------------
void makeDirectories(boost::filesystem::path const& dir)  // @0x2cdef0
{
    namespace fs = boost::filesystem;
    try {
        boost::system::error_code ec;
        fs::create_directories(dir, ec);

        if (isDirectoryWriteable(dir)) {
            return;
        }

        std::ostringstream oss;
        oss << "Could not access directory '" << dir << "'.";
        BOOST_THROW_EXCEPTION(Exception(oss.str()));
    } catch (std::exception const& inner) {
        std::ostringstream oss;
        oss << "Could not create directory '" << dir << "'." << inner.what();
        BOOST_THROW_EXCEPTION(Exception(oss.str()));
    }
}

// ---------------------------------------------------------------------------
// markFileHidden(boost::filesystem::path const&)                 @0x2eb940
//
// Linux build is a no-op (the `.dotfile` convention is applied at
// create-time elsewhere).  Kept as a defined symbol so the LabOne
// ZiFolder code can call it unconditionally.  The Windows variant
// would call `SetFileAttributesW` but is not present in this build.
// ---------------------------------------------------------------------------
void markFileHidden(boost::filesystem::path const& /*p*/)  // @0x2eb940
{
    // Intentional no-op on Linux.
}

// ---------------------------------------------------------------------------
// initBoostFilesystemForUnicode()                                @0x2ec020
//
// Linux build is a no-op (UTF-8 is the natural path encoding).
// The Windows variant would install a UTF-8 codecvt facet on
// `boost::filesystem::path::imbue` but is not present in this build.
// ---------------------------------------------------------------------------
void initBoostFilesystemForUnicode()  // @0x2ec020
{
    // Intentional no-op on Linux.
}

} // namespace zhinst
