// =============================================================================
// seqcc — `seqdump` personality (T9).
//
// C++ port of `tests/dump_elf.py`.  Uses the same `parseElfSections`
// machinery as the seqcc dump pipeline (`elf_reader.cpp`) plus a
// dedicated section pretty-printer that mirrors `dump_elf.py`'s
// `dump_section()` heuristics — text sections rendered as text, the
// few known-binary sections (`.text`, `.linenr`, `.wf_*`, `.channels`,
// `.version_bin`) rendered with their respective struct decodings,
// everything else hex-dumped.
//
// Side-by-side `--diff` is intentionally line-oriented rather than
// structural: it shows the original-vs-recon decoded output for any
// section that differs, lifted as-is from dump_elf.py's `--both`
// flow.  This is the workflow most useful when chasing a difftest
// regression.
// =============================================================================
#include "seqdump.hpp"

#include "elf_reader.hpp"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <set>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace seqcc {

namespace {

constexpr const char* kSeqdumpVersion = "0.7.0-T9";

// --- Argument parsing ------------------------------------------------

struct SeqdumpArgs {
    std::string elfPath;
    std::string diffPath;            // empty == no diff
    std::vector<std::string> sections;
    bool all = false;
    std::size_t maxLines = 200;
    bool wantHelp = false;
    bool wantVersion = false;
};

void printUsage(std::ostream& os, std::string const& argv0) {
    os << "Usage: " << argv0 << " [OPTIONS] <elf-file>\n"
       << "       " << argv0 << " --diff <other.elf> <elf-file>\n"
       << "\n"
       << "Dump SeqC compiler ELF sections (32-bit LE format).  Mirrors\n"
       << "the layout of tests/dump_elf.py.\n"
       << "\n"
       << "Options:\n"
       << "  --section=<name>   Dump only the named section.  Repeatable.\n"
       << "                     Default: dump every section.\n"
       << "  --all              Dump every section (explicit form).\n"
       << "  --diff <other.elf> Compare against another ELF; print only\n"
       << "                     differing sections side-by-side.\n"
       << "  --max-lines=N      Truncate text-section dumps to N lines\n"
       << "                     (default 200).\n"
       << "  --help, -h         Show this message.\n"
       << "  --version          Print seqdump version.\n";
}

// gcc-style "--key=value" splitter.  Returns true and fills `value`
// if `arg` matches `--key=…`; otherwise leaves `value` untouched.
bool eatLongValue(std::string_view arg, std::string_view key,
                  std::string& value) {
    if (arg.size() < key.size() + 1) return false;
    if (arg.substr(0, key.size()) != key) return false;
    if (arg[key.size()] != '=') return false;
    value.assign(arg.begin() + key.size() + 1, arg.end());
    return true;
}

// Parse a non-negative size_t from `s`.  Returns false on overflow,
// trailing junk, or empty input.
bool parseSize(std::string const& s, std::size_t& out) {
    if (s.empty()) return false;
    std::size_t pos = 0;
    try {
        unsigned long long v = std::stoull(s, &pos, 10);
        if (pos != s.size()) return false;
        out = static_cast<std::size_t>(v);
        return true;
    } catch (...) {
        return false;
    }
}

// Parse seqdump's argv subset.  Returns 0 on success, non-zero exit
// code on user error (with a diagnostic already printed to stderr).
int parseArgs(std::vector<std::string> const& args,
              std::string const& argv0,
              SeqdumpArgs& out) {
    std::vector<std::string> positional;
    positional.reserve(args.size());

    for (std::size_t i = 0; i < args.size(); ++i) {
        std::string const& a = args[i];
        if (a == "--help" || a == "-h") {
            out.wantHelp = true;
            return 0;
        }
        if (a == "--version") {
            out.wantVersion = true;
            return 0;
        }
        if (a == "--all") {
            out.all = true;
            continue;
        }
        if (a == "--diff") {
            if (i + 1 >= args.size()) {
                std::cerr << argv0 << ": --diff requires a path argument\n";
                return 2;
            }
            out.diffPath = args[++i];
            continue;
        }
        std::string v;
        if (eatLongValue(a, "--section", v)) {
            if (v.empty()) {
                std::cerr << argv0 << ": --section= requires a name\n";
                return 2;
            }
            out.sections.push_back(std::move(v));
            continue;
        }
        if (eatLongValue(a, "--max-lines", v)) {
            std::size_t n;
            if (!parseSize(v, n)) {
                std::cerr << argv0 << ": --max-lines= expects a non-negative "
                                      "integer (got " << v << ")\n";
                return 2;
            }
            out.maxLines = n;
            continue;
        }
        if (!a.empty() && a[0] == '-') {
            std::cerr << argv0 << ": unrecognised option: " << a << "\n";
            return 2;
        }
        positional.push_back(a);
    }

    if (positional.empty()) {
        std::cerr << argv0 << ": expected an ELF file path\n";
        return 2;
    }
    if (positional.size() > 1) {
        std::cerr << argv0 << ": unexpected extra positional arguments "
                             "(use --diff to compare two ELFs)\n";
        return 2;
    }
    out.elfPath = positional.front();
    return 0;
}

// --- File I/O --------------------------------------------------------

bool readFile(std::string const& path, std::string& out) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return false;
    std::ostringstream ss;
    ss << f.rdbuf();
    out = ss.str();
    return true;
}

// --- Section-detection heuristics ------------------------------------

// `dump_elf.py` heuristic: treat the section as text if every byte in
// the first 200 is printable ASCII or a common whitespace control.
bool sectionLooksTextual(std::string_view data) {
    auto const limit = std::min<std::size_t>(data.size(), 200);
    for (std::size_t i = 0; i < limit; ++i) {
        auto b = static_cast<unsigned char>(data[i]);
        bool ok = (b >= 32 && b < 127) || b == '\n' || b == '\r' || b == '\t';
        if (!ok) return false;
    }
    return !data.empty();
}

// --- Pretty-printers -------------------------------------------------

// Read a little-endian integer from `data` at `off`.  No bounds check
// — callers compute the offset from data.size() / record size, so
// out-of-range access would be a logic bug.
std::uint32_t leU32(std::string_view data, std::size_t off) {
    std::uint32_t v;
    std::memcpy(&v, data.data() + off, sizeof(v));
    return v;
}

std::int32_t leI32(std::string_view data, std::size_t off) {
    std::int32_t v;
    std::memcpy(&v, data.data() + off, sizeof(v));
    return v;
}

std::int16_t leI16(std::string_view data, std::size_t off) {
    std::int16_t v;
    std::memcpy(&v, data.data() + off, sizeof(v));
    return v;
}

std::string hex(std::string_view data) {
    std::ostringstream ss;
    ss << std::hex << std::setfill('0');
    for (auto c : data) {
        ss << std::setw(2)
           << static_cast<unsigned>(static_cast<unsigned char>(c));
    }
    return ss.str();
}

void dumpText(std::ostream& os, std::string_view name, std::string_view data,
              std::string const& indent, std::size_t maxLines) {
    os << indent << name << ": (" << data.size() << " bytes)\n";
    std::size_t line = 0;
    std::size_t start = 0;
    std::size_t totalLines = 0;
    for (std::size_t i = 0; i < data.size(); ++i) {
        if (data[i] == '\n') ++totalLines;
    }
    if (!data.empty() && data.back() != '\n') ++totalLines;
    for (std::size_t i = 0; i <= data.size() && line < maxLines; ++i) {
        bool end = (i == data.size()) || data[i] == '\n';
        if (end) {
            os << indent << "  " << data.substr(start, i - start) << '\n';
            ++line;
            start = i + 1;
        }
    }
    if (totalLines > maxLines) {
        os << indent << "  ... (" << (totalLines - maxLines)
           << " more lines)\n";
    }
}

void dumpDotText(std::ostream& os, std::string_view name,
                 std::string_view data, std::string const& indent) {
    auto words = data.size() / 4;
    os << indent << name << ": " << data.size() << " bytes = "
       << words << " instructions\n";
    for (std::size_t i = 0; i < words; ++i) {
        os << indent << "  [" << std::setw(3) << i << "] 0x"
           << std::hex << std::setw(8) << std::setfill('0')
           << leU32(data, i * 4)
           << std::dec << std::setfill(' ') << '\n';
    }
}

void dumpDotLinenr(std::ostream& os, std::string_view name,
                   std::string_view data, std::string const& indent) {
    auto records = data.size() / 16;
    os << indent << name << ": " << data.size() << " bytes = "
       << records << " records\n";
    for (std::size_t i = 0; i < records; ++i) {
        auto base = i * 16;
        os << indent << "  abs=" << leI32(data, base)
           << " ctr=" << leI32(data, base + 4)
           << " seq=" << leI32(data, base + 8)
           << " line=" << leI32(data, base + 12) << '\n';
    }
}

void dumpDotChannels(std::ostream& os, std::string_view name,
                     std::string_view data, std::string const& indent) {
    os << indent << name << ": " << data.size() << " bytes = "
       << hex(data) << '\n';
}

void dumpDotVersionBin(std::ostream& os, std::string_view name,
                       std::string_view data, std::string const& indent) {
    os << indent << name << ": " << hex(data) << '\n';
}

void dumpDotWf(std::ostream& os, std::string_view name,
               std::string_view data, std::string const& indent) {
    auto samples = data.size() / 2;
    os << indent << name << ": " << data.size() << " bytes = "
       << samples << " samples\n";
    auto preview = std::min<std::size_t>(samples, 16);
    for (std::size_t i = 0; i < preview; ++i) {
        auto v = leI16(data, i * 2);
        auto u = static_cast<std::uint16_t>(v);
        os << indent << "  [" << std::setw(3) << i << "] "
           << std::setw(6) << v << " (0x"
           << std::hex << std::setw(4) << std::setfill('0')
           << u << std::dec << std::setfill(' ') << ")\n";
    }
    if (samples > preview) {
        os << indent << "  ...\n";
    }
}

void dumpUnknownBinary(std::ostream& os, std::string_view name,
                       std::string_view data, std::string const& indent) {
    os << indent << name << ": " << data.size() << " bytes (binary)\n";
    auto preview = data.substr(0, std::min<std::size_t>(data.size(), 64));
    os << indent << "  hex: " << hex(preview) << '\n';
}

void dumpSection(std::ostream& os, std::string_view name,
                 std::string_view data, std::string const& indent,
                 std::size_t maxLines) {
    if (name == ".text") {
        dumpDotText(os, name, data, indent);
        return;
    }
    if (name == ".linenr") {
        dumpDotLinenr(os, name, data, indent);
        return;
    }
    if (name == ".channels") {
        dumpDotChannels(os, name, data, indent);
        return;
    }
    if (name == ".version_bin") {
        dumpDotVersionBin(os, name, data, indent);
        return;
    }
    if (name.rfind(".wf_", 0) == 0) {
        dumpDotWf(os, name, data, indent);
        return;
    }
    if (sectionLooksTextual(data)) {
        dumpText(os, name, data, indent, maxLines);
        return;
    }
    dumpUnknownBinary(os, name, data, indent);
}

// --- Single-ELF dump -------------------------------------------------

// Filter & sort the section name list for output.  Names beginning
// with `__` are seqcc-internal pseudo-sections (e.g. `__e_entry__`
// in the Python tool) and are never user-visible here.
std::vector<std::string> selectSections(ElfSections const& elf,
                                        SeqdumpArgs const& args) {
    std::vector<std::string> out;
    out.reserve(elf.sections.size());
    if (!args.sections.empty()) {
        // Honour the order the user requested, but skip any name not
        // present in the ELF (printed as a diagnostic by the caller).
        for (auto const& name : args.sections) {
            if (elf.sections.find(name) != elf.sections.end()) {
                out.push_back(name);
            }
        }
        return out;
    }
    for (auto const& [name, _slice] : elf.sections) {
        if (name.empty()) continue;  // .shstrtab self-name often empty
        if (name.size() >= 2 && name[0] == '_' && name[1] == '_') continue;
        out.push_back(name);
    }
    std::sort(out.begin(), out.end());
    return out;
}

int dumpSingle(std::string const& argv0, std::string const& path,
               SeqdumpArgs const& args) {
    std::string buf;
    if (!readFile(path, buf)) {
        std::cerr << argv0 << ": cannot read " << path << '\n';
        return 1;
    }
    ElfSections elf = parseElfSections(buf);
    if (!elf.valid) {
        std::cerr << argv0 << ": " << path
                  << " is not a recognised SeqC ELF\n";
        return 1;
    }
    std::cout << "=== " << path << ": " << buf.size()
              << " bytes, e_entry=0x" << std::hex << elf.entry
              << std::dec << " ===\n\n";

    // Diagnose --section names that aren't present in the ELF.
    if (!args.sections.empty()) {
        for (auto const& name : args.sections) {
            if (elf.sections.find(name) == elf.sections.end()) {
                std::cerr << argv0 << ": warning: section " << name
                          << " not present in " << path << '\n';
            }
        }
    }

    for (auto const& name : selectSections(elf, args)) {
        std::string_view data = elf.get(name, buf);
        dumpSection(std::cout, name, data, "  ", args.maxLines);
        std::cout << '\n';
    }
    return 0;
}

// --- Diff mode -------------------------------------------------------

int dumpDiff(std::string const& argv0,
             std::string const& leftPath,
             std::string const& rightPath,
             SeqdumpArgs const& args) {
    std::string lbuf, rbuf;
    if (!readFile(leftPath, lbuf)) {
        std::cerr << argv0 << ": cannot read " << leftPath << '\n';
        return 1;
    }
    if (!readFile(rightPath, rbuf)) {
        std::cerr << argv0 << ": cannot read " << rightPath << '\n';
        return 1;
    }
    ElfSections lelf = parseElfSections(lbuf);
    ElfSections relf = parseElfSections(rbuf);
    if (!lelf.valid) {
        std::cerr << argv0 << ": " << leftPath
                  << " is not a recognised SeqC ELF\n";
        return 1;
    }
    if (!relf.valid) {
        std::cerr << argv0 << ": " << rightPath
                  << " is not a recognised SeqC ELF\n";
        return 1;
    }

    std::cout << "=== Comparing: left " << lbuf.size()
              << " bytes vs right " << rbuf.size() << " bytes ===\n";
    std::cout << "e_entry: left=0x" << std::hex << lelf.entry
              << "  right=0x" << relf.entry << std::dec << '\n';

    // Union of section names, honouring --section= filter if any.
    std::set<std::string> names;
    auto take = [&](ElfSections const& e) {
        for (auto const& [n, _s] : e.sections) {
            if (n.empty()) continue;
            if (n.size() >= 2 && n[0] == '_' && n[1] == '_') continue;
            if (!args.sections.empty() &&
                std::find(args.sections.begin(), args.sections.end(), n)
                    == args.sections.end()) {
                continue;
            }
            names.insert(n);
        }
    };
    take(lelf);
    take(relf);

    int differs = 0;
    for (auto const& name : names) {
        auto li = lelf.sections.find(name);
        auto ri = relf.sections.find(name);
        std::string_view ldata = (li != lelf.sections.end())
            ? lelf.get(name, lbuf) : std::string_view{};
        std::string_view rdata = (ri != relf.sections.end())
            ? relf.get(name, rbuf) : std::string_view{};

        if (li == lelf.sections.end()) {
            std::cout << "\n>>> " << name << ": MISSING IN LEFT (right has "
                      << rdata.size() << " bytes)\n";
            dumpSection(std::cout, name, rdata, "  right: ", args.maxLines);
            ++differs;
            continue;
        }
        if (ri == relf.sections.end()) {
            std::cout << "\n>>> " << name << ": MISSING IN RIGHT (left has "
                      << ldata.size() << " bytes)\n";
            dumpSection(std::cout, name, ldata, "  left: ", args.maxLines);
            ++differs;
            continue;
        }
        if (ldata == rdata) {
            std::cout << "\n  " << name << ": IDENTICAL (" << ldata.size()
                      << " bytes)\n";
            continue;
        }
        std::cout << "\n>>> " << name << ": DIFFERS (left=" << ldata.size()
                  << " right=" << rdata.size() << " bytes)\n";
        std::cout << "  --- LEFT (" << leftPath << ") ---\n";
        dumpSection(std::cout, name, ldata, "    ", args.maxLines);
        std::cout << "  --- RIGHT (" << rightPath << ") ---\n";
        dumpSection(std::cout, name, rdata, "    ", args.maxLines);
        ++differs;
    }
    // Summary line so callers grepping for "DIFFERS" can also pull
    // a single count out of the tail.  Exit 0 even when sections
    // differ — `seqdump --diff` is a reporting tool, not a
    // pass/fail gate.  Callers who want the gate can grep for
    // `DIFFERS` or parse the count.
    std::cout << "\n=== " << differs << " section(s) differ ===\n";
    return 0;
}

}  // namespace

int seqdumpMain(std::string const& argv0, std::vector<std::string> args) {
    SeqdumpArgs parsed;
    int rc = parseArgs(args, argv0, parsed);
    if (rc != 0) return rc;

    if (parsed.wantHelp) {
        printUsage(std::cout, argv0);
        return 0;
    }
    if (parsed.wantVersion) {
        std::cout << "seqdump " << kSeqdumpVersion << '\n';
        return 0;
    }

    if (!parsed.diffPath.empty()) {
        // `--diff <other>` + positional <elf>: by convention `--diff`
        // names the *right-hand* file and the positional is the left.
        // Same orientation as `tests/dump_elf.py --both` (orig=left).
        return dumpDiff(argv0, parsed.elfPath, parsed.diffPath, parsed);
    }
    return dumpSingle(argv0, parsed.elfPath, parsed);
}

}  // namespace seqcc
