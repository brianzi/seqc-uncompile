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

#include "address_impl.hpp"

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
class Immediate {
public:
    union Storage {
        detail::AddressImpl<uint32_t> address;  // index 0
        int32_t                       integer;  // index 1
        std::string                   str;      // index 2

        Storage() {}
        ~Storage() {}
    };

    Storage data_;     // +0x00
    uint32_t index_;   // after union

    // --- Constructors (all confirmed from disassembly) ---
    Immediate() : index_(0xFFFFFFFF) {}       // default: valueless state
    Immediate(detail::AddressImpl<uint32_t> addr);  // 0x290ab0
    Immediate(uint32_t v);                           // 0x290ac0 — sets index=0 (same as AddressImpl)
    Immediate(int32_t v);                            // 0x290ad0 — sets index=1
    Immediate(std::string const& s);                 // 0x290ae0 — sets index=2
    Immediate(Immediate const& other);               // copy ctor (needed due to union with std::string)
    Immediate(Immediate&& other) noexcept;           // move ctor
    Immediate& operator=(Immediate const& other);    // copy assignment
    Immediate& operator=(Immediate&& other) noexcept; // move assignment
    ~Immediate();                                    // 0x15c4f0

    // --- Query ---
    bool holdsUnsigned() const;     // 0x290b30 — returns index_ == 0

    // --- Conversions ---
    operator int() const;           // 0x290cc0 — vtable dispatch at b070b0
    operator unsigned int() const;  // 0x290d00 — vtable dispatch at b070c8

    // --- Comparison ---
    bool operator==(Immediate const& other) const;  // 0x290d40 — vtable dispatch at b070e0
};

// toString(Immediate) — 0x290b40 (free function, sret)
// Dispatches through vtable at b07068. Returns string representation.
std::string toString(Immediate const& imm);

// operator<<(ostream&, Immediate const&) — 0x290b90
// Copies Immediate, calls toString, writes to ostream.
std::ostream& operator<<(std::ostream& os, Immediate const& imm);

// ============================================================================
// ValueException — thrown by Value conversion methods
//
// Layout (0x20 bytes):
//   +0x00  vptr (vtable at b03cc8+0x10)
//   +0x08  std::string msg_ (24 bytes, libc++ SSO)
// ============================================================================
class ValueException : public std::exception {
    std::string msg_;
public:
    explicit ValueException(std::string const& msg);  // 0x16e7f0
    ~ValueException() override;                        // 0x16e850
    const char* what() const noexcept override;        // 0x16f110
};

// ============================================================================
// ValueType — outer type tag for Value
// ============================================================================
enum class ValueType : int32_t {
    Unspecified = 0,   // throws on any conversion attempt
    Int        = 1,    // holds int32_t
    Bool       = 2,    // holds bool
    Double     = 3,    // holds double
    String     = 4,    // holds std::string
};

// ============================================================================
// Value — ValueType tag + tagged union
//
// Layout (0x20 = 32 bytes):
//   +0x00  [4 bytes]   ValueType type_   — outer tag (0..4)
//   +0x04  [4 bytes]   int32_t which_    — variant discriminator
//   +0x08  [24 bytes]  union storage {int, bool, double, string}
//   +0x20              END
//
// Note: Original binary uses libc++ (24-byte std::string). On libstdc++
// (32-byte std::string), the actual size will be larger. We use char[]
// for the string storage to keep the layout stable.
// ============================================================================
class Value {
public:
    ValueType type_;       // +0x00
    int32_t which_;        // +0x04
    union Storage {
        int32_t     i;              // which==0, type_==Int
        bool        b;              // which==1, type_==Bool
        double      d;              // which==2, type_==Double
        char        str_storage[24]; // which>=3, type_==String (libc++ SSO)

        Storage() : i(0) {}
        ~Storage() {}
    };
    Storage storage_;      // +0x08

    // --- Constructors ---
    // Int, Bool, Double constructors are fully inlined (no symbols).
    // String constructor is the only non-inline one:
    Value(std::string const& s);  // 0x22c2b0 — sets type_=4, which_=3, copies string
    Value();  // default ctor — needed by Resources::Variable initialization

    // --- Conversion methods ---
    double toDouble() const;       // 0x15a560
    int32_t toInt() const;         // 0x15c250
    bool toBool() const;           // 0x164200
    std::string toString() const;  // 0x15de50

    // --- Comparison ---
    bool operator==(Value const& other) const;  // 0x21a780

    // --- Destructor ---
    ~Value();  // 0x15a9c0
};

static_assert(sizeof(Value) == 32, "Value should be 32 bytes (0x20)");

} // namespace zhinst
