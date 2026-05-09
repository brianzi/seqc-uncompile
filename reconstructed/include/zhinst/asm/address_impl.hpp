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
//! integers in `AsmCommands`, `Cache`, `Prefetch`, and `ElfWriter` APIs
//! while remaining trivially copyable: it implicitly constructs from the
//! underlying integer type and converts back to it, so call sites read
//! naturally yet the compiler refuses unintended numeric mixing.
template <typename T>
struct AddressImpl {
    T value;

    AddressImpl() : value(0) {}
    AddressImpl(T v) : value(v) {}  // implicit — binary assigns uint directly

    AddressImpl& operator=(T v) { value = v; return *this; }
    operator T() const { return value; }
};

} // namespace detail

// operator<<(ostream&, AddressImpl<uint>) — 0x1c7ce0
// Formats as "%u" via boost::format.
std::ostream& operator<<(std::ostream& os, detail::AddressImpl<uint32_t> addr);

} // namespace zhinst
