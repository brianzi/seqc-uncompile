# Batch 28 — awg_compiler

> **Phase S.1 reconciliation note (2026-04-29)**: This batch contains
> 4 row(s) from the 226-item Phase Q backlog. Per-row triage:
> B1 (mechanical, staged for S.2): 1; B2 (borderline, deferred): 2;
> B3 (already resolved during Phase D/R): 1;
> B4 (wontfix / kept-as-is): 0.
> The authoritative per-row table lives in
> [`SYNTHESIS.md` §6](SYNTHESIS.md#6-low-confidence-and-unsure-parked--reconciled-in-phase-s1).
> Individual rows below are NOT struck through; consult §6 for status.

## 1. Files considered

- `reconstructed/include/zhinst/awg_compiler.hpp`
- `reconstructed/src/awg_compiler.cpp`

Use-site survey: `grep -rn` across `reconstructed/src/` and
`reconstructed/include/zhinst/`. Forward header is small (one pimpl
class, public method names mirror the impl).

Cross-batch counterparts consulted:
- `notes/symbol-renaming-audit/23_awg_compiler_config.md`
  (`isHirzel`/`channelGrouping`/`sampleFormat`/`cacheType` cluster
  anchor; producer of all `config_->` reads in this file).
- `notes/symbol-renaming-audit/30_awg_device_props.md`
  (cluster origin upstream of config).
- `notes/symbol-renaming-audit/32_frontend_lowering.md`
  (third leg of channelGrouping cluster).
- `notes/symbol-renaming-audit/07_compiler.md`
  (`Compiler` member type embedded at `+0x0C0`; renames there
  inform what `compiler_` here is expected to expose).
- `notes/symbol-renaming-audit/13_awg_assembler_impl.md`,
  `notes/symbol-renaming-audit/33_awg_assembler.md`
  (`AWGAssembler assembler_` at `+0x278`).
- `notes/symbol-renaming-audit/09_prefetch.md`
  (`getUsedFourChannelMode()`; the `isHirzel` cluster downstream).
- `notes/symbol-renaming-audit/31_device_constants.md`
  (`DeviceConstants` member at `+0x008`; field names referenced).
- `notes/symbol-renaming-audit/47_elf_writer.md`,
  `notes/symbol-renaming-audit/45_wavetable_front.md`,
  `notes/symbol-renaming-audit/46_wavetable_ir.md`,
  `notes/symbol-renaming-audit/16_waveform_ir.md`.

Binary symbol table consulted via
`nm --demangle /home/brian/zhinst/seqc_compiler/_seqc_compiler.so |
grep -E "AWGCompiler(Impl)?::|writeWavesToElf"`.

Authoritative under §3 (excluded from rename):

- Type `zhinst::AWGCompiler` (in mangled symbols of every method).
- Type `zhinst::AWGCompilerImpl` (likewise).
- Methods on `zhinst::AWGCompiler`:
  `AWGCompiler(AWGCompilerConfig const&)` (0x103210),
  `~AWGCompiler()` (0x103260),
  `compileString` (0x1032b0), `compileFile` (0x1032a0),
  `addWaveforms` (0x1032c0), `writeToStream` (0x1032d0),
  `writeToFile` (0x1032e0), `writeAssemblerToFile` (0x1032f0),
  `getCompileReport` (0x1033c0),
  `getJsonWaveformMemoryInfo` (0x1033e0),
  `setCancelCallback` (0x103300),
  `setProgressCallback` (0x103360).
- Methods on `zhinst::AWGCompilerImpl`:
  ctor `(0x103b40)`, dtor `(0x103400)`,
  `compileString` (0x106cb0), `compileFile` (0x106690),
  `addWaveforms` (0x104660), `writeToStream` (0x108cc0),
  `writeToFile` (0x10b9b0), `writeAssemblerToFile` (0x107d10),
  `setCancelCallback` (0x103eb0),
  `setProgressCallback` (0x103f90),
  `getBinVersion() const` (0x10b830),
  `getJsonVersion() const` (0x10ac60),
  `nodeListToJson(...) const` (0x1088d0) — recon names this as a
  body-helper inside `writeToStream`, but it is a real method in
  the binary symbol table,
  `getCompileReport() const` (0x104030),
  `getJsonArguments(...) const` (0x10a3c0),
  `getAssemblerHeader(...) const` (0x1083d0),
  `getJsonWaveformMemoryInfo() const` (0x10a1b0).
- Anonymous-namespace free functions inside the binary:
  `zhinst::(anonymous namespace)::writeWavesToElfMapped(`
  `AWGCompilerConfig const&, shared_ptr<WavetableIR>, ElfWriter&)`,
  `…::writeWavesToElfAbsolute(...)`, parameters
  `AWGCompilerConfig const&, std::shared_ptr<WavetableIR>,
  ElfWriter&` known via mangling — function names are excluded;
  parameter names are NOT in scope here (they live in a different
  source file, `write_waves_to_elf.cpp`, addressed in batch 23
  cross-references).
- Free helpers in `util::wave`: `awg2double`, `awg2marker`,
  `awg2double16` — present in `nm` (qualified `zhinst::util::wave::`),
  out of scope.

NOT in the symbol table (recon-only / instance data, in scope):

- `AWGCompiler::impl_` (instance data member).
- All instance data members of `AWGCompilerImpl` (`config_`,
  `deviceConstants_`, `wavetable_`, `wavetableIR_`, `compiler_`,
  `string_200_`, `string_218_`, `string_230_`, `string_248_`,
  `compileMessages_`, `assembler_`, `wavePaths_`,
  `cancelCallback_`, `progressCallback_`, padding fields).
- All parameter names of all in-scope methods.
- The local `compressSourceString(source, format)` helper inside
  the anonymous namespace (it is in fact an inline lambda body
  emitted into `writeToStream` at 0x109e90; the function name is
  recon-coined, so its parameters are also in scope).

Tier-2 string evidence (verified present in `.rodata` via
`strings _seqc_compiler.so | grep -E …`):

- ELF section names quoted in source: `.filename`, `.c`, `.asm`,
  `.linenr`, `.nodes`, `.nodes_json`, `.channels`,
  `.required_sample_rate`, `.waveforms`, `.wavemem`,
  `.arguments`, `.version_json`, `.version_bin`.
- JSON keys appearing in `getJsonWaveformMemoryInfo`:
  `"exceedsFpgaMemory"`, `"fpgaMemoryUsed"`.
- JSON keys in `getJsonVersion`/`getJsonArguments`:
  `"compiler"`, `"target"`, `"bitstream"`,
  `"external_triggering"`, `"required_options"`,
  `"destination"`, `"source"`, `"waves"`, `"nodes"`, `"modes"`.
- Header text strings in `getAssemblerHeader`:
  `"// Source file : "`, `"// Compiler    : ziAWG Compiler Version "`,
  `"// Created     : "`, `"// This file was generated automatically, do not edit!"`.

## 2. Overview

| Symbol | Misnomer? | Conf | Justification (≤ 5 words) | Proposal(s) | Status |
|---|---|---|---|---|---|
| `AWGCompiler::impl_` | no | high | sole pimpl pointer | keep current (high) | not-misnomer |
| `AWGCompiler::AWGCompiler::config` (param) | no | high | stored as `config_` ref | keep current (high) | — |
| `AWGCompiler::compileString::source` (param) | no | high | forwarded as SeqC source | keep current (high) | — |
| `AWGCompiler::compileFile::path` (param) | no | high | filesystem path | keep current (high) | — |
| `AWGCompiler::addWaveforms::paths` (param) | no | high | vector of wave-file paths | keep current (high) | — |
| `AWGCompiler::writeToStream::os` (param) | no | high | output stream | keep current (high) | — |
| `AWGCompiler::writeToStream::format` (param) | unsure | low | actually a filename | `outputName`, `filename`, keep current | — |
| `AWGCompiler::writeToFile::path` (param) | no | high | output file path | keep current (high) | — |
| `AWGCompiler::writeAssemblerToFile::path` (param) | no | high | assembler output path | keep current (high) | — |
| `AWGCompiler::setCancelCallback::cb` (param) | no | medium | weak_ptr forwarded | keep current (high) | — |
| `AWGCompiler::setProgressCallback::cb` (param) | no | medium | weak_ptr forwarded | keep current (high) | — |
| `AWGCompilerImpl::config_` | no | high | sibling of `config` ctor arg | keep current (high) | not-misnomer |
| `AWGCompilerImpl::deviceConstants_` | no | high | populated by `getDeviceConstants` | keep current (high) | not-misnomer |
| `AWGCompilerImpl::wavetable_` | no | high | `WavetableFront` shared_ptr | keep current (high) | — |
| `AWGCompilerImpl::wavetableIR_` | no | high | `WavetableIR` shared_ptr | keep current (high) | — |
| `AWGCompilerImpl::pad_0B8_` | no | high | inert padding | keep current (high) | — |
| `AWGCompilerImpl::compiler_` | no | high | embedded `Compiler` instance | keep current (high) | — |
| `AWGCompilerImpl::pad_1F8_` | no | high | inert padding | keep current (high) | — |
| `AWGCompilerImpl::string_200_` | yes | high | source filename of last compile | `sourceFilename_`, `compiledFilePath_`, keep current | — |
| `AWGCompilerImpl::string_218_` | unsure | low | no read/write located | `pad_218_`, keep current | in-scope (nm/strings: no hit → recon-introduced) |
| `AWGCompilerImpl::string_230_` | yes | high | last compiled source text | `sourceText_`, `compiledSource_`, keep current | — |
| `AWGCompilerImpl::string_248_` | yes | high | rendered assembler text | `assemblerText_`, `asmText_`, keep current | — |
| `AWGCompilerImpl::compileMessages_` | no | high | matches compiler-messages role | keep current (high) | — |
| `AWGCompilerImpl::assembler_` | no | high | embedded `AWGAssembler` | keep current (high) | — |
| `AWGCompilerImpl::wavePaths_` | no | high | wave-file path list | keep current (high) | — |
| `AWGCompilerImpl::cancelCallback_` | no | high | `weak_ptr<CancelCallback>` | keep current (high) | — |
| `AWGCompilerImpl::progressCallback_` | no | high | `weak_ptr<ProgressCallback>` | keep current (high) | — |
| `AWGCompilerImpl::pad_2B8_` | no | high | inert padding | keep current (high) | — |
| `AWGCompilerImpl::AWGCompilerImpl::config` (param) | no | high | stored as `config_` ptr | keep current (high) | — |
| `AWGCompilerImpl::compileString::source` (param) | no | high | SeqC source text | keep current (high) | — |
| `AWGCompilerImpl::compileString::isHirzel` (local) | no | high | reads `config_->isHirzel` | keep current (high) | — |
| `AWGCompilerImpl::compileString::deviceByte` (local) | yes | medium | placeholder, mis-typed read | `hasDeviceConstants`, `validDevice` | — |
| `AWGCompilerImpl::compileString::devStr` (local) | no | high | device-name string | keep current (high) | — |
| `AWGCompilerImpl::compileString::asmList` (local) | no | high | vector of `AssemblerInstr` | keep current (high) | — |
| `AWGCompilerImpl::compileString::compileResult` (local) | no | medium | result of `compiler_.compile` | keep current (high) | — |
| `AWGCompilerImpl::compileString::oss` (local) | no | high | text-builder stream | keep current (high) | — |
| `AWGCompilerImpl::compileString::afterLabel` (local) | no | high | label-suppress-indent flag | keep current (high) | — |
| `AWGCompilerImpl::compileString::indent` (local) | no | high | label right-pad count | keep current (high) | — |
| `AWGCompilerImpl::compileString::s` (local) | unsure | low | rendered instruction text | `instrText`, keep current | — |
| `AWGCompilerImpl::compileString::msgs` (local) | no | high | compile-message vector | keep current (high) | — |
| `AWGCompilerImpl::compileString::opcodes` (local) | no | high | assembled opcode words | keep current (high) | — |
| `AWGCompilerImpl::compileString::opcodeCount` (local) | yes | high | counts uint32 words, not opcodes | `opcodeWordCount`, `wordCount` | — |
| `AWGCompilerImpl::compileString::maxSeqLen` (local) | no | high | matches `maxSequenceLen` | keep current (high) | — |
| `AWGCompilerImpl::compileString::msg` (local) | no | high | new compile-message | keep current (high) | — |
| `AWGCompilerImpl::compileString::totalWaveforms` (local) | yes | medium | counts non-null entries only | `nonNullWaveformCount`, `usedWaveformCount` | — |
| `AWGCompilerImpl::compileString::wf` / `ptr` (locals) | no | medium | iterator deref + raw ptr | keep current (medium) | — |
| `AWGCompilerImpl::compileString::maxWaveforms` (local) | no | medium | matches `waveformMemSize` cap | keep current (medium) | — |
| `AWGCompilerImpl::compileString::progress` (local) | no | high | locked progress callback | keep current (high) | — |
| `AWGCompilerImpl::compileFile::path` (param) | no | high | file path | keep current (high) | — |
| `AWGCompilerImpl::compileFile::fpath`/`st`/`ifs`/`oss`/`contents` (locals) | no | high | conventional names | keep current | — |
| `AWGCompilerImpl::addWaveforms::paths` (param) | no | high | vector of wave-file paths | keep current (high) | — |
| `AWGCompilerImpl::addWaveforms::pathStr`/`fpath`/`stem`/`ext` (locals) | no | high | obvious | keep current (high) | — |
| `AWGCompilerImpl::addWaveforms::cancel`/`ifs`/`fileSize`/`rawBuf`/`samples`/`markers` (locals) | no | high | obvious | keep current (high) | — |
| `AWGCompilerImpl::addWaveforms::allMarkerBits` (local) | no | high | OR of marker bits | keep current (high) | — |
| `AWGCompilerImpl::addWaveforms::p`/`end`/`m`/`signal`/`markerBits` (locals) | no | medium | small loop locals | keep current (high) | — |
| `AWGCompilerImpl::writeToStream::os` (param) | no | high | output stream | keep current (high) | — |
| `AWGCompilerImpl::writeToStream::format` (param) | unsure | low | actually output filename | `outputName`, `filename`, keep current | — |
| `AWGCompilerImpl::writeToStream::elfWriter`/`opcodes`/`fmtPath`/`filename`/`sectionName` (locals) | no | high | obvious | keep current (high) | — |
| `AWGCompilerImpl::writeToStream::mappedMode` (local) | unsure | medium | reads `hasPrecomp`, dual role | `precompMode`, `mappedMode`, keep current | — |
| `AWGCompilerImpl::writeToStream::wf`/`wfPtr`/`gap`/`alignMask`/`padding`/`rawData`/`currentOffset` (locals) | no | high | match observed semantics | keep current (high) | — |
| `AWGCompilerImpl::writeToStream::compressSource` (local) | no | high | mirrors `config_->compressSource` | keep current (high) | — |
| `AWGCompilerImpl::writeToStream::compC`/`compAsm` (locals) | no | medium | compressed `.c`/`.asm` blobs | keep current (high) | — |
| `AWGCompilerImpl::writeToStream::lineMap` (local) | no | high | line-number table | keep current (high) | — |
| `AWGCompilerImpl::writeToStream::devType` (local) | no | high | `config_->deviceType` copy | keep current (high) | — |
| `AWGCompilerImpl::writeToStream::nodeList`/`modeMap`/`packed`/`addr`/`direct`/`item`/`node`/`entry`/`it`/`modesArr`/`m`/`items`/`root`/`json` (locals) | no | medium | conventional | keep current (medium) | — |
| `AWGCompilerImpl::writeToStream::chanInfo` (local) | no | high | from `compiler_.getChannelInfo()` | keep current (high) | — |
| `AWGCompilerImpl::writeToStream::sr`/`srf` (locals) | no | high | `deviceSampleRate` (double/float) | keep current (high) | — |
| `AWGCompilerImpl::writeToStream::wfJson`/`wmJson`/`argsJson`/`verJson`/`binVer` (locals) | no | high | self-describing | keep current (high) | — |
| `AWGCompilerImpl::writeToFile::path` (param) | no | high | output file path | keep current (high) | — |
| `AWGCompilerImpl::writeToFile::ofs` (local) | no | high | output filestream | keep current (high) | — |
| `AWGCompilerImpl::writeAssemblerToFile::path` (param) | no | high | output file path | keep current (high) | — |
| `AWGCompilerImpl::writeAssemblerToFile::oss`/`ofs`/`content` (locals) | no | high | obvious | keep current (high) | — |
| `AWGCompilerImpl::getAssemblerHeader::path` (param) | no | high | path printed in banner | keep current (high) | — |
| `AWGCompilerImpl::getAssemblerHeader::banner`/`separator`/`oss`/`now` (locals) | no | high | obvious | keep current (high) | — |
| `AWGCompilerImpl::getJsonArguments::destination` (param) | no | high | becomes JSON `"destination"` | keep current (high) | not-misnomer |
| `AWGCompilerImpl::getJsonArguments::root`/`wavesArr`/`child`/`wp`/`oss` (locals) | no | high | conventional | keep current (high) | — |
| `AWGCompilerImpl::getJsonVersion::root`/`oss` (locals) | no | high | conventional | keep current (high) | — |
| `AWGCompilerImpl::getJsonVersion::ver`/`binVer` (locals) | no | high | LabOne version blob | keep current (high) | — |
| `AWGCompilerImpl::getJsonVersion::targetName` (local) | no | high | mapped device family name | keep current (high) | — |
| `AWGCompilerImpl::getJsonVersion::cf`/`trigMode`/`feature`/`elem`/`optionsArray` (locals) | no | high | role matches name | keep current (high) | — |
| `AWGCompilerImpl::getBinVersion::laboneVer`/`suffix`/`regBase`/`result` (locals) | no | high | match observed values | keep current (high) | — |
| `AWGCompilerImpl::getJsonWaveformMemoryInfo::obj`/`result` (locals) | no | high | json::object builders | keep current (high) | — |
| `AWGCompilerImpl::getJsonWaveformMemoryInfo::exceedsFpga` (local) | no | high | matches `"exceedsFpgaMemory"` | keep current (high) | not-misnomer |
| `AWGCompilerImpl::getJsonWaveformMemoryInfo::totalBytes` (local) | no | high | accumulated wave bytes | keep current (high) | — |
| `AWGCompilerImpl::getJsonWaveformMemoryInfo::alignedAddresses` (local) | no | high | set of aligned base addrs | keep current (high) | — |
| `AWGCompilerImpl::getJsonWaveformMemoryInfo::alignment`/`alignedAddr`/`addr` (locals) | no | high | obvious | keep current (high) | — |
| `AWGCompilerImpl::getJsonWaveformMemoryInfo::cacheType`/`multiplier`/`maxContribution` (locals) | no | high | mirrors `config_->cacheType` switch | keep current (high) | — |
| `AWGCompilerImpl::getJsonWaveformMemoryInfo::wf`/`contrib`/`it` (locals) | no | high | obvious | keep current (high) | — |
| `AWGCompilerImpl::getJsonWaveformMemoryInfo::memSize`/`usage` (locals) | no | high | matches `waveformMemorySize` | keep current (high) | — |
| `AWGCompilerImpl::getCompileReport::oss`/`msg` (locals) | no | high | obvious | keep current (high) | — |
| `AWGCompilerImpl::setCancelCallback::cb` (param) | no | high | weak_ptr argument | keep current (high) | — |
| `AWGCompilerImpl::setProgressCallback::cb` (param) | no | high | weak_ptr argument | keep current (high) | — |
| `compressSourceString::source` (anon ns param) | no | high | input deflate stream | keep current (high) | — |
| `compressSourceString::format` (anon ns param) | yes | medium | only used for error context | `outputName`, `filename`, keep current | — |
| `compressSourceString::strm`/`ret`/`result`/`chunk`/`have` (locals) | no | high | conventional zlib names | keep current (high) | — |

## 3. Detailed findings

### AWGCompilerImpl::string_200_  [yes / high / —]

Evidence:
- `awg_compiler.cpp:135` declaration: `std::string string_200_; // +0x200`.
- `awg_compiler.cpp:550` `string_200_ = path;` inside
  `compileFile`, *just before* `compileString(contents)`.
- `awg_compiler.cpp:529` doc-comment: `Store path into string_200_
  (at this+0x200) — the source filename`.
- `awg_compiler.cpp:1060-1061` `getAssemblerHeader`:
  `if (!string_200_.empty()) { oss << "// Source file : " <<
  string_200_ << "\n"; }` — the literal `"// Source file : "` is in
  `.rodata`.
- `awg_compiler.cpp:1097-1098` `getJsonArguments`:
  `if (!string_200_.empty()) { root.put("source", string_200_); }`
  — JSON key `"source"` matches.

Interpretation:
- The field stores the path passed to the most-recent `compileFile`
  call and is read back as the human-visible "source file" name in
  the assembler header and as the `"source"` field in the JSON
  arguments dump. Two independent string-tier-2 anchors agree:
  `"// Source file : "` (assembler) and `"source"` (JSON).
- The `string_200_` placeholder name encodes the byte offset, not
  the role, and was clearly assigned during initial layout
  reconstruction.

Judgement:
- Yes; placeholder name; the field's role is "the source filename
  of the last `compileFile` invocation".

Proposals:
- `sourceFilename_`     (high)
- `compiledFilePath_`   (medium)
- `inputFilePath_`      (low)
- keep current          (low)

Locations consulted:
- declared: src/awg_compiler.cpp:135
- written: src/awg_compiler.cpp:550
- read:    src/awg_compiler.cpp:1060-1061, 1097-1098

### AWGCompilerImpl::string_218_  [unsure / low / verify-not-original]

Evidence:
- `awg_compiler.cpp:136` declaration: `std::string string_218_; // +0x218`.
- Default-constructed in the ctor's member-init list
  (`awg_compiler.cpp:173`).
- `grep -rn "string_218_" reconstructed/` returns only the
  declaration and the ctor init — **no read or write site**.
- The binary ctor at 0x103b40 zeros 0x70 bytes spanning all four
  string slots plus `compileMessages_`, but the recon notes
  (`awg_compiler.cpp:188`) record this bulk-zero, not which slots
  are subsequently consumed.

Interpretation:
- The field exists by virtue of layout reconstruction (offset
  0x218 = 0x200 + 24, the next `std::string` slot). No reader or
  writer was located. Either there is a consumer the audit missed
  (likely candidate: a debug or `compile_seqc` round-trip path that
  feeds something into one of the 4 strings), or the slot is truly
  unused and should be marked padding.

Judgement:
- Unsure: cannot decide between "live string with unfound consumer"
  and "inert padding" without further symbol-table or trace work.

Proposals:
- keep current  (medium) — preserves layout if a consumer surfaces
- `pad_218_`    (low) — only if synthesis confirms no live consumer

Cross-reference:
- Mirrors the `AWGCompilerConfig::numCores` situation in batch 23
  (write-only field, no located reader, flagged
  `verify-not-original`).

Locations consulted:
- declared: src/awg_compiler.cpp:136
- ctor:     src/awg_compiler.cpp:173

### AWGCompilerImpl::string_230_  [yes / high / —]

Evidence:
- `awg_compiler.cpp:137` declaration: `std::string string_230_; // +0x230`.
- `awg_compiler.cpp:409` `string_230_ = source;` — assigned the
  raw SeqC source string in `compileString`.
- `awg_compiler.cpp:815, 822` in `writeToStream`: passed to
  `compressSourceString(string_230_, format)` (compressed) or
  `elfWriter.addData(string_230_.data(), string_230_.size(), ".c")`
  (raw) — both branches emit the `.c` ELF section, which contains
  the source text.
- ELF section name `".c"` is a confirmed faithful section name.

Interpretation:
- Stores the most-recent SeqC source text, later embedded into the
  ELF as `.c`. Placeholder name; role is unambiguous.

Judgement:
- Yes; placeholder name for the compiled source text.

Proposals:
- `sourceText_`       (high)
- `compiledSource_`   (medium)
- `seqcSource_`       (medium)
- keep current        (low)

Locations consulted:
- declared: src/awg_compiler.cpp:137
- written: src/awg_compiler.cpp:409
- read:    src/awg_compiler.cpp:815, 822

### AWGCompilerImpl::string_248_  [yes / high / —]

Evidence:
- `awg_compiler.cpp:138` declaration: `std::string string_248_; // +0x248`.
- `awg_compiler.cpp:463` `string_248_ = oss.str();` — assigned the
  formatted assembler-text built from the `asmList` in
  `compileString`.
- `awg_compiler.cpp:818, 825` in `writeToStream`: emitted as ELF
  section `.asm` (compressed or raw).
- `awg_compiler.cpp:1000` in `writeAssemblerToFile`:
  `if (string_248_.empty()) return;` (used as the "have we
  compiled anything yet?" guard).
- `awg_compiler.cpp:1012` `oss << string_248_ << "\n";` writes the
  assembler text to the output file body.
- ELF section name `".asm"` is faithful.

Interpretation:
- Stores the rendered, indented assembler-text representation of
  the last compile result. Role is unambiguous and named directly
  by the `.asm` section it backs.

Judgement:
- Yes; placeholder name; canonical role is "assembler text".

Proposals:
- `assemblerText_`  (high)
- `asmText_`        (medium)
- keep current      (low)

Locations consulted:
- declared: src/awg_compiler.cpp:138
- written: src/awg_compiler.cpp:463
- read:    src/awg_compiler.cpp:818, 825, 1000, 1012

### AWGCompilerImpl::compileString::deviceByte  [yes / medium / —]

Evidence:
- `awg_compiler.cpp:391`
  `bool deviceByte = (deviceConstants_.deviceType != 0); // simplified check`
- Used at `awg_compiler.cpp:393` and `:400` to gate the two
  `ZIAWGCompilerException` throws (Hirzel vs non-Hirzel branch).
- `awg_compiler.cpp:358-359` doc-comment in the function header:
  `If config->isHirzel … is true AND this->deviceConstants_ byte
  at +0x0C (relative to this) is 0`. (Note: the comment says
  byte at `+0x0C of this` which is *inside* `deviceConstants_`,
  not `deviceConstants_.deviceType` itself; recon then inserted
  the simplification visible at line 391.)
- The recon body is admittedly a `// simplified check` — the
  same line records that the underlying binary read is a single
  byte at a not-yet-pinned offset within `deviceConstants_`.

Interpretation:
- The local is a placeholder boolean for whatever device-validity
  byte the binary tests; the recon names it `deviceByte` (a
  description of the bit-level read) instead of its semantic role
  ("device is supported / has constants populated").
- The name "deviceByte" describes the *source* of the value, not
  the boolean question it answers; combined with the explicit
  `// simplified check` self-flag this is recon scaffolding.

Judgement:
- Yes; placeholder name; the value is a "device-supported / has
  constants" predicate.

Proposals:
- `hasDeviceConstants`  (medium)
- `validDevice`         (medium)
- `deviceSupported`     (low)
- keep current          (low) — only if the underlying byte's
  semantic is later shown to be a raw flag with no cleaner label

Locations consulted:
- declared/used: src/awg_compiler.cpp:391, 393, 400

### AWGCompilerImpl::compileString::opcodeCount  [yes / high / —]

Evidence:
- `awg_compiler.cpp:475-479`
  ```
  auto const& opcodes = assembler_.getOpcode();
  size_t opcodeCount = (opcodes.size());  // in uint32_t words
  size_t maxSeqLen = deviceConstants_.maxSequenceLen;
  if (opcodeCount / 4 > maxSeqLen) {
  ```
- The recon comment `// in uint32_t words` and the `/ 4` guard at
  `:479` together establish that `opcodes.size()` is a count of
  32-bit *words*, and the actual instruction (opcode) count is
  `opcodeCount / 4`. The recon disassembly note at the same line
  is `@0x10739e: sar $2, %r15; cmp %rcx, %r15`, i.e. arithmetic
  shift right by 2 — divide-by-4 in instruction units.
- `notes/symbol-renaming-audit/13_awg_assembler_impl.md` and
  `notes/symbol-renaming-audit/33_awg_assembler.md` discuss
  `getOpcode()` returning the raw uint32 word stream.
- The error message at `:484` reports `static_cast<uint64_t>(opcodeCount / 4)`
  as the human-visible "opcode count", confirming the divided
  value is the true opcode count.

Interpretation:
- Local `opcodeCount` is named after a count of opcodes but holds
  a count of uint32 words; the actual opcode count is
  `opcodeCount / 4`. The unit mismatch is documented in the same
  line's trailing comment.

Judgement:
- Yes; the name describes the wrong quantity.

Proposals:
- `opcodeWordCount`  (high)
- `wordCount`        (medium)
- `opcodeWords`      (medium)
- keep current       (low)

Locations consulted:
- declared/used: src/awg_compiler.cpp:476, 479, 484

### AWGCompilerImpl::compileString::totalWaveforms  [yes / medium / —]

Evidence:
- `awg_compiler.cpp:491-499`
  ```
  size_t totalWaveforms = 0;
  for (auto it = wavetableIR_->begin(); ...) {
      auto const& wf = *it;
      auto const* ptr = wf.get();
      if (ptr) {
          // Check if waveform is a "playback" type and count it
          totalWaveforms++;
      }
  }
  ```
- The loop body's own comment (`Check if waveform is a "playback"
  type and count it`) telegraphs that the binary's loop has
  filtering criteria the recon has not yet reproduced; the recon
  currently counts every non-null entry.
- The result is then compared against
  `deviceConstants_.waveformMemSize` at `:501-502` — a
  *byte-count* limit, not a per-waveform count.
- `notes/symbol-renaming-audit/31_device_constants.md` records
  `waveformMemSize` as "max waveform memory in samples".

Interpretation:
- The local sums non-null waveform pointers, not waveforms-of-a-
  particular-kind, and is then compared against a size-in-samples
  cap. Both the unit mismatch and the recon's own "playback
  filter" comment flag the name as overclaiming.

Judgement:
- Yes; the name overstates what is counted (and the comparand has
  the wrong unit), reflecting the recon's not-yet-pinned filter.

Proposals:
- `nonNullWaveformCount`  (medium)
- `usedWaveformCount`     (medium) — if the binary's filter is in
  fact `markedForLoad`/`used`
- keep current            (low) — only if the loop body is later
  reconstructed to truly sum a "total waveforms" quantity in the
  same units as `waveformMemSize`

Cross-reference:
- The recon's `waveformMemSize > totalWaveforms` test is unit-
  mismatched (samples vs waveform-count); incidental finding,
  belongs in `incidental_findings.md`. Not addressable here.

Locations consulted:
- declared/used: src/awg_compiler.cpp:491-509

### AWGCompiler::writeToStream::format & AWGCompilerImpl::writeToStream::format  [unsure / low / —]

Evidence:
- `awg_compiler.hpp:58` `void writeToStream(std::ostream& os, std::string const& format);`
  — public facade signature.
- `awg_compiler.cpp:742, 804, 928` — every read of the parameter
  treats it as a *path* and feeds it through
  `boost::filesystem::path(format).filename().string()` to derive
  the section name and the JSON `"destination"` value.
- `awg_compiler.cpp:964-971` `writeToFile(path)` calls
  `writeToStream(ofs, path)` — i.e. `format` receives the *file
  path* the user wants to write to.
- The string is also forwarded into `compressSourceString(..., format)`
  where it is *only* used as an error-message context tag
  (`awg_compiler.cpp:70, 83, 93`).
- The argument is *not* a "format selector" (e.g. "ELF" vs "JSON")
  — the function unconditionally writes ELF.

Interpretation:
- The parameter is consistently treated as a filename (or path),
  not as a format selector. The `format` name suggests the latter
  and is misleading at every call site.
- The corresponding parameter in `writeToFile` is named `path`,
  and the public/impl pair forward to each other — see also the
  identically-misnamed `format` in the local
  `compressSourceString::format` helper.

Judgement:
- Unsure-leaning-yes: the name describes a role (format selector)
  the value does not play. Held at "unsure" because the public
  facade signature is part of the user-facing API and the original
  Python binding may rely on the parameter name as a keyword
  argument; cannot verify the API binding here.

Proposals:
- `outputName`     (medium) — matches the JSON `"destination"` key
- `filename`       (medium) — matches the actual usage
- `outputPath`     (low)
- keep current     (medium) — only if Python binding pins the kwarg

Cross-reference:
- `compressSourceString::format` (in this batch) — same misnomer
  in the helper that consumes the value purely for error context.
- `writeToFile::path` (this batch) — same role, honest name; the
  pair is internally inconsistent (`writeToFile` calls
  `writeToStream(ofs, path)`).

Locations consulted:
- declared (facade): include/zhinst/awg_compiler.hpp:58
- declared (impl):   src/awg_compiler.cpp:742, 1262
- used:             src/awg_compiler.cpp:742, 804-808, 815, 818,
                    928-932, 970-971

### compressSourceString::format  [yes / medium / —]

Evidence:
- `awg_compiler.cpp:62`
  `std::string compressSourceString(std::string const& source, std::string const& format)`
- The helper is in the file's anonymous namespace (recon-coined,
  not in `nm`), so its parameter names are in scope.
- `awg_compiler.cpp:70, 83, 93` — every use of `format` is
  `ErrorMessages::format(ErrorMessageT(0x1e), format)` — the
  parameter is purely an error-context tag.
- The helper is called only from `writeToStream` (`:815, 818`),
  always passing the misnamed outer `format` parameter through
  unchanged; that outer parameter is itself a filename (see the
  block above).

Interpretation:
- The name `format` here is inherited from the outer
  misnomer. Inside the helper, the value is unambiguously a
  filename used purely as error context.

Judgement:
- Yes; the name describes a role the value does not play; the
  real role is "filename used for error message".

Proposals:
- `outputName`   (medium)
- `filename`     (medium)
- `errorContext` (low)
- keep current   (low)

Cross-reference:
- Coupled with `writeToStream::format`; rename them together.

Locations consulted:
- declared/used: src/awg_compiler.cpp:62, 70, 83, 93

### AWGCompilerImpl::writeToStream::mappedMode  [unsure / medium / —]

Evidence:
- `awg_compiler.cpp:766-792`
  ```
  bool mappedMode = deviceConstants_.hasPrecomp;
  if (mappedMode) {
      // Mapped mode: padSize=0, ByName order
      ...
      elfWriter.addWaveform(wf, ..., /*mapped=*/true, /*padSize=*/0);
  } else {
      // Absolute mode: compute padding, ByIndex order
      ...
      elfWriter.addWaveform(wf, ..., /*mapped=*/false, padding);
  }
  ```
- The *underlying* read is `deviceConstants_.hasPrecomp` (verified
  in `notes/symbol-renaming-audit/31_device_constants.md` as the
  precompensation/DA-support flag at `+0x88`).
- The local then *renames* this to `mappedMode` because that's how
  the value is interpreted at the next-tier consumer
  (`elfWriter.addWaveform(..., mapped=true/false, ...)`).

Interpretation:
- Two reasonable names: `precompMode` (faithful to the source
  field) or `mappedMode` (faithful to the consumer call). The
  current name is the latter; not strictly wrong, but it elides
  the `hasPrecomp` provenance and could plausibly mislead a reader
  to expect a separate "is the wavetable mapped" field.

Judgement:
- Unsure: the name fits one valid view of the value but obscures
  its origin; whether that is a misnomer is a judgement call.

Proposals:
- `precompMode`  (medium) — faithful to source field
- `mappedMode`   (medium) — keep current; faithful to consumer
- keep current   (medium)

Cross-reference:
- Depends on `DeviceConstants::hasPrecomp` (batch 31) staying as
  is. If batch 31 ever renames `hasPrecomp_` to a "mapped mode"
  name, this local should follow.

Locations consulted:
- declared/used: src/awg_compiler.cpp:766, 767, 770-790

### AWGCompilerImpl::config_  [no / high / not-misnomer]

Evidence:
- `awg_compiler.cpp:127` declaration: `AWGCompilerConfig const* config_; // +0x000`.
- Stored from the ctor `config` parameter (`:163`, `:184`).
- Read at >25 sites in this file as `config_->deviceType`,
  `config_->isHirzel`, `config_->cacheType`,
  `config_->compressSource`, `config_->sampleFormat`,
  `config_->addressImpl`, `config_->deviceSampleRate` — every
  read is the canonical config-pointer dereference.
- Every other `Compiler*` / `Prefetch*` / `CustomFunctions*` class
  in this codebase has an identically-named `config_` member
  (per batches 07/09/30/23).

Interpretation:
- Field name matches its role and matches the convention used
  across the cluster anchored by batch 23.

Judgement:
- Not a misnomer.

Proposals:
- keep current  (high)

Locations consulted:
- declared: src/awg_compiler.cpp:127
- read:     src/awg_compiler.cpp:164, 245, 297-299, 390, 392, 396,
            403, 759, 766, 771, 786, 812, 840, 903, 914, 1142,
            1162, 1165, 1170, 1181, 1221

### AWGCompilerImpl::deviceConstants_  [no / high / not-misnomer]

Evidence:
- `awg_compiler.cpp:128` declaration:
  `DeviceConstants deviceConstants_; // +0x008 (0x90 bytes)`.
- Initialized via `getDeviceConstants(config.deviceType)` at
  `:164` — the canonical producer per
  `notes/symbol-renaming-audit/31_device_constants.md`.
- Read in `getBinVersion` (`:245`),
  `getJsonWaveformMemoryInfo` (`:297, 300, 301, 334`),
  `compileString` (`:477, 501`), and `writeToStream` (`:766`) —
  every read names the field `deviceConstants_` and reaches into
  it for the same fields documented in batch 31.
- Passed by reference to `WavetableFront` ctor (`:166`),
  `Compiler` ctor (`:170`), and `AWGAssembler` ctor (`:177`).

Interpretation:
- Standard "device constants" dependency-injection field; name
  matches role and convention.

Judgement:
- Not a misnomer.

Proposals:
- keep current  (high)

Locations consulted:
- declared: src/awg_compiler.cpp:128
- written: src/awg_compiler.cpp:164
- read:    src/awg_compiler.cpp:166, 170, 177, 245, 297, 300, 301,
           334, 477, 501, 766

### AWGCompiler::impl_  [no / high / not-misnomer]

Evidence:
- `awg_compiler.hpp:71` declaration:
  `AWGCompilerImpl* impl_;  // +0x00, sole member`.
- Allocated `new AWGCompilerImpl(config)` in the facade ctor
  (`awg_compiler.cpp:1238`).
- Every facade method body is `impl_->method(...)` — 12 forwarding
  call sites, all consistent.
- The facade is documented as a "thin pimpl wrapper" and the
  binary symbol-table assertion that "all methods: `mov rdi,[rdi];
  jmp Impl::method`" (header comment) is consistent with `impl_`
  being the sole indirection.

Interpretation:
- `impl_` is the canonical pimpl-pointer name used throughout the
  C++ community and matches the role here.

Judgement:
- Not a misnomer.

Proposals:
- keep current  (high)

Locations consulted:
- declared: include/zhinst/awg_compiler.hpp:71
- used:     src/awg_compiler.cpp:1238, 1244-1246, 1251, 1255,
            1259, 1263, 1267, 1271, 1275, 1279, 1283, 1287

### AWGCompilerImpl::getJsonArguments::destination  [no / high / not-misnomer]

Evidence:
- `awg_compiler.cpp:1090, 1094`
  ```
  std::string AWGCompilerImpl::getJsonArguments(std::string const& destination) const {
      ...
      root.put("destination", destination);
  ```
- The string literal `"destination"` is the JSON key the parameter
  becomes. Tier-2 evidence: the key is present verbatim in
  `.rodata`.
- The caller in `writeToStream` (`:929-930`) computes
  `filename2 = fmtPath2.filename().string()` and passes it as
  `destination` — i.e. the parameter does receive the output
  filename, which is exactly what the JSON `"destination"` field
  records.

Interpretation:
- Parameter name matches the JSON key it produces and the role of
  the value; a clean alignment between source name and emitted
  string.

Judgement:
- Not a misnomer.

Proposals:
- keep current  (high)

Locations consulted:
- declared: src/awg_compiler.cpp:1090
- used:     src/awg_compiler.cpp:1094
- caller:   src/awg_compiler.cpp:929-930

### AWGCompilerImpl::getJsonWaveformMemoryInfo::exceedsFpga  [no / high / not-misnomer]

Evidence:
- `awg_compiler.cpp:293, 328, 336, 345`
  - declaration: `bool exceedsFpga = false;`
  - written `true` when set membership fails (`:328`) or when the
    accumulated `totalBytes` exceeds the device memory cap
    (`:336`).
  - emitted as `result["exceedsFpgaMemory"] = exceedsFpga;` at
    `:345`.
- JSON key `"exceedsFpgaMemory"` is faithful (test diff guide in
  `AGENTS.md` lists `.wavemem` → `{"exceedsFpgaMemory": ...,
  "fpgaMemoryUsed": ...}`).

Interpretation:
- The local is the bool that becomes the JSON `"exceedsFpgaMemory"`
  field. Name (`exceedsFpga`) matches the key (modulo the
  trailing "Memory" word).

Judgement:
- Not a misnomer.

Proposals:
- keep current  (high)

Locations consulted:
- declared/used: src/awg_compiler.cpp:293, 328, 336, 345

## 4. Symbols inspected and judged routinely fine

Public facade params:
- `AWGCompiler::AWGCompiler::config` — stored as `config_` ref;
  matches sibling-class convention (Compiler/Prefetch/etc).
- `AWGCompiler::compileString::source` — forwarded as the SeqC
  source string.
- `AWGCompiler::compileFile::path`, `writeToFile::path`,
  `writeAssemblerToFile::path` — all unambiguously file paths.
- `AWGCompiler::addWaveforms::paths` — `vector<string> const&` of
  wave-file paths; matches role.
- `AWGCompiler::writeToStream::os` — `std::ostream&`; conventional.
- `AWGCompiler::setCancelCallback::cb`,
  `setProgressCallback::cb` — `std::weak_ptr<...>`; conventional.

Impl params (mirrors of facade params, same conclusions):
- `AWGCompilerImpl::AWGCompilerImpl::config`,
  `compileString::source`, `compileFile::path`,
  `addWaveforms::paths`, `writeToStream::os`, `writeToFile::path`,
  `writeAssemblerToFile::path`, `getAssemblerHeader::path`,
  `setCancelCallback::cb`, `setProgressCallback::cb`.

Impl fields (data members not flagged above):
- `wavetable_` — `shared_ptr<WavetableFront>`; matches.
- `wavetableIR_` — `shared_ptr<WavetableIR>`; matches.
- `compiler_` — embedded `Compiler`; matches batch 07 convention.
- `compileMessages_` — `vector<CompilerMessage>`; matches role
  (`getCompileReport` iterates it; `compileString` clears and
  populates it).
- `assembler_` — `AWGAssembler`; matches batch 33 convention.
- `wavePaths_` — `vector<string>` of wave-file paths; populated
  by `addWaveforms`, read by `getJsonArguments`.
- `cancelCallback_`, `progressCallback_` — `weak_ptr`s; matches
  setter names.
- `pad_0B8_`, `pad_1F8_`, `pad_2B8_` — inert layout filler.

Locals across all bodies that are conventional and self-describing
(stream builders `oss`/`ofs`/`ifs`, paths `fpath`/`fmtPath`,
loop variables `it`/`p`/`end`/`m`/`wp`/`feature`, json builders
`root`/`obj`/`items`/`elem`/`entry`/`modesArr`, signal-construction
locals `samples`/`markers`/`markerBits`/`signal`,
`allMarkerBits`, time `now`, version `ver`/`binVer`/`laboneVer`,
suffix string pointer `suffix`, target string pointer
`targetName`, JSON outputs `wfJson`/`wmJson`/`argsJson`/`verJson`,
ELF metadata `regBase`/`memSize`/`alignment`/`alignedAddresses`/
`alignedAddr`/`addr`/`contrib`/`maxContribution`/`multiplier`/
`cacheType`, ELF builder `elfWriter`, opcode access `opcodes`,
section name `sectionName`, stream content `content`,
`compC`/`compAsm`, line table `lineMap`, channel info `chanInfo`,
device-type `devType`, sample rate `sr`/`srf`,
node-list machinery `nodeList`/`modeMap`/`packed`/`addr`/`direct`/
`item`/`node`/`json`, custom-functions pointer `cf`, trigger mode
`trigMode`, callback lock `progress`/`cancel`, raw waveform
buffers `rawBuf`/`fileSize`).

Standalone:
- `compressSourceString::source` — input to deflate; conventional
  zlib-stream parameter name.
- zlib state `strm`, `chunk`, `have`, `ret`, `result` — all
  standard zlib idioms.

## 5. Coverage

- **Fully covered:**
  - `AWGCompiler` (facade): the sole field `impl_` and all 12
    method parameters.
  - `AWGCompilerImpl`: all 18 instance data members
    (`config_`, `deviceConstants_`, `wavetable_`, `wavetableIR_`,
    `pad_0B8_`, `compiler_`, `pad_1F8_`, four `string_*_` slots,
    `compileMessages_`, `assembler_`, `wavePaths_`,
    `cancelCallback_`, `progressCallback_`, `pad_2B8_`).
  - All in-scope parameters of all in-scope methods: ctor,
    `compileString`, `compileFile`, `addWaveforms`,
    `writeToStream`, `writeToFile`, `writeAssemblerToFile`,
    `getAssemblerHeader`, `getJsonArguments`,
    `getJsonVersion` (no params), `getBinVersion` (no params),
    `getJsonWaveformMemoryInfo` (no params),
    `getCompileReport` (no params),
    `setCancelCallback`, `setProgressCallback`.
  - In-batch local variables across every method body, with
    flagged candidates promoted to §3 and routine ones listed in
    §4.
  - Anonymous-namespace helper `compressSourceString` parameters
    and locals (function name itself is recon-coined and so is
    in scope, but no plausible better name was identified —
    omitted from the table).

- **Deferred:** none.

- **Not covered (out of scope per RULES §2/§3):**
  - Type names `AWGCompiler` and `AWGCompilerImpl` — present in
    the binary symbol table.
  - All method names on both classes — present in `nm` output as
    `t` symbols (12 facade methods + ~14 impl methods, including
    `nodeListToJson` which is a real binary method even though
    recon emits its body inline).
  - `zhinst::(anonymous namespace)::writeWavesToElfMapped` and
    `…::writeWavesToElfAbsolute` — function names present as
    `t` symbols; only their bodies (in a different file) are
    addressable. Their parameter names belong to whichever batch
    audits `write_waves_to_elf.cpp`.
  - `zhinst::util::wave::awg2double`/`awg2marker`/`awg2double16`
    — present in `nm`; out of scope here.
  - `getDeviceConstants` (free function) — present in `nm`,
    addressed in batch 31.
  - `ErrorMessages::format`, `ErrorMessageT` (enum/macro), `CalVer`,
    `getLaboneVersion`, `asBinary`, `formatTime`,
    `ZIAWGCompilerException` — all lie in other batches.
  - `boost::*`, `std::*`, `zlib` symbols — third-party; out of
    scope per §2.

- **Cross-batch arbitration outcomes:**
  - All `config_->*` reads in this file rely on names already
    vindicated by batch 23 (`isHirzel`, `cacheType`,
    `compressSource`, `deviceType`, `sampleFormat`,
    `addressImpl`, `deviceSampleRate`). No new arbitration is
    raised against batch 23.
  - `mappedMode` local is informally coupled to
    `DeviceConstants::hasPrecomp` (batch 31); not promoted to
    `cross-batch-arbitration` because both names are individually
    defensible.
  - `compileString::totalWaveforms` involves a unit-mismatched
    comparison with `DeviceConstants::waveformMemSize`; this is
    an *incidental finding* about the recon body, not a name
    issue, and belongs in `incidental_findings.md` rather than
    against batch 31.

- **Status:** `complete` (single-batch).
