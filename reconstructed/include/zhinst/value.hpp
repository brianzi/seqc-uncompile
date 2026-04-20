// ============================================================================
// Reconstructed from disassembly of _seqc_compiler.so
// Value and Immediate types — instruction operands
//
// Source path (from error strings):
//   /builds/labone/labone/ziAWG/ziAWGUtils/src/main/include/Value.hpp
// ============================================================================
#pragma once

#include <cstdint>
#include <cmath>
#include <string>
#include <variant>
#include <boost/variant.hpp>
#include <boost/numeric/conversion/converter.hpp>
#include <boost/throw_exception.hpp>

namespace zhinst {

// ============================================================================
// AddressImpl<T> — thin wrapper around an unsigned value
// ============================================================================
namespace detail {
    template <typename T>
    struct AddressImpl {
        T value;

        AddressImpl() : value(0) {}
        explicit AddressImpl(T v) : value(v) {}

        operator T() const { return value; }
    };
}

// ============================================================================
// Immediate — std::variant<AddressImpl<uint>, int, std::string>
//
// Layout (28 bytes total, 0x1C):
//   +0x00  [24 bytes]  union storage:
//            - AddressImpl<uint>: 4 bytes at +0x00 (uint32_t)
//            - int:               4 bytes at +0x00
//            - std::string:       24 bytes at +0x00 (libc++ SSO string)
//   +0x18  [4 bytes]   variant index (uint32_t)
//            0 = AddressImpl<uint>  (a.k.a. "unsigned" / holdsUnsigned)
//            1 = int
//            2 = std::string
//            0xFFFFFFFF = valueless_by_exception
//
// This is a std::variant using the libc++ layout where the discriminator
// follows the union storage.  The visitor dispatch uses function-pointer
// tables stored in .data.rel.ro (not a switch/jump-table).
// ============================================================================
class Immediate {
public:
    // --- Storage ---
    // std::variant handles this internally; shown here for clarity
    union Storage {
        detail::AddressImpl<uint32_t> address;  // index 0
        int32_t                       integer;  // index 1
        std::string                   str;      // index 2

        Storage() {}
        ~Storage() {}
    };

    Storage data_;     // +0x00, 24 bytes
    uint32_t index_;   // +0x18, 4 bytes

    // --- Constructors ---

    // Immediate(AddressImpl<uint>) — 0x290ab0
    //   mov esi, [rdi+0x00]    ; store value at +0x00
    //   mov dword [rdi+0x18], 0 ; index = 0
    Immediate(detail::AddressImpl<uint32_t> addr)
        : index_(0)
    {
        data_.address = addr;
    }

    // Immediate(unsigned int) — 0x290ac0
    //   mov esi, [rdi+0x00]     ; store value at +0x00
    //   mov dword [rdi+0x18], 0 ; index = 0
    // NOTE: unsigned int constructor also sets index=0, same as AddressImpl!
    // This wraps the uint into AddressImpl<uint> implicitly.
    Immediate(uint32_t v)
        : index_(0)
    {
        data_.address.value = v;
    }

    // Immediate(int) — 0x290ad0
    //   mov esi, [rdi+0x00]     ; store value at +0x00
    //   mov dword [rdi+0x18], 1 ; index = 1
    Immediate(int32_t v)
        : index_(1)
    {
        data_.integer = v;
    }

    // Immediate(string const&) — 0x290ae0
    //   Copies string (SSO or heap) into +0x00..+0x17
    //   mov dword [rdi+0x18], 2 ; index = 2
    Immediate(std::string const& s)
        : index_(2)
    {
        new (&data_.str) std::string(s);
    }

    // ~Immediate() — 0x15c4f0
    //   if (index_ != 0xFFFFFFFF) call destructor via vtable[index_]
    //   index_ = 0xFFFFFFFF   (valueless)
    // Only index=2 (string) actually needs destruction.
    ~Immediate() {
        if (index_ == 2) {
            data_.str.~basic_string();
        }
        index_ = 0xFFFFFFFF;
    }

    // --- Methods ---

    // holdsUnsigned() — 0x290b30
    //   return index_ == 0
    bool holdsUnsigned() const {
        return index_ == 0;
    }

    // operator int() — 0x290cc0
    // Dispatches through visitor vtable at b070b0[index_]
    // Throws bad_variant_access if valueless (index_ == 0xFFFFFFFF)
    operator int() const {
        if (index_ == 0xFFFFFFFF)
            std::__throw_bad_variant_access();
        // Visitor returns:
        //   index 0 (AddressImpl<uint>): static_cast<int>(data_.address.value)
        //   index 1 (int): data_.integer
        //   index 2 (string): likely std::stoi or similar
        return (index_ == 1) ? data_.integer
             : static_cast<int>(data_.address.value);
    }

    // operator unsigned int() — 0x290d00
    // Same pattern, vtable at b070c8[index_]
    operator unsigned int() const {
        if (index_ == 0xFFFFFFFF)
            std::__throw_bad_variant_access();
        return (index_ == 0) ? data_.address.value
             : static_cast<uint32_t>(data_.integer);
    }

    // operator==(Immediate) — 0x290d40
    //   if (this->index_ == 0xFFFFFFFF || lhs.index_ != rhs.index_) return false
    //   dispatch comparison through vtable at b070e0[index_]
    bool operator==(Immediate other) const {
        if (index_ == 0xFFFFFFFF) return false;
        if (index_ != other.index_)  return false;
        // Per-type comparison dispatched via visitor
        switch (index_) {
            case 0: return data_.address.value == other.data_.address.value;
            case 1: return data_.integer == other.data_.integer;
            case 2: return data_.str == other.data_.str;
        }
        return false;
    }
};
static_assert(sizeof(Immediate) <= 32, "Immediate should be ~28 bytes");

// toString(Immediate) — 0x290b40 (free function, returns std::string by value)
//   Dispatches through vtable at b07068[index_]
//   Throws bad_variant_access if valueless
// For index 0: likely std::to_string(address.value) as unsigned
// For index 1: likely std::to_string(integer)
// For index 2: returns the string directly
inline std::string toString(Immediate imm) {
    // visitor dispatch
    switch (imm.index_) {
        case 0: return std::to_string(imm.data_.address.value);
        case 1: return std::to_string(imm.data_.integer);
        case 2: return imm.data_.str;
    }
    std::__throw_bad_variant_access();
}

// ============================================================================
// ValueException — thrown by Value conversion methods
// ============================================================================
class ValueException : public std::exception {
    std::string msg_;
public:
    explicit ValueException(std::string const& msg) : msg_(msg) {}
    ~ValueException() override;
    const char* what() const noexcept override { return msg_.c_str(); }
};

// ============================================================================
// Value — boost::variant<int, bool, double, std::string>
//
// This is a BOOST variant (not std::variant). The discriminator uses boost's
// "which()" encoding where negative values encode backup types during
// assignment (the sign bit trick: `(which >> 31) ^ which` gives the real
// type index).
//
// Layout (40 bytes, 0x28):
//   +0x00  [4 bytes]   int which_    (boost::variant discriminator)
//                       Positive = direct type, negative = backup during assign
//                       Decoded index = (which_ >> 31) ^ which_
//                       0 = int
//                       1 = bool
//                       2 = double  (*** NOT string — see below)
//                       3 = string  (when decoded_index >= 3)
//   +0x04  [4 bytes]   (padding)
//   +0x08  [4 bytes]   additional discriminator/flags (from 0x8(rdi) reads)
//                       Actually: this IS which_ at +0x00, and +0x08 is part
//                       of storage. Re-examining...
//
// CORRECTION based on disassembly:
//   +0x00  [4 bytes]   type tag (0..4, used as jump-table index)
//                       0 = "unspecified" / empty → throws ValueException
//                       1 = int         (value at +0x10, 4 bytes)
//                       2 = bool        (value at +0x10, 1 byte)
//                       3 = double      (value at +0x10, 8 bytes)
//                       4 = string      (value at +0x10, 24 bytes SSO)
//
//   Actually the disassembly of toDouble shows:
//     mov eax, [rdi+0x00]    ; load type tag
//     cmp rax, 4             ; > 4 → unknown type → throw
//     jump-table dispatch on rax (5 cases: 0,1,2,3,4)
//
//   Case 0 → 0x15a606: throws "unspecified value type detected in toDouble"
//   Case 1 → 0x15a585: boost::get<int> then cvtsi2sd → int to double
//   Case 2 → 0x15a5a1: boost::get<bool> then cvtsi2sd → bool to double
//   Case 3 → 0x15a5e7: boost::get<double> → already double, return it
//            Wait no — case 3 tries get<double> with check ecx==2
//   Case 4 → 0x15a5c3: boost::get<string> then std::stod → string to double
//
// So the +0x00 field is the top-level type selector with 5 values (0=empty,
// 1-4 = the 4 variant alternatives), BUT then there's also the boost which_
// field at +0x08.
//
// Actually re-reading more carefully:
//   Case 1 (int):    loads [rdi+0x08] into eax, does (eax>>31)^eax, checks ==0
//                    → this is boost which_ at +0x08
//                    If which_decoded==0, reads int from [rdi+0x10]
//   Case 2 (bool):   which_decoded must == 1, reads bool from [rdi+0x10]
//   Case 3 (double): which_decoded must == 2, reads double from [rdi+0x10]
//   Case 4 (string): which_decoded must > 2, reads string from [rdi+0x10]
//
// So the layout is actually:
//   +0x00  [4 bytes]   outer_tag (0..4 jump table, possibly redundant with which_)
//   +0x04  [4 bytes]   (padding)
//   +0x08  [4 bytes]   boost::variant::which_  (boost's internal discriminator)
//   +0x0C  [4 bytes]   (padding)
//   +0x10  [24 bytes]  union storage {int, bool, double, string}
//   +0x28  END → total 40 bytes
//
// BUT WAIT — the destructor at 0x15a9c0:
//   mov eax, [rdi+0x08]       ; which_
//   decode = (eax>>31) ^ eax
//   if decode < 3 → return (int/bool/double need no destruction)
//   if [rdi+0x10] & 1 → long string → free heap
//
// This confirms: +0x08 = which_, +0x10 = storage start, string at index ≥3.
// The field at +0x00 appears to be a separate outer enum/tag.
//
// For operator==:
//   Compares [rdi+0x00] first (the outer tag). If different → false.
//   Then dispatches per-type comparison using the outer tag as index.
//
// The outer tag at +0x00 appears to be a separate "ValueType" enum that
// mirrors the variant alternative but adds an "unspecified" (0) state.
// ============================================================================

enum class ValueType : int32_t {
    Unspecified = 0,   // throws on any conversion attempt
    Int        = 1,    // holds int32_t
    Bool       = 2,    // holds bool
    Double     = 3,    // holds double  (NOTE: swapped vs variant order)
    String     = 4,    // holds std::string
};

class Value {
public:
    // +0x00: outer type tag (ValueType enum)
    ValueType type_;

    // +0x04: padding

    // +0x08: boost::variant<int, bool, double, std::string>::which_
    //        Decoded: 0=int, 1=bool, 2=double, 3+=string
    int32_t which_;

    // +0x0C: padding

    // +0x10: union storage (24 bytes for SSO string)
    union Storage {
        int32_t     i;     // which==0, type_==Int
        bool        b;     // which==1, type_==Bool
        double      d;     // which==2, type_==Double
        // std::string occupies all 24 bytes (which>=3, type_==String)
        char        str_storage[24];

        Storage() : i(0) {}
        ~Storage() {}
    };
    Storage storage_;

    // --- Conversion methods ---

    // toDouble() — 0x15a560
    // Switch on type_:
    //   Unspecified(0): throw ValueException("unspecified value type detected in toDouble conversion")
    //   Int(1):         boost::get<int>(variant) → cvtsi2sd → double
    //   Bool(2):        boost::get<bool>(variant) → (int)(bool) → double
    //   Double(3):      boost::get<double>(variant) → return directly
    //   String(4):      boost::get<string>(variant) → std::stod()
    //   >4:             throw ValueException("unknown value type detected in toDouble conversion")
    double toDouble() const {
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
                return std::stod(*reinterpret_cast<const std::string*>(&storage_));
            default:
                BOOST_THROW_EXCEPTION(ValueException(
                    "unknown value type detected in toDouble conversion"));
        }
    }

    // toInt() — 0x15c250
    // Switch on type_:
    //   Unspecified(0): throw ValueException("unspecified value type ... toInt")
    //   Int(1):         boost::get<int> → return directly
    //   Bool(2):        boost::get<bool> → return (unsigned)bool (movzbl)
    //   String(4):      boost::get<string> → std::stol(str, nullptr, 10)
    //   Double(3):      boost::get<double> → range check → floor/ceil → cvttsd2si
    //                   If range check throws (overflow), catches exception,
    //                   re-gets as double, range-checks for uint32_t, then
    //                   floor/ceil → cvttsd2si as int64 (unsigned path)
    //   >4:             throw ValueException("unknown value type ... toInt")
    int32_t toInt() const {
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
                    std::stol(*reinterpret_cast<const std::string*>(&storage_),
                              nullptr, 10));
            case ValueType::Double: {
                // Try int range first
                double d = storage_.d;
                // boost::numeric range check for int←double
                // On overflow, catches and tries uint←double range instead
                if (d < 0.0)
                    return static_cast<int32_t>(std::ceil(d));
                else
                    return static_cast<int32_t>(std::floor(d));
            }
            default:
                BOOST_THROW_EXCEPTION(ValueException(
                    "unknown value type detected in toInt conversion"));
        }
    }

    // toBool() — 0x164200
    // Switch on type_:
    //   Unspecified(0): throw ValueException("unspecified value type ... toBool")
    //   Int(1):         boost::get<int> → return value != 0
    //   Bool(2):        boost::get<bool> → return directly
    //   String(4):      boost::get<string> → compare length==4 then memcmp "true"
    //   Double(3):      boost::get<double> → return fabs(d) >= epsilon
    //   >4:             throw ValueException("unknown value type ... toBool")
    bool toBool() const {
        switch (type_) {
            case ValueType::Unspecified:
                BOOST_THROW_EXCEPTION(ValueException(
                    "unspecified value type detected in toBool conversion"));
            case ValueType::Int:
                return storage_.i != 0;
            case ValueType::Bool:
                return storage_.b;
            case ValueType::String: {
                auto const& s = *reinterpret_cast<const std::string*>(&storage_);
                return s.size() == 4 && std::memcmp(s.data(), "true", 4) == 0;
            }
            case ValueType::Double:
                return std::fabs(storage_.d) >= /* epsilon from .rodata */ 1e-15;
            default:
                BOOST_THROW_EXCEPTION(ValueException(
                    "unknown value type detected in toBool conversion"));
        }
    }

    // toString() — 0x15de50
    // Switch on type_:
    //   Unspecified(0): throw ValueException("unspecified value type ... toString")
    //   Int(1):         std::to_string(get<int>)
    //   Bool(2):        std::to_string((int)get<bool>)  (treats bool as int)
    //   String(4):      copy-construct string from variant
    //   Double(3):      std::to_string(get<double>)
    //   >4:             throw ValueException("unknown value type ... toString")
    std::string toString() const {
        switch (type_) {
            case ValueType::Unspecified:
                BOOST_THROW_EXCEPTION(ValueException(
                    "unspecified value type detected in toString conversion"));
            case ValueType::Int:
                return std::to_string(storage_.i);
            case ValueType::Bool:
                return std::to_string(static_cast<int>(storage_.b));
            case ValueType::String:
                return *reinterpret_cast<const std::string*>(&storage_);
            case ValueType::Double:
                return std::to_string(storage_.d);
            default:
                BOOST_THROW_EXCEPTION(ValueException(
                    "unknown value type detected in toString conversion"));
        }
    }

    // operator==(Value const&) — 0x21a780
    // If type_ differs → false
    // Per-type dispatch:
    //   Int:    get<int> == other.toInt()
    //   Bool:   get<bool> == other.toBool()
    //   String: toString() == other.toString()   (compares string representations)
    //   Double: fabs(get<double> - other.toDouble()) < epsilon
    //   Unspecified/unknown: throw ValueException("... in comparison")
    bool operator==(Value const& other) const {
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
                return std::fabs(diff) < /* epsilon */ 1e-15;
            }
            default:
                BOOST_THROW_EXCEPTION(ValueException(
                    "unspecified value type detected in comparison"));
        }
    }

    // ~Value() — 0x15a9c0
    //   decode = (which_ >> 31) ^ which_
    //   if decode < 3 → return  (int/bool/double trivially destructible)
    //   if string is heap-allocated (bit 0 of first byte set) → free
    ~Value() {
        int decoded = (which_ >> 31) ^ which_;
        if (decoded >= 3) {
            // Destroy string
            reinterpret_cast<std::string*>(&storage_)->~basic_string();
        }
    }
};

static_assert(sizeof(Value) == 40, "Value should be 40 bytes");

} // namespace zhinst
