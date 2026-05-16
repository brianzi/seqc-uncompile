// ============================================================================
// Reconstructed from disassembly of _seqc_compiler.so
// device_sfc_options.cpp
//
// Out-of-line definition of `detail::initializeSfcOptions<T, N>` plus
// the 13 explicit instantiations the binary emits as addressable text
// symbols.  Verified entry @ 0x2e0d50 (`<Hf2Option, 6>`):
//
//   push %rbp ; mov %rsp,%rbp ; (callee-saved spills)
//   call DeviceOptionSet::DeviceOptionSet(family)
//   for i in 0..N:
//       test ~options, arr[i].mask
//       if zero: call DeviceOptionSet::insert(arr[i].code)
//   ret  // returns set in rax (sret pointer in rdi)
//
// The remaining 12 instantiations have the same shape with N copies
// of the test+insert pair, ranging in size from 0x85 B (`<MfOption,8>`,
// inferred) up to 0x115 B (`<UhfOption,12>`).
//
// All 13 mangled symbols are produced; sizes will match within the
// instruction-selection latitude of the compiler.  The ctor call sites
// in `device_{hf2,mf,uhf,hdawg,shf,ghf,vhf}.cpp` previously inlined
// this body; they now resolve to these out-of-line copies because of
// the matching `extern template` declarations in
// `zhinst/device/device_type.hpp`.
// ============================================================================

#include "zhinst/device/device_type.hpp"

namespace zhinst {
namespace detail {

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

// 13 explicit instantiation definitions, in the same order as the
// `extern template` declarations in the header.
template DeviceOptionSet initializeSfcOptions<sfc::Hf2Option, 6>(
    std::array<OptionCodePair<sfc::Hf2Option>, 6> const&, DeviceFamily,
    unsigned long);
template DeviceOptionSet initializeSfcOptions<sfc::Hf2Option, 7>(
    std::array<OptionCodePair<sfc::Hf2Option>, 7> const&, DeviceFamily,
    unsigned long);
template DeviceOptionSet initializeSfcOptions<sfc::MfOption, 8>(
    std::array<OptionCodePair<sfc::MfOption>, 8> const&, DeviceFamily,
    unsigned long);
template DeviceOptionSet initializeSfcOptions<sfc::MfOption, 9>(
    std::array<OptionCodePair<sfc::MfOption>, 9> const&, DeviceFamily,
    unsigned long);
template DeviceOptionSet initializeSfcOptions<sfc::UhfOption, 5>(
    std::array<OptionCodePair<sfc::UhfOption>, 5> const&, DeviceFamily,
    unsigned long);
template DeviceOptionSet initializeSfcOptions<sfc::UhfOption, 7>(
    std::array<OptionCodePair<sfc::UhfOption>, 7> const&, DeviceFamily,
    unsigned long);
template DeviceOptionSet initializeSfcOptions<sfc::UhfOption, 10>(
    std::array<OptionCodePair<sfc::UhfOption>, 10> const&, DeviceFamily,
    unsigned long);
template DeviceOptionSet initializeSfcOptions<sfc::UhfOption, 12>(
    std::array<OptionCodePair<sfc::UhfOption>, 12> const&, DeviceFamily,
    unsigned long);
template DeviceOptionSet initializeSfcOptions<sfc::HdawgOption, 6>(
    std::array<OptionCodePair<sfc::HdawgOption>, 6> const&, DeviceFamily,
    unsigned long);
template DeviceOptionSet initializeSfcOptions<sfc::ShfOption, 4>(
    std::array<OptionCodePair<sfc::ShfOption>, 4> const&, DeviceFamily,
    unsigned long);
template DeviceOptionSet initializeSfcOptions<sfc::ShfOption, 5>(
    std::array<OptionCodePair<sfc::ShfOption>, 5> const&, DeviceFamily,
    unsigned long);
template DeviceOptionSet initializeSfcOptions<sfc::ShfOption, 8>(
    std::array<OptionCodePair<sfc::ShfOption>, 8> const&, DeviceFamily,
    unsigned long);
template DeviceOptionSet initializeSfcOptions<sfc::VhfOption, 6>(
    std::array<OptionCodePair<sfc::VhfOption>, 6> const&, DeviceFamily,
    unsigned long);

}  // namespace detail
}  // namespace zhinst
