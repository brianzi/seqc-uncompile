// ============================================================================
// Reconstructed from disassembly of _seqc_compiler.so
//
// zhinst::Exception — base class for the entire ZI exception hierarchy.
//
// Reconstructed Layout is documented in
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
    //          Binary: in this libboost ABI error_code is 24 bytes:
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
//! \brief Compact stand-in for `boost::system::error_code` carried inside
//!        every `zhinst::Exception`.
//!
//! Pairs an integer error value with an opaque category pointer (and a
//! source-location placeholder), giving each `Exception` a programmatic
//! identity in addition to its human-readable message.  Only `value()`
//! and `to_string()` are exercised at the source level.
struct ErrorCode {
    int                         value_     = 0;     //!< Integer error value (the "code"). +0x00
    int                         _pad_      = 0;     //!< Alignment padding between `value_` and the category pointer.
    void const*                 category_  = nullptr; //!< Opaque pointer to the originating `boost::system::error_category` singleton. +0x08
    // boost::system::error_code carries a `source_location` (16B) since
    // boost 1.79+. We model it as two qwords to preserve total size = 24.
    // Binary: actual layout is { value (4B), pad, category*,
    // source_location (8B?) } — exact internal split inside the +0x30..+0x47
    // region is irrelevant for source-level use; only `code()` returning by
    // value cares about total size.
    void const*                 source_    = nullptr; //!< Boost source-location filler; never inspected at the source level. +0x10

    //! \brief Return the integer error value, suitable for
    //!        equality / category-specific lookup.
    //! \return The stored `value_` slot.
    int value() const noexcept { return value_; }

    // Approximation of boost::system::error_code::to_string().
    // Binary calls this at 0x2e55db to produce the code portion of the
    // Exception message.
    //! \brief Render the error code as the string `"error:<N>"` for
    //!        embedding in the human-readable message.
    //! \return Newly built diagnostic fragment used by the
    //!         `Exception(ErrorCode)` constructor.
    std::string to_string() const {
        return "error:" + std::to_string(value_);
    }
};

//! \brief Returns the sentinel `ErrorCode` used to tag exceptions that
//!        were not constructed with an explicit code.
//! \details The returned code carries the ZI API's "unspecified error"
//! value `0x8000`.  Every `Exception` default- or message-only
//! constructor seeds its `errorCode_` from this helper so that callers
//! inspecting `code()` always see a well-defined value.
// Sentinel value for the Exception default ctor (unknowns #91).
// Binary calls make_error_code(ZIResult_enum) @0x2e4550 with value 0x8000.
// The real implementation uses a custom ZiApiErrorCategory singleton at
// 0xb7c570; we approximate with a plain value-only ErrorCode.
inline ErrorCode makeDefaultErrorCode() {
    ErrorCode ec;
    ec.value_ = 0x8000;
    return ec;
}

// Carries an error_code together with an explanatory message. Used as
// a constructor input for Exception. Verified layout at the call site
// 0x2e5810.
//! \brief Error-code + message bundle accepted by `Exception`'s
//!        description constructor.
//!
//! Pairs an `ErrorCode` with the human-readable explanation that should
//! accompany it.  Construction call sites build a description in place
//! and let `Exception(GenericErrorDescription)` move both fields into
//! the new exception, avoiding a separate string copy.
template <typename T>
struct GenericErrorDescription {
    T            code{};       //!< Programmatic error code (typically `ErrorCode`); 24 bytes when `T = ErrorCode`. +0x00
    std::string  message;      //!< Human-readable explanation moved into the new `Exception`. +0x18

    //! \brief Default-construct an empty description (zero code,
    //!        empty message).
    GenericErrorDescription() = default;
    //! \brief Bundle a code with a message in one step.
    //! \param codeVal Programmatic error code; moved into `code`.
    //! \param msg     Human-readable text; moved into `message`.
    GenericErrorDescription(T codeVal, std::string msg)
        : code(std::move(codeVal)), message(std::move(msg)) {}
};

// ----------------------------------------------------------------------------
// zhinst::Exception
//
// Base of the ZI exception hierarchy. See file-level comment for the
// binary layout reconstruction.
// ----------------------------------------------------------------------------
//! \brief Base of the Zurich Instruments exception hierarchy thrown by
//!        the SeqC compiler.
//!
//! Carries an `ErrorCode` and a human-readable message together, exposed
//! as `code()`, `description()`, and `what()`.  Constructible from a
//! message alone, an `ErrorCode` alone, both, or a
//! `GenericErrorDescription` aggregate; supplying only an `ErrorCode`
//! synthesises a default message from the code's category.  Each of
//! the 26 derived `ZIxxxException` classes (declared via
//! `ZHINST_DECLARE_EXCEPTION` below) adds no new state and exists solely
//! so that `catch` clauses can discriminate by error kind.
class Exception : public std::exception {
public:
    //! \brief Constructs an `Exception` carrying the default ZI error
    //!        code and the message `"ZIException"`.
    //! \details The error code is the ZI API sentinel value `0x8000`
    //! (see `makeDefaultErrorCode()`); the message is a fixed literal
    //! identifying the exception kind in catch-all logs.
    // 0x2e5410 — default ctor. Initializes errorCode_ from
    // make_error_code(0x8000) (= ZIResult_enum sentinel) and message_
    // to the literal "ZIException".
    Exception();

    //! \brief Constructs an `Exception` whose `what()` returns the
    //!        supplied message and whose code is the default sentinel.
    //! \details `msg` is moved into the internal message buffer.
    //! \param msg Human-readable message; if empty, the message is
    //!        replaced with the default text derived from the error
    //!        code's category.
    //! \note An empty `msg` triggers a fallback that fills the
    //! message from the error category rather than leaving it blank.
    // 0x2e54b0 — string ctor (by value, moved into message_).
    // If `msg` is empty (SSO byte 0 == 0), falls back to
    // ErrorCodeTraits<error_code>::defaultMessage(errorCode_).
    explicit Exception(std::string msg);

    //! \brief Constructs an `Exception` for the given error code,
    //!        synthesising a message from the code.
    //! \details The resulting message has the form
    //! `"ZIException with status code: <code-string>"`.
    //! \param code Error code to copy into the new exception.
    // 0x2e55b0 — error_code ctor. Builds message_ from
    // `code.to_string() + "; "` (verified via to_string + insert at
    // 0x2e5601 / 0x2e5618 with prefix string at .rodata 0x90c6c6).
    explicit Exception(ErrorCode const& code);

    //! \brief Constructs an `Exception` carrying both an error code
    //!        and a caller-supplied message.
    //! \param code Error code copied into the new exception.
    //! \param msg  Message text used verbatim; if empty, replaced by
    //!             the error category's default message.
    // 0x2e5700 — error_code + string ctor. Uses the supplied message
    // verbatim (default message fallback only when string is empty).
    Exception(ErrorCode const& code, std::string msg);

    //! \brief Constructs an `Exception` from an error-code-plus-message
    //!        description, moving both fields into the new object.
    //! \param desc Bundle whose `code` and `message` are moved out;
    //!        the description is left in a valid moved-from state.
    // 0x2e5810 — GenericErrorDescription ctor. Moves both code and
    // message out of the description.
    explicit Exception(GenericErrorDescription<ErrorCode> desc);

    //! \brief Destroys the exception and releases its message buffer.
    // 0x2e7720 — virtual dtor. Releases boost::exception::data_ and
    // the message_ string.
    ~Exception() override;

    //! \brief Returns the human-readable message as a null-terminated
    //!        C string.
    //! \return Pointer to the message buffer; valid for the lifetime
    //!         of the exception.
    // 0x2e5870 — returns SSO buffer or heap data ptr depending on
    // string layout. Matches std::string::c_str().
    const char* what() const noexcept override;

    //! \brief Returns the error code carried by this exception, by value.
    //! \return A copy of the internal `ErrorCode`.
    // 0x2e5890 — returns errorCode_ by value (sret in binary).
    ErrorCode code() const noexcept;

    //! \brief Returns a pointer to the internal error-code field.
    //! \return Pointer to the embedded `ErrorCode`; valid for the
    //!         lifetime of the exception.
    //! \binarynote Despite its name, this accessor exposes only the
    //! error code, not the message; the pointer aliases an internal
    //! field rather than returning an independent description object.
    // 0x2e58b0 — returns &errorCode_ (pointer into the object).
    ErrorCode const* description() const noexcept;

protected:
    //! \brief Mutable access to the message buffer for derived ctors
    //!        that need to rewrite the human-readable text after
    //!        forwarding to a base constructor.
    //! \return Reference to the internal message string.
    // Direct access for derived classes that may reset the message via
    // their own ctors (none currently observed, but kept as a hook).
    std::string&       message()       noexcept { return message_; }

    //! \brief Read-only access to the message buffer.
    //! \return `const` reference to the internal message string.
    std::string const& message() const noexcept { return message_; }

private:
    // boost::exception subobject placeholder. The real binary stores
    // `void* data_; char const* throw_function_; char const* throw_file_;
    //  int throw_line_` here, occupying offsets +0x10..+0x2F of the
    // derived-most object. For source compilation we just hold raw
    // pointers; the exact field semantics are not invoked from src/.

    //! \brief Opaque pointer to the boost::exception error-info container.
    //! \details Source-level placeholder; never read or written from
    //! reconstructed code.
    void const* boost_data_           = nullptr;  // ~ +0x10

    //! \brief Source-location filler — name of the throwing function.
    //! \details Source-level placeholder; never read or written from
    //! reconstructed code.
    void const* boost_throw_function_ = nullptr;  // ~ +0x18

    //! \brief Source-location filler — file path of the throw site.
    //! \details Source-level placeholder; never read or written from
    //! reconstructed code.
    void const* boost_throw_file_     = nullptr;  // ~ +0x20

    //! \brief Source-location filler — line number of the throw site,
    //!        or `-1` when not set.
    //! \details Source-level placeholder; never read or written from
    //! reconstructed code.
    long        boost_throw_line_     = -1;       // ~ +0x28

    //! \brief Programmatic identity of the exception, returned by
    //!        `code()` and pointed at by `description()`.
    ErrorCode   errorCode_;       // ~ +0x30 (24 bytes in binary)

    //! \brief Human-readable message returned by `what()`.
    std::string message_;         // ~ +0x48 (24 bytes libc++ SSO)
};

// ----------------------------------------------------------------------------
// Derived exception classes.
//
// All 26 subclasses add NO data fields — verified from the binary: each
// ctor only sets vptrs and forwards a string to Exception(string).
// They differ only in vtable identity (RTTI / exception matching).
//
// Macro-declared to avoid 26× repetition of the identical pattern.
// Each class has:
//   - default ctor (passes class name as string to base)
//   - explicit string ctor (forwards to Exception(string))
//   - defaulted dtor
// ----------------------------------------------------------------------------

//! \brief Declares a concrete `Exception` subclass whose sole purpose
//!        is type-identity-based dispatch in `catch` clauses.
//!
//! Each expansion produces a class derived from `Exception` with three
//! members:
//!   - a default constructor that forwards the stringified class name
//!     as the message to `Exception(std::string)`;
//!   - an explicit `std::string` constructor that forwards its argument
//!     to `Exception(std::string)`;
//!   - an `override` destructor.
//!
//! The subclass adds no fields and no behaviour beyond `Exception`; it
//! differs from its siblings only in vtable identity, allowing callers
//! to write `catch (ZIDeviceException&)` and similar.  The matching
//! `ZHINST_DEFINE_EXCEPTION` in `exception.cpp` emits the out-of-line
//! bodies.
//!
//! \param ClassName Identifier of the generated class.
#define ZHINST_DECLARE_EXCEPTION(ClassName)                            \
    class ClassName : public Exception {                               \
    public:                                                            \
        ClassName();                                                   \
        explicit ClassName(std::string msg);                           \
        ~ClassName() override;                                         \
    }

//! \name Concrete exception types
//! \brief The 26 `Exception` subclasses thrown by the compiler and the
//!        runtime, one per error category.
//!
//! Every entry is a `ZHINST_DECLARE_EXCEPTION` expansion: a class
//! derived from `Exception` that adds no state and exists solely so
//! that `catch` clauses can discriminate by type.  Per-class
//! behaviour is identical and is described on
//! `ZHINST_DECLARE_EXCEPTION`; the brief on each line names the error
//! category the type represents.
//! @{

// Binary addresses: default ctor / string ctor
//! Thrown for failures originating in the ZI API layer.
ZHINST_DECLARE_EXCEPTION(ZIAPIException);                   // 0x2e58c0 / 0x2e5930
//! Thrown for general I/O failures (file, stream, transport).
ZHINST_DECLARE_EXCEPTION(ZIIOException);                    // 0x2e5a30 / 0x2e5aa0
//! Thrown for device-level errors not covered by a more specific type.
ZHINST_DECLARE_EXCEPTION(ZIDeviceException);                // 0x2e5c90 / 0x2e5cf0
//! Thrown for socket / network communication failures.
ZHINST_DECLARE_EXCEPTION(ZISocketException);                // 0x2e5d60 / 0x2e5dc0
//! Thrown when a buffer or counter overflows its capacity.
ZHINST_DECLARE_EXCEPTION(ZIOverflowException);              // 0x2e5e30 / 0x2e5ea0
//! Thrown when a buffer underruns or starves a consumer.
ZHINST_DECLARE_EXCEPTION(ZIUnderrunException);              // 0x2e5f10 / 0x2e5f80
//! Thrown when an operation does not complete within its time budget.
ZHINST_DECLARE_EXCEPTION(ZITimeoutException);               // 0x2e5ff0 / 0x2e6060
//! Thrown when a write is attempted on a read-only resource.
ZHINST_DECLARE_EXCEPTION(ZIReadOnlyException);              // (none)  / 0x2e60d0
//! Thrown when a read is attempted on a write-only resource.
ZHINST_DECLARE_EXCEPTION(ZIWriteOnlyException);             // (none)  / 0x2e6190
//! Thrown when a requested name, path, or entity does not exist.
ZHINST_DECLARE_EXCEPTION(ZINotFoundException);              // 0x2e6250 / 0x2e62c0
//! Thrown when source input contains an unknown or misused keyword.
ZHINST_DECLARE_EXCEPTION(ZIInvalidKeywordException);        // (none)  / 0x2e6330
//! Thrown when a value's type does not match the operation's expectation.
ZHINST_DECLARE_EXCEPTION(ZITypeMismatchException);          // 0x2e63f0 / 0x2e6480
//! Thrown when a numeric or indexed value falls outside the legal range.
ZHINST_DECLARE_EXCEPTION(ZIOutOfRangeException);            // 0x2e64f0 / 0x2e6560
//! Thrown to signal that a long-running operation was interrupted.
ZHINST_DECLARE_EXCEPTION(ZIInterruptException);             // 0x2e65d0 / 0x2e6640
//! Thrown for invariant violations and unexpected internal states.
ZHINST_DECLARE_EXCEPTION(ZIInternalException);              // 0x2e66b0 / 0x2e6760
//! Thrown when a device exists but is not currently discoverable.
ZHINST_DECLARE_EXCEPTION(ZIDeviceNotVisibleException);      // 0x2e6820 / 0x2e68f0
//! Thrown when a requested device serial is not known to the server.
ZHINST_DECLARE_EXCEPTION(ZIDeviceNotFoundException);        // 0x2e69b0 / 0x2e6a80
//! Thrown when a device is already claimed by another session.
ZHINST_DECLARE_EXCEPTION(ZIDeviceInUseException);           // 0x2e6b40 / 0x2e6c00
//! Thrown for failures of the host/device transport interface.
ZHINST_DECLARE_EXCEPTION(ZIDeviceInterfaceException);       // 0x2e6cc0 / 0x2e6d90
//! Thrown when a device connection cannot be established in time.
ZHINST_DECLARE_EXCEPTION(ZIDeviceConnectionTimeoutException); // 0x2e6e50 / 0x2e6f20
//! Thrown when a device is reachable only on a different interface than requested.
ZHINST_DECLARE_EXCEPTION(ZIDeviceDifferentInterfaceException); // 0x2e6fe0 / 0x2e70c0
//! Thrown for device firmware errors (mismatch, upload failure, runtime fault).
ZHINST_DECLARE_EXCEPTION(ZIDeviceFWException);              // 0x2e7180 / 0x2e7230
//! Thrown by the AWG compiler front-end on user-source errors.
ZHINST_DECLARE_EXCEPTION(ZIAWGCompilerException);           // 0x2e72f0 / 0x2e7360
//! Thrown by the AWG optimizer when a pass cannot complete.
ZHINST_DECLARE_EXCEPTION(ZIAWGOptimizerException);          // 0x2e73d0 / 0x2e7460
//! Thrown when a host/firmware/API version is incompatible.
ZHINST_DECLARE_EXCEPTION(ZIVersionException);               // 0x2e74d0 / 0x2e7540
//! Thrown when a node path is syntactically invalid or otherwise rejected.
ZHINST_DECLARE_EXCEPTION(ZIIllegalPathException);           // 0x2e75b0 / 0x2e7620

//! @}

#undef ZHINST_DECLARE_EXCEPTION


} // namespace zhinst
