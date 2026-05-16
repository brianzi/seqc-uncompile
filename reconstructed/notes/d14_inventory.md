# D14 Inventory: Unreconstructed `zhinst::` Surface
Generated 2026-05-12.  Source binary: `_seqc_compiler.so`.

## Status (2026-05-16)

All 14 D14 clusters have been processed.  The inventory below is
preserved as a historical reference for the as-of-2026-05-12
absent / divergent surface; per-cluster status notes have been
added in-line under each cluster header.

**Closeable work: complete.**  Remaining un-closed entries fall
into two categories:

1. **Deferred-by-design** (zero recon callers; faithful
   reconstruction would require disproportionate
   infrastructure).  Currently 2 symbols, both in
   `misc::?`: the two `getKind(...)` overloads (see IF-284).
2. **`csv_waveform_2arg::io`** (2 large CSV→Waveform
   parser symbols, 20 222 B total) — never promoted to a
   reconstruction phase because the diff-test suite has no
   case that loads CSV-shaped waveforms via the public
   binding.  Listed in the inventory but not in the active
   backlog.

All other clusters were either reconstructed
(`diagnostics_text`, `ast_misc`, `zi_environment`,
`waveform_misc`, `base64`, `compiler_helpers`,
`numeric`, `random`, `awg_config`, `device_option`,
`node_misc`, `misc::?` 3-of-5) or determined to be
auto-emitted compiler thunks (`exceptions::core`).

For the chronological narrative of how each cluster was
closed, see `OVERVIEW.md` Phase D and the F-followups
section.

## Methodology

1. **Symbol enumeration.**  All symbols defined in `_seqc_compiler.so`
   were listed via `nm --defined-only --print-size`.  Symbols whose
   mangled name begins with `_ZN6zhinst` (the project's namespace)
   were retained; all 26 142 functions visible to `objdump -d` were
   indexed for caller cross-referencing.

2. **Recon matching.**  Each binary symbol's mangled name was
   intersected with the set of mangled names defined in
   `reconstructed/build/_seqc_compiler.so`
   (`nm --defined-only`).  This is a strict byte-for-byte ABI check:
   the recon either exports the same mangled symbol or it does not.
   Of the 273 zhinst symbols in the binary not exported by the recon,
   114 have **no equivalent qualified-name match** under
   `reconstructed/src/` or `reconstructed/include/` and are reported
   as **absent** below.

3. **Signature divergence.**  62 of the 273 binary symbols have a
   matching `Class::method` qualified name in recon source but a
   different mangled name.  These are almost always the libc++
   (`std::__1`) vs libstdc++ (`std::__cxx11`) iterator/string-allocator
   ABI mismatch — the recon implements the function but mangles its
   argument types under a different STL ABI.  They are listed
   informationally in the "Divergent signatures" section, not as
   reconstruction work.

4. **Stub detection.**  Symbols defined in recon but with bodies
   shorter than ~50 bytes or containing a `// stub` / `// TODO`
   comment are listed in the "Stub-only user code" section.  The
   detection sweep flagged 38 symbols total, of which 33 are
   compiler-generated thunks (`_ZThn*_N5boost10wrapexcept...`) or
   default-synthesisable copy-assignment operators — the 5 hand-
   written stubs are listed in detail.

5. **Per-symbol enrichment.**  For every absent / stub / divergent
   symbol the report records: address (from `nm`), size in bytes,
   the first instruction at the entry point (from
   `objdump -d _seqc_compiler.so`), up to 10 callers (any function
   whose disassembly contains a `call ... <mangled>` to the symbol),
   and string evidence (any `lea <addr>(%rip)` whose target lies
   inside `.rodata` and decodes to a printable string ≥ 3 chars).

6. **libc++ caveat.**  The binary is built against libc++ (`std::__1`,
   visible in every `_ZNS_3__1...` mangling) while the recon is
   built against libstdc++.  Apparent absences of STL container
   helpers (e.g. `unordered_map` constructors) are usually ABI noise,
   not real reconstruction work.  Only symbols inside `_ZN6zhinst...`
   are surveyed in this report.
## Summary statistics
| Bucket | Count | Total size (B) |
|---|---:|---:|
| Absent (no recon match)                       | 114 | 70,690 |
| Stub-only — hand-written user code            |   5 | — |
| Stub-only — compiler-synthesised (excluded)   |  33 | — |
| Divergent signatures (informational)          |  62 | — |
| **Total binary `zhinst::` symbols not exported by recon** | **273** | — |

## Cluster index

- [`diagnostics_text::core`](#cluster-diagnostics-text-core) — 19 members, 33,844 B total — core (diagnostics / text munging)
- [`csv_waveform_2arg::io`](#cluster-csv-waveform-2arg-io) — 2 members, 20,222 B total — io (CSV → Waveform parsers)
- [`ast_misc::ast`](#cluster-ast-misc-ast) — 54 members, 8,558 B total — ast (SeqC*::clone polymorphic copies)
- [`zi_environment::io`](#cluster-zi-environment-io) — 8 members, 1,884 B total — io (Zurich Instruments runtime environment detection)
- [`waveform_misc::waveform`](#cluster-waveform-misc-waveform) — 3 members, 1,655 B total — waveform (raw-wave + JSON helpers)
- [`exceptions::core`](#cluster-exceptions-core) — 13 members, 1,173 B total — core (boost::wrapexcept thunk vtables)
- [`base64::infra`](#cluster-base64-infra) — 1 members, 910 B total — infra (base64 codec)
- [`compiler_helpers::codegen`](#cluster-compiler-helpers-codegen) — 1 members, 796 B total — codegen (helper not yet identified)
- [`misc::?`](#cluster-misc) — 5 members, 570 B total — ? (mixed)
- [`numeric::core`](#cluster-numeric-core) — 3 members, 394 B total — core (numeric helpers)
- [`random::infra`](#cluster-random-infra) — 1 members, 297 B total — infra (PRNG)
- [`awg_config::device`](#cluster-awg-config-device) — 1 members, 254 B total — device (AWG configuration helper)
- [`device_option::device`](#cluster-device-option-device) — 2 members, 90 B total — device (DeviceOption hash/compare)
- [`node_misc::core`](#cluster-node-misc-core) — 1 members, 43 B total — core (Node helper)

Plus the following separate sections:

- [Stub-only user code](#stub-only-user-code) — 5 hand-written stubs
- [Divergent signatures](#divergent-signatures-informational) — 62 STL-ABI-mismatch entries
- [Excluded from this report](#excluded-from-this-report)
- [Open questions](#open-questions)

## Cluster: `diagnostics_text::core`

### Overview

**Members:** 19  |  **Aggregate size:** 33,844 B  |  **Subsystem:** core (diagnostics / text munging)

User-facing text helpers used by error formatting, JSON / CSV / XML escaping, URL → query rewriting, filename sanitisation, unit substitution, and friend-name lookup. Many of these (e.g. `xmlUnescape`, `escapeStringForJson`, `escapeStringForCsharp`, `entityNumberToTxt`) are large (1.6–5.3 kB each) and reference distinctive string tables — XML entity tables, regex literals, JSON escape sequences. The cluster has very few internal callers (most symbols are leaves or are called by exactly one other diagnostic helper), which makes them attractive single-shot reconstruction targets: each is independent and the embedded string evidence pins the algorithm.

Reconstruction difficulty: **medium per symbol** (sizable bodies, regex usage, but isolated). Recommended approach: reconstruct one helper at a time; the regex literals in `.rodata` directly reveal the pattern they match. None of these symbols are called from the hot compile path, so omitting them does not affect difftests — they exist for downstream tooling that loads the .so for diagnostics-only purposes.

### Symbols

#### `zhinst::xmlUnescape(std::__1::basic_string<char, std::__1::char_traits<char>, std::__1::allocator<char> >&)`

- **Mangled**: `_ZN6zhinst11xmlUnescapeERNSt3__112basic_stringIcNS0_11char_traitsIcEENS0_9allocatorIcEEEE`
- **Address**: `0x2fadd0` | **Size**: 5290 B | **Status**: absent
- **First insn**: `push   %rbp`
- **Callers**: `_ZN6zhinst15xmlUnescapeCopyENSt3__112basic_stringIcNS0_11char_traitsIcEENS0_9...`
- **String evidence**: `"&amp;"`, `"Attempt to access an uninitialized boost::match_results<> class."`, `"&#x[0-9a-fA-F]+;|&#[0-9]+;"`

#### `zhinst::entityNumberToTxt(std::__1::basic_string<char, std::__1::char_traits<char>, std::__1::allocator<char> > const&)`

- **Mangled**: `_ZN6zhinst17entityNumberToTxtERKNSt3__112basic_stringIcNS0_11char_traitsIcEENS0_9allocatorIcEEEE`
- **Address**: `0x2f4e90` | **Size**: 4853 B | **Status**: absent
- **First insn**: `push   %rbp`
- **Callers**: _none found_
- **String evidence**: `"Ohm"`

#### `zhinst::linkToQuery(std::__1::basic_string<char, std::__1::char_traits<char>, std::__1::allocator<char> > const&)`

- **Mangled**: `_ZN6zhinst11linkToQueryERKNSt3__112basic_stringIcNS0_11char_traitsIcEENS0_9allocatorIcEEEE`
- **Address**: `0x2f2f20` | **Size**: 4365 B | **Status**: absent
- **First insn**: `push   %rbp`
- **Callers**: _none found_
- **String evidence**: `"%0A"`, `"%0D"`, `"%2B"`, `"%2C"`, `"%2F"`

#### `zhinst::entityNameToNumber(std::__1::basic_string<char, std::__1::char_traits<char>, std::__1::allocator<char> > const&)`

- **Mangled**: `_ZN6zhinst18entityNameToNumberERKNSt3__112basic_stringIcNS0_11char_traitsIcEENS0_9allocatorIcEEEE`
- **Address**: `0x2f4290` | **Size**: 3061 B | **Status**: absent
- **First insn**: `push   %rbp`
- **Callers**: _none found_
- **String evidence**: `"&amp;"`, `"&Omega;"`, `"&deg;"`, `"&Theta;"`, `"&plusmn;"`

#### `zhinst::escapeStringForCsharp(std::__1::basic_string<char, std::__1::char_traits<char>, std::__1::allocator<char> >)`

- **Mangled**: `_ZN6zhinst21escapeStringForCsharpENSt3__112basic_stringIcNS0_11char_traitsIcEENS0_9allocatorIcEEEE`
- **Address**: `0x2f9df0` | **Size**: 2213 B | **Status**: absent
- **First insn**: `push   %rbp`
- **Callers**: _none found_
- **String evidence**: _none observed_

#### `zhinst::sanitizeFilename(std::__1::basic_string<char, std::__1::char_traits<char>, std::__1::allocator<char> >&)`

- **Mangled**: `_ZN6zhinst16sanitizeFilenameERNSt3__112basic_stringIcNS0_11char_traitsIcEENS0_9allocatorIcEEEE`
- **Address**: `0x2fcbe0` | **Size**: 2060 B | **Status**: absent
- **First insn**: `push   %rbp`
- **Callers**: `_ZN6zhinst23sanitizeInvalidFilenameERNSt3__112basic_stringIcNS0_11char_traits...`
- **String evidence**: _none observed_

#### `zhinst::replaceUnit(std::__1::basic_string<char, std::__1::char_traits<char>, std::__1::allocator<char> > const&, std::__1::basic_string<char, std::__1::char_traits<char>, std::__1::allocator<char> > const&, std::__1::basic_string<char, std::__1::char_traits<char>, std::__1::allocator<char> > const&)`

- **Mangled**: `_ZN6zhinst11replaceUnitERKNSt3__112basic_stringIcNS0_11char_traitsIcEENS0_9allocatorIcEEEES8_S8_`
- **Address**: `0x2f7ae0` | **Size**: 1860 B | **Status**: absent
- **First insn**: `push   %rbp`
- **Callers**: _none found_
- **String evidence**: `"\E\)(.*)"`

#### `zhinst::browseTo(std::__1::basic_string<char, std::__1::char_traits<char>, std::__1::allocator<char> >)`

- **Mangled**: `_ZN6zhinst8browseToENSt3__112basic_stringIcNS0_11char_traitsIcEENS0_9allocatorIcEEEE`
- **Address**: `0x2eb950` | **Size**: 1739 B | **Status**: absent
- **First insn**: `push   %rbp`
- **Callers**: _none found_
- **String evidence**: _none observed_

#### `zhinst::escapeStringForJson(std::__1::basic_string<char, std::__1::char_traits<char>, std::__1::allocator<char> >&)`

- **Mangled**: `_ZN6zhinst19escapeStringForJsonERNSt3__112basic_stringIcNS0_11char_traitsIcEENS0_9allocatorIcEEEE`
- **Address**: `0x2f89b0` | **Size**: 1663 B | **Status**: absent
- **First insn**: `push   %rbp`
- **Callers**: _none found_
- **String evidence**: `"&((#0*34)|(#x0*22)|(quot));"`

#### `zhinst::escapeStringForPython(std::__1::basic_string<char, std::__1::char_traits<char>, std::__1::allocator<char> >)`

- **Mangled**: `_ZN6zhinst21escapeStringForPythonENSt3__112basic_stringIcNS0_11char_traitsIcEENS0_9allocatorIcEEEE`
- **Address**: `0x2f9780` | **Size**: 1644 B | **Status**: absent
- **First insn**: `push   %rbp`
- **Callers**: _none found_
- **String evidence**: _none observed_

#### `zhinst::sanitizeInvalidFilename(std::__1::basic_string<char, std::__1::char_traits<char>, std::__1::allocator<char> >&)`

- **Mangled**: `_ZN6zhinst23sanitizeInvalidFilenameERNSt3__112basic_stringIcNS0_11char_traitsIcEENS0_9allocatorIcEEEE`
- **Address**: `0x2fd3f0` | **Size**: 829 B | **Status**: absent
- **First insn**: `push   %rbp`
- **Callers**: _none found_
- **String evidence**: `"COM[1-9]|PRN"`

#### `zhinst::truncateXmlSafe(std::__1::basic_string<char, std::__1::char_traits<char>, std::__1::allocator<char> >&, unsigned long)`

- **Mangled**: `_ZN6zhinst15truncateXmlSafeERNSt3__112basic_stringIcNS0_11char_traitsIcEENS0_9allocatorIcEEEEm`
- **Address**: `0x2fc690` | **Size**: 817 B | **Status**: absent
- **First insn**: `push   %rbp`
- **Callers**: _none found_
- **String evidence**: `"Attempt to access an uninitialized boost::match_results<> class."`, `"&#x[0-9a-fA-F]+;|&#[0-9]+;|&amp;|&lt;|&gt|&quot;|&apos;"`

#### `zhinst::xmlEscapeUtf8Critical(std::__1::basic_string<char, std::__1::char_traits<char>, std::__1::allocator<char> >&)`

- **Mangled**: `_ZN6zhinst21xmlEscapeUtf8CriticalERNSt3__112basic_stringIcNS0_11char_traitsIcEENS0_9allocatorIcEEEE`
- **Address**: `0x2faaa0` | **Size**: 803 B | **Status**: absent
- **First insn**: `push   %rbp`
- **Callers**: _none found_
- **String evidence**: `"&#%03d;"`

#### `zhinst::generateSfc(std::__1::basic_string<char, std::__1::char_traits<char>, std::__1::allocator<char> > const&, std::__1::basic_string<char, std::__1::char_traits<char>, std::__1::allocator<char> > const&)`

- **Mangled**: `_ZN6zhinst11generateSfcERKNSt3__112basic_stringIcNS0_11char_traitsIcEENS0_9allocatorIcEEEES8_`
- **Address**: `0x2d10b0` | **Size**: 788 B | **Status**: absent
- **First insn**: `push   %rbp`
- **Callers**: _none found_
- **String evidence**: `"Request to generate SFC code for an unsupported device family ("`, `"/builds/labone/labone/device/types/src/device_option.cpp"`, `"sfc::FeaturesCode zhinst::generateSfc(const std::string &, const std::string &)"`

#### `zhinst::xmlEscapeCritical(std::__1::basic_string<char, std::__1::char_traits<char>, std::__1::allocator<char> >&)`

- **Mangled**: `_ZN6zhinst17xmlEscapeCriticalERNSt3__112basic_stringIcNS0_11char_traitsIcEENS0_9allocatorIcEEEE`
- **Address**: `0x2fa7e0` | **Size**: 689 B | **Status**: absent
- **First insn**: `push   %rbp`
- **Callers**: _none found_
- **String evidence**: `"&amp;"`, `"&lt;"`, `"&gt;"`, `"&(?![gl]t;|amp;|quot;|#[0-9]+;|#x[0-9a-fA-F]+;)"`

#### `zhinst::queryToLink(std::__1::basic_string<char, std::__1::char_traits<char>, std::__1::allocator<char> > const&)`

- **Mangled**: `_ZN6zhinst11queryToLinkERKNSt3__112basic_stringIcNS0_11char_traitsIcEENS0_9allocatorIcEEEE`
- **Address**: `0x2f4030` | **Size**: 596 B | **Status**: absent
- **First insn**: `push   %rbp`
- **Callers**: _none found_
- **String evidence**: _none observed_

#### `zhinst::truncateUtf8Safe(std::__1::basic_string<char, std::__1::char_traits<char>, std::__1::allocator<char> >&, unsigned long)`

- **Mangled**: `_ZN6zhinst16truncateUtf8SafeERNSt3__112basic_stringIcNS0_11char_traitsIcEENS0_9allocatorIcEEEEm`
- **Address**: `0x2fca40` | **Size**: 337 B | **Status**: absent
- **First insn**: `push   %rbp`
- **Callers**: `_ZN6zhinst15truncateXmlSafeERNSt3__112basic_stringIcNS0_11char_traitsIcEENS0_...`
- **String evidence**: _none observed_

#### `zhinst::toCheckedString(char const*)`

- **Mangled**: `_ZN6zhinst15toCheckedStringEPKc`
- **Address**: `0x2f2700` | **Size**: 179 B | **Status**: absent
- **First insn**: `push   %rbp`
- **Callers**: _none found_
- **String evidence**: _none observed_

#### `zhinst::xmlUnescapeCopy(std::__1::basic_string<char, std::__1::char_traits<char>, std::__1::allocator<char> >)`

- **Mangled**: `_ZN6zhinst15xmlUnescapeCopyENSt3__112basic_stringIcNS0_11char_traitsIcEENS0_9allocatorIcEEEE`
- **Address**: `0x2fcba0` | **Size**: 58 B | **Status**: absent
- **First insn**: `push   %rbp`
- **Callers**: _none found_
- **String evidence**: _none observed_

## Cluster: `csv_waveform_2arg::io`

### Overview

**Members:** 2  |  **Aggregate size:** 20,222 B  |  **Subsystem:** io (CSV → Waveform parsers)

Two large (10 111 byte each) `CsvParser::csvFileToWaveform` template instantiations: one for `WaveformIR`, one for `WaveformFront`. They are the two-argument overload (path + device-type) and are called from `WavetableIR::loadWaveform` / `WavetableFront::loadWaveform`. They sit alongside reconstructed multi-argument variants under `reconstructed/src/io/csv_parser*.cpp`.

Reconstruction difficulty: **large** (10 kB each — substantial logic). They open and parse a CSV file, producing samples with marker bits per the SHF/HDAWG sample format. Recommended approach: reconstruct in tandem with the existing `csvFileToWaveform` reconstructions since the templates share an underlying `_impl` body — likely most of the 10 kB is shared template code that should be hoisted into a single non-template helper.

### Symbols

#### `void zhinst::CsvParser::csvFileToWaveform<zhinst::WaveformIR>(std::__1::shared_ptr<zhinst::WaveformIR>, zhinst::AwgDeviceType)`

- **Mangled**: `_ZN6zhinst9CsvParser17csvFileToWaveformINS_10WaveformIREEEvNSt3__110shared_ptrIT_EENS_13AwgDeviceTypeE`
- **Address**: `0x2be830` | **Size**: 10111 B | **Status**: absent
- **First insn**: `push   %rbp`
- **Callers**: `_ZN6zhinst11WavetableIR12loadWaveformENSt3__110shared_ptrINS_10WaveformIREEE`
- **String evidence**: _none observed_

#### `void zhinst::CsvParser::csvFileToWaveform<zhinst::WaveformFront>(std::__1::shared_ptr<zhinst::WaveformFront>, zhinst::AwgDeviceType)`

- **Mangled**: `_ZN6zhinst9CsvParser17csvFileToWaveformINS_13WaveformFrontEEEvNSt3__110shared_ptrIT_EENS_13AwgDeviceTypeE`
- **Address**: `0x2ba8b0` | **Size**: 10111 B | **Status**: absent
- **First insn**: `push   %rbp`
- **Callers**: `_ZN6zhinst14WavetableFront12loadWaveformENSt3__110shared_ptrINS_13WaveformFro...`
- **String evidence**: _none observed_

## Cluster: `ast_misc::ast`

### Overview

**Members:** 54  |  **Aggregate size:** 8,558 B  |  **Subsystem:** ast (SeqC*::clone polymorphic copies)

Polymorphic `clone()` overrides for 53 `SeqC*` AST node subclasses (plus `SeqCRepeat::cond()` getter). Each clone() override is a few hundred bytes — it allocates a `shared_ptr` of the same subclass and copy-constructs from `*this`. There is exactly one such override per AST node type. They are pure boilerplate but cannot be elided because the binary's vtables reference them and the recon's vtable layout must match for shared-pointer-of-base operations.

Reconstruction difficulty: **trivial per symbol, mechanical at scale**. Recommended approach: a single header macro / template that emits the override for each subclass in one line, then verify the vtable slot ordering against the binary. The bodies themselves disassemble to nearly identical sequences (`make_shared<T>(*this)`) parameterised only on T.

### Symbols

Of the 54 members, 53 are `SeqC*::clone() const` polymorphic copy constructors and 1 is `SeqCRepeat::cond() const`.  All 53 clone overrides have the identical shape (`push %rbp; ... call operator new; ret`) — only the placement-`new` typeid differs.  Condensed table:

| Subclass | Mangled | Addr | Size | First insn |
|---|---|---|---:|---|
| `SeqCIfElse` | `_ZNK6zhinst10SeqCIfElse5cloneEv` | `0x2021a0` | 278 | `push   %rbp` |
| `SeqCCondExpr` | `_ZNK6zhinst12SeqCCondExpr5cloneEv` | `0x203f50` | 278 | `push   %rbp` |
| `SeqCAssign` | `_ZNK6zhinst10SeqCAssign5cloneEv` | `0x2082b0` | 212 | `push   %rbp` |
| `SeqCGEqual` | `_ZNK6zhinst10SeqCGEqual5cloneEv` | `0x206910` | 212 | `push   %rbp` |
| `SeqCLEqual` | `_ZNK6zhinst10SeqCLEqual5cloneEv` | `0x206680` | 212 | `push   %rbp` |
| `SeqCLogAnd` | `_ZNK6zhinst10SeqCLogAnd5cloneEv` | `0x207d90` | 212 | `push   %rbp` |
| `SeqCNEqual` | `_ZNK6zhinst10SeqCNEqual5cloneEv` | `0x206e30` | 212 | `push   %rbp` |
| `SeqCOrExpr` | `_ZNK6zhinst10SeqCOrExpr5cloneEv` | `0x207870` | 212 | `push   %rbp` |
| `SeqCShiftL` | `_ZNK6zhinst10SeqCShiftL5cloneEv` | `0x205ed0` | 212 | `push   %rbp` |
| `SeqCShiftR` | `_ZNK6zhinst10SeqCShiftR5cloneEv` | `0x205c40` | 212 | `push   %rbp` |
| `SeqCAndExpr` | `_ZNK6zhinst11SeqCAndExpr5cloneEv` | `0x2075e0` | 212 | `push   %rbp` |
| `SeqCGreater` | `_ZNK6zhinst11SeqCGreater5cloneEv` | `0x206160` | 212 | `push   %rbp` |
| `SeqCXorExpr` | `_ZNK6zhinst11SeqCXorExpr5cloneEv` | `0x207b00` | 212 | `push   %rbp` |
| `SeqCDec` | `_ZNK6zhinst7SeqCDec5cloneEv` | `0x207350` | 212 | `push   %rbp` |
| `SeqCDiv` | `_ZNK6zhinst7SeqCDiv5cloneEv` | `0x205720` | 212 | `push   %rbp` |
| `SeqCInc` | `_ZNK6zhinst7SeqCInc5cloneEv` | `0x2070c0` | 212 | `push   %rbp` |
| `SeqCMod` | `_ZNK6zhinst7SeqCMod5cloneEv` | `0x2059b0` | 212 | `push   %rbp` |
| `SeqCMult` | `_ZNK6zhinst8SeqCMult5cloneEv` | `0x205490` | 212 | `push   %rbp` |
| `SeqCNoOp` | `_ZNK6zhinst8SeqCNoOp5cloneEv` | `0x208590` | 212 | `push   %rbp` |
| `SeqCPlus` | `_ZNK6zhinst8SeqCPlus5cloneEv` | `0x204f70` | 212 | `push   %rbp` |
| `SeqCEqual` | `_ZNK6zhinst9SeqCEqual5cloneEv` | `0x206ba0` | 212 | `push   %rbp` |
| `SeqCLogOr` | `_ZNK6zhinst9SeqCLogOr5cloneEv` | `0x208020` | 212 | `push   %rbp` |
| `SeqCLower` | `_ZNK6zhinst9SeqCLower5cloneEv` | `0x2063f0` | 212 | `push   %rbp` |
| `SeqCMinus` | `_ZNK6zhinst9SeqCMinus5cloneEv` | `0x205200` | 212 | `push   %rbp` |
| `SeqCRepeat` | `_ZNK6zhinst10SeqCRepeat5cloneEv` | `0x203a60` | 202 | `push   %rbp` |
| `SeqCDoWhile` | `_ZNK6zhinst11SeqCDoWhile5cloneEv` | `0x2036a0` | 202 | `push   %rbp` |
| `SeqCOperator` | `_ZNK6zhinst12SeqCOperator5cloneEv` | `0x1ff1a0` | 202 | `push   %rbp` |
| `SeqCCaseEntry` | `_ZNK6zhinst13SeqCCaseEntry5cloneEv` | `0x202a20` | 202 | `push   %rbp` |
| `SeqCWhileLoop` | `_ZNK6zhinst13SeqCWhileLoop5cloneEv` | `0x2032e0` | 202 | `push   %rbp` |
| `SeqCSwitchCase` | `_ZNK6zhinst14SeqCSwitchCase5cloneEv` | `0x2025d0` | 202 | `push   %rbp` |
| `SeqCIfCondition` | `_ZNK6zhinst15SeqCIfCondition5cloneEv` | `0x201cb0` | 202 | `push   %rbp` |
| `SeqCValue` | `_ZNK6zhinst9SeqCValue5cloneEv` | `0x1fe560` | 165 | `push   %rbp` |
| `SeqCVariable` | `_ZNK6zhinst12SeqCVariable5cloneEv` | `0x1fe160` | 137 | `push   %rbp` |
| `SeqCNotExpr` | `_ZNK6zhinst11SeqCNotExpr5cloneEv` | `0x204c60` | 120 | `push   %rbp` |
| `SeqCReturnStatement` | `_ZNK6zhinst19SeqCReturnStatement5cloneEv` | `0x204360` | 120 | `push   %rbp` |
| `SeqCInv` | `_ZNK6zhinst7SeqCInv5cloneEv` | `0x204a20` | 120 | `push   %rbp` |
| `SeqCNeg` | `_ZNK6zhinst7SeqCNeg5cloneEv` | `0x2045a0` | 120 | `push   %rbp` |
| `SeqCPos` | `_ZNK6zhinst7SeqCPos5cloneEv` | `0x2047e0` | 120 | `push   %rbp` |
| `SeqCArgList` | `_ZNK6zhinst11SeqCArgList5cloneEv` | `0x1ffa00` | 81 | `push   %rbp` |
| `SeqCForLoop` | `_ZNK6zhinst11SeqCForLoop5cloneEv` | `0x202f70` | 81 | `push   %rbp` |
| `SeqCDeclList` | `_ZNK6zhinst12SeqCDeclList5cloneEv` | `0x2002f0` | 81 | `push   %rbp` |
| `SeqCFunction` | `_ZNK6zhinst12SeqCFunction5cloneEv` | `0x1feb60` | 81 | `push   %rbp` |
| `SeqCStmtList` | `_ZNK6zhinst12SeqCStmtList5cloneEv` | `0x201920` | 81 | `push   %rbp` |
| `SeqCParamList` | `_ZNK6zhinst13SeqCParamList5cloneEv` | `0x200d10` | 81 | `push   %rbp` |
| `SeqCFunctionCall` | `_ZNK6zhinst16SeqCFunctionCall5cloneEv` | `0x1feea0` | 81 | `push   %rbp` |
| `SeqCArray` | `_ZNK6zhinst9SeqCArray5cloneEv` | `0x1ff530` | 81 | `push   %rbp` |
| `SeqCCommand` | `_ZNK6zhinst11SeqCCommand5cloneEv` | `0x1fe660` | 52 | `push   %rbp` |
| `SeqCOperation` | `_ZNK6zhinst13SeqCOperation5cloneEv` | `0x1fdb00` | 52 | `push   %rbp` |
| `SeqCVariableType` | `_ZNK6zhinst16SeqCVariableType5cloneEv` | `0x1fe6f0` | 52 | `push   %rbp` |
| `SeqCBreakStatement` | `_ZNK6zhinst18SeqCBreakStatement5cloneEv` | `0x2041e0` | 52 | `push   %rbp` |
| `SeqCContinueStatement` | `_ZNK6zhinst21SeqCContinueStatement5cloneEv` | `0x204150` | 52 | `push   %rbp` |
| `SeqCLabel` | `_ZNK6zhinst9SeqCLabel5cloneEv` | `0x2019f0` | 52 | `push   %rbp` |
| `SeqCNoCmd` | `_ZNK6zhinst9SeqCNoCmd5cloneEv` | `0x204d80` | 52 | `push   %rbp` |
| `SeqCRepeat` | `_ZNK6zhinst10SeqCRepeat4condEv` | `0x203b80` | 10 | `push   %rbp` |

All 54 of these symbols are called by `make_shared<SeqC...>` and by the AST traversal code (each clone has 1–3 callers — typically `cloneNode` or a specific transform pass).  None reference distinctive `.rodata` strings.

## Cluster: `zi_environment::io`

### Overview

**Members:** 8  |  **Aggregate size:** 1,884 B  |  **Subsystem:** io (Zurich Instruments runtime environment detection)

Zurich Instruments runtime environment detection. Helpers that resolve the LabOne installation directory, the user's Zurich Instruments folder, the device-type executing the compiler (MF/HF/UHF/SHF/HDAWG/etc.), and per-platform path conventions. `runningOnMfDevice()` (currently a recon stub returning `false`) is the anchor symbol — IF-253 records that several waveform / ELF paths take a different branch when the compiler is hosted on an MF device.

Reconstruction difficulty: **small to medium** — bodies are short (most under 300 bytes), but they touch platform / filesystem boundaries that need careful host-side stubbing for tests. Recommended approach: reconstruct `runningOnMfDevice` first (tiny TLS-cached function reading a config file or environment variable), then the path/folder helpers that depend on it.

### Symbols

#### `zhinst::hasMediaDevNode(std::__1::basic_string<char, std::__1::char_traits<char>, std::__1::allocator<char> > const&)`

- **Mangled**: `_ZN6zhinst15hasMediaDevNodeERKNSt3__112basic_stringIcNS0_11char_traitsIcEENS0_9allocatorIcEEEE`
- **Address**: `0x2eb550` | **Size**: 770 B | **Status**: absent
- **First insn**: `push   %rbp`
- **Callers**: _none found_
- **String evidence**: `"^/media/sd[a-z][0-9]+$"`

#### `zhinst::makeDirectories(boost::filesystem::path const&)`

- **Mangled**: `_ZN6zhinst15makeDirectoriesERKN5boost10filesystem4pathE`
- **Address**: `0x2cdef0` | **Size**: 583 B | **Status**: absent
- **First insn**: `push   %rbp`
- **Callers**: _none found_
- **String evidence**: `"Could not access directory '"`, `"/builds/labone/labone/utils/filesystem/src/zi_folder.cpp"`, `"void zhinst::makeDirectories(const fs::path &)"`, `"Could not create directory '"`

#### `zhinst::canCreateFileForWriting(boost::filesystem::path const&)`

- **Mangled**: `_ZN6zhinst23canCreateFileForWritingERKN5boost10filesystem4pathE`
- **Address**: `0x2eb860` | **Size**: 221 B | **Status**: absent
- **First insn**: `push   %rbp`
- **Callers**: `_ZN6zhinst20isDirectoryWriteableERKN5boost10filesystem4pathE`
- **String evidence**: _none observed_

#### `zhinst::runningOnMfDevice(std::__1::basic_string<char, std::__1::char_traits<char>, std::__1::allocator<char> > const&)`

- **Mangled**: `_ZN6zhinst17runningOnMfDeviceERKNSt3__112basic_stringIcNS0_11char_traitsIcEENS0_9allocatorIcEEEE`
- **Address**: `0x2ec160` | **Size**: 120 B | **Status**: absent
- **First insn**: `push   %rbp`
- **Callers**: _none found_
- **String evidence**: _none observed_

#### `zhinst::runningOnMf64Device(std::__1::basic_string<char, std::__1::char_traits<char>, std::__1::allocator<char> > const&)`

- **Mangled**: `_ZN6zhinst19runningOnMf64DeviceERKNSt3__112basic_stringIcNS0_11char_traitsIcEENS0_9allocatorIcEEEE`
- **Address**: `0x2ec3d0` | **Size**: 95 B | **Status**: absent
- **First insn**: `push   %rbp`
- **Callers**: _none found_
- **String evidence**: _none observed_

#### `zhinst::runningOnMf64Device()`

- **Mangled**: `_ZN6zhinst19runningOnMf64DeviceEv`
- **Address**: `0x2ec680` | **Size**: 83 B | **Status**: absent
- **First insn**: `movzbl 0x898c21(%rip),%eax        # b852a8 <_ZGVZN6zhinst19runningOnMf64DeviceEvE13runningOnMf64>`
- **Callers**: _none found_
- **String evidence**: _none observed_

#### `zhinst::markFileHidden(boost::filesystem::path const&)`

- **Mangled**: `_ZN6zhinst14markFileHiddenERKN5boost10filesystem4pathE`
- **Address**: `0x2eb940` | **Size**: 6 B | **Status**: absent
- **First insn**: `push   %rbp`
- **Callers**: _none found_
- **String evidence**: _none observed_

#### `zhinst::initBoostFilesystemForUnicode()`

- **Mangled**: `_ZN6zhinst29initBoostFilesystemForUnicodeEv`
- **Address**: `0x2ec020` | **Size**: 6 B | **Status**: absent
- **First insn**: `push   %rbp`
- **Callers**: _none found_
- **String evidence**: _none observed_

## Cluster: `waveform_misc::waveform`

### Overview

**Members:** 3  |  **Aggregate size:** 1,655 B  |  **Subsystem:** waveform (raw-wave + JSON helpers)

Three remaining waveform helpers: a `RawWaveHirzel16` ctor (650 B) that builds a Hirzel-format raw-wave from doubles + markers, plus two small adapters. Tightly coupled to existing `RawWaveHirzel*` reconstructions; likely a missing constructor overload that the multi-arg base path doesn't currently exercise.

Reconstruction difficulty: **small**. Recommended approach: read the existing `RawWaveHirzel16` body in `reconstructed/src/waveform/`, identify the missing overload signature, port the binary's body.

### Symbols

#### `zhinst::Waveform::File::typeToStr(zhinst::Waveform::File::Type)`

- **Mangled**: `_ZN6zhinst8Waveform4File9typeToStrENS1_4TypeE`
- **Address**: `0x2a3a90` | **Size**: 932 B | **Status**: absent
- **First insn**: `push   %rbp`
- **Callers**: `_ZNK6zhinst8Waveform6toJsonEv`
- **String evidence**: `"unordered_map::at: key not found"`, `"csv"`, `"raw"`, `"gen"`
- **Notes**: candidate qualified-name match in reconstructed/src/waveform/waveform.cpp — verify whether the recon implements the same overload (likely signature-divergent rather than truly absent)

#### `zhinst::Waveform::File::typeFromStr(std::__1::basic_string<char, std::__1::char_traits<char>, std::__1::allocator<char> >)`

- **Mangled**: `_ZN6zhinst8Waveform4File11typeFromStrENSt3__112basic_stringIcNS2_11char_traitsIcEENS2_9allocatorIcEEEE`
- **Address**: `0x2a63c0` | **Size**: 528 B | **Status**: absent
- **First insn**: `push   %rbp`
- **Callers**: `_ZN6zhinst8Waveform8fromJsonERKN5boost4json5valueERKNS_15DeviceConstantsE`
- **String evidence**: `"csv"`, `"raw"`, `"gen"`, `"unordered_map::at: key not found"`
- **Notes**: candidate qualified-name match in reconstructed/src/waveform/waveform.cpp — verify whether the recon implements the same overload (likely signature-divergent rather than truly absent)

#### `zhinst::Waveform::File::operator==(zhinst::Waveform::File const&) const`

- **Mangled**: `_ZNK6zhinst8Waveform4FileeqERKS1_`
- **Address**: `0x2a9680` | **Size**: 195 B | **Status**: absent
- **First insn**: `movzbl (%rdi),%eax`
- **Callers**: `_ZNK6zhinst8WaveformeqERKS0_`
- **String evidence**: _none observed_
- **Notes**: candidate qualified-name match in reconstructed/src/waveform/waveform.cpp — verify whether the recon implements the same overload (likely signature-divergent rather than truly absent)

## Cluster: `exceptions::core`

### Overview

**Members:** 13  |  **Aggregate size:** 1,173 B  |  **Subsystem:** core (boost::wrapexcept thunk vtables)

Thirteen `_ZThn*_N5boost10wrapexceptIN6zhinst...EED{0,1}Ev` thunks — virtual-base offset thunks for `boost::wrapexcept<zhinst::Exception>` and friends. These are compiler-generated MI thunks that adjust `this` by 8/16/40 bytes before calling the real destructor. They appear absent because the recon uses single inheritance (or a different ABI) and the compiler does not need to emit the offset thunks.

Reconstruction difficulty: **trivial / no source change**. These will be auto-emitted once the recon's exception class hierarchy matches the binary's multiple-inheritance shape. No hand-written code is required; this cluster is informational only.

### Symbols

#### `zhinst::ZIIOException::ZIIOException(std::__1::basic_string<char, std::__1::char_traits<char>, std::__1::allocator<char> >, ZIResult_enum)`

- **Mangled**: `_ZN6zhinst13ZIIOExceptionC1ENSt3__112basic_stringIcNS1_11char_traitsIcEENS1_9allocatorIcEEEE13ZIResult_enum`
- **Address**: `0x2e5be0` | **Size**: 175 B | **Status**: absent
- **First insn**: `push   %rbp`
- **Callers**: _none found_
- **String evidence**: _none observed_

#### `zhinst::ZIIOException::ZIIOException(std::__1::basic_string<char, std::__1::char_traits<char>, std::__1::allocator<char> >, ZIResult_enum)`

- **Mangled**: `_ZN6zhinst13ZIIOExceptionC2ENSt3__112basic_stringIcNS1_11char_traitsIcEENS1_9allocatorIcEEEE13ZIResult_enum`
- **Address**: `0x2e5be0` | **Size**: 175 B | **Status**: absent
- **First insn**: `(not seen in disasm)`
- **Callers**: _none found_
- **String evidence**: _none observed_

#### `zhinst::ZIAPIException::ZIAPIException(std::__1::basic_string<char, std::__1::char_traits<char>, std::__1::allocator<char> >, boost::system::error_code)`

- **Mangled**: `_ZN6zhinst14ZIAPIExceptionC1ENSt3__112basic_stringIcNS1_11char_traitsIcEENS1_9allocatorIcEEEEN5boost6system10error_codeE`
- **Address**: `0x2e59a0` | **Size**: 139 B | **Status**: absent
- **First insn**: `push   %rbp`
- **Callers**: _none found_
- **String evidence**: _none observed_

#### `zhinst::ZIAPIException::ZIAPIException(std::__1::basic_string<char, std::__1::char_traits<char>, std::__1::allocator<char> >, boost::system::error_code)`

- **Mangled**: `_ZN6zhinst14ZIAPIExceptionC2ENSt3__112basic_stringIcNS1_11char_traitsIcEENS1_9allocatorIcEEEEN5boost6system10error_codeE`
- **Address**: `0x2e59a0` | **Size**: 139 B | **Status**: absent
- **First insn**: `(not seen in disasm)`
- **Callers**: _none found_
- **String evidence**: _none observed_

#### `zhinst::ErrorCodeTraits<boost::system::error_code>::asException(zhinst::GenericErrorDescription<boost::system::error_code>)`

- **Mangled**: `_ZN6zhinst15ErrorCodeTraitsIN5boost6system10error_codeEE11asExceptionENS_23GenericErrorDescriptionIS3_EE`
- **Address**: `0x2ea190` | **Size**: 134 B | **Status**: absent
- **First insn**: `push   %rbp`
- **Callers**: _none found_
- **String evidence**: _none observed_
- **Notes**: candidate qualified-name match in reconstructed/include/zhinst/core/exception.hpp — verify whether the recon implements the same overload (likely signature-divergent rather than truly absent)

#### `zhinst::special::toApiCode(zhinst::Exception const&)`

- **Mangled**: `_ZN6zhinst7special9toApiCodeERKNS_9ExceptionE`
- **Address**: `0x2e7690` | **Size**: 132 B | **Status**: absent
- **First insn**: `push   %rbp`
- **Callers**: _none found_
- **String evidence**: _none observed_

#### `zhinst::isApiError(boost::system::error_code const&)`

- **Mangled**: `_ZN6zhinst10isApiErrorERKN5boost6system10error_codeE`
- **Address**: `0x2e4490` | **Size**: 81 B | **Status**: absent
- **First insn**: `push   %rbp`
- **Callers**: `_ZN6zhinst7special9toApiCodeERKNS_9ExceptionE`
- **String evidence**: _none observed_

#### `zhinst::isApiError(zhinst::RemoteErrorCode const&)`

- **Mangled**: `_ZN6zhinst10isApiErrorERKNS_15RemoteErrorCodeE`
- **Address**: `0x2e44f0` | **Size**: 74 B | **Status**: absent
- **First insn**: `movzbl (%rdi),%eax`
- **Callers**: _none found_
- **String evidence**: _none observed_

#### `zhinst::toApiCode(zhinst::ErrorKind)`

- **Mangled**: `_ZN6zhinst9toApiCodeENS_9ErrorKindE`
- **Address**: `0x2e5280` | **Size**: 41 B | **Status**: absent
- **First insn**: `push   %rbp`
- **Callers**: `_ZN6zhinst7special9toApiCodeERKNS_9ExceptionE`
- **String evidence**: _none observed_

#### `zhinst::getApiErrorBase(ZIResult_enum)`

- **Mangled**: `_ZN6zhinst15getApiErrorBaseE13ZIResult_enum`
- **Address**: `0x2e4980` | **Size**: 33 B | **Status**: absent
- **First insn**: `push   %rbp`
- **Callers**: _none found_
- **String evidence**: _none observed_

#### `zhinst::toZiErrorKind(zhinst::ErrorKind)`

- **Mangled**: `_ZN6zhinst13toZiErrorKindENS_9ErrorKindE`
- **Address**: `0x2e5240` | **Size**: 18 B | **Status**: absent
- **First insn**: `push   %rbp`
- **Callers**: _none found_
- **String evidence**: _none observed_

#### `zhinst::fromZiErrorKind(ZIErrorKind_enum)`

- **Mangled**: `_ZN6zhinst15fromZiErrorKindE16ZIErrorKind_enum`
- **Address**: `0x2e5260` | **Size**: 17 B | **Status**: absent
- **First insn**: `push   %rbp`
- **Callers**: _none found_
- **String evidence**: _none observed_

#### `zhinst::make_error_condition(zhinst::ErrorKind)`

- **Mangled**: `_ZN6zhinst20make_error_conditionENS_9ErrorKindE`
- **Address**: `0x2e50c0` | **Size**: 15 B | **Status**: absent
- **First insn**: `push   %rbp`
- **Callers**: _none found_
- **String evidence**: _none observed_

## Cluster: `base64::infra`

### Overview

**Members:** 1  |  **Aggregate size:** 910 B  |  **Subsystem:** infra (base64 codec)

Single 910 B function `zhinst::base64Encode(unsigned char const*, unsigned long)` returning a `std::string`. Standalone leaf utility; the recon does not currently expose a base64 codec.

Reconstruction difficulty: **trivial**. Recommended approach: standard 3-byte → 4-character base64 encoder; the binary almost certainly uses the canonical alphabet — confirm via the lookup table in `.rodata` near the function.

### Symbols

#### `zhinst::base64::encode(std::__1::span<unsigned char const, 18446744073709551615ul>)`

- **Mangled**: `_ZN6zhinst6base646encodeENSt3__14spanIKhLm18446744073709551615EEE`
- **Address**: `0x2f8620` | **Size**: 910 B | **Status**: absent
- **First insn**: `push   %rbp`
- **Callers**: _none found_
- **String evidence**: `"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/"`

## Cluster: `compiler_helpers::codegen`

### Overview

**Members:** 1  |  **Aggregate size:** 796 B  |  **Subsystem:** codegen (helper not yet identified)

Single 796 B helper `zhinst::compileWithRetry` (or similar — exact signature in member list). Called from a code-generation entry point.

Reconstruction difficulty: **small**. Recommended approach: read disassembly + caller context to determine retry policy.

### Symbols

#### `zhinst::AWGCompilerImpl::nodeListToJson(std::__1::vector<zhinst::NodeMapItem, std::__1::allocator<zhinst::NodeMapItem> > const&, std::__1::unordered_map<zhinst::NodeMapItem, std::__1::set<zhinst::AccessMode, std::__1::less<zhinst::AccessMode>, std::__1::allocator<zhinst::AccessMode> >, std::__1::hash<zhinst::NodeMapItem>, std::__1::equal_to<zhinst::NodeMapItem>, std::__1::allocator<std::__1::pair<zhinst::NodeMapItem const, std::__1::set<zhinst::AccessMode, std::__1::less<zhinst::AccessMode>, std::__1::allocator<zhinst::AccessMode> > > > > const&) const`

- **Mangled**: `_ZNK6zhinst15AWGCompilerImpl14nodeListToJsonERKNSt3__16vectorINS_11NodeMapItemENS1_9allocatorIS3_EEEERKNS1_13unordered_mapIS3_NS1_3setINS_10AccessModeENS1_4lessISB_EENS4_ISB_EEEENS1_4hashIS3_EENS1_8equal_toIS3_EENS4_INS1_4pairIKS3_SF_EEEEEE`
- **Address**: `0x1088d0` | **Size**: 796 B | **Status**: absent
- **First insn**: `push   %rbp`
- **Callers**: `_ZN6zhinst15AWGCompilerImpl13writeToStreamERNSt3__113basic_ostreamIcNS1_11cha...`
- **String evidence**: `".soft"`, `"modes"`, `"nodes"`, `"array::at"`, `"unordered_map::at: key not found"`

## Cluster: `misc::?`

### Overview

**Members:** 5  |  **Aggregate size:** 570 B  |  **Subsystem:** ? (mixed)

Five symbols that didn't cluster well: a mix of small leaf helpers from different subsystems. Triage individually.

**F-followup status (2026-05-16)**: 3 of 5 closed; 2
deferred-by-design.  See `OVERVIEW.md` chronology and IF-283
/ IF-284 for the rationale.

### Symbols

#### `zhinst::getKind(zhinst::Exception const&)`

- **Mangled**: `_ZN6zhinst7getKindERKNS_9ExceptionE`
- **Address**: `0x2e5180` | **Size**: 189 B | **Status**: deferred-by-design (IF-284)
- **First insn**: `push   %rbp`
- **Callers**: _none found_
- **String evidence**: _none observed_
- **Why deferred**: zero recon callers; faithful body needs
  `dynamic_cast` over `boost::wrapexcept<...>` thunks plus
  tail-call into the `error_code` overload below.  Cost
  greatly outweighs value while no caller exists.

#### `zhinst::getKind(boost::system::error_code const&)`

- **Mangled**: `_ZN6zhinst7getKindERKN5boost6system10error_codeE`
- **Address**: `0x2e50d0` | **Size**: 170 B | **Status**: deferred-by-design (IF-284)
- **First insn**: `mov    0x10(%rdi),%rax`
- **Callers**: `_ZN6zhinst7special9toApiCodeERKNS_9ExceptionE`
- **String evidence**: _none observed_
- **Why deferred**: needs real `boost::system::error_category*`
  comparison against an anon-namespace `singleErrorKindCategory`
  (binary @0xb7c5a8) plus `boost::system::detail::generic_cat_holder`
  interop.  Recon currently uses a stand-in `ErrorCode` (not
  `boost::system::error_code`); building out the interop would
  add ~200 LoC for zero in-tree callers.

#### `TLS init function for zhinst::GlobalResources::random`

- **Mangled**: `_ZTHN6zhinst15GlobalResources6randomE`
- **Address**: `0x1f6090` | **Size**: 164 B | **Status**: present (IF-283)
- **First insn**: `push   %rbp`
- **Callers**: `_ZN6zhinst17WaveformGenerator13randomUniformERKNSt3__16vectorINS_5ValueENS1_9...`, `_ZN6zhinst15GlobalResourcesC2ERKNSt3__110shared_ptrINS_9ResourcesEEE`, `_ZN6zhinst17WaveformGenerator11randomGaussERKNSt3__16vectorINS_5ValueENS1_9al...`, `_ZN6zhinst15CustomFunctions10randomSeedERKNSt3__16vectorINS_15EvalResultValue...`
- **String evidence**: _none observed_
- **Recon**: gcc-emitted from
  `thread_local std::array<uint64_t, 313> GlobalResources::random
   = seed_mt19937_64_state();` in
  `src/runtime/global_resources.cpp`.  Matches binary
  semantics (once-per-thread MT19937-64 seeding); fixed
  IF-283 (per-construction re-seed regression).

#### `zhinst::ErrorCodeTraits<boost::system::error_code>::defaultMessage(boost::system::error_code const&)`

- **Mangled**: `_ZN6zhinst15ErrorCodeTraitsIN5boost6system10error_codeEE14defaultMessageERKS3_`
- **Address**: `0x2ea170` | **Size**: 24 B | **Status**: present (template-arg mangling diverges)
- **First insn**: `push   %rbp`
- **Callers**: `_ZN6zhinst9ExceptionC1ERKN5boost6system10error_codeENSt3__112basic_stringIcNS...`, `_ZN6zhinst9ExceptionC1ENSt3__112basic_stringIcNS1_11char_traitsIcEENS1_9alloc...`, `_ZN6zhinst9ExceptionC1ERKN5boost6system10error_codeE`
- **String evidence**: _none observed_
- **Recon**: out-of-line specialisation in
  `src/core/exception.cpp` for
  `ErrorCodeTraits<ErrorCode>` (recon stand-in for
  `boost::system::error_code`).  Body matches `objdump`
  byte-for-byte; mangled name diverges only in the
  template-arg portion (`IN6zhinst9ErrorCodeE` vs
  `IN5boost6system10error_codeE`).  Faithful name match
  would require faking `boost::system::error_code` — out
  of scope.

#### `zhinst::ErrorCodeTraits<boost::system::error_code>::successCode()`

- **Mangled**: `_ZN6zhinst15ErrorCodeTraitsIN5boost6system10error_codeEE11successCodeEv`
- **Address**: `0x2ea150` | **Size**: 23 B | **Status**: present (template-arg mangling diverges)
- **First insn**: `push   %rbp`
- **Callers**: _none found_
- **String evidence**: _none observed_
- **Recon**: same approach as `defaultMessage` above;
  template-arg portion of mangling diverges by design.

## Cluster: `numeric::core`

### Overview

**Members:** 3  |  **Aggregate size:** 394 B  |  **Subsystem:** core (numeric helpers)

Three small numeric helpers (under 300 B each). Likely fixed-point conversion or rounding utilities used by waveform / asm code paths.

Reconstruction difficulty: **trivial**. Reconstruct after consulting callers.

### Symbols

#### `zhinst::almostEqual(double, double)`

- **Mangled**: `_ZN6zhinst11almostEqualEdd`
- **Address**: `0x2ec070` | **Size**: 234 B | **Status**: absent
- **First insn**: `ucomisd %xmm1,%xmm0`
- **Callers**: _none found_
- **String evidence**: _none observed_

#### `zhinst::toRawByteArray(std::__1::basic_string_view<char, std::__1::char_traits<char> >, std::__1::span<unsigned char, 18446744073709551615ul>)`

- **Mangled**: `_ZN6zhinst14toRawByteArrayENSt3__117basic_string_viewIcNS0_11char_traitsIcEEEENS0_4spanIhLm18446744073709551615EEE`
- **Address**: `0x2f27c0` | **Size**: 100 B | **Status**: absent
- **First insn**: `test   %rcx,%rcx`
- **Callers**: _none found_
- **String evidence**: _none observed_

#### `zhinst::fromRawByteArray(std::__1::span<unsigned char const, 18446744073709551615ul>)`

- **Mangled**: `_ZN6zhinst16fromRawByteArrayENSt3__14spanIKhLm18446744073709551615EEE`
- **Address**: `0x2f2830` | **Size**: 60 B | **Status**: absent
- **First insn**: `mov    %rdi,%rax`
- **Callers**: _none found_
- **String evidence**: _none observed_

## Cluster: `random::infra`

### Overview

**Members:** 1  |  **Aggregate size:** 297 B  |  **Subsystem:** infra (PRNG)

Single 297 B `Random::seedRandom()` (or similar). Recon already has `seedRandom` mentioned in `runtime/custom_functions_playback.cpp` and `infra/prng_libcxx.cpp` — this absent symbol is likely a different overload or a thread-local TLS initializer.

Reconstruction difficulty: **trivial**. Cross-check with existing PRNG reconstruction.

### Symbols

#### `zhinst::Random::seedRandom()`

- **Mangled**: `_ZN6zhinst6Random10seedRandomEv`
- **Address**: `0x16be80` | **Size**: 297 B | **Status**: absent
- **First insn**: `push   %rbp`
- **Callers**: `_ZN6zhinst15CustomFunctions10randomSeedERKNSt3__16vectorINS_15EvalResultValue...`
- **String evidence**: _none observed_
- **Notes**: candidate qualified-name match in reconstructed/src/runtime/custom_functions_playback.cpp — verify whether the recon implements the same overload (likely signature-divergent rather than truly absent)

## Cluster: `awg_config::device`

### Overview

**Members:** 1  |  **Aggregate size:** 254 B  |  **Subsystem:** device (AWG configuration helper)

Single 254 B AWG-configuration helper.

Reconstruction difficulty: **trivial**.

### Symbols

#### `zhinst::AwgPathPatterns::AwgPathPatterns(zhinst::AwgPathPatterns const&)`

- **Mangled**: `_ZN6zhinst15AwgPathPatternsC2ERKS0_`
- **Address**: `0x2cc4f0` | **Size**: 254 B | **Status**: absent
- **First insn**: `push   %rbp`
- **Callers**: `_ZN6zhinst17getAwgDevicePropsILNS_13AwgDeviceTypeE8EEENS_14AwgDevicePropsERKN...`, `_ZN6zhinst17getAwgDevicePropsILNS_13AwgDeviceTypeE1EEENS_14AwgDevicePropsERKN...`, `_ZN6zhinst17getAwgDevicePropsILNS_13AwgDeviceTypeE256EEENS_14AwgDevicePropsER...`, `_ZN6zhinst17getAwgDevicePropsILNS_13AwgDeviceTypeE2EEENS_14AwgDevicePropsERKN...`, `_ZN6zhinst17getAwgDevicePropsILNS_13AwgDeviceTypeE4EEENS_14AwgDevicePropsERKN...`, `_ZN6zhinst17getAwgDevicePropsILNS_13AwgDeviceTypeE64EEENS_14AwgDevicePropsERK...`, `_GLOBAL__sub_I_properties.cpp`, `_ZN6zhinst17getAwgDevicePropsILNS_13AwgDeviceTypeE128EEENS_14AwgDevicePropsER...`, `_ZN6zhinst17getAwgDevicePropsILNS_13AwgDeviceTypeE16EEENS_14AwgDevicePropsERK...`, `_ZN6zhinst17getAwgDevicePropsILNS_13AwgDeviceTypeE32EEENS_14AwgDevicePropsERK...`
- **String evidence**: _none observed_
- **Notes**: **closed as ABI-divergence by design (IF-280, 2026-05-16)**.  Header declares copy ctor `= default`; libstdc++-built recon inlines it at all 9 callsites.  Binary's body is libc++-string-shaped (SSO bit-0 test) and cannot be reproduced from a libstdc++ TU.  A libc++ shim TU would emit a symbol with no caller.  Observable behavior is identical (member-wise copy of 3 strings with proper exception cleanup).

## Cluster: `device_option::device`

### Overview

**Members:** 2  |  **Aggregate size:** 90 B  |  **Subsystem:** device (DeviceType pimpl accessor + polymorphic deep-copy)

Two small `DeviceType` / `DeviceTypeImpl` accessors: a 9 B pimpl getter and an 81 B virtual deep-copy.  (Original cluster prose said "hash/compare" — that was wrong; cross-checked against actual symbol-list per AGENTS.md guidance.)

Reconstruction difficulty: **trivial**.

### Symbols

#### `zhinst::detail::DeviceTypeImpl::doClone() const`

- **Mangled**: `_ZNK6zhinst6detail14DeviceTypeImpl7doCloneEv`
- **Address**: `0x2d3280` | **Size**: 81 B | **Status**: **fixed (IF-281, 2026-05-16)** — recon had it as `DeviceTypeImpl::clone()`; renamed to `doClone()` matching binary mangling.
- **First insn**: `push   %rbp`
- **Callers**: _none found_
- **String evidence**: _none observed_

#### `zhinst::DeviceType::deviceType() const`

- **Mangled**: `_ZNK6zhinst10DeviceType10deviceTypeEv`
- **Address**: `0x2d2c20` | **Size**: 9 B | **Status**: **fixed (IF-281, 2026-05-16)** — recon had it as `DeviceType::impl()`; renamed to `deviceType()` matching binary mangling.
- **First insn**: `push   %rbp`
- **Callers**: _none found_
- **String evidence**: _none observed_

## Cluster: `node_misc::core`

### Overview

**Members:** 1  |  **Aggregate size:** 43 B  |  **Subsystem:** core (Node helper)

Single 43 B `Node` helper — likely a 1-line accessor.

Reconstruction difficulty: **trivial**. Probably auto-resolved once read.

### Symbols

#### `zhinst::NodeMap::pauPoffIwrap(unsigned int)`

- **Mangled**: `_ZN6zhinst7NodeMap12pauPoffIwrapEj`
- **Address**: `0x1c5650` | **Size**: 43 B | **Status**: **fixed (IF-282, 2026-05-16)** — recon had the wrap logic as anon-namespace `wrap23` helper; promoted to public static `NodeMap::pauPoffIwrap` matching binary mangling.
- **First insn**: `push   %rbp`
- **Callers**: _none found_
- **String evidence**: _none observed_

## Stub-only user code

Five symbols defined in `reconstructed/src/` with trivially-short or explicitly-stubbed bodies.  The remaining 33 stub-flagged symbols are compiler-generated and listed in 'Excluded from this report'.

### `zhinst::SeqCIfElse::operator=(zhinst::SeqCIfElse)`

- **Mangled**: `_ZN6zhinst10SeqCIfElseaSES0_`
- **Address**: `0x201f70` | **Size**: 367 B | **Status**: stub
- **First insn**: `push   %rbp`
- **Callers**: _none found_
- **String evidence**: _none observed_
- **Notes**: Copy-assignment operator for `SeqCIfElse`.  Recon body is the synthesised default; the binary explicitly emits one (367 bytes) likely because at least one base subobject has a non-trivial assign.  Low priority — only matters if a specific copy-and-assign pattern in user code triggers a behaviour diff.

### `zhinst::SeqCCondExpr::operator=(zhinst::SeqCCondExpr)`

- **Mangled**: `_ZN6zhinst12SeqCCondExpraSES0_`
- **Address**: `0x203d20` | **Size**: 367 B | **Status**: stub
- **First insn**: `push   %rbp`
- **Callers**: _none found_
- **String evidence**: _none observed_
- **Notes**: Copy-assignment operator for `SeqCCondExpr`.  Same shape and rationale as `SeqCIfElse::operator=` above.

### `zhinst::WavetableFront::dummyWarning(std::__1::basic_string<char, std::__1::char_traits<char>, std::__1::allocator<char> > const&, int)`

- **Mangled**: `_ZN6zhinst14WavetableFront12dummyWarningERKNSt3__112basic_stringIcNS1_11char_traitsIcEENS1_9allocatorIcEEEEi`
- **Address**: `0x29ac60` | **Size**: 159 B | **Status**: stub
- **First insn**: `push   %rbp`
- **Callers**: _none found_
- **String evidence**: `"Warning not tracked: "`
- **Notes**: Default warning sink installed by `WavetableFront`.  Recon body is empty `{}`; binary body is 159 B and emits `"Warning not tracked: "` to a sink — see the captured string evidence.  Difference is observable only when no real warning callback is registered, which the test suite always does.

### `zhinst::tracing::TraceProvider::~TraceProvider()`

- **Mangled**: `_ZN6zhinst7tracing13TraceProviderD2Ev`
- **Address**: `0xfa5e0` | **Size**: 105 B | **Status**: stub
- **First insn**: `push   %rbp`
- **Callers**: _none found_
- **String evidence**: _none observed_
- **Notes**: `TraceProvider::~TraceProvider()`.  Recon uses `= default`; binary destructor is 105 B — likely shuts down an OpenTelemetry provider before releasing the `provider_` member.  Tracing is disabled at runtime in tests, so the divergence is invisible to difftests.

### `zhinst::runningOnMfDevice()`

- **Mangled**: `_ZN6zhinst17runningOnMfDeviceEv`
- **Address**: `0x2ec580` | **Size**: 83 B | **Status**: stub
- **First insn**: `movzbl 0x898d11(%rip),%eax        # b85298 <_ZGVZN6zhinst17runningOnMfDeviceEvE11runningOnMf>`
- **Callers**: `_ZN6zhinst8ZiFolder8ziFolderENS0_13DirectoryTypeE`
- **String evidence**: _none observed_
- **Notes**: IF-253 anchor.  Recon stub returns `false`; binary body is 83 B and reads a TLS-cached flag (note the `movzbl ...(%rip)` first instruction touching guard variable `_ZGVZN6zhinst17runningOnMfDeviceEvE11runningOnMf`).  Several IO/path helpers in the `zi_environment` cluster gate behaviour on this flag, so reconstructing it unblocks that whole cluster.

## Divergent signatures (informational)

Sixty-two binary symbols whose `Class::method` qualified name matches an existing recon function but whose mangled name differs.  Almost all are STL-ABI-mismatch artefacts (libc++ `std::__1` vs libstdc++ `std::__cxx11`/default), or differences in iterator types (`__wrap_iter` vs `__normal_iterator`).  These do **not** represent missing functionality — the recon already implements them; only the mangling differs.  Listed for completeness so they are not mistakenly added to the work backlog.

### By group

**`AsmOptimize`** (5 entries) — typical cause: STL-ABI iterator/string mangling difference:

- `zhinst::AsmOptimize::splitReg(zhinst::AsmList&, AsmRegister, std::__1::__wrap_iter<zhinst::AsmList::Asm const*>, std::__1::__wrap_iter<zhins` — `_ZN6zhinst11AsmOptimize8splitRegERNS_7AsmListE11AsmRegisterNSt3__111__wrap_iterIPKNS1_3AsmEEES9_` @ `0x281000` (1291 B)
- `zhinst::AsmOptimize::registerIsNeverWritten(zhinst::AsmList&, AsmRegister, std::__1::__wrap_iter<zhinst::AsmList::Asm const*>, std::__1::__w` — `_ZNK6zhinst11AsmOptimize22registerIsNeverWrittenERNS_7AsmListE11AsmRegisterNSt3__111__wrap_iterIPKNS1_3AsmEEES9_` @ `0x280f50` (164 B)
- `zhinst::AsmOptimize::getNextActionForReg(std::__1::__wrap_iter<zhinst::AsmList::Asm const*>, AsmRegister)` — `_ZN6zhinst11AsmOptimize19getNextActionForRegENSt3__111__wrap_iterIPKNS_7AsmList3AsmEEE11AsmRegister` @ `0x281a10` (225 B)
- `zhinst::AsmOptimize::isLabelCalled(std::__1::basic_string<char, std::__1::char_traits<char>, std::__1::allocator<char> > const&, std::__1::_` — `_ZN6zhinst11AsmOptimize13isLabelCalledERKNSt3__112basic_stringIcNS1_11char_traitsIcEENS1_9allocatorIcEEEENS1_11__wrap_iterIPKNS_7AsmList3AsmEEE` @ `0x27d9c0` (237 B)
- `zhinst::AsmOptimize::simplifyAssign(std::__1::__wrap_iter<zhinst::AsmList::Asm*>)` — `_ZN6zhinst11AsmOptimize14simplifyAssignENSt3__111__wrap_iterIPNS_7AsmList3AsmEEE` @ `0x280e10` (315 B)

**`DeviceOptionSetConstIterator`** (2 entries) — typical cause: STL-ABI iterator/string mangling difference:

- `zhinst::DeviceOptionSetConstIterator::DeviceOptionSetConstIterator(std::__1::__map_const_iterator<std::__1::__tree_const_iterator<std::__1::` — `_ZN6zhinst28DeviceOptionSetConstIteratorC1ENSt3__120__map_const_iteratorINS1_21__tree_const_iteratorINS1_12__value_typeINS1_12basic_stringIcNS1_11char_traitsIcEENS1_9allocatorIcEEEENS_12DeviceOptionEEEPNS1_11__tree_nodeISC_PvEElEEEE` @ `0x2cf900` (9 B)
- `zhinst::DeviceOptionSetConstIterator::DeviceOptionSetConstIterator(std::__1::__map_const_iterator<std::__1::__tree_const_iterator<std::__1::` — `_ZN6zhinst28DeviceOptionSetConstIteratorC2ENSt3__120__map_const_iteratorINS1_21__tree_const_iteratorINS1_12__value_typeINS1_12basic_stringIcNS1_11char_traitsIcEENS1_9allocatorIcEEEENS_12DeviceOptionEEEPNS1_11__tree_nodeISC_PvEElEEEE` @ `0x2cf900` (9 B)

**`Exception`** (6 entries) — typical cause: STL-ABI iterator/string mangling difference:

- `zhinst::Exception::Exception(boost::system::error_code const&, std::__1::basic_string<char, std::__1::char_traits<char>, std::__1::allocator` — `_ZN6zhinst9ExceptionC1ERKN5boost6system10error_codeENSt3__112basic_stringIcNS6_11char_traitsIcEENS6_9allocatorIcEEEE` @ `0x2e5700` (263 B)
- `zhinst::Exception::Exception(boost::system::error_code const&, std::__1::basic_string<char, std::__1::char_traits<char>, std::__1::allocator` — `_ZN6zhinst9ExceptionC2ERKN5boost6system10error_codeENSt3__112basic_stringIcNS6_11char_traitsIcEENS6_9allocatorIcEEEE` @ `0x2e5700` (263 B)
- `zhinst::Exception::Exception(boost::system::error_code const&)` — `_ZN6zhinst9ExceptionC1ERKN5boost6system10error_codeE` @ `0x2e55b0` (336 B)
- `zhinst::Exception::Exception(boost::system::error_code const&)` — `_ZN6zhinst9ExceptionC2ERKN5boost6system10error_codeE` @ `0x2e55b0` (336 B)
- `zhinst::Exception::Exception(zhinst::GenericErrorDescription<boost::system::error_code>)` — `_ZN6zhinst9ExceptionC1ENS_23GenericErrorDescriptionIN5boost6system10error_codeEEE` @ `0x2e5810` (93 B)
- `zhinst::Exception::Exception(zhinst::GenericErrorDescription<boost::system::error_code>)` — `_ZN6zhinst9ExceptionC2ENS_23GenericErrorDescriptionIN5boost6system10error_codeEEE` @ `0x2e5810` (93 B)

**`Immediate`** (1 entries) — typical cause: STL-ABI iterator/string mangling difference:

- `zhinst::Immediate::operator==(zhinst::Immediate) const` — `_ZNK6zhinst9ImmediateeqES0_` @ `0x290d40` (75 B)

**`Node`** (3 entries) — typical cause: STL-ABI iterator/string mangling difference:

- `zhinst::Node::Node(zhinst::Node::NodeType, int, int)` — `_ZN6zhinst4NodeC2ENS0_8NodeTypeEii` @ `0x12ace0` (466 B)
- `zhinst::Node::type2str(zhinst::Node::NodeType)` — `_ZN6zhinst4Node8type2strENS0_8NodeTypeE` @ `0x269970` (540 B)
- `zhinst::Node::Node(int, int, std::__1::vector<std::__1::optional<std::__1::basic_string<char, std::__1::char_traits<char>, std::__1::allocat` — `_ZN6zhinst4NodeC2EiiRKNSt3__16vectorINS1_8optionalINS1_12basic_stringIcNS1_11char_traitsIcEENS1_9allocatorIcEEEEEENS7_ISA_EEEEiNS0_8NodeTypeENS_10PlayConfigESG_11AsmRegisteriSH_iNS1_10shared_ptrIS0_EERKNS2_ISJ_NS7_ISJ_EEEESJ_NS1_8weak_ptrIS0_EEijbbi` @ `0x26c4a0` (640 B)

**`PlayArgs`** (3 entries) — typical cause: STL-ABI iterator/string mangling difference:

- `zhinst::PlayArgs::parseImplicitChannels(std::__1::__wrap_iter<zhinst::EvalResultValue const*>, std::__1::__wrap_iter<zhinst::EvalResultValue` — `_ZN6zhinst8PlayArgs21parseImplicitChannelsENSt3__111__wrap_iterIPKNS_15EvalResultValueEEES6_` @ `0x16fb30` (1220 B)
- `zhinst::PlayArgs::secureLoadWaveform(std::__1::basic_string<char, std::__1::char_traits<char>, std::__1::allocator<char> > const&, unsigned ` — `_ZN6zhinst8PlayArgs18secureLoadWaveformERKNSt3__112basic_stringIcNS1_11char_traitsIcEENS1_9allocatorIcEEEEm` @ `0x1711a0` (1348 B)
- `zhinst::PlayArgs::parseExplicitChannels(std::__1::__wrap_iter<zhinst::EvalResultValue const*>, std::__1::__wrap_iter<zhinst::EvalResultValue` — `_ZN6zhinst8PlayArgs21parseExplicitChannelsENSt3__111__wrap_iterIPKNS_15EvalResultValueEEES6_` @ `0x170000` (3298 B)

**`RawWaveHirzel16`** (1 entries) — typical cause: STL-ABI iterator/string mangling difference:

- `zhinst::RawWaveHirzel16::RawWaveHirzel16(std::__1::vector<double, std::__1::allocator<double> > const&, std::__1::vector<unsigned char, std:` — `_ZN6zhinst15RawWaveHirzel16C2ERKNSt3__16vectorIdNS1_9allocatorIdEEEERKNS2_IhNS3_IhEEEERKNS_20MarkerBitsPerChannelE` @ `0x297140` (650 B)

**`Resources`** (1 entries) — typical cause: STL-ABI iterator/string mangling difference:

- `zhinst::Resources::setReturnValue(zhinst::Value)` — `_ZN6zhinst9Resources14setReturnValueENS_5ValueE` @ `0x1e3b30` (517 B)

**`ScopedSpan`** (2 entries) — typical cause: STL-ABI iterator/string mangling difference:

- `zhinst::tracing::ScopedSpan::ScopedSpan(opentelemetry::v1::nostd::string_view, opentelemetry::v1::nostd::string_view, std::initializer_list<` — `_ZN6zhinst7tracing10ScopedSpanC1EN13opentelemetry2v15nostd11string_viewES5_RKSt16initializer_listINSt3__14pairIS5_NS7_7variantIJbiljdPKcS5_NS4_4spanIKbLm18446744073709551615EEENSC_IKiLm18446744073709551615EEENSC_IKlLm18446744073709551615EEENSC_IKjLm18446744073709551615EEENSC_IKdLm18446744073709551615EEENSC_IKS5_Lm18446744073709551615EEEmNSC_IKmLm18446744073709551615EEENSC_IKhLm18446744073709551615EEEEEEEEE` @ `0xfaae0` (945 B)
- `zhinst::tracing::ScopedSpan::ScopedSpan(opentelemetry::v1::nostd::string_view, opentelemetry::v1::nostd::string_view, std::initializer_list<` — `_ZN6zhinst7tracing10ScopedSpanC2EN13opentelemetry2v15nostd11string_viewES5_RKSt16initializer_listINSt3__14pairIS5_NS7_7variantIJbiljdPKcS5_NS4_4spanIKbLm18446744073709551615EEENSC_IKiLm18446744073709551615EEENSC_IKlLm18446744073709551615EEENSC_IKjLm18446744073709551615EEENSC_IKdLm18446744073709551615EEENSC_IKS5_Lm18446744073709551615EEEmNSC_IKmLm18446744073709551615EEENSC_IKhLm18446744073709551615EEEEEEEEE` @ `0xfaae0` (945 B)

**`SeqcParserContext`** (1 entries) — typical cause: STL-ABI iterator/string mangling difference:

- `zhinst::SeqcParserContext::setErrorCallback(std::__1::function<void (int, std::__1::basic_string<char, std::__1::char_traits<char>, std::__1` — `_ZN6zhinst17SeqcParserContext16setErrorCallbackERKNSt3__18functionIFviRKNS1_12basic_stringIcNS1_11char_traitsIcEENS1_9allocatorIcEEEEEEE` @ `0x247a60` (124 B)

**`Signal`** (3 entries) — typical cause: STL-ABI iterator/string mangling difference:

- `zhinst::Signal::Signal(zhinst::Signal::ReserveOnly const&, unsigned long, zhinst::MarkerBitsPerChannel const&)` — `_ZN6zhinst6SignalC2ERKNS0_11ReserveOnlyEmRKNS_20MarkerBitsPerChannelE` @ `0x25ef50` (269 B)
- `zhinst::Signal::Signal(unsigned long, zhinst::MarkerBitsPerChannel const&)` — `_ZN6zhinst6SignalC2EmRKNS_20MarkerBitsPerChannelE` @ `0x25f1a0` (353 B)
- `zhinst::Signal::Signal(std::__1::vector<double, std::__1::allocator<double> > const&, std::__1::vector<unsigned char, std::__1::allocator<un` — `_ZN6zhinst6SignalC2ERKNSt3__16vectorIdNS1_9allocatorIdEEEERKNS2_IhNS3_IhEEEERKNS_20MarkerBitsPerChannelE` @ `0x106340` (476 B)

**`TraceProvider`** (1 entries) — typical cause: STL-ABI iterator/string mangling difference:

- `zhinst::tracing::TraceProvider::getProvider()` — `_ZN6zhinst7tracing13TraceProvider11getProviderEv` @ `0xfa650` (30 B)

**`Waveform`** (1 entries) — typical cause: STL-ABI iterator/string mangling difference:

- `zhinst::Waveform::Waveform(std::__1::basic_string<char, std::__1::char_traits<char>, std::__1::allocator<char> >, zhinst::Waveform::File::Ty` — `_ZN6zhinst8WaveformC2ENSt3__112basic_stringIcNS1_11char_traitsIcEENS1_9allocatorIcEEEENS0_4File4TypeES7_NS1_10shared_ptrIS8_EEbNS_6detail11AddressImplIjEES7_iiiSE_RKNS_15DeviceConstantsENS_6SignalE` @ `0x2a71e0` (422 B)

**`WaveformFront>`** (2 entries) — typical cause: STL-ABI iterator/string mangling difference:

- `zhinst::detail::WavetableManager<zhinst::WaveformFront>::newWaveformFromFile(std::__1::basic_string<char, std::__1::char_traits<char>, std::` — `_ZN6zhinst6detail16WavetableManagerINS_13WaveformFrontEE19newWaveformFromFileERKNSt3__112basic_stringIcNS4_11char_traitsIcEENS4_9allocatorIcEEEESC_NS_8Waveform4File4TypeERKNS_15DeviceConstantsE` @ `0x29b110` (1039 B)
- `zhinst::detail::WavetableManager<zhinst::WaveformFront>::newWaveformFromFile(std::__1::basic_string<char, std::__1::char_traits<char>, std::` — `_ZN6zhinst6detail16WavetableManagerINS_13WaveformFrontEE19newWaveformFromFileERKNSt3__112basic_stringIcNS4_11char_traitsIcEENS4_9allocatorIcEEEERKNS_6SignalENS0_11AddressImplIjEESC_NS_8Waveform4File4TypeERKNS_15DeviceConstantsE` @ `0x29b560` (1126 B)

**`WaveformIR`** (1 entries) — typical cause: STL-ABI iterator/string mangling difference:

- `zhinst::WaveformIR::toJsonElement(zhinst::SampleFormat)` — `_ZN6zhinst10WaveformIR13toJsonElementENS_12SampleFormatE` @ `0x2c5440` (2310 B)

**`WavetableFront`** (2 entries) — typical cause: STL-ABI iterator/string mangling difference:

- `zhinst::WavetableFront::newWaveformFromFile(std::__1::basic_string<char, std::__1::char_traits<char>, std::__1::allocator<char> > const&, st` — `_ZN6zhinst14WavetableFront19newWaveformFromFileERKNSt3__112basic_stringIcNS1_11char_traitsIcEENS1_9allocatorIcEEEES9_NS_8Waveform4File4TypeE` @ `0x29b0e0` (34 B)
- `zhinst::WavetableFront::newWaveformFromFile(std::__1::basic_string<char, std::__1::char_traits<char>, std::__1::allocator<char> > const&, zh` — `_ZN6zhinst14WavetableFront19newWaveformFromFileERKNSt3__112basic_stringIcNS1_11char_traitsIcEENS1_9allocatorIcEEEERKNS_6SignalES9_NS_8Waveform4File4TypeE` @ `0x29b520` (52 B)

**`ZIIOException`** (2 entries) — typical cause: STL-ABI iterator/string mangling difference:

- `zhinst::ZIIOException::ZIIOException(unsigned long)` — `_ZN6zhinst13ZIIOExceptionC1Em` @ `0x2e5b10` (199 B)
- `zhinst::ZIIOException::ZIIOException(unsigned long)` — `_ZN6zhinst13ZIIOExceptionC2Em` @ `0x2e5b10` (199 B)

**`__1`** (2 entries) — typical cause: STL-ABI iterator/string mangling difference:

- `zhinst::CompilerMessage& std::__1::vector<zhinst::CompilerMessage, std::__1::allocator<zhinst::CompilerMessage> >::emplace_back<zhinst::Comp` — `_ZNSt3__16vectorIN6zhinst15CompilerMessageENS_9allocatorIS2_EEE12emplace_backIJNS2_19CompilerMessageTypeENS_12basic_stringIcNS_11char_traitsIcEENS3_IcEEEEEEERS2_DpOT_` @ `0x107ac0` (121 B)
- `unsigned long std::__1::__tree<std::__1::__value_type<std::__1::vector<unsigned int, std::__1::allocator<unsigned int> >, zhinst::CachedPars` — `_ZNSt3__16__treeINS_12__value_typeINS_6vectorIjNS_9allocatorIjEEEEN6zhinst12CachedParser10CacheEntryEEENS_19__map_value_compareIS5_S9_NS_4lessIS5_EELb1EEENS3_IS9_EEE14__erase_uniqueIS5_EEmRKT_` @ `0x2b5810` (345 B)

**`detail`** (1 entries) — typical cause: STL-ABI iterator/string mangling difference:

- `zhinst::detail::logExceptionToClog(std::exception_ptr, char const*, bool)` — `_ZN6zhinst6detail18logExceptionToClogESt13exception_ptrPKcb` @ `0x314a30` (953 B)

**`zhinst`** (22 entries) — typical cause: STL-ABI iterator/string mangling difference:

- `zhinst::operator<<(std::__1::basic_ostream<char, std::__1::char_traits<char> >&, zhinst::CalVer const&)` — `_ZN6zhinstlsERNSt3__113basic_ostreamIcNS0_11char_traitsIcEEEERKNS_6CalVerE` @ `0x100b40` (120 B)
- `zhinst::(anonymous namespace)::AwgDevicePropsWithDefault::~AwgDevicePropsWithDefault()` — `_ZN6zhinst12_GLOBAL__N_125AwgDevicePropsWithDefaultD2Ev` @ `0x2cc870` (129 B)
- `zhinst::(anonymous namespace)::ErrorKindCategory::name() const` — `_ZNK6zhinst12_GLOBAL__N_117ErrorKindCategory4nameEv` @ `0x2e52b0` (13 B)
- `zhinst::(anonymous namespace)::ZiApiErrorCategory::name() const` — `_ZNK6zhinst12_GLOBAL__N_118ZiApiErrorCategory4nameEv` @ `0x2e4540` (13 B)
- `zhinst::(anonymous namespace)::ZiApiErrorCategory::default_error_condition(int) const` — `_ZNK6zhinst12_GLOBAL__N_118ZiApiErrorCategory23default_error_conditionEi` @ `0x2e46b0` (161 B)
- `zhinst::floatEqual(float, float)` — `_ZN6zhinst10floatEqualEff` @ `0x2ec030` (18 B)
- `zhinst::isSet(std::__1::array<unsigned long, 3ul> const&)` — `_ZN6zhinst5isSetERKNSt3__15arrayImLm3EEE` @ `0x1020d0` (20 B)
- `zhinst::operator<<(std::__1::basic_ostream<char, std::__1::char_traits<char> >&, zhinst::Immediate)` — `_ZN6zhinstlsERNSt3__113basic_ostreamIcNS0_11char_traitsIcEEEENS_9ImmediateE` @ `0x290b90` (298 B)
- `zhinst::isValidUtf8(std::__1::__wrap_iter<char const*>, std::__1::__wrap_iter<char const*>)` — `_ZN6zhinst11isValidUtf8ENSt3__111__wrap_iterIPKcEES4_` @ `0x2f8330` (310 B)
- `zhinst::(anonymous namespace)::ErrorKindCategory::message(int) const` — `_ZNK6zhinst12_GLOBAL__N_117ErrorKindCategory7messageEi` @ `0x2e52c0` (327 B)
- `zhinst::getApiErrorMessage(ZIResult_enum)` — `_ZN6zhinst18getApiErrorMessageE13ZIResult_enum` @ `0x2e4820` (337 B)
- `zhinst::parseOptionalRate(std::__1::__wrap_iter<zhinst::EvalResultValue const*>, std::__1::__wrap_iter<zhinst::EvalResultValue const*>, std:` — `_ZN6zhinst17parseOptionalRateENSt3__111__wrap_iterIPKNS_15EvalResultValueEEES5_S5_RKNS0_12basic_stringIcNS0_11char_traitsIcEENS0_9allocatorIcEEEEb` @ `0x163980` (345 B)
- `zhinst::(anonymous namespace)::checkWaveformInit(std::__1::shared_ptr<zhinst::WaveformFront>, std::__1::basic_string<char, std::__1::char_tr` — `_ZN6zhinst12_GLOBAL__N_117checkWaveformInitENSt3__110shared_ptrINS_13WaveformFrontEEERKNS1_12basic_stringIcNS1_11char_traitsIcEENS1_9allocatorIcEEEE` @ `0x29c6f0` (357 B)
- `zhinst::formatTime(boost::posix_time::ptime const&, std::__1::basic_string<char, std::__1::char_traits<char>, std::__1::allocator<char> > co` — `_ZN6zhinst10formatTimeERKN5boost10posix_time5ptimeERKNSt3__112basic_stringIcNS5_11char_traitsIcEENS5_9allocatorIcEEEE` @ `0x2f7410` (41 B)
- `zhinst::fromDecimal(std::__1::basic_string<char, std::__1::char_traits<char>, std::__1::allocator<char> > const&)` — `_ZN6zhinst11fromDecimalERKNSt3__112basic_stringIcNS0_11char_traitsIcEENS0_9allocatorIcEEEE` @ `0x100520` (439 B)
- `zhinst::(anonymous namespace)::getUniqueName(std::__1::basic_string<char, std::__1::char_traits<char>, std::__1::allocator<char> > const&, i` — `_ZN6zhinst12_GLOBAL__N_113getUniqueNameERKNSt3__112basic_stringIcNS1_11char_traitsIcEENS1_9allocatorIcEEEEii` @ `0x2a0fd0` (559 B)
- `zhinst::(anonymous namespace)::ZiApiErrorCategory::message(int) const` — `_ZNK6zhinst12_GLOBAL__N_118ZiApiErrorCategory7messageEi` @ `0x2e4760` (63 B)
- `zhinst::toString(zhinst::Immediate)` — `_ZN6zhinst8toStringENS_9ImmediateE` @ `0x290b40` (65 B)
- `zhinst::toDeviceFamily(unsigned int)` — `_ZN6zhinst14toDeviceFamilyEj` @ `0x2df5c0` (80 B)
- `zhinst::toString(std::__1::array<unsigned long, 3ul> const&)` — `_ZN6zhinst8toStringERKNSt3__15arrayImLm3EEE` @ `0x101d80` (839 B)
- `zhinst::(anonymous namespace)::xmlEscapeSeqToInt(std::__1::__wrap_iter<char const*>, std::__1::__wrap_iter<char const*>)` — `_ZN6zhinst12_GLOBAL__N_117xmlEscapeSeqToIntENSt3__111__wrap_iterIPKcEES5_` @ `0x2fc280` (881 B)
- `zhinst::(anonymous namespace)::IndexedArgs::~IndexedArgs()` — `_ZN6zhinst12_GLOBAL__N_111IndexedArgsD2Ev` @ `0x163dc0` (98 B)

## Excluded from this report

- **682 non-`zhinst::` symbols** present in `_seqc_compiler.so` but outside the project namespace.  These are vendored third-party libraries (zlib, OpenSSL, fmt, spdlog, OpenTelemetry, libstdc++/libc++, Boost ICL) linked statically into the .so.  They are out of scope for this reconstruction and tracked separately in `reconstructed/notes/binary_contents_excluded.md`.
- **33 compiler-generated thunks** flagged by the stub sweep: 18 `_ZThn*_N5boost10wrapexceptIN6zhinst...EED{0,1}Ev` virtual-base destructor thunks, plus 15 STL container copy-constructors (`unordered_set`, `unordered_map`, `vector`) parameterised on `zhinst::DeviceOption` / `zhinst::Node`.  These are auto-emitted by the C++ compiler and have no hand-written counterpart in the binary's source.

## Open questions

- **CSV parser size mystery.**  Both `csvFileToWaveform` template instantiations are exactly 10 111 bytes — suggesting a shared `_impl` body.  Confirming this would let one reconstruction pass fix both.  Needs side-by-side disassembly comparison.
- **Anchor symbols beyond `runningOnMfDevice`.**  Are any other absent symbols gating a known recon stub?  A targeted grep of `reconstructed/src/` for `// TODO` / `// stub` next to a call-site naming any cluster member would surface these.
- **`zi_environment` callers.**  Several environment-detection helpers have zero observed callers in the binary.  Are they reachable only via `dlsym` from external tooling, or genuinely dead?  A check of the exported-symbol table of LabOne's host binaries would answer this.
- **`AwgPathPatterns::AwgPathPatterns` candidate match.**  Recon exposes the constructor but the binary's mangling differs.  Determine whether the recon ctor is byte-equivalent or a different overload.
- **`Random::seedRandom` overload.**  Recon already implements a function called `seedRandom` in two files; the absent symbol may be a second overload (different argument list) or a TLS-init helper.  Resolution is a one-line nm grep but was deferred from this scout.
- **Vtable-slot ordering for `SeqC*::clone()`.**  All 53 clone overrides are mechanical, but the binary's vtables must be matched slot-for-slot for downcasts to work.  Need to confirm slot order via `objdump -s -j .data.rel.ro` before bulk-emitting them.
