// ============================================================================
// Reconstructed from disassembly of _seqc_compiler.so
// NodeMapData hierarchy + NodeMapItem — implementation extracted from
// custom_functions.cpp file-organization split (audit
// Section C1).
//
// See notes/audit_phase16a.md for context.
// ============================================================================

#include "zhinst/runtime/node_map_data.hpp"

#include <cstring>
#include <functional>

namespace zhinst {

// ============================================================================
// NodeMapData
// ============================================================================

NodeMapData::~NodeMapData() {}  // @0x1c5720

// ============================================================================
// VirtAddrNodeMapData
// ============================================================================

// @0x1c4dc0 — copy ctor: copies vptr, name_ (string), addresses_ (vector)
VirtAddrNodeMapData::VirtAddrNodeMapData(VirtAddrNodeMapData const& other)
    : NodeMapData(), name_(other.name_), addresses_(other.addresses_) {
}

VirtAddrNodeMapData::~VirtAddrNodeMapData() {}  // @0x1c4ec0

// @0x1c4cc0 — Constructs a local VirtAddrNodeMapData copy of `other` (assumes
// caller has already verified type compatibility via vtable typeinfo check in
// operator==), then compares name_ and addresses_ byte-for-byte.
bool VirtAddrNodeMapData::compareEq(NodeMapData const& other) const {
    // The disasm copy-constructs a VirtAddrNodeMapData from other directly
    // (no dynamic_cast). This is safe because NodeMapItem::operator== checks
    // vtable typeinfo equality before dispatching here.
    auto const& o = static_cast<VirtAddrNodeMapData const&>(other);
    return name_ == o.name_ && addresses_ == o.addresses_;
}

// @0x1c4f10 — Hashes name_ via std::hash<string> (libc++ wyhash), then
// hash-combines each element of addresses_ using a splitmix-style finalizer
// with seed 0x9e3779b9.
size_t VirtAddrNodeMapData::hash() const {
    size_t h = std::hash<std::string>{}(name_);
    // Combine with addresses: iterative hash_combine using golden-ratio seed
    // (matches the 0x9e3779b9 constant and splitmix finalizer in the disasm)
    size_t seed = 0;
    constexpr size_t kGoldenRatioHash = 0x9e3779b9ULL;
    constexpr size_t kMul    = 0x0e9846af9b1a615dULL;
    for (int32_t addr : addresses_) {
        seed += kGoldenRatioHash;
        size_t v = static_cast<size_t>(addr) + seed;
        v = (v ^ (v >> 32)) * kMul;
        v = (v ^ (v >> 32)) * kMul;
        v = v ^ (v >> 28);
        seed = v;
    }
    // Final combine: splitmix the wyhash result, then combine with address seed
    // and splitmix again (matches binary at 0x1c517d–0x1c51cf).
    size_t tmp = h + kGoldenRatioHash;
    tmp = (tmp ^ (tmp >> 32)) * kMul;
    tmp = (tmp ^ (tmp >> 32)) * kMul;
    tmp = tmp ^ (tmp >> 28);
    size_t combined = tmp + seed + kGoldenRatioHash;
    combined = (combined ^ (combined >> 32)) * kMul;
    combined = (combined ^ (combined >> 32)) * kMul;
    combined = combined ^ (combined >> 28);
    return combined;
}

// @0x1c51e0 — allocates 0x38 bytes, copy-constructs into it
NodeMapData* VirtAddrNodeMapData::clone() const {
    return new VirtAddrNodeMapData(*this);
}

// @0x1c5240 — writes "name" key from name_, builds a json::array from
// addresses_ (int32_t elements), assigns to "addrs" key.
void VirtAddrNodeMapData::getJson(boost::json::object& obj) const {
    // obj["name"] = name_;  (string literal "name" at 904d94, len 4)
    obj["name"] = boost::json::string_view(name_.data(), name_.size());
    // Build array of addresses
    boost::json::array arr;
    for (int32_t a : addresses_) {
        arr.push_back(static_cast<std::int64_t>(a));
    }
    // obj["index"] = std::move(arr);  (string literal "index" at 8fe799, len 5)
    obj["index"] = std::move(arr);
}

// ============================================================================
// DirectAddrNodeMapData
// ============================================================================

DirectAddrNodeMapData::~DirectAddrNodeMapData() {}  // @0x1c5730

// @0x1c5460 — Simply compares this->addr_ (at +0x08) with other's +0x08.
// No dynamic_cast; caller ensures type match before calling.
bool DirectAddrNodeMapData::compareEq(NodeMapData const& other) const {
    auto const& o = static_cast<DirectAddrNodeMapData const&>(other);
    return addr_ == o.addr_;
}

// @0x1c5370 — splitmix-style hash of addr_ with seed kGoldenRatioHash
size_t DirectAddrNodeMapData::hash() const {
    constexpr size_t kGoldenRatioHash = 0x9e3779b9ULL;
    constexpr size_t kMul = 0x0e9846af9b1a615dULL;
    size_t v = static_cast<size_t>(addr_) + kGoldenRatioHash;
    v = (v ^ (v >> 32)) * kMul;
    v = (v ^ (v >> 32)) * kMul;
    v = v ^ (v >> 28);
    return v;
}

// @0x1c53c0 — allocates 0x10, sets vptr, copies addr_
NodeMapData* DirectAddrNodeMapData::clone() const {
    auto* p = new DirectAddrNodeMapData();
    p->addr_ = addr_;
    return p;
}

// @0x1c5400 — obj["direct_address"] = (uint64_t)addr_
// String literal "direct_address" at 904d99, len 0xd (13). The disasm
// directly writes the int64 representation into the json::value storage
// (tag byte 0x3 = boost::json int64).
void DirectAddrNodeMapData::getJson(boost::json::object& obj) const {
    obj["direct_address"] = static_cast<std::int64_t>(addr_);
}

// ============================================================================
// NodeMapItem
// ============================================================================

// @0x1c5490 — Compares typeIdx first, then hasFast flags (with fastAddr if
// both have fast), then checks vtable typeinfo match before dispatching to
// data->compareEq(*other.data).
bool NodeMapItem::operator==(NodeMapItem const& other) const {
    // 1c5494: compare typeIdx (offset +0x08)
    if (typeIdx != other.typeIdx)
        return false;
    // 1c549c-1c54c0: compare hasFast flags; if both true, compare fastAddr
    bool thisHas  = hasFast;
    bool otherHas = other.hasFast;
    if (thisHas && otherHas) {
        if (fastAddr != other.fastAddr)
            return false;
    } else if (thisHas != otherHas) {
        return false;
    }
    // 1c54c2-1c54de: check that both data pointers have same vtable typeinfo
    // (via vptr -> typeinfo pointer comparison), then tail-call compareEq
    if (typeid(*data) != typeid(*other.data))
        return false;
    return data->compareEq(*other.data);
}

// @0x1c5470 — returns fastAddr if hasFast, else 0
uint32_t NodeMapItem::fastAddress() const {
    return hasFast ? fastAddr : 0;
}

// @0x1c54f0 — Creates a json::object, calls data->getJson(obj), then looks up
// typeIdx-1 in a table to get a type multiplier/value, and sets obj["type"].
// The table at 0x95ad18 has 4 uint64 entries for typeIdx values 1-4.
// Default (typeIdx outside 1-4) is 1.
boost::json::value NodeMapItem::toJson() const {
    // 1c5500: initialize as empty object (tag 0x7 = object)
    boost::json::object obj;
    // 1c5516-1c551f: call data->getJson(obj) via vtable slot +0x20
    data->getJson(obj);
    // 1c5522-1c553c: lookup typeIdx-1 in table, default to 1
    // The table at 95ad18 contains 4 int64 values; exact values depend on
    // the enum encoding. From the disasm pattern (typeIdx 1-4 -> table[0-3]),
    // this maps node type indices to a JSON-serializable type code.
    std::int64_t sizeVal = 1;
    unsigned idx = static_cast<unsigned>(typeIdx) - 1;
    if (idx <= 3) {
        // Static lookup table at 0x95ad18 — 4 x int64, verified from binary:
        //   typeIdx 1 -> 2, 2 -> 1, 3 -> 2, 4 -> 2
        static const std::int64_t sizeTable[4] = {2, 1, 2, 2};
        sizeVal = sizeTable[idx];
    }
    // 1c5540-1c5578: obj["size"] = sizeVal  (string literal "size" at 904da7, len 4)
    obj["size"] = sizeVal;
    return boost::json::value(std::move(obj));
}

// @0x1c55a0 — Looks up typeIdx-1 in a 4-entry uint32 table at 0x8fc630.
// Default (out of range) returns 1.
size_t NodeMapItem::size() const {
    unsigned idx = static_cast<unsigned>(typeIdx) - 1;
    if (idx <= 3) {
        // Static lookup table at 0x8fc630 — 4 x uint32, verified from binary:
        //   typeIdx 1 -> 2, 2 -> 1, 3 -> 2, 4 -> 2
        static const uint32_t sizeTable[4] = {2, 1, 2, 2};
        return sizeTable[idx];
    }
    return 1;
}

} // namespace zhinst
