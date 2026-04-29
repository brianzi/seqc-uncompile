# Batch 14 — waveform

> **Phase S.1 reconciliation note (2026-04-29)**: This batch contains
> 4 row(s) from the 226-item Phase Q backlog. Per-row triage:
> B1 (mechanical, staged for S.2): 0; B2 (borderline, deferred): 4;
> B3 (already resolved during Phase D/R): 0;
> B4 (wontfix / kept-as-is): 0.
> The authoritative per-row table lives in
> [`SYNTHESIS.md` §6](SYNTHESIS.md#6-low-confidence-and-unsure-parked--reconciled-in-phase-s1).
> Individual rows below are NOT struck through; consult §6 for status.

## 1. Files considered

- `reconstructed/include/zhinst/waveform.hpp`
- `reconstructed/src/waveform.cpp`
- `reconstructed/src/util_wave.cpp`

Symbol-table reference: `nm --demangle _seqc_compiler.so`.

## 2. Overview

| Symbol | Misnomer? | Conf | Justification (≤5 words) | Proposal(s) | Status |
|---|---|---|---|---|---|
| `WaveformFile` (struct name) | unsure | low | binary uses nested `File` | `Waveform::File` (already alias), keep current | in-scope (nm: binary defines zhinst::Waveform::File, not WaveformFile) |
| `WaveformFile::name` | no | high | matches usage | keep current | not-misnomer |
| `WaveformFile::formatType` | no | medium | matches csv_parser dispatch | keep current | not-misnomer |
| `WaveformFile::columnMode` | yes | low | placeholder; never read | unknownField1C, keep current | — |
| `WaveformFile::isIntegerFormat` | no | medium | toggles integer/float path | keep current | not-misnomer |
| `WaveformFile::data` | yes | high | actually SHA-1 hash | hash, fileHash, contentHash | — |
| `WaveformFile::Type` (enum) | no | high | in symbol table | keep current | not-misnomer |
| `WaveformFile::Type::CSV/RAW/GEN` | no | medium | match `"csv"/"raw"/"gen"` strings | keep current | not-misnomer |
| `WaveformFile::typeToStr::type` (param) | no | high | trivially obvious | keep current | — |
| `WaveformFile::typeFromStr::str` (param) | no | medium | generic but fits | keep current | — |
| `WaveformFile::operator==::other` (param) | no | high | conventional | keep current | — |
| `WaveformFile::WaveformFile::filename` (param) | no | high | conventional | keep current | — |
| `Waveform::name` | no | high | JSON key `"name"` | keep current | not-misnomer |
| `Waveform::waveformType` | unsure | low | redundant prefix | type, fileType, keep current | — |
| `Waveform::secondaryName` | yes | high | JSON key `"functionArgs"` | functionArgs, funcArgs | — |
| `Waveform::file` | no | high | self-evident | keep current | not-misnomer |
| `Waveform::used` | no | high | JSON key `"load"` | keep current | not-misnomer |
| `Waveform::addressValue` | unsure | low | mildly generic | address, globalAddress, keep current | — |
| `Waveform::funDescrName` | yes | medium | matches `"genFunc"` better | genFunc, genFuncName, keep current | — |
| `Waveform::playWord` | yes | high | JSON key `"playConfig"` | playConfig, playConfigWord | — |
| `Waveform::waveIndex` | no | high | JSON key `"waveIndex"` | keep current | not-misnomer |
| `Waveform::seqRegWidth` | yes | medium | actually min sample length | minLengthSamples, minSamples | — |
| `Waveform::allocationByteSize` | unsure | medium | byte-size; may be samples | allocationSize, allocSamples, keep current | — |
| `Waveform::deviceConstants` | no | high | self-evident | keep current | not-misnomer |
| `Waveform::signal` | no | high | nested member | keep current | not-misnomer |
| `Waveform::Waveform(13-arg)::name_` (param) | no | high | trailing-underscore convention | keep current | — |
| `Waveform::Waveform(13-arg)::type_` (param) | no | medium | matches member `waveformType` | keep current | — |
| `Waveform::Waveform(13-arg)::secondaryName_` | yes | high | inherits secondaryName misnomer | functionArgs_ | coordinated-rename |
| `Waveform::Waveform(13-arg)::file_` (param) | no | high | trivial | keep current | — |
| `Waveform::Waveform(13-arg)::used_` (param) | no | high | trivial | keep current | — |
| `Waveform::Waveform(13-arg)::addr_` (param) | no | medium | obvious | keep current | — |
| `Waveform::Waveform(13-arg)::genFunc_` (param) | no | high | matches JSON key | keep current | not-misnomer |
| `Waveform::Waveform(13-arg)::playWord_` | yes | high | inherits playWord misnomer | playConfig_ | coordinated-rename |
| `Waveform::Waveform(13-arg)::waveIndex_` | no | high | matches member | keep current | — |
| `Waveform::Waveform(13-arg)::seqRegWidth_` | yes | medium | inherits seqRegWidth misnomer | minLengthSamples_ | coordinated-rename |
| `Waveform::Waveform(13-arg)::allocationByteSize_` | unsure | medium | inherits ambiguity | keep current | coordinated-rename |
| `Waveform::Waveform(13-arg)::dc_` (param) | no | high | trivial | keep current | — |
| `Waveform::Waveform(13-arg)::signal_` (param) | no | high | trivial | keep current | — |
| `Waveform::Waveform(rename)::source` (param) | no | high | obvious | keep current | — |
| `Waveform::Waveform(rename)::newName` (param) | no | high | obvious | keep current | — |
| `Waveform::Waveform(rename)::src` (local) | no | high | obvious | keep current | — |
| `Waveform::Waveform(copy)::other` (param) | no | high | conventional | keep current | — |
| `Waveform::operator==::other` (param) | no | high | conventional | keep current | — |
| `Waveform::fromJson::json` (param) | no | high | conventional | keep current | — |
| `Waveform::fromJson::dc` (param) | no | high | obvious | keep current | — |
| `Waveform::fromJson::nameStr` (local) | no | high | matches | keep current | — |
| `Waveform::fromJson::typeJsonStr/typeCStr/typeLocalStr` (locals) | no | low | reconstruction-only | keep current | — |
| `Waveform::fromJson::fileType` (local) | no | high | obvious | keep current | — |
| `Waveform::fromJson::secondaryNameStr` (local) | yes | medium | inherits misnomer | functionArgsStr | coordinated-rename |
| `Waveform::fromJson::filePtr` (local) | no | high | obvious | keep current | — |
| `Waveform::fromJson::usedVal` (local) | no | high | obvious | keep current | — |
| `Waveform::fromJson::addrVal` (local) | no | high | obvious | keep current | — |
| `Waveform::fromJson::genFuncStr` (local) | no | high | obvious | keep current | — |
| `Waveform::fromJson::playConfigVal` (local) | no | high | matches JSON key | keep current | — |
| `Waveform::fromJson::waveIndexVal` (local) | no | high | obvious | keep current | — |
| `Waveform::fromJson::minLengthVal` (local) | no | medium | matches JSON key | keep current | — |
| `Waveform::fromJson::allocSizeVal` (local) | no | medium | matches JSON key | keep current | — |
| `Waveform::fromJson::sig` (local) | no | high | obvious | keep current | — |
| `Waveform::getSizePerDevice` (no params) | — | — | — | — | — |
| `Waveform::getSizePerDevice::channels` (local) | no | high | matches signal field | keep current | — |
| `Waveform::getSizePerDevice::length` (local) | no | high | matches signal field | keep current | — |
| `Waveform::getSizePerDevice::dc` (local) | no | high | obvious | keep current | — |
| `Waveform::getSizePerDevice::minLen` (local) | no | medium | clamp lower bound | keep current | — |
| `Waveform::getSizePerDevice::granularity` (local) | no | high | divisor for round-up | keep current | — |
| `Waveform::getSizePerDevice::quotient/remainder/rounded` (locals) | no | high | math accumulators | keep current | — |
| `Waveform::getSizePerDevice::sizeTimesChannels` (local) | no | medium | descriptive | keep current | — |
| `Waveform::getSizePerDevice::bitsPerSample` (local) | no | high | matches DC field | keep current | — |
| `Waveform::getSizePerDevice::totalBits` (local) | no | high | descriptive | keep current | — |
| `Waveform::getSizePerDevice::bitRemainder` (local) | no | high | descriptive | keep current | — |
| `Waveform::getSizePerDevice::bytes` (local) | no | high | matches return | keep current | — |
| `util::wave::double2awg::sample` (param) | no | high | matches usage | keep current | — |
| `util::wave::double2awg::marker` (param) | no | high | masked into low 2 bits | keep current | not-misnomer |
| `util::wave::double2awg::scaled/rounded` (locals) | no | high | math accumulators | keep current | — |
| `util::wave::double2awg::kFullScale` (constant) | no | high | matches rodata 8191.0 | keep current | not-misnomer |
| `util::wave::double2awg1m::sample/marker/scaled/rounded/kFullScale` | no | high | analogous | keep current | not-misnomer |
| `util::wave::double2awg16::sample/scaled/rounded/kFullScale/r` | no | high | analogous | keep current | — |
| `util::wave::hash::filePath` (param) | no | high | matches usage | keep current | — |
| `util::wave::hash::sha/in/buf/n/digest_bytes/out/w/i` (locals) | no | high | conventional | keep current | — |

## 3. Detailed findings

### WaveformFile (struct name)  [unsure / low / verify-not-original]

Evidence:
- `nm --demangle ... | grep WaveformFile` returns nothing.
- `nm --demangle ... | grep "Waveform::File"` returns the type and its
  methods (typeToStr/typeFromStr/operator==).
- `waveform.hpp:48` declares `struct WaveformFile`.
- `waveform.hpp:97` adds `using File = WaveformFile;` so that
  `Waveform::File` resolves to it.

Interpretation:
- The original symbol is the nested type `Waveform::File`. We chose
  the top-level name `WaveformFile` to avoid a forward-declaration
  ordering problem and added the `using File = WaveformFile` alias.
  The original binary has no `WaveformFile` symbol; only
  `Waveform::File` exists.

Judgement:
- Unsure: not strictly a misnomer (the alias preserves
  `Waveform::File`), but the top-level type name is a reconstructor
  invention rather than the binary's name.

Proposals:
- Move the body inside `class Waveform` and drop the top-level
  alias  (low) — closer to original layout.
- Keep current  (medium) — works, alias preserves the original
  qualified name.

Locations consulted:
- declared: include/zhinst/waveform.hpp:48,97
- used:     src/waveform.cpp throughout
- nm output for `Waveform::File*`

### WaveformFile::columnMode  [yes / low / —]

Evidence:
- Defined in waveform.hpp:51.
- Set to `0` in `WaveformFile::WaveformFile(const char*)`
  (waveform.cpp:991).
- Set to `1` in `waveform_ir.cpp:45,65` (initial WaveformIR setup).
- Read only in `Waveform::File::operator==` (waveform.cpp:779).
- A single comment at `csv_parser.cpp:718` says "columnMode
  determines if single or multi value per line", but no actual code
  in csv_parser.cpp reads the field; the value is never branched on.

Interpretation:
- The field is written but never used to drive control flow. Its
  intended meaning ("single vs multi value per line") is asserted in
  a comment without supporting code paths. Other format-driving
  decisions actually go through `formatType` and `isIntegerFormat`.

Judgement:
- Yes: the name claims a behavioural role that the codebase doesn't
  exercise; effectively an unread placeholder.

Proposals:
- `unknownField1C`           (low)  — reflects "set but never read".
- Keep current               (medium) — comment-grade hypothesis,
  hard to disprove without binary.

Locations consulted:
- declared: include/zhinst/waveform.hpp:51
- used:     src/waveform.cpp:779,991; src/waveform_ir.cpp:45,65;
            src/csv_parser.cpp:718 (comment only)

### WaveformFile::data  [yes / high / —]

Evidence:
- Declared `std::vector<unsigned int> data;` in waveform.hpp:54.
- waveform.hpp:54 trailing comment: "file hash (from
  CachedParser::getHash)".
- Assignment site `csv_parser.cpp:489`:
  `fileRef.data = cache.getHash(fileRef.name);`
- Same pattern at `csv_parser.cpp:810`.
- Lookup uses it as the cache key:
  `cache.getCachedFile(fileRef.data)` (csv_parser.cpp:493,812).
- `util::wave::hash` (util_wave.cpp:129) returns
  `vector<unsigned int>` of 5 SHA-1 words.
- `WaveformFile::operator==` compares it byte-for-byte
  (waveform.cpp:797-805) — i.e. fingerprint equality.

Interpretation:
- The vector stores the SHA-1 fingerprint of the source file's
  contents (5 32-bit words = 160 bits) and is used as a cache key.
  It is never the file's data.

Judgement:
- Yes: `data` describes the wrong thing; the field is the file's
  content hash.

Proposals:
- `hash`         (high)
- `contentHash`  (high)
- `fileHash`     (medium)

Locations consulted:
- declared: include/zhinst/waveform.hpp:54
- used:     src/csv_parser.cpp:488,489,493,809,810,812;
            src/waveform_ir.cpp:41,62 (clear/shrink_to_fit);
            src/waveform.cpp:797-805

### Waveform::secondaryName  [yes / high / —]

Evidence:
- Field at +0x20 (waveform.hpp:102).
- `toJson` writes it under JSON key `"functionArgs"` (waveform.cpp:84,
  string at rodata 0x90a35a).
- `fromJson` reads `"functionArgs"` into it (waveform.cpp:208-211).
- `waveform_ir.cpp:165-166` serializes it under key `"function"`
  (`result.put("function", secondaryName);`).
- `seqc_ast_nodes_evaluate.cpp:6791` writes it from
  `result->name_` (e.g. `"zeros(64)"`), i.e. the textual form of a
  generator-function call.

Interpretation:
- The field consistently carries the **function-call argument string**
  (a printed expression like `"zeros(64)"`) and is faithfully
  serialized as `"functionArgs"` in JSON. The current name
  `secondaryName` is a generic placeholder ("a second name") that
  does not describe this role at all.

Judgement:
- Yes: tier-2 evidence (faithful JSON key) plus the assignment
  site identify the field as the generator-call arguments string;
  `secondaryName` is uninformative.

Proposals:
- `functionArgs`  (high)
- `funcArgs`      (medium)

Cross-reference:
- All call/copy sites referring to `secondaryName` (constructor
  param, fromJson local) are listed as `coordinated-rename`.

Locations consulted:
- declared: include/zhinst/waveform.hpp:102
- used:     src/waveform.cpp:84,114,156,208-211,353,485-487,
            606,849,914,966; src/waveform_ir.cpp:108,134,165-166;
            src/wavetable_manager_ir.cpp:154,155;
            src/seqc_ast_nodes_evaluate.cpp:6789,6791;
            src/waveform_front.cpp:89

### Waveform::funDescrName  [yes / medium / —]

Evidence:
- Declared at waveform.hpp:107, JSON key `"genFunc"`
  (waveform.cpp:88).
- Read in `custom_functions.cpp:1107`:
  `if (!waveform->file && waveform->funDescrName().empty())` —
  used to detect "this waveform was created by a generator function".
- `waveform_front.hpp:104-106` exposes accessors named
  `funDescrName()` / `setFunDescrName(...)`.
- `placeholder_field_names.md:18` lists `thirdString → funDescrName`
  i.e. `funDescrName` itself was a renaming pass output.

Interpretation:
- The field carries the name of the SeqC generator function (e.g.
  `"sine"`, `"zeros"`, `"gauss"`). The serialized key is `"genFunc"`,
  consistent with that meaning. `funDescrName` ("function
  description name") is awkward and not used elsewhere in this
  codebase as a recognized term; it doesn't appear in any JSON key,
  rodata string or comment in the binary that would justify the
  exact form.

Judgement:
- Yes (low-confidence form): the name's meaning fits but the
  spelling is non-canonical relative to the JSON key and to the
  variable/parameter `genFunc` used everywhere else (constructor
  parameter, fromJson local).

Proposals:
- `genFunc`        (high)   — matches JSON key + every local name.
- `genFuncName`    (medium)
- Keep current     (low)

Locations consulted:
- declared: include/zhinst/waveform.hpp:107
- used:     src/waveform.cpp:88,118,357,519-523,619,865,927,954,964;
            src/waveform_ir.cpp:81,112;
            src/wavetable_manager_front.cpp:197,221;
            src/custom_functions.cpp:1107;
            src/waveform_front.cpp:93;
            include/zhinst/waveform_front.hpp:104-106

### Waveform::playWord  [yes / high / —]

Evidence:
- Field at +0x68 (waveform.hpp:108), declared `uint32_t`.
- JSON key `"playConfig"` (waveform.cpp:93, rodata 0x90a3ad).
- `custom_functions_io.cpp:484` and `:479` write it from
  `PlayConfig::encodeCwvf(...)`:
  `wf->playWord = playConfig.encodeCwvf(/*defaultRate=*/-1);`
- `asm_commands.cpp:1100`:
  `currentWvf->playWord = node->config.encodeCwvf(-1);`
- Batch 38 (`38_play_config.md`) documents that `encodeCwvf` returns
  the packed PlayConfig "word" (`PlayConfig::encodeCwvf::word`
  local).

Interpretation:
- The value stored is the packed `PlayConfig` word produced by
  `PlayConfig::encodeCwvf`. It is faithfully serialized as
  `"playConfig"`. The current name `playWord` describes the value's
  *shape* (a 32-bit word) but not its semantics; `playConfig`
  matches the JSON key (tier-2 evidence) and the producing API.

Judgement:
- Yes: a faithful-serializer JSON key plus uniform "encodeCwvf
  result" provenance both point at `playConfig`; `playWord` is the
  weaker name.

Proposals:
- `playConfig`       (high)
- `playConfigWord`   (medium)
- Keep current       (low)

Cross-reference:
- 13-arg ctor parameter `playWord_` carries the same misnomer →
  `coordinated-rename`. See batch 38 for `PlayConfig::encodeCwvf`.

Locations consulted:
- declared: include/zhinst/waveform.hpp:108
- used:     src/waveform.cpp:36,54,93,119,287,358,463,525-528,
            580-582,624-625,812,820,830,867-869,933-934;
            src/custom_functions_io.cpp:479,484;
            src/asm_commands.cpp:1091-1100;
            src/waveform_front.cpp:94;
            src/waveform_ir.cpp:82,113

### Waveform::seqRegWidth  [yes / medium / —]

Evidence:
- Field at +0x70 (waveform.hpp:110), declared `int`.
- JSON key `"minLengthSamples"` (waveform.cpp:90, rodata 0x90a386).
- Initialized from `dc.waveformGranularity` (32) in
  `waveform_ir.cpp:115` and `waveform_front.cpp:96`.
- Re-assigned from `devConst_->waveformMinSamples` in
  `custom_functions_io.cpp:496` (note: device_constants.hpp:71
  comments call this exact field "WaveformFront.seqRegWidth").
- Used as a sample-count clamp in `wavetable_ir.cpp:807-808`:
  `size_t maxSamples = static_cast<size_t>(wf->seqRegWidth);`
  with comment "Clamp: binary uses cmova → max(aligned,
  seqRegWidth)".
- Used as a sample length in `prefetch_prepare.cpp:699`:
  `int minLenSamples = waveform2->seqRegWidth;`
- Used as `unsigned int minSamples` in `custom_functions.cpp:656`.
- Batch 31 (`31_device_constants.md`:266,529) flagged the related
  `DeviceConstants::waveformMinSamples` and notes "values match
  initial seqRegWidth".

Interpretation:
- The value is consistently used as the **minimum waveform length
  in samples**, sourced from `DeviceConstants::waveformMinSamples` /
  `waveformGranularity`, serialized as `"minLengthSamples"`, and
  consumed everywhere as a sample count. The current name
  `seqRegWidth` ("sequencer register width") suggests a hardware
  register bit-width concept, which does not fit any usage site.

Judgement:
- Yes: a faithful-serializer JSON key plus several
  sample-count-typed reads point at "min length in samples";
  `seqRegWidth` is misleading.

Proposals:
- `minLengthSamples`  (high)
- `minSamples`        (medium)
- Keep current        (low)

Cross-reference:
- 13-arg ctor parameter `seqRegWidth_` and `fromJson::minLengthVal`
  → `coordinated-rename`. See batch 31 for sibling
  `DeviceConstants::waveformMinSamples`.

Locations consulted:
- declared: include/zhinst/waveform.hpp:110
- used:     src/waveform.cpp:38,90,121,303,344,360,465,535-538,
            582-583,629,812,820,830,875-877,933,936;
            src/waveform_ir.cpp:84,115;
            src/waveform_front.cpp:96;
            src/custom_functions.cpp:655-656;
            src/custom_functions_io.cpp:494-496;
            src/wavetable_ir.cpp:795-808;
            src/prefetch_prepare.cpp:695-699;
            include/zhinst/device_constants.hpp:71,77

### Waveform::allocationByteSize  [unsure / medium / —]

Evidence:
- Field at +0x74 (waveform.hpp:111), declared `int`.
- JSON key `"allocationSize"` (waveform.cpp:91, rodata 0x90a397) —
  the JSON key does **not** contain "byte".
- Demangled 13-arg constructor signature (`nm --demangle`):
  `Waveform(string, Type, string, shared_ptr<File>, bool,
   AddressImpl<unsigned int>, string, int, int, int,
   AddressImpl<unsigned int>, DeviceConstants const&, Signal)`
  i.e. **slot 11** (which initializes +0x74) is
  `AddressImpl<unsigned int>`, not `int`. Our header/ctor declare
  it as `int allocationByteSize_`.
- `wavetable_ir.cpp:333` stores `allocationBytes` (a byte count
  including bits-per-sample math) into it.
- `prefetch_helpers.cpp:635`:
  `uint32_t wfmSize = (uint32_t)wfm->allocationByteSize;`
  consumed as a byte size (compared to `maxAlloc`).
- `prefetch_placesingle.cpp:1109`: `uint32_t byteSize =
  waveform->allocationByteSize;`
- Memory-allocator audit (`02_memory_allocator.md:307,315,344`)
  treats it as a byte count.

Interpretation:
- The arithmetic that fills the field clearly produces a **byte
  count** (samples * channels * bitsPerSample / 8 with rounding,
  see `getSizePerDevice` for the same shape), and downstream
  consumers store it into `uint32_t byteSize`. The trailing
  "ByteSize" portion of the field name therefore matches usage.
- However: (a) the JSON key is just `"allocationSize"`, not
  `"allocationByteSize"`; (b) the binary's constructor signature
  reveals the parameter type is actually `AddressImpl<unsigned int>`
  (an address/offset wrapper), not `int`. The `int` declaration in
  the header is a recon error; the *name* may also subtly mis-state
  whether the unit is bytes or address words.

Judgement:
- Unsure: the unit (bytes) is well-supported by usage, but the
  type mismatch with the binary signature and the simpler JSON key
  leave room for the original name to have been `allocationSize`
  (matching the key) with the byte-vs-sample distinction implicit
  in the type.

Proposals:
- `allocationSize`       (medium) — matches JSON key (tier-2).
- Keep current           (medium) — correct unit, reads cleanly.
- `allocationOffset`     (low)    — if the AddressImpl type means
  offset rather than size.

Side observation (per RULES §2a):
- The 13-arg ctor's parameter type for slot 11 is
  `detail::AddressImpl<unsigned int>` per the demangled symbol.
  The header currently declares `int allocationByteSize_` and the
  field as `int allocationByteSize`. This is a type-suspicion to
  follow up; record under this block, not as a separate type-only
  finding.

Locations consulted:
- declared: include/zhinst/waveform.hpp:111
- used:     src/waveform.cpp:39,52,91,122,313-321,361,466,
            540-543,584-585,630,812,821,832,879-881,933,937;
            src/wavetable_ir.cpp:260-333;
            src/prefetch_helpers.cpp:635-643;
            src/prefetch_placesingle.cpp:1109;
            src/incidental_findings.md:911 (allocator role)
- nm:       `_ZN6zhinst8WaveformC2E...` (full ctor signature)

### Waveform::addressValue  [unsure / low / —]

Evidence:
- Field at +0x4C (waveform.hpp:106), declared `uint32_t`.
- 13-arg ctor parameter type per demangled signature:
  `AddressImpl<unsigned int>` (slot 6).
- JSON key `"globalAddress"` (waveform.cpp:87).
- Used as a memory address/offset in `wavetable_ir.cpp:328,365,
  396,417,462,464`, `prefetch_*.cpp` and `asm_commands.cpp` —
  always as the waveform's memory base.
- `prefetch_splitplay.cpp:198` even has the comment
  "(formerly waveformOffset)".

Interpretation:
- Genuinely a memory address/offset; the JSON key is
  `"globalAddress"`. `addressValue` is generic but not actively
  misleading.

Judgement:
- Unsure: not a hard misnomer, but `address` or `globalAddress`
  would carry more meaning and match the JSON key.

Proposals:
- `address`         (medium)
- `globalAddress`   (medium) — matches JSON key (tier-2).
- Keep current      (medium)

Locations consulted:
- declared: include/zhinst/waveform.hpp:106
- used:     widespread (see grep output for `addressValue`)

### Waveform::waveformType  [unsure / low / —]

Evidence:
- Field at +0x18 (waveform.hpp:100), type `WaveformFile::Type`.
- JSON key `"type"` (waveform.cpp:83).
- Read in `wavetable_ir.cpp:283` as
  `if (wf->waveformType == Waveform::File::Type{})`.

Interpretation:
- Inside class `Waveform`, the prefix `waveform` is redundant
  noise — the field is just "type" of the underlying file (CSV /
  RAW / GEN). The serialized key is the bare `"type"`.

Judgement:
- Unsure (mild): not actively misleading, but
  `waveform.waveformType` is awkward where `waveform.type` (or
  `fileType`) would be clearer and match the JSON key.

Proposals:
- `type`       (medium) — matches JSON key.
- `fileType`   (medium) — matches the locals `fileType` used in
  fromJson and in our reconstruction comments.
- Keep current (medium)

Locations consulted:
- declared: include/zhinst/waveform.hpp:100
- used:     src/waveform.cpp:60,83,100,482,603,844,911;
            src/wavetable_ir.cpp:257,283

### Positive blocks (not-misnomer)

#### Waveform::name / Waveform::file / Waveform::used  [no / high / not-misnomer]

Evidence:
- JSON keys: `"name"` (waveform.cpp:82), `"fileName"` (file's name,
  waveform.cpp:85), `"load"` (used, waveform.cpp:86).
- `file` is `shared_ptr<WaveformFile>`; the only thing it can be is
  the source file.
- `used` is set in `WavetableFront::loadWaveform` as a "this
  waveform is referenced" flag.

Interpretation:
- All three are tier-2 (faithful JSON key) or trivially
  self-describing.

Judgement:
- No: names match.

Proposals: none.

#### WaveformFile::Type members CSV/RAW/GEN  [no / medium / not-misnomer]

Evidence:
- typeToStr static map in waveform.cpp:672-676 maps
  CSV→"csv", RAW→"raw", GEN→"gen" (rodata strings 0x90a3b8/bc/c0).

Interpretation:
- Enumerator names match the rodata strings exactly (case-folded);
  RTTI-style mapping qualifies as tier-2 evidence.

Judgement:
- No.

Proposals: none.

#### util::wave::double2awg::marker, kFullScale  [no / high / not-misnomer]

Evidence:
- util_wave.cpp:43,51: constant 8191.0 then `(marker & 0x3u) +
  (rounded * 4)`. Marker is masked into the low 2 bits — that's
  what AWG marker bits are.
- 8191.0 = 2^13 − 1, the documented "full-scale" for the 14-bit
  signed AWG sample format (UHFLI/UHFQA path).

Interpretation:
- Both names are technically accurate descriptions of well-known
  AWG sample-packing constants.

Judgement:
- No.

Proposals: none.

## 4. Symbols inspected and judged routinely fine

- `WaveformFile::name` — bare `string`, used as filesystem path /
  identifier; matches.
- `WaveformFile::formatType` — drives csv_parser dispatch (auto /
  AWG / multi-column); see `csv_parser.cpp:312,355,357,...`.
- `WaveformFile::isIntegerFormat` — boolean flag for AWG-integer
  vs float CSV cell parsing; see `csv_parser.cpp:145,207,...`.
- `WaveformFile::operator==::other` — conventional.
- `WaveformFile::WaveformFile(const char*)::filename` —
  conventional.
- `WaveformFile::typeToStr::type`, `typeFromStr::str` — trivially
  obvious, matches usage (note `str` is generic but parameter is a
  bare string lookup, no role to expand).
- `Waveform::deviceConstants` — pointer to `DeviceConstants`;
  trivially descriptive.
- `Waveform::signal` — embedded `Signal`; trivial.
- `Waveform::waveIndex` — JSON key `"waveIndex"`, also used
  consistently in `wavetable_ir.cpp:471`, `custom_functions_io.cpp:486`,
  asm_commands and prefetch as the AWG wave-table slot index.
  Tier-2 positive; flagged `not-misnomer` in §2.
- `Waveform::Waveform(rename)::source/newName` — obvious.
- `Waveform::operator==/copy ctor/fromJson::other`/`json`/`dc` —
  conventional.
- All `Waveform::Waveform(13-arg)::*_` parameter underscores —
  follow the project's "constructor param mirrors member with
  trailing underscore" convention; only flagged where the member
  itself is flagged.
- `Waveform::getSizePerDevice` locals (`channels`, `length`, `dc`,
  `minLen`, `granularity`, `quotient`, `remainder`, `rounded`,
  `sizeTimesChannels`, `bitsPerSample`, `totalBits`, `bitRemainder`,
  `bytes`) — each describes its computed role; nothing misleading
  even though `minLen` and `granularity` ultimately come from
  DC fields with different names (`waveformGranularity` and
  `waveformPageSize` respectively — see batch 31). The names here
  describe their *role* in this function (lower clamp /
  divisor), not the source field they came from, and that role
  reading is correct.
- `util::wave::double2awg::sample`, `double2awg1m::sample/marker`,
  `double2awg16::sample`, `hash::filePath` — match usage.
- `util::wave::hash` locals (`sha`, `in`, `buf`, `n`, `digest_bytes`,
  `out`, `w`, `i`) — conventional SHA / I/O accumulator names.

## 5. Coverage

**Fully covered:**
- All in-scope symbols of `WaveformFile` and `Waveform` declared in
  `waveform.hpp` (fields, member methods' parameters and named
  locals, the nested `Type` enum and its members).
- Free functions in `util_wave.cpp`: `double2awg`, `double2awg1m`,
  `double2awg16`, `hash` — parameters, local variables, named
  constants.

**Deferred:**
- `Waveform::Waveform()` defaulted default constructor — no params,
  nothing to audit.
- The `~Waveform()` body is empty; nothing to audit.
- Reconstruction-only locals in `Waveform::fromJson` such as
  `obj`, `nameJsonStr`, `typeJsonStr`, `typeCStr`, `typeLocalStr`,
  `funcArgsJsonStr`, `funcArgsCStr`, `fileNameJsonStr`,
  `fileNameLen`, `fileNameCStr`, `addrJson`, `ec`/`ec2`,
  `genFuncJsonStr`, `genFuncCStr`, `allocJson`, `allocSizeVal` —
  these mirror inline boost::json operations the binary expanded
  in place; their names track the JSON keys closely and were
  judged routinely fine in §4 (covered as a single bullet rather
  than enumerated individually).

**Not covered (out of scope per RULES §2/§3):**
- Method names: `Waveform::Waveform`, `~Waveform`, `toJson`,
  `fromJson`, `getSizePerDevice`, `operator==`, `getName`,
  `Waveform::File::typeToStr`, `typeFromStr`, `operator==`. All
  appear in `nm --demangle` output for `_seqc_compiler.so`
  (tier-1 authoritative).
- Type names `Waveform`, `Waveform::File`, `Waveform::File::Type`
  also appear in mangled symbols → excluded.
- Free functions `util::wave::double2awg`, `double2awg1m`,
  `double2awg16`, `hash` are tier-1 in nm output.
- Member type alias `Waveform::File = WaveformFile` (alias) —
  excluded by §2 ("member type aliases").
- Template specializations and stdlib SSO machinery referenced
  inline in the .cpp comments — out of scope.

**Anomaly noted (not a renaming finding, recorded for synthesis):**
- The 13-arg `Waveform` constructor's slot-11 parameter (which
  initializes `+0x74 = allocationByteSize`) is
  `detail::AddressImpl<unsigned int>` per the demangled symbol,
  not `int` as our header declares. Recorded under
  `Waveform::allocationByteSize` block as a §2a side observation.
