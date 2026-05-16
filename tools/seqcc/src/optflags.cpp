// =============================================================================
// seqcc — optimisation-flag plumbing (T8).
// =============================================================================
#include "optflags.hpp"

#include <algorithm>
#include <iomanip>
#include <ostream>
#include <stdexcept>

namespace seqcc {

namespace {

std::vector<PassInfo> const& passTable() {
    static std::vector<PassInfo> const table = {
        // name              bit    description
        {"jump-elim",        0x01,
         "Collapse trivial branch targets."},
        {"label-cleanup",    0x02,
         "Drop unreferenced asm labels."},
        {"dead-code-elim",   0x04,
         "Mark unused instructions INVALID."},
        {"reg-zero-merge",   0x08,
         "Coalesce R0/RW0 reads."},
        {"reg-alloc",        0x10,
         "Assign physical registers."},
    };
    return table;
}

}  // namespace

std::vector<PassInfo> const& knownPasses() {
    return passTable();
}

PassInfo const* findPass(std::string const& name) {
    auto const& t = passTable();
    auto it = std::find_if(t.begin(), t.end(),
                           [&](PassInfo const& p) {
                               return p.name == name;
                           });
    return (it == t.end()) ? nullptr : &*it;
}

uint64_t optimizationLevelMask(unsigned level) {
    switch (level) {
        case 0: return 0x00;
        // -O1: "safe" passes — jump-elim + dead-code-elim.  This
        // mirrors the historical 0x05 "minimum useful optimisation"
        // setting noted in optimization_passes.md.
        case 1: return 0x01 | 0x04;
        // -O2: every defined pass.  Computed from the table so adding
        // a new pass automatically bumps -O2 to include it.  Bits
        // above 0x10 stay zero.
        case 2: {
            uint64_t m = 0;
            for (auto const& p : passTable()) m |= p.bit;
            return m;
        }
        // -O3: gcc parity; alias for 0xFF on the current binary.
        // Effectively identical to -O2 because the higher bits are
        // unused — kept distinct so user intent is preserved in
        // compile-report dumps and for forward compatibility if
        // future passes claim bits 0x20+.
        default: return 0xFF;
    }
}

void printPassTable(std::ostream& os) {
    auto const& t = passTable();
    os << "Optimisation passes (toggle with -f<name> / -fno-<name>):\n";
    for (auto const& p : t) {
        os << "  " << std::left << std::setw(18) << p.name
           << "0x" << std::hex << std::setw(4) << std::setfill(' ')
           << p.bit << std::dec << "  " << p.description << '\n';
    }
    os << "\nCurated levels:\n"
       << "  -O0  0x00  no passes\n"
       << "  -O1  0x05  jump-elim + dead-code-elim\n"
       << "  -O2  0x1F  all defined passes\n"
       << "  -O3  0xFF  gcc parity (alias for -O2 today)\n"
       << "\nIndividual toggles compose with -O<n>.  Rightmost wins on "
          "conflict (e.g. `-O0 -fdead-code-elim` ⇒ 0x04).\n";
}

void applyPassToggle(uint64_t& flags, std::string const& name, bool enable) {
    PassInfo const* p = findPass(name);
    if (p == nullptr) {
        throw std::invalid_argument(
            "seqcc: unknown -f" + std::string(enable ? "" : "no-") + name +
            " (run --print-passes for the list)");
    }
    if (enable) {
        flags |= p->bit;
    } else {
        flags &= ~p->bit;
    }
}

}  // namespace seqcc
