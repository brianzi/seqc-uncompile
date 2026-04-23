# TODO — Reconstructed zhinst SeqC Compiler

> **Completed Phases 1-12 archived in
> [`reconstructed/notes/archive/TODO_phases_1-12.md`](reconstructed/notes/archive/TODO_phases_1-12.md).**
> 597 completed items across 12 phases (plus sub-phases 10.5-10.8).

---

## Summary of remaining work

| Category | Stub count | Notes |
|----------|-----------|-------|
| SeqCAstNode print/clone bodies | 106 (53×2) | Macro-expanded; only SeqCValue::clone has a caveat |
| CustomFunctions PlayArgs-blocked | 4 | playAuxWave, playDIOWave, playWaveDIO, playWaveZSync (complex wrappers) |
| CustomFunctions builtin stubs | ~55 | return-nullptr stubs with addresses; many follow mechanical patterns |
| CustomFunctions ctor binding gap | 3 | 78/81 entries reconstructed; 3 alias targets TBD |
| WaveformGenerator remaining | 3 | readDoubleAmplitude check, interpolateLinear formula, rrc 0x64 |
| MathCompiler symbols | 0 | ✅ complete (67 symbols, all accounted for) |
| WavetableManager\<T\> | 14 methods | Template, partially done |
| DeviceType/Family/Option | 0 | ✅ complete (Phase 14b) |
| CachedParser method bodies | 0 | ✅ resolved in Phase 13d |
| Prefetch/Cache approximate logic | 0 | ✅ resolved in Phase 15b |
| AsmOptimize carry-forwards | 3 | simplifyAssign body, splitReg body, register field rename (Phase 15c) |
| Compiler pipeline gaps | 2 | lower() sret done; vtable wiring + evaluate() remain |
| Logging/tracing | 0 | ✅ done — see notes/logging_tracing.md |
| NodeMapData | 0 | ✅ complete (Phase 17a) |

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

- [x] Reconstruct trig family (sin, cos, sinc) — share frequency/phase reader
- [ ] Reconstruct envelope family (blackman, hamming, hann, drag) — Gauss-derived
- [x] Reconstruct ramp family (ramp, sawtooth, triangle, chirp)
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

### 16b. File-organization split ✅ DONE

Completed 2026-04-23. Refactored `custom_functions.{cpp,hpp}` into
four well-scoped TU pairs. Build clean, zero lost markers.

- [x] **Split `custom_functions.{cpp,hpp}` (audit §C1)** — extracted:
    - [x] `MathCompiler` + `MathCompilerException` → `math_compiler.{cpp,hpp}`
    - [x] `NodeMapData` hierarchy + `NodeMapItem` → `node_map_data.{cpp,hpp}`
    - [x] `EvalResultValue` → `eval_result_value.{cpp,hpp}`
- [x] **ErrorMessages ODR investigation (audit §F)** — single class,
      single definition in `error_messages.{hpp,cpp}`. Audit grep was
      over-broad; all 9 .cpp "occurrences" are just `format()`/`get()`
      calls. No action needed.
- [x] **compiler.cpp Facade split (audit §E)** — `FrontEndLoweringFacade::lower`
      is 36 lines and gates on unfinished `SeqCAstNode::evaluate` virtual.
      Not worth splitting. Will revisit when the virtual is wired.
- [ ] ~~Split `waveform_generator.cpp` exceptions (audit §D)~~ — deferred
      to opportunistic; execute next time `waveform_generator.cpp` is
      opened for unrelated work.
- [x] Sub-phase wrap-up: build clean; OVERVIEW.md updated; audit findings
      resolved or deferred with rationale.

### 16c. TODO.md / unknowns.md reconciliation ✅ DONE

Completed 2026-04-23. Summary table rewritten with 16 rows matching
audit-verified reality. Audit file renamed to `notes/outstanding_work.md`.

- [x] Refresh summary table — all Section G rows corrected.
- [x] Promote Phase 15c carry-forwards — new "AsmOptimize carry-forwards"
      row (3 items: simplifyAssign, splitReg, register rename).
- [x] Cross-link unknowns.md — new "Unknowns cross-ref" row (#98, #10).
- [x] Fold `audit_phase16a.md` content into TODO.md; file deleted (no
      synonym files for TODO — see AGENTS.md).
- [x] Sub-phase wrap-up: TODO.md and unknowns.md numbers match reality.

### 16d. Stub & gap execution ✅ DONE (HIGH + MEDIUM complete)

Completed 2026-04-23. HIGH and MEDIUM items all done. LOW items deferred
to future phases (verification-only or diminishing returns).

Order of execution: HIGH → MEDIUM → LOW. Each item references
audit catalog for context.

#### High priority — runtime correctness ✅ DONE

Completed 2026-04-23. Ctor binding block (78/81), play() + playIndexed()
structurally reconstructed, SubFunc enum fully resolved (Default=1, Aux=2,
Now=3, DigTrigger=4). 4 complex wrappers identified and split to own TODO.

- [x] **CustomFunctions::CustomFunctions ctor binding gap (audit §C2,
      C3, item I.2)** — 78/81 emplace calls reconstructed (76 standard +
      2 confirmed aliases). 3 alias entries at 12c352/12dfe2/12e082
      unresolved (require deeper register tracking).
- [x] **Reconstruct `CustomFunctions::play` @0x15f090 (audit §C2)** —
      full structural reconstruction (7536B, SubFunc switch, PlayArgs,
      channel loop, merge, asm emit). Documented pseudocode in .cpp.
- [x] **Reconstruct `CustomFunctions::playIndexed` @0x160e00 (audit
      §C2)** — same treatment (6428B).
- [x] **Resolve aux-wrapper SubFunc TBD values (audit §C2 last
      bullet)** — SubFunc enum: Default=1, Aux=2, Now=3, DigTrigger=4.
      playAuxWaveIndexed→playIndexed(2), playWaveDigTrigger→play(4).
      4 wrappers (playAuxWave, playDIOWave, playWaveDIO, playWaveZSync)
      are complex — own PlayArgs construction, no play() delegation.

#### Medium priority — feature completeness

- [x] **Reconstruct remaining CustomFunctions utility stubs (audit
      §C2)** — All 13 dispatch helpers reconstructed from binary:
      `checkFunctionSupported` (bitmask test + throw 0x49),
      `checkWaveformMinLengthTrig` (throw 0xF3),
      `checkOffspecWaveLength` (warn 0xF4/0xE6),
      `optionAvailable` (linear scan + usedFeatures_ insert),
      `lookupNode` (initNodeMap + retrieve + throw 0x83),
      `setWaitCyclesReg` (addi + suser 0x6F),
      `generateWaveform` (prepend name + delegate to generate()),
      `addSyncCommand` (Hirzel/Cervino switch),
      `printF` (boost::format with VarType dispatch),
      `addWaitCycles` (addi + suser 0x69),
      `writeLS64bit` (two addi+suser pairs for 64-bit split).
      `mergeWaveforms` and `writeToNode` remain as documented stubs
      (3KB and 23KB respectively, need WaveformGenerator complete type).
- [ ] **Reconstruct 4 complex play wrappers** — Each has own PlayArgs
      construction, no play()/playIndexed() delegation:
      `playAuxWave` @0x135610 (~5KB), `playDIOWave` @0x1369f0 (~3.4KB),
      `playWaveDIO` @0x137740 (784B), `playWaveZSync` @0x137a50 (~3.2KB).
- [x] **WaveformGenerator DSP throw-stubs (audit §B, §D)** — All 32
      throw-stubs reconstructed from binary. Full implementations for:
      sin, cos, sinc, ramp, sawtooth, triangle, chirp (trig/math);
      drag, blackman, hamming, hann, rrc (window/pulse); mask, marker
      (wrappers to markerImpl); rand, randomGauss, randomUniform,
      lfsrGaloisMarker (random); vect, placeholder, flip, interleave
      (composition). Structural outlines for larger functions: join,
      multiply, cut, circshift, merge, grow, filter, readWave,
      markerImpl. File grew from 772 to 2305 lines.
- [x] **MathCompiler 67 symbols** — Already complete. 23 single-arg
      wrappers + 5 multi-arg + ctor + call() + functionExists() +
      exception class. All 74 nm symbols accounted for. File: 197 lines.

#### Low priority — quality / completeness

- [ ] **SeqCAstNode print/clone macro expansion (audit §I.6)** — the
      macro at `seqc_ast_node.hpp:154` expands to 53×2 stub method
      bodies. Most are mechanical; verify against binary.
- [ ] **WavetableManager\<T\> remaining 14 methods** — template, partially
      done.
- [ ] **smap remaining logic** — ~0x1E6 bytes after alui call (unknowns
      #10).
- [ ] **mergeWaveforms full reconstruction** — 3KB function @0x15e060.
      Blocked on WaveformGenerator complete type (needs merge/interleave/
      grow/getOrCreateWaveform accessible). Currently a documented stub.
- [ ] **writeToNode full reconstruction** — ~23KB function @0x164550.
      4 static boost::regex objects, 6-case node-type switch, frequency/
      phase conversion. Largest unreconstructed CustomFunctions method.
- [ ] **floatEqual @0x2ec050** — trivial utility (double equality with
      epsilon). Used by rrc, drag, and other DSP functions. Currently
      forward-declared only.
- [ ] **AWGCompilerConfig::supportedDeviceTypes documentation** —
      field at config+0x00 is a uint32 bitmask. Values per device type
      need documenting (discovered during checkFunctionSupported analysis).

### 16e. Validation against the real .so ✅ DONE

Completed 2026-04-23.

- [x] Verify struct sizes match binary — added `static_assert` for
      PlayConfig (0x20) and Signal (0x58). Cache::Pointer (0x24 in
      libc++, 0x28 in libstdc++) confirmed as ABI-dependent — not
      assertable. Existing asserts: AsmRegister(8), Value(40),
      DeviceConstants(0x90), MemoryBlock(12), SeqCAstNode(0x18),
      EvalResultValue(0x38), NodeMapItem(0x18), AwgPathPatterns(0x60),
      AwgDeviceProps(0xa0), FrontendLoweringContext(0x50),
      FrontendLoweringState(0x30), plus 12 SeqCAstNode subclass asserts.
- [x] Final marker sweep — 181 total markers across 40 files:
      95 TODO, 29 stub, 22 VERIFY, 14 TBD, 11 APPROXIMATE, 10 not-impl,
      0 FIXME. Top hotspots: custom_functions.cpp (46), waveform_generator
      (29), node_map_data (22).
- [x] Write comparison tests / test key methods — **deferred**. Direct
      testing against the real .so requires a C ABI bridge or Python
      wrapper due to libc++ vs libstdc++ incompatibility. Not practical
      within Phase 16 scope. Future validation should use Python ctypes
      or a thin C shim calling into the .so.

---

## Phase 17 — Hotspot depth pass

Focus on reducing the 181 markers in the top 3 files. Order: node_map_data
(smallest, most mechanical), then waveform_generator structural stubs,
then custom_functions remaining builtins.

### 17a. node_map_data.cpp (22 markers → 0) ✅ DONE

- [x] Reconstruct NodeMap method bodies from binary (12 TODO stubs)
- [x] Verify rodata tables from binary data section (typeTable @0x95ad18 = {2,1,2,2}, sizeTable @0x8fc630 = {2,1,2,2})
- [x] Build verify + sub-phase wrap-up

### 17b. waveform_generator.cpp structural stubs (29 markers → 3) ✅ DONE

- [x] Reconstruct readWave — return type corrected to shared_ptr<WaveformFront>,
      18 call sites updated to extract ->signal
- [x] Reconstruct eval — EvalResults now fully available; make_shared + setValue(VarType(5))
      + waveformFront_ assignment at +0x48
- [x] Reconstruct markerImpl — 1858 bytes (not 4576), creates uniform Signal with constant
      marker; only 2 args (length + markerValue), isMask only affects param names
- [x] Fix interpolateLinear bug (size/sizeof)
- [x] Verify grow error code (0x3d), merge no length validation, circshift min=1, cut offset
- [x] Clean up stale TODO/VERIFY comments (aliasMap_ confirmed empty, rand formula verified,
      marker manipulation NOTE'd)
- [x] Build verify + sub-phase wrap-up
- [ ] 3 remaining markers: readDoubleAmplitude |x|>1.0 check, interpolateLinear formula,
      rrc error 0x64 validation (low priority, deferred)

### 17c. custom_functions.cpp remaining builtins (46 markers → 19) ✅ DONE

- [x] Reconstruct checkPlayMinLength/checkPlayAlignment warning callbacks (0xF5, 0xE7)
- [x] Verify and fix getWaitTime shift direction (left-shift + sar, not right-shift)
- [x] Fix getNodeAddress — return direct->addr_, throw on miss (not return 0)
- [x] Fix getAccessModes — throws out_of_range on miss (not return empty)
- [x] Document getSampleClock fallback (readConst mechanism not fully understood)
- [x] Implement 5 simple 0-arg functions: waitWave (wwvf), waitPlayQueueEmpty (wwvfq),
      sync (addSyncCommand), randomSeed (seedRandom stub), now (suser 0x1c)
- [x] Implement error/info (printF + asmMessage)
- [x] Convert 8 TODO/TBD comments to NOTEs (aliasMap_ empty, NodeMap, etc.)
- [x] Document parseOptionalString and getPlayRate as stubs with addresses
- [x] Build verify + sub-phase wrap-up
- [ ] 19 remaining markers: all blocked on PlayArgs layout (play/playIndexed/4 complex
      wrappers), writeToNode (23KB), mergeWaveforms (3KB), or header field unknowns.
      These require PlayArgs reconstruction as a prerequisite.

---

## Phase 18 — PlayArgs + bulk builtin stubs

### 18a. PlayArgs layout reconstruction

Decode PlayArgs struct (~0x80 bytes, NOT 0x200 — previous offset estimates were
wrong; they were stack locals in play()). Unblocks play(), playIndexed(),
mergeWaveforms(), and the 4 complex play wrappers.

- [x] Disassemble PlayArgs ctor @0x15d600 — 0x80 byte struct with
      shared_ptr<WavetableFront>, std::function, string, uint16_t×2,
      vector<vector<WaveAssignment>>, bool
- [x] Disassemble PlayArgs::parse @0x15d7b0 — pre-scan for VarSubType==2
      (marker), split String/Const vs other, dispatch to parseImplicit/Explicit
- [x] Reconstruct PlayArgs struct in custom_functions.hpp (was opaque_[0x300])
- [x] Implement PlayArgs ctor, dtor (=default), parse(), getMaxSampleLength()
- [x] Rename AWGCompilerConfig::unknown_14 → channelsPerGroup[2]
- [x] Implement parseImplicitChannels @0x16fb30 (~1200B)
- [x] Implement parseExplicitChannels @0x170000 (~1500B)
- [x] Add secureLoadWaveform @0x1711a0 (thin wrapper)
- [x] Implement play() @0x15f090 using real PlayArgs
- [x] Fix mergeWaveforms return type (void → shared_ptr<WaveformFront>)
- [x] Implement playIndexed() @0x160e00 using real PlayArgs
- [ ] Implement addChannelWave @0x170ec0 (~500B)
- [ ] Implement mergeWaveforms() @0x15e060 (~3KB, 7-phase dispatch)
- [ ] Implement 4 complex play wrappers (playAuxWave @0x135610 ~5KB,
      playDIOWave @0x1369f0 ~3.4KB, playWaveDIO @0x137740 784B,
      playWaveZSync @0x137a50 ~3.2KB)
- [x] Build verify + sub-phase wrap-up

### 18c. Consider splitting custom_functions.cpp

After all Phase 18 work is complete, evaluate splitting custom_functions.cpp
(currently ~1850 lines, will grow further) into logical units:
- `custom_functions_play.cpp` — play/playIndexed/PlayArgs methods/wrappers
- `custom_functions_builtins.cpp` — bulk builtin implementations
- `custom_functions.cpp` — ctor, dispatch, utility methods, exceptions

### 18b. Bulk builtin function stubs (~60 return-nullptr stubs)

Many follow a common pattern: checkFunctionSupported → arg validation →
EvalResults(VarType(1)) → single AsmCommands call → return. Batch-disassemble
the smallest ones first for maximum marker reduction.

#### 18b-i. Small builtins (<1KB, mechanical pattern)
- [x] prefetch @0x1351d0 — thin wrapper to play(SubFunc::Default)
- [x] prefetchIndexed @0x135290 — always throws (mask 0x0)
- [x] setWaveDIO @0x14cae0 — always throws (deprecated)
- [x] waitTimestamp @0x1401c0 — st(Reg(0), 0x1b)
- [x] getPRNGValue @0x151a70 — luser(reg, 0x77), returns register as int
- [x] waitOnGrid @0x13d000 — asmWtrigLSPlaceholder (TODO: grid constant from res)
- [x] waitOnSync @0x13d3a0 — st(Reg(0), 0x92)
- [x] resetRTLoggerTimestamp @0x153f90 — st(Reg(0), 0x62/0x6d) device-dependent
- [x] getQAResult @0x14f380 — ld(reg, 0x61), returns register as int
- [x] Build verify + sub-phase wrap-up

#### 18b-ii. Medium builtins (1-2KB)
- [x] setDIO @0x130780 — sdio(reg, highBank), waitState_ protocol
- [x] getDIO @0x131040 — ldio(reg, highBank), waitState_ protocol
- [x] getDIOTriggered @0x131410 — ldiotrig(reg), waitState_ protocol
- [x] setID @0x1334a0 — sid(reg, highBank)
- [x] setTrigger @0x1454c0 — strig(reg)
- [x] getTrigger @0x145ad0 — addi+ltrig+andr, 2 registers
- [x] setInternalTrigger @0x146140 — sinttrig(reg)
- [x] lock @0x14dc70 — asmLockPlaceholder(wf, deviceIndex)
- [x] unlock @0x14e180 — asmUnlockPlaceholder(wf, deviceIndex)
 - [x] getCnt @0x14e8d0 — lcnt + devConst->field_54 range check
 - [x] waitDIOTrigger @0x13d630 — readConst + waitState_ + device-type dispatch
 - [x] getSweeperLength @0x14bca0 — readConst("AWG_USERREG_SWEEP_COUNT0/1") + luser
 - [x] setPrecompClear @0x14c720 — asmSetPrecompFlags(bool)
 - [x] getUserReg @0x14b480 — luser + HDAWG addi/suser/addSyncCommand path
 - [x] playZero @0x1387f0 — asmPlay with hold=false
 - [x] playHold @0x139030 — asmPlay with hold=true
 - [x] waitCntTrigger @0x13e460 — readConst("AWG_CNT_TRIGGERn_INDEX") + asmWtrigLSPlaceholder
 - [x] waitZSyncTrigger @0x13dcf0 — readConst + waitState_=2 + device-type dispatch
- [x] Build verify + sub-phase wrap-up

#### 18b-iii. Large builtins (>2KB)
- [x] getZSyncData @0x1316f0 (~3KB)
- [x] getFeedback @0x132420 (~4KB)
- [x] assignWaveIndex @0x133c40 (~5KB) — partial: boost::regex omitted, field_48_/field_70_/field_4C TODOs
- [x] wait @0x139760 (4640B)
- [x] waitTrigger @0x13abf0 (~2KB)
- [x] waitAnaTrigger @0x13b4b0 (~3KB)
- [x] waitDigTrigger @0x13c110 (~4KB)
- [x] waitDemodOscPhase @0x13eba0 (~3KB)
- [x] waitSineOscPhase @0x13f790 (~2.5KB)
- [x] resetOscPhase @0x1403b0 (~6.5KB)
- [x] setSinePhase @0x141df0 (~4KB)
- [x] incrementSinePhase @0x142da0 (~4KB)
- [x] waitDemodSample @0x143d50 (~5.2KB)
- [x] getAnaTrigger @0x146740 (~3.2KB)
- [x] getDigTrigger @0x147420 (~3.2KB)
- [x] setInt @0x1480d0 (~2.5KB) — delegates to writeToNode (unimplemented)
- [x] setDouble @0x148ac0 (~3.3KB) — delegates to writeToNode (unimplemented)
- [x] generate @0x149940 (~2.8KB)
- [x] setUserReg @0x14a420 (~4KB)
- [x] at @0x14ce30 (~2.5KB)
- [x] waitQAResultTrigger @0x14edc0 (~1.4KB)
- [x] startQAResult @0x14f620 (~2.7KB)
- [x] startQAMonitor @0x1500b0 (~2.1KB)
- [x] executeTableEntry @0x150900 (~2.7KB)
- [x] setPRNGSeed @0x1513e0 (~1.6KB)
- [x] setPRNGRange @0x151ce0 (~2.4KB)
- [x] startQA @0x152690 (~6.2KB)
- [x] configFreqSweep @0x154240 (~5KB)
- [x] setSweepStep @0x155640 (~5KB)
- [x] setOscFreq @0x156a70 (~5KB)
- [x] configureFeedbackProcessing @0x157e60 (~5.6KB)
- [x] Build verify + sub-phase wrap-up
- [x] readConst/readString/readWave/readCvar return type corrected: void → EvalResultValue

---

## Phase 19 — Resources / EvalResults missing implementations (HIGH PRIORITY) ✅ DONE 2026-04-24

Surfaced after Phase 18 wrap-up: the static-archive build hides 295 undefined
zhinst symbols. Two clusters directly block earlier reconstruction work
(custom_functions builtins call `Resources::readConst` and `EvalResults::setValue`
without those symbols actually being defined). Tackle these first because
they would break any executable link of the library.

**Sub-phase status**: 19a ✅, 19b ✅, 19c folded into 20e, 19c-followup ✅,
19d ✅. All 19c-followup investigation prerequisites for Phase 20 cleared.

### 19a. EvalResults out-of-line definitions (6 blocking symbols) ✅ DONE 2026-04-23

Created `src/eval_results.cpp` (270 lines). All 14 methods implemented
(6 required + 8 nice-to-have), full impls — no stubs.

- [x] `EvalResults::EvalResults(VarType)` ctor @0x176bc0
- [x] `~EvalResults()` dtor @0x16f3d0
- [x] `setValue(VarType, Value const&)` @0x211b70
- [x] `setValue(VarType, VarSubType, Value const&)` @0x16bfb0
- [x] `setValue(VarType, int)` @0x15c850
- [x] `addAssembler(AsmList::Asm const&)` @0x15c1b0
- [x] BONUS: copy ctor @0x231c60, getValue @0x211ab0, plus 6 more
      setValue overloads (Value, VarType, double, string, VarType+Value+int,
      VarType+VarSubType+Value+int)
- [x] Build verify — clean, all 6 required symbols now `T`
- [x] Notes update: appended setValue codegen pattern to
      `notes/struct_layouts.md`. Key discovery: `setValue(double)` is
      the only overload that hard-codes VarType (=VarType_Const).

### 19b. Resources missing implementations — direct blockers ✅ DONE 2026-04-23

Appended ~190 lines to `src/resources.cpp` (482 → 671). All 3 blocker
symbols + the GlobalResources TLS statics now defined.

- [x] `Resources::readConst(string, EDirection)` @0x1e7d70 — partial
      (int/bool/double variant paths fully decoded; absW≥3 string path
      has a TODO referencing 1e7e2c..1e7e36 for variant-string layout).
- [x] `Resources::addConst(string, double, VarSubType)` @0x1e7010 —
      full impl. **Discovery**: writes literal **4** to Variable+0x00,
      not VarType_Const=3 — the on-disk record tag for value-bearing
      const entries is 4 (the bare-stub `addConst(name, st)` overload
      probably uses a different value). Worth a follow-up note.
- [x] `Resources::newLabel(string)` @0x1ec6b0 — full impl. Post-
      increments `GlobalResources::labelIndex` (TLS+0x4c) and appends
      decimal value to the (possibly empty → "label") prefix via
      ostringstream.
- [x] `GlobalResources::random`, `regNumber`, `labelIndex` TLS statics
      — all defined.
- [x] `ResourcesException` ctor/dtor — verified defined in resources.cpp.
- [x] Build verify — clean, all 3 blocker symbols now `T`.
- [x] Discovery: readConst error codes are 0xb0=UninitializedVar (miss)
      and 0xaf=TypeMismatchWrite (wrong type). addConst dup-name throws
      0xab=AlreadyDefined.

### 19c. Resources full sweep (all remaining declared methods, ~38)

**Folded into Phase 19d's planning output.** Will be re-organized as
work-packages alongside the other class-by-class undefined symbols
discovered during the full sweep. Items listed here for reference:

- [ ] `createSubScope` @0x1e36a0
- [ ] `updateParent` @0x1e38f0
- [ ] `variableDependsOnVar` @0x1e40e0
- [ ] `variableExists` @0x1e4230
- [ ] `variableExistsInScope` @0x1e4390
- [ ] `getVariableType` @0x1e4460
- [ ] `getVariableSubType` @0x1e4580
- [ ] `addVar` @0x1e46b0
- [ ] `updateVar` @0x1e4c40
- [ ] `checkVar` @0x1e4e20
- [ ] String family: addString×2 @0x1e5020/0x1e54f0, updateString
      @0x1e59d0, readString @0x1e5d70
- [ ] Wave family: addWave×2 @0x1e6020/0x1e64f0, updateWave @0x1e69c0,
      readWave @0x1e6d60
- [ ] Const family: addConst (overload) @0x1e74e0, updateConst
      @0x1e79b0, constIsSet @0x1e8050
- [ ] Cvar family: addCvar×2 @0x1e8180/0x1e8650, updateCvar @0x1e8b20,
      readCvar @0x1e8e80
- [ ] Function family: functionExists @0x1e9110, getFunction @0x1e9370,
      functionExistsInScope @0x1e95d0, getPossibleFunctions @0x1e9740,
      addFunction @0x1e9c10
- [ ] `getRegister` @0x1eba50
- [ ] `setupGlobalState` @0x1ec8f0 (the Resources(name, deviceConstants)
      ctor variant)
- [ ] StaticResources ctor/dtor/getVariable @0x129cb0/0x129db0/0x129e00/0x129e60
- [ ] GlobalResources ctor/dtor @0x12a710/0x12ab40
- [ ] Function::addArguments + addBody bodies (currently empty stubs;
      depend on SeqCAstNode interface)
- [ ] Resources::Variable::~Variable variant string cleanup (currently
      stubbed)

### 19c-followup. Open investigation items from 19a/19b ✅ DONE 2026-04-24

Surfaced during 19a/19b reconstruction; all three resolved before
Phase 20.

- [x] **Variable record-tag mismatch (CRITICAL for 19c)** — RESOLVED.
      Root cause was a wrong VarType enum, not a separate "record-tag"
      mechanism. `str(VarType) @0x247dd0` jump table at 0x95c2a0 yields
      the canonical mapping `Unset=0, Void=1, Var=2, String=3, Const=4,
      Wave=5, Cvar=6` (was `Unset=0, Var=2, Const=3, Cvar=4, String=5,
      Wave=6`). Tag 4 IS VarType_Const under the corrected enum;
      Variable+0x00 IS the VarType directly. Tag mapping verified
      across all add/read overloads. Cascading fix applied to
      resources.hpp, resources.cpp (str, toString, addConst, readConst),
      expression.cpp:177, eval_results.cpp setValue(double) doc, and
      custom_functions.cpp comments. See `notes/struct_layouts.md`
      ("VarType enum — corrected mapping" section).
- [x] **readConst absW≥3 string path** — RESOLVED. Filled in for both
      SSO (bulk 24-byte copy) and long-form (placement-new of
      std::string(ptr, size)) paths in resources.cpp. Variable layout
      reconciled simultaneously: `which_` is at +0x10, variant storage
      at +0x18, with a `subType` (VarSubType) field at +0x08 not
      previously identified. Size 0x58 confirmed by `add r14, 0x58` at
      1e8441. New `variantStorage[16]` field replaces the prior
      embedded-Value approach. `Variable::~Variable` dtor body filled
      in. See `notes/struct_layouts.md` ("Variable layout — corrected"
      section).
- [x] **GlobalResources TLS layout audit** — RESOLVED. Five zhinst
      slots in shared `.so` TLS module block fully mapped: nextID
      (+0x40), Node::idCounter_ (+0x44), GlobalResources::regNumber
      (+0x48), labelIndex (+0x4c), random[313] MT19937-64 state (+0x50,
      2504B). Total zhinst TLS = 0x9D8. `Node::idCounter_` declaration
      added to Node class (replaced free static `node_id_counter`).
      All members are C++11 thread_local statics, NOT a struct
      instance. randomSeed builtin (Phase 17c stub) would write to
      `random[0]` and re-run the seed loop. resources.hpp
      GlobalResources doc-block fully rewritten.

### 19d. Full undefined-symbol sweep + triage ✅ DONE 2026-04-23

Methodology: `nm libzhinst_seqc.a` → set difference (U − T) =
**95 truly-undefined zhinst symbols**. Full audit document at
`reconstructed/notes/undefined_symbols_audit.md` (420 lines). Plan
folded into Phase 20 below; Phase 19c carry-forward Resources items
folded into 20e.

- [x] Run full nm sweep and categorize by class — 95 symbols across
      ~17 classes/free-fn groups; `ErrorMessages::format<...>` is the
      single largest cluster (64/95 = 73%).
- [x] Produce `notes/undefined_symbols_audit.md` with a class-by-class
      inventory + cost estimates.
- [x] Propose phased work-packages in TODO.md — Phase 20 below
      (5 sub-phases 20a–20e).
- [x] User review / approval of the plan — confirmed; Q1
      (ErrorMessages strategy = inline header template) and Q2
      (WavetableManager\<IR\> = out-of-line specialization) resolved.

---

## Phase 20 — Undefined-symbol elimination (executable-link prep)

Outcome of Phase 19d audit (`notes/undefined_symbols_audit.md`).
95 symbols are referenced by some TU but defined nowhere. Five
work-packages, ordered by impact-per-effort: **20a clears 73% of the
gap with one cheap edit**; 20e (Resources sweep) is deferred to last
because of size.

### 20a. Globals + ErrorMessages template body (69 symbols, biggest single win) ✅ DONE 2026-04-24

Highest-impact, lowest-cost package: a single header edit + a few
one-line global definitions clear 73% of the entire gap (69/95) and
unblock 14 caller TUs.

- [x] Inline `ErrorMessages::format<Args...>(ErrorMessageT, Args&&...)`
      body into `error_messages.hpp:457-466` using `boost::format` +
      `std::initializer_list<int>{...}` chained `operator%` (C++17,
      safer than fold-expr). **Strategy confirmed by user (2026-04-23).**
      All 64 instantiations now generated as weak symbols `W` at caller TUs.
- [x] Define `ErrorMessages errMsg;` in `error_messages.cpp:28`
      (BSS @ 0x95de60).
- [x] Define `Prefetch::minIndexedSize` static int in `prefetch.cpp:2294`.
      Init value `0x1000` (4096) recovered from `__cxx_global_var_init`
      at 0xd4361.
- [x] Define `zhinst::zsyncDataPqscDecoder`, `zsyncDataPqscRegister`,
      `constAwgIntegrationTrigger` `const std::string` globals in
      `error_messages.cpp:45-47`. Strings recovered from binary rodata
      via `strings _seqc_compiler.so`. Note: binary uses `L` prefix
      mangling (internal linkage); our header declares `extern` for
      cross-TU access — slight ABI deviation, behaviour unchanged.
- [x] Build verify (`cmake --build .` from `reconstructed/build/`).
      Build clean. Diff against `/tmp/truly_undefined.txt`: 64
      `format<>` template-instantiation symbols + 5 globals all
      resolved (69/95 = 73% gap closure, as planned).
- [x] Sub-phase wrap-up.

**Follow-ups (deferred to executable-link phase):**

- [ ] When an executable target is added to CMakeLists, verify that
      the inlined `format<>` template body produces semantically-
      equivalent output to the binary's explicit instantiations.
      Currently we only verified link-resolution, not runtime output.
      Spot-check at least: `<int>`, `<string>`, `<int,int>`,
      `<string,int,int>`, `<char const*,string,unsigned short,short>`.
- [x] Document the `extern`-vs-`L`-internal-linkage ABI deviation for
      the 5 zsync/integration-trigger globals. Added to existing
      `notes/libcpp_abi.md` under new "ABI deviations" section
      (2026-04-24).

**Estimated sessions:** 1.

### 20b. Trivial Rule-of-Five and missing default ctors (8 symbols) ✅ DONE 2026-04-24

After 20a, the remaining gap is 26. This package clears 8 more cheap
items, leaving 18 for the more involved packages.

- [x] `Immediate(Immediate const&)` copy ctor — added to `value.cpp`
      with per-index switch dispatch (placement-new for string case).
- [x] `Immediate(Immediate&&)` move ctor — leaves `other` in valueless
      state (matches variant-move idiom).
- [x] `Immediate& operator=(Immediate const&)` copy assignment —
      destroys current state then copy-constructs in place.
- [x] `Value()` default ctor — `value.cpp`. Sets `type_=Unspecified,
      which_=0, storage_.i=0`. Any toX() conversion will throw.
- [x] `Node()` default ctor — `node.cpp`. Delegates to 3-arg ctor with
      `(NodeType{0}, 0, -1)`.
- [x] `WaveformFile(const char*)` ctor — `waveform.cpp`. Copy-constructs
      `name`, zero-inits `field18/1C/20`, default-init `data`. Note:
      binary's 0x2a7ff0 was a `std::construct_at<>` specialization
      inlining the body — no dedicated ctor symbol existed.
- [x] `floatEqual(double, double)` @0x2ec050 — added to
      `waveform_generator.cpp`. **Surprise**: despite the name, the
      binary uses `cmpeqsd` (exact IEEE-754 equality), NOT a tolerance
      check. Reconstruction matches.
- [x] Move `logExceptionToClog` from `zhinst::detail` to
      `zhinst::logging::detail` in `log_exception.cpp` — single namespace
      wrap fix.
- [x] Build verify. All 8 symbols verified `T` in archive. Total
      progress: **77/95 (81%) done, 18 remaining.**
- [x] Sub-phase wrap-up.

**Estimated sessions:** 1.

### 20c. Wavetable / Waveform ctors and template instantiations (5 symbols) ✅ DONE 2026-04-24

Same area of the codebase — batched to share disassembly context.

- [x] `WaveformIR::WaveformIR(string const&, WaveformFile::Type, DeviceConstants const&)`
      — body recovered from `allocate_shared<WaveformIR>` dispatcher
      inlining at `0x2aa170-0x2aa20f`. Added to `waveform_ir.cpp`.
- [x] `WaveformFront::WaveformFront(string const&, WaveformFile::Type, DeviceConstants const&)`
      — body recovered from `newWaveformFromFile` dispatcher inlining at
      `0x29b110-0x29b24f`. Added to `waveform_front.cpp`. Also corrected
      the misleading `bitsPerSample=dc[0x40]` comment in
      `waveform_front.hpp:65` (real fields: `seqRegWidth ←
      dc.waveformGranularity`; `frontField1 = 1`).
- [x] Added `WavetableManager<WaveformIR>::insertWaveform` body
      (mirror of `WaveformFront` specialization in
      `wavetable_manager_front.cpp:41` with `WaveformIR` substituted).
      Required forward-declared specialization at top-of-file because
      same-TU calls implicitly used it earlier (C++14 [temp.expl.spec]/6).
- [x] Added explicit-instantiation lines at the bottom of
      `wave_index_tracker.cpp` for both `WaveformFront` and `WaveformIR`.
      Symbols emit as `W` (weak), satisfying U references at link time.
      Also fixed pre-existing typo: template body referenced
      `wp->playIndex` but field is named `waveIndex` at Waveform+0x6C.
- [x] Build clean.
- [x] Sub-phase wrap-up: OVERVIEW.md row added; audit doc WP-C marked
      DONE with detailed findings; running total 82/95 (86%) symbols
      cleared, 13 remaining for WP-D + WP-E.

**Estimated sessions:** 1. **Actual:** 1.

**Follow-ups (deferred to executable-link time):**
- [ ] Verify at executable-link time that the binary's `0x29d000`/`0x29d410`
      `WaveIndexTracker` template-ctor instantiations actually read
      `Waveform+0x6C`. The fix from `playIndex` → `waveIndex` is correct
      against our struct layout, but direct disassembly confirmation of
      the offset within these specific instantiations was not done.
- [x] Document the WaveformIR/WaveformFront 3-arg ctor ABI deviation in
      `notes/libcpp_abi.md` (member-init form vs dispatcher zero-then-
      overwrite — same observable state, different instruction sequence).
- [ ] *Optional, low-priority*: split the two `WaveIndexTracker` explicit-
      instantiation lines from `wave_index_tracker.cpp` into a separate
      `wave_index_tracker_inst.cpp` so disasm-evidence comments stay
      separate from link-machinery.

### 20d. Pimpl wrappers + parser context + NodeMap helpers (7 symbols) ✅ DONE 2026-04-24

These are scattered small methods, each in a different TU. Batched
to clear several caller TUs entirely in one pass.

- [x] Uncomment `AWGAssembler::assembleStringToExpressionsVec`
      wrapper in `awg_assembler.cpp:43-45` with the correct
      `vector<shared_ptr<AsmExpression>>` return type (sret @0x285100).
- [x] Implement `AWGAssemblerImpl::extractComment(string const&)`
      body (find `;` or `//`, return suffix). NOTE: binary inlined
      this twice as `$_1` lambda in `assembleStringToExpressionsVec`
      and never emitted a standalone symbol; we externalize as a real
      method to satisfy the cross-TU U reference.
- [x] Implement `SeqcParserContext::raiseError(string const&)`
      @0x247ae0 and `SeqcParserContext::setSyntaxError()` @0x247cb0
      (mirror `AsmParserContext` equivalents). New TU
      `src/seqc_parser_context.cpp` uses raw byte-offset access since
      class is opaque in header. ABI risk: vtable[6]/+0x30 callback
      offset is libc++-specific.
- [x] Implement `NodeMap::toFrequency(double, double)` @0x1c5630 and
      `NodeMap::toPhase(float)` @0x1c5680. Math fully decoded:
      `toFrequency = (int64_t)(freq * 2^48 / sampleClock)` with
      `2^48 = 281474976710656.0` from rodata `0x956078`;
      `toPhase = roundf(value * 23301.689f)` then 23-bit two's-comp
      wrap (scale `0x46b60b61` from rodata `0x8fd2b4`, wrap shared
      with `pauPoffIwrap` extracted as `wrap23()`).
- [x] Implement `parseOptionalRate(it, end, end, name, bool)` free fn
      @0x163980. Header param naming misleading: third iter is parse
      cursor (typically `PlayArgs::parse()` return). If exactly one
      EvalResultValue remains, calls `getPlayRate()`; else encodes
      "no-rate" sentinel `((strict?1:0)-1)|5` (5 strict, 0xffffffff
      lax). Throws on extra unparsed args.
- [x] Build verify (clean, no warnings, no link errors).
- [x] Sub-phase wrap-up (this commit). 6 of 7 symbols verified `T`/`W`
      in archive; the 3 string/iterator-bearing symbols emit with
      libstdc++ mangling (`__cxx1112basic_string`,
      `__gnu_cxx17__normal_iterator`) instead of libc++
      (`__1::basic_string`, `__1::__wrap_iter`) — internally
      consistent and resolved at link time. Total now 89/95 (94%);
      6 symbols remaining for WP-E.

**Estimated sessions:** 1. **Actual:** 1.

**Follow-ups:**
- [ ] Verify at executable-link time that `SeqcParserContext`
      callback indirect-call works with the actual parser harness.
      The vtable[6]/+0x30 dispatch is libc++-specific; if the harness
      uses libstdc++ `std::function`, adjust the offsets in
      `src/seqc_parser_context.cpp` accordingly.
- [ ] Update the per-symbol audit script
      (`nm | grep " T $sym\$"`) to query both libc++ and libstdc++
      mangled forms, OR document that clean `cmake --build` is the
      authoritative gate (not raw symbol-presence checks against
      libc++ mangled names).

### 20e. util::wave + MemoryAllocator + CsvParser + 19c-carry-forward Resources sweep

Largest package. Combines the remaining 5 trivial undefineds with the
big Resources class sweep (37 method bodies from the 19c carry-forward
list) so that those Resources methods land before any future consumer
re-introduces undefined references.

**Prerequisite — 19c-followup investigation items** ✅ CLEARED 2026-04-24
(VarType enum + Variable layout corrected, TLS layout fully mapped). The
38 Resources method bodies below now have a known-correct struct layout
to write against.

**Prerequisite — fresh undefined-symbol gap check (added Phase 20d wrap-up):**
Before starting WP-E work, re-run the gap analysis querying BOTH
mangling variants (libc++ `__1::basic_string`/`__wrap_iter` AND
libstdc++ `__cxx1112basic_string`/`__gnu_cxx17__normal_iterator`).
The post-20d count may be lower than the audit's 6 if some symbols
are now incidentally satisfied by template instantiations dragged in
during 20a–20d. Update `notes/undefined_symbols_audit.md` with the
true remaining list before starting implementation.

#### 20e-i. Cheap wins: util::wave + MemoryAllocator + CsvParser (5-6 symbols) ✅ DONE 2026-04-24

Risk-ordered first batch — all small, each in its own TU; no
cross-cutting layout assumptions. Land these before tackling the
Resources sweep.

- [x] `zhinst::util::wave::double2awg(double, uint)` @0x299630.
      14-bit signed sample, 2 marker bits in low. Saturate at 1.0,
      scale `8190.0`, lround, pack `(rounded<<2)|(marker&3)`.
- [x] `zhinst::util::wave::double2awg1m(double, uint)` @0x299680.
      15-bit signed sample, 1 marker bit. Scale `16383.0`,
      pack `(rounded<<1)|(marker&1)`.
- [x] `zhinst::util::wave::double2awg16(double)` @0x299700.
      16-bit signed sample, no marker. Scale `32767.0`.
- [x] `zhinst::util::wave::hash(string const&)` @0x299760.
      **Uses `boost::uuids::detail::sha1`** (binary calls
      `process_block` @0x29a270 and `get_digest` @0x29a000).
      Returns `vector<uint32>` of 5 words (160-bit SHA-1).
      Reconstructed via `boost::uuids::detail::sha1::process_bytes`
      + `get_digest(uchar[20])` with manual MSB-first packing.
- [x] `MemoryAllocator::MemoryAllocator(DeviceConstants const*, uint32_t)`.
      Inlined at all binary call sites; we provide a real ctor
      matching the documented 0x70-byte layout.
- [x] `CsvParser::csvFileToWaveform<WaveformIR>` @0x2be830 — STUB.
      Full reconstruction deferred (~7000 bytes; not on hot path).
      Stub throws `std::runtime_error` instead of silently
      producing zero-filled waveforms. Open question #1 resolved:
      binary DOES contain real specializations (not inlined as
      archived Phase 12c claimed).
- [x] Build verify (clean, no warnings).
- [x] Mini-sub-phase wrap-up. **Final undefined-zhinst-symbol
      gap = 0** — `comm -23 <undefined> <defined>` is empty.
      Static archive fully self-contained for the zhinst namespace.

**Estimated sessions:** 1. **Actual:** 1.

**Follow-ups:**
- [ ] If CSV-loaded waveform handling is ever needed by tests,
      reconstruct `CsvParser::csvFileToWaveform<WaveformIR/Front>`
      from disasm at 0x2be830 / 0x2ba8b0 (~7000 bytes each).
- [ ] Verify SHA-1 byte order at executable-link time. Boost may
      change `get_digest` API across versions; current code uses
      the `unsigned char[20]` overload which is universally
      supported, but the bswap-then-store order in the binary
      could be tested against a known SHA-1 vector.

#### 20e-ii. Resources 19c carry-forward sweep (~38 methods)

Bulk Resources-class implementation. Heavier risk because all share
the same Variable/VarType/Scope layout — if the layout has any
remaining surprises, they'll cascade across all 38 methods. Do this
AFTER 20e-i so the cheap wins are safely landed first.

- [ ] **Resources 19c carry-forward** — implement all 37 declared-but-
      undefined Resources methods (createSubScope @0x1e36a0,
      updateParent @0x1e38f0, variableDependsOnVar @0x1e40e0,
      variableExists @0x1e4230, variableExistsInScope @0x1e4390,
      getVariableType @0x1e4460, getVariableSubType @0x1e4580, addVar
      @0x1e46b0, updateVar @0x1e4c40, checkVar @0x1e4e20,
      addString×2 @0x1e5020/0x1e54f0, updateString @0x1e59d0,
      readString @0x1e5d70, addWave×2 @0x1e6020/0x1e64f0, updateWave
      @0x1e69c0, readWave @0x1e6d60, addConst overload @0x1e74e0,
      updateConst @0x1e79b0, constIsSet @0x1e8050, addCvar×2
      @0x1e8180/0x1e8650, updateCvar @0x1e8b20, readCvar @0x1e8e80,
      functionExists @0x1e9110, getFunction @0x1e9370,
      functionExistsInScope @0x1e95d0, getPossibleFunctions @0x1e9740,
      addFunction @0x1e9c10, getRegister @0x1eba50, setupGlobalState
      @0x1ec8f0).
- [ ] **StaticResources** ctor/dtor/getVariable
      @0x129cb0/0x129db0/0x129e00/0x129e60.
- [ ] **GlobalResources** ctor/dtor @0x12a710/0x12ab40.
- [ ] `Function::addArguments` and `addBody` bodies (currently empty
      stubs — depend on SeqCAstNode interface).
- [ ] `Resources::Variable::~Variable` variant string cleanup.
- [ ] Build verify after each group of ~10 methods.
- [ ] Sub-phase wrap-up — final
      `nm libzhinst_seqc.a | awk '/ U _ZN6zhinst/' | sort -u | comm -23 - defined.txt | wc -l`
      should be ≤ 5 (only true cross-library externs).

**Estimated sessions:** 1-2.

---

## Phase 20e-ii wrap-up cleanup (housekeeping)

Added 2026-04-24 during Phase 20e-ii Batch 5a wrap-up audit. All four
items can be executed in a single short session.

- [ ] **OVERVIEW.md narrative compression.** Lines 86-174 contain a long
      Phase 13c/13d/13e/14a narrative that duplicates the per-phase
      summary table at line 178+. Net redundancy ~90 lines. Action:
      keep only the phase table; archive the narrative paragraphs into
      `notes/archive/OVERVIEW_phase_13_14_narrative.md` per AGENTS.md
      ("archive/ holds historical or superseded content").
- [ ] **OVERVIEW.md stale marker-count table (lines 92-100).** Snapshot
      from Phase 16e (133 markers); now drift > 50%. Currently src/
      alone has ~45 raw markers across ~14 files. Action: delete the
      table outright (too-stale-to-maintain; per-file counts churn
      every depth pass and have low long-term value).
- [ ] **Promote 3-way duplication to single source of truth.** The
      simplifyAssign / splitReg / register-rename trio currently appears
      in `notes/unknowns.md` lines 99-106, `TODO.md` lines 1359-1369,
      and (post-cleanup placeholder note) `OVERVIEW.md` Open Questions.
      Per AGENTS.md, TODO.md is the actionable list; notes/ holds
      technical detail. Action: keep entries in TODO.md +
      `notes/optimization_passes.md`; remove the carry-forward block
      from `unknowns.md` and the duplicate listing from OVERVIEW.md.
- [ ] **Variable header refactor: embed `Value value_` at +0x08.** The
      header still uses field names `flagWord`/`which_`/`variantStorage`
      at +0x08/+0x10/+0x18, but disasm proves Variable embeds a full
      `Value` at +0x08 (readString @0x1e5db5 does
      `add rsi, 0x8; call Value::toString` — passing &v+8 as Value*).
      Action: rename the trio to a single `Value value_` member;
      update all read*/add*/update* references in resources.cpp and
      resources_function.cpp; update the layout doc in
      `notes/struct_layouts.md`. Cascade scope: ~7 methods + comments.



- [ ] **Audit-script update — query both mangling variants**
      (added Phase 20d wrap-up). The current per-symbol gap check
      (`nm libzhinst_seqc.a | grep " T $sym\$"`) silently returns 0
      for libstdc++-mangled symbols when queried with libc++ names
      (`__1::basic_string` vs `__cxx1112basic_string`,
      `__wrap_iter` vs `__gnu_cxx17__normal_iterator`). Either:
      (a) extend the script to translate both variants and query
      both, or (b) document that clean `cmake --build` (zero
      warnings, zero unresolved-at-link-time) is the authoritative
      gate, with `nm` only used as a hint. Pick one before the next
      symbol-clearing phase (20e prerequisite).

## Deferred / Low Priority

- [ ] WavetableManager\<T\> remaining methods (14 methods, ~6KB) — template, partially done
- [ ] SeqCAstNode `evaluate()` virtual — signature TBD (vtable slot 8 / +0x40, see seqc_ast_node.hpp:114)
- [ ] Semantic/naming questions from Phase 2-3 (unknowns #23-28, #32, #38) — low value
- [ ] AWGAssemblerImpl internal field purposes (#39-41) — documented, low priority
- [ ] Cache/Prefetch implementation detail unknowns (#61-63, #68-69, #75, #81) — diminishing returns (#64 + #77 already closed in unknowns.md)
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
- [ ] **libc++/libstdc++ ABI unification — design discussion needed**
      (added Phase 20d wrap-up). Currently we paper over the mismatch
      on a per-call-site basis: opaque Pimpl wrappers, raw byte-offset
      access for `SeqcParserContext`, accepting that `std::function`
      vtable layouts differ at +0x30 vs +0x10. A unified strategy
      (build the reconstructed lib with libc++ via clang; or maintain
      a translation header that picks ABI based on `_GLIBCXX_USE_CXX11_ABI`;
      or accept libstdc++ as canonical and document each site where the
      original libc++ ABI bleeds through) deserves its own design
      discussion before any executable-link integration. Until then,
      every cross-ABI assumption (vtable offsets, string layout,
      iterator wrappers) gets a code-comment flag.
