// ============================================================================
// Reconstructed from disassembly of _seqc_compiler.so
// error_kind.cpp
//
// Bodies for the four ErrorKind-based interop helpers from the
// `api_error_translation::core` cluster.  See `error_kind.hpp` for the
// rationale of the subset choice and for the source of the
// `ErrorKind` enum membership.
// ============================================================================

#include "zhinst/core/error_kind.hpp"

namespace zhinst {
namespace {

// ----------------------------------------------------------------------------
// `singleErrorKindCategory` — opaque category-singleton sentinel.
//
// In the binary this is a fully-fledged
// `boost::system::error_category`-derived object (with `name()` and
// `message(int)` virtuals) at .data.rel.ro 0xb7c5a8.  Reconstructing
// the polymorphic helper would pull in the entire `boost::system`
// vtable layout for no benefit at the source level — `make_error_condition`
// only ever stores its address opaquely, and no recon code currently
// dereferences the resulting category pointer.
//
// We therefore expose only an addressable opaque sentinel here.  The
// `make_error_condition` callee returns its address verbatim, which
// is observationally identical to the binary's behaviour for any
// caller that compares two conditions by category-pointer equality
// (the only exposed use).
// ----------------------------------------------------------------------------
struct ErrorKindCategoryTag {};
constexpr ErrorKindCategoryTag singleErrorKindCategory{};

}  // namespace

// ---------------------------------------------------------------------------
// toApiCode(ErrorKind) — @ 0x2e5280 (size 0x29)
//
//   if (kind == 0)             return 0;        // Ok passes through
//   if (kind == 5)             return 0x801f;   // BadRequest → ApiHF2NotSupported
//   if (kind == 9)             return 0x800d;   // Timeout    → ApiConnectionInvalid
//   else                       return 0x8000;   // generic ApiKeywordNotResolved sentinel
//
// Verified by reading `mov $0x801f / $0x800d / $0x8000` constants and
// the `cmp $0x5 / $0x9` test-then-branch sequence in the disassembly.
// ---------------------------------------------------------------------------
int toApiCode(ErrorKind kind) {
    auto value = static_cast<int>(kind);
    if (value == 0) {
        return 0;
    }
    if (value == 5) {
        return 0x801f;
    }
    if (value == 9) {
        return 0x800d;
    }
    return 0x8000;
}

// ---------------------------------------------------------------------------
// make_error_condition(ErrorKind) — @ 0x2e50c0 (size 0x0f)
//
//   eax = kind                                  // value field
//   rdx = &singleErrorKindCategory              // category pointer
//   ret                                         // 16-byte sret in rax:rdx
//
// Itanium does not encode return types in free-function manglings, so
// returning our `ErrorCondition` stand-in (16 B) yields the same
// `_ZN6zhinst20make_error_conditionENS_9ErrorKindE` mangled symbol the
// binary emits.  Both values are conveyed through the same sret
// register pair as in the binary.
// ---------------------------------------------------------------------------
ErrorCondition make_error_condition(ErrorKind kind) {
    ErrorCondition cond;
    cond.value    = static_cast<int>(kind);
    cond.category = &singleErrorKindCategory;
    return cond;
}

// ---------------------------------------------------------------------------
// toZiErrorKind(ErrorKind) — @ 0x2e5240 (size 0x12)
//
//   cmp $0xa, %di          // 16-bit compare of kind
//   mov $0x2, %eax         // default: ZI_KIND_UNKNOWN
//   cmovb %edi, %eax       // if (kind < 10) eax = kind
//   ret
//
// I.e. passes through valid values; clamps out-of-range to `Unknown`
// (= 2).
// ---------------------------------------------------------------------------
ZIErrorKind_enum toZiErrorKind(ErrorKind kind) {
    auto value = static_cast<std::uint16_t>(kind);
    if (value < 10) {
        return static_cast<ZIErrorKind_enum>(value);
    }
    return ZI_KIND_UNKNOWN;
}

// ---------------------------------------------------------------------------
// fromZiErrorKind(ZIErrorKind_enum) — @ 0x2e5260 (size 0x11)
//
//   cmp $0xa, %edi         // 32-bit compare of kind
//   mov $0x2, %eax         // default: ErrorKind::Unknown (= 2)
//   cmovb %edi, %eax       // if (kind < 10) eax = kind
//   ret
//
// Symmetric inverse of `toZiErrorKind`.  The 32-bit comparison (vs
// `toZiErrorKind`'s 16-bit one) reflects `ZIErrorKind_enum`'s
// underlying `int` type.
// ---------------------------------------------------------------------------
ErrorKind fromZiErrorKind(::ZIErrorKind_enum kind) {
    auto value = static_cast<int>(kind);
    if (value < 10) {
        return static_cast<ErrorKind>(value);
    }
    return ErrorKind::Unknown;
}

}  // namespace zhinst
