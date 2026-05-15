// ============================================================================
// Reconstructed from disassembly of _seqc_compiler.so
// device_type.cpp + 14b-ii-a
//
// Covers:
//   - DeviceType pimpl wrapper (ctors/dtor/getters)
//   - detail::DeviceTypeImpl base (ctors/dtor/getters)
//   - DeviceOptionSet (dual-storage container) and its iterator
//   - toString(DeviceFamily) / (DeviceTypeCode) / (DeviceOption,family) /
//     (DeviceType const&) / (DeviceOptionSet, family, separator)
//   - All predicate free functions (isDefined / isIa / isMfia / ... /
//     isVhfli / hasMf)
//   - toDeviceTypeCode (lazy-initialised function-local static map)
//
// Deferred to 14b-ii-b:
//   - DeviceType(string, vector<string>) full parsing
//   - All detail::* device subclasses (Hf2li, Mfli, ..., Vhfli)
//   - Per-family `initializeXxxOptions(unsigned long)` helpers (each is an
//     inline call site of detail::initializeSfcOptions<>)
// ============================================================================

#include "zhinst/device/device_type.hpp"
#include "zhinst/core/exception.hpp"
#include "zhinst/device/generic_device_type.hpp"
#include "zhinst/device/device_factories.hpp"

#include <algorithm>
#include <exception>
#include <map>
#include <ostream>
#include <stdexcept>
#include <tuple>
#include <unordered_map>
#include <utility>
#include <vector>

#include <boost/algorithm/string/classification.hpp>
#include <boost/algorithm/string/join.hpp>
#include <boost/algorithm/string/predicate.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/algorithm/string/trim.hpp>
#include <boost/throw_exception.hpp>

namespace zhinst {

// ============================================================================
// detail::DeviceTypeImpl
// ============================================================================
namespace detail {

// @ 0x2d3060
DeviceTypeImpl::DeviceTypeImpl()
    : code_(DeviceTypeCode::Unknown),
      family_(DeviceFamily::Unknown),
      options_(DeviceFamily::Unknown) {}

// @ 0x2d3090
DeviceTypeImpl::DeviceTypeImpl(DeviceTypeCode code, DeviceFamily family)
    : code_(code), family_(family), options_(family) {}

// @ 0x2d30b0
DeviceTypeImpl::DeviceTypeImpl(DeviceTypeCode code, DeviceFamily family,
                               DeviceOptionSet options)
    : code_(code), family_(family), options_(std::move(options)) {}

// @ 0x2d3190
//
// Tuple-taking ctor used exclusively by detail::GenericDeviceType. The
// compiled body unpacks the tuple inline (the first 8 bytes hold
// {code, family} packed; the next 72 bytes hold the DeviceOptionSet).
// At source level, std::get<>() + std::move() achieves the same effect;
// the compiler is free to inline it back to the same memcpy/move
// pattern seen in the disassembly.
DeviceTypeImpl::DeviceTypeImpl(
    std::tuple<DeviceTypeCode, DeviceFamily, DeviceOptionSet> args)
    : code_(std::get<0>(args)),
      family_(std::get<1>(args)),
      options_(std::move(std::get<2>(args))) {}

DeviceTypeImpl::~DeviceTypeImpl() = default;

DeviceTypeImpl* DeviceTypeImpl::doClone() const {
    return new DeviceTypeImpl(*this);
}

// @ 0x2d32e0
DeviceFamily DeviceTypeImpl::family() const { return family_; }

// @ 0x2d32f0
DeviceTypeCode DeviceTypeImpl::code() const { return code_; }

// @ 0x2d3300
bool DeviceTypeImpl::hasOption(DeviceOption opt) const {
    return options_.contains(opt);
}

// @ 0x2d3310
DeviceOptionSet const& DeviceTypeImpl::options() const { return options_; }

}  // namespace detail

// ============================================================================
// DeviceType (pimpl wrapper)
// ============================================================================

// @ 0x2d2900
DeviceType::DeviceType()
    : impl_(new detail::DeviceTypeImpl()) {}

// @ 0x2d2960
DeviceType::DeviceType(DeviceTypeCode code, DeviceFamily family)
    : impl_(new detail::DeviceTypeImpl(code, family)) {}

// @ 0x2d2ae0
//
// Constructs a GenericDeviceType from the runtime strings and pimpls
// it. The compiled body inlines the `new`+ctor call directly into this
// frame; the source-level shape is just one `new` expression.
DeviceType::DeviceType(std::string const& deviceType,
                       std::vector<std::string> const& options)
    : impl_(new detail::GenericDeviceType(deviceType, options)) {}

// @ 0x2d2a00
//
// Two-string convenience overload: splits the second string via
// splitDeviceOptions() and forwards to the (string, vector) ctor. The
// compiled body inlines the GenericDeviceType ctor and then walks the
// temporary vector for cleanup; at source level the construct-then-
// delegate pattern below mirrors what the developer wrote.
DeviceType::DeviceType(std::string const& deviceType,
                       std::string const& options)
    : impl_(new detail::GenericDeviceType(
          deviceType, splitDeviceOptions(options))) {}

// @ 0x2d2b40 — vtable[0] virtual clone.
DeviceType::DeviceType(DeviceType const& other)
    : impl_(other.impl_ ? other.impl_->doClone() : nullptr) {}

// @ 0x2d2b50 — pointer steal.
DeviceType::DeviceType(DeviceType&& other) noexcept
    : impl_(other.impl_) {
    other.impl_ = nullptr;
}

DeviceType& DeviceType::operator=(DeviceType const& other) {
    if (this != &other) {
        detail::DeviceTypeImpl* new_impl =
            other.impl_ ? other.impl_->doClone() : nullptr;
        delete impl_;
        impl_ = new_impl;
    }
    return *this;
}

DeviceType& DeviceType::operator=(DeviceType&& other) noexcept {
    if (this != &other) {
        delete impl_;
        impl_ = other.impl_;
        other.impl_ = nullptr;
    }
    return *this;
}

// @ 0x2d2b70
DeviceType::~DeviceType() { delete impl_; }

// @ 0x2d2c40
DeviceTypeCode DeviceType::code() const { return impl_->code(); }

// @ 0x2d2c30
DeviceFamily DeviceType::family() const { return impl_->family(); }

// @ 0x2d2c60
DeviceOptionSet const& DeviceType::options() const { return impl_->options(); }

// @ 0x2d2c50
bool DeviceType::hasOption(DeviceOption opt) const {
    return impl_->hasOption(opt);
}

// @ 0x2d2c70 — bitwise-AND test against the family enum.
bool DeviceType::belongsTo(DeviceFamily f) const {
    return (static_cast<uint32_t>(impl_->family()) &
            static_cast<uint32_t>(f)) != 0;
}

// @ 0x2d2930 — ctor from DeviceFamily, uses factory to create default impl.
DeviceType::DeviceType(DeviceFamily family) {
    auto factory = detail::makeDeviceFamilyFactory(family);
    impl_ = factory->makeDefault().release();
}

// @ 0x2d2990 — ctor from DeviceFamily + options bitmask.
DeviceType::DeviceType(DeviceFamily family, unsigned long options) {
    auto factory = detail::makeDeviceFamilyFactory(family);
    impl_ = factory->makeDeviceType(options).release();
}

// @ 0x2d2c20 — returns raw impl_ pointer.
detail::DeviceTypeImpl* DeviceType::deviceType() const {
    return impl_;
}

// @ 0x2d2cb0 — member toString: delegates to free function via code().
std::string DeviceType::toString() const {
    return zhinst::toString(impl_->code());
}

// @ 0x2d2ce0 — print: writes code to ostream.
void DeviceType::print(std::ostream& os) const {
    os << impl_->code();
}

// @ 0x2d2d10 — swap impl_ pointers.
void DeviceType::swap(DeviceType& other) {
    std::swap(impl_, other.impl_);
}

// @ 0x2d2d30 — equality by code.
//! \brief Compare two `DeviceType` values for equality.
//! \details Defined as equality of the underlying impl `code()`
//! (the `DeviceTypeCode` enum), so two `DeviceType` instances are
//! equal iff they identify the same hardware family.
//! \param lhs Left operand.
//! \param rhs Right operand.
//! \return `true` if both sides report the same `code()`.
bool operator==(DeviceType const& lhs, DeviceType const& rhs) {
    return lhs.impl_->code() == rhs.impl_->code();
}

// @ 0x2d2d60 — less-than by code.
//! \brief Strict total order on `DeviceType` by underlying code.
//! \details Allows `DeviceType` to be used as a key in ordered
//! associative containers; ordering reflects the numeric
//! `DeviceTypeCode` enum, not any human-meaningful sort.
//! \param lhs Left operand.
//! \param rhs Right operand.
//! \return `true` if `lhs.code()` is numerically less than
//!         `rhs.code()`.
bool operator<(DeviceType const& lhs, DeviceType const& rhs) {
    return lhs.impl_->code() < rhs.impl_->code();
}

// @ 0x2d2da0 — stream insertion: writes code.
//! \brief Stream-insert a `DeviceType` by writing its numeric
//!        `DeviceTypeCode` to the output stream.
//! \details Matches the binary's behaviour of streaming the raw
//! integer code rather than a human-readable name — callers that
//! want the family string should go through `toString()` instead.
//! \param os Destination stream.
//! \param dt Device type value.
//! \return Reference to `os` after insertion.
std::ostream& operator<<(std::ostream& os, DeviceType const& dt) {
    os << dt.impl_->code();
    return os;
}

// @ 0x2d2d90 — free function hasOption.
bool hasOption(DeviceType const& dt, DeviceOption const& opt) {
    return dt.hasOption(opt);
}

// ============================================================================
// DeviceOptionSet
// ============================================================================

// @ 0x2cf970
DeviceOptionSet::DeviceOptionSet(DeviceFamily family)
    : values_(), byName_(), family_(family) {}

// @ 0x2cf9a0
DeviceOptionSet::DeviceOptionSet(
    std::initializer_list<DeviceOption> const& options, DeviceFamily family)
    : values_(), byName_(), family_(family) {
    for (DeviceOption opt : options) {
        values_.insert(opt);
        byName_.emplace(toString(opt, family), opt);
    }
}

// @ 0x2cfb60
DeviceOptionSet::const_iterator DeviceOptionSet::begin() const {
    return const_iterator(byName_.begin());
}

// @ 0x2cfb70
DeviceOptionSet::const_iterator DeviceOptionSet::end() const {
    return const_iterator(byName_.end());
}

// @ 0x2cfb80
bool DeviceOptionSet::contains(DeviceOption opt) const {
    return values_.find(opt) != values_.end();
}

// @ 0x2cfcc0
bool DeviceOptionSet::empty() const { return values_.empty(); }

// @ 0x2cfcd0
std::size_t DeviceOptionSet::size() const { return values_.size(); }

// @ 0x2cfce0
DeviceFamily DeviceOptionSet::family() const { return family_; }

// @ 0x2cfcf0 — disasm only confirms the unordered_set insert; we mirror
// into byName_ to keep both stores consistent (matches the initializer-list
// ctor's invariant).
void DeviceOptionSet::insert(DeviceOption opt) {
    if (values_.insert(opt).second) {
        byName_.emplace(toString(opt, family_), opt);
    }
}

// @ 0x2cfd80 — compares the unordered_sets only.
// Binary is a 6-byte tail-call to std::operator== for the
// underlying unordered_set; family_ is not part of the contract.
//! \brief Compare two `DeviceOptionSet` instances for equality.
//! \details Equal iff the option codes (`values_`) match.  The
//! `family_` field is **not** part of the equality contract — two
//! sets with identical option codes but different families compare
//! equal.
//! \param a Left operand.
//! \param b Right operand.
//! \return `true` when both option sets contain the same codes.
bool operator==(DeviceOptionSet const& a, DeviceOptionSet const& b) {
    return a.values_ == b.values_;
}

// ============================================================================
// DeviceOptionSetConstIterator
// ============================================================================

// @ 0x2cf900
DeviceOptionSetConstIterator::DeviceOptionSetConstIterator(
    underlying_iterator it)
    : iter_(it) {}

// @ 0x2cf910
DeviceOption DeviceOptionSetConstIterator::dereference() const {
    return iter_->second;
}

// @ 0x2cf920
void DeviceOptionSetConstIterator::increment() { ++iter_; }

// @ 0x2cf960
bool DeviceOptionSetConstIterator::equal(
    DeviceOptionSetConstIterator const& other) const {
    return iter_ == other.iter_;
}

// ============================================================================
// toString(DeviceFamily) — @ 0x2df610
// ============================================================================
std::string toString(DeviceFamily family) {
    switch (family) {
        case DeviceFamily::Unknown: return std::string();
        case DeviceFamily::HF2:    return "HF2";
        case DeviceFamily::UHF:    return "UHF";
        case DeviceFamily::MF:     return "MF";
        case DeviceFamily::HDAWG:  return "HDAWG";
        case DeviceFamily::SHF:    return "SHF";
        case DeviceFamily::PQSC:   return "PQSC";
        case DeviceFamily::HWMOCK: return "HWMOCK";
        case DeviceFamily::SHFACC: return "SHFACC";
        case DeviceFamily::GHF:    return "GHF";
        case DeviceFamily::QHUB:   return "QHUB";
        case DeviceFamily::VHF:    return "VHF";
    }
    return "unknown";
}

// ============================================================================
// expand(DeviceFamily) — @ 0x2dfbc0
//
// Decomposes a DeviceFamily bitmask into its individual single-bit
// DeviceFamily values, returned as a std::vector<DeviceFamily>.
//
// The compiled body pre-allocates 0x2c bytes (= 11 × sizeof(int)), then
// iterates a bit cursor `bit = 1, 2, 4, 8, ...` while `bit < family`,
// appending `bit` if `(bit & family) != 0`. The cursor is also compared
// against 0x800 each iteration: if it ever reaches 0x800 the value is
// unconditionally appended and the loop exits. This is a defensive
// saturation guard since the highest defined family bit is VHF=0x400;
// passing a family value with bit 0x800 (or higher) set produces a
// vector ending in DeviceFamily(0x800) rather than runaway iteration.
//
// Note: family values 0 and 1 (Unknown / HF2) skip the loop entirely
// and return an empty vector — input < 2 is never decomposed.
// ============================================================================
std::vector<DeviceFamily> expand(DeviceFamily family) {
    std::vector<DeviceFamily> out;
    out.reserve(11);  // 0x2c bytes pre-allocated in the binary

    auto const f = static_cast<uint32_t>(family);
    if (f < 2) {
        return out;
    }

    for (uint32_t bit = 1; bit < f; bit <<= 1) {
        if (bit == 0x800u) {
            // Defensive saturation: see header comment above.
            out.push_back(static_cast<DeviceFamily>(0x800u));
            break;
        }
        if ((bit & f) != 0) {
            out.push_back(static_cast<DeviceFamily>(bit));
        }
    }
    return out;
}

// ============================================================================
// toStrings(std::vector<DeviceFamily> const&) — @ 0x2df760
//
// Returns a vector<string> with one entry per input DeviceFamily, using
// the same name mapping as toString(DeviceFamily). The compiled body
// inlines the toString switch (using libc++ SSO encoding directly into
// each push_back) rather than calling it; semantically equivalent.
// ============================================================================
std::vector<std::string>
toStrings(std::vector<DeviceFamily> const& families) {
    std::vector<std::string> out;
    out.reserve(families.size());
    for (DeviceFamily f : families) {
        out.push_back(toString(f));
    }
    return out;
}

// ============================================================================
// operator<<(ostream&, DeviceFamily) — @ 0x2dfa00
//
// Writes toString(family) to the stream. The compiled body inlines the
// switch with libc++ SSO encoding then calls __put_character_sequence
// directly (avoiding a temporary std::string heap allocation when the
// name fits in SSO — which all 11 names do). Semantically equivalent
// to `os << toString(family)`.
// ============================================================================
std::ostream& operator<<(std::ostream& os, DeviceFamily family) {
    return os << toString(family);
}

// ============================================================================
// toString(DeviceTypeCode) — @ 0x2d40b0
// ============================================================================
std::string toString(DeviceTypeCode code) {
    switch (code) {
        case DeviceTypeCode::Unknown: return std::string();
        case DeviceTypeCode::HF2:     return "HF2";
        case DeviceTypeCode::HF2LI:   return "HF2LI";
        case DeviceTypeCode::HF2IS:   return "HF2IS";
        case DeviceTypeCode::UHF:     return "UHF";
        case DeviceTypeCode::UHFLI:   return "UHFLI";
        case DeviceTypeCode::UHFAWG:  return "UHFAWG";
        case DeviceTypeCode::UHFQA:   return "UHFQA";
        case DeviceTypeCode::UHFIA:   return "UHFIA";
        case DeviceTypeCode::MF:      return "MF";
        case DeviceTypeCode::MFLI:    return "MFLI";
        case DeviceTypeCode::MFIA:    return "MFIA";
        case DeviceTypeCode::HDAWG:   return "HDAWG";
        case DeviceTypeCode::HDAWG4:  return "HDAWG4";
        case DeviceTypeCode::HDAWG8:  return "HDAWG8";
        case DeviceTypeCode::SHF:     return "SHF";
        case DeviceTypeCode::SHFQA2:  return "SHFQA2";
        case DeviceTypeCode::SHFQA4:  return "SHFQA4";
        case DeviceTypeCode::SHFSG2:  return "SHFSG2";
        case DeviceTypeCode::SHFSG4:  return "SHFSG4";
        case DeviceTypeCode::SHFSG8:  return "SHFSG8";
        case DeviceTypeCode::SHFQC:   return "SHFQC";
        case DeviceTypeCode::SHFLI:   return "SHFLI";
        case DeviceTypeCode::PQSC:    return "PQSC";
        case DeviceTypeCode::SHFACC:  return "SHFACC";
        case DeviceTypeCode::SHFPPC2: return "SHFPPC2";
        case DeviceTypeCode::SHFPPC4: return "SHFPPC4";
        case DeviceTypeCode::GHF:     return "GHF";
        case DeviceTypeCode::GHFLI:   return "GHFLI";
        case DeviceTypeCode::HWMOCK:  return "HWMOCK";
        case DeviceTypeCode::QHUB:    return "QHUB";
        case DeviceTypeCode::VHF:     return "VHF";
        case DeviceTypeCode::VHFLI:   return "VHFLI";
    }
    return "unknown";
}

// ============================================================================
// toString(DeviceOption, DeviceFamily) — @ 0x2cfee0
//
// 31-entry switch decoded from the jump table at .rodata 0x961bac. Codes
// 0 ("MF"/"MFK") and 6 ("RT"/"RTK") have a family-dependent name: HF2
// family yields the "K"-suffixed variant. All other codes map to a single
// fixed string. Out-of-range values (>= 31) return an empty string.
// ============================================================================
std::string toString(DeviceOption opt, DeviceFamily family) {
    auto v = static_cast<uint32_t>(opt);
    switch (v) {
        case 0:  return (family == DeviceFamily::HF2) ? "MFK" : "MF";
        case 1:  return "MD";
        case 2:  return "FF";
        case 3:  return "PLL";
        case 4:  return "PID";
        case 5:  return "MOD";
        case 6:  return (family == DeviceFamily::HF2) ? "RTK" : "RT";
        case 7:  return "UHS";
        case 8:  return "AWG";
        case 9:  return "DIG";
        case 10: return "10G";
        case 11: return "QE";
        case 12: return "F5M";
        case 13: return "RUB";
        case 14: return "BOX";
        case 15: return "IA";
        case 16: return "WEB";
        case 17: return "CNT";
        case 18: return "NOUI";
        case 19: return "ME";
        case 20: return "PC";
        case 21: return "QA";
        case 22: return "SKW";
        case 23: return "16W";
        case 24: return "QC2CH";
        case 25: return "QC4CH";
        case 26: return "QC6CH";
        case 27: return "RTR";
        case 28: return "PLUS";
        case 29: return "LRT";
        case 30: return "F200M";
        default: return std::string();  // out-of-range -> empty
    }
}

// ============================================================================
// toString(DeviceType const&) — @ 0x2d2e20
//
// Surprisingly minimal: just returns toString(dt.code()). Options are
// formatted separately by the (DeviceOptionSet, family, separator) overload
// below. Confirmed from the 5-instruction body at 0x2d2e20.
// ============================================================================
std::string toString(DeviceType const& dt) {
    return toString(dt.code());
}

// ============================================================================
// getOptionsAsString(DeviceType const&, std::string const& separator)
//   @ 0x2d2dd0
//
// Thin convenience wrapper. Disassembly is essentially:
//
//   r12 = DeviceTypeImpl::options()         // pointer to DeviceOptionSet
//   eax = DeviceTypeImpl::family()          // DeviceFamily enum
//   return toString(*r12, eax, separator)   // tail-call equivalent
//
// All three of dt.options(), dt.family() and the (set,family,sep) overload
// are already reconstructed.
// ============================================================================
std::string getOptionsAsString(DeviceType const& dt,
                               std::string const& separator) {
    return toString(dt.options(), dt.family(), separator);
}

// ============================================================================
// toString(DeviceOptionSet const&, DeviceFamily, std::string const&)
//   @ 0x2d0130
//
// Walks the set's name-keyed map (alphabetical iteration order), translates
// each DeviceOption to its name via toString(opt,family), then joins the
// resulting vector with `separator` using boost::algorithm::join.
//
// Fast path: if the set is empty, returns an empty string immediately
// (the disasm checks the unordered_set's size_ at +0x18).
// ============================================================================
std::string toString(DeviceOptionSet const& set,
                     DeviceFamily family,
                     std::string const& separator) {
    if (set.empty()) {
        return std::string();
    }
    std::vector<std::string> names;
    names.reserve(set.size());
    for (auto it = set.begin(); it != set.end(); ++it) {
        names.push_back(toString(*it, family));
    }
    return boost::algorithm::join(names, separator);
}

// ============================================================================
// operator<<(ostream, DeviceTypeCode) — @ 0x2d4350
// ============================================================================
std::ostream& operator<<(std::ostream& os, DeviceTypeCode code) {
    return os << toString(code);
}

// ============================================================================
// toDeviceTypeCode — @ 0x2d43d0
//
// Lazy-initialised function-local static unordered_map<string,
// DeviceTypeCode>. The original binary builds the map from 33 entries and
// also caches the map's end() iterator in a separate static
// (`deviceTypeCodesEnd`); we just call .find() each call.
//
// The map has one special entry: "none" -> Unknown(0). Any string not in
// the map yields DeviceTypeCode(33) ("unknown") — out of the named range.
//
// Decoded from the lazy-init body at 0x2d445d which constructs 33
// std::pair<string, DeviceTypeCode> entries on the stack and inserts each.
// ============================================================================
DeviceTypeCode toDeviceTypeCode(std::string const& s) {
    static std::unordered_map<std::string, DeviceTypeCode> const codes = []{
        std::unordered_map<std::string, DeviceTypeCode> m;
        m.reserve(33);
        m.emplace("none",     DeviceTypeCode::Unknown);
        m.emplace("HF2",      DeviceTypeCode::HF2);
        m.emplace("HF2LI",    DeviceTypeCode::HF2LI);
        m.emplace("HF2IS",    DeviceTypeCode::HF2IS);
        m.emplace("UHF",      DeviceTypeCode::UHF);
        m.emplace("UHFLI",    DeviceTypeCode::UHFLI);
        m.emplace("UHFAWG",   DeviceTypeCode::UHFAWG);
        m.emplace("UHFQA",    DeviceTypeCode::UHFQA);
        m.emplace("UHFIA",    DeviceTypeCode::UHFIA);
        m.emplace("MF",       DeviceTypeCode::MF);
        m.emplace("MFLI",     DeviceTypeCode::MFLI);
        m.emplace("MFIA",     DeviceTypeCode::MFIA);
        m.emplace("HDAWG",    DeviceTypeCode::HDAWG);
        m.emplace("HDAWG4",   DeviceTypeCode::HDAWG4);
        m.emplace("HDAWG8",   DeviceTypeCode::HDAWG8);
        m.emplace("SHF",      DeviceTypeCode::SHF);
        m.emplace("SHFQA2",   DeviceTypeCode::SHFQA2);
        m.emplace("SHFQA4",   DeviceTypeCode::SHFQA4);
        m.emplace("SHFSG2",   DeviceTypeCode::SHFSG2);
        m.emplace("SHFSG4",   DeviceTypeCode::SHFSG4);
        m.emplace("SHFSG8",   DeviceTypeCode::SHFSG8);
        m.emplace("SHFQC",    DeviceTypeCode::SHFQC);
        m.emplace("SHFLI",    DeviceTypeCode::SHFLI);
        m.emplace("PQSC",     DeviceTypeCode::PQSC);
        m.emplace("SHFACC",   DeviceTypeCode::SHFACC);
        m.emplace("SHFPPC2",  DeviceTypeCode::SHFPPC2);
        m.emplace("SHFPPC4",  DeviceTypeCode::SHFPPC4);
        m.emplace("GHF",      DeviceTypeCode::GHF);
        m.emplace("GHFLI",    DeviceTypeCode::GHFLI);
        m.emplace("HWMOCK",   DeviceTypeCode::HWMOCK);
        m.emplace("QHUB",     DeviceTypeCode::QHUB);
        m.emplace("VHF",      DeviceTypeCode::VHF);
        m.emplace("VHFLI",    DeviceTypeCode::VHFLI);
        return m;
    }();
    auto it = codes.find(s);
    if (it != codes.end()) {
        return it->second;
    }
    // Not found: return out-of-range "unknown" sentinel (33).
    return static_cast<DeviceTypeCode>(33);
}

// ============================================================================
// Predicate free functions
// ============================================================================

// @ 0x2d2e50
bool isDefined(DeviceType const& dt) {
    return dt.code() != DeviceTypeCode::Unknown;
}

// @ 0x2d2e70
//
// Decoded body:
//   code = dt.code();
//   if (code >= 31)            goto fallback;
//   if (!(0x46BF7901 >> code & 1)) goto fallback;
//   return (0x900 >> code) & 1;        // true iff code in {8, 11}
// fallback:
//   return dt.hasOption(DeviceOption::IA);
//
// Broad mask 0x46BF7901 covers the codes for which the in-bitfield result
// is meaningful; codes outside the broad mask (and codes >= 31) defer
// entirely to the IA-option check. The "unconditional" mask 0x900 marks
// the two codes (UHFIA=8, MFIA=11) that are always considered IA.
bool isIa(DeviceType const& dt) {
    auto codeVal = static_cast<uint32_t>(dt.code());
    if (codeVal >= 31) {
        return dt.hasOption(DeviceOption::IA);
    }
    constexpr uint32_t broad_mask         = 0x46BF7901u;
    constexpr uint32_t unconditional_mask = 0x00000900u;
    if (((broad_mask >> codeVal) & 1u) == 0u) {
        return dt.hasOption(DeviceOption::IA);
    }
    return ((unconditional_mask >> codeVal) & 1u) != 0u;
}

// @ 0x2d2ec0
bool isMfia(DeviceType const& dt) {
    return dt.code() == DeviceTypeCode::MFIA;
}

// @ 0x2d2ee0
bool isUhfqa(DeviceType const& dt) {
    return dt.code() == DeviceTypeCode::UHFQA;
}

// @ 0x2d2f00
bool isShfqa(DeviceType const& dt) {
    DeviceTypeCode c = dt.code();
    return c == DeviceTypeCode::SHFQA2 ||
           c == DeviceTypeCode::SHFQA4 ||
           c == DeviceTypeCode::SHFQC;
}

// @ 0x2d2f40
bool isShfsg(DeviceType const& dt) {
    DeviceTypeCode c = dt.code();
    return c == DeviceTypeCode::SHFSG2 ||
           c == DeviceTypeCode::SHFSG4 ||
           c == DeviceTypeCode::SHFSG8;
}

// @ 0x2d2f80
bool isShfqc(DeviceType const& dt) {
    return dt.code() == DeviceTypeCode::SHFQC;
}

// @ 0x2d2fa0
bool isShfppc(DeviceType const& dt) {
    DeviceTypeCode c = dt.code();
    return c == DeviceTypeCode::SHFPPC2 || c == DeviceTypeCode::SHFPPC4;
}

// @ 0x2d2fd0
bool isShfli(DeviceType const& dt) {
    return dt.code() == DeviceTypeCode::SHFLI;
}

// @ 0x2d2ff0
bool isGhfli(DeviceType const& dt) {
    return dt.code() == DeviceTypeCode::GHFLI;
}

// @ 0x2d3010
bool isVhfli(DeviceType const& dt) {
    return dt.code() == DeviceTypeCode::VHFLI;
}

// @ 0x2d3030 — body decoded:
//   esi = (family == DeviceFamily::MF) ? 1 : 0;       // i.e. DeviceOption(1)=MD : DeviceOption(0)=MF
//   tail-call hasOption(esi);
//
// So: if family is MF, return hasOption(MD); otherwise return hasOption(MF).
// Looks counter-intuitive but matches the binary exactly; the option codes
// "MF" (0) and "MD" (1) play the role of "this is an MF-flavoured unit"
// in their respective family contexts.
bool hasMf(DeviceType const& dt) {
    DeviceOption probe = (dt.family() == DeviceFamily::MF)
                             ? DeviceOption::MD
                             : DeviceOption::MF;
    return dt.hasOption(probe);
}

// ============================================================================
// parsing helpers
// ============================================================================

// @ 0x2d0460
//
// Trims leading/trailing whitespace from `s` (using
// boost::algorithm::trim_copy_if(ctype::space)), then splits on '\n'
// (token_compress_off). Returns an empty vector for an all-whitespace
// input.
//
// The compiled body sets up a one-character `'\n'` string buffer on
// the stack and passes `boost::algorithm::is_any_of<char>` as the
// predicate. The end result is identical to splitting on '\n' below.
std::vector<std::string> splitDeviceOptions(std::string const& s) {
    std::vector<std::string> out;
    std::string trimmed =
        boost::algorithm::trim_copy_if(s, boost::algorithm::is_space());
    if (trimmed.empty()) {
        return out;
    }
    boost::algorithm::split(out, trimmed, boost::algorithm::is_any_of("\n"),
                            boost::algorithm::token_compress_off);
    return out;
}

// @ 0x2d05b0
//
// Reverse-lookup of a DeviceOption from its name string. The binary
// builds a function-local-static unordered_map<string, DeviceOption>
// of exactly 33 entries (the `mov edx, 0x21` count at the bucket-list
// allocation confirms this) on first call, then throws
// `zhinst::Exception` (via boost::throw_exception with a
// boost::source_location) on miss. toDeviceOptions() above catches
// std::exception and silently drops unknown names.
//
// The 33 source-level entries come from anonymous-namespace
// `const char*` constants `zhinst::(anonymous namespace)::DeviceOptionName::*`
// at .data.rel.ro 0xb08ef8..0xb08ff8 (8 bytes apart confirms pointer,
// not std::string), each pointing to a literal in .rodata starting at
// 0x90b60e. The resolved (string, DeviceOption) pairs are reproduced
// verbatim below.
//
// Naming asymmetry worth noting: the C++ symbol `DeviceOptionName::qc`
// holds the literal string `"QC"` and parses to DeviceOption code 21,
// but `toString(DeviceOption, family)` for code 21 returns `"QA"` —
// so the parser accepts "QC" as input while the formatter emits "QA".
// This is a real binary quirk, not a bug in the reconstruction.
//
// Two duplicate keys map code 0 ("MF" and "MFK") and code 6 ("RT" and
// "RTK"); the "K"-suffixed forms are the HF2 family's canonical
// stringification.
//
// At source level we use a simple BOOST_THROW_EXCEPTION on miss with
// a formatted message; the binary's source-location apparatus is a
// boost::throw_exception side effect not visible from the C++ source.
DeviceOption toDeviceOption(std::string const& s) {
    static std::unordered_map<std::string, DeviceOption> const table = {
        {"MF",    DeviceOption::MF},        // DeviceOptionName::mf
        {"MFK",   DeviceOption::MF},        // DeviceOptionName::mfHf2
        {"MD",    DeviceOption::MD},        // DeviceOptionName::md
        {"FF",    DeviceOption::FF},        // DeviceOptionName::ff
        {"PLL",   DeviceOption::PLL},       // DeviceOptionName::pll
        {"PID",   DeviceOption::PID},       // DeviceOptionName::pid
        {"MOD",   DeviceOption::MOD},       // DeviceOptionName::mod
        {"RT",    DeviceOption::RT},        // DeviceOptionName::rt
        {"RTK",   DeviceOption::RT},        // DeviceOptionName::rtHf2
        {"UHS",   DeviceOption::UHS},       // DeviceOptionName::uhs
        {"AWG",   DeviceOption::AWG},       // DeviceOptionName::awg
        {"DIG",   DeviceOption::DIG},       // DeviceOptionName::dig
        {"10G",   DeviceOption::Option10G},      // DeviceOptionName::e10g
        {"QE",    DeviceOption::QE},        // DeviceOptionName::qe
        {"F5M",   DeviceOption::F5M},       // DeviceOptionName::f5m
        {"RUB",   DeviceOption::RUB},       // DeviceOptionName::rub
        {"BOX",   DeviceOption::BOX},       // DeviceOptionName::box
        {"IA",    DeviceOption::IA},        // DeviceOptionName::ia
        {"WEB",   DeviceOption::WEB},       // DeviceOptionName::web
        {"CNT",   DeviceOption::CNT},       // DeviceOptionName::cnt
        {"NOUI",  DeviceOption::NOUI},      // DeviceOptionName::noui
        {"ME",    DeviceOption::ME},        // DeviceOptionName::me
        {"PC",    DeviceOption::PC},        // DeviceOptionName::pc
        {"QC",    DeviceOption::QA},        // DeviceOptionName::qc — see asymmetry note above
        {"SKW",   DeviceOption::SKW},       // DeviceOptionName::skw
        {"16W",   DeviceOption::Option16W}, // DeviceOptionName::w16
        {"QC2CH", DeviceOption::QC2CH},     // DeviceOptionName::qc2ch
        {"QC4CH", DeviceOption::QC4CH},     // DeviceOptionName::qc4ch
        {"QC6CH", DeviceOption::QC6CH},     // DeviceOptionName::qc6ch
        {"RTR",   DeviceOption::RTR},       // DeviceOptionName::rtr
        {"PLUS",  DeviceOption::PLUS},      // DeviceOptionName::plus
        {"LRT",   DeviceOption::LRT},       // DeviceOptionName::lrt
        {"F200M", DeviceOption::F200M},     // DeviceOptionName::f200m
    };
    auto it = table.find(s);
    if (it == table.end()) {
        BOOST_THROW_EXCEPTION(Exception(
            "toDeviceOption: '" + s + "' is not a valid DeviceOption name"));
    }
    return it->second;
}

// @ 0x2d0fb0
//
// Builds a DeviceOptionSet by translating each option-name string to
// a DeviceOption via toDeviceOption() and inserting it. Unknown names
// throw zhinst::Exception out of toDeviceOption() (via
// boost::throw_exception); the inner try/catch swallows them so that
// an unrecognized option simply doesn't appear in the result.
//
// The compiled body inlines DeviceOptionSet's ctor (writing the
// 1.0f load-factor sentinel and a self-pointer for the empty map's
// header) before entering the loop. The exception filter at the
// landing pad checks selector value 1, which corresponds to
// `catch (zhinst::Exception const&)` in the gcc_except_table.
DeviceOptionSet toDeviceOptions(std::vector<std::string> const& opts,
                                DeviceFamily family) {
    DeviceOptionSet result(family);
    for (auto const& name : opts) {
        try {
            result.insert(toDeviceOption(name));
        } catch (Exception const&) {
            // Silently drop unknown option names. Mirrors the binary's
            // __cxa_begin_catch / __cxa_end_catch pair around the
            // insert call site, which filters on zhinst::Exception
            // (the type thrown by toDeviceOption).
        }
    }
    return result;
}

// @ 0x2debd0
//
// Fuzzy device-family lookup with two layers (verified by static
// disassembly + 87-input ctypes probe; see IF-267):
//
//   1. A length-keyed jump-table at .rodata 0x961d88 (8 entries,
//      indexed by `min(len, 7)`; len > 7 bypasses the table and goes
//      straight to layer 2):
//        - len 0           -> return DeviceFamily(0) (Unknown=0 sentinel)
//        - len 1, 2, 3, 5  -> straight to layer 2
//        - len 4           -> if s == "none" return 0; else layer 2
//        - len 6           -> if s == "SHFACC" return SHFACC; else layer 2
//        - len 7           -> if s == "DEFAULT" return HF2;
//                             if s == "SHFPPC2" or "SHFPPC4" return SHFACC;
//                             else layer 2
//      The literal compares are emitted as packed-int xor (e.g.
//      0x656e6f6e = "none", 0x41464853 ^ 0x4343 = "SHFACC",
//      0x41464544 ^ 0x544c5541 = "DEFAULT", 0x50464853 ^ 0x32435050
//      = "SHFPPC2").
//
//   2. Slow path: a function-local-static
//      std::map<std::string, DeviceFamily> populated lazily under
//      __cxa_guard with **exactly 10 entries**, all UPPERCASE keys:
//        {"HF2",1}, {"UHF",2}, {"MF",4}, {"HDAWG",8}, {"SHF",0x10},
//        {"PQSC",0x20}, {"HWMOCK",0x40}, {"GHF",0x100}, {"QHUB",0x200},
//        {"VHF",0x400}.
//      Note SHFACC (0x80) is NOT in the map — it is reachable only via
//      the layer-1 fastpaths.
//      Lookup uses `upper_bound(s)` then steps back to the largest key
//      <= s, and verifies via `boost::algorithm::starts_with(s, key)`.
//      On miss returns DeviceFamily(0x800) (rendered as "unknown").
//
// Lookups are case-sensitive; lower-case device-type strings do not
// match.
DeviceFamily toDeviceFamily(std::string const& s) {
    auto const n = s.size();

    // jump-table[0]: empty input returns the Unknown=0 sentinel
    // (distinct from the 0x800 miss sentinel).
    if (n == 0) {
        return static_cast<DeviceFamily>(0u);
    }

    // Length-keyed literal fastpaths (len 1..7).  len > 7 bypasses
    // these and goes straight to the map lookup.
    if (n <= 7) {
        if (n == 4 && s == "none") {
            return static_cast<DeviceFamily>(0u);
        }
        if (n == 6 && s == "SHFACC") {
            return DeviceFamily::SHFACC;
        }
        if (n == 7) {
            if (s == "DEFAULT") {
                return DeviceFamily::HF2;
            }
            if (s == "SHFPPC2" || s == "SHFPPC4") {
                return DeviceFamily::SHFACC;
            }
        }
        // Fall through to map lookup for all other 1..7-length inputs.
    }

    // Lazy-init prefix table — 10 uppercase canonical family names.
    // std::map orders alphabetically; the upper_bound + step-back
    // idiom locates the largest key <= s, which `starts_with` then
    // verifies as an actual prefix of s.
    static std::map<std::string, DeviceFamily> const familyNames = {
        {"GHF",    DeviceFamily::GHF},
        {"HDAWG",  DeviceFamily::HDAWG},
        {"HF2",    DeviceFamily::HF2},
        {"HWMOCK", DeviceFamily::HWMOCK},
        {"MF",     DeviceFamily::MF},
        {"PQSC",   DeviceFamily::PQSC},
        {"QHUB",   DeviceFamily::QHUB},
        {"SHF",    DeviceFamily::SHF},
        {"UHF",    DeviceFamily::UHF},
        {"VHF",    DeviceFamily::VHF},
    };

    auto it = familyNames.upper_bound(s);
    if (it == familyNames.begin()) {
        return static_cast<DeviceFamily>(0x800u);  // miss sentinel
    }
    --it;
    if (boost::algorithm::starts_with(s, it->first)) {
        return it->second;
    }
    return static_cast<DeviceFamily>(0x800u);  // miss sentinel
}

// ============================================================================
// allDevices() — @ 0x2d4c30
//
// Lazily-initialized function-local static returning a
// boost::container::flat_set<DeviceTypeCode> with all 33 named codes
// (DeviceTypeCode values 0..32).
//
// The compiled body delegates to an anonymous-namespace helper
// `makeDevicesSet()` @ 0x2d4cb0 which:
//   1. Bulk-inserts the 33 entries via boost::transform_iterator with
//      a `[](int i) -> DeviceTypeCode { return DeviceTypeCode(i); }`
//      lambda over `integer_iterator<int>` from 0..33.
//   2. Sorts via boost::movelib::pdqsort (already sorted; cheap).
//   3. Calls inplace_set_unique_difference + adaptive_merge to dedupe
//      (also a no-op here; this is just the flat_set bulk-insert idiom).
//
// All of that machinery degenerates to "insert 0..32 into a sorted
// vector". The reconstruction below uses the equivalent source-level
// init.
// ============================================================================
namespace {

boost::container::flat_set<DeviceTypeCode> makeDevicesSet() {
    boost::container::flat_set<DeviceTypeCode> s;
    s.reserve(33);
    // The anonymous-namespace lambda makeDevicesSet()::$_0 is just a
    // static_cast<DeviceTypeCode>(int).
    for (int i = 0; i < 33; ++i) {
        s.insert(static_cast<DeviceTypeCode>(i));
    }
    return s;
}

}  // namespace

boost::container::flat_set<DeviceTypeCode> const& allDevices() {
    static boost::container::flat_set<DeviceTypeCode> const allDevicesSet =
        makeDevicesSet();
    return allDevicesSet;
}

}  // namespace zhinst
