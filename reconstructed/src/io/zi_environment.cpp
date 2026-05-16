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
// Three-stage check (matching the binary's two `status` calls):
//   1. Match `p` against the function-local-static regex
//      `^/media/sd[a-z][0-9]+$` (a Meyers singleton; the regex
//      object lives at 0xb85278 with guard at 0xb85288 in the
//      binary).  If no match, return false.
//   2. Append "/dev" to `p`, call `boost::filesystem::status` on the
//      result, and early-out with `false` if the type is
//      `status_error` (0) or `file_not_found` (1) — i.e. `type <= 1`
//      (binary @0x2eb6d6 + cmp @0x2eb70a).
//   3. Call `boost::filesystem::status` a second time on the same
//      path/ec pair (binary @0x2eb6f7).  Return true only if the
//      ec is unset AND `type == character_file` (5).
//
// The duplicate `status` call (IF-274) is binary-faithful: the
// original lacks the CSE that would collapse the two calls.  Both
// calls go to the same `boost::filesystem::detail::status` symbol
// (@0x36f280) with the same arguments; semantically the second call
// is what determines the result.
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

    // First status call (binary @0x2eb6d6): early-out on
    // status_error / file_not_found before the ec is even inspected.
    auto st1 = boost::filesystem::status(devPath, ec);
    if (static_cast<int>(st1.type()) <= boost::filesystem::file_not_found) {
        return false;
    }

    // Second status call (binary @0x2eb6f7) — same args, no CSE.
    auto st2 = boost::filesystem::status(devPath, ec);
    if (ec) {
        return false;
    }
    return st2.type() == boost::filesystem::character_file;
}

// ---------------------------------------------------------------------------
// makeDirectories(boost::filesystem::path const&)                @0x2cdef0
//
// Recursively creates `dir` and any missing parent components, then
// verifies the result is actually writeable.  On failure throws a
// `zhinst::Exception` carrying the ZI API code `0x8011` and the
// message `"Could not access directory '<dir>'."`.
//
// Binary flow (linear, no try/catch):
//   1. `boost::filesystem::detail::create_directories(dir, ec)` —
//      error_code overload, never throws.  The ec result is discarded
//      (the writeability check below is what gates success).
//   2. `isDirectoryWriteable(dir)` — if true, return cleanly.
//   3. Otherwise build the message
//      `"Could not access directory '" + dir.string() + "'."` (rodata
//      `0x90b4be` + `0x8ff0aa`) and throw
//      `Exception(ErrorCode{0x8011}, msg)` via
//      `boost::throw_exception` with `BOOST_CURRENT_LOCATION`
//      (source-location pair at rodata `0x90b456` / `0x90b48f`,
//      line/column packed as `0x3e0000002c`).
//
// Note on the error code: the binary uses constant `0x8011`, which
// occupies the `ZIResult_enum::ApiBufferTooSmall` slot in
// `error_messages.hpp`.  The semantic mismatch (a buffer-size code
// attached to a directory-permissions failure) is a binary quirk —
// see the `\binarynote` on the public declaration in
// `zhinst/io/zi_environment.hpp`.
//
// History (IF-273): an earlier reconstruction added a spurious outer
// `try { ... } catch (std::exception const&)` block that translated
// the message to `"Could not create directory ..."`.  The binary has
// no `__cxa_begin_catch` in this function; that translation does not
// happen here and the `"Could not create directory '"` literal in the
// binary's rodata is referenced from a different (unidentified) call
// site.
// ---------------------------------------------------------------------------
void makeDirectories(boost::filesystem::path const& dir)  // @0x2cdef0
{
    namespace fs = boost::filesystem;

    boost::system::error_code ec;
    fs::create_directories(dir, ec);

    if (isDirectoryWriteable(dir)) {
        return;
    }

    std::ostringstream oss;
    oss << "Could not access directory '" << dir << "'.";

    ErrorCode code;
    code.value_ = 0x8011;
    BOOST_THROW_EXCEPTION(Exception(code, oss.str()));
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
