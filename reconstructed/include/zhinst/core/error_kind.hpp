// ============================================================================
// Reconstructed from disassembly of _seqc_compiler.so
// error_kind.hpp
//
// `ErrorKind` enum + the four ErrorKind-based interop helpers from the
// `api_error_translation::core` cluster.  These four were chosen as a
// scoped subset because none of them depend on `boost::system::error_code`
// or `boost::system::error_category`; they operate purely on the
// `ErrorKind` value and a single opaque category-singleton pointer.
//
// Symbols reconstructed (verified addresses in the binary):
//
//   - `zhinst::toApiCode(ErrorKind)`           @ 0x2e5280  (size 0x29)
//   - `zhinst::make_error_condition(ErrorKind)` @ 0x2e50c0 (size 0x0f)
//   - `zhinst::toZiErrorKind(ErrorKind)`        @ 0x2e5240 (size 0x12)
//   - `::fromZiErrorKind(ZIErrorKind_enum)` (in `zhinst` namespace)
//                                                @ 0x2e5260 (size 0x11)
//
// `ZIErrorKind_enum` is mangled `16ZIErrorKind_enum` (top-level, no
// namespace) — this is the C-API enum that lives in the public ZI
// header outside the `zhinst::` namespace.  We declare it at global
// scope to preserve the mangling.
//
// `ErrorKind` enum membership (10 named values + the implicit default
// `Unknown` for out-of-range inputs) is recovered from the dispatch
// table of the anon-namespace `ErrorKindCategory::message(int)` helper
// at 0x2e52c0 — see `notes/api_error_translation.md`.
//
// `make_error_condition(ErrorKind)` returns the binary's
// `boost::system::error_condition` (16 B: int value + category pointer
// in `rax:rdx`).  Recon stand-in `ErrorCondition` matches that calling
// convention (sizeof == 16, first 4 B = value, last 8 B = category
// pointer).  Mangling matches exactly because Itanium does not encode
// free-function return types.
// ============================================================================

#ifndef ZHINST_CORE_ERROR_KIND_HPP
#define ZHINST_CORE_ERROR_KIND_HPP

#include <cstdint>

// ----------------------------------------------------------------------------
// `ZIErrorKind_enum` — top-level (no namespace) C-API enum.
//
// Mangling evidence: the binary mangles `fromZiErrorKind` as
// `_ZN6zhinst15fromZiErrorKindE16ZIErrorKind_enum` — the
// `16ZIErrorKind_enum` portion is an unqualified `<source-name>` token,
// so the type lives at global scope.
//
// Underlying type is 32-bit (signed int), per disassembly @0x2e5260
// which uses `cmp $0xa, %edi` (full 32-bit comparison) on the parameter.
// Value range mirrors `zhinst::ErrorKind`; out-of-range inputs are
// clamped to `ZI_KIND_UNKNOWN` (= 2) by `fromZiErrorKind`.
// ----------------------------------------------------------------------------
//! \brief C-API enum for the LabOne error-kind taxonomy.
//!
//! Top-level (non-namespaced) enum exposed by the public C API; the
//! `zhinst::` translation helpers `fromZiErrorKind` and `toZiErrorKind`
//! shuttle values between this and the typed `zhinst::ErrorKind`.
//! Out-of-range values are clamped to `ZI_KIND_UNKNOWN`.
enum ZIErrorKind_enum : int {
    ZI_KIND_OK            = 0,  //!< No error.
    ZI_KIND_CANCELLED     = 1,  //!< Operation cancelled by caller.
    ZI_KIND_UNKNOWN       = 2,  //!< Default/fallback for unrecognised values.
    ZI_KIND_NOT_FOUND     = 3,  //!< Requested resource missing.
    ZI_KIND_OVERWHELMED   = 4,  //!< Server overloaded; retry later.
    ZI_KIND_BAD_REQUEST   = 5,  //!< Caller-side request malformed.
    ZI_KIND_UNIMPLEMENTED = 6,  //!< Feature not implemented on target.
    ZI_KIND_INTERNAL      = 7,  //!< Internal server error.
    ZI_KIND_UNAVAILABLE   = 8,  //!< Service temporarily unavailable.
    ZI_KIND_TIMEOUT       = 9,  //!< Deadline elapsed before completion.
};

namespace zhinst {

// ----------------------------------------------------------------------------
// `zhinst::ErrorKind` — typed counterpart to `ZIErrorKind_enum`.
//
// Underlying type is 16-bit (uint16_t), per disassembly:
//   - `toZiErrorKind(ErrorKind)` @0x2e5240 uses `cmp $0xa, %di` (16-bit).
//   - `make_error_condition(ErrorKind)` @0x2e50c0 stores the value via
//     `mov %edi, %eax`; only the low 16 bits are meaningful.
//
// Membership recovered from the dispatch-table message strings of the
// anon-namespace `ErrorKindCategory::message(int)` @0x2e52c0:
//
//     0=Ok, 1=Cancelled, 2=Unknown, 3=NotFound, 4=Overwhelmed,
//     5=BadRequest, 6=Unimplemented, 7=Internal, 8=Unavailable,
//     9=Timeout
//
// Inputs >= 10 are clamped to `Unknown` by `toZiErrorKind` and to the
// "Unknown" message by the category's message dispatcher.
// ----------------------------------------------------------------------------
//! \brief C++-typed error-kind taxonomy used by the LabOne API
//! interop layer.
//!
//! Each value names a category of failure surfaced through
//! `Exception::code()` and the `ZIException` hierarchy.  The 10 named
//! values are the slots that `ErrorKindCategory::message(int)`
//! recognises by name; values outside this range are reported as
//! `Unknown` by the category's renderer and clamped to `Unknown` by
//! `toZiErrorKind`.
enum class ErrorKind : std::uint16_t {
    Ok            = 0,   //!< No error.
    Cancelled     = 1,   //!< Operation cancelled by caller.
    Unknown       = 2,   //!< Default/fallback for unrecognised values.
    NotFound      = 3,   //!< Requested resource missing.
    Overwhelmed   = 4,   //!< Server overloaded; retry later.
    BadRequest    = 5,   //!< Caller-side request malformed.
    Unimplemented = 6,   //!< Feature not implemented on target.
    Internal      = 7,   //!< Internal server error.
    Unavailable   = 8,   //!< Service temporarily unavailable.
    Timeout       = 9,   //!< Deadline elapsed before completion.
};

// ----------------------------------------------------------------------------
// `zhinst::ErrorCondition` — recon stand-in for `boost::system::error_condition`.
//
// `make_error_condition(ErrorKind)` @0x2e50c0 returns a 16-byte struct
// in `rax:rdx`:
//   - `eax`  = `static_cast<int>(kind)`              (low 32-bit value)
//   - `rdx`  = `&singleErrorKindCategory`            (category pointer)
//
// Layout below matches that calling convention (4 B value + 4 B pad +
// 8 B category-pointer = 16 B; trivially copyable; standard layout).
// The opaque `category` pointer stores the address of an
// `boost::system::error_category` singleton in the binary; our
// stand-in stores the address of the recon-local `singleErrorKindCategory`
// sentinel object.  Free-function return types are not encoded in
// Itanium mangling, so the symbol matches the binary regardless of
// the chosen wrapper.
// ----------------------------------------------------------------------------
//! \brief Recon stand-in for `boost::system::error_condition`.
//!
//! Pairs an integer error value with an opaque category-singleton
//! pointer; produced by `make_error_condition(ErrorKind)`.  The
//! category pointer is opaque at the source level and only meaningful
//! for downstream comparisons against well-known singletons.
struct ErrorCondition {
    //! \brief Numeric error value (the `ErrorKind` cast to `int`).
    int         value     = 0;
    //! \brief Padding to align the category pointer to 8 bytes.
    int         _pad      = 0;
    //! \brief Opaque pointer to the originating category singleton.
    void const* category  = nullptr;
};

// ----------------------------------------------------------------------------
// API surface
// ----------------------------------------------------------------------------

//! \brief Map an `ErrorKind` to the LabOne C-API result code that
//! best represents it.
//!
//! Most kinds collapse to the generic `ApiKeywordNotResolved` sentinel
//! (`0x8000`).  Two kinds receive a more specific code:
//!
//!   - `BadRequest`  → `ApiHF2NotSupported`   (`0x801f`)
//!   - `Timeout`     → `ApiConnectionInvalid` (`0x800d`)
//!
//! `Ok` returns 0 unchanged.  The mapping is the binary's; downstream
//! callers consume the result as a `ZIResult_enum`.
//!
//! \param kind  Error kind to translate.
//! \return Numeric LabOne-API result code; 0 for `Ok`, special values
//!         for `BadRequest`/`Timeout`, otherwise `0x8000`.
//! \binarynote The `BadRequest → ApiHF2NotSupported` mapping looks
//! incongruent at the C++ level but is what the binary emits at
//! 0x2e5280; preserved verbatim for compatibility.
int toApiCode(ErrorKind kind);                                    // @ 0x2e5280

//! \brief Wrap an `ErrorKind` in an `ErrorCondition` bound to the
//! library's error-kind category singleton.
//!
//! The returned condition carries `static_cast<int>(kind)` as its
//! value and a pointer to the global `singleErrorKindCategory`
//! sentinel as its category.  Mirrors the binary's
//! `boost::system::error_condition` constructor pattern exactly
//! (16-byte sret in `rax:rdx`).
//!
//! \param kind  Error kind to wrap.
//! \return `ErrorCondition{value=int(kind), category=&singleErrorKindCategory}`.
ErrorCondition make_error_condition(ErrorKind kind);              // @ 0x2e50c0

//! \brief Convert a typed `ErrorKind` to its C-API counterpart.
//!
//! Direct value passthrough for `kind < 10`; out-of-range inputs are
//! clamped to `ZI_KIND_UNKNOWN` (= 2).  Symmetric with
//! `fromZiErrorKind`.
//!
//! \param kind  Typed error kind.
//! \return Equivalent `ZIErrorKind_enum` value.
ZIErrorKind_enum toZiErrorKind(ErrorKind kind);                   // @ 0x2e5240

}  // namespace zhinst

//! \brief Convert a C-API error kind to its typed `zhinst::ErrorKind`
//! counterpart.
//!
//! Lives in the `zhinst::` namespace (despite taking a top-level
//! `ZIErrorKind_enum` argument), per the binary's mangling
//! `_ZN6zhinst15fromZiErrorKindE16ZIErrorKind_enum`.  Direct value
//! passthrough for `kind < 10`; out-of-range inputs are clamped to
//! `ErrorKind::Unknown` (= 2).
//!
//! \param kind  C-API error kind.
//! \return Equivalent `zhinst::ErrorKind` value.
namespace zhinst {
ErrorKind fromZiErrorKind(::ZIErrorKind_enum kind);               // @ 0x2e5260
}

#endif  // ZHINST_CORE_ERROR_KIND_HPP
