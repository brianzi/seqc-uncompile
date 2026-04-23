# Archived: OVERVIEW.md Phase 13/14 narrative + Phase 16e marker table

Moved from `OVERVIEW.md` lines 94-178 on 2026-04-24 during Phase 20e-ii
Batch 5a wrap-up. Content fully duplicated by the per-phase summary table
that begins at the (post-cleanup) `### Completed phases (summary)` header.
The marker counts were a 16e snapshot and had drifted >50% by the time
of archival.

---

### Per-file marker counts (files with >5 markers, from 16e sweep)

| File | TODO | Stub | VERIFY | Other | Total |
|------|------|------|--------|-------|-------|
| `src/custom_functions.cpp` | 12 | 0 | 0 | 0 | **12** |
| `src/waveform_generator.cpp` | 1 | 0 | 0 | 2 | **3** |
| `src/node_map_data.cpp` | 0 | 0 | 0 | 0 | **0** ✅ |
| `include/zhinst/custom_functions.hpp` | 3 | 0 | 0 | 6 | 9 |
| `include/zhinst/seqc_ast_node.hpp` | 5 | 1 | 1 | 2 | 9 |
| Other files (scattered) | 29 | 6 | 13 | 18 | 66 |
| **Total** | **~67** | **~16** | **~17** | **~33** | **~133** |

Phase 13a resolved 106 stubs → 1 remaining (SeqCValue::clone has a
shallow-copy caveat for long strings; needs full variant interface).

Phase 13c resolved infrastructure + 6 representative DSP functions in
WaveformGenerator: call(), getOrCreateWaveform(), createDummyWaveform(),
readInt/readDouble/readDoubleAmplitude/readPositiveInt, plus
zeros/ones/rect/scale/add/gauss/sinc/drag/blackman/hamming/hann/mask/marker/rrc.
Subsequently, 18 more DSP functions were reconstructed:
rand/randomGauss/randomUniform/lfsrGaloisMarker (random generators using
TLS GlobalResources::random mt19937_64),
vect/placeholder/join/interleave/multiply/cut/flip/circshift/merge/grow
(composition operators), filter (FIR/IIR), plus expanded stubs for
readWave (WavetableFront integration), markerImpl, and interpolateLinear.
sin/cos/sinc/ramp/sawtooth/triangle/chirp were reconstructed from full
disassembly: trig family uses Signal::append loop with 2*nPeriods*PI*i/length
formula; ramp uses linear interpolation; sawtooth/triangle delegate to
genericTriangle with riseRatio 1.0/0.5 respectively; chirp implements a
linear frequency sweep with zero-padding to 16-sample alignment.
eval() body is documented but throws
"blocked on EvalResults" pending Phase 15a layout work. No DSP
stubs remain — all waveform generator functions are now implemented.
4 return types fixed (call/eval/getOrCreateWaveform/createDummyWaveform
return shared_ptr<WaveformFront>/shared_ptr<EvalResults>, not Signal);
field_50_ → createdNames_; aliasMap_ confirmed empty; field_B0_ deferred.

Phase 13d resolved all 7 CachedParser methods + 2 BONUS methods
(cleanCache, cacheFileOutdated) + the CacheEntry::serialize template.
loadCacheIndex/saveCacheIndex are reconstructed with their boost-archive
operations as documented placeholders (avoid heavy boost::archive include
churn). cacheFile body is a documented skeleton (ElfWriter calls); the
other methods are full implementations. Layout corrections: CachedFile
has NO `found_` bool — proper layout is `uint16_t channel_; vector<uint8_t>
markerBits_; vector<double> samples_; vector<uint8_t> markers_`. Map size
fields cacheSize_/currentSize_ are BYTES not entry counts. valid_ flag
acts as in-use pin (set true on read access, prevents eviction). New
header: `elf_reader.hpp` (minimal forward decl for ElfReader/ElfSection,
since cacheFileOutdated and getCachedFile both use it). **Phase 14d
upgraded `elf_reader.hpp` to a full reconstruction (0x98 bytes, real
private `ELFIO::elfio` base, ElfException class, `sectionAsString()`
helper) and added `src/elf_reader.cpp` with all 5 methods +
ElfException ctor/dtor/what; cached_parser.cpp call sites switched
from the fictional `getDataAsString()` to `sectionAsString()`.**

Phase 13e re-audited CustomFunctions layout end-to-end. **HISTORICAL NOTE
to prevent regression**: an earlier session misattributed the dtor at
**0x1306c1** (which is in `pybind11::detail::internals` or similar, NOT
CustomFunctions) and derived several wrong field types from it. The real
`CustomFunctions::~CustomFunctions` is at **0x127c90** and the real ctor
is at **0x12bcf0**. Corrections applied: +0xF8 is `unique_ptr<NodeMap>`
(not inline `map<string,NodeMapItem>`); +0x168 is `vector<T>` with 8B
trivially-destructible T (not `unordered_set<string>`, T still TBD).
MathCompiler internal layout resolved (#102): two `std::map`s at +0x00
(single-arg) and +0x18 (multi-arg). DirectAddrNodeMapData has
`uint32_t addr_` at +0x08 (#104). VirtAddrNodeMapData fields deferred
to Phase 14a.

Phase 14a fully reconstructed MathCompiler (33 zhinst:: methods): the ctor
at 0x1c2250 populates both maps with 23 single-arg + 5 multi-arg `std::bind`
emplaces; `call(name, args)` and `functionExists(name, argCount, strict)`
have correct dispatch logic with `MathCompilerException` throws via
ErrorMessages codes 136 (FuncSingleArg), 137 (FuncExactly2Args), 216
(UnknownFunction). Single-arg wrappers map directly to `<cmath>` functions.
**HISTORICAL CORRECTION (cascading from 13e)**: the Phase 13e claim that
field_168 was a `vector<T>` was itself wrong — the dtor at 127cf2 has a
node-walk loop at 127d40-127d70 reached via the `jne` at 127cf0 that the
13e analysis missed. Real type is `std::unordered_set<std::string>` (40B
container, 40B node = 16B header + 24B std::string), confirmed by 1.0f
max_load_factor at +0x188 in ctor and string-dtor pattern in node loop.
Original Phase 11d classification was correct after all. Also resolved
VirtAddrNodeMapData (0x38): vptr + `std::string name_` at +0x08 +
`std::vector<int32_t> addresses_` at +0x20 — confirmed by copy ctor
calling `vector<int>::__throw_length_error` and getJson writing the "name"
key from the string field. Both #101 and #104 fully closed.
