// Reconstructed from _seqc_compiler.so
//
// zhinst::-namespace runtime-environment helpers covering two related
// concerns:
//
//   1. MF / MF64 instrument-host detection — whether the compiler is
//      currently executing on a Zurich Instruments MF-class device
//      (a stand-alone instrument running the LabOne stack on its
//      embedded SoC) versus on a regular workstation.  The decision is
//      derived from the LabOne installation manifest at
//      `/opt/zi/LabOne/manifest.json` (a JSON `device` / `platform`
//      key pair); the public 0-arg accessors cache the result in a
//      Meyers-singleton bool.
//
//   2. Filesystem helpers used by the LabOne ZiFolder code path:
//      `hasMediaDevNode` (regex-matches `/media/sd[a-z][0-9]+$`),
//      `makeDirectories` (mkdir -p with detailed error reporting),
//      `markFileHidden` / `initBoostFilesystemForUnicode` (no-ops on
//      Linux; their non-trivial Windows implementations are not part
//      of this binary).
//
// Original source path inferred from string literals in the binary:
//   /builds/labone/labone/utils/filesystem/src/zi_folder.cpp        (makeDirectories)
//   /builds/labone/labone/device/types/src/device_option.cpp        (sibling)
//
// Binary symbols (all in `zhinst::` namespace):
//   runningOnMfDevice()                              @0x2ec580   83 B
//   runningOnMfDevice(string const&)                 @0x2ec160  120 B
//   runningOnMf64Device()                            @0x2ec680   83 B
//   runningOnMf64Device(string const&)               @0x2ec3d0   95 B
//   hasMediaDevNode(string const&)                   @0x2eb550  770 B
//   makeDirectories(boost::filesystem::path const&)  @0x2cdef0  583 B
//   markFileHidden(boost::filesystem::path const&)   @0x2eb940    6 B
//   initBoostFilesystemForUnicode()                  @0x2ec020    6 B
//
// File-local helpers (anon namespace in the binary, mirrored in the
// .cpp):
//   readManifest()                                   @0x2ec5e0   94 B
//   readManifest(string const&)                      @0x2ec210  438 B
//   isMf(ptree const&)                               @0x2ec1e0   36 B
//   isMf64(ptree const&)                             @0x2ec430  331 B
//   doIsMf(ptree const&)                             @0x2ec700  284 B
//
// History: see IF-253 for the prior cosmetic state (constant-`false`
// stub at `core/stubs.cpp:34` with a wrong `/proc/device-tree/model`
// narrative); that narrative was scrubbed in 2026-05-12.  This header
// + its companion `.cpp` are the proper rebuild.

#pragma once

#include <string>

#include <boost/filesystem/path.hpp>

namespace zhinst {

// ---------------------------------------------------------------------------
// MF / MF64 detection
// ---------------------------------------------------------------------------

//! \brief Reports whether the compiler is hosted on a Zurich
//!        Instruments MF-class device.
//!
//! The result is computed from the LabOne installation manifest at
//! `/opt/zi/LabOne/manifest.json` by parsing it as JSON and checking
//! whether the top-level `device` value begins with `mf` (the MF
//! family prefix).  On a workstation the manifest is normally absent,
//! so the function returns `false`.  The result is computed once on
//! first call and cached in a function-local static (Meyers
//! singleton) gated by `__cxa_guard`.
//!
//! \return `true` when running on an MF instrument; `false`
//!         elsewhere (the typical PC test-host case).
bool runningOnMfDevice();                                       // @0x2ec580

//! \brief Single-shot variant of \ref runningOnMfDevice that
//!        consults a caller-supplied manifest path.
//!
//! Parses the manifest at \p manifestPath as JSON, runs the same
//! `device`-prefix check as the 0-arg form, but does **not** cache
//! the result and does **not** swallow exceptions thrown by the
//! `boost::property_tree` JSON reader — a malformed manifest will
//! propagate.
//!
//! \param manifestPath  Path to a LabOne manifest JSON file.  If the
//!        file does not exist the path-arg `readManifest` returns an
//!        empty ptree and the function reports `false`.
//! \return `true` iff the manifest's `device` value begins with `mf`.
bool runningOnMfDevice(std::string const& manifestPath);        // @0x2ec160

//! \brief Reports whether the compiler is hosted on an MF64 device
//!        (an MF-class device running the LabOne stack on a 64-bit
//!        Linux/ARM kernel).
//!
//! Stricter than `runningOnMfDevice()`: requires both that the
//! manifest's `device` key starts with `mf` *and* that its
//! `platform` key equals `linuxARM64`.  Caches like its non-64
//! sibling.
//!
//! \return `true` only when running on an MF64 instrument.
bool runningOnMf64Device();                                     // @0x2ec680

//! \brief Single-shot variant of \ref runningOnMf64Device taking an
//!        explicit manifest path.
//!
//! \param manifestPath  Path to a LabOne manifest JSON file.
//! \return `true` iff the manifest's `device` value begins with `mf`
//!         AND its `platform` value equals `linuxARM64`.
bool runningOnMf64Device(std::string const& manifestPath);      // @0x2ec3d0

// ---------------------------------------------------------------------------
// Filesystem helpers
// ---------------------------------------------------------------------------

//! \brief Tests whether \p p names a `/media/sd*` mount whose
//!        underlying block device node is present on the host.
//!
//! Matches \p p against the anchored regular expression
//! `^/media/sd[a-z][0-9]+$` (i.e. `sd<letter><digits>`); if it
//! matches, appends `"/dev"` to the path and calls
//! `boost::filesystem::status` on the result, returning `true` only
//! when the status call succeeds **and** the resulting `file_type`
//! equals `character_file` (5).  In other words: "is this a media
//! mount that actually has a `/dev` character-device entry under
//! it?".
//!
//! \param p  Filesystem path to test (the mount path, not the
//!           `/dev` child — the child is appended internally).
//! \return `true` iff \p p matches the pattern and `<p>/dev` exists
//!         and is a character-device node.
bool hasMediaDevNode(std::string const& p);                     // @0x2eb550

//! \brief Recursively creates \p dir and any missing parent
//!        directories, then verifies the result is writeable.
//!
//! Equivalent to `boost::filesystem::create_directories` but with a
//! writeability post-check: if the directory cannot be written to
//! after creation, throws a \ref zhinst::Exception with the message
//! `"Could not access directory '<path>'."`.  The underlying
//! `create_directories` call uses the non-throwing `error_code`
//! overload, so a pre-existing directory or a failed mkdir alone
//! does not throw — only the writeability check gates success.
//!
//! \param dir  Target directory.  Already-existing writeable
//!             directories are accepted as success.
//! \throws zhinst::Exception  When \p dir is not writeable after the
//!         `create_directories` call returns.  The exception carries
//!         the integer error code `0x8011`.
//! \binarynote The error code attached to the thrown exception is
//!             `0x8011`, which occupies the
//!             `ZIResult_enum::ApiBufferTooSmall` slot in the
//!             top-level error-code enum.  This is the value the
//!             original binary emits and is preserved verbatim;
//!             the semantic mismatch between the slot's name and a
//!             directory-permissions failure is a quirk of the
//!             original code, not a reconstruction choice.
void makeDirectories(boost::filesystem::path const& dir);       // @0x2cdef0

//! \brief Marks \p p as hidden in the host's file manager.
//!
//! On Linux this is a no-op (the convention is a leading `.` in the
//! filename, applied at create-time elsewhere).  The Windows variant
//! of this helper would call `SetFileAttributesW` but is not present
//! in this binary build.
//!
//! \param p  Path to mark.  Ignored on Linux.
void markFileHidden(boost::filesystem::path const& p);          // @0x2eb940

//! \brief Configures `boost::filesystem` for Unicode path handling
//!        as required by the host platform.
//!
//! On Linux this is a no-op (UTF-8 is the natural path encoding).
//! The Windows variant would install a UTF-8 codecvt facet but is
//! not present in this binary build.
void initBoostFilesystemForUnicode();                           // @0x2ec020

} // namespace zhinst
