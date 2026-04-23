// ============================================================================
// Reconstructed from disassembly of _seqc_compiler.so
//
// zhinst::Exception — base class for the entire ZI exception hierarchy.
//
// Reconstructed in Phase 10.7d. Layout is documented in
// notes/struct_layouts.md (Exception entry).
//
// ----------------------------------------------------------------------------
// Inheritance (multiple inheritance, two non-empty bases):
//   primary  : std::bad_exception   (vptr at +0x00, no data)
//   secondary: boost::exception     (vptr at +0x08, ~24B of data following)
//
// Total derived-most size is 0x60 bytes (verified by
// `Exception::~Exception` calling `operator delete(this, 0x60)` at
// 0x2e7791).
//
// Layout (verified offsets):
//
//   +0x00  vptr (std::bad_exception primary)            [b09fa0]
//   +0x08  vptr (boost::exception secondary)            [b09fc8]
//   +0x10  void*  data_                  // boost::exception::data_
//          (refcounted error_info_container; released in dtor via
//           virtual `release()` at vtable+0x20 — see 0x2e776c)
//   +0x18  char const* throw_function_   // boost::exception (init: 0)
//   +0x20  char const* throw_file_       // boost::exception (init: 0)
//   +0x28  int         throw_line_       // boost::exception (init: -1)
//          (the qword at +0x28 is initialized to 0xFFFFFFFFFFFFFFFF;
//           low 32 bits are the line number sentinel)
//   +0x30  boost::system::error_code  errorCode_   // 24 bytes
//          (xmm at +0x30..+0x3F  = error_value (8B) + padding (8B);
//           qword at +0x40       = error_category const*)
//          NOTE: in this libboost ABI error_code is 24 bytes:
//          {int value, error_category const*, source_location source_}
//   +0x48  std::string  message_               // 24 bytes (libc++ SSO)
//          (SSO: byte 0 = (length<<1)|0, then up to 22 bytes inline,
//           terminator at +0x5F. Long: byte 0 = cap*2|1, qword[+0x50]
//           = size, qword[+0x58] = data ptr.)
//   +0x60  end of object
//
// Verified ctors:
//   0x2e5410  Exception()                                  // default
//   0x2e54b0  Exception(std::string msg)                   // by-value
//   0x2e55b0  Exception(boost::system::error_code const&)
//   0x2e5700  Exception(boost::system::error_code const&,
//                       std::string msg)
//   0x2e5810  Exception(GenericErrorDescription<error_code>)
//
// Verified observers:
//   0x2e5870  what() const noexcept   -> char const*
//             (returns SSO buffer ptr if short, else heap data ptr)
//   0x2e5890  code() const            -> boost::system::error_code (sret)
//             (returns errorCode_ by value)
//   0x2e58b0  description() const     -> error_code const*
//             (returns &errorCode_ — pointer to internal field)
//
// Verified dtor:
//   0x2e7720  ~Exception()
//             - Resets vptrs to base (b09fa0/b09fc8) — required by
//               C++ vtable rules during destructor chain
//             - Releases SSO/heap message_
//             - Calls boost::exception::data_->release() if non-null
//             - Chains to ~bad_exception
//             - operator delete(this, 0x60)
//
// ----------------------------------------------------------------------------
// GenericErrorDescription<T = boost::system::error_code>:
//
// Layout verified from Exception(GenericErrorDescription) ctor at
// 0x2e5810 — the parameter is a 48-byte object passed by reference,
// laid out as:
//   +0x00..+0x17  T               // 24 bytes  (boost::system::error_code)
//   +0x18..+0x2F  std::string     // 24 bytes  (message)
//
// The ctor moves both into the new Exception (zeroing the source
// string at +0x18..+0x28 to mark it as moved-from).
//
// Helper at 0x2ea190
//   ErrorCodeTraits<error_code>::asException(GenericErrorDescription)
// constructs an Exception in heap memory from a description.
//
// Helper at 0x2ea170
//   ErrorCodeTraits<error_code>::defaultMessage(error_code const&)
// is invoked from the (string) and (error_code) ctors when the input
// string is empty / SSO byte 0 == 0, to populate message_ from the
// error_category's message().
//
// ----------------------------------------------------------------------------
// Hierarchy (derived classes verified at 0x2e58c0..0x2e76xx):
//
//   ZIAPIException                   (vtable b09fe8)
//   ZIIOException                    (vtable b0a020 region)
//   ZIDeviceException
//   ZISocketException
//   ZIOverflowException
//   ZIUnderrunException
//   ZITimeoutException
//   ZIReadOnlyException
//   ZIWriteOnlyException
//   ZINotFoundException
//   ZIInvalidKeywordException
//   ZITypeMismatchException
//   ZIOutOfRangeException
//   ZIInterruptException
//   ZIInternalException
//   ZIDeviceNotVisibleException
//   ZIDeviceNotFoundException
//   ZIDeviceInUseException
//   ZIDeviceInterfaceException
//   ZIDeviceConnectionTimeoutException
//   ZIDeviceDifferentInterfaceException
//   ZIDeviceFWException
//   ZIAWGCompilerException           (vtable b0a618)  ← used by seqc compiler
//   ZIAWGOptimizerException          (vtable b0a660)  ← used by seqc compiler
//   ZIVersionException
//   ZIIllegalPathException
//
// All derived classes add NO data fields (verified by ZIAWGCompilerException
// at 0x2e7360: ctor only sets vptrs and forwards string to the base).
// They differ only in vtable identity (which controls what()'s
// derived-class string and how RTTI / exception matching works).
//
// ----------------------------------------------------------------------------
// Source-level reconstruction strategy:
//
// We reconstruct Exception as a single class deriving publicly from
// std::exception (NOT std::bad_exception). The semantic difference is
// negligible for source compilation:
//   - std::bad_exception itself derives from std::exception
//   - call sites only `throw X(msg)` and `catch (std::exception&)`
//   - the actual MI layout of the binary cannot be expressed in
//     portable source without bringing in <boost/exception/exception.hpp>
//     and that's outside our scope
//
// The fields message_, errorCode_, and a placeholder for the
// boost::exception data slot are kept so that what() / code() /
// description() compile and behave correctly.
// ============================================================================
#pragma once

#include <exception>
#include <string>
#include <system_error>
#include <utility>

namespace zhinst {

// Source-level approximation of boost::system::error_code that matches
// the binary's 24-byte size. Real implementation lives in <boost/system>.
// Kept opaque-ish to avoid coupling reconstructed source to a specific
// boost version.
struct ErrorCode {
    int                         value_     = 0;     // +0x00
    int                         _pad_      = 0;     // padding
    void const*                 category_  = nullptr; // +0x08 (error_category*)
    // boost::system::error_code carries a `source_location` (16B) since
    // boost 1.79+. We model it as two qwords to preserve total size = 24.
    // NOTE: the binary's actual layout is { value (4B), pad, category*,
    // source_location (8B?) } — exact internal split inside the +0x30..+0x47
    // region is irrelevant for source-level use; only `code()` returning by
    // value cares about total size.
    void const*                 source_    = nullptr; // +0x10

    int value() const noexcept { return value_; }
};

// Carries an error_code together with an explanatory message. Used as
// a constructor input for Exception. Verified layout at the call site
// 0x2e5810.
template <typename T>
struct GenericErrorDescription {
    T            code{};       // +0x00 (24 bytes when T = ErrorCode)
    std::string  message;      // +0x18 (24 bytes libc++)

    GenericErrorDescription() = default;
    GenericErrorDescription(T c, std::string m)
        : code(std::move(c)), message(std::move(m)) {}
};

// ----------------------------------------------------------------------------
// zhinst::Exception
//
// Base of the ZI exception hierarchy. See file-level comment for the
// binary layout reconstruction.
// ----------------------------------------------------------------------------
class Exception : public std::exception {
public:
    // 0x2e5410 — default ctor. Initializes errorCode_ from
    // make_error_code(0x8000) (= ZIResult_enum sentinel) and message_
    // to the literal "ZIException".
    Exception();

    // 0x2e54b0 — string ctor (by value, moved into message_).
    // If `msg` is empty (SSO byte 0 == 0), falls back to
    // ErrorCodeTraits<error_code>::defaultMessage(errorCode_).
    explicit Exception(std::string msg);

    // 0x2e55b0 — error_code ctor. Builds message_ from
    // `code.to_string() + "; "` (verified via to_string + insert at
    // 0x2e5601 / 0x2e5618 with prefix string at .rodata 0x90c6c6).
    explicit Exception(ErrorCode const& code);

    // 0x2e5700 — error_code + string ctor. Uses the supplied message
    // verbatim (default message fallback only when string is empty).
    Exception(ErrorCode const& code, std::string msg);

    // 0x2e5810 — GenericErrorDescription ctor. Moves both code and
    // message out of the description.
    explicit Exception(GenericErrorDescription<ErrorCode> desc);

    // 0x2e7720 — virtual dtor. Releases boost::exception::data_ and
    // the message_ string.
    ~Exception() override;

    // 0x2e5870 — returns SSO buffer or heap data ptr depending on
    // string layout. Matches std::string::c_str().
    const char* what() const noexcept override;

    // 0x2e5890 — returns errorCode_ by value (sret in binary).
    ErrorCode code() const noexcept;

    // 0x2e58b0 — returns &errorCode_ (pointer into the object).
    ErrorCode const* description() const noexcept;

protected:
    // Direct access for derived classes that may reset the message via
    // their own ctors (none currently observed, but kept as a hook).
    std::string&       message()       noexcept { return message_; }
    std::string const& message() const noexcept { return message_; }

private:
    // boost::exception subobject placeholder. The real binary stores
    // `void* data_; char const* throw_function_; char const* throw_file_;
    //  int throw_line_` here, occupying offsets +0x10..+0x2F of the
    // derived-most object. For source compilation we just hold raw
    // pointers; the exact field semantics are not invoked from src/.
    void const* boost_data_           = nullptr;  // ~ +0x10
    void const* boost_throw_function_ = nullptr;  // ~ +0x18
    void const* boost_throw_file_     = nullptr;  // ~ +0x20
    long        boost_throw_line_     = -1;       // ~ +0x28

    ErrorCode   errorCode_;       // ~ +0x30 (24 bytes in binary)
    std::string message_;         // ~ +0x48 (24 bytes libc++ SSO)
};

// ----------------------------------------------------------------------------
// Derived exception classes that the seqc compiler actually constructs.
// Only message-string ctors are exposed here — the binary defines a
// no-arg ctor too (with a hardcoded class-name message), but no src/
// reconstruction calls it.
//
// NOTE: ZIAWGCompilerException and ZIAWGOptimizerException are the only
// derived classes touched by seqc compiler reconstructed sources. The
// other ~25 derived types (ZIIOException, ZITimeoutException, etc.) are
// declared in the binary but never thrown from the seqc compilation
// path; we omit them here and add on demand.
// ----------------------------------------------------------------------------

// 0x2e7360 (string ctor), 0x2e72f0 (default ctor)
// vtable: 0xb0a618
class ZIAWGCompilerException : public Exception {
public:
    ZIAWGCompilerException();                              // 0x2e72f0
    explicit ZIAWGCompilerException(std::string msg);      // 0x2e7360
    ~ZIAWGCompilerException() override;
};

// 0x2e7460 (string ctor), 0x2e73d0 (default ctor)
// vtable: 0xb0a660
class ZIAWGOptimizerException : public Exception {
public:
    ZIAWGOptimizerException();                             // 0x2e73d0
    explicit ZIAWGOptimizerException(std::string msg);     // 0x2e7460
    ~ZIAWGOptimizerException() override;
};

} // namespace zhinst
