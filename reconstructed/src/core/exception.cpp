// ============================================================================
// Reconstructed from disassembly of _seqc_compiler.so
//
// zhinst::Exception and the two derived classes used by the seqc
// compiler (ZIAWGCompilerException, ZIAWGOptimizerException).
//
// See include/zhinst/exception.hpp for the full layout reconstruction
// and the address map of every reconstructed function.
//
// IMPORTANT: This is a SOURCE-LEVEL reconstruction. The binary uses
// multiple inheritance (std::bad_exception + boost::exception) and
// stores fields via that MI layout. This source approximates the
// observable behaviour (what(), code(), description(), throw/catch
// matching against std::exception&) without requiring the consumer to
// pull in <boost/exception/exception.hpp>.
// ============================================================================
#include "zhinst/core/exception.hpp"

namespace zhinst {

// ---------------------------------------------------------------------------
// Exception::Exception() — 0x2e5410
//
// Binary behaviour:
//   - Zeroes boost::exception subobject (data_/throw_function_/throw_file_),
//     sets throw_line_ to -1
//   - Sets vptrs to b09fa0 / b09fc8
//   - Calls make_error_code(0x8000) (= ZIResult_enum sentinel value), stores
//     into errorCode_
//   - Hardcodes message_ = "ZIException" via SSO inline writes:
//       byte 0       = 0x16  (length 11 << 1)
//       qword [+1]   = 0x7065637845495a  ("ZIExcept")  + first 'i'
//       qword [+9]   = 0x6e6f6974        ("tion\0\0\0\0")
//       qword [+0x11]= 0
// ---------------------------------------------------------------------------
Exception::Exception()
    : errorCode_(makeDefaultErrorCode())
    , message_("ZIException")
{
}

// ---------------------------------------------------------------------------
// Exception::Exception(std::string msg) — 0x2e54b0
//
// Binary behaviour:
//   - Same vtable + boost::exception init as default ctor
//   - errorCode_ = make_error_code(0x8000)
//   - Moves `msg` into message_ via libc++ string move (steals
//     SSO/heap representation)
//   - If `msg` is empty (SSO byte 0 == 0), invokes
//     ErrorCodeTraits<error_code>::defaultMessage(errorCode_) to
//     populate message_ from the error category, then deletes the
//     moved-out heap buffer if any.
// ---------------------------------------------------------------------------
Exception::Exception(std::string msg)
    : errorCode_(makeDefaultErrorCode())
    , message_(std::move(msg))
{
}

// ---------------------------------------------------------------------------
// Exception::Exception(ErrorCode const& code) — 0x2e55b0
//
// Binary behaviour:
//   - Copies the 24-byte error_code into errorCode_
//   - Calls boost::system::error_code::to_string() to get the
//     stringified code (e.g. "system:42")
//   - Inserts "ZIException with status code: " (30 bytes, .rodata
//     0x90c6c6) at position 0 — yielding
//     "ZIException with status code: system:42"
//   - Moves the result into message_
//   - If empty, falls back to defaultMessage()
// ---------------------------------------------------------------------------
Exception::Exception(ErrorCode const& code)
    : errorCode_(code)
{
    std::string s = code.to_string();
    s.insert(0, "ZIException with status code: ");
    message_ = std::move(s);
}

// ---------------------------------------------------------------------------
// Exception::Exception(ErrorCode const& code, std::string msg) — 0x2e5700
//
// Binary behaviour:
//   - Copies error_code into errorCode_
//   - Moves `msg` into message_ verbatim (no prefix)
//   - Empty fallback: same defaultMessage path as the other ctors
// ---------------------------------------------------------------------------
Exception::Exception(ErrorCode const& code, std::string msg)
    : errorCode_(code)
    , message_(std::move(msg))
{
}

// ---------------------------------------------------------------------------
// Exception::Exception(GenericErrorDescription<ErrorCode>) — 0x2e5810
//
// Binary behaviour: pure move-construct from the description.
//   - Copies xmm[+0x00..+0x0F]   from desc → errorCode_ value bytes
//   - Copies qword[+0x10]        from desc → errorCode_ category ptr
//   - Moves desc.message into message_ (xmm + size qword), zeros
//     desc.message in place to mark moved-from
// ---------------------------------------------------------------------------
Exception::Exception(GenericErrorDescription<ErrorCode> desc)
    : errorCode_(std::move(desc.code))
    , message_(std::move(desc.message))
{
}

// ---------------------------------------------------------------------------
// Exception::~Exception() — 0x2e7720
//
// Binary behaviour:
//   - Resets vptrs back to base class layouts (required during
//     destructor chain)
//   - If message_ is heap-allocated (SSO byte & 1), calls
//     operator delete on the buffer
//   - Resets boost::exception vptr to base
//   - If boost_data_ != nullptr, calls its virtual release() (vtable+0x20)
//     and nulls the field
//   - Chains to ~bad_exception
//
// Source-level: the base std::exception dtor and std::string dtor cover
// what we can observe from this layer.
// ---------------------------------------------------------------------------
Exception::~Exception() = default;

// ---------------------------------------------------------------------------
// Exception::what() const noexcept — 0x2e5870
//
// Binary code:
//   test BYTE PTR [rdi+0x48], 0x1     ; check SSO/heap discriminator bit
//   je   short                         ; SSO branch
//   mov  rax, [rdi+0x58]              ; heap: return data ptr
//   ret
//   <short:>
//   add  rax, 0x49                    ; SSO: return ptr to inline buffer
//   ret
//
// Equivalent to message_.c_str() in libc++.
// ---------------------------------------------------------------------------
const char* Exception::what() const noexcept
{
    return message_.c_str();
}

// ---------------------------------------------------------------------------
// Exception::code() const — 0x2e5890
//
// Binary returns boost::system::error_code by value via sret:
//   movups xmm0, [rsi+0x30]   ; copy first 16 bytes of errorCode_
//   movups [rdi],     xmm0    ; into return slot
//   mov    rcx, [rsi+0x40]    ; copy qword 3 (category*)
//   mov    [rdi+0x10], rcx
//
// Equivalent to `return errorCode_;`
// ---------------------------------------------------------------------------
ErrorCode Exception::code() const noexcept
{
    return errorCode_;
}

// ---------------------------------------------------------------------------
// Exception::description() const — 0x2e58b0
//
// Binary code:
//   lea rax, [rdi+0x30]
//   ret
//
// Returns pointer to the internal errorCode_ field — semantically an
// alias for &this->errorCode_. (Despite the name, the binary returns
// only the error_code, not the message; the field name `description_`
// in earlier notes was wrong.)
// ---------------------------------------------------------------------------
ErrorCode const* Exception::description() const noexcept
{
    return &errorCode_;
}

// ===========================================================================
// All 26 derived exception classes — macro-generated.
//
// Every subclass follows the same binary pattern:
//   default ctor: constructs std::string("<ClassName>"), forwards to
//                 Exception(string), then resets vptrs to class-specific values.
//   string ctor:  moves msg into Exception(string), resets vptrs.
//   dtor:         structurally identical to Exception::~Exception.
//
// Binary constructor addresses are documented per-class in exception.hpp.
// ===========================================================================

#define ZHINST_DEFINE_EXCEPTION(ClassName)                                   \
    ClassName::ClassName()                                                    \
        : Exception(std::string(#ClassName))                                 \
    {}                                                                       \
    ClassName::ClassName(std::string msg)                                     \
        : Exception(std::move(msg))                                          \
    {}                                                                       \
    ClassName::~ClassName() = default

ZHINST_DEFINE_EXCEPTION(ZIAPIException);                   // 0x2e58c0 / 0x2e5930
ZHINST_DEFINE_EXCEPTION(ZIIOException);                    // 0x2e5a30 / 0x2e5aa0
ZHINST_DEFINE_EXCEPTION(ZIDeviceException);                // 0x2e5c90 / 0x2e5cf0
ZHINST_DEFINE_EXCEPTION(ZISocketException);                // 0x2e5d60 / 0x2e5dc0
ZHINST_DEFINE_EXCEPTION(ZIOverflowException);              // 0x2e5e30 / 0x2e5ea0
ZHINST_DEFINE_EXCEPTION(ZIUnderrunException);              // 0x2e5f10 / 0x2e5f80
ZHINST_DEFINE_EXCEPTION(ZITimeoutException);               // 0x2e5ff0 / 0x2e6060
ZHINST_DEFINE_EXCEPTION(ZIReadOnlyException);              // (none)  / 0x2e60d0
ZHINST_DEFINE_EXCEPTION(ZIWriteOnlyException);             // (none)  / 0x2e6190
ZHINST_DEFINE_EXCEPTION(ZINotFoundException);              // 0x2e6250 / 0x2e62c0
ZHINST_DEFINE_EXCEPTION(ZIInvalidKeywordException);        // (none)  / 0x2e6330
ZHINST_DEFINE_EXCEPTION(ZITypeMismatchException);          // 0x2e63f0 / 0x2e6480
ZHINST_DEFINE_EXCEPTION(ZIOutOfRangeException);            // 0x2e64f0 / 0x2e6560
ZHINST_DEFINE_EXCEPTION(ZIInterruptException);             // 0x2e65d0 / 0x2e6640
ZHINST_DEFINE_EXCEPTION(ZIInternalException);              // 0x2e66b0 / 0x2e6760
ZHINST_DEFINE_EXCEPTION(ZIDeviceNotVisibleException);      // 0x2e6820 / 0x2e68f0
ZHINST_DEFINE_EXCEPTION(ZIDeviceNotFoundException);        // 0x2e69b0 / 0x2e6a80
ZHINST_DEFINE_EXCEPTION(ZIDeviceInUseException);           // 0x2e6b40 / 0x2e6c00
ZHINST_DEFINE_EXCEPTION(ZIDeviceInterfaceException);       // 0x2e6cc0 / 0x2e6d90
ZHINST_DEFINE_EXCEPTION(ZIDeviceConnectionTimeoutException); // 0x2e6e50 / 0x2e6f20
ZHINST_DEFINE_EXCEPTION(ZIDeviceDifferentInterfaceException); // 0x2e6fe0 / 0x2e70c0
ZHINST_DEFINE_EXCEPTION(ZIDeviceFWException);              // 0x2e7180 / 0x2e7230
ZHINST_DEFINE_EXCEPTION(ZIAWGCompilerException);           // 0x2e72f0 / 0x2e7360
ZHINST_DEFINE_EXCEPTION(ZIAWGOptimizerException);          // 0x2e73d0 / 0x2e7460
ZHINST_DEFINE_EXCEPTION(ZIVersionException);               // 0x2e74d0 / 0x2e7540
ZHINST_DEFINE_EXCEPTION(ZIIllegalPathException);           // 0x2e75b0 / 0x2e7620

#undef ZHINST_DEFINE_EXCEPTION

} // namespace zhinst
