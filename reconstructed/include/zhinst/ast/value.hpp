// ============================================================================
// Reconstructed from disassembly of _seqc_compiler.so
// Value and Immediate types — instruction operands
//
// Source path (from error strings):
//   /builds/labone/labone/ziAWG/ziAWGUtils/src/main/include/Value.hpp
//
// Immediate: std::variant<AddressImpl<uint>, int, std::string> (0x1C bytes)
//   Variant dispatch uses function-pointer tables in .data.rel.ro.
//
// Value: outer ValueType tag + boost::variant<int,bool,double,string> (0x28 bytes)
//   The outer tag at +0x00 mirrors the variant but adds "Unspecified" (0).
//
// AddressImpl<T>: trivial wrapper around an unsigned value.
//   Passed by value in registers. Formatted as "%u" via boost::format.
// ============================================================================
#pragma once

#include "zhinst/asm/address_impl.hpp"

#include <cstdint>
#include <cmath>
#include <ostream>
#include <string>

namespace zhinst {

// ============================================================================
// Immediate — std::variant<AddressImpl<uint>, int, std::string>
//
// Layout (40 bytes on libstdc++, 28 bytes on libc++):
//   +0x00  [32/24 bytes]  union storage (AddressImpl<uint> / int / std::string)
//   +0x20/+0x18  [4 bytes]   variant index:
//                         0 = AddressImpl<uint>
//                         1 = int
//                         2 = std::string
//                         0xFFFFFFFF = valueless_by_exception
//   (4 bytes padding to alignment)
//
// Note: libstdc++ std::string is 32 bytes (SSO buffer), libc++ is 24.
// The original binary was built with libc++ (0x1C = 28 bytes total).
// ============================================================================
//! \brief Tagged-union operand for assembly instructions.
//!
//! `Immediate` is a small variant that an assembly instruction operand can
//! take one of three forms: an unsigned address, a signed 32-bit integer, or
//! a string label.  The active alternative is recorded by `index_` (see
//! `ImmediateKind`); a default-constructed instance is `Valueless` and will
//! throw on conversion.
//!
//! Conversion operators (`int`, `unsigned int`) and `toString()` dispatch
//! through a function-pointer table per active alternative; passing the
//! wrong-typed conversion through them is a programming error.
class Immediate {
public:
    //! \brief Union of the three possible immediate payloads.
    //!
    //! Active alternative is selected by `Immediate::index_`; only
    //! the matching member is constructed.  `Storage` itself has
    //! trivial ctor / dtor so the enclosing `Immediate` is
    //! responsible for in-place construction / destruction of the
    //! active member.
    union Storage {
        detail::AddressImpl<uint32_t> address;  //!< \brief Active when `index_ == 0`.
        int32_t                       integer;  //!< \brief Active when `index_ == 1`.
        std::string                   str;      //!< \brief Active when `index_ == 2`.

        //! \brief Trivial — the active member is constructed in
        //!        place by `Immediate`.
        Storage() {}
        //! \brief Trivial — the active member is destroyed in
        //!        place by `Immediate`.
        ~Storage() {}
    };

    //! \brief Tagged-union storage holding one of the three
    //!        alternatives (see `ImmediateKind`).
    Storage data_;     // +0x00
    //! \brief Active-alternative discriminator; values match
    //!        `ImmediateKind`.
    uint32_t index_;   // after union — see ImmediateKind enum below

    // --- Constructors (all confirmed from disassembly) ---
    //! \brief Default-constructs a `Valueless` immediate; any
    //! conversion will throw.
    Immediate() : index_(0xFFFFFFFF) {}       // default: valueless state
    //! \brief Constructs an `Address`-kind immediate from an
    //! existing `AddressImpl` wrapper.
    //! \param addr Pre-wrapped address payload.
    Immediate(detail::AddressImpl<uint32_t> addr);  // 0x290ab0
    //! \brief Constructs an `Address`-kind immediate from a raw
    //! unsigned value (same kind as the `AddressImpl` overload).
    //! \param v Address payload as an unsigned 32-bit value.
    Immediate(uint32_t v);                           // 0x290ac0 — sets index=0 (same as AddressImpl)
    //! \brief Constructs an `Integer`-kind immediate from a signed
    //! 32-bit value.
    //! \param v Signed integer payload.
    Immediate(int32_t v);                            // 0x290ad0 — sets index=1
    //! \brief Constructs a `String`-kind immediate carrying the
    //! given label/symbol name.
    //! \param s Label or symbol text used as the immediate's
    //! payload.
    Immediate(std::string const& s);                 // 0x290ae0 — sets index=2
    //! \brief Copy-construct from `other`, deep-copying the active
    //!        alternative (in particular, copying the string when
    //!        the active kind is `String`).
    //! \param other  Source immediate.
    Immediate(Immediate const& other);               // copy ctor (needed due to union with std::string)
    //! \brief Move-construct from `other`, transferring ownership
    //!        of the active alternative and leaving `other` in the
    //!        `Valueless` state.
    //! \param other  Source immediate; mutated to `Valueless`.
    Immediate(Immediate&& other) noexcept;           // move ctor
    //! \brief Copy-assign: destroys the current active alternative
    //!        and copy-constructs the one held by `other`.
    //! \param other  Source immediate.
    //! \return `*this`.
    Immediate& operator=(Immediate const& other);    // copy assignment
    //! \brief Move-assign: destroys the current active alternative
    //!        and move-constructs the one held by `other`, leaving
    //!        `other` in the `Valueless` state.
    //! \param other  Source immediate; mutated to `Valueless`.
    //! \return `*this`.
    Immediate& operator=(Immediate&& other) noexcept; // move assignment
    //! \brief Destroy the active alternative in place.
    ~Immediate();                                    // 0x15c4f0

    // --- Query ---
    //! \brief Returns true iff the active alternative is
    //! `ImmediateKind::Address`.
    //! \return `true` when `index_ == 0` (Address kind), otherwise
    //! `false`.
    bool holdsUnsigned() const;     // 0x290b30 — returns index_ == 0

    // --- Conversions ---
    //! \brief Returns the integer value of an `Integer`-kind
    //! immediate; calling on any other kind dispatches through the
    //! variant's converter table and is a programming error.
    operator int() const;           // 0x290cc0 — vtable dispatch at b070b0
    //! \brief Returns the unsigned address value of an
    //! `Address`-kind immediate; calling on any other kind
    //! dispatches through the variant's converter table and is a
    //! programming error.
    operator unsigned int() const;  // 0x290d00 — vtable dispatch at b070c8

    // --- Comparison ---
    //! \brief Element-wise equality: equal iff both immediates hold
    //! the same alternative with the same value.
    //! \param other Right-hand operand to compare against.
    //! \return `true` when both alternatives and payloads match.
    bool operator==(Immediate const& other) const;  // 0x290d40 — vtable dispatch at b070e0
};

// ImmediateKind — variant discriminator for Immediate::index_ (A1)
//! \brief Discriminator value carried by `Immediate::index_`,
//! identifying the active variant alternative.
enum class ImmediateKind : uint32_t {
    Address   = 0,            //!< Active alternative is `AddressImpl<uint32_t>`.
    Integer   = 1,            //!< Active alternative is `int32_t`.
    String    = 2,            //!< Active alternative is `std::string` (label).
    Valueless = 0xFFFFFFFF,   //!< Default-constructed / moved-from state.
};

// toString(Immediate) — 0x290b40 (free function, sret)
// Dispatches through vtable at b07068. Returns string representation.
//! \brief Renders an `Immediate` as the string the assembler
//! emits for its active alternative (`%u` for addresses, decimal
//! for integers, the literal string for label kinds).
//! \param imm Immediate to render.
//! \return The assembler-syntax string for `imm`.
std::string toString(Immediate const& imm);

// operator<<(ostream&, Immediate const&) — 0x290b90
// Copies Immediate, calls toString, writes to ostream.
//! \brief Writes `toString(imm)` to `os`.
//! \param os Destination stream.
//! \param imm Immediate to render.
//! \return Reference to `os`.
std::ostream& operator<<(std::ostream& os, Immediate const& imm);

// ============================================================================
// ValueException — thrown by Value conversion methods
//
// Layout (0x20 bytes):
//   +0x00  vptr (vtable at b03cc8+0x10)
//   +0x08  std::string msg_ (24 bytes, libc++ SSO)
// ============================================================================
//! \brief Exception thrown by `Value` conversion methods.
//!
//! Raised when `Value::toInt()`, `toDouble()`, `toBool()`, or `toString()`
//! is called on an instance whose `type_` is `Unspecified`, or whose stored
//! alternative cannot be coerced to the requested type.
class ValueException : public std::exception {
    //! \brief Diagnostic text returned by `what()`.
    std::string msg_;
public:
    //! \brief Constructs a `ValueException` carrying `msg` as the
    //! `what()` string.
    //! \param msg Diagnostic text returned later by `what()`.
    explicit ValueException(std::string const& msg);  // 0x16e7f0
    //! \brief Release the embedded `msg_` storage and chain to
    //!        `~std::exception`.
    ~ValueException() override;                        // 0x16e850
    //! \brief Returns the `msg` passed at construction.
    //! \return Pointer to the internal C string of `msg_`; valid
    //! for the lifetime of `*this`.
    const char* what() const noexcept override;        // 0x16f110
};

// ============================================================================
// ValueType — outer type tag for Value
// ============================================================================
//! \brief Outer tag carried by `Value::type_`, identifying the
//! semantic type of the held literal.  `Unspecified` is the
//! default; every `toX()` conversion on an `Unspecified` value
//! throws `ValueException`.
enum class ValueType : int32_t {
    Unspecified = 0,   //!< Default state; conversions throw.
    Int        = 1,    //!< Holds `int32_t` in `storage_.i`.
    Bool       = 2,    //!< Holds `bool` in `storage_.b`.
    Double     = 3,    //!< Holds `double` in `storage_.d`.
    String     = 4,    //!< Holds `std::string` in `storage_.str`.
};

// ============================================================================
// Value — ValueType tag + tagged union
//
// Layout (libc++ binary):
//   +0x00  [4 bytes]   ValueType type_   — outer tag (0..4)
//   +0x04  [4 bytes]   (padding)
//   +0x08  [4 bytes]   int32_t which_    — variant discriminator
//   +0x0C  [4 bytes]   (padding)
//   +0x10  [24 bytes]  union storage {int, bool, double, string}
//   +0x28              END  (= 0x28 / 40 bytes on libc++)
//
// On libstdc++: std::string is 32 bytes (vs libc++ 24 bytes), so the
// union storage grows to 32 bytes and sizeof(Value) = 48. Verified
// against binary: Value::Value(string) @0x22c2b0 copies 24 bytes into
// this+0x10 (libc++ SSO); Value::~Value @0x15a9c0 checks SSO flag at
// this+0x10 bit 0, frees heap ptr at this+0x20. Both are libc++ ABI
// patterns. Our libstdc++ build must use a real std::string member to
// get correct construction/destruction semantics for 32-byte strings.
//
// Layout: type_ at +0x00 (with 4B pad), which_ at +0x08 (with 4B pad),
// storage union at +0x10. Disasm evidence:
//   Value::~Value @0x15a9c0 reads which_ from [rdi+0x08].
//   Storage SSO check at [rdi+0x10], heap-free ptr at [rdi+0x20].
// The padding before which_ comes from the storage union requiring 8B
// alignment (contains double/pointer).
//
// Storage uses a real std::string member rather than a fixed buffer:
// sizeof(Value) is ABI-dependent (40 on libc++, 48 on libstdc++). All
// field accesses use named members, so the size difference is transparent.
// ============================================================================
//! \brief Tagged scalar literal carried through the SeqC frontend.
//!
//! `Value` is the runtime representation of a SeqC literal or evaluated
//! constant.  It pairs an outer `ValueType` tag with a discriminated union
//! holding one of `int32_t`, `bool`, `double`, or `std::string`.  A
//! default-constructed `Value` has `type_ == Unspecified` and will throw
//! `ValueException` from any of the `toX()` conversion methods.
//!
//! `Value` is the payload of `EvalResultValue::value_` and the return type
//! of `EvalResults::getValue()`; it is the primary medium by which
//! constant-folded expression values flow through the lowering pipeline.
class Value {
public:
    //! \brief Outer semantic-type tag; see `ValueType`.
    ValueType type_;       // +0x00
    //! \brief Alignment padding (zero-initialised).
    int32_t   pad_04_{};   // +0x04 — alignment padding
    //! \brief Variant discriminator selecting the active member of
    //!        `storage_`.
    int32_t   which_;      // +0x08 — variant discriminator
    //! \brief Alignment padding (zero-initialised).
    int32_t   pad_0C_{};   // +0x0C — alignment padding
    //! \brief Union of the four possible payload types.
    //!
    //! Active alternative is selected jointly by `type_` and
    //! `which_`.  `Storage` itself has trivial ctor / dtor so
    //! `Value` is responsible for in-place construction /
    //! destruction of the active member.
    union Storage {
        int32_t     i;              //!< \brief Active when `type_ == Int`.
        bool        b;              //!< \brief Active when `type_ == Bool`.
        double      d;              //!< \brief Active when `type_ == Double`.
        std::string str;            //!< \brief Active when `type_ == String`.
                                    // (24 bytes on libc++, 32 on libstdc++)

        //! \brief Trivial — `i` is zero-initialised; `Value`
        //!        reconstructs the active member in place.
        Storage() : i(0) {}
        //! \brief Trivial — `Value` destroys the active member in
        //!        place.
        ~Storage() {}
    };
    //! \brief Tagged-union storage holding one of the four
    //!        alternatives (see `ValueType`).
    Storage storage_;      // +0x10

    // --- Constructors ---
    // Int, Bool, Double constructors are fully inlined (no symbols).
    // String constructor is the only non-inline one:
    //! \brief Constructs a `String`-kind value from the given text.
    //! \param s Text payload copied into `storage_.str`.
    Value(std::string const& s);  // 0x22c2b0 — sets type_=4, which_=3, copies string
    //! \brief Default-constructs an `Unspecified` value; any
    //! conversion will throw `ValueException`.
    Value();  // default ctor — needed by Resources::Variable initialization
    //! \brief Constructs a `Double`-kind value.
    //! \param d Double payload stored in `storage_.d`.
    explicit Value(double d) : type_(ValueType(3)), pad_04_{}, which_(2), pad_0C_{} { storage_.d = d; }
    //! \brief Constructs an `Int`-kind value.
    //! \param i Signed integer payload stored in `storage_.i`.
    explicit Value(int32_t i) : type_(ValueType(1)), pad_04_{}, which_(0), pad_0C_{} { storage_.i = i; }
    //! \brief Constructs a `Bool`-kind value.
    //! \param b Boolean payload stored in `storage_.b`.
    explicit Value(bool b) : type_(ValueType(2)), pad_04_{}, which_(1), pad_0C_{} { storage_.b = b; }

    // Copy/move — implicit in binary (char[24] union was trivially copyable).
    // Explicit here because libstdc++ union has real std::string member.
    //! \brief Copy-construct from `other`, deep-copying the active
    //!        alternative.
    //! \param other  Source value.
    Value(Value const& other);
    //! \brief Move-construct from `other`, transferring ownership
    //!        of the active alternative and leaving `other` in the
    //!        `Unspecified` state.
    //! \param other  Source value; mutated to `Unspecified`.
    Value(Value&& other) noexcept;
    //! \brief Copy-assign: destroys the current active alternative
    //!        and copy-constructs the one held by `other`.
    //! \param other  Source value.
    //! \return `*this`.
    Value& operator=(Value const& other);
    //! \brief Move-assign: destroys the current active alternative
    //!        and move-constructs the one held by `other`, leaving
    //!        `other` in the `Unspecified` state.
    //! \param other  Source value; mutated to `Unspecified`.
    //! \return `*this`.
    Value& operator=(Value&& other) noexcept;

    // --- Conversion methods ---
    //! \brief Coerces the held value to `double`: numeric kinds
    //! convert directly, `Bool` converts to 0.0/1.0, and `String`
    //! is parsed via `std::stod`.
    //! \return Held payload coerced to `double`.
    //! \throws ValueException if `type_ == Unspecified` or out of
    //! the recognised set.
    double toDouble() const;       // 0x15a560
    //! \brief Coerces the held value to `int32_t`: `Int` returns
    //! directly, `Bool` returns 0/1, `String` is parsed via
    //! `std::stol`, and `Double` is truncated toward zero.
    //! \return Held payload coerced to `int32_t`.
    //! \note The `Double` path uses `cvttsd2si`-style truncation
    //! semantics: values outside `[INT32_MIN, INT32_MAX]` are
    //! reinterpreted via wrapping `uint32_t` truncation, which
    //! preserves the encoding of large hex literals stored as
    //! positive doubles (e.g. `0xAAAAAAAA`).
    //! \throws ValueException if `type_ == Unspecified` or out of
    //! the recognised set.
    int32_t toInt() const;         // 0x15c250
    //! \brief Coerces the held value to `bool`: `Int` is non-zero,
    //! `Bool` is direct, `String` matches the literal `"true"`
    //! exactly, and `Double` is non-zero within `~1e-15`.
    //! \return Held payload coerced to `bool`.
    //! \throws ValueException if `type_ == Unspecified` or out of
    //! the recognised set.
    bool toBool() const;           // 0x164200
    //! \brief Renders the held value as a string.
    //! \return String rendering of the held payload (decimal for
    //! numeric kinds, `"true"`/`"false"` for booleans, identity
    //! for `String`).
    //! \throws ValueException if `type_ == Unspecified` or out of
    //! the recognised set.
    std::string toString() const;  // 0x15de50

    // --- Comparison ---
    //! \brief Returns true iff both values hold the same type and
    //! the same payload value.
    //! \param other Right-hand operand to compare against.
    //! \return `true` when both `type_` and the active payload
    //! match.
    bool operator==(Value const& other) const;  // 0x21a780

    // --- Destructor ---
    //! \brief Destroy the active payload member of `storage_` in
    //!        place; the `Storage` union has a trivial destructor.
    ~Value();  // 0x15a9c0
};

// sizeof(Value): 40 on libc++ (binary), 48 on libstdc++ (reconstruction).
// Both are correct for their respective ABI — the difference is solely
// from std::string size (24 vs 32 bytes).
static_assert(sizeof(Value) >= 40, "Value must be at least 40 bytes");

} // namespace zhinst
