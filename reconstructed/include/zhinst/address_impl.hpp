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
