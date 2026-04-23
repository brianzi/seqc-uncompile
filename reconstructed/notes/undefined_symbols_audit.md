# Undefined Symbols Audit (Phase 19d)

**Date:** 2026-04-23
**Input:** `/tmp/truly_undefined.txt` (95 mangled symbols)
**Goal:** Catalog every zhinst symbol that is referenced by a TU in
`libzhinst_seqc.a` but not defined anywhere, group by class, and
propose ordered work-packages for Phase 20+.

## 1. Methodology

1. `nm libzhinst_seqc.a` over the static archive built by
   `reconstructed/CMakeLists.txt`.
2. Collected the union of all `U _ZN6zhinst…` entries (303) and the
   union of all `T/W/V/B/D _ZN6zhinst…` entries (1699).
3. **Truly undefined = U − defined**, giving 95 symbols. Inter-TU
   references that are satisfied somewhere in the archive are
   excluded; only symbols that would break an executable link survive.
4. Each surviving symbol was demangled with `c++filt` and grouped by
   owning class / namespace via name pattern matching, then verified
   against the relevant header.

The 95 symbols partition into 18 owners (16 classes/structs + free
functions + global variables). Only `ErrorMessages::format<>` accounts
for **64 of the 95** (every observed instantiation of the variadic
template is missing because the header has only the declaration).

## 2. Class / Namespace Grouping

| Class / Namespace                     | # missing | Symbols (short names)                                                                  | Header status                     |
|---------------------------------------|-----------|----------------------------------------------------------------------------------------|------------------------------------|
| `ErrorMessages` (template `format<>`) | 64        | 64 distinct `format<...>` instantiations                                               | declaration only — body missing    |
| `zhinst::util::wave` (free fns)       | 4         | `double2awg`, `double2awg16`, `double2awg1m`, `hash`                                   | declared in `rawwave.hpp`          |
| `Immediate` (3 ctor/op)               | 3         | `Immediate(const&)`, `Immediate(&&)`, `operator=(const&)`                              | declared in `value.hpp`            |
| `WaveformIR` / `WaveformFront` ctor   | 2         | `(string, WaveformFile::Type, DeviceConstants const&)` for both                        | declared in `waveform_*.hpp`       |
| `WaveIndexTracker<T>` template        | 2         | template ctor for `<WaveformIR>` and `<WaveformFront>`                                 | template body present, no inst.    |
| `WavetableManager<WaveformIR>::insertWaveform` | 1 | template method                                                                       | not specialised in IR variant      |
| `SeqcParserContext`                   | 2         | `raiseError`, `setSyntaxError`                                                         | declared in stub header            |
| `Node`                                | 1         | `Node()` default ctor                                                                  | declared in `node.hpp`             |
| `Value`                               | 1         | `Value()` default ctor                                                                 | declared in `value.hpp`            |
| `WaveformFile`                        | 1         | `WaveformFile(const char*)`                                                            | declared in `waveform.hpp`         |
| `MemoryAllocator`                     | 1         | `MemoryAllocator(DeviceConstants const*, uint32_t)`                                    | declared in `memory_allocator.hpp` |
| `AWGAssembler` / `AWGAssemblerImpl`   | 2         | `AWGAssembler::assembleStringToExpressionsVec`, `AWGAssemblerImpl::extractComment`     | declared, defs missing             |
| `NodeMap` (in custom_functions.cpp)   | 2         | `toFrequency(double,double)`, `toPhase(float)`                                         | declared in inline class           |
| `Prefetch::minIndexedSize` (static)   | 1         | static int member                                                                       | declared in `prefetch.hpp`         |
| `CsvParser::csvFileToWaveform<IR>`    | 1         | template method                                                                        | declared in stub class             |
| `parseOptionalRate` (free fn)         | 1         | declared in `custom_functions.hpp`                                                     | declared, no body                  |
| `floatEqual` (free fn)                | 1         | declared as forward in `waveform_generator.cpp`                                        | declared, no body                  |
| `logging::detail::logExceptionToClog` | 1         | one fn                                                                                 | wrong namespace in current `.cpp`  |
| Global `errMsg` (`ErrorMessages` var) | 1         | extern declaration only                                                                | not defined anywhere               |
| `zsyncDataPqscDecoder/Register/constAwgIntegrationTrigger` | 3 | three `extern const std::string` globals (BSS)                              | extern, not defined                |
| **TOTAL**                             | **95**    |                                                                                        |                                    |

EvalResults (Phase 19a) and the three Resources blockers (Phase 19b)
are now `T` in the archive — confirmed empty here.

## 3. Per-class detail

### 3.1 `ErrorMessages::format<...>` (64 symbols, blocking 14 TUs)

**Header:** `error_messages.hpp:453-454`
```cpp
template <typename... Args>
static std::string format(ErrorMessageT id, Args&&... args);
```
The template **body is not in the header**. Every TU that calls
`ErrorMessages::format<X>(id, x)` therefore expects a definition that
is provided by exactly nobody. The actual binary uses ~64 distinct
instantiations spanning `int`, `string`, `char const(&)[N]`,
`unsigned long`, plus 2/3/4-arg combos.

**Callers** (14 TUs): asm_commands, asm_commands_impl_cervino,
asm_commands_impl_hirzel, awg_assembler_impl_pipeline,
awg_assembler_opcodes, custom_functions, math_compiler, node, prefetch,
prefetch_emit, resources, static_resources, waveform_generator,
wave_index_tracker.

**Reconstruction strategy:**
- The template body is structurally trivial — the binary's pattern is
  `messages.at(id) → boost::basic_format(str) → feed % args → str()`.
- **Best fix: move the template body inline into `error_messages.hpp`.**
  This satisfies all 64 instantiations without disassembly.
- Alternative (avoids header bloat): write the body in
  `error_messages.cpp` and emit explicit instantiations for all 64
  argument-pack combinations.

**Estimated cost:** ~30 lines of header code + boost/format.hpp include.
**Size class:** small (one body, 64 implicit instantiations).

### 3.2 `zhinst::util::wave` free functions (4 symbols)

| Function           | Address    | Used by         | Size class |
|--------------------|------------|-----------------|------------|
| `double2awg`       | 0x299630   | rawwave, signal | small      |
| `double2awg1m`     | 0x299680   | rawwave         | small      |
| `double2awg16`     | 0x299700   | rawwave         | small      |
| `hash(string&)`    | (unknown)  | cached_parser   | small      |

The first three are referenced from `rawwave.cpp` and `signal.cpp` for
sample encoding. `hash` is used by `cached_parser::getHash` which
already declares the dependency in a comment. Bodies are very small
(uint16 conversion math + branch; hash is a fixed-output hash of a
string). All four are declared in `rawwave.hpp:29-31` (encoders) and
referenced as `util::wave::hash` in `cached_parser.cpp:472`.

### 3.3 `Immediate` copy/move ctor + copy assignment (3 symbols)

`Immediate` is a discriminated-union type containing a `std::string` —
it MUST have a non-trivial copy ctor / move ctor / copy assignment.
The header declares all three (`value.hpp:63-66`); only the four
content ctors and `operator==` are defined in `value.cpp`. The three
required Rule-of-Five members are missing.

**Callers:** asm_commands, asm_list, asm_optimize, custom_functions,
eval_results, prefetch_emit, prefetch_placesingle, prefetch_splitplay
(8 TUs — heavy).

**Cost:** small (3 short bodies, ~30 lines total). The variant-tag
state machine is identical to the existing string ctor + dtor.

### 3.4 `WaveformIR(string, Type, DeviceConstants&)` and `WaveformFront(string, Type, DeviceConstants&)` (2 symbols)

Both are declared and called from `wavetable_manager_*.cpp`. The
existing `.cpp` files only define the move/`shared_ptr<Source>`
overloads. Disassembly addresses TBD (need to grep binary for
the mangled name).

**Callers:** wavetable_manager_ir (WaveformIR), wavetable_manager_front
(WaveformFront).

**Cost:** medium — these are full `Waveform*` constructors that
populate `name_`, `file_` (via `WaveformFile(string)`), and forward
device constants.

### 3.5 `WaveIndexTracker<T>` template ctor (2 symbols)

**Body already exists** in `wave_index_tracker.cpp:148`. The fix is a
trivial **explicit instantiation** at the end of that file:
```cpp
template WaveIndexTracker::WaveIndexTracker(int, const detail::WavetableManager<WaveformIR>&);
template WaveIndexTracker::WaveIndexTracker(int, const detail::WavetableManager<WaveformFront>&);
```
**Cost:** trivial (2 lines).

### 3.6 `WavetableManager<WaveformIR>::insertWaveform(shared_ptr<WaveformIR>)` (1 symbol)

The general template lives only in `wavetable_manager_front.cpp`
(specialised for `WaveformFront` directly). The IR variant `.cpp`
historically had its body removed (per a TODO comment) on the
assumption it was a duplicate. Need to **either** restore the IR
specialization in `wavetable_manager_ir.cpp` **or** add an explicit
instantiation that points at a shared template body.

**Callers:** wavetable_ir, wavetable_manager_ir.

**Cost:** small (one method body, ~50 lines, mirrors WaveformFront
version with `WaveformIR` substituted).

### 3.7 `SeqcParserContext` — `raiseError` + `setSyntaxError` (2 symbols)

Declared in `seqc_parser_context.hpp:17-18` (a stub file marked
"deferred Phase 11+ or later"). Bodies addresses 0x247ae0 / 0x247cb0
in the binary.

**Callers:** expression.cpp (single TU).

**Cost:** small (two short methods that touch the parser context's
syntax-error flag and accumulate a message string). Probably ≤ 100
lines. Mirrors `AsmParserContext::raiseError/setSyntaxError` already
implemented.

### 3.8 `Node()` default ctor (1 symbol)

Declared in `node.hpp:119`. Only the (NodeType, int, int) and big
ctor are defined. The default ctor is needed by:

**Callers:** asm_commands.

Likely zero-init the Node POD members and call
`enable_shared_from_this`'s default ctor implicitly. Address TBD
(disasm grep). **Cost:** small.

### 3.9 `Value()` default ctor (1 symbol)

Declared `value.hpp:159` as required by `Resources::Variable`
initialization. Header has the `(string)` and `~Value()` defined; the
default ctor is missing.

**Callers:** custom_functions, eval_results, resources,
waveform_generator (4 TUs).

**Cost:** small. Likely sets `which_=invalid` and zero-inits storage.

### 3.10 `WaveformFile(const char*)` ctor (1 symbol)

Declared in `waveform.hpp`. Existing definitions only cover the string
overload (`C5` symbol present in archive). Need the C-string overload.

**Callers:** waveform, wavetable_manager_front (2 TUs).

**Cost:** small (likely delegates to the std::string ctor).

### 3.11 `MemoryAllocator(DeviceConstants const*, uint32_t)` (1 symbol)

Declared `memory_allocator.hpp:61`; existing `.cpp` has only the dtor.
Single caller: wavetable_ir.

**Cost:** medium — touches CL alignment, free-block list, device
constants. Address TBD.

### 3.12 `AWGAssembler::assembleStringToExpressionsVec` + `AWGAssemblerImpl::extractComment` (2 symbols)

The `Impl::assembleStringToExpressionsVec` at 0x286e40 IS defined in
`awg_assembler_impl_pipeline.cpp:217`. The **public pimpl wrapper**
at 0x285100 is commented out in `awg_assembler.cpp:43-45` because
the return type is non-trivial (vector of shared_ptr<AsmExpression>).
The wrapper needs to be uncommented with the correct return type.

`extractComment` is declared on AWGAssemblerImpl
(`awg_assembler_impl.hpp:145`) and *called* from the pipeline file but
**never defined**. Trivial body (find `;` or `//` in line, extract
suffix).

**Callers:** asm_list (assembleString…), awg_assembler_impl_pipeline
(extractComment).

**Cost:** small for both.

### 3.13 `NodeMap::toFrequency` + `NodeMap::toPhase` (2 symbols)

Declared inline inside an anonymous helper class at
`custom_functions.cpp:47` (NodeMap class with two static methods).
Used heavily by feedback / sweep / oscillator builtins.

Addresses: `toPhase` @0x1c5680, `toFrequency` @0x1c5630 (per existing
comments).

**Callers:** custom_functions only (intra-TU but compiler emitted as
out-of-line statics).

**Cost:** small. Both are math conversions: `toPhase(float)` =
`int(value * 65536/360) & 0xFFFF` (typical phase encoding);
`toFrequency(double, double)` = fixed-point freq encoding.

### 3.14 `Prefetch::minIndexedSize` static int (1 symbol)

Static class member declared at `prefetch.hpp:257` (BSS at 0xb846d8).
Definition out-of-line: `int Prefetch::minIndexedSize = <value>;`
where the runtime value can be read from BSS in the binary (likely a
small constant like 16 or 64 — should be confirmed by disasm).

**Callers:** prefetch_placesingle (1 TU).

**Cost:** trivial (one line).

### 3.15 `CsvParser::csvFileToWaveform<WaveformIR>` (1 symbol)

A stub `CsvParser` class is declared inline in `wavetable_ir.cpp:32`
with a static template method that has no body. The Phase 12c entry
in archived TODO marks this as cancelled ("no zhinst::CsvParser
symbols found in binary"). However, `wavetable_ir.cpp:655` actually
calls it.

**Resolution options:**
- (a) Provide a no-op stub body (`return;`) explicit instantiation
  for `<WaveformIR>` since the binary may inline this away.
- (b) Disassemble the actual call site at 0x???? to confirm.

**Cost:** trivial (stub) or medium (full).

### 3.16 `parseOptionalRate` free fn (1 symbol)

Declared `custom_functions.hpp:211` @0x163980. **Caller:** custom_functions.
Cost: small (parses an EvalResultValue range for an optional integer
rate parameter).

### 3.17 `floatEqual(double, double)` free fn (1 symbol)

Forward-declared at `waveform_generator.cpp:39`. Trivial body
(`std::abs(a-b) < epsilon`). Address @0x2ec050 (per Phase 16d notes).
**Caller:** waveform_generator only (intra-TU but emitted out-of-line).

**Cost:** trivial (one line).

### 3.18 `zhinst::logging::detail::logExceptionToClog` (1 symbol)

Currently the implementation in `log_exception.cpp` is in namespace
`zhinst::detail`, **not** `zhinst::logging::detail`. Mangled-name
mismatch.

**Caller:** logging.cpp.

**Cost:** trivial (move the function inside `namespace logging {}`).

### 3.19 Global `errMsg` (1 symbol)

`extern ErrorMessages errMsg;` declared in 4 TUs (cache,
custom_functions, node, prefetch_emit) but never defined anywhere.
The binary has it at 0x95de60 — a process-wide singleton instance of
`ErrorMessages`.

**Cost:** trivial (one line: `ErrorMessages errMsg;` in
`error_messages.cpp`).

### 3.20 `zsyncDataPqscDecoder`, `zsyncDataPqscRegister`, `constAwgIntegrationTrigger` (3 symbols)

`extern const std::string` references in `static_resources.cpp:21-23`
(BSS @ 0xb84690 / 0xb846a8 / 0xb846c0 per phase 7e notes). Initialized
at startup with constant strings (decoded from rodata).

**Cost:** trivial (three lines plus the rodata string literals).

## 4. Free functions / globals summary (verified non-class)

Two of the entries categorized as "free" deserve confirmation:

- **`zhinst::floatEqual`** — confirmed free function. Forward-declared
  at file scope in `waveform_generator.cpp:39`; mangled name is
  `_ZN6zhinst10floatEqualEdd` with no class-name segment after the
  namespace tag.
- **`zhinst::parseOptionalRate`** — confirmed free function. Mangled
  `_ZN6zhinst17parseOptionalRateE…`; declared at namespace scope in
  `custom_functions.hpp:211`. No enclosing class.

Other globals (not functions): `zhinst::errMsg`,
`zhinst::Prefetch::minIndexedSize`, `zhinst::zsyncDataPqsc{Register,
Decoder}`, `zhinst::constAwgIntegrationTrigger`. Of these, only the
three string globals are at namespace scope; `Prefetch::minIndexedSize`
is a static class member.

## 5. Suggested Work-Package Ordering

Five work-packages, ordered for execution. The ordering puts trivial
high-impact items first (cheap link-fix wins) and leaves
template/full-method reconstructions for later.

### WP-A: Globals & ErrorMessages template body (~64 + 5 = 69 symbols, **highest impact**) ✅ DONE 2026-04-24 (Phase 20a)

- `errMsg` global (1) — defined in `error_messages.cpp:28`
- 3 zsync/integration-trigger string globals (3) — defined in
  `error_messages.cpp:45-47`. Strings recovered from binary rodata.
  Note: binary mangles with `L` prefix (internal linkage / static-in-
  namespace); our header declares them `extern` for cross-TU access.
  Slight ABI deviation, behaviour unchanged.
- `Prefetch::minIndexedSize` static (1) — defined in
  `prefetch.cpp:2294` with init value `0x1000` (4096), recovered from
  `__cxx_global_var_init` at 0xd4361.
- ErrorMessages::format<…> template body inlined (64) — body inlined
  into `error_messages.hpp:457-466` using `boost::format` chained via
  `operator%` driven by `std::initializer_list<int>{...}` (C++17,
  safer than fold-expr). All 64 instantiations now generated as
  weak symbols `W` at caller TUs.
- Bonus: zero-arg `ErrorMessages::format(ErrorMessageT)` non-template
  overload was already defined at `error_messages.cpp:69` (was not in
  the 95 — counted separately, satisfies all `format(enum)` calls).

This single work-package eliminated **73% of the gap (69/95)** and
unblocked every `errMsg[…]` callsite + every `format<>` call.
Build clean post-completion.

### WP-B: Trivial Rule-of-Five and missing default ctors (8 symbols) ✅ DONE 2026-04-24 (Phase 20b)

- `Immediate(const&)`, `Immediate(&&)`, `operator=(const&)` (3) —
  added to `value.cpp` after the string ctor. Per-index switch
  dispatch matches the existing `~Immediate`/`operator int`/etc.
  pattern (no vtable dispatch — semantically equivalent).
- `Value()` default ctor (1) — added to `value.cpp` after the
  string ctor. Sets `type_=Unspecified, which_=0, storage_.i=0`;
  any `toX()` conversion will throw, matching the
  "uninitialized variable" error path.
- `Node()` default ctor (1) — added to `node.cpp` after the
  TLS counter. Delegates to the 3-arg ctor with neutral defaults
  (`NodeType{0}, 0, -1`).
- `WaveformFile(const char*)` ctor (1) — added at the end of
  `waveform.cpp`. Copy-constructs `name` from the C-string,
  zero-inits the three `field*` members and `data`. (Binary's
  0x2a7ff0 was actually a `std::construct_at<>` specialization
  inlining the body — no dedicated `WaveformFile::WaveformFile
  (char const*)` symbol existed.)
- `floatEqual(double,double)` @0x2ec050 (1) — out-of-line definition
  in `waveform_generator.cpp`. **Surprise**: despite the name, the
  binary's body is a 5-instruction `cmpeqsd` — exact IEEE-754
  bitwise equality, NOT tolerance-based. Reconstruction matches.
- `logging::detail::logExceptionToClog` namespace fix (1) —
  `log_exception.cpp` namespace corrected from `zhinst::detail` to
  `zhinst::logging::detail` to match the binary's mangled symbol
  (and the `logging.cpp` declaration site).

After 20a + 20b: **77/95 done (81%), 18 remaining.** Build clean.

All 6 ctor symbols verified `T` (defined) in
`libzhinst_seqc.a`. Per-TU `U` entries from `nm` are normal for
static archives — they get resolved at executable-link time.
The naive `comm -23` diff against `truly_undefined.txt` underreports
because per-`.o` `U` references persist even when the symbol is
defined elsewhere in the same archive — use direct `T`/`B`/`D` count
instead (verified by per-symbol grep).

### WP-C: Wavetable / WaveformIR / WaveformFront ctors and templates (5 symbols) ✅ DONE 2026-04-24 (Phase 20c)

- `WaveformIR(string, Type, DeviceConstants&)` (1) — body recovered from
  the `allocate_shared<WaveformIR>` dispatcher inlining at
  `0x2aa170-0x2aa20f`. Sets `name`, `waveformType`, `waveIndex=-1`,
  `seqRegWidth = dc.waveformGranularity (dc+0x40)`, `deviceConstants=&dc`,
  zeroes IR-extension bools, `irField2 = dc.field_24 (dc+0x24)`.
- `WaveformFront(string, Type, DeviceConstants&)` (1) — body recovered
  from the `newWaveformFromFile` dispatcher inlining at `0x29b110-0x29b24f`.
  Identical to IR variant in the Waveform-base region; sets
  `frontField1 = 1` (NOT 0 like IR), other Front-extension fields zero.
- `WavetableManager<WaveformIR>::insertWaveform` (1) — added specialization
  to `wavetable_manager_ir.cpp` mirroring the WaveformFront body. Required
  a forward-declared specialization at top-of-file (C++14 [temp.expl.spec]/6)
  because the same TU's earlier ctor and `newWaveform` already implicitly
  use the function.
- `WaveIndexTracker<WaveformIR>` + `<WaveformFront>` explicit
  instantiations (2) — added at end of `wave_index_tracker.cpp`. Symbols
  emit as `W` (weak), satisfying the `U` references at link time.

Side findings:
- The header comment at `waveform_front.hpp:65` claimed
  `bitsPerSample=dc[0x40]` — wrong on two counts: `dc[0x40]` is
  `waveformGranularity` (not `bitsPerSample`, which is at `dc+0x50`),
  and the destination field is `Waveform::seqRegWidth` (`+0x70`), not
  `bitsPerSample`. Comment replaced with verified offset documentation.
- The `WaveIndexTracker` template body referenced `wp->playIndex`, but
  the field is named `waveIndex` at `Waveform+0x6C`. Fixed.
- `WaveformFile(const char*)` — listed in pre-audit drafts as a 6th
  WP-C item, but was already cleared in Phase 20b (WP-B item 4).
- Build clean; 5 targets verified `T`/`W` defined in archive.

### WP-D: AWGAssembler / parser / NodeMap small methods (7 symbols) — ✅ DONE 2026-04-24 (Phase 20d)

- ✅ `AWGAssembler::assembleStringToExpressionsVec` — uncommented in
  `awg_assembler.cpp:40-47` with return type
  `std::vector<std::shared_ptr<AsmExpression>>`.
- ✅ `AWGAssemblerImpl::extractComment` — body added at end of
  `awg_assembler_impl_pipeline.cpp` (find `"//"` substring → return
  suffix). The binary inlined this twice as the `$_1` lambda inside
  `assembleStringToExpressionsVec`; we externalize as a real method.
- ✅ `SeqcParserContext::raiseError` + `setSyntaxError` — implemented
  in new `src/seqc_parser_context.cpp` using raw byte-offset access.
  The class is opaque in the header (no data members); the binary
  uses an indirect callback at `[this+0x30]` (vtable[6]/+0x30 path)
  with std::clog fallback. **ABI risk noted**: the vtable offset is
  libc++-specific.
- ✅ `NodeMap::toFrequency` — `(int64_t)(freq * 2^48 / sampleClock)`,
  reinterpret as `uint64_t`. Constant `2^48 = 281474976710656.0` from
  rodata `0x956078 = 0x42f0000000000000`.
- ✅ `NodeMap::toPhase` — `roundf(value * (2^23/360))` then 23-bit
  two's-complement wrap. Scale constant
  `0x46b60b61 ≈ 23301.689453125f` from rodata `0x8fd2b4`. Wrap logic
  shared with `pauPoffIwrap` (extracted as `wrap23()` helper in
  anonymous namespace).
- ✅ `parseOptionalRate` — implemented at end of `custom_functions.cpp`.
  Disasm-derived semantics: header parameter naming is misleading;
  the third iterator (rdx) is the *parse cursor* (typically the
  return of `PlayArgs::parse()`). If exactly one EvalResultValue
  remains between cursor and `end`, calls `getPlayRate()`. Otherwise
  encodes the "no-rate" sentinel as `((strict ? 1 : 0) - 1) | 5`
  (5 if strict, 0xffffffff if not). Throws
  `CustomFunctionsValueException(format(0xee, name), itemIndex)` if
  extra unparsed args remain.

**Side findings:**

- libstdc++ vs libc++ ABI mangling difference: when verifying
  symbol presence with `nm | grep " T $sym\$"`, the libc++-style
  mangled names (e.g. `__1`, `__wrap_iter`) DO NOT match. The
  reconstructed code is being built with libstdc++, which mangles
  the same source-level signatures differently
  (`__cxx1112basic_string`, `__gnu_cxx17__normal_iterator`). The
  static archive is internally consistent — both definitions and
  per-`.o` `U` references use the libstdc++ mangling, so they
  resolve at link time. This means our existing per-symbol audit
  script (`nm | grep " T $sym\$"`) needs both mangling variants to
  give a true picture, OR we must trust the clean `cmake --build`
  + zero unresolved-at-link-time as the ground truth.
- `SeqcParserContext` partial layout documented in header (offsets
  0x00, 0x01, 0x02, 0x03, 0x04, 0x30) but kept opaque (no data
  members) since the class is never instantiated in reconstructed
  code — only used by-pointer.

Build clean; total cleared 7 symbols. Phase 20 progress: 89/95 (94%).

### WP-E: util::wave + MemoryAllocator + CsvParser + 19c carry-forwards

#### WP-E-i (✅ DONE 2026-04-24, Phase 20e-i): 6 symbols

- ✅ `util::wave::double2awg(double, uint)` @0x299630 — 14-bit signed
  sample with 2 marker bits in low 2. Constants from rodata: 1.0
  (`0x956030`), -1.0 (`0x956068`), 8190.0 (`0x956330`).
- ✅ `util::wave::double2awg1m(double, uint)` @0x299680 — 15-bit
  signed sample with 1 marker bit in low. Scale 16383.0 (`0x956338`).
- ✅ `util::wave::double2awg16(double)` @0x299700 — 16-bit signed
  sample, no marker. Scale 32767.0 (`0x956340`).
- ✅ `util::wave::hash(string const&)` @0x299760 — **uses
  `boost::uuids::detail::sha1`** internally (calls `process_block`
  @0x29a270 and `get_digest` @0x29a000). Returns `vector<uint32>`
  of 5 words (160-bit SHA-1, MSB-first per word). Reconstructed via
  `boost::uuids::detail::sha1::process_bytes` + `get_digest(uchar[20])`
  with manual MSB-first packing into uint32 to match binary's
  bswap-then-store sequence.
- ✅ `MemoryAllocator(const DeviceConstants*, uint32_t)` ctor —
  inlined at all call sites in the binary, so no standalone symbol
  exists; we provide a real ctor matching the documented 0x70-byte
  layout. Initializes per-CL ownership table to all `0xFFFFFFFF`
  (free), `numCacheLines_ = memorySize / cacheLineSize`.
- ✅ `CsvParser::csvFileToWaveform<WaveformIR>` @0x2be830 — STUB.
  Full reconstruction deferred (~7000-byte function, complex
  CachedParser/CSV-parsing pipeline, not on hot path for non-CSV
  SeqC programs). Stub throws `std::runtime_error` so callers fail
  loudly rather than silently producing zero-filled waveforms.

**Side findings:**

- `util::wave::hash` resolves the long-standing question (open
  question #1, line 503): the binary uses Boost SHA-1, not a custom
  hash. The IV constants visible at `0x8fc720`
  (`67452301 efcdab89 98badcfe 10325476`) plus `0xc3d2e1f0` are the
  standard SHA-1 initial state, confirming.
- **Final undefined-zhinst-symbol gap = 0** after Phase 20e-i:
  `comm -23 <undefined> <defined>` produces empty output. The
  static archive is now fully self-contained for the zhinst
  namespace (only stdlib/Boost/external externs remain).

#### WP-E-ii (REMAINING): Resources 19c carry-forward (~38 methods)

- **All 19c-carry-forward Resources methods** (37 methods listed in
  the task brief — `createSubScope`, `updateParent`, the
  variable/string/wave/cvar/const/function families, `getRegister`,
  `setupGlobalState`, plus StaticResources & GlobalResources
  ctor/dtor). These have **no current undefined-symbol references**
  in the static archive (caller code is all stubbed/missing) but
  will appear as fresh `U` entries the moment SeqCAstNode evaluation,
  the AST tree-walk facade, or any compiler-pipeline integration
  code is reconstructed.

This is the bulk of the remaining work; **likely 1-2 sessions**.
Lower urgency now that the static archive links cleanly without
them.

## 6. Open questions / low-confidence items

1. **CsvParser**: ✅ resolved 2026-04-24 (Phase 20e-i). The binary
   DOES contain real definitions: `csvFileToWaveform<WaveformFront>`
   @0x2ba8b0 and `csvFileToWaveform<WaveformIR>` @0x2be830, each
   ~7000 bytes. The Phase 12c "no symbols" note was wrong — the
   functions exist as full template specializations, not inlined.
   Reconstruction stubbed (throws on call) since CSV waveform
   loading is not on hot path.
2. **Static globals BSS values**: the runtime values of
   `zsyncDataPqscDecoder` etc. need confirmation by reading BSS
   contents at 0xb84690..0xb846c0 in the binary (not in rodata until
   the dynamic constructors copy from rodata).
3. **`Prefetch::minIndexedSize` initial value**: ✅ resolved 2026-04-24 —
   value is `0x1000` (4096), set by `__cxx_global_var_init` at
   0xd4361 (`mov DWORD PTR [rip+...],0x1000  # 0xb846d8`).
4. **`format<>` instantiations vs inline body**: if we inline the
   template body, the existing 64 explicit-instantiation symbols in
   the binary become **implicit** in our reconstruction, which is
   fine for linking but loses the "exact same set of instantiations"
   property. Acceptable trade-off given the source we control is the
   `.cpp` consumer side, not a public ABI.
5. **`WavetableManager<WaveformIR>::insertWaveform`**: the historical
   note says the IR specialization was removed because it was a
   "general template". But the WaveformFront variant IS specialised
   (out-of-line in `wavetable_manager_front.cpp`). Need to decide
   whether to mirror the Front spec or re-add a real IR spec.
