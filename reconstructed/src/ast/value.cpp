// ============================================================================
// Reconstructed from disassembly of _seqc_compiler.so
// Value, Immediate, AddressImpl — method implementations
//
// Binary addresses noted in comments next to each function.
// ============================================================================

#include "zhinst/ast/value.hpp"

#include <cstring>
#include <variant>
#include <boost/format.hpp>
#include <boost/throw_exception.hpp>

namespace zhinst {

// Discriminator value for the moved-from / default-constructed
// "valueless" state of Immediate::index_.  Mirrors the
// ImmediateKind::Valueless enumerator declared in value.hpp; kept
// as a uint32_t alias because index_ itself is uint32_t (the
// header documents the enum mapping but stores the raw int form).
static constexpr uint32_t kImmediateValueless =
    static_cast<uint32_t>(ImmediateKind::Valueless);

// ============================================================================
// operator<<(ostream&, AddressImpl<uint>) — 0x1c7ce0
//
// Formats the uint value as "%u" using boost::format.
// AddressImpl<uint> is passed by value (register-sized).
// ============================================================================
//! \brief Streams a 32-bit address-wrapper as an unsigned decimal
//! number (formatted as `%u` via `boost::format`).
//! \param os   Destination output stream.
//! \param addr Address wrapper whose `value` is written to `os`.
//! \return Reference to `os` to allow chaining.
std::ostream& operator<<(std::ostream& os,  // 0x1c7ce0
                          detail::AddressImpl<uint32_t> addr)
{
    os << boost::format("%u") % addr.value;
    return os;
}

// ============================================================================
// Immediate constructors
// ============================================================================

Immediate::Immediate(detail::AddressImpl<uint32_t> addr)  // 0x290ab0
    : index_(0)
{
    data_.address = addr;
}

Immediate::Immediate(uint32_t v)  // 0x290ac0
    : index_(0)  // NOTE: same index as AddressImpl — wraps uint into AddressImpl
{
    data_.address.value = v;
}

Immediate::Immediate(int32_t v)  // 0x290ad0
    : index_(1)
{
    data_.integer = v;
}

Immediate::Immediate(std::string const& s)  // 0x290ae0
    : index_(2)
{
    new (&data_.str) std::string(s);
}

// ============================================================================
// Immediate copy/move/copy-assign additions.
//
// These three operations are referenced by callers but **never defined in
// the binary** (no symbol present). They were either synthesized implicitly
// (via libc++ variant-style helpers) or inlined at the very few call sites.
// We provide explicit out-of-line definitions following the same per-index
// dispatch pattern as ~Immediate, holdsUnsigned, operator int, etc.
// ============================================================================

Immediate::Immediate(Immediate const& other)
    : index_(other.index_)
{
    switch (other.index_) {
        case 0:  data_.address = other.data_.address;       break;
        case 1:  data_.integer = other.data_.integer;       break;
        case 2:  new (&data_.str) std::string(other.data_.str); break;
        default: /* valueless */                              break;
    }
}

Immediate::Immediate(Immediate&& other) noexcept
    : index_(other.index_)
{
    switch (other.index_) {
        case 0:  data_.address = other.data_.address;                            break;
        case 1:  data_.integer = other.data_.integer;                            break;
        case 2:  new (&data_.str) std::string(std::move(other.data_.str));       break;
        default: /* valueless */                                                  break;
    }
    // Per the binary's ~Immediate, leave `other` in valueless state so its
    // dtor is a no-op. (Strictly only needed for the string case, but doing
    // it unconditionally costs nothing and matches the variant-move idiom.)
    if (other.index_ == 2) {
        other.data_.str.~basic_string();
    }
    other.index_ = kImmediateValueless;
}

Immediate& Immediate::operator=(Immediate const& other) {
    if (this == &other) return *this;
    // Destroy current state, then copy-construct the new one in place.
    if (index_ == 2) {
        data_.str.~basic_string();
    }
    index_ = other.index_;
    switch (other.index_) {
        case 0:  data_.address = other.data_.address;                  break;
        case 1:  data_.integer = other.data_.integer;                  break;
        case 2:  new (&data_.str) std::string(other.data_.str);        break;
        default: /* valueless */                                        break;
    }
    return *this;
}

// ============================================================================
// Immediate::~Immediate() — 0x15c4f0
//
// Dispatches through vtable if index_ != kImmediateValueless.
// Only index=2 (string) actually needs destruction.
// Sets index_ = kImmediateValueless afterwards (valueless state).
// ============================================================================
Immediate::~Immediate() {  // 0x15c4f0
    if (index_ == 2) {
        data_.str.~basic_string();
    }
    index_ = kImmediateValueless;
}

// ============================================================================
// Immediate::holdsUnsigned() — 0x290b30
// ============================================================================
bool Immediate::holdsUnsigned() const {  // 0x290b30
    return index_ == 0;
}

// ============================================================================
// Immediate::operator int() — 0x290cc0
//
// Dispatches through visitor vtable at b070b0[index_].
// Throws bad_variant_access if valueless.
// ============================================================================
Immediate::operator int() const {  // 0x290cc0
    switch (index_) {
        case 0:  return static_cast<int>(data_.address.value);
        case 1:  return data_.integer;
        case 2:  // string → int conversion (likely stoi, but uncommon path)
            return std::stoi(data_.str);
        default:
            throw std::bad_variant_access();  // index_ == kImmediateValueless
    }
}

// ============================================================================
// Immediate::operator unsigned int() — 0x290d00
//
// Dispatches through visitor vtable at b070c8[index_].
// ============================================================================
Immediate::operator unsigned int() const {  // 0x290d00
    switch (index_) {
        case 0:  return data_.address.value;
        case 1:  return static_cast<uint32_t>(data_.integer);
        case 2:  return static_cast<uint32_t>(std::stoul(data_.str));
        default:
            throw std::bad_variant_access();
    }
}

// ============================================================================
// Immediate::operator==(Immediate) — 0x290d40
//
// If either is valueless or indices differ → false.
// Otherwise dispatches per-type comparison via vtable at b070e0[index_].
// ============================================================================
bool Immediate::operator==(Immediate const& other) const {  // 0x290d40
    if (index_ == kImmediateValueless) return false;
    if (index_ != other.index_) return false;
    switch (index_) {
        case 0: return data_.address.value == other.data_.address.value;
        case 1: return data_.integer == other.data_.integer;
        case 2: return data_.str == other.data_.str;
    }
    return false;
}

// ============================================================================
// toString(Immediate const&) — 0x290b40 (free function)
//
// Dispatches through vtable at b07068[index_]. Returns string via sret.
// Throws bad_variant_access if valueless.
// ============================================================================
std::string toString(Immediate const& imm) {  // 0x290b40
    switch (imm.index_) {
        case 0: return std::to_string(imm.data_.address.value);
        case 1: return std::to_string(imm.data_.integer);
        case 2: return imm.data_.str;
        default:
            throw std::bad_variant_access();
    }
}

// ============================================================================
// operator<<(ostream&, Immediate const&) — 0x290b90
//
// Copies the Immediate locally, calls toString on it, writes result to ostream.
// ============================================================================
std::ostream& operator<<(std::ostream& os,  // 0x290b90
                          Immediate const& imm)
{
    os << toString(imm);
    return os;
}

// ============================================================================
// ValueException
// ============================================================================

ValueException::ValueException(std::string const& msg)  // 0x16e7f0
    : msg_(msg)
{
    // vptr set automatically by compiler
}

ValueException::~ValueException() {  // 0x16e850
    // msg_ destroyed automatically; base ~exception() called
}

const char* ValueException::what() const noexcept {  // 0x16f110
    return msg_.c_str();
}

// ============================================================================
// Value::Value(string const&) — 0x22c2b0
//
// Sets type_ = 4 (String), which_ = 3, copies string into storage at +0x10.
// Other constructors (int, bool, double) are fully inlined — no symbols.
// ============================================================================
Value::Value(std::string const& s) {  // 0x22c2b0
    type_ = ValueType::String;          // +0x00: movl $0x4, (%rdi)
    new (&storage_.str) std::string(s);     // +0x10: copy-construct string
    which_ = 3;                         // +0x08: movl $0x3, 0x8(%rbx)
}

// ============================================================================
// Value() default ctor addition.
//
// Not defined in the binary (no symbol `_ZN6zhinst5ValueC1Ev` present).
// Required by Resources::Variable initialization (Variable holds a Value
// member at +0x18..+0x40 that must be default-constructible).
//
// Behaviour: zero out the tag/discriminator and leave storage in the
// trivially-zero state (`storage_.i = 0`, set by `Storage::Storage()`).
// type_=Unspecified causes any toX() conversion to throw, matching the
// "uninitialized variable" error path.
// ============================================================================
Value::Value()
    : type_(ValueType::Unspecified)
    , pad_04_{}
    , which_(0)        // matches Int variant slot — but type_=Unspecified disables conversions
    , pad_0C_{}
    , storage_{}       // Storage() default-init: i=0 (trivial zero)
{}

// ============================================================================
// Value copy/move constructors and assignment operators.
//
// Not present as symbols in the binary — the original libc++ build used
// char[24] for string storage, making the union trivially copyable and
// the compiler generated implicit copies. On libstdc++ (32-byte string),
// the union has a real std::string member, so we must provide explicit
// copy/move semantics that mirror the binary's variant-aware behaviour.
//
// Logic mirrors Value::~Value: which_ decoded via (w >> 31) ^ w;
// if decoded >= 3, the storage holds a std::string.
// ============================================================================

Value::Value(Value const& other)
    : type_(other.type_), pad_04_{}, which_(other.which_), pad_0C_{}
{
    int decoded = (other.which_ >> 31) ^ other.which_;
    if (decoded >= 3)
        new (&storage_.str) std::string(other.storage_.str);
    else
        std::memcpy(&storage_, &other.storage_, sizeof(storage_));
}

Value::Value(Value&& other) noexcept
    : type_(other.type_), pad_04_{}, which_(other.which_), pad_0C_{}
{
    int decoded = (other.which_ >> 31) ^ other.which_;
    if (decoded >= 3)
        new (&storage_.str) std::string(std::move(other.storage_.str));
    else
        std::memcpy(&storage_, &other.storage_, sizeof(storage_));
}

Value& Value::operator=(Value const& other) {
    if (this != &other) {
        // Destroy current string if held
        int curDecoded = (which_ >> 31) ^ which_;
        if (curDecoded >= 3)
            storage_.str.~basic_string();
        // Copy
        type_ = other.type_;
        which_ = other.which_;
        int decoded = (other.which_ >> 31) ^ other.which_;
        if (decoded >= 3)
            new (&storage_.str) std::string(other.storage_.str);
        else
            std::memcpy(&storage_, &other.storage_, sizeof(storage_));
    }
    return *this;
}

Value& Value::operator=(Value&& other) noexcept {
    if (this != &other) {
        int curDecoded = (which_ >> 31) ^ which_;
        if (curDecoded >= 3)
            storage_.str.~basic_string();
        type_ = other.type_;
        which_ = other.which_;
        int decoded = (other.which_ >> 31) ^ other.which_;
        if (decoded >= 3)
            new (&storage_.str) std::string(std::move(other.storage_.str));
        else
            std::memcpy(&storage_, &other.storage_, sizeof(storage_));
    }
    return *this;
}

// ============================================================================
// Value::toDouble() — 0x15a560
//
// Switch on type_:
//   Unspecified(0): throw "unspecified value type detected in toDouble conversion"
//   Int(1):         get<int> → cvtsi2sd → double
//   Bool(2):        get<bool> → (int)(bool) → double
//   Double(3):      get<double> → return
//   String(4):      get<string> → std::stod()
//   >4:             throw "unknown value type detected in toDouble conversion"
// ============================================================================
double Value::toDouble() const {  // 0x15a560
    switch (type_) {
        case ValueType::Unspecified:
            BOOST_THROW_EXCEPTION(ValueException(
                "unspecified value type detected in toDouble conversion"));
        case ValueType::Int:
            return static_cast<double>(storage_.i);
        case ValueType::Bool:
            return static_cast<double>(storage_.b);
        case ValueType::Double:
            return storage_.d;
        case ValueType::String:
            return std::stod(storage_.str);
        default:
            BOOST_THROW_EXCEPTION(ValueException(
                "unknown value type detected in toDouble conversion"));
    }
}

// ============================================================================
// Value::toInt() — 0x15c250
//
// Switch on type_:
//   Unspecified(0): throw
//   Int(1):         return directly
//   Bool(2):        return (unsigned)bool
//   String(4):      std::stol(str, nullptr, 10)
//   Double(3):      range check → floor/ceil → cvttsd2si
//                   On int overflow, catches and retries as uint32_t
//   >4:             throw
// ============================================================================
int32_t Value::toInt() const {  // 0x15c250
    switch (type_) {
        case ValueType::Unspecified:
            BOOST_THROW_EXCEPTION(ValueException(
                "unspecified value type detected in toInt conversion"));
        case ValueType::Int:
            return storage_.i;
        case ValueType::Bool:
            return static_cast<int32_t>(storage_.b);
        case ValueType::String:
            return static_cast<int32_t>(
                std::stol(storage_.str, nullptr, 10));
        case ValueType::Double: {
            double d = storage_.d;
            double truncated = (d < 0.0) ? std::ceil(d) : std::floor(d);
            // @0x15c250: binary uses cvttsd2si; on overflow (value outside
            // int32_t range), x86 cvttsd2si returns 0x80000000 ("indefinite").
            // The binary detects this and retries via uint32_t truncation,
            // which correctly handles large hex literals like 0xAAAAAAAA that
            // are stored as positive doubles (e.g. 2863311530.0).
            if (truncated >= static_cast<double>(INT32_MIN) &&
                truncated <= static_cast<double>(INT32_MAX)) {
                return static_cast<int32_t>(truncated);
            }
            // Overflow path: reinterpret via uint32_t (wrapping truncation)
            return static_cast<int32_t>(static_cast<uint32_t>(truncated));
        }
        default:
            BOOST_THROW_EXCEPTION(ValueException(
                "unknown value type detected in toInt conversion"));
    }
}

// ============================================================================
// Value::toBool() — 0x164200
//
// Switch on type_:
//   Unspecified(0): throw
//   Int(1):         return value != 0
//   Bool(2):        return directly
//   String(4):      return length==4 && memcmp(data, "true", 4) == 0
//   Double(3):      return fabs(d) >= epsilon (~1e-15)
//   >4:             throw
// ============================================================================
bool Value::toBool() const {  // 0x164200
    switch (type_) {
        case ValueType::Unspecified:
            BOOST_THROW_EXCEPTION(ValueException(
                "unspecified value type detected in toBool conversion"));
        case ValueType::Int:
            return storage_.i != 0;
        case ValueType::Bool:
            return storage_.b;
        case ValueType::String: {
            auto const& s = storage_.str;
            return s.size() == 4 && std::memcmp(s.data(), "true", 4) == 0;
        }
        case ValueType::Double:
            return std::fabs(storage_.d) >= 1e-15;  // epsilon from .rodata
        default:
            BOOST_THROW_EXCEPTION(ValueException(
                "unknown value type detected in toBool conversion"));
    }
}

// ============================================================================
// Value::toString() — 0x15de50
//
// Switch on type_:
//   Unspecified(0): throw
//   Int(1):         std::to_string(get<int>)
//   Bool(2):        std::to_string((int)get<bool>)
//   String(4):      copy-construct from variant
//   Double(3):      std::to_string(get<double>)
//   >4:             throw
// ============================================================================
std::string Value::toString() const {  // 0x15de50
    switch (type_) {
        case ValueType::Unspecified:
            BOOST_THROW_EXCEPTION(ValueException(
                "unspecified value type detected in toString conversion"));
        case ValueType::Int:
            return std::to_string(storage_.i);
        case ValueType::Bool:
            return std::to_string(static_cast<int>(storage_.b));
        case ValueType::String:
            return storage_.str;
        case ValueType::Double:
            return std::to_string(storage_.d);
        default:
            BOOST_THROW_EXCEPTION(ValueException(
                "unknown value type detected in toString conversion"));
    }
}

// ============================================================================
// Value::operator==(Value const&) — 0x21a780
//
// If type_ differs → false.
// Per-type dispatch:
//   Int:    get<int> == other.toInt()
//   Bool:   get<bool> == other.toBool()
//   String: toString() == other.toString()
//   Double: fabs(get<double> - other.toDouble()) < epsilon
//   Unspecified/unknown: throw
// ============================================================================
bool Value::operator==(Value const& other) const {  // 0x21a780
    if (static_cast<int>(type_) != static_cast<int>(other.type_))
        return false;
    switch (type_) {
        case ValueType::Int:
            return storage_.i == other.toInt();
        case ValueType::Bool:
            return storage_.b == other.toBool();
        case ValueType::String:
            return toString() == other.toString();
        case ValueType::Double: {
            double diff = storage_.d - other.toDouble();
            return std::fabs(diff) < 1e-15;  // epsilon from .rodata
        }
        default:
            BOOST_THROW_EXCEPTION(ValueException(
                "unspecified value type detected in comparison"));
    }
}

// ============================================================================
// Value::~Value() — 0x15a9c0
//
// Decodes which_: decoded = (which_ >> 31) ^ which_.
// If decoded < 3 → return (int/bool/double are trivially destructible).
// If decoded >= 3 → destroy string at +0x10.
// ============================================================================
Value::~Value() {  // 0x15a9c0
    int decoded = (which_ >> 31) ^ which_;
    if (decoded >= 3) {
        storage_.str.~basic_string();
    }
}

} // namespace zhinst
