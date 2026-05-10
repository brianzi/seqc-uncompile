// ============================================================================
// Reconstructed from disassembly of _seqc_compiler.so
// DeviceType / DeviceFamily / DeviceTypeCode / DeviceOption / DeviceOptionSet
//
// enums, DeviceType pimpl wrapper, detail::DeviceTypeImpl base,
// DeviceOptionSet (dual-storage container) and its iterator.
//
// full DeviceOption enum, sfc::*Option per-family bitmask
// enums, OptionCodePair / initializeSfcOptions template registry, real
// toString(DeviceOption,family) and toString(DeviceOptionSet,family,sep)
// implementations.
//
// Per-family `detail::*` device subclasses and their `initializeXxxOptions`
// helpers are deferred to 
//
// Sources of evidence are noted in reconstructed/notes/device_type.md.
// ============================================================================
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <iosfwd>
#include <map>
#include <string>
#include <tuple>
#include <unordered_set>
#include <vector>

#include <boost/container/flat_set.hpp>

namespace zhinst {

// ============================================================================
// enum class DeviceFamily : uint32_t  (one-hot bitfield)
//
// Decoded from toString(DeviceFamily) jump table at .rodata 0x961eac.
// ============================================================================
enum class DeviceFamily : uint32_t {
    Unknown = 0,
    HF2     = 0x001,
    UHF     = 0x002,
    MF      = 0x004,
    HDAWG   = 0x008,
    SHF     = 0x010,
    PQSC    = 0x020,
    HWMOCK  = 0x040,
    SHFACC  = 0x080,
    GHF     = 0x100,
    QHUB    = 0x200,
    VHF     = 0x400,
};

// ============================================================================
// enum class DeviceTypeCode : uint32_t  (plain 0..32)
//
// Decoded from toString(DeviceTypeCode) jump table at .rodata 0x961cc4.
// 33 named values; toDeviceTypeCode() returns 33 ("unknown") on miss.
// ============================================================================
enum class DeviceTypeCode : uint32_t {
    Unknown = 0,
    HF2     = 1,
    HF2LI   = 2,
    HF2IS   = 3,
    UHF     = 4,
    UHFLI   = 5,
    UHFAWG  = 6,
    UHFQA   = 7,
    UHFIA   = 8,
    MF      = 9,
    MFLI    = 10,
    MFIA    = 11,
    HDAWG   = 12,
    HDAWG4  = 13,
    HDAWG8  = 14,
    SHF     = 15,
    SHFQA2  = 16,
    SHFQA4  = 17,
    SHFSG2  = 18,
    SHFSG4  = 19,
    SHFSG8  = 20,
    SHFQC   = 21,
    SHFLI   = 22,
    PQSC    = 23,
    SHFACC  = 24,
    SHFPPC2 = 25,
    SHFPPC4 = 26,
    GHF     = 27,
    GHFLI   = 28,
    HWMOCK  = 29,
    QHUB    = 30,
    VHF     = 31,
    VHFLI   = 32,
};

// ============================================================================
// enum class DeviceOption : uint32_t
//
// 31 values (0..30). Value 0 is named "MF" canonically, but
// toString(DeviceOption, DeviceFamily) returns "MFK" for the HF2 family.
// Likewise value 6 is "RT" canonically, "RTK" for HF2.
//
// Decoded from the 31-entry jump table at .rodata 0x961bac in
// toString(DeviceOption, DeviceFamily) at 0x2cfee0. Family-dependent
// strings live at .rodata 0x90b60e ("MFK\0MF\0...\0RTK\0RT").
//
// Note: None=0 collides with MF=0 (the binary uses code 0 for both
// "no option" and the MF/MFK marker option). Inserted in the
// per-family `initializeXxxOptions` arrays at .rodata 0x962394+.
// ============================================================================
enum class DeviceOption : uint32_t {
    None  = 0,    // sentinel; same value as MF (binary uses 0 dually)
    MF    = 0,    // toString -> "MF" (or "MFK" for DeviceFamily::HF2)
    MD    = 1,
    FF    = 2,
    PLL   = 3,
    PID   = 4,
    MOD   = 5,
    RT    = 6,    // toString -> "RT" (or "RTK" for DeviceFamily::HF2)
    UHS   = 7,
    AWG   = 8,
    DIG   = 9,
    Option10G  = 10,   // toString -> "10G"
    QE    = 11,
    F5M   = 12,
    RUB   = 13,
    BOX   = 14,
    IA    = 15,   // confirmed via isIa @ 0x2d2e70
    WEB   = 16,
    CNT   = 17,
    NOUI  = 18,
    ME    = 19,
    PC    = 20,
    QA    = 21,
    SKW   = 22,
    Option16W = 23, // toString -> "16W"
    QC2CH = 24,
    QC4CH = 25,
    QC6CH = 26,
    RTR   = 27,
    PLUS  = 28,
    LRT   = 29,
    F200M = 30,
};

// ============================================================================
// sfc::*Option per-family one-hot bitmask enums
//
// These carry the device-options bitmask passed at construction (e.g.
// `Hf2li(unsigned long options)`) before being mapped through the
// `initializeXxxOptions::knownOptions` arrays into DeviceOption values.
//
// Bit values were decoded from the (mask, code) pairs in the 15
// knownOptions arrays at .rodata 0x962394..0x962aa0. See
// notes/device_type.md for the full per-family table.
//
// All bits below appear in the binary's known-options arrays for at least
// one family member; the underlying user-facing options bitmask may carry
// additional unmapped bits (the loop simply ignores any bit not present
// in the array).
// ============================================================================
namespace sfc {

// ----------------------------------------------------------------------------
// FeaturesCode — strong-typed wrapper around a uint64 SFC bitfield.
//
// Origin: the `BOOST_THROW_EXCEPTION` source_location literal embedded in
// `detail::generateMfSfc` (binary @ 0x2deb37) records the function as
// `sfc::FeaturesCode zhinst::detail::generateMfSfc(...)`. Free-function
// return types are NOT encoded in the Itanium mangling, so this is the
// only direct evidence of the type name. The disassembly at 0x2deac1
// shows the value is returned plain in `rax` (not via sret), which fixes
// the size at <= 8 bytes and rules out non-trivial copy/dtor. A naked
// uint64 wrapper is the only layout consistent with both observations.
//
// Wrapping in a struct (rather than `using FeaturesCode = uint64_t`)
// preserves the type identity that the binary's mangled callsites would
// have used — important for downstream code that may have overloads
// keyed on `FeaturesCode` vs raw integer types. The wrapper is trivially
// copyable, trivially destructible, and standard-layout, so calling
// convention matches the observed `rax`-return.
//! \brief Strong-typed 64-bit wrapper for an MFLI/MFIA Smart Feature
//! Code (SFC) bitfield.
//!
//! Returned by `detail::generateMfSfc` and threaded through the SFC
//! subsystem.  Wrapping the integer keeps the type distinct from raw
//! `uint64_t` device-option bitmasks at call sites that may overload
//! on both.
struct FeaturesCode {
    std::uint64_t value;

    constexpr FeaturesCode() noexcept : value(0) {}
    constexpr explicit FeaturesCode(std::uint64_t v) noexcept : value(v) {}

    constexpr explicit operator std::uint64_t() const noexcept { return value; }

    friend constexpr bool operator==(FeaturesCode a, FeaturesCode b) noexcept {
        return a.value == b.value;
    }
    friend constexpr bool operator!=(FeaturesCode a, FeaturesCode b) noexcept {
        return a.value != b.value;
    }
};

enum class Hf2Option : uint32_t {
    None = 0,
    MF  = 0x0001,  // -> code 0 (MF/MFK)
    PLL = 0x0002,  // -> code 3 (PLL)
    MOD = 0x0004,  // -> code 5 (MOD)
    RT  = 0x0008,  // -> code 6 (RT/RTK)
    UHS = 0x0010,  // -> code 7 (UHS)
    PID = 0x0020,  // -> code 4 (PID)
    WEB = 0x1000,  // -> code 16 (WEB)
};

enum class MfOption : uint32_t {
    None  = 0,
    MD    = 0x00001,  // -> code 1  (MD)
    PID   = 0x00002,  // -> code 4  (PID)
    MOD   = 0x00004,  // -> code 5  (MOD)
    FF    = 0x00020,  // -> code 2  (FF)
    DIG   = 0x00400,  // -> code 9  (DIG)
    F5M   = 0x00800,  // -> code 12 (F5M)
    BOX   = 0x04000,  // -> code 14 (BOX)
    IA    = 0x08000,  // -> code 15 (IA)  [Mfli only]
    NOUI  = 0x20000,  // -> code 18 (NOUI)
};

enum class UhfOption : uint32_t {
    None       = 0,
    MF         = 0x00001,  // -> code 0  (MF)
    PID        = 0x00002,  // -> code 4  (PID)
    MOD        = 0x00004,  // -> code 5  (MOD)
    QA         = 0x00008,  // -> code 21 (QA)
    FF         = 0x00020,  // -> code 2  (FF)
    AWG        = 0x00200,  // -> code 8  (AWG)  [Uhfli/Uhfia]
    DIG        = 0x00400,  // -> code 9  (DIG)
    Option10G  = 0x00800,  // -> code 10 (10G)  [Uhfia only]
    QE         = 0x01000,  // -> code 11 (QE)   [Uhfqa/Uhfia]
    RUB        = 0x02000,  // -> code 13 (RUB)
    BOX        = 0x04000,  // -> code 14 (BOX)
    CNT        = 0x10000,  // -> code 17 (CNT)
};

enum class HdawgOption : uint32_t {
    None = 0,
    MF   = 0x00001,  // -> code 0  (MF)
    FF   = 0x00020,  // -> code 2  (FF)
    ME   = 0x00200,  // -> code 19 (ME)
    SKW  = 0x00800,  // -> code 22 (SKW)
    PC   = 0x08000,  // -> code 20 (PC)
    CNT  = 0x10000,  // -> code 17 (CNT)
};

enum class ShfOption : uint32_t {
    None       = 0,
    MF         = 0x00001,  // -> code 0  (MF)   [Shfli only]
    PID        = 0x00002,  // -> code 4  (PID)  [Shfli only]
    MOD        = 0x00004,  // -> code 5  (MOD)  [Shfli only]
    QC2CH      = 0x00008,  // -> code 24 (QC2CH)
    QC4CH      = 0x00010,  // -> code 25 (QC4CH)
    FF         = 0x00020,  // -> code 2  (FF)
    AWG        = 0x00200,  // -> code 8  (AWG)  [Shfli only]
    QC6CH      = 0x00800,  // -> code 26 (QC6CH)
    Option16W  = 0x01000,  // -> code 23 (16W)
    RTR        = 0x02000,  // -> code 27 (RTR)
    PLUS       = 0x04000,  // -> code 28 (PLUS)
    LRT        = 0x08000,  // -> code 29 (LRT)
};

enum class VhfOption : uint32_t {
    None  = 0,
    MD    = 0x0001,  // -> code 1  (MD)
    PID   = 0x0002,  // -> code 4  (PID)
    MOD   = 0x0004,  // -> code 5  (MOD)
    FF    = 0x0020,  // -> code 2  (FF)
    AWG   = 0x0200,  // -> code 8  (AWG)
    F200M = 0x0800,  // -> code 30 (F200M)
};

// Note: there is NO `sfc::GhfOption` in the binary. `detail::Ghfli`'s
// constructor calls `initializeSfcOptions<sfc::ShfOption, 5>` directly
// (mangled symbol confirmed in disasm of 0x2e3a00 ctor, knownOptions
// array @ .rodata 0x96298c). The GHF family reuses `ShfOption`. See
// reconstructed/notes/device_type.md.

}  // namespace sfc

// Forward decls
class DeviceType;
class DeviceOptionSet;

namespace detail {
class DeviceTypeImpl;
}  // namespace detail

// ============================================================================
// DeviceOptionSetConstIterator — 8 bytes
//
// Wraps a std::map<std::string, DeviceOption>::const_iterator (which is in
// turn a wrapper around a __tree_node<__value_type<string,DeviceOption>>*).
// Iteration order is alphabetical by option name (toString(opt, family)).
//
// Methods @ 0x2cf900..0x2cf960. dereference() reads node[+0x38] which is
// the DeviceOption field of the value pair.
// ============================================================================
//! \brief Forward iterator over a `DeviceOptionSet` that yields
//! `DeviceOption` values in alphabetical order of their family-scoped
//! string names.
//!
//! Wraps the const_iterator of the set's `byName_` ordered-map storage,
//! exposing only the option value (not the name).  Returned by
//! `DeviceOptionSet::begin()` / `end()`.
class DeviceOptionSetConstIterator {
public:
    using map_type = std::map<std::string, DeviceOption>;
    using underlying_iterator = map_type::const_iterator;

    explicit DeviceOptionSetConstIterator(underlying_iterator it);

    DeviceOption dereference() const;
    void increment();
    bool equal(DeviceOptionSetConstIterator const& other) const;

    DeviceOption operator*() const { return dereference(); }
    DeviceOptionSetConstIterator& operator++() { increment(); return *this; }
    DeviceOptionSetConstIterator operator++(int) {
        DeviceOptionSetConstIterator tmp(*this);
        increment();
        return tmp;
    }
    bool operator==(DeviceOptionSetConstIterator const& other) const {
        return equal(other);
    }
    bool operator!=(DeviceOptionSetConstIterator const& other) const {
        return !equal(other);
    }

private:
    underlying_iterator iter_;
};

// ============================================================================
// DeviceOptionSet — 72 bytes (0x48), DUAL STORAGE
//
// Offset  Size  Type                               Name      Notes
// +0x00   40    unordered_set<DeviceOption>        values_   fast lookup
// +0x28   24    map<string, DeviceOption>          byName_   iteration order
// +0x40   4     DeviceFamily                       family_
// +0x44   4     (padding)
// sizeof(DeviceOptionSet) = 0x48
// ============================================================================
//! \brief Set of `DeviceOption` values associated with a specific
//! `DeviceFamily`, stored with dual indexing for fast membership and
//! ordered iteration.
//!
//! Members are kept in two parallel containers: an
//! `unordered_set<DeviceOption>` for O(1) `contains()` and an
//! ordered `map<string, DeviceOption>` keyed by each option's
//! family-scoped string name (via `toString(opt, family)`) for
//! deterministic alphabetical iteration.  Both are updated together
//! by `insert`.  The `family_` field selects which name an option
//! resolves to for options whose string differs across families
//! (e.g. `MF` vs `MFK` on HF2).  Used wherever the compiler needs
//! to test for or enumerate the user-supplied options on a
//! `DeviceType`.
class DeviceOptionSet {
public:
    using const_iterator = DeviceOptionSetConstIterator;

    explicit DeviceOptionSet(DeviceFamily family);                       // @ 0x2cf970
    DeviceOptionSet(std::initializer_list<DeviceOption> const& options,
                    DeviceFamily family);                                // @ 0x2cf9a0

    DeviceOptionSet(DeviceOptionSet const&) = default;
    DeviceOptionSet(DeviceOptionSet&&) noexcept = default;
    DeviceOptionSet& operator=(DeviceOptionSet const&) = default;
    DeviceOptionSet& operator=(DeviceOptionSet&&) noexcept = default;
    ~DeviceOptionSet() = default;

    const_iterator begin() const;       // @ 0x2cfb60
    const_iterator end() const;         // @ 0x2cfb70

    bool contains(DeviceOption opt) const;  // @ 0x2cfb80
    bool empty() const;                     // @ 0x2cfcc0
    std::size_t size() const;               // @ 0x2cfcd0
    DeviceFamily family() const;            // @ 0x2cfce0

    void insert(DeviceOption opt);          // @ 0x2cfcf0

    friend bool operator==(DeviceOptionSet const& a, DeviceOptionSet const& b);
    friend bool operator!=(DeviceOptionSet const& a, DeviceOptionSet const& b) {
        return !(a == b);
    }

private:
    std::unordered_set<DeviceOption> values_;       // +0x00, 40 bytes
    std::map<std::string, DeviceOption> byName_;    // +0x28, 24 bytes
    DeviceFamily family_;                           // +0x40
};

// ============================================================================
// detail::DeviceTypeImpl — polymorphic base class
//
// Offset  Size  Type            Name       Notes
// +0x00   8     vptr                       (virtual: ~dtor, clone)
// +0x08   4     DeviceTypeCode  code_
// +0x0C   4     DeviceFamily    family_
// +0x10   72    DeviceOptionSet options_
// sizeof(DeviceTypeImpl) = 0x58
// ============================================================================
namespace detail {

//! \brief Polymorphic base for all concrete device-type identities.
//!
//! Carries the `DeviceTypeCode` (specific model), the `DeviceFamily`
//! (broader product family), and the `DeviceOptionSet` (installed
//! hardware/software options) that together describe one connected
//! instrument.  Subclasses (one per family/model in
//! `device_subclasses.hpp`, plus `GenericDeviceType` for runtime-string
//! construction) only override the vtable to preserve their identity
//! through `clone()`; they add no data members.  Instances are owned
//! by `DeviceType` via raw pimpl pointer and reproduced by
//! `clone()` for copy semantics.
class DeviceTypeImpl {
public:
    DeviceTypeImpl();                                              // @ 0x2d3060
    DeviceTypeImpl(DeviceTypeCode code, DeviceFamily family);      // @ 0x2d3090
    DeviceTypeImpl(DeviceTypeCode code, DeviceFamily family,
                   DeviceOptionSet options);                       // @ 0x2d30b0
    // Tuple-taking ctor used by the GenericDeviceType ctor (and by it
    // alone, as far as the binary shows). Unpacks the std::tuple by
    // moving its DeviceOptionSet into options_. The first 8 bytes of
    // the tuple hold {DeviceTypeCode, DeviceFamily} and are copied
    // bitwise into [code_, family_].
    explicit DeviceTypeImpl(
        std::tuple<DeviceTypeCode, DeviceFamily, DeviceOptionSet> args);  // @ 0x2d3190

    virtual ~DeviceTypeImpl();
    virtual DeviceTypeImpl* clone() const;  // vtable[0]; impl @ 0x2d3280
                                            // (doClone in mangled name).

    DeviceFamily   family() const;          // @ 0x2d32e0
    DeviceTypeCode code() const;            // @ 0x2d32f0
    bool           hasOption(DeviceOption opt) const;  // @ 0x2d3300
    DeviceOptionSet const& options() const; // @ 0x2d3310

protected:
    DeviceTypeCode  code_;     // +0x08
    DeviceFamily    family_;   // +0x0c
    DeviceOptionSet options_;  // +0x10
};

// ----------------------------------------------------------------------------
// detail::OptionCodePair<T> — 8 bytes (uint32 mask + uint32 DeviceOption code)
//
// Each entry of an `initializeXxxOptions::knownOptions` array. T is the
// per-family one-hot bitmask enum (sfc::Hf2Option / MfOption / ...).
// ----------------------------------------------------------------------------
template <class T>
struct OptionCodePair {
    T            mask;
    DeviceOption code;
};

// ----------------------------------------------------------------------------
// detail::initializeSfcOptions<T,N> — header-only template helper.
//
// Body confirmed at 0x2e0d50 (instantiation for <Hf2Option,6>):
//   set = DeviceOptionSet(family);
//   for (i = 0; i < N; ++i)
//       if ((options & arr[i].mask) != 0) set.insert(arr[i].code);
//   return set;
//
// All 15 per-family `initializeXxxOptions(unsigned long)` helpers in the
// binary inline this same loop with their own `knownOptions` array.
// ----------------------------------------------------------------------------
template <class T, std::size_t N>
DeviceOptionSet initializeSfcOptions(
    std::array<OptionCodePair<T>, N> const& known,
    DeviceFamily family,
    unsigned long options) {
    DeviceOptionSet set(family);
    for (std::size_t i = 0; i < N; ++i) {
        if ((options & static_cast<unsigned long>(known[i].mask)) != 0u) {
            set.insert(known[i].code);
        }
    }
    return set;
}

// generateMfSfc — @ 0x2de910
//
// Encodes an MFLI/MFIA "Smart Feature Code" (SFC) bitfield by checking
// for specific DeviceOptions on the input set. The first argument is a
// device-type-name string (passed to toDeviceTypeCode) which selects
// the MFLI vs MFIA encoding (different "always-on" base bits).
//
// Throws zhinst::Exception with the message "Requested to generate an
// SFC for an unsupported device type (<code>)." if the type code is
// neither MFLI (10) nor MFIA (11).
//
// Returns a `sfc::FeaturesCode` strong-typed wrapper around the composed
// uint64 bitmask. The binary returns the value in `rax` (no sret), which
// is consistent with FeaturesCode being a trivial 8-byte POD wrapper.
sfc::FeaturesCode generateMfSfc(std::string const& deviceTypeName,
                                DeviceOptionSet const& options);

}  // namespace detail

// ============================================================================
// DeviceType — pimpl wrapper
//
// Offset  Size  Type             Name   Notes
// +0x00   8     DeviceTypeImpl*  impl_
// sizeof(DeviceType) = 0x08
// ============================================================================
//! \brief Value-typed handle to a concrete device-type identity (model,
//! family, and installed options).
//!
//! Pimpl wrapper around an owned `detail::DeviceTypeImpl*`.  Provides
//! deep-copy semantics by delegating to `clone()` so the wrapper can
//! be passed by value while preserving the dynamic subclass identity.
//! Constructible from any of: a known `(family, options)` pair, a
//! `(code, family)` pair, a runtime device-type string with options
//! (parsed via `splitDeviceOptions` / `toDeviceOption` and instantiated
//! through `detail::GenericDeviceType`), or default-constructed to
//! `UnknownDevice`.  The threaded-through device identity used by the
//! frontend, codegen, and ELF writer to gate per-family behaviour.
class DeviceType {
public:
    DeviceType();                                                   // @ 0x2d2900
    DeviceType(DeviceFamily family);                                // @ 0x2d2930
    DeviceType(DeviceFamily family, unsigned long options);          // @ 0x2d2990
    DeviceType(DeviceTypeCode code, DeviceFamily family);           // @ 0x2d2960
    DeviceType(std::string const& deviceType,
               std::vector<std::string> const& options);            // @ 0x2d2ae0
    // Convenience overload: splits the second string into a vector via
    // splitDeviceOptions(), then forwards to the (string, vector) ctor.
    DeviceType(std::string const& deviceType,
               std::string const& options);                         // @ 0x2d2a00

    DeviceType(DeviceType const& other);                            // @ 0x2d2b40
    DeviceType(DeviceType&& other) noexcept;                        // @ 0x2d2b50
    DeviceType& operator=(DeviceType const& other);
    DeviceType& operator=(DeviceType&& other) noexcept;
    ~DeviceType();                                                  // @ 0x2d2b70

    DeviceTypeCode  code() const;                                   // @ 0x2d2c40
    DeviceFamily    family() const;                                 // @ 0x2d2c30
    DeviceOptionSet const& options() const;                         // @ 0x2d2c60
    bool            hasOption(DeviceOption opt) const;              // @ 0x2d2c50
    bool            belongsTo(DeviceFamily f) const;                // @ 0x2d2c70
    detail::DeviceTypeImpl* impl() const;                     // @ 0x2d2c20
    std::string     toString() const;                               // @ 0x2d2cb0
    void            print(std::ostream& os) const;                  // @ 0x2d2ce0
    void            swap(DeviceType& other);                        // @ 0x2d2d10

    friend bool operator==(DeviceType const& lhs, DeviceType const& rhs); // @ 0x2d2d30
    friend bool operator<(DeviceType const& lhs, DeviceType const& rhs);  // @ 0x2d2d60
    friend std::ostream& operator<<(std::ostream& os, DeviceType const& dt); // @ 0x2d2da0

private:
    detail::DeviceTypeImpl* impl_;  // +0x00
};

// ============================================================================
// Free string conversion functions
// ============================================================================

// @ 0x2df610
std::string toString(DeviceFamily family);

// @ 0x2dfbc0 — decompose a DeviceFamily bitmask into its single-bit
// components. Family values 0 and 1 produce an empty vector. A 0x800
// bit acts as a saturation sentinel (no defined family uses it).
std::vector<DeviceFamily> expand(DeviceFamily family);

// @ 0x2df760 — apply toString() to each entry of `families`.
std::vector<std::string>
toStrings(std::vector<DeviceFamily> const& families);

// @ 0x2dfa00 — operator<< writes toString(family) to ostream.
std::ostream& operator<<(std::ostream& os, DeviceFamily family);

// @ 0x2d40b0
std::string toString(DeviceTypeCode code);

// @ 0x2cfee0 — looks up option name in the 31-entry jump table; for
// DeviceOption values 0 ("MF"/"MFK") and 6 ("RT"/"RTK") the result depends
// on whether `family == DeviceFamily::HF2`.
std::string toString(DeviceOption opt, DeviceFamily family);

// @ 0x2d2e20 — surprisingly simple: returns toString(dt.code()). Options
// are NOT appended here. To stringify a DeviceType together with its
// options, use the (DeviceOptionSet, DeviceFamily, separator) overload
// below to format `dt.options()` separately and concatenate.
std::string toString(DeviceType const& dt);

// @ 0x2d0130 — joins option names from `set` (in alphabetical order, per
// the underlying byName_ map) using `separator`. Implemented via a
// vector<string> + boost::algorithm::join.
std::string toString(DeviceOptionSet const& set,
                     DeviceFamily family,
                     std::string const& separator);

// @ 0x2d2dd0 — convenience wrapper around the (DeviceOptionSet,
// DeviceFamily, separator) overload above. Equivalent to:
//   toString(dt.options(), dt.family(), separator)
std::string getOptionsAsString(DeviceType const& dt,
                               std::string const& separator);

// @ 0x2d2d90 — free function: delegates to dt.hasOption(opt).
bool hasOption(DeviceType const& dt, DeviceOption const& opt);

// @ 0x2d4350 — operator<< writes toString(code) to ostream.
std::ostream& operator<<(std::ostream& os, DeviceTypeCode code);

// @ 0x2d43d0 — reverse lookup; returns DeviceTypeCode(33) on failure.
// Implementation uses a function-local static unordered_map<string,
// DeviceTypeCode> built lazily (33 entries; key 0 is the special "none"
// alias for DeviceTypeCode::Unknown, plus all 32 named codes).
DeviceTypeCode toDeviceTypeCode(std::string const& s);

// @ 0x2debd0 — fuzzy device-family lookup from a (possibly mixed-case,
// option-suffixed) device-type string.
//
// Implementation has two layers:
//   1. A fast inline string-prefix table that recognizes exact tokens:
//        "none"     -> DeviceFamily(0)         [the "no family" sentinel]
//        "DEFAULT"  -> DeviceFamily::HF2       [used by some default code paths]
//        "SHFACC"   -> DeviceFamily::SHF
//        "SHFPPC2"  -> DeviceFamily::SHF
//        "SHFPPC4"  -> DeviceFamily::SHF
//   2. A function-local static `std::map<std::string, DeviceFamily>
//      familyNames` (lazy-initialized via __cxa_guard) populated with 10
//      entries from anonymous-namespace strings (hf2/uhf/mf/hdawg/shf/
//      pqsc/hwmock/ghf/qhub/vhf — note: no shfacc, no unknown). The
//      lookup uses `upper_bound(s) - 1` then verifies via
//      `boost::algorithm::starts_with(s, key)`.
// On miss, returns DeviceFamily(0x800) (one bit beyond VHF=0x400 — the
// Unknown sentinel that toString renders as "unknown").
DeviceFamily toDeviceFamily(std::string const& s);

// @ 0x2d05b0 — reverse-lookup string -> DeviceOption. Throws on miss
// (the lazy-init table is a std::unordered_map; toDeviceOptions() below
// catches and swallows the resulting exception so unknown option names
// are silently dropped during DeviceType parsing).
DeviceOption toDeviceOption(std::string const& s);

// @ 0x2d0fb0 — builds a DeviceOptionSet by mapping each entry of `opts`
// through toDeviceOption() and inserting it. Unknown option names are
// caught and dropped (try/catch around insert). The resulting set
// carries `family` as its DeviceFamily.
DeviceOptionSet toDeviceOptions(std::vector<std::string> const& opts,
                                DeviceFamily family);

// @ 0x2d0460 — splits a device-options string. Trims leading/trailing
// whitespace via `boost::algorithm::trim_copy_if(ctype::space)`, then
// splits on '\n' (token_compress_off) into a `vector<string>`. An empty
// trimmed input yields an empty vector.
std::vector<std::string> splitDeviceOptions(std::string const& s);

// ============================================================================
// Free predicate functions
// ============================================================================
bool isDefined(DeviceType const& dt);   // @ 0x2d2e50 — code != Unknown
bool isIa(DeviceType const& dt);        // @ 0x2d2e70
bool isMfia(DeviceType const& dt);      // @ 0x2d2ec0 — code == MFIA
bool isUhfqa(DeviceType const& dt);     // @ 0x2d2ee0 — code == UHFQA
bool isShfqa(DeviceType const& dt);     // @ 0x2d2f00 — code in {SHFQA2, SHFQA4, SHFQC}
bool isShfsg(DeviceType const& dt);     // @ 0x2d2f40 — code in {SHFSG2, SHFSG4, SHFSG8}
bool isShfqc(DeviceType const& dt);     // @ 0x2d2f80 — code == SHFQC
bool isShfppc(DeviceType const& dt);    // @ 0x2d2fa0 — code in {SHFPPC2, SHFPPC4}
bool isShfli(DeviceType const& dt);     // @ 0x2d2fd0 — code == SHFLI
bool isGhfli(DeviceType const& dt);     // @ 0x2d2ff0 — code == GHFLI
bool isVhfli(DeviceType const& dt);     // @ 0x2d3010 — code == VHFLI
bool hasMf(DeviceType const& dt);       // @ 0x2d3030

// @ 0x2d4c30 — returns the lazily-initialized
// boost::container::flat_set<DeviceTypeCode> containing all 33 named
// DeviceTypeCode values (0..32). The function-local static is built
// once on first call by an anonymous-namespace helper that uses a
// transform_iterator<int -> DeviceTypeCode> to bulk-insert the range.
boost::container::flat_set<DeviceTypeCode> const& allDevices();

}  // namespace zhinst

// Note: no std::hash<DeviceOption> specialization needed — libstdc++
// provides an auto-specialization for any enum type since C++14.
