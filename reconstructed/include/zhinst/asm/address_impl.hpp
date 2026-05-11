// ============================================================================
// Reconstructed from disassembly of _seqc_compiler.so
// detail::AddressImpl<T> — thin wrapper around an unsigned address value
//
// Used as parameter type throughout AsmCommands, Cache, Prefetch, ElfWriter.
// Passed by value in registers. Formatted as "%u" via boost::format.
//
// operator<<(ostream&, AddressImpl<uint>) at 0x1c7ce0.
// ============================================================================
#pragma once

#include <cstdint>
#include <ostream>

namespace zhinst {
namespace detail {

//! \brief Strongly-typed wrapper around an unsigned address value.
//!
//! `AddressImpl<T>` distinguishes address-typed parameters from raw
//! integers in `AsmCommands`, `Cache`, `Prefetch`, and `ElfWriter`
//! signatures.  The underlying integer is stored in `value`; both the
//! constructor from `T` and the conversion back to `T` are implicit so
//! call sites read naturally, while the named type still documents
//! intent and selects the address-formatting overload of `operator<<`.
template <typename T>
struct AddressImpl {
    T value;  //!< Underlying address value held by this strongly-typed wrapper.

    //! \brief Default-constructs the wrapper holding a zero address.
    AddressImpl() : value(0) {}
    //! \brief Implicitly wraps a raw address value.
    //! \param v Address value to store.
    AddressImpl(T v) : value(v) {}  // implicit — binary assigns uint directly

    //! \brief Reassigns the wrapped address value.
    //! \param v New address value to store.
    //! \return Reference to `*this`.
    AddressImpl& operator=(T v) { value = v; return *this; }
    //! \brief Implicit conversion back to the raw address value, so the
    //! wrapper participates transparently in arithmetic and comparisons.
    operator T() const { return value; }
};

} // namespace detail

// operator<<(ostream&, AddressImpl<uint>) — 0x1c7ce0
// Formats as "%u" via boost::format.
std::ostream& operator<<(std::ostream& os, detail::AddressImpl<uint32_t> addr);

} // namespace zhinst
