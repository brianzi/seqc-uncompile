// ============================================================================
// Reconstructed from disassembly of _seqc_compiler.so
// StaticResources and GlobalResources — subclass implementations.
//
// Split from resources.cpp Batch 7 because the
// getVariable override alone is ~270 lines of active code. The CMake
// glob `file(GLOB src/*.cpp)` picks this file up automatically.
//
// All method addresses below refer to offsets in _seqc_compiler.so.
// ============================================================================

#include "zhinst/runtime/resources.hpp"
#include "zhinst/core/error_messages.hpp"

#include <cstdint>
#include <cstring>
#include <functional>
#include <memory>
#include <string>

namespace zhinst {

// External globals (defined in error_messages.cpp).
extern const std::string constAwgIntegrationTrigger;
extern const std::string zsyncDataPqscRegister;
extern const std::string zsyncDataPqscDecoder;

// ============================================================================
// StaticResources::StaticResources — @0x129cb0
//
// Disassembly walk (0x129cb0..0x129d61):
//   1. Construct SSO string "static" on stack: byte[0]=0x0c (len 6<<1),
//      dword "stat", word "ic", null terminator.
//   2. Zero a weak_ptr (xorps+movaps) for the parent param.
//   3. Call Resources::Resources("static", empty_weak_ptr) @0x1e3420.
//   4. Clean up temp weak_ptr (release_weak if ctrl non-null) and temp
//      string (free if heap-allocated).
//   5. Set vptr to StaticResources vtable @0xb03940+0x10.
//   6. Store 0 to [this+0xd8] → usedSampleRate_ = false.
//   7. Copy the std::function `logger` parameter:
//      - Read logger's callable ptr at [r14+0x20].
//      - If null → store null to [this+0x100].
//      - If ptr == internal buffer (r14 itself) → clone in place:
//        set [this+0x100] = &this->functionStorage_ (this+0xe0),
//        call vtable[0x18] (clone_into) on the source callable.
//      - Else (external allocation) → call vtable[0x10] (clone),
//        store returned ptr to [this+0x100].
// ============================================================================
StaticResources::StaticResources(
    std::function<void(std::string const&)> const& logger)  // @0x129cb0
    : Resources(std::string("static"), std::weak_ptr<Resources>{}),
      usedSampleRate_(false)
{
    // Set vptr (implicit in C++ via vtable dispatch, but noted: binary
    // explicitly writes b03950 to [this+0x00] at 129d0f).

    // Copy the std::function.
    // The libc++ layout of std::function<void(string const&)> is 48B:
    //   [0x00..0x1f] inline buffer (32B)
    //   [0x20]       __base* callable pointer (8B)
    // We model this as raw bytes + a pointer in the header
    // (functionStorage_ + functionPtr_). At the C++ level, the binary
    // is doing a manual deep copy of the std::function internals.
    // We reproduce this by reinterpreting our storage as a std::function
    // and using its copy constructor.
    //
    // ABI-fragile (libc++ specific). The header documents the layout; here
    // we use placement new for correctness.
    // Under libc++, std::function is 0x30 bytes (48) which spans the full
    // functionStorage_[0x20] + functionPtr_ + pad_108_ region — correct.
    // Under libstdc++, std::function is 0x20 bytes (32) which fits within
    // functionStorage_ alone — the extra fields are structural padding.
#ifndef _LIBCPP_VERSION
    static_assert(sizeof(std::function<void(std::string const&)>) <= 0x28,
                  "std::function size exceeds StaticResources storage");
#else
    static_assert(sizeof(std::function<void(std::string const&)>) == 0x30,
                  "Expected libc++ std::function to be 48 bytes");
#endif
    auto* target = reinterpret_cast<std::function<void(std::string const&)>*>(
        &functionStorage_);
    ::new (target) std::function<void(std::string const&)>(logger);
    functionPtr_ = reinterpret_cast<void*>(target);
}

// ============================================================================
// StaticResources::~StaticResources — D1 @0x129db0, D0 @0x129e00
//
// D1 (base-object dtor):
//   1. Set vptr to StaticResources vtable.
//   2. Destroy the std::function stored at functionStorage_/functionPtr_:
//      - If functionPtr_ == &functionStorage_ (inline) → call vtable[0x20]
//        (destroy_in_place). This destroys the callable without freeing
//        the inline buffer.
//      - If functionPtr_ != null and != inline → call vtable[0x28]
//        (destroy+dealloc). This destroys AND frees the external buffer.
//      - If functionPtr_ == null → skip.
//   3. Tail-call Resources::~Resources() @0x12a8f0.
//
// D0 (complete-object dtor):
//   Same as D1 but after Resources::~Resources(), calls
//   operator delete(this, 0x110).
//
// In C++ we express this as a defaulted dtor — the placement-new'd
// std::function is manually destroyed, and Resources::~Resources is
// called by the implicit base-class teardown.
// ============================================================================
StaticResources::~StaticResources() {  // D1 @0x129db0, D0 @0x129e00
    auto* target = reinterpret_cast<std::function<void(std::string const&)>*>(
        &functionStorage_);
    target->~function();
}

// ============================================================================
// StaticResources::getVariable — @0x129e60
//
// Virtual override of Resources::getVariable. Intercepts certain
// well-known variable names to:
//   (a) Set usedSampleRate_ = true when accessing "DEVICE_SAMPLE_RATE".
//   (b) Log deprecation warnings (via the logger callback) for 5 legacy
//       constant names that have been replaced by newer APIs.
//
// Algorithm (0x129e60..0x12a2a1):
//
//   1. Pre-check: if name == "DEVICE_SAMPLE_RATE" (18 bytes, SSE compare
//      at 0x8fc450/0x8fc460):
//        → set this->usedSampleRate_ (+0xD8) = true.
//
//   2. Delegate to Resources::getVariable(name) @0x1eb0a0.
//      If result [rbx] is null AND logger is null → __throw_bad_function_call.
//      If result is null (but logger non-null) → just return null
//      (the binary actually calls __throw_bad_function_call at 0x12a2af
//      for all three logger-invoke paths if logger is null — this is the
//      std::function null-check).
//
//   3. Deprecation checks — 5 names compared:
//
//      (a) "AWG_MONITOR_TRIGGER" (19B, SSE overlap @0x8fc470/0x8fc480):
//          → format(DeprecatedConst, name, "'startQA' function")
//      (b) constAwgIntegrationTrigger (global string, bcmp):
//          → format(DeprecatedConst, name, "'startQA' function")
//      (c) "AWG_INTEGRATION_ARM" (19B, SSE overlap @0x8fc490/0x8fc4a0):
//          → format(DeprecatedConst, name, "'startQA' function")
//      (d) zsyncDataPqscRegister (global string, bcmp):
//          → format(DeprecatedConst, name, "'ZSYNC_DATA_PROCESSED_A'")
//      (e) zsyncDataPqscDecoder (global string, bcmp):
//          → format(DeprecatedConst, name, "'ZSYNC_DATA_PROCESSED_B'")
//
//      On each match: call ErrorMessages::format(DeprecatedConst=52, name,
//      hint_literal) → string; invoke logger callback at [this+0x100]
//      via vtable+0x30 (std::function::operator()). Then fall through to
//      return the Variable* — deprecation is a WARNING, not a block.
//
//   4. Return the Variable* from step 2 (always in rax via rbx).
//
// The SSE "overlap trick" for 19-byte strings:
//   movdqu xmm0, [ptr+0]   — bytes 0..15
//   movdqu xmm1, [ptr+3]   — bytes 3..18
//   pcmpeqb each against rodata reference, pand, pmovmskb == 0xffff.
//   The overlap at bytes 3..15 is checked twice (harmless).
// ============================================================================
Resources::Variable* StaticResources::getVariable(std::string const& name)  // @0x129e60
{
    // (1) Flag "DEVICE_SAMPLE_RATE" access.
    if (name == "DEVICE_SAMPLE_RATE") {
        usedSampleRate_ = true;
    }

    // (2) Delegate to base class.
    Variable* var = Resources::getVariable(name);

    // (3) Deprecation checks — warn but still return the variable.
    // Per binary @0x129ee6/0x129eea: if var is null, skip the deprecation
    // checks entirely and just return null.  Otherwise the warning would
    // mask the real "tried to access unknown variable" error from the
    // caller (e.g. SHFQA + ZSYNC_DATA_PQSC_REGISTER, which is HDAWG/SHFSG
    // only — the binary wants the unknown-variable error to surface).
    if (!var) {
        return nullptr;
    }

    auto* fn = reinterpret_cast<std::function<void(std::string const&)>*>(
        &functionStorage_);

    if (name == "AWG_MONITOR_TRIGGER" ||
        name == constAwgIntegrationTrigger ||
        name == "AWG_INTEGRATION_ARM") {
        std::string msg = ErrorMessages::format(
            DeprecatedConst, std::string(name), "'startQA' function");
        (*fn)(msg);
    } else if (name == zsyncDataPqscRegister) {
        std::string msg = ErrorMessages::format(
            DeprecatedConst, std::string(name), "'ZSYNC_DATA_PROCESSED_A'");
        (*fn)(msg);
    } else if (name == zsyncDataPqscDecoder) {
        std::string msg = ErrorMessages::format(
            DeprecatedConst, std::string(name), "'ZSYNC_DATA_PROCESSED_B'");
        (*fn)(msg);
    }

    return var;
}

// ============================================================================
// GlobalResources ctor/dtor are defined in global_resources.cpp.
// ============================================================================

} // namespace zhinst
