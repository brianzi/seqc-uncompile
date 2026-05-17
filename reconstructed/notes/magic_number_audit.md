# Magic-number audit — Phase D7-C.1.a report

Read-only audit of the reconstructed SeqC compiler under
`reconstructed/include/zhinst/` and `reconstructed/src/`.  Catalogs
sites where raw integer literals (`0xN`, decimal sentinels, bare
`static_cast<EnumT>(N)`) appear where a named constant or enumerator
would be clearer, plus literal-only patterns (repeated cross-file
magic values, unnamed bitmap LUTs, ad-hoc sentinels).

This is the **C.1.a deliverable**.  It is the read of the codebase;
the corresponding write (the literal→name patches) is staged behind
user review of this report as **C.1.b..h**.

**Scope (in)**: `reconstructed/include/zhinst/{asm,ast,codegen,core,
device,infra,io,runtime,waveform}/`, `reconstructed/src/<same>/`,
and `reconstructed/src/pybind_seqc.cpp`.

**Scope (out)**: `reconstructed/notes/`, `reconstructed/notes/archive/`,
`reconstructed/build/`, `tools/seqcc/`, `tests/`, generated lexer
/parser files (`src/ast/seqc_lexer.c`, `src/ast/seqc_parser.tab.*`).

Notes on prior art:

- `notes/magic_numbers_proposal.md` is the original 2025-era
  proposal (Categories A1..A16, B1..B11, C1, D1).  16 of the A
  items are already implemented; this audit cross-checks the rest.
- `notes/error_message_audit.md` is the Phase-57.A.1 inventory of
  every `ErrorMessages::format(...)` template — used here as the
  authority for naming the `ErrorMessageT` literals.
- `notes/special_registers.md` is the authority for the
  `kSuser*` / `kAddr*` address constants in `core/types.hpp`.
- `notes/incidental_findings.md` IF-312 already flags one
  misnamed constant (`kSuserUserRegBase`); this audit picks that
  up as fix-now batch F9 and surfaces sibling issues.

The audit is intentionally not promoted to the Doxygen site — it
is reconstruction metadata, not user-facing reference material per
the Phase D7 "no reconstruction narrative in promoted pages"
principle.  When the C.1.b..h fixes land, the long-lived facts
they encode (new constant names, enum renames) will be picked up
by Doxygen via the enum / constant declarations themselves; this
audit file then becomes historical context.

---

## 1. Methodology

Three parallel read-only explore passes generated the raw evidence
catalogued below:

- **R1** — `static_cast` and enum-related sites.  All 1,708
  `static_cast<…>(…)` hits classified by target type, by direction
  (int→enum vs enum→int), and by syntactic context (in conditional,
  in switch case label, in member initialiser).
- **R2** — pure literal patterns.  Hex literals, typed-suffix
  literals (`27ul`, `0xFFFFu`, `…ULL`), numeric literals in
  conditionals, shift/mask bit-fiddling, sentinels, single-char
  discriminators, string-literal-as-enum.
- **R3** — baseline inventory: every existing enum, every file- and
  namespace-scope named constant, every `static_assert`, plus a
  delta against `magic_numbers_proposal.md`.

Pattern-by-pattern findings are in §2.  The cross-cutting view is
in §3 (hotspots) and §4 (consolidation candidates).  The triage
(what to do about each finding) is in §5..§8.

The audit deliberately does **not** propose new doxygen briefs.  A
named constant or scoped enum will get its own brief when it is
declared; that's Phase D7's job, not this audit's.

---

## 2. Per-pattern raw findings

Per-subsystem density (non-blank, non-comment LOC denominator, raw
match counts in the other columns):

| subsystem | LOC | hex | typed | cond | bits | sent | char | hex/kLOC |
|----|--:|--:|--:|--:|--:|--:|--:|--:|
| asm      | 3321 |  91 |  21 |  46 |  49 |  7 |  3 |  27.4 |
| ast      | 8026 | 237 |   0 | 118 |  11 |  7 |  5 |  29.5 |
| codegen  | 7007 |  94 |  15 | 158 |  44 | 10 | 11 |  13.4 |
| core     | 2196 |  95 |  14 |  32 |  31 |  0 | 67 |  43.3 |
| device   | 2570 | 170 | 103 |  10 |   1 |  0 |  0 |  66.2 |
| infra    |  604 |   7 |   2 |   6 |  13 |  1 |  2 |  11.6 |
| io       |  888 |   4 |   0 |  12 |   0 |  0 |  2 |   4.5 |
| pybind   |  134 |   0 |   1 |   0 |   0 |  0 |  1 |   0.0 |
| runtime  | 9216 | 981 |  40 | 223 |  81 |  2 | 21 | 106.4 |
| waveform | 4315 |  29 |   8 | 145 |  21 |  5 |  0 |   6.7 |

`runtime/` is the densest by a wide margin; the bulk lives in
`custom_functions*.cpp` (devtype/varype membership masks, opcode
encoders) and `csv_parser.cpp` (separator-class LUT + format
discriminators).

### 2.1 `static_cast<…>(…)` (1,708 sites)

Top target types by frequency:

| target | hits |
|---|--:|
| `int` | 307 |
| `size_t` | 114 |
| `uint32_t` | 100 |
| `double` | 66 |
| `int32_t` | 65 |
| `ErrorMessageT` | 40 |
| `AwgDeviceType` | 34 |
| `uint64_t` | 29 |
| `SeqCAstNode&` | 28 |
| `unsigned` (and `unsigned int`) | 51 |
| `Assembler::Command` | 22 |
| `uint16_t` | 18 |
| `float` | 14 |
| `int64_t` | 11 |
| `VarType` | 9 |
| `VarSubType` | 8 |
| `DeviceFamily` | 7 |
| `AccessMode` | 6 |
| `NodeType` | 4 |
| `DeviceTypeCode` | 4 |
| `Tag` | 3 |
| `EDirection` | 2 |
| `Cache::PointerState` | 2 |
| singletons (`ErrorKind`, `ECommandType`, `ValueType`, …) | 1 each |

**Int→enum casts (raw integer → typed enum)**: 152 sites total.
Per-subsystem:

| subsystem | int→enum casts |
|---|--:|
| codegen | 52 |
| runtime | 33 |
| ast     | 18 |
| core    | 15 |
| device  | 14 |
| asm     | 14 |
| waveform|  6 |
| infra/io|  0 |

The most concentrated raw-int-→enum sites:

| file | hits | example |
|---|--:|---|
| `codegen/awg_assembler_opcodes.cpp` | 33 | `ErrorMessages::format(static_cast<ErrorMessageT>(5), val, bits);` |
| `include/zhinst/core/types.hpp` | 13 | `kDevAll = static_cast<AwgDeviceType>(0x1FF)` (intentional in-header) |
| `runtime/csv_parser.cpp` | 12 | CSV parser-state casts |
| `codegen/awg_compiler_config.cpp` | 9 | `if (iequals(str,"cervino")) return static_cast<AwgDeviceType>(1);` |
| `device/device_type.cpp` | 9 | `return static_cast<DeviceTypeCode>(33);`, `DeviceFamily(0x800u)` |
| `ast/seqc_ast_eval_control.cpp` | 8 | `? static_cast<VarType>(0) : ifVal.varType_;` |
| `runtime/custom_functions.cpp` | 8 | `if (scan->varSubType_ == static_cast<VarSubType>(2))` |
| `asm/asm_optimize_regalloc.cpp` | 7 | `cmd == static_cast<Assembler::Command>(4)` (NOP, 4×) |
| `runtime/custom_functions_registers.cpp` | 5 | `addNodeAccess(nodeItem, static_cast<AccessMode>(2));` (×3) |
| `asm/asm_list.cpp` | 4 | `instr.cmd = static_cast<Assembler::Command>(4);` |
| `codegen/awg_compiler.cpp` | 4 | `static_cast<AwgDeviceType>(config_->deviceType));` |
| `codegen/prefetch.cpp` | 3 | `case static_cast<NodeType>(8):` (literal in switch label!) |
| `device/awg_device_props.cpp` | 3 | `return static_cast<AwgDeviceType>(0); // unsupported` |
| `device/device_unknown.cpp` | 2 | `DeviceTypeImpl(DeviceTypeCode(33), DeviceFamily(0x800))` |

**Round-trip enums (both `static_cast<int>(EnumT)` *and*
`static_cast<EnumT>(int)` appear on the same enum within one file)**:

| enum | files |
|---|---|
| `AccessMode` | `runtime/custom_functions_{dio,play,registers}.cpp` |
| `Assembler::Command` | `asm/asm_{expression,list,optimize_regalloc}.cpp`, `codegen/awg_assembler_{impl_pipeline,opcodes}.cpp` |
| `AwgDeviceType` | `codegen/awg_compiler{,_config}.cpp`, `device/awg_device_props.cpp`, `runtime/custom_functions{,_registers}.cpp`, `waveform/wavetable_ir.cpp` |
| `EDirection` | `ast/seqc_ast_eval_arithmetic.cpp` |
| `ErrorMessageT` | `codegen/awg_assembler_opcodes.cpp`, `runtime/custom_functions.cpp`, `runtime/resources.cpp` |
| `NodeType` | `include/zhinst/ast/node.hpp`, `codegen/prefetch.cpp`, `codegen/prefetch_print.cpp` |
| `VarSubType` | `ast/seqc_ast_eval_control.cpp`, `runtime/custom_functions.cpp` |
| `VarType` | `ast/seqc_ast_eval_control.cpp`, `runtime/custom_functions_playback.cpp`, `waveform/waveform_generator.cpp` |
| `DeviceTypeCode` / `DeviceFamily` | `device/device_type.cpp`, `device/device_unknown.cpp` |

`NodeType` operators are explicitly written as round-trips in
`include/zhinst/ast/node.hpp` (`operator|`, `operator&`, mixed
`operator==(NodeType, int)`).  These are intentional — enum-flag
implementation pattern — and stay in DISMISS.

### 2.2 Hex literals (1,708 sites)

Top 30 hex literal *values* by total frequency across the tree:

| count | files | value | observation |
|--:|--:|---|---|
| 21 | 14 | `0x20` | byte sizes / devtype codes |
| 18 |  5 | `0x30` | |
| 18 |  4 | `0x28` | |
| 17 |  5 | `0x18` | |
| 15 | 10 | `0x10` | |
| 14 |  8 | `0xFFFFFFFF` | sentinel (see §2.5) |
| 14 |  5 | `0xFF` | byte mask |
| 14 |  4 | `0x38` | |
| 14 |  1 | `0x27` | **`ErrorMessageT(0x27)` in 14 sites in `seqc_ast_eval_control.cpp`** — message 39 (`%1% expects a var or const argument`); see §2.1 |
| 9  |  6 | `0x3` | |
| 9  |  2 | `0x80000000` | `addressImpl` for AWG (per-device) |
| 9  |  2 | `0x23` | **`ErrorMessageT(0x23)`** = `CsvInconsistentChannels`, 9 sites in `csv_parser.cpp` + 1 in `core/types.hpp` |
| 7  |  7 | `0x8000` | device-trait or nodeType bit |
| 7  |  6 | `0x40` | |
| 7  |  6 | `0x2000` | nodeType / device-trait bit |
| 7  |  4 | `0xFFF` | |
| 7  |  4 | `0x70` | |
| 7  |  4 | `0x58` | |
| 7  |  4 | `0x08` | |
| 7  |  3 | `0x4000000040004041ULL` | **devtype membership LUT — see §4** |
| 7  |  3 | `0x29` | **cmd-set membership LUT** — see §4 |
| 7  |  2 | `0xFFFFF` | 20-bit address clamp in `prefetch_emit.cpp` |
| 7  |  2 | `0x00000000` | |
| 7  |  1 | `0x0d05` | `dc.sequencerRegBase`, 7 sites in `device_constants.cpp` |
| 6  |  6 | `0x1000` | |
| 6  |  3 | `0xF` | |
| 6  |  3 | `0x800` | `DeviceFamily(0x800)` sentinel + device-trait |
| 6  |  3 | `0x6F` | |
| 6  |  3 | `0x54` | **VarType {2,4,6} membership LUT** — see §4 |
| 5  |  5 | `0xD` | |
| 5  |  5 | `0x100` | nodeType / cache-page size |
| 5  |  4 | `0xFFFF` | |
| 5  |  4 | `0x0001…0x0020` (device-trait bits) | factory tables |

### 2.3 Typed-suffix literals (204 sites)

Per-subsystem: `asm` 21, `ast` 0, `codegen` 15, `core` 14, `device`
103, `infra` 2, `io` 0, `pybind` 1, `runtime` 40, `waveform` 8.

Top distinct values: `1u` (21), `15u`/`0u` (15 each), `3u` (8),
`10000u` (8), `202002L` (7, the `__cpp_lib_*` feature-test version),
`0x80000000ull` (7), `0x4000000040004041ULL` (7), `0u` (6),
`2000u`/`1000u` (6 each), `0x00000000u` (6).

Spec-derived constants worth noting:

- **Hashing / RNG**: `0x9e3779b9ULL` (golden ratio, SplitMix-like)
  in `node_map_data.cpp:53,110`; `0x0e9846af9b1a615dULL` in
  `node_map_data.cpp:54,111`; `0x5851f42d4c957f2dULL` (PCG
  multiplier) in `global_resources.cpp:32`; Park–Miller minstd_rand
  `48271ULL`/`2147483647ULL` in `prng_libcxx.cpp:167-168`.
- **Standard library version literal**: `202002L` at the six
  `__cpp_lib_*` feature-test sites in `core/base64.hpp`,
  `core/numeric.hpp`, `core/base64.cpp`, `core/numeric.cpp`.
- **Big-endian byte packing**: `<<24, <<16, <<8` repeated across
  `awg_assembler_opcodes.cpp`, `infra/calver.cpp`,
  `waveform/util_wave.cpp`.
- **64-bit splits**: `>>32` repeated in `runtime/
  custom_functions_play.cpp` (5×), `asm/asm_list.cpp`,
  `asm/assembler.cpp`.
- **Device serial-number thresholds**: 19 boundary serials
  (`1000u`..`240000u`) in `device/serial_predicates.cpp`.

### 2.4 Bit-fiddling

251 sites total (113 shifts, 137 masks, 1 tilde).

Shift counts (top): `>>32` 14, `>>31` 11, `>>1` 10, `<<24` 8, `>>3` 7,
`<<20`/`<<16`/`<<8`/`<<4` 5-6 each.  Mask values (top): `&1` 43,
`&0xFF` 12, `&0xFF00` 5, `&0x3` 7, `&0xFFF` 6, `&0x7` 5, `&0x08` 4.

Combined shift+mask sites worth flagging:

- `src/asm/asm_optimize.cpp:103,203` — `if ((cmdType >> 1) & 1)`
  (also `:174, 759` in `asm_optimize_regalloc.cpp`).
- `src/asm/asm_list.cpp:117` — `bool valid = (result >> 32) & 1;`
- `src/core/base64.cpp:51,52,62,63,68` — base64-pack arithmetic
  `((b0<<4)&0x30) | (b1>>4)` etc.
- `src/device/device_factories.cpp:198` — `switch ((opts >> 6) & 7ul)`
  (subtype slot extraction).
- `src/infra/calver.cpp:262,263` — `(b >> 16) & 0xFF`, `(b >> 12) & 0xF`
  (version-bit unpacking).

The bit-extract idiom `(x >> N) & ((1u << W) - 1)` is **not used**
anywhere; the equivalent is always written with a literal mask.

### 2.5 Sentinels (32 sites)

By kind:

| kind | count |
|---|--:|
| `0xFFFFFFFF` | 12 |
| `== -1` | 11 |
| `!= -1` | 6 |
| `numeric_limits<…>::max()` | 1 |
| `INT_MIN` | 1 |
| `INT_MAX` | 1 |

`0xFFFFFFFF` as sentinel — semantic groupings:

| meaning | named constant | bare-literal sites |
|---|---|---|
| Invalid assembler command | `Assembler::INVALID` (`asm/assembler.hpp:130`) | `src/asm/asm_optimize.cpp:538` (mask-not-sentinel; ambiguous) |
| Empty immediate variant | `ImmediateKind::Valueless` (`ast/value.hpp:86,161`) | `src/ast/value.cpp:99,129,178` (writes / reads as bare literal) |
| Unused cache line | `unusedCacheLine` (`runtime/cache.hpp:346`) | `src/codegen/memory_allocator.cpp:177`, `src/waveform/wavetable_ir.cpp:375,425` |
| (`custom_functions_play.cpp:2379` `value & 0xFFFFFFFF` is a low-32-bit mask, not a sentinel) | — | — |

`-1` as sentinel — already named constants:

- `kRateInherit`, `kNoWaveIndex`, `kNoNodeId`, `kNoPlayIndex` all
  defined as `-1` in `include/zhinst/core/types.hpp:95-98`.
- 11 sites still write the bare `-1` literal (asm/asm_list.cpp,
  ast/node.cpp, codegen/compiler.cpp, codegen/prefetch.cpp ×5,
  codegen/prefetch_emit.cpp ×2, codegen/prefetch_placesingle.cpp,
  waveform/wave_index_tracker.cpp, waveform/wavetable_front.cpp,
  waveform/wavetable_ir.cpp).

`numeric_limits<int64_t>::max()` — single site in
`infra/tracing.cpp:146`; legitimately a clamp.  DISMISS.

`INT_MAX` / `INT_MIN` — `asm/asm_commands.cpp:455,461`; legitimate.
DISMISS.

`size_t(-1)`, `UINT_MAX`, `SIZE_MAX`, `UINT32_MAX`, `UINT64_MAX`,
`-1U` — **no occurrences anywhere**.

### 2.6 Switches on integer (28 sites)

Top discriminators by case count:

1. `src/device/device_type.cpp:496` `switch (v)` 0..19 — returns
   option-name strings (`"MFK"`, `"MD"`, `"FF"`, `"PLL"`, …).
   `DeviceOption` enum exists and could replace; B4 in proposal.
2. `src/device/awg_device_props.cpp:331` `switch (c)` over codes
   4..32 mapping to `AwgDeviceType`; `c` is
   `static_cast<int>(code)`.  `DeviceTypeCode` enum exists; B3.
3. `src/codegen/prefetch_placesingle.cpp:101` `switch (nodeType)`
   with hex-bitmask cases `0x100/0x200/0x2000/0x4000/0x8000`.
   These are unnamed; PROPOSE-ENUM P3.
4. `src/ast/value.cpp` (7 switches): all on `index_` / `other.index_`
   with cases 0/1/2 (variant copy/move/destroy).  `VariantSlot`
   enum proposed by A3 but never created; PROPOSE-ENUM P1.
5. `src/runtime/get_node_map.cpp:1308,1336` — both cast `devType`
   to `int` purely to switch on raw codes.  Same pattern as #2.

Generated-code switches in `seqc_lexer.c` and `seqc_parser.tab.c`
are DISMISS.

### 2.7 If-ladders on the same variable (26 clusters)

Notable:

- `runtime/custom_functions.cpp:384` — 12-branch ladder on `dc`
  with RHS values `1,2,4` repeated.  Discriminator: device class.
- `core/error_kind.cpp:51` — `value` against `0, 5, 9` (matches
  documented `kind == 0 → 0`, `kind == 5 → 0x801f`, `kind == 9 →
  0x800d` mapping).
- `codegen/prefetch_placesingle.cpp:1087/1376/1387/1431` — `nodeType`
  against `0x100, 0x200, 0x2000, 0x4000, 0x8000`.  Same nodeType
  cluster as §2.6 #3.
- `runtime/custom_functions_wait.cpp:290,401,786` — `trigIndex`,
  `oscIdx` against `1,2`.
- `runtime/custom_functions_registers.cpp:132,196` — `argVal`
  against `1,2`.
- `waveform/waveform_generator_dsp.cpp:805,1135` — `args.size()`
  against `5, 4` (signature dispatch); function-arity not enum.

### 2.8 Character-literal discriminators (112 sites)

`'i' 'q' 'm' 'r' 'w'` mode-letters **do not appear** anywhere.

The 112 char literals are dominated by:

- CSV format dispatch in `csv_parser.cpp` (`'%'` comment marker,
  `'0','x'` hex prefix, `'.','e'` float-vs-int).
- URL-encoding LUT in `core/diagnostics_text.cpp`.
- Number / UTF-8 parsing in `core/platform.cpp`.
- Version-string parsing in `infra/calver.cpp`.
- Base64 padding `'='`.

None of these are enum-substitutable; the chars are the protocol.
All DISMISS.

### 2.9 String-literal as enum-equivalent (5 clusters)

| file | variable | strings |
|---|---|---|
| `src/codegen/awg_compiler.cpp` | `ext` | `.csv`, `.txt`, `.bin`, `.bin16`, `.wave`, `.wave16` |
| `src/codegen/compile_seqc.cpp` | `upper` | `QA`, `SG` |
| `src/device/device_type.cpp` | `s` | `none`, `SHFACC`, `DEFAULT`, `SHFPPC2`, `SHFPPC4` |
| `src/io/zi_folder.cpp` | `subdir` | `/data`, `/settings` |
| `src/runtime/resources_static_global.cpp` | `name` | `AWG_INTEGRATION_ARM`, `AWG_MONITOR_TRIGGER`, `DEVICE_SAMPLE_RATE` |

The `ext` cluster maps directly to `WaveformFile::Type` (already
exists with CSV/RAW/GEN); the BIN/BIN16/WAVE/WAVE16 variants are
not yet in the enum.  PROPOSE-ENUM P8.

The `compile_seqc.cpp` `QA/SG` ladder maps to `AwgSequencerType`
(already exists with `Auto/QA/SG`); the ladder is "user-facing
string → enum"; appropriate to keep as a string lookup or
replace with a small `if/else` over enum, depending on use.

The `zi_folder.cpp` cluster maps to `ZiFolder::DirectoryType`
(already exists with 3 enumerators).  Likely a clean PROPOSE-ENUM
swap.

---

## 3. Hotspots

Combining all signals, the densest magic-number files in the tree
(in rough order of total signal-weighted density):

1. **`runtime/custom_functions*.cpp`** — 128 `static_cast<int>` sites,
   12-branch `if (dc==1/2/4)` ladder, 6 `switch(static_cast<int>())`,
   bitmap LUTs `0x4000000040004041ULL` (×6), `0x54` (×6).  Highest
   density of any subsystem (`runtime` is 106 hex/kLOC).
2. **`codegen/prefetch*.cpp`** — 67 `static_cast<int>`, hex-bitmask
   `nodeType` ladder (`0x100`..`0x8000`, 6 sites), `case
   static_cast<NodeType>(8):` literal in switch label, 20-bit
   clamp `0xFFFFF` (×7).
3. **`codegen/awg_assembler_opcodes.cpp`** — 33 `ErrorMessageT(N)`
   bare-id casts, 8 `Assembler::Command` re-tags, 5 `if (opcode ==
   0xF…000000)` chains, opcode-byte tags `|0xF6000000`,
   `|0xFD000000`, `|0xFE000000`.
4. **`codegen/awg_compiler_config.cpp`** — 9 `static_cast<AwgDeviceType>
   (1..256)` returns in `parseCpuType` with no enum name on RHS.
5. **`ast/value.cpp`** — 7 switches on raw `index_` with `case 0:
   case 1: case 2:` (comment-documented variant tags), 3 bare
   `index_ = 0xFFFFFFFF;` writes (should be
   `ImmediateKind::Valueless`).
6. **`device/device_type.cpp` / `device_unknown.cpp`** —
   `DeviceTypeCode(33)`, `DeviceFamily(0x800u)` raw-int
   construction; 20-way switch over option codes 0..19.
7. **`include/zhinst/core/types.hpp`** — 13 `kDev*` constants
   declared as `static_cast<AwgDeviceType>(0xNN)`.  These are the
   *target* of the audit (named constants already), but the
   `static_cast` itself is unavoidable because `AwgDeviceType`
   doesn't enumerate bitmask combinations.  KEEP (DISMISS).
8. **`asm/asm_optimize*.cpp`** — `cmd == Assembler::Command(4)`
   repeated 5× (should be NOP), `(0x29 >> idx) & 1` LUT.

---

## 4. Cross-file repeated magic values

Strongest consolidation candidates (same literal, multiple files,
no shared named constant):

| value | sites | semantic | proposed name |
|---|---|---|---|
| `0x4000000040004041ULL` | `runtime/custom_functions_wait.cpp:72,119,345,487,577`; `runtime/custom_functions_play.cpp:79`; `asm/asm_commands_impl.cpp:22` (7 sites) | bitmap of `(devType-2) ∈ {0,1,6,14,16,30,62}` = {HDAWG, UHFQA, SHFQA, SHFSG, SHFQC_SG, SHFLI} — "device types that support play/wait" | `kPlayWaitDevTypeMask` in `core/types.hpp` (or `runtime/custom_functions_internal.hpp`).  `play.cpp:79` already names it as a local `constexpr kCheckPlaySupportedMask`; hoist that name. |
| `0x800100000000200ULL` | `runtime/csv_parser.cpp:389,478,618,933` (4 sites) | `(mask >> ch) & 1` over ASCII chars ≤ 59 = "CSV separator class" (bits at `,`, `;`, `\t`) | `kCsvSeparatorMask` file-scope `constexpr` in `csv_parser.cpp` |
| `0x29` | `asm/asm_optimize.cpp:533`; `asm/asm_optimize_regalloc.cpp:39,153,730` (4 sites) | `(0x29 >> cmdPlus1) & 1` over `Assembler::Command` values = membership in {0,3,5} | name depends on which cmd-set this is (`kCmdSet_BranchOrJump`? — verify semantics: DEFER D5) |
| `0x54` | `runtime/custom_functions.cpp:884,950`; `runtime/custom_functions_play.cpp:763,771`; `runtime/custom_functions_registers.cpp:251,286` (6 sites) | `(0x54 >> vt) & 1` over `VarType` = membership in {2,4,6} = {`VarType_Var`, `VarType_Const`, `VarType_Cvar`} | `kVarTypeAssignable` (or similar — DEFER D6 for semantic name) |
| `0x27` (= 39 dec) | `ast/seqc_ast_eval_control.cpp:695,712,825,1245,1259,1727,1789,1876,2064,2174,2373,2737,2751,2973` (14 sites) | `ErrorMessages::format(ErrorMessageT(0x27), "if"/…)` — error 39 = `"%1% expects a var or const argument"` | Replace with named enumerator from `ErrorMessageT`.  **The current enum entry at index 39 is also misnamed** (`CantDivConstByWave` per `error_messages.hpp:107`, but `m[39] = "%1% expects a var or const argument"` per `error_messages.cpp:292`) — see **DEFER D7** below; we need to fix the enum name first, then fix the call sites. |
| `0x23` (= 35 dec) | `runtime/csv_parser.cpp:571,713,745,818,888,1011,1035,1085` (8 sites); also `core/types.hpp:205` (1 site — `kAddrInternalTrig=0x23` *unrelated*) | `ErrorMessageT::CsvInconsistentChannels` (`error_messages.hpp:103`); `csv_parser.cpp:571` already has a `// CsvInconsistentChannels` comment | Replace with named enumerator (no rename needed) |
| `0xFFFFFFFF` as sentinel | see §2.5 table | three distinct semantics already named (`Assembler::INVALID`, `ImmediateKind::Valueless`, `unusedCacheLine`) | Replace bare literals with the right named sentinel per site |
| `-1` as sentinel | 11 bare `== -1` / `!= -1` / `return -1;` sites (see §2.5) | four distinct semantics already named in `core/types.hpp:95-98` (`kRateInherit`, `kNoWaveIndex`, `kNoNodeId`, `kNoPlayIndex`) | Replace per site; needs careful per-site classification (some `-1` are local counters / return-code conventions, not the named sentinels) |
| `0x9e3779b9ULL` / `0x0e9846af9b1a615dULL` | `runtime/node_map_data.cpp:53,54,110,111` — same values in two functions of one file | golden-ratio hash + multiplier | hoist to file-scope `kGoldenRatioHash` / `kHashMul` (4 sites collapse to 2 declarations) |
| `kFullScale` | `waveform/util_wave.cpp:43,72,95` — same *name* used three times for different values (8191.0, 16383.0, 32767.0) | 14-bit, 15-bit, 16-bit full-scale | rename to `kFullScale14`, `kFullScale15`, `kFullScale16` (or `…Bits14` etc.) |
| `M_PI` `#define` | `waveform/waveform_generator_dsp.cpp:32`, `waveform/waveform_generator.cpp:40` | guard against `<cmath>` not defining `M_PI` | hoist to a shared header (or use `std::numbers::pi_v<double>` from `<numbers>` in C++20) |

The `kSuser*` vs `DeviceConstants::*` register-address dual scheme
(`core/types.hpp:137-192` vs `device/device_constants.hpp:147-178`)
overlaps ~20 of the same hardware addresses with two parallel
naming conventions.  This is a structural duplication and is
larger than a literal-→-name swap; it belongs in PROPOSE-ENUM /
the sibling C.2 phase, not in this audit's FIX-NOW.

---

## 5. FIX-NOW batches (proposed, awaiting user review)

Each batch is a literal-→named-name substitution against an
existing enum or named constant.  Expected to preserve byte
equality (`diff_test_fast` 2693/2693) because the underlying
integer values do not change.

### F1.  `ErrorMessageT` literal → named enumerator (~57 sites)

Replace `ErrorMessages::format(static_cast<ErrorMessageT>(N), …)`
and `ErrorMessages::format(ErrorMessageT(N), …)` with the named
enumerator from `include/zhinst/core/error_messages.hpp`.

Affected files (cross-checked against `notes/error_message_audit.md`):

| file | hits | values |
|---|--:|---|
| `codegen/awg_assembler_opcodes.cpp` | 33 | `1, 2, 4, 5, 7, 10, …` |
| `ast/seqc_ast_eval_control.cpp` | 14 | `0x27` (= 39) — see D7 below: rename enum entry first |
| `runtime/csv_parser.cpp` | 8 | `0x23` (= 35) = `CsvInconsistentChannels` |
| `runtime/custom_functions.cpp` | (count tbd) | (audit in batch) |
| `runtime/resources.cpp` | (count tbd) | (audit in batch) |

Sub-task: **F1.a** is the 8 csv_parser sites (clean — name exists,
no rename needed); **F1.b** is the 33 opcodes sites (cross-check
each numeric ID against `error_messages.hpp` enum; about 20+
distinct named entries needed); **F1.c** is the 14 control-flow
sites (blocked on **D7** — rename `CantDivConstByWave` →
`VarOrConstExpected` to match the actual template).

### F2.  `Assembler::Command` literal → named enumerator (~12 sites)

Replace `static_cast<Assembler::Command>(N)` / `Assembler::Command(N)`
with the named enumerator from `asm/assembler.hpp:67`.

| file | hits | values |
|---|--:|---|
| `asm/asm_optimize_regalloc.cpp` | 5 | `4` (= `NOP`); some `!= 4` |
| `asm/asm_list.cpp` | 2 | `4` (= `NOP`) |
| `asm/asm_parser_context.cpp` | 1 | `0x02` (= `LABEL`) |
| `asm/asm_expression.cpp` | tbd | |
| `codegen/awg_assembler_{impl_pipeline,opcodes}.cpp` | tbd | |

### F3.  `AccessMode` literal → named enumerator (~5 sites)

Replace `static_cast<AccessMode>(2)` with the named enumerator
from `runtime/custom_functions.hpp:178` (`Direct` = 2 according to
inventory).  Affected: `runtime/custom_functions_{dio,play,
registers}.cpp` (3 sites in registers, 1 each in dio and play).

`AccessMode` already has a `toString` helper (`custom_functions.hpp:
197`); the substitution is mechanical.

### F4.  `VarType` / `VarSubType` literal → named enumerator (~14 sites)

Replace `static_cast<VarType>(0|1)` and `static_cast<VarSubType>(0|2)`
with named enumerators from `runtime/resources.hpp:55,109`.

Affected: `ast/seqc_ast_eval_control.cpp` (8), `runtime/
custom_functions.cpp`, `runtime/custom_functions_playback.cpp`,
`waveform/waveform_generator.cpp`.

`VarType(0)` and `VarSubType(0)` are used as "invalid/none"
sentinels in `seqc_ast_eval_control.cpp`.  Confirm that an
enumerator with value 0 exists (e.g. `VarType_None`) — if not,
this becomes PROPOSE-ENUM (P-item) instead.

### F5.  Hoist `0x4000000040004041ULL` mask (7 sites → 1 named constant)

Declare `constexpr uint64_t kPlayWaitDevTypeMask = 0x4000000040004041ULL;`
once (best home: `include/zhinst/core/types.hpp` near the existing
`kDev*` constants, or in a new internal `runtime/custom_functions_internal.hpp`
header).  Update 6 function-local `constexpr` sites and 1 inline
literal at `asm_commands_impl.cpp:22` to reference it.  Drop the
duplicate name `kCheckPlaySupportedMask` (`custom_functions_play.cpp:79`)
and `supportedMask` (`custom_functions_wait.cpp:577`).

The brief should explain: bitmap of `(devType - 2)` such that
bit-i set ⇔ device-type `i + 2` supports `playWaveAux` / `wait`.

### F6.  Name `0x800100000000200ULL` (4 sites → 1 named constant)

Declare `constexpr uint64_t kCsvSeparatorMask = 0x800100000000200ULL;`
at file scope in `runtime/csv_parser.cpp`.  Update 4 sites.
The brief should explain the bit-positions (`\t`=9, `,`=44, `;`=59).

### F7.  Hoist `kGoldenRatioHash` / `kHashMul` (4 sites → 2 declarations)

In `runtime/node_map_data.cpp`, move the duplicated function-local
`kGoldenRatioHash` / `kMul` declarations to file scope.  Each
function references the same names.

### F8.  Rename triplicate `kFullScale` in `waveform/util_wave.cpp`

Three local `constexpr kFullScale = 8191.0 / 16383.0 / 32767.0`
declarations have the same name for different values.  Rename to
`kFullScale14` / `kFullScale15` / `kFullScale16` (or `…Bits14`).
Choose the suffix convention to match what `Waveform::File::Type`
or the device bit-width specifications use.

### F9.  IF-312 — rename `kSuserUserRegBase = 0x5F`

Per IF-312, rename to `kAddrOscPhaseReset` (moving it from the
`kSuser*` block to the `kAddr*` block in `core/types.hpp`).  No
callers consume the constant currently, so the rename is a one-edit
patch + IF closure.

Update the `\brief` comment to "Oscillator phase reset (pulse,
written via `st`/`st 0`)."  Cross-reference `special_registers.md`
§8.

### F10.  Sentinel cleanup (~14 sites)

Replace bare `0xFFFFFFFF` and `-1` sentinel literals with the
already-named sentinels per the §2.5 / §4 table.  Each substitution
needs to be verified against the actual semantic (some `-1`s are
local return-code conventions, not the named sentinels).

Sub-task: **F10.a** = `0xFFFFFFFF` substitutions (3 in
`ast/value.cpp`, 1 in `codegen/memory_allocator.cpp`, 2 in
`waveform/wavetable_ir.cpp`); **F10.b** = `-1` substitutions
(per-site triage).

### F-summary

| batch | sites | risk | enum/constant exists? |
|---|--:|---|---|
| F1.a (csv) | 8 | low | yes (`CsvInconsistentChannels`) |
| F1.b (opcodes) | 33 | low | yes (mostly named already) |
| F1.c (control flow) | 14 | medium | **blocked on D7 enum rename** |
| F2 (Assembler::Command) | ~12 | low | yes |
| F3 (AccessMode) | 5 | low | yes |
| F4 (VarType/VarSubType) | 14 | low/medium | yes for non-zero values; verify zero |
| F5 (`kPlayWaitDevTypeMask`) | 7 | low | new constant declaration needed |
| F6 (`kCsvSeparatorMask`) | 4 | low | new constant declaration needed |
| F7 (golden-ratio hash hoist) | 4 → 2 | low | new file-scope declarations needed |
| F8 (kFullScale rename) | 3 | low | rename in place |
| F9 (IF-312 rename) | 1 | low | rename in place; closes IF-312 |
| F10.a (`0xFFFFFFFF`) | 6 | low | yes |
| F10.b (`-1`) | 11 | medium | yes; per-site triage |

Total FIX-NOW patches: ~120 individual literal-→name substitutions
across ~25 files.

---

## 6. DEFER findings (file as IF-313..IF-318)

These have a clear literal but the right *name* requires evidence
beyond what this audit gathered.  Each should be filed as an IF
with the call-site table and the proposed-but-unverified semantic,
then picked up when a GDB / disassembly trace confirms intent.

### D1 (IF-313) — `0x29` cmd-set membership LUT

4 sites in `asm/asm_optimize*.cpp` test `(0x29 >> cmdPlus1) & 1`
to check if a command is in `{0, 3, 5}` after `+1` adjustment.
The set semantic (which cmds, what they have in common) isn't
documented; GDB-confirm whether this is e.g. "branch/jump cmds" or
"flow-control cmds" before naming.

### D2 (IF-314) — `case static_cast<NodeType>(8):` literal label

`codegen/prefetch.cpp` has a `case static_cast<NodeType>(8):`
switch label with a comment `// 0x1d1273` (a binary address).
`NodeType::Loop` is `0x0008` per `notes/struct_layouts.md`; if
this is the same value, the substitution is trivial (and is a
FIX-NOW once confirmed).  Quick check, low risk.

### D3 (IF-315) — `DeviceTypeCode(33)` / `DeviceFamily(0x800u)` sentinels

`device/device_type.cpp` and `device/device_unknown.cpp` construct
these as "unknown" instances.  `DeviceTypeCode` / `DeviceFamily`
both have `Unknown = 0` per R3 — so why `33` and `0x800u`?  Two
hypotheses: (a) `33` and `0x800u` are sentinel-but-not-zero values
chosen for "outside the valid range"; (b) the enums are missing
the right enumerator.  GDB-confirm which.

### D4 (IF-316) — `parseCpuType` raw-numeric returns

`codegen/awg_compiler_config.cpp:38` returns 9 raw integer values
from a string-keyed lookup.  Need to confirm each maps to an
existing `AwgDeviceType` enumerator before naming.  If they don't,
this is PROPOSE-ENUM (add the missing values to `AwgDeviceType`)
not FIX-NOW.

### D5 (IF-317) — `holdSuppressExceptSigouts = 0x27C`

Named constant in `play_config.hpp:83` but the value derivation
(636 = 0x27C) isn't documented.  Check whether 0x27C is composable
from the other `*Mask` constants in the same file.  May be a
DISMISS once confirmed.

### D6 (IF-318) — `0x54` VarType-set semantic name

`{2,4,6} = {VarType_Var, VarType_Const, VarType_Cvar}` per the
existing enum, but the *group* name (why these three, what they
have in common) isn't documented.  Likely "assignable" or
"non-string non-wave"; needs the caller-side semantic check.

### D7 (IF-319) — `CantDivConstByWave` enum entry is misnamed

`error_messages.hpp:107` declares `CantDivConstByWave = 39`, but
`error_messages.cpp:292` defines `m[39] = "%1% expects a var or
const argument"`.  All 14 known users (`seqc_ast_eval_control.cpp`)
pass control-flow keywords (`"if"`, `"while"`, `"switch"`, `"do"`,
`"repeat"`) as the first format arg, consistent with the cpp side.
The enum entry needs to be renamed (proposed: `VarOrConstExpected`
or `ExpectsVarOrConst`).  **F1.c is blocked on this rename**
because we can't substitute a wrong name for a wrong literal.

This is technically a name-vs-template mismatch and should also be
filed as an incidental finding regardless of the audit outcome.

---

## 7. DISMISS

Patterns that show up in the recon but are not productive
substitution targets:

- **Loop counters and small arithmetic literals**: `for (i = 0; i
  < 5; ++i)`, `if (n < 7)`, `i * 8`, `byte_size <= 0x10`, etc.
  The number *is* the meaning.
- **ABI / sizeof literals** in `static_assert`s.  These are
  invariants of the libstdc++/libc++ choice, not magic numbers
  that want a name.
- **Generated lexer/parser files** (`src/ast/seqc_lexer.c`,
  `src/ast/seqc_parser.tab.[ch]`).  Auto-generated; do not touch.
- **Mathematical constants**: `M_PI` (the value *is* the name, but
  see F-hoist note in §4 about the duplicate `#define`), Park–
  Miller `48271`, PCG `0x5851f42d4c957f2dULL`, golden-ratio
  `0x9e3779b9ULL` (covered by F7 hoist).
- **Unicode codepoint boundaries** (`0xD800`, `0xDC00`, `0x10000`,
  `0xC0`, `0xE0`, `0xF0`, `0xF8`, `0xBF`) in
  `core/diagnostics_text.cpp` and `core/platform.cpp`.  These are
  Unicode standard values; they're as named as they get.
- **Big-endian byte assembly** (`<<24, <<16, <<8`).  The shift
  count *is* the byte position.
- **Field-width shift+mask pairs** that are documented in the
  same line (e.g. opcode field encoders in
  `awg_assembler_opcodes.cpp` — see `notes/opcode_encoding.md`
  for the per-field width/shift table).
- **Bit-fiddling within explicitly-named bit-width contexts**
  (e.g. 14-bit, 20-bit, 32-bit operations where the bit-count is
  on the same line).
- **`include/zhinst/core/types.hpp` `kDev*` constants** declared
  as `static_cast<AwgDeviceType>(0xNN)`.  The `static_cast` is
  unavoidable because `AwgDeviceType` is a plain enum (not enum
  class) and the values are bitmask compositions, not single
  enumerators.  KEEP.
- **`NodeType` round-trip operators** in `include/zhinst/ast/
  node.hpp`.  These are the enum-flag implementation pattern;
  intentional.
- **`'%'`, `'.'`, `','`, `';'`, `':'`, `'='`, `'\t'`, `'\n'`
  char literals** in protocol parsers.  These are not enums; they
  are the data.
- **Pybind cast tags** (`py::int_(0UL)` etc.) — these match the
  Python C API expectations.

---

## 8. PROPOSE-ENUM batches (deferred to sibling Phase D7-C.2)

Recorded here so the gaps are not lost.  Each needs design
discussion before implementation.

| ID | scope | gap |
|---|---|---|
| **P1** | A3 (residual) | `VariantSlot` enum for `Variable::value.which_` (Int/Bool/Double/String = 0..3 per `runtime/resources.cpp` comments).  7 switches in `ast/value.cpp` would convert. |
| **P2** | A7 (residual) | Device factory feature-bit constants `kDevFlagFF = 0x020`, `kDevFlagRTR = 0x2000`, `kDevFlagPlus = 0x4000`, `kDevFlagLRT = 0x8000`.  Only the 5 subtype-slot bits are named; the 4 feature bits are still raw across `device_*.cpp` factory tables. |
| **P3** | new | `NodeTypeMask*` constants for the `0x100/0x200/0x2000/0x4000/0x8000` hex-bitmask cluster in `codegen/prefetch_placesingle.cpp`.  Needs disambiguation from the `NodeType` enum itself, which uses different values. |
| **P4** | new | `kPrefetchAddr20BitMask = 0xFFFFFu` for 7 clamp/test sites in `codegen/prefetch_emit.cpp` / `prefetch_placesingle.cpp`. |
| **P5** | D1 follow-up | `kCmdSet…` name for the `0x29` LUT once D1 (IF-313) confirms semantic. |
| **P6** | D6 follow-up | `kVarType…` name for the `0x54` LUT once D6 (IF-318) confirms semantic. |
| **P7** | new | Opcode-byte tag constants `kOpTag…` for the 5-way chain `0xF0/F1/F7/F8/FF000000` in `awg_assembler_opcodes.cpp:599`, plus the `|0xF6000000`, `|0xFD000000`, `|0xFE000000` write sites. |
| **P8** | new | Extend `WaveformFile::Type` enum (or add a sibling) for the `.bin / .bin16 / .wave / .wave16` extension ladder in `codegen/awg_compiler.cpp:1196,1202,1245`. |
| **P9** | structural | Deduplicate `kSuser*` (`core/types.hpp:137-192`) vs `DeviceConstants::*` (`device/device_constants.hpp:147-178`).  ~20 overlapping addresses with two parallel naming schemes.  Larger than a literal-→name swap. |
| **P10** | B-series sweep | The entire Category B from `magic_numbers_proposal.md` (`VarType`, `AwgDeviceType`, `DeviceTypeCode`, `DeviceOption`, `AwgSequencerType`, `SubFunc`, `NodeType`, `ValueType`, `Assembler::INVALID`, `Cache::unusedCacheLine`).  Each is "the enum exists, but call sites pass bare ints"; the FIX-NOW batches above handle the most concentrated subset (F1..F4); B-series sweep is the long tail. |

---

## 9. Status of `magic_numbers_proposal.md`

Cross-checking R3's enum inventory against the original proposal:

| ID | proposal | status as of this audit |
|---|---|---|
| A1 | `ImmediateKind` | ✅ implemented (`ast/value.hpp:157`) |
| A2 | `AsmExprType` | ✅ implemented (`asm/asm_expression.hpp:95`) |
| A3 | `VariantSlot` | ❌ never implemented — **P1** above |
| A4 | `VarFlag_Written` / `VarFlag_Frozen` | ✅ implemented (`runtime/resources.hpp:350,355`) |
| A5 | `OptPassFlag` | ✅ implemented (`asm/asm_optimize.hpp:35`) |
| A6 | `MessageType` | ✅ implemented as `CompilerMessage::CompilerMessageType` (`core/compiler_message.hpp:33`) — different name |
| A7 | Device factory option bits | ⚠️ partial — 5 subtype-slot bits named (`device/device_factories.cpp:32-36`); 4 feature bits (FF/RTR/PLUS/LRT) still raw — **P2** above |
| A8 | `OpcodeFormat` | ✅ implemented (`asm/assembler.hpp:171`) |
| A9 | `NodeTypeIdx` | ✅ implemented (`runtime/node_map_data.hpp:36`) |
| A10 | `CmdType` | ✅ implemented (`asm/assembler.hpp:149`) |
| A11 | `RegOrder` | ✅ implemented (`asm/assembler.hpp:160`) |
| A12 | `RegAction` | ✅ implemented (`asm/asm_optimize.hpp:58`) |
| A13 | `kCycle_*` | ✅ implemented (`asm/assembler.hpp:220-222`) |
| A14 | Suser address constants | ✅ implemented (`core/types.hpp:137-192`); **but** parallel `DeviceConstants::*` scheme (`device/device_constants.hpp:147-178`) overlaps — **P9** above.  IF-312 rename = **F9** above. |
| A15 | `-1` sentinels (`kRateInherit`, …) | ✅ implemented (`core/types.hpp:95-98`); 11 bare-literal call sites remain — **F10.b** above |
| A16 | `kChannelTag_I` / `kChannelTag_Q` | ✅ implemented (`core/types.hpp:122-123`) |
| B1..B11 | Existing enums used as bare ints at call sites | ❌ not actioned — partially covered by **F1..F4** (the most concentrated subsets); long tail = **P10** above |
| C1 | `debugFlags` bits | ❌ never reconstructed; no constants exist |
| D1 | `hasPrecomp & 0x1` redundant masks | ❌ not audited (would need a separate call-site grep) |

Summary: 14 of 16 Category-A items fully done; A3 and A7 partially
done.  None of Category B done.  C1 and D1 not done.  IF-312 was
filed against the A14 work and is the F9 fix-now batch.

---

## 10. Open questions for review

Before executing F1..F10, the user should confirm:

1. **F1.c blocker (D7 / IF-319).**  Are we comfortable renaming
   `CantDivConstByWave` (`error_messages.hpp:107`) to match the
   actual template `"%1% expects a var or const argument"` (proposed:
   `VarOrConstExpected` or `ExpectsVarOrConst`)?  This unblocks
   the 14 control-flow sites in F1.c.
2. **F5 home.**  Should `kPlayWaitDevTypeMask` live in
   `core/types.hpp` (alongside the `kDev*` constants) or in a new
   `runtime/custom_functions_internal.hpp`?  The mask is logically
   a runtime concept but `core/types.hpp` already hosts the
   device-bitmask family.
3. **F4 zero-value sentinels.**  `static_cast<VarType>(0)` and
   `static_cast<VarSubType>(0)` are used as "invalid/none" markers
   in `ast/seqc_ast_eval_control.cpp`.  Do the enums actually
   have a zero-valued enumerator, or do we need to add one
   (`VarType_None = 0` / `VarSubType_None = 0`)?  R3 inventory
   shows the enums but didn't enumerate their value 0 explicitly.
4. **F-batch granularity.**  Per-batch commits (10 commits, one
   per F-batch) gives the cleanest bisection, but the smallest
   batches (F8, F9) are 1–3 file edits.  Acceptable to merge
   F7+F8+F9 into one "small-constants cleanup" commit if that
   reduces churn.
5. **DEFER scope.**  All 7 DEFER items (D1..D7 = IF-313..IF-319)
   are independently file-able.  Confirm the IF range allocation
   doesn't collide with anything in flight.
6. **Recursion into pybind.**  `pybind_seqc.cpp` has minimal
   magic-number surface (1 typed-suffix literal `0UL`).  Skip it
   from the FIX-NOW batches?  Recommendation: yes — out of scope.

Once these are settled, the C.1.b..h execution batches can land
in sequence with the standard verification gate per batch:

- `cmake --build .` clean (0 errors, 0 doxygen-warning drift)
- `python tests/diff_test_fast.py` stays 2693/2693 byte-identical
- `python tests/tools/test_seqcc_*` stays 70/70 green
