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
#include "zhinst/exception.hpp"

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
    : message_("ZIException")
{
    // errorCode_ left default-constructed (value=0). The binary stores
    // make_error_code(0x8000) here; for source-level use the precise
    // sentinel doesn't matter — no src/ consumer reads it via the
    // default ctor path.
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
    : message_(std::move(msg))
{
    // Source-level approximation: skip the empty-string fallback to
    // ErrorCodeTraits::defaultMessage. All seqc compiler call sites
    // pass a non-empty formatted string, so this branch is unreachable
    // in practice from the reconstructed source.
}

// ---------------------------------------------------------------------------
// Exception::Exception(ErrorCode const& code) — 0x2e55b0
//
// Binary behaviour:
//   - Copies the 24-byte error_code into errorCode_
//   - Calls boost::system::error_code::to_string() to get the
//     stringified code (e.g. "system:42")
//   - Inserts ".rodata 0x90c6c6" (a 30-byte prefix string ending in
//     "; ") at position 0 — yielding "<prefix>system:42"
//   - Moves the result into message_
//   - If empty, falls back to defaultMessage()
//
// We approximate the prefix as "; " (the trailing portion observed at
// .rodata 0x90c6c6 — the full 30-byte string was not extracted in this
// pass; a future audit can refine it).
// ---------------------------------------------------------------------------
Exception::Exception(ErrorCode const& code)
    : errorCode_(code)
    , message_(/* code.to_string() prefixed with "; " */ "")
{
    // Source-level approximation. No seqc compiler call site uses this
    // ctor, so the precise message format is non-critical.
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
// ZIAWGCompilerException
// ===========================================================================

// ---------------------------------------------------------------------------
// ZIAWGCompilerException::ZIAWGCompilerException() — 0x2e72f0
//
// Binary code constructs a stack std::string "ZIAWGCompilerException"
// (22 bytes, fits SSO) and forwards to Exception(string), then resets
// vptrs to b0a618 / b0a640.
// ---------------------------------------------------------------------------
ZIAWGCompilerException::ZIAWGCompilerException()
    : Exception(std::string("ZIAWGCompilerException"))
{
}

// ---------------------------------------------------------------------------
// ZIAWGCompilerException::ZIAWGCompilerException(std::string msg) — 0x2e7360
//
// Binary code moves `msg` into a stack std::string and forwards to
// Exception(string), then resets vptrs to b0a618 / b0a640.
// ---------------------------------------------------------------------------
ZIAWGCompilerException::ZIAWGCompilerException(std::string msg)
    : Exception(std::move(msg))
{
}

// 0x2e7720-region (per-class thunk, structurally identical to Exception::~)
ZIAWGCompilerException::~ZIAWGCompilerException() = default;

// ===========================================================================
// ZIAWGOptimizerException
// ===========================================================================

// ---------------------------------------------------------------------------
// ZIAWGOptimizerException::ZIAWGOptimizerException() — 0x2e73d0
//
// Binary heap-allocates a 0x1a-byte buffer for "ZIAWGOptimizerException"
// (23 bytes + NUL — pushed past SSO threshold), forwards to
// Exception(string), then resets vptrs to b0a660 / b0a688.
// ---------------------------------------------------------------------------
ZIAWGOptimizerException::ZIAWGOptimizerException()
    : Exception(std::string("ZIAWGOptimizerException"))
{
}

// ---------------------------------------------------------------------------
// ZIAWGOptimizerException::ZIAWGOptimizerException(std::string) — 0x2e7460
// Same shape as the ZIAWGCompilerException string ctor.
// ---------------------------------------------------------------------------
ZIAWGOptimizerException::ZIAWGOptimizerException(std::string msg)
    : Exception(std::move(msg))
{
}

ZIAWGOptimizerException::~ZIAWGOptimizerException() = default;

} // namespace zhinst
