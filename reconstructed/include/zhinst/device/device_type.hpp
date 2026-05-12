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
//! \brief Coarse Zurich Instruments product family identifier, encoded
//! as a one-hot bitmask so that several families can be combined for
//! membership tests.
//!
//! Each named family occupies a single bit (`HF2 = 0x001` … `VHF =
//! 0x400`), which lets `DeviceType::belongsTo()` implement family
//! membership as a bitwise AND.  The enum also doubles as the storage
//! for `expand()`, which decomposes a multi-bit value back into its
//! constituent single-bit families.  `Unknown = 0` is the default
//! sentinel; values with bit `0x800` set are produced by
//! `toDeviceFamily()` on a parse miss and render as `"unknown"`.
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
//! \brief Specific Zurich Instruments device-model identifier (33
//! values, plain 0..32).
//!
//! Distinguishes individual product variants within a `DeviceFamily`
//! (e.g. `HDAWG4` vs `HDAWG8`, `SHFQA2` vs `SHFQA4`).  Used as the
//! primary discriminator throughout the compiler for per-model
//! behaviour: instruction-set variant selection, channel counts,
//! waveform memory sizes, and feature gating (see `getDeviceConstants`
//! and `getAwgDeviceProps`).  `Unknown = 0` is the default; the
//! lazily-built reverse-lookup table in `toDeviceTypeCode()` returns
//! the sentinel value 33 on a parse miss.
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
//! \brief Hardware/software option installed on a device, as carried
//! by `DeviceOptionSet`.
//!
//! 31 values (0..30).  After construction these are produced by
//! `initializeSfcOptions()`, which maps each set bit of a per-family
//! one-hot bitmask (see `sfc::Hf2Option`, `sfc::MfOption`, …) to the
//! corresponding `DeviceOption` code.  The string rendered by
//! `toString(DeviceOption, DeviceFamily)` is family-scoped: most names
//! are constant, but value 0 renders as `"MFK"` on `DeviceFamily::HF2`
//! and `"MF"` everywhere else, and value 6 renders as `"RTK"` on HF2
//! and `"RT"` elsewhere.
//!
//! \binarynote `None` and `MF` share the integer value 0; the binary
//! uses code 0 dually as both the "no option" sentinel and the
//! MF/MFK marker, distinguished only by context (a value not present
//! in the underlying set means "no option" regardless of name).
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
//! \brief Service-Feature-Code (SFC) bitfields encoding the licensed /
//! installed option set for each device family.
//!
//! Each device family contributes one strongly-typed `*Option` enum
//! plus a `FeaturesCode` value carrying the same bits in a single
//! 64-bit word; the user-facing `DeviceOptionSet` is built by mapping
//! these bits through the family-specific `initialize*SfcOptions`
//! helpers.  Bits not listed in those tables are silently ignored.
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
    //! \brief Underlying 64-bit SFC bitfield.
    std::uint64_t value;

    //! \brief Default-constructs to an all-zero SFC bitfield.
    constexpr FeaturesCode() noexcept : value(0) {}
    //! \brief Wraps the given raw 64-bit SFC bitfield.
    //! \param v Raw SFC bitfield value.
    constexpr explicit FeaturesCode(std::uint64_t v) noexcept : value(v) {}

    //! \brief Extracts the underlying 64-bit SFC bitfield.
    //! \return The raw `value` member.
    constexpr explicit operator std::uint64_t() const noexcept { return value; }

    //! \brief Equality comparison on the underlying bitfields.
    //! \param a Left operand.
    //! \param b Right operand.
    //! \return `true` if `a.value == b.value`.
    friend constexpr bool operator==(FeaturesCode a, FeaturesCode b) noexcept {
        return a.value == b.value;
    }
    //! \brief Inequality comparison on the underlying bitfields.
    //! \param a Left operand.
    //! \param b Right operand.
    //! \return `true` if `a.value != b.value`.
    friend constexpr bool operator!=(FeaturesCode a, FeaturesCode b) noexcept {
        return a.value != b.value;
    }
};

//! \brief Per-bit option mask used at construction of HF2-family
//! devices; mapped through `initializeSfcOptions` into
//! `DeviceOption` values.
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

//! \brief Per-bit option mask used at construction of MF-family
//! devices (MFLI / MFIA); mapped through `initializeSfcOptions` into
//! `DeviceOption` values.  `IA` is only valid on MFLI.
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

//! \brief Per-bit option mask used at construction of UHF-family
//! devices (UHFLI / UHFAWG / UHFQA / UHFIA); mapped through
//! `initializeSfcOptions` into `DeviceOption` values.  Several bits
//! are only valid on a subset of the family — see per-bit comments.
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

//! \brief Per-bit option mask used at construction of HDAWG-family
//! devices (HDAWG4 / HDAWG8); mapped through `initializeSfcOptions`
//! into `DeviceOption` values.
enum class HdawgOption : uint32_t {
    None = 0,
    MF   = 0x00001,  // -> code 0  (MF)
    FF   = 0x00020,  // -> code 2  (FF)
    ME   = 0x00200,  // -> code 19 (ME)
    SKW  = 0x00800,  // -> code 22 (SKW)
    PC   = 0x08000,  // -> code 20 (PC)
    CNT  = 0x10000,  // -> code 17 (CNT)
};

//! \brief Per-bit option mask used at construction of SHF-family
//! devices (SHFQA{2,4} / SHFSG{2,4,8} / SHFQC / SHFLI) and reused
//! verbatim by GHF-family `Ghfli`; mapped through
//! `initializeSfcOptions` into `DeviceOption` values.  Several bits
//! are SHFLI-only — see per-bit comments.
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

//! \brief Per-bit option mask used at construction of VHF-family
//! devices (VHFLI); mapped through `initializeSfcOptions` into
//! `DeviceOption` values.
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
    //! \brief Map type the iterator walks (option-name → option).
    using map_type = std::map<std::string, DeviceOption>;
    //! \brief Underlying ordered-map `const_iterator` wrapped by this class.
    using underlying_iterator = map_type::const_iterator;

    //! \brief Wraps an existing map const_iterator.
    //! \param it Position in the option set's name-keyed map.
    explicit DeviceOptionSetConstIterator(underlying_iterator it);

    //! \brief Yields the `DeviceOption` at the current position.
    //! \return The option value at the iterator's current position.
    DeviceOption dereference() const;
    //! \brief Advances to the next `DeviceOption` in alphabetical
    //! name order.
    void increment();
    //! \brief Returns true when both iterators refer to the same
    //! position in the same underlying map.
    //! \param other Iterator to compare against.
    //! \return `true` when both iterators refer to the same position
    //!         in the same underlying map; `false` otherwise.
    bool equal(DeviceOptionSetConstIterator const& other) const;

    //! \brief Dereference; equivalent to `dereference()`.
    //! \return The `DeviceOption` at the current position.
    DeviceOption operator*() const { return dereference(); }
    //! \brief Pre-increment; advances past the current element.
    //! \return Reference to `*this` after advancing.
    DeviceOptionSetConstIterator& operator++() { increment(); return *this; }
    //! \brief Post-increment; returns the position before advancing.
    //! \return Copy of the iterator at its pre-increment position.
    DeviceOptionSetConstIterator operator++(int) {
        DeviceOptionSetConstIterator tmp(*this);
        increment();
        return tmp;
    }
    //! \brief Equality comparison against another iterator.
    //! \param other Iterator to compare against.
    //! \return `true` when both iterators refer to the same position.
    bool operator==(DeviceOptionSetConstIterator const& other) const {
        return equal(other);
    }
    //! \brief Inequality comparison against another iterator.
    //! \param other Iterator to compare against.
    //! \return `true` when the two iterators refer to different positions.
    bool operator!=(DeviceOptionSetConstIterator const& other) const {
        return !equal(other);
    }

private:
    //! \brief Wrapped ordered-map const_iterator.
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
    //! \brief Forward const_iterator type yielded by `begin()` /
    //!        `end()`; walks the set in alphabetical name order.
    using const_iterator = DeviceOptionSetConstIterator;

    //! \brief Constructs an empty option set bound to the given family.
    //! \param family Device family the set is bound to.
    explicit DeviceOptionSet(DeviceFamily family);                       // @ 0x2cf970
    //! \brief Constructs an option set pre-populated with `options`,
    //! bound to the given family.
    //! \param options Initial options to insert.
    //! \param family  Device family the set is bound to.
    DeviceOptionSet(std::initializer_list<DeviceOption> const& options,
                    DeviceFamily family);                                // @ 0x2cf9a0

    //! \brief Copy-constructs by duplicating both storage indices and
    //!        the bound family.
    DeviceOptionSet(DeviceOptionSet const&) = default;
    //! \brief Move-constructs by stealing both storage indices.
    DeviceOptionSet(DeviceOptionSet&&) noexcept = default;
    //! \brief Copy-assigns by duplicating both storage indices and
    //!        the bound family.
    //! \return Reference to `*this`.
    DeviceOptionSet& operator=(DeviceOptionSet const&) = default;
    //! \brief Move-assigns by stealing both storage indices.
    //! \return Reference to `*this`.
    DeviceOptionSet& operator=(DeviceOptionSet&&) noexcept = default;
    //! \brief Destroys both storage indices.
    ~DeviceOptionSet() = default;

    //! \brief Returns an iterator to the first option in alphabetical
    //! name order (per the family-scoped names from `toString`).
    //! \return Iterator positioned at the first option.
    const_iterator begin() const;       // @ 0x2cfb60
    //! \brief Returns the past-the-end iterator.
    //! \return Iterator one past the last option.
    const_iterator end() const;         // @ 0x2cfb70

    //! \brief O(1) membership test against the underlying
    //! `unordered_set<DeviceOption>`.
    //! \param opt Option to test for membership.
    //! \return `true` when `opt` is present; `false` otherwise.
    bool contains(DeviceOption opt) const;  // @ 0x2cfb80
    //! \brief Returns true if the set contains no options.
    //! \return `true` when the set is empty; `false` otherwise.
    bool empty() const;                     // @ 0x2cfcc0
    //! \brief Returns the number of options in the set.
    //! \return Count of options currently held.
    std::size_t size() const;               // @ 0x2cfcd0
    //! \brief Returns the family this set was bound to at construction.
    //! \return The bound device family.
    DeviceFamily family() const;            // @ 0x2cfce0

    //! \brief Inserts `opt` into both the value-set and the
    //! name-keyed map.  The name used as the map key is
    //! `toString(opt, family())`, so the set's iteration order
    //! depends on the bound family.
    //! \param opt Option to insert.
    void insert(DeviceOption opt);          // @ 0x2cfcf0

    friend bool operator==(DeviceOptionSet const& a, DeviceOptionSet const& b);
    //! \brief Inequality comparison; negation of `operator==`.
    //! \param a Left operand.
    //! \param b Right operand.
    //! \return `true` when the two sets differ in either family or contents.
    friend bool operator!=(DeviceOptionSet const& a, DeviceOptionSet const& b) {
        return !(a == b);
    }

private:
    //! \brief Fast O(1) membership index.
    std::unordered_set<DeviceOption> values_;       // +0x00, 40 bytes
    //! \brief Iteration-order index keyed by family-scoped option name.
    std::map<std::string, DeviceOption> byName_;    // +0x28, 24 bytes
    //! \brief Family the set is bound to; selects option-name scoping.
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
    //! \brief Default-constructs an `Unknown` device with no options
    //! and `DeviceFamily::Unknown`.
    DeviceTypeImpl();                                              // @ 0x2d3060
    //! \brief Constructs with an explicit code/family pair and an
    //! empty option set bound to `family`.
    //! \param code   Device-model code.
    //! \param family Device family.
    DeviceTypeImpl(DeviceTypeCode code, DeviceFamily family);      // @ 0x2d3090
    //! \brief Constructs with an explicit code/family pair and an
    //! existing option set (moved into the implementation).
    //! \param code    Device-model code.
    //! \param family  Device family.
    //! \param options Option set to move into this implementation.
    DeviceTypeImpl(DeviceTypeCode code, DeviceFamily family,
                   DeviceOptionSet options);                       // @ 0x2d30b0
    //! \brief Tuple-unpacking constructor used internally by
    //! `GenericDeviceType` to forward parsed `(code, family,
    //! options)` triples through a single argument.
    //! \param args Tuple holding the code, family, and option set
    //!             to install.
    // Tuple-taking ctor used by the GenericDeviceType ctor (and by it
    // alone, as far as the binary shows). Unpacks the std::tuple by
    // moving its DeviceOptionSet into options_. The first 8 bytes of
    // the tuple hold {DeviceTypeCode, DeviceFamily} and are copied
    // bitwise into [code_, family_].
    explicit DeviceTypeImpl(
        std::tuple<DeviceTypeCode, DeviceFamily, DeviceOptionSet> args);  // @ 0x2d3190

    //! \brief Polymorphic destructor for the device-type hierarchy.
    virtual ~DeviceTypeImpl();
    //! \brief Polymorphic deep copy used by `DeviceType`'s copy
    //! constructor and assignment to preserve the dynamic subclass
    //! identity.
    //! \return Newly-allocated copy whose dynamic type matches the
    //!         most-derived subclass.
    virtual DeviceTypeImpl* clone() const;  // vtable[0]; impl @ 0x2d3280
                                            // (doClone in mangled name).

    //! \brief Returns the broad product family this implementation
    //! belongs to.
    //! \return The bound device family.
    DeviceFamily   family() const;          // @ 0x2d32e0
    //! \brief Returns the specific device-model code.
    //! \return The device-model code.
    DeviceTypeCode code() const;            // @ 0x2d32f0
    //! \brief Returns true if `opt` is in the implementation's
    //! option set; delegates to `DeviceOptionSet::contains`.
    //! \param opt Option to test for.
    //! \return `true` when `opt` is present in the option set;
    //!         `false` otherwise.
    bool           hasOption(DeviceOption opt) const;  // @ 0x2d3300
    //! \brief Returns the installed-options set.
    //! \return Reference to the implementation's option set.
    DeviceOptionSet const& options() const; // @ 0x2d3310

protected:
    //! \brief Specific device-model identifier.
    DeviceTypeCode  code_;     // +0x08
    //! \brief Broad product-family identifier.
    DeviceFamily    family_;   // +0x0c
    //! \brief Installed hardware/software options.
    DeviceOptionSet options_;  // +0x10
};

// ----------------------------------------------------------------------------
// detail::OptionCodePair<T> — 8 bytes (uint32 mask + uint32 DeviceOption code)
//
// Each entry of an `initializeXxxOptions::knownOptions` array. T is the
// per-family one-hot bitmask enum (sfc::Hf2Option / MfOption / ...).
// ----------------------------------------------------------------------------
//! \brief One entry of a per-family option-mapping table: the
//! single-bit `mask` value carried by the family-specific bitmask
//! enum, paired with the canonical `DeviceOption` `code` it
//! resolves to.
template <class T>
struct OptionCodePair {
    //! \brief Single-bit value of the family-specific bitmask enum.
    T            mask;
    //! \brief Canonical `DeviceOption` this bit maps to.
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
//! \brief Translates a raw user-supplied options bitmask into a
//! `DeviceOptionSet` by walking a per-family `(mask, code)` table.
//!
//! For each entry of `known`, if the corresponding bit is set in
//! `options` the entry's `code` is inserted into a fresh
//! `DeviceOptionSet` bound to `family`.  Bits not present in
//! `known` are silently ignored.  This is the single template that
//! all 15 per-family `initializeXxxOptions` helpers in the binary
//! instantiate against their own `knownOptions` array.
//!
//! \param known   per-family option-mapping table (`mask` bits to
//!                `DeviceOption` codes).
//! \param family  family the resulting set is bound to.
//! \param options raw options bitmask supplied by the caller.
//! \return populated `DeviceOptionSet`.
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
//! \brief Encodes the MFLI/MFIA Smart Feature Code (SFC) bitfield
//! that corresponds to the given device-type name and option set.
//!
//! Resolves `deviceTypeName` via `toDeviceTypeCode`, picks the
//! MFLI- or MFIA-specific "always-on" base bits accordingly, then
//! ORs in further bits for each `DeviceOption` in `options` that
//! the encoding recognises.
//!
//! \param deviceTypeName Device-type string ("MFLI" or "MFIA"); any
//!                       other value is rejected.
//! \param options        Options carried by the target device.
//! \return Composed 64-bit feature code wrapped in `sfc::FeaturesCode`.
//! \throws zhinst::Exception if `deviceTypeName` does not resolve to
//!         `DeviceTypeCode::MFLI` or `DeviceTypeCode::MFIA`.
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
    //! \brief Default-constructs to an `Unknown` device with no
    //! options.
    DeviceType();                                                   // @ 0x2d2900
    //! \brief Constructs the canonical default device for `family`
    //! via the family's factory (no options).
    //! \param family Device family to instantiate.
    DeviceType(DeviceFamily family);                                // @ 0x2d2930
    //! \brief Constructs the canonical default device for `family`
    //! with the given raw options bitmask, mapped through the
    //! family's `initializeSfcOptions` table.
    //! \param family  Device family to instantiate.
    //! \param options Raw per-family options bitmask.
    DeviceType(DeviceFamily family, unsigned long options);          // @ 0x2d2990
    //! \brief Constructs from an explicit `(code, family)` pair with
    //! no options.
    //! \param code   Device-model code.
    //! \param family Device family.
    DeviceType(DeviceTypeCode code, DeviceFamily family);           // @ 0x2d2960
    //! \brief Constructs a generic device-type from a runtime
    //! device-type name and a pre-split list of option strings.
    //! Unknown option names are silently dropped.  Implemented via
    //! `detail::GenericDeviceType`.
    //! \param deviceType Device-type name string.
    //! \param options    Per-option name strings.
    DeviceType(std::string const& deviceType,
               std::vector<std::string> const& options);            // @ 0x2d2ae0
    //! \brief Convenience overload: splits `options` on newlines via
    //! `splitDeviceOptions()` and forwards to the
    //! `(string, vector<string>)` constructor.
    //! \param deviceType Device-type name string.
    //! \param options    Newline-separated option-name string.
    DeviceType(std::string const& deviceType,
               std::string const& options);                         // @ 0x2d2a00

    //! \brief Deep-copies via the underlying `clone()`.
    //! \param other Source instance to copy from.
    DeviceType(DeviceType const& other);                            // @ 0x2d2b40
    //! \brief Steals the impl pointer from `other`, leaving it null.
    //! \param other Source instance to move from.
    DeviceType(DeviceType&& other) noexcept;                        // @ 0x2d2b50
    //! \brief Copy-assigns by cloning the source impl (or
    //!        copy-and-swap equivalent), then destroying the prior
    //!        impl.  Self-assignment is a no-op.
    //! \param other Source instance to copy from.
    //! \return Reference to `*this`.
    DeviceType& operator=(DeviceType const& other);
    //! \brief Move-assigns by destroying the prior impl and stealing
    //!        the source impl pointer.  Self-assignment is a no-op.
    //! \param other Source instance to move from.
    //! \return Reference to `*this`.
    DeviceType& operator=(DeviceType&& other) noexcept;
    //! \brief Destroys the owned `DeviceTypeImpl` instance.
    ~DeviceType();                                                  // @ 0x2d2b70

    //! \brief Returns the specific device-model code.
    //! \return The device-model code.
    DeviceTypeCode  code() const;                                   // @ 0x2d2c40
    //! \brief Returns the broad product family.
    //! \return The bound device family.
    DeviceFamily    family() const;                                 // @ 0x2d2c30
    //! \brief Returns the installed-options set.
    //! \return Reference to the device's option set.
    DeviceOptionSet const& options() const;                         // @ 0x2d2c60
    //! \brief Returns true if `opt` is in the installed-options set.
    //! \param opt Option to test for.
    //! \return `true` when `opt` is present; `false` otherwise.
    bool            hasOption(DeviceOption opt) const;              // @ 0x2d2c50
    //! \brief Returns true if any bit of this device's family is
    //! also set in `f`; multi-bit `f` values test for membership in
    //! any of the constituent families.
    //! \param f Family bitmask to test against.
    //! \return `true` when this device's family overlaps `f`;
    //!         `false` otherwise.
    bool            belongsTo(DeviceFamily f) const;                // @ 0x2d2c70
    //! \brief Returns the raw pimpl pointer (non-owning) for
    //! subsystems that need to access the polymorphic identity
    //! directly.
    //! \return Non-owning pointer to the underlying implementation.
    detail::DeviceTypeImpl* impl() const;                     // @ 0x2d2c20
    //! \brief Returns the device-type-code string only; options are
    //! NOT appended.  Use `getOptionsAsString()` for the options
    //! portion.
    //! \return Device-type-code name string.
    std::string     toString() const;                               // @ 0x2d2cb0
    //! \brief Writes `toString()` to `os`.
    //! \param os Output stream to write to.
    void            print(std::ostream& os) const;                  // @ 0x2d2ce0
    //! \brief Swaps impl pointers with `other` in O(1).
    //! \param other Instance to swap impl pointers with.
    void            swap(DeviceType& other);                        // @ 0x2d2d10

    friend bool operator==(DeviceType const& lhs, DeviceType const& rhs); // @ 0x2d2d30
    friend bool operator<(DeviceType const& lhs, DeviceType const& rhs);  // @ 0x2d2d60
    friend std::ostream& operator<<(std::ostream& os, DeviceType const& dt); // @ 0x2d2da0

private:
    //! \brief Owned pimpl pointer carrying the polymorphic identity.
    detail::DeviceTypeImpl* impl_;  // +0x00
};

// ============================================================================
// Free string conversion functions
// ============================================================================

// @ 0x2df610
//! \brief Renders a `DeviceFamily` as its canonical uppercase name
//! (e.g. `"HDAWG"`, `"SHF"`).  Unrecognised single-bit values render
//! as `"unknown"`.
std::string toString(DeviceFamily family);

// @ 0x2dfbc0 — decompose a DeviceFamily bitmask into its single-bit
// components. Family values 0 and 1 produce an empty vector. A 0x800
// bit acts as a saturation sentinel (no defined family uses it).
//! \brief Decomposes a `DeviceFamily` bitmask into the list of its
//! constituent single-bit families.
//! \note `Unknown = 0` and `HF2 = 1` both yield an empty
//! vector; bit `0x800` acts as a saturation sentinel (no defined
//! family uses it) and stops the walk early.
std::vector<DeviceFamily> expand(DeviceFamily family);

// @ 0x2df760 — apply toString() to each entry of `families`.
//! \brief Returns the names of each family in `families` via
//! `toString(DeviceFamily)`.
std::vector<std::string>
toStrings(std::vector<DeviceFamily> const& families);

// @ 0x2dfa00 — operator<< writes toString(family) to ostream.
//! \brief Writes `toString(family)` to `os`.
std::ostream& operator<<(std::ostream& os, DeviceFamily family);

// @ 0x2d40b0
//! \brief Renders a `DeviceTypeCode` as its canonical name (e.g.
//! `"HDAWG8"`, `"SHFQA4"`).  Out-of-range values render as
//! `"unknown"`.
std::string toString(DeviceTypeCode code);

// @ 0x2cfee0 — looks up option name in the 31-entry jump table; for
// DeviceOption values 0 ("MF"/"MFK") and 6 ("RT"/"RTK") the result depends
// on whether `family == DeviceFamily::HF2`.
//! \brief Renders a `DeviceOption` as its short name, scoped to the
//! given `family`.
//! \note `DeviceOption(0)` renders as `"MFK"` on
//! `DeviceFamily::HF2` and `"MF"` everywhere else; `DeviceOption(6)`
//! renders as `"RTK"` on HF2 and `"RT"` elsewhere.  All other
//! values are family-agnostic.
std::string toString(DeviceOption opt, DeviceFamily family);

// @ 0x2d2e20 — surprisingly simple: returns toString(dt.code()). Options
// are NOT appended here. To stringify a DeviceType together with its
// options, use the (DeviceOptionSet, DeviceFamily, separator) overload
// below to format `dt.options()` separately and concatenate.
//! \brief Returns the device-type-code name of `dt`.
//! \note Options are intentionally NOT appended; combine with
//! `getOptionsAsString()` to render a device together with its
//! options.
std::string toString(DeviceType const& dt);

// @ 0x2d0130 — joins option names from `set` (in alphabetical order, per
// the underlying byName_ map) using `separator`. Implemented via a
// vector<string> + boost::algorithm::join.
//! \brief Joins the family-scoped names of every option in `set`
//! using `separator`.  Options appear in the alphabetical order
//! enforced by the set's name-keyed map.
std::string toString(DeviceOptionSet const& set,
                     DeviceFamily family,
                     std::string const& separator);

// @ 0x2d2dd0 — convenience wrapper around the (DeviceOptionSet,
// DeviceFamily, separator) overload above. Equivalent to:
//   toString(dt.options(), dt.family(), separator)
//! \brief Convenience wrapper that renders `dt.options()` joined by
//! `separator`, scoped to `dt.family()`.
std::string getOptionsAsString(DeviceType const& dt,
                               std::string const& separator);

// @ 0x2d2d90 — free function: delegates to dt.hasOption(opt).
//! \brief Free-function form of `DeviceType::hasOption`; delegates
//! to the member function.
bool hasOption(DeviceType const& dt, DeviceOption const& opt);

// @ 0x2d4350 — operator<< writes toString(code) to ostream.
//! \brief Writes `toString(code)` to `os`.
std::ostream& operator<<(std::ostream& os, DeviceTypeCode code);

// @ 0x2d43d0 — reverse lookup; returns DeviceTypeCode(33) on failure.
// Implementation uses a function-local static unordered_map<string,
// DeviceTypeCode> built lazily (33 entries; key 0 is the special "none"
// alias for DeviceTypeCode::Unknown, plus all 32 named codes).
//! \brief Reverse lookup from a device-type name to its
//! `DeviceTypeCode`.  The string `"none"` maps to
//! `DeviceTypeCode::Unknown`.
//! \return The matching code, or the sentinel value `DeviceTypeCode(33)`
//!         on a parse miss.
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
//! \brief Fuzzy reverse lookup from a (possibly option-suffixed)
//! device-type string to its `DeviceFamily`, using **case-sensitive**
//! prefix matching against a small lazily-built name table of
//! uppercase canonical family names.
//! \note Special tokens (case-sensitive literals): `""` and `"none"`
//! map to `DeviceFamily(0)`; `"DEFAULT"` maps to `DeviceFamily::HF2`;
//! `"SHFACC"`, `"SHFPPC2"` and `"SHFPPC4"` map to
//! `DeviceFamily::SHFACC`.  Any other input is matched against the
//! 10-entry prefix table (`HF2`, `UHF`, `MF`, `HDAWG`, `SHF`, `PQSC`,
//! `HWMOCK`, `GHF`, `QHUB`, `VHF`); on a miss the function returns
//! the sentinel `DeviceFamily(0x800)`, which renders as `"unknown"`.
//! \binarynote Lower-case input strings (`"hdawg8"`, `"mfli"`, ...)
//! do **not** match — only uppercase prefixes are recognised.
DeviceFamily toDeviceFamily(std::string const& s);

// @ 0x2d05b0 — reverse-lookup string -> DeviceOption. Throws on miss
// (the lazy-init table is a std::unordered_map; toDeviceOptions() below
// catches and swallows the resulting exception so unknown option names
// are silently dropped during DeviceType parsing).
//! \brief Reverse lookup from an option name to its `DeviceOption`
//! value.
//! \throws std::out_of_range if `s` is not a recognised option name.
DeviceOption toDeviceOption(std::string const& s);

// @ 0x2d0fb0 — builds a DeviceOptionSet by mapping each entry of `opts`
// through toDeviceOption() and inserting it. Unknown option names are
// caught and dropped (try/catch around insert). The resulting set
// carries `family` as its DeviceFamily.
//! \brief Builds a `DeviceOptionSet` bound to `family` from the
//! given option-name strings.  Unrecognised names are silently
//! dropped (the per-entry `toDeviceOption` exception is caught).
DeviceOptionSet toDeviceOptions(std::vector<std::string> const& opts,
                                DeviceFamily family);

// @ 0x2d0460 — splits a device-options string. Trims leading/trailing
// whitespace via `boost::algorithm::trim_copy_if(ctype::space)`, then
// splits on '\n' (token_compress_off) into a `vector<string>`. An empty
// trimmed input yields an empty vector.
//! \brief Splits a device-options string on newlines after trimming
//! surrounding whitespace.  An all-whitespace input yields an empty
//! vector; empty lines between options are preserved as empty
//! tokens.
std::vector<std::string> splitDeviceOptions(std::string const& s);

// ============================================================================
// Free predicate functions
// ============================================================================
//! \brief Returns true if `dt` has a known device-type code (i.e.
//! is not the default-constructed `Unknown`).
bool isDefined(DeviceType const& dt);   // @ 0x2d2e50 — code != Unknown
//! \brief Returns true if `dt` is an Impedance Analyzer.  Codes
//! `UHFIA` and `MFIA` are unconditionally true; codes covered by an
//! IA-capable family also return true if the device carries the
//! `IA` option; all other codes are false.
bool isIa(DeviceType const& dt);        // @ 0x2d2e70
//! \brief Returns true if the device-type code is `MFIA`.
bool isMfia(DeviceType const& dt);      // @ 0x2d2ec0 — code == MFIA
//! \brief Returns true if the device-type code is `UHFQA`.
bool isUhfqa(DeviceType const& dt);     // @ 0x2d2ee0 — code == UHFQA
//! \brief Returns true if the device-type code is one of `SHFQA2`,
//! `SHFQA4`, or `SHFQC`.
bool isShfqa(DeviceType const& dt);     // @ 0x2d2f00 — code in {SHFQA2, SHFQA4, SHFQC}
//! \brief Returns true if the device-type code is one of `SHFSG2`,
//! `SHFSG4`, or `SHFSG8`.
bool isShfsg(DeviceType const& dt);     // @ 0x2d2f40 — code in {SHFSG2, SHFSG4, SHFSG8}
//! \brief Returns true if the device-type code is `SHFQC`.
bool isShfqc(DeviceType const& dt);     // @ 0x2d2f80 — code == SHFQC
//! \brief Returns true if the device-type code is one of `SHFPPC2`
//! or `SHFPPC4`.
bool isShfppc(DeviceType const& dt);    // @ 0x2d2fa0 — code in {SHFPPC2, SHFPPC4}
//! \brief Returns true if the device-type code is `SHFLI`.
bool isShfli(DeviceType const& dt);     // @ 0x2d2fd0 — code == SHFLI
//! \brief Returns true if the device-type code is `GHFLI`.
bool isGhfli(DeviceType const& dt);     // @ 0x2d2ff0 — code == GHFLI
//! \brief Returns true if the device-type code is `VHFLI`.
bool isVhfli(DeviceType const& dt);     // @ 0x2d3010 — code == VHFLI
//! \brief Returns true if the device carries an MF-style marker
//! option in its options set.
//! \binarynote The probed option depends on family: on
//! `DeviceFamily::MF` the predicate tests for `DeviceOption::MD`
//! (code 1), and for every other family it tests for
//! `DeviceOption::MF` (code 0).  This swap mirrors the binary's use
//! of code 1 as the "this is an MF-flavoured unit" marker on the MF
//! family itself.
bool hasMf(DeviceType const& dt);       // @ 0x2d3030

// @ 0x2d4c30 — returns the lazily-initialized
// boost::container::flat_set<DeviceTypeCode> containing all 33 named
// DeviceTypeCode values (0..32). The function-local static is built
// once on first call by an anonymous-namespace helper that uses a
// transform_iterator<int -> DeviceTypeCode> to bulk-insert the range.
//! \brief Returns a reference to a lazily-initialised flat set
//! containing every named `DeviceTypeCode` value (0..32).  Used as
//! the universe of valid codes by validation paths.
boost::container::flat_set<DeviceTypeCode> const& allDevices();

}  // namespace zhinst

// Note: no std::hash<DeviceOption> specialization needed — libstdc++
// provides an auto-specialization for any enum type since C++14.
