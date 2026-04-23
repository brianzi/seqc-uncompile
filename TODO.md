# TODO — Reconstructed zhinst SeqC Compiler

> **Completed Phases 1-12 archived in
> [`reconstructed/notes/archive/TODO_phases_1-12.md`](reconstructed/notes/archive/TODO_phases_1-12.md).**
> 597 completed items across 12 phases (plus sub-phases 10.5-10.8).

---

## Summary of remaining work

| Category | Stub count | Notes |
|----------|-----------|-------|
| SeqCAstNode print/clone bodies | 1 | SeqCValue::clone shallow-copy caveat |
| CustomFunctions built-in function bodies | 27 | 27 one-liner stubs remain (others reconstructed) |
| WaveformGenerator waveform function bodies | ~35 | DSP functions (sine, gauss, etc.) |
| CachedParser method bodies | 0 | ✅ resolved in Phase 13d |
| CustomFunctions field unknowns | 6 | MathCompiler, NodeMapData, etc. (was 8, 2 closed) |
| Prefetch/Cache approximate logic | 10 | Deferred from Phase 10.5f |
| AsmOptimize approximate logic | 2 | simplifyAssign + splitReg (the 3 main unknowns #47-49 closed in Phase 15c) |
| Compiler pipeline gaps | 1 | lower() return + EvalResults done; vtable wiring remains |
| MathCompiler | 67 symbols | Not yet started |
| WavetableManager\<T\> | 14 methods | Template, partially done |
| DeviceType/Family/Option | 150 symbols | Not yet started |
| Logging/tracing | 20 zhinst symbols (17 fns) | ✅ done — see notes/logging_tracing.md |

---

## Phase 13: Method body reconstruction

Fill in stubbed method bodies by disassembling the binary at the addresses
noted in each stub's comment. Priority: highest-value stubs first.

### 13a. SeqCAstNode subclass bodies

106 stubs across 53 subclasses (print + clone for each). Most are
straightforward (print writes to stream, clone deep-copies children).

- [x] Analyze print() pattern from one representative class (e.g. SeqCIfElse @0x201df0)
- [x] Implement print() macro template based on pattern
- [x] Analyze clone() pattern (likely recursive deep-copy)
- [x] Implement clone() macro template
- [x] Fill in non-trivial subclass bodies (SeqCVariable, SeqCValue, SeqCFunction)
- [ ] Resolve unknown #96 (SeqCAstNode `type` field meaning)
- [x] Sub-phase wrap-up

### 13b. CustomFunctions method bodies

~35 SeqC built-in function stubs (play, playZero, playHold, setTrigger,
waitDigTrigger, etc.) plus 5 infrastructure methods.

- [x] Reconstruct ctor registration logic @0x12bcf0 (maps function names → lambdas)
- [x] Reconstruct call() @0x159470 (alias resolution + funcMap_ dispatch)
- [ ] ~~Reconstruct eval()~~ — no standalone eval(); moved to Phase 13e
- [x] Pick 5-10 representative built-in functions, reconstruct bodies
- [x] Stub remaining built-in functions with improved comments
- [ ] Resolve field unknowns: MathCompiler layout (#102), field_168 (#101) — deferred to 13e
- [x] Sub-phase wrap-up

### 13c. WaveformGenerator method bodies

~35 waveform DSP function stubs (sine, cosine, gauss, drag, rrc, etc.)
plus call/eval/getOrCreateWaveform infrastructure.

- [x] Reconstruct call() — alias resolution + funcMap_ dispatch
- [x] Reconstruct eval() — body documented; throws "blocked on EvalResults" with full algorithm in comments (real impl deferred to Phase 15a once EvalResults layout is known)
- [x] Reconstruct getOrCreateWaveform() — caching layer
- [x] Pick 5-10 representative waveform functions, reconstruct bodies (zeros, ones, rect, scale, add, gauss + readers + createDummyWaveform)
- [x] Stub remaining with improved comments (27 stubs documented with expected signatures)
- [x] Resolve field unknowns: field_50_ (#108 — renamed createdNames_), field_B0_ (#110 — deferred, kept as shared_ptr<void>)
- [x] Reconstruct aliasMap_ contents from ctor registration code (#111 — empty, no aliases registered in this binary)
- [x] Sub-phase wrap-up

### 13d. CachedParser method bodies

7 stubs — most are Boost.Serialization-heavy.

- [x] Reconstruct loadCacheIndex @0x2afec0 (boost::archive::text_iarchive) — ifstream → text_iarchive >> index_, then recompute currentSize_ by walking tree (sum of fileSize_); cleanCache() on any exception
- [x] Reconstruct saveCacheIndex @0x2b03c0 (boost::archive::text_oarchive) — skip if disabled or empty; ofstream → text_oarchive << index_; sticky-disable enabled_ on exception
- [x] Reconstruct cacheFile @0x2b05b0 (~700 instructions) — algorithmic outline + section-name table; full ELF-building body left as documented stub (heavy ELFIO interaction)
- [x] Reconstruct getCachedFile @0x2b1900 (~400 instructions) — full lookup-and-load with cacheFileOutdated check, eviction, ElfReader → 4 sections (.channels, .marker_bits, .data, .marker)
- [x] Reconstruct removeOldFiles @0x2b01a0 — copy-to-vector + sort by timestamp + evict-while-oversize, respecting valid_ pin flag
- [x] Implement CacheEntry::serialize template — inline 6-field `ar &` sequence
- [x] BONUS: Reconstructed cleanCache @0x2b0140 with exception swallowing
- [x] BONUS: Reconstructed cacheFileOutdated @0x2b14d0 — ElfReader-based ".format" + ".file_name" mtime check (NOT an index_ lookup as previously assumed)
- [x] BONUS: Created minimal ElfReader/ElfSection forward-decl header
- [x] BONUS: CORRECTED CachedFile layout — there is NO bool found_ field; layout is `uint16_t channel_; vector<uint8_t> markerBits_; vector<double> samples_; vector<uint8_t> markers_` (resolves #113)
- [x] BONUS: cacheSize_/currentSize_ are tracked in BYTES, not entry counts (header doc fixed)
- [x] BONUS: valid_ flag semantics clarified — set true on access (LRU-touch alongside timestamp_), prevents eviction (acts as "in-use pin")
- [x] Sub-phase wrap-up

### 13e. Quick-win unknown resolutions

Small disassembly tasks from the unknowns triage. Each is a single
ctor/method read to determine field types or contents.

- [x] MathCompiler internal layout (#102) — RESOLVED in re-audit. Two
      `std::map`s at +0x00 (single-arg) and +0x18 (multi-arg). Header updated.
- [x] NodeMapData subclass fields (#104) — DirectAddrNodeMapData has
      `uint32_t addr_` at +0x08 (after 8B vptr). Header updated.
      VirtAddrNodeMapData fields still unknown — deferred (no clear read sites).
- [x] CustomFunctions field_168 (#101) — RE-CHARACTERIZED. Real type per
      ~CustomFunctions @127cf2 is `std::vector<T>` with sizeof(T)==8 trivially
      destructible. Element T still unidentified — moved to Phase 14a follow-up.
- [x] BONUS: corrected pre-existing `nodeMap_` field at CustomFunctions+0xF8
      from `std::map<string,NodeMapItem>` (24B inline) to `std::unique_ptr<NodeMap>`
      (8B). Confirmed via lookupNode @0x15c530 dereferencing [this+0xf8] as a
      single ptr and calling NodeMap::retrieve(). Added NodeMap class definition
      in src/custom_functions.cpp.
- [x] BONUS: documented historical confusion with the dtor at 0x1306c1 (which
      is NOT CustomFunctions::~CustomFunctions — that's at 0x127c90). Note added
      to unknowns_open.md and to the layout comment in custom_functions.hpp.
- [x] WaveformGenerator field_50_ — RESOLVED in 13c (renamed createdNames_, set<string> cache)
- [ ] WaveformGenerator field_B0_ — moved to Phase 15a (deferred from 13c)
- [x] WaveformGenerator aliasMap_ — RESOLVED in 13c (empty in this binary)
- [x] Sub-phase wrap-up

### 13f. WaveformGenerator DSP function expansion (optional)

**Optional / low priority.** Existing stubs are correct one-liners.
Expanding to full DSP math gives little value without test infrastructure
to validate sample-by-sample equivalence with the binary. Skip unless
explicitly requested.

After 13e, optionally expand the 27 documented-but-stubbed DSP functions
(sin, cos, drag, blackman, hamming, hann, chirp, rrc, etc.) using the
patterns established in 13c (gauss/rect/scale). Many follow the same
checkArgCount → readers → fill-vector → return Signal pattern.

- [ ] Reconstruct trig family (sin, cos, sinc) — share frequency/phase reader
- [ ] Reconstruct envelope family (blackman, hamming, hann, drag) — Gauss-derived
- [ ] Reconstruct ramp family (ramp, sawtooth, triangle, chirp)
- [ ] Reconstruct random family (rand, randomGauss, randomUniform, lfsrGaloisMarker)
- [ ] Reconstruct combinator family (join, interleave, multiply, cut, flip, filter)
- [ ] Reconstruct misc (rrc, vect, placeholder, mask, marker, markerImpl,
      readWave, interpolateLinear)
- [ ] Sub-phase wrap-up

---

Types not yet started, identified from symbol table.

### 14a. MathCompiler (67 symbols, 0x30 bytes)

Embedded in CustomFunctions at +0xC8. Separate math expression compiler
for constant-folding and evaluation.

- [x] Determine MathCompiler layout from ctor @0x1c2250 — already resolved in
      Phase 13e re-audit: two `std::map`s at +0x00 (single-arg) and +0x18 (multi-arg).
- [x] Identify all 67 methods from symbol table — 33 zhinst:: methods (28 single-arg
      libm wrappers, 5 multi-arg, ctor, dtor, call, functionExists, exception class);
      remaining ~34 symbols are libc++ __function::__func<__bind<...>> instantiations.
- [x] Reconstruct layout + key methods — full ctor (populates both maps via std::bind),
      `call()` with single→multi dispatch and FuncSingleArg/UnknownFunction throws,
      `functionExists()` with strict|found semantics, `pow()` with FuncExactly2Args
      throw on size!=2. Single-arg wrappers were already stubbed correctly; fixed
      sin/sinh/sqrt addresses (typo: sqrt was duplicating sinh's address).
- [x] FOLLOW-UP from 13e: identify CustomFunctions field_168 element type
      RESOLVED — Phase 13e characterization was wrong. The dtor at 127cf2 has a
      node-walk loop at 127d40-127d70 reached via `jne` at 127cf0 (initially
      missed). Real type: `std::unordered_set<std::string>` (40B container, 40B
      node = 16B header + 24B std::string element). Confirmed by 1.0f
      max_load_factor at +0x188 in ctor at 12bec9 and by string-dtor pattern
      at 127d58-127d6b. Original Phase 11d classification was correct.
- [x] FOLLOW-UP from 13e: resolve VirtAddrNodeMapData fields
      RESOLVED — total 0x38 = vptr (8B) + `std::string name_` (+0x08, 24B)
      + `std::vector<int32_t> addresses_` (+0x20, 24B). Smoking gun:
      copy ctor @0x1c4dc0 calls `vector<int>::__throw_length_error` at 1c4e72;
      getJson @0x1c5240 reads "name" key from string field then iterates the
      vector with `movsxd rax, [r12]` (int32 sign-extension).
- [x] Sub-phase wrap-up

### 14b. DeviceType / DeviceFamily / DeviceOption (~169 symbols)

Device enumeration and capability queries. Survey done 2026-04-23 found
~169 symbols across ~13 distinct types and ~30 free functions. Split
into four sub-phases.

- [x] Survey symbol table — group by class

#### 14b-i. Enums + DeviceType pimpl + DeviceOptionSet — COMPLETE 2026-04-23
- [x] Decode DeviceFamily enum (12 one-hot values) from toString jump table
- [x] Decode DeviceTypeCode enum (33 values) from toString jump table
- [x] Reconstruct DeviceType pimpl (8B wrapping DeviceTypeImpl*)
- [x] Reconstruct detail::DeviceTypeImpl (88B / 0x58, polymorphic base)
- [x] Reconstruct DeviceOptionSet (72B / 0x48, dual-storage:
      unordered_set<DeviceOption> + map<string,DeviceOption>) — corrected
      from initial 0x30 estimate after investigating iterator type
- [x] Reconstruct DeviceOptionSetConstIterator (8B, wraps map iterator)
- [x] Reconstruct 11 predicate free functions (isDefined, isIa, isMfia,
      isUhfqa, isShfqa, isShfsg, isShfqc, isShfppc, isShfli, isGhfli, isVhfli)
- [x] Reconstruct toString(DeviceFamily) and toString(DeviceTypeCode)
- [x] Created reconstructed/notes/device_type.md
- [x] Build clean (0 errors, 0 warnings)
- [x] Sub-phase wrap-up

#### 14b-ii. DeviceOption enum + sfc options + GenericDeviceType subclass

Split 2026-04-23 after survey revealed scale: 14 knownOptions arrays,
6 sfc::*Option enums, 30+ detail::* device-type subclasses, plus
GenericDeviceType + string parsing. Two sub-sub-phases:

##### 14b-ii-a. Enums, options data, string conversions — COMPLETE 2026-04-23
- [x] Decoded all 15 .rodata knownOptions arrays at 0x962394..0x962aa0
      (Hf2li, Hf2is, Mfli, Mfia, Uhfli, Uhfawg, Uhfqa, Uhfia, Hdawg4,
      Hdawg8, Shfqa2, Shfqc, Shfli, Ghfli, Vhfli) — full (mask,code) tables
      in notes/device_type.md
- [x] Reconstructed sfc::Hf2Option / MfOption / UhfOption / HdawgOption /
      ShfOption / VhfOption / GhfOption enums (one-hot bitmasks, union
      across subclasses per family)
- [x] Reconstructed detail::OptionCodePair<T> template (8B: T mask + DeviceOption code)
- [x] Reconstructed detail::initializeSfcOptions<T,N> header-only template
      (body confirmed at 0x2e0d50)
- [x] Replaced DeviceOption stub with full 31-value enum (0..30, with MF/None
      alias at 0 and family-dependent strings for codes 0 and 6)
- [x] Replaced toString(DeviceOption, DeviceFamily) stub with real 31-case
      switch including family-dep "MFK"/"RTK" branches for HF2
- [x] toString(DeviceType const&) — CORRECTION: it is NOT a "<CODE> [opts]"
      formatter; the binary just returns toString(dt.code()). Existing 14b-i
      stub was actually correct.
- [x] BONUS: added toString(DeviceOptionSet, DeviceFamily, separator)
      helper @ 0x2d0130 (boost::algorithm::join over alphabetical names)
- [x] Replaced toDeviceTypeCode stub with lazy-init function-local static
      unordered_map<string, DeviceTypeCode> (33 entries incl. "none" alias)
- [x] FIXED isIa logic — Phase 14b-i impl had inverted truth table; correct
      semantics: codes UHFIA/MFIA always true, codes in broad mask 0x46BF7901
      else false, all other codes (incl. >=31) defer to hasOption(IA)
- [x] FIXED hasMf body — probes hasOption(MD=1) when family==MF, else
      hasOption(MF=0); matches binary at 0x2d3030 exactly
- [x] Build clean (0 errors, 0 warnings)
- [x] Sub-phase wrap-up

##### 14b-ii-b. detail::* device-type subclasses + GenericDeviceType

###### 14b-ii-b1. Per-family device subclasses — COMPLETE 2026-04-23
- [x] Reconstructed all 32 detail::* device-type subclasses derived from
      DeviceTypeImpl (Hf2, Hf2li, Hf2is, Mf, Mfli, Mfia, Uhf, Uhfli, Uhfawg,
      Uhfqa, Uhfia, Hdawg, Hdawg4, Hdawg8, Shf, Shfqa2, Shfqa4, Shfsg2,
      Shfsg4, Shfsg8, Shfqc, Shfli, Shfacc, Shfppc2, Shfppc4, Ghf, Ghfli,
      Pqsc, Qhub, Hwmock, Vhf, Vhfli) — split: 15 template-driven via
      `initializeSfcOptions<sfc::*Option,N>`, 11 inline-bit, 6 empty-options.
      All are 0x58 bytes (same as base) with vptr-only override.
- [x] Created device_subclasses.hpp with all 32 declarations.
- [x] Created 11 per-family .cpp files (device_hf2.cpp, device_mf.cpp,
      device_uhf.cpp, device_hdawg.cpp, device_shf.cpp, device_shfacc.cpp,
      device_ghf.cpp, device_pqsc.cpp, device_qhub.cpp, device_hwmock.cpp,
      device_vhf.cpp) mirroring the binary's per-family layout.
- [x] RESOLVED open question: there is NO `sfc::GhfOption`. The GHF family
      reuses `sfc::ShfOption` (Ghfli ctor at 0x2e3a00 calls the mangled
      `initializeSfcOptions<sfc::ShfOption, 5>` template instantiation
      directly, knownOptions @ 0x96298c). Removed the bogus GhfOption
      enum that was added in 14b-ii-a.
- [x] Discovered Hf2Factory subtype-selector dispatch pattern
      (subtype-selector bits in the high bits of the `unsigned long opts`
      arg; mask 0x1c0 for HF2). Same shape applies family-by-family in
      14b-ii-b2.
- [x] Build clean (0 errors, 0 warnings).
- [x] Sub-phase wrap-up.

###### 14b-ii-b1.5. knownOptions verification debt
- [ ] Disasm-verify the per-entry (mask, code) selections in 10
      knownOptions arrays where 14b-ii-b1 inferred them from
      sfc::*Option per-bit comments rather than reading the rodata
      payload directly: Uhfli (0x962580), Uhfawg (0x9625d0), Uhfqa
      (0x962608), Uhfia (0x962630), Hdawg4 (0x9626f8), Hdawg8
      (0x962728), Shfqa2 (0x962850), Shfqc (0x962870), Shfli
      (0x9628b0), Vhfli (0x962aa0). Hf2li, Hf2is, Mfli, Mfia, Ghfli
      are already verified. Inferred entries are flagged with TODO
      comments in the per-family .cpp files. Schedule before 14c.
      **Status (post-14e):** 14c-14e completed without doing this work;
      effectively deferred indefinitely. Low priority — affects only
      field-name accuracy of inferred (mask, code) pairs, not behavior.
      Consider closing if no consumer ever needs these names.

###### 14b-ii-b2. Factories, Unknown/Generic device types, parser — COMPLETE 2026-04-23
- [x] Reconstruct abstract base `detail::DeviceFamilyFactory` @ 0x2e0590
      (vtable-only 8B base; virtual `doMakeDefault()` and
      `doMakeDeviceType(unsigned long)`; non-virtual base trampolines
      `makeDefault()` @ 0x2e0590 and `makeDeviceType(opts)` @ 0x2e05b0).
- [x] Reconstruct `detail::makeDeviceFamilyFactory(DeviceFamily)`
      dispatcher @ 0x2e05d0 — switches on family one-hot value to
      construct one of the 11 per-family Factory instances (or
      UnknownDeviceTypeFactory).
- [x] Reconstruct `detail::NoDeviceTypeFactory` @ 0x2e0700 / 0x2e0730
      (returns nullptr from both methods).
- [x] Reconstruct `detail::UnknownDeviceTypeFactory` @ 0x2e0760 / 0x2e07b0
      (both methods return a heap-allocated `UnknownDevice`).
- [x] Reconstruct 11 per-family Factory classes — each with
      `makeDefault()` and `doMakeDeviceType(unsigned long opts)` and
      its own subtype-selector dispatch (see notes/device_type.md
      Phase 14b-ii-b2 table for the complete selector mappings).
      All 11 wired into `device_factories.{hpp,cpp}` via a single
      `ZHINST_DEFINE_FAMILY_FACTORY` macro.
- [x] Reconstruct `detail::UnknownDevice` @ 0x2d3320 — synthetic sentinel
      ctor stores `code=33`, `family=0x800` via `movabs rax,0x80000000021`.
- [x] Reconstruct `detail::GenericDeviceType` @ 0x2d3c60 — same 0x58
      layout as base (no extra fields); only the deleting dtor slot is
      overridden (clone reuses the base impl). Ctor calls
      toDeviceTypeCode + toDeviceFamily + (toDeviceOptions OR empty
      DeviceOptionSet) and forwards to `DeviceTypeImpl(tuple)`.
- [x] Reconstruct `DeviceTypeImpl(tuple<DeviceTypeCode, DeviceFamily, DeviceOptionSet>)`
      ctor @ 0x2d3190 (only used by GenericDeviceType).
- [x] Reconstruct `DeviceType(string, vector<string>)` @ 0x2d2ae0
      (NOT 0x2d29c0 as previously documented). Wraps a fresh
      GenericDeviceType.
- [x] Reconstruct **NEW** `DeviceType(string, string)` @ 0x2d2a00 —
      splits the second string via splitDeviceOptions() and forwards
      to the (string, vector) ctor.
- [x] Reconstruct **NEW** free function `splitDeviceOptions(string)`
      @ 0x2d0460 — boost::trim_copy_if then split on '\n'.
- [x] Reconstruct **NEW** free function `toDeviceFamily(string)`
      @ 0x2debd0 — fast-path inline string compares + lazy-init
      function-local-static `map<string, DeviceFamily>` of 10 entries
      (no shfacc, no unknown) + `boost::starts_with` prefix verify.
      Default on miss = DeviceFamily(0x800).
- [x] Reconstruct **NEW** free function `toDeviceOptions(vector<string>, DeviceFamily)`
      @ 0x2d0fb0 — try/catch around toDeviceOption(name) inserts;
      unknown names silently dropped.
- [x] Build clean (0 errors, 0 warnings).
- [x] Sub-phase wrap-up.

###### 14b-ii-b2-followup. toDeviceOption(string) body — COMPLETE 2026-04-23
- [x] Reconstruct `zhinst::toDeviceOption(string)` @ 0x2d05b0.
      Function-local-static unordered_map of 33 entries
      (mov edx, 0x21 confirms count) decoded from disasm lines
      533249..533490. Source strings live in .data.rel.ro 0xb08ef8..
      0xb08ff8 as `const char*` pointers (NOT std::string objects)
      targeting .rodata literals at 0x90b60e. Two duplicate keys map
      code 0 ("MF" and "MFK") and code 6 ("RT" and "RTK"). Throws
      `zhinst::Exception` via `boost::throw_exception` on miss;
      `toDeviceOptions()` catches and silently drops.
- [x] Resolve QA/QC asymmetry: parser accepts "QC" → DeviceOption::QA
      while toString emits "QA". Real binary quirk; enum kept as `QA`,
      parser table maps "QC" → QA. Documented in notes/device_type.md.
- [x] Confirm DeviceOptionName::* storage class: 8-byte spacing in
      .data.rel.ro confirms `const char*` pointers, not std::string.
- [x] Build clean (0 warnings).
- [x] Verify symbol now defined: nm shows `T` for
      `_ZN6zhinst14toDeviceOptionERK...` in device_type.cpp.o
      (previously `U`).

#### 14b-iii. AwgDeviceType + AwgSequencerType + getAwgDeviceProps templates
- [x] Decode AwgDeviceType enum (one-hot bitfield, 9 values 1..256)
      — already in `types.hpp` from Phase 3d; no changes needed.
- [x] Decode AwgSequencerType enum from toAwgDeviceType — 3 named values
      (Auto=0, QA=1, SG=2); the formatter's "unknown" branch has no
      enumerator (modeled as default-case behavior).
- [x] Reconstruct AwgDeviceProps struct (return type of getAwgDeviceProps<>) —
      0x80 bytes (libc++) / 0xa0 (libstdc++); 4 strings + type + 2 qwords
      + bool. Field NAMES (`maxWaveformSamples`, `maxWaveformBytes`,
      `supportsExtraFeature`) are inferred — see follow-up below.
- [x] Reconstruct AwgPathPatterns struct (3 strings, 0x48/0x60 bytes)
      and 6 anonymous-namespace globals (Default / GrimselQa / GrimselSg
      / GrimselLi / Gurnigel / Maloja); the latter two are runtime copies
      of GrimselLi.
- [x] Reconstruct all 9 explicit specializations of getAwgDeviceProps<T>
      (binary 0x2cc5f0..0x2cdb00). HDAWG is the only one that consults
      `dt.hasOption(ME=0x13)`.
- [x] Reconstruct toAwgDeviceType — 29-entry switch over (code-4) for
      codes 4..32, with SHFQC(21) special-cased on sequencer.
- [x] Reconstruct makeUnsupportedAwgSequencerErrorMessage — produces
      `"Unsupported device or sequencer type (<dtype>, sequencer: <seq>)."`.
- [x] Build clean — `cmake --build .` from `reconstructed/build/` succeeds
      with no warnings; `awg_device_props.cpp.o` symbol is `T`.
- [x] Sub-phase wrap-up — see below for proposed follow-ups.

#### 14b-iii follow-ups
- [ ] **Verify inferred AwgDeviceProps field names** when Phase 14b-iv
      consumers are reconstructed — `maxWaveformSamples`,
      `maxWaveformBytes`, `supportsExtraFeature` are guesses.
- [x] **Reconstruct `getDeviceConstants(AwgDeviceType)` body @ 0x2cc0c0**
      — done in Phase 14b-iii.5. All 9 case blocks were already
      populated in Phase 7e; this sub-phase replaced the placeholder
      `std::runtime_error` default branch with the real
      `BOOST_THROW_EXCEPTION(ZIAWGCompilerException(...))` throw, added
      the necessary includes, and documented the jump-table layout,
      XMM constant table, sampling-rate doubles, and source_location
      values in `reconstructed/notes/device_constants.md`. Build clean.

#### 14b-iv. Helpers + free functions + final wrap-up
- [x] Reconstruct getOptionsAsString(DeviceType const&)
- [x] Reconstruct expand(DeviceFamily) @ 0x2dfbc0
- [x] Reconstruct toStrings(vector<DeviceFamily> const&)
- [x] Reconstruct toDeviceOptions, detail::generateMfSfc,
      detail::makeDeviceFamilyFactory
- [x] Reconstruct anonymous-namespace makeDevicesSet()::$_0 lambda
- [ ] **Verify inferred AwgDeviceProps field names** (`maxWaveformSamples`,
      `maxWaveformBytes`, `supportsExtraFeature`) — no consumer in 14b-iv;
      carry forward to a later phase (14d or 15) where AwgDeviceProps
      callers will be reconstructed.
- [x] Update OVERVIEW.md with consolidated Phase 14b summary
- [x] Build clean
- [x] Sub-phase wrap-up

#### Phase 14b — COMPLETE

All four sub-phases (14b-i, 14b-ii, 14b-iii, 14b-iii.5, 14b-iv) closed.
One carry-forward item:

- [ ] **AwgDeviceProps field-name verification debt** (carried from 14b-iii):
      `maxWaveformSamples`, `maxWaveformBytes`, `supportsExtraFeature` are
      INFERRED from values, not confirmed by reading callers. To be
      verified during Phase 14d (or wherever AwgDeviceProps consumers
      next surface). Notes: `notes/awg_device_props.md`.

### 14c. Logging + tracing (20 zhinst symbols across 17 functions) — DONE

Logging infrastructure used across the compiler. The earlier "73 symbols"
estimate counted boost.log + OpenTelemetry template-instantiation glue;
the actual zhinst-namespace surface is 20 symbols (8 logging + 12 tracing).

- [x] Surveyed symbols — 20 zhinst-namespace symbols across 17 distinct
      functions (logging: 8 / tracing: 12). The "73" estimate counted
      boost.log + OTel template-instantiation glue.
- [x] Determined LogRecord (≥0x118), TraceProvider (0x20), ScopedSpan
      (0x30) layouts; documented in notes/logging_tracing.md.
- [x] Reconstructed all 17 functions — clean cmake build, zero warnings.
- [x] Sub-phase wrap-up — OVERVIEW + notes updated; OpenTelemetry stub
      headers added under `include/opentelemetry/` to type-check
      tracing.cpp without the real SDK installed.

**Verification debt** (carried forward):

- Severity enumerator ordering not directly observed (only the 8 string
  names "Trace".."Fatal" are in .rodata). Verify when the first LOG_*
  macro caller surfaces in a later-phase reconstruction.
- OpenTelemetry stub headers approximate ABI sizes only — real linking
  against opentelemetry-cpp would require swapping the stub umbrella
  (`include/opentelemetry/_stub_fwd.hpp`) for the actual SDK headers.

### 14d. ElfReader / ElfWriter (Phase 13d dependency)

Surfaced during Phase 13d: cacheFileOutdated, cacheFile and getCachedFile
all use ElfReader/ElfWriter. Phase 13d created a minimal forward-decl
header (`elf_reader.hpp`) but the implementations are still placeholder.

- [x] Disassemble ElfReader::ElfReader(string) @0x2c3110 — file open + ELFIO load
- [x] Disassemble ElfReader::getSection(string) const @0x2c4000 — section table walk (throws ElfException on miss, not nullptr)
- [~] ~~Verify ElfSection vtable layout (get_data @ +0xC0, get_size @ +0x98)~~ — CANCELLED: reconstruction uses `ELFIO::section` directly; the original stub's `ElfSection` class was fictional. The free helper `zhinst::sectionAsString(const ELFIO::section*)` in `elf_reader.hpp` covers the only access pattern that mattered.
- [x] Disassemble ElfWriter::ElfWriter(uint16_t) @0x2934a0 — channel arg ctor (already complete from earlier phases — src/elf_writer.cpp has 8 methods, ctor 0x2934a0 included)
- [x] Identify ElfWriter::addSection / save methods (already complete — src/elf_writer.cpp: addCode/addData/addWaveform/writeFile×2/setMemoryOffset/prepareHeader)
- [x] Once ElfReader is real, fold the deferred CachedParser stubs (cacheFile body) into full reconstructions — 5 call sites in cached_parser.cpp converted from fictional `getDataAsString()` to `sectionAsString()`
- [x] Sub-phase wrap-up: complete — see `reconstructed/notes/elf_reader.md`. Net additions: `src/elf_reader.cpp` (5 methods + ElfException ctor/dtor/what), `elf_reader.hpp` upgraded from 71-line forward-decl stub to 0x98-byte full reconstruction with `private ELFIO::elfio` base + `sectionAsString()` helper. ElfReader layout corrected (was 0x90, now 0x98 with `vector<section*>` at +0x78); `readHeader` sentinels reinterpreted as `get_class()==ELFCLASS32` && `get_encoding()==ELFDATA2LSB`; `getSection` semantics corrected (throws, not nullptr). Real cmake build clean, zero warnings.

### 14e. zhinst::sfc namespace (NEW — added during 14b-iv wrap-up)

Surfaced during Phase 14b-iv reconstruction of `generateMfSfc`: its
mangled return type is `zhinst::sfc::FeaturesCode`, used as a
strong-typed wrapper around uint64. Currently stubbed as `uint64_t`
in `mf_sfc.cpp`. The sfc namespace also contains:

- `sfc::FeaturesCode` (type — likely strong typedef / wrapper)
- `sfc::Hf2Option`, `sfc::MfOption`, `sfc::UhfOption`, `sfc::HdawgOption`,
  `sfc::ShfOption` (enum classes)
- `detail::OptionCodePair<T>` template (small POD pair type)
- 8+ `detail::initializeSfcOptions<sfc::*Option, N>` template
  specializations (parser helpers turning SFC bitfield → DeviceOptionSet)

- [x] Survey full `zhinst::sfc::` symbol set in nm output — 13 `initializeSfcOptions` instantiations across 6 OptionEnum types (Hf2/Mf/Uhf/Hdawg/Shf/Vhf); no out-of-line `FeaturesCode` symbols (header-only POD); see `notes/device_type.md` (sfc namespace section)
- [x] Determine `sfc::FeaturesCode` layout (likely 8B wrapper around uint64) — confirmed: 8B trivially-copyable POD wrapper around uint64; evidence is the source_location literal at 0x2deb37 naming the type, plus the direct `rax`-return at 0x2deac1 (no sret, ≤8B & trivially-copyable per SysV ABI)
- [x] Reconstruct sfc option enums (compare bit positions vs `generateMfSfc`) — already complete from Phase 14b-iv: 6 enum classes in `device_type.hpp:163-285` (Hf2Option, MfOption, UhfOption, HdawgOption, ShfOption, VhfOption), bit positions decoded from .rodata 0x962394..0x962aa0 knownOptions arrays
- [x] Reconstruct `OptionCodePair<T>` and `initializeSfcOptions<>` template — already complete from Phase 14b-iv: header-only template in `device_type.hpp`; 13 instantiations confirmed in nm survey
- [x] Update `mf_sfc.cpp` to return real `sfc::FeaturesCode` type — declaration in `device_type.hpp` and body in `mf_sfc.cpp` both promoted; 3 `return` sites wrap their uint64 expression in `sfc::FeaturesCode{...}`
- [x] Sub-phase wrap-up: complete — see `reconstructed/notes/device_type.md` (sfc namespace section). Net additions: `sfc::FeaturesCode` struct in `device_type.hpp` (8B wrapper); `generateMfSfc` declaration & body promoted from `uint64_t` to `sfc::FeaturesCode`. No other call sites needed updating. Real cmake build clean, zero warnings.


---

## Phase 15: Accuracy refinement

Fix approximate logic and deferred TODO markers in existing code.

> **Recommended order**: 15a (close item #4 done) → **15b first** (small
> verification work, builds momentum) → 15a-i (multi-session, frontend
> lowering data model) → 15a #1 (lower() return type, depends on 15a-i)
> → 15c (AsmOptimize, dense disasm).

### 15a. Compiler pipeline gaps

- [x] WaveformGenerator field_B0_ (shared_ptr<void>) — **CLOSED 2026-04-23 as
      negative finding.** No setter exists anywhere in the binary. The 16B
      slot at +0xB0 is reserved-but-unused (zero-initialized in WG ctor at
      0x2482aa, never written elsewhere). Apparent +0xb0/+0xb8 read sites
      inside WG methods (251973, 25385a, 255068, ...) were misattributed —
      they read the union body of `Value` parameter objects (Value has
      type tag at +0xA8 and union storage at +0xB0). The single true
      +0xb0 write at 0x11d180 is `Compiler::Compiler` writing its own
      AsmCommands shared_ptr, not WG-related. Evidence chain documented
      in `waveform_generator.hpp` comments. Closes #110.
- [x] Fix lower() return type in compiler.hpp — **DONE in 15a-i.** Returns
      `LowerResult` (32B sret = 2 shared_ptrs: `shared_ptr<Node>` from
      FrontendLoweringState.result + `shared_ptr<EvalResults>` from the
      evaluate virtual). Was void, now corrected. Body partially implemented
      (Context+State construction done, virtual call TODO'd pending vtable
      slot declaration on SeqCAstNode).

### 15a-i. Frontend Lowering data model — COMPLETE 2026-04-23

Decode the data types flowing through `FrontEndLoweringFacade::lower`.
~130 related symbols across EvalResults / FrontendLoweringState /
FrontendLoweringContext. EvalResults is the central data type of the
lowering pipeline (not a leaf), so this is large. Blocks 15a #1.

**Findings:** EvalResults = 0x80 bytes (7 fields, 14 methods). Value
layout was WRONG (0x20→0x28 correction). lower() return = 32B sret
(2 shared_ptrs, not 64B/4 as previously claimed). FrontendLoweringState
.result = shared_ptr\<Node\> (was void/TBD). EvalResults.arrayBacking_
at +0x70 = shared_ptr\<EvalResults\> (self-referential, used by
SeqCArray). See `notes/frontend_lowering.md`.

- [x] EvalResults layout (0x80B; 14 direct methods scattered across
      0x15a750..0x247600 — heavy inlining): 10 setValue overloads,
      ctor, copy-ctor, dtor, getValue — all declared in eval_results.hpp
- [x] EvalResultValue layout (0x38B) — renamed fields from opaque to
      typed: VarType + VarSubType + Value(embedded 0x28) + AsmRegister
- [x] lower() sret aggregate type — **CORRECTED to 32B / 2 shared_ptrs**
      (was wrongly claimed as 64B / 4 sps). Return type:
      `LowerResult{shared_ptr<Node> astResult, shared_ptr<EvalResults> evalResult}`
- [x] FrontendLoweringState::result pointee — **shared_ptr\<Node\>**
      (was shared_ptr\<void\>/TBD). Evidence: lower() copies state.result
      into sret[0]; caller stores into Compiler+0x28 (= shared_ptr\<Node\> ast_)
- [x] Sub-phase wrap-up — see notes/frontend_lowering.md. Build clean.

### 15b. Prefetch/Cache deferred items (10 markers) — COMPLETE 2026-04-23

From Phase 10.5f — minor verification items in prefetch.cpp and
prefetch_splitplay.cpp.

**Audit finding 2026-04-23**: All 10 markers were already resolved during
post-audit edits on 2026-04-22 (between todo_audit.md @ 01:39 and the
prefetch source edits at 04:35-04:53) but were never officially closed
out. Verified by case-insensitive search for TODO/FIXME/APPROXIMATE/
VERIFY/XXX in both files — zero markers remain. The relevant code
sections now contain inline disasm citations (e.g. line 476-481 cites
`mov edx,0x114; bt edx,ecx` for the bitmask, line 526 cites 0x1d031d→0x1d0869
for the eligibility branch, line 125 cites 0x1dd462..0x1dd49c for the
weak_ptr::lock pattern). Build clean.

- [x] Verify bitmask 0x114 exact bits — done; cited at prefetch.cpp:476-481
- [x] Verify enum name ordering — done; cited at prefetch.cpp:500-506
- [x] Verify NodeType::Play cmp $0x2 — done; cited at prefetch.cpp:504
- [x] Verify shared_ptr aliasing constructor — replaced with semantic
      `weak_ptr::lock()` at prefetch_splitplay.cpp:126
- [x] Resolve remaining 6 prefetch_splitplay.cpp items — all converted
      to semantic equivalents with disasm citations
- [x] Sub-phase wrap-up — see notes/archive/phase_15b_prefetch_audit.md

### 15c. AsmOptimize approximate logic — COMPLETE 2026-04-23

**Primary finding**: register field semantics in `AssemblerInstr` were
inverted throughout all AsmOptimize methods. reg2(+0x20)=READ source,
reg0(+0x28)=WRITE destination, reg1(+0x30)=dual (read if cmdType==1 or 7;
written if cmdType==7). Prior code had reg2 as "dest" and reg0 as "src1"
— exactly backwards. Fixed in isRead, isWritten, getNextActionForReg
(rewritten), registerIsNeverWritten, registerUpdate, isLabelCalled
(iteration direction). Field NAMES kept as reg0/reg1/reg2 to avoid
cascading rename across 20+ files; semantic comments cite Asm-relative
and AssemblerInstr-relative offsets.

- [x] removeUnusedRegs inner scan loop (#47) — fully reconstructed @0x27e760
      (291 lines). Skip bitmask 0x29 documented (skips INVALID, LABEL, cmd=4
      — NOT NOP/MESSAGE/ERROR_MSG as previously claimed).
- [x] registerAllocation conflict detection (#48) — structurally reconstructed
      @0x27ebb0 (1466 lines, 6-phase graph-coloring with backward-branch
      live-range extension). Internal `PhysicalRegister` type confirmed via
      vector dtor mangled name @0x281840.
- [x] splitConstRegisters splitting heuristic (#49) — structurally reconstructed
      @0x280440 (444 lines, 2-pass barrier algorithm). Added
      `AsmRegister::magicSkipRegister()` @0x28ebb0 returning `{INT_MAX, true}`.
- [x] Sub-phase wrap-up — see `notes/optimization_passes.md`. Build clean.

---

## Phase 16: Validation, file reorganization, and stub recovery

Phase 16 is a multi-stage cleanup before final validation. Originated
from a noticed pile-up in `custom_functions.cpp` and the realisation
that prior tracking had drifted. Full audit catalog in
`reconstructed/notes/audit_phase16a.md`.

### 16a. Codebase marker & stub sweep

- [x] Grep all sources for TODO/FIXME/TBD/APPROXIMATE/VERIFY/XXX/HACK
      markers; cross-reference with TODO.md and unknowns.md;
      catalog stub patterns including the `throw std::runtime_error("X
      not implemented")` pattern (which is invisible to plain TODO grep).
- [x] Per-file namespace/class inventory (find files piling multiple
      unrelated types).
- [x] Catalog written to `reconstructed/notes/audit_phase16a.md`.
- [x] Sub-phase wrap-up: 6 untracked items + multiple stale TODO.md rows
      surfaced. Each one tracked as an explicit item below; details in
      the audit file.

### 16b. File-organization split

Issues from audit Section C, D, E, F. Refactor (no semantic change) so
that subsequent disassembly work lands in the right files.

- [ ] **Split `custom_functions.{cpp,hpp}` (audit §C1)** — currently
      contains 10 distinct classes. Move out:
    - [ ] `MathCompiler` + `MathCompilerException` → `math_compiler.{cpp,hpp}`
    - [ ] `NodeMapData` + `VirtAddrNodeMapData` + `DirectAddrNodeMapData`
          + `NodeMapItem` → `node_map_data.{cpp,hpp}`
    - [ ] `EvalResultValue` → fold into `frontend_lowering.{hpp,cpp}`
          (Phase 15a-i data model) or own file `eval_result_value.*`
- [ ] **Investigate `ErrorMessages` ODR risk (audit §F)** — class
      appears in 9 separate .cpp files. Verify whether it is
      anonymous-namespace per-TU (benign) or shared (ODR violation).
      If shared, consolidate into `error_messages.{cpp,hpp}`.
- [ ] **Verify `compiler.cpp` Compiler/FrontEndLoweringFacade split
      (audit §E)** — likely a small adapter, may warrant own file.
- [ ] **Split `waveform_generator.cpp` exceptions (audit §D)** — minor;
      `WaveformGeneratorException` + `WaveformGeneratorValueException`
      → `waveform_generator_exceptions.{cpp,hpp}`. Low priority.
- [ ] Sub-phase wrap-up: real cmake build clean; OVERVIEW.md updated;
      relevant notes refreshed.

### 16c. TODO.md / unknowns.md reconciliation

Issues from audit Section G. Refresh tracking against the post-16b
codebase before executing 16d.

- [ ] Refresh summary table at top of TODO.md with audit-verified
      counts (Section G):
    - SeqCAstNode print/clone — recount including macro-expanded stubs
      (53 subclasses × 2 methods)
    - CustomFunctions built-in function bodies — recategorise: 14
      utility-method stubs (dispatch hubs) + 6 thin play wrappers + N
      one-liner builtins
    - WaveformGenerator throw-stubs — bump to "32 throw-stubs in
      waveform_generator.cpp lines 666-769 + 4 misc"
    - Prefetch/Cache — change "10" to "0 ✅" (resolved per Phase 15b
      audit)
    - Compiler pipeline — "1" should be "2"
    - CustomFunctions field unknowns — recount; #101/#102/#104 closed
- [ ] Promote Phase 15c carry-forwards (simplifyAssign, splitReg,
      register field rename) to explicit summary table rows so they're
      visible at a glance.
- [ ] Cross-link unknowns.md actionable items (#98, #10) to summary
      table rows.
- [ ] Sub-phase wrap-up: TODO.md and unknowns.md numbers match audit
      reality.

### 16d. Stub & gap execution

Order of execution agreed: HIGH → MEDIUM → LOW. Each item references
audit catalog for context.

#### High priority — runtime correctness

- [ ] **CustomFunctions::CustomFunctions ctor binding gap (audit §C2,
      C3, item I.2)** — line 422 of `custom_functions.cpp` notes the
      19KB binding block (`funcMap_[name] = bind(&method, this, _1)`
      for 87 builtins) is omitted. **Without this the funcMap is empty
      at runtime and dispatch fails.** Reconstruct, possibly via a
      script that walks the binary's call-site list. Critical
      correctness gap.
- [ ] **Reconstruct `CustomFunctions::play` @0x15f090 (audit §C2)** —
      core dispatch hub; many wrappers delegate to it.
- [ ] **Reconstruct `CustomFunctions::playIndexed` @0x160e00 (audit
      §C2)** — same.
- [ ] **Resolve aux-wrapper SubFunc TBD values (audit §C2 last
      bullet)** — 6 wrappers (`playAuxWave`, `playAuxWaveIndexed`,
      `playDIO*`, etc.) all use `SubFunc::Default` placeholder.
      Determine real values from binary.

#### Medium priority — feature completeness

- [ ] **Reconstruct remaining CustomFunctions utility stubs (audit
      §C2)** — 11 more dispatch helpers: `generateWaveform`,
      `mergeWaveforms`, `writeToNode`, `addSyncCommand`, `printF`,
      `addWaitCycles`, `writeLS64bit`, `checkFunctionSupported`,
      `checkWaveformMinLengthTrig`, `checkOffspecWaveLength`,
      `lookupNode`, `setWaitCyclesReg`, `optionAvailable`.
- [ ] **WaveformGenerator DSP throw-stubs (audit §B, §D)** — 32
      functions in `waveform_generator.cpp:666-769`. Includes trig
      (sin/cos/sinc/ramp/sawtooth/triangle/chirp), pulses (drag,
      blackman, hamming, hann, mask, marker), random (rand,
      randomGauss, randomUniform, lfsrGaloisMarker), filters (rrc,
      filter), composition (vect, placeholder, join, add, interleave,
      multiply, cut, flip, circshift, merge, grow), and 4 misc
      (readWave, markerImpl, interpolateLinear, eval).
- [ ] **MathCompiler 67 symbols** — Phase 14a was deferred. After 16b
      split, work happens in `math_compiler.cpp`.

#### Low priority — quality / completeness

- [ ] **SeqCAstNode print/clone macro expansion (audit §I.6)** — the
      macro at `seqc_ast_node.hpp:154` expands to 53×2 stub method
      bodies. Most are mechanical; verify against binary.
- [ ] **WavetableManager\<T\> remaining 14 methods** — template, partially
      done.
- [ ] **smap remaining logic** — ~0x1E6 bytes after alui call (unknowns
      #10).

### 16e. Validation against the real .so

Original Phase 16 goals — execute after 16b/c/d.

- [ ] Write comparison tests against the real .so
- [ ] Verify struct sizes match binary (where `static_assert(sizeof(X)
      == N)` is possible)
- [ ] Test key methods against known inputs/outputs
- [ ] Final marker sweep: re-run the audit grep from 16a and confirm
      counts have dropped to expected residual levels.

---

## Deferred / Low Priority

- [ ] WavetableManager\<T\> remaining methods (14 methods, ~6KB) — template, partially done
- [ ] SeqCAstNode `evaluate()` virtual — signature TBD (#113 in archived unknowns)
- [ ] Semantic/naming questions from Phase 2-3 (unknowns #23-28, #32, #38) — low value
- [ ] AWGAssemblerImpl internal field purposes (#39-41) — documented, low priority
- [ ] Cache/Prefetch implementation detail unknowns (#61-64, #68-69, #75, #77, #81) — diminishing returns
- [ ] Exception error-code details (#90-91) — not on seqc critical path
- [ ] smap remaining logic — ~0x1E6 bytes after alui call (unknowns #10)
- [ ] **AsmOptimize::simplifyAssign @0x280e10** — still uses pre-correction
      register field references (reg0/reg2 swapped). Not in scope of
      Phase 15c unknowns #47-49 but should be revisited if simplifyAssign
      is ever exercised by tests. Carry-forward from Phase 15c.
- [ ] **AsmOptimize::splitReg @0x281000** — current stub is ~20 lines;
      real binary function is ~500 lines. Full reconstruction deferred.
      Carry-forward from Phase 15c.
- [ ] **assembler.hpp register field rename** — reg0/reg1/reg2 names are
      misleading (reg2 is the READ source, reg0 is the WRITE destination).
      Renaming would cascade across 20+ files; deferred indefinitely.
      Carry-forward from Phase 15c.
