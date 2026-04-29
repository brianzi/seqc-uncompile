# Batch 15 — cached_parser

> **Phase S.1 reconciliation note (2026-04-29)**: This batch contains
> 2 row(s) from the 226-item Phase Q backlog. Per-row triage:
> B1 (mechanical, staged for S.2): 0; B2 (borderline, deferred): 2;
> B3 (already resolved during Phase D/R): 0;
> B4 (wontfix / kept-as-is): 0.
> The authoritative per-row table lives in
> [`SYNTHESIS.md` §6](SYNTHESIS.md#6-low-confidence-and-unsure-parked--reconciled-in-phase-s1).
> Individual rows below are NOT struck through; consult §6 for status.

## 1. Files considered

- `reconstructed/include/zhinst/cached_parser.hpp`
- `reconstructed/src/cached_parser.cpp`

Use-site survey across `reconstructed/src/**.cpp` and
`reconstructed/include/zhinst/**.hpp`, especially `csv_parser.cpp`,
`wavetable_ir.cpp`, `wavetable_front.cpp`, `waveform.hpp`,
`elf_format.md`, plus the binary symbol table via
`nm --demangle _seqc_compiler.so | grep CachedParser`.

Authoritative exclusions per §3 (mangled-symbol presence in
`_seqc_compiler.so`):

- Types: `zhinst::CachedParser`, `zhinst::CachedParser::CacheEntry`,
  `zhinst::CachedParser::CachedFile` — all appear as nested types
  inside mangled symbols (e.g.
  `…<…vector<unsigned int>…, zhinst::CachedParser::CacheEntry>…>`,
  `zhinst::CachedParser::CachedFile::~CachedFile()`).
- Method names: `CachedParser::CachedParser(unsigned long, path const&)`,
  `~CachedParser` (via `~CachedFile`/dtor mangling), `loadCacheIndex`,
  `saveCacheIndex`, `cleanCache`, `removeOldFiles`, `cacheFile`,
  `cacheFileOutdated`, `getCachedFile`, `getHash`,
  `CacheEntry::CacheEntry(string const&, string const&, unsigned long, vector<unsigned int>, bool)`,
  `CacheEntry::CacheEntry(CacheEntry const&)`, `CacheEntry::~CacheEntry`,
  `CacheEntry::operator=`, `CacheEntry::serialize<text_iarchive>`,
  `CacheEntry::serialize<text_oarchive>`, `CachedFile::~CachedFile`.
  All present in `nm` output; excluded.

What remains in scope: parameters of the above methods, data members
of all three nested types, locals only when misleading, and any
non-static layout symbols.

## 2. Overview

| Symbol | Misnomer? | Conf | Justification (≤ 5 words) | Proposal(s) | Status |
|---|---|---|---|---|---|
| `CachedParser::index_` | no | high | the on-disk cache map | keep current (high) | not-misnomer |
| `CachedParser::enabled_` | no | high | gates every method | keep current (high) | not-misnomer |
| `CachedParser::cacheSize_` | no | high | capacity in bytes | keep current (high) | — |
| `CachedParser::currentSize_` | no | high | running used-byte total | keep current (high) | — |
| `CachedParser::cachePath_` | no | high | cache directory path | keep current (high) | — |
| `CachedParser::indexFilePath_` | no | high | `cachePath_ / "index"` | keep current (high) | — |
| `CachedParser::CachedParser::cacheSize` | no | high | initializes `cacheSize_` | keep current (high) | — |
| `CachedParser::CachedParser::cachePath` | no | high | initializes `cachePath_` | keep current (high) | — |
| `CachedParser::cacheFile::name` | no | medium | source-file path stored to `.file_name` | keep current (medium) | — |
| `CachedParser::cacheFile::hash` | no | high | map key, into hash2str filename | keep current (high) | — |
| `CachedParser::cacheFile::sampleFormat` | unsure | low | written verbatim to `.channels` | keep current, `channelMask` | — |
| `CachedParser::cacheFile::markers` | yes | medium | written to `.marker_bits` section | `markerBits`, keep current | coordinated-rename |
| `CachedParser::cacheFile::markerBits` | unsure | low | int, → `.config` decimal text | keep current, `configValue` | — |
| `CachedParser::cacheFile::samples` | no | high | sample doubles → `.data` | keep current (high) | not-misnomer |
| `CachedParser::cacheFile::markerBitsVec` | yes | medium | written to `.marker` section | `markers`, keep current | coordinated-rename |
| `CachedParser::cacheFile::budget` (local) | yes | medium | bytes added to `currentSize_` | `numBytes`, `entrySize` | — |
| `CachedParser::cacheFileOutdated::cachedElfPath` | no | high | reconstructed name; opens as ELF | keep current (high) | not-misnomer |
| `CachedParser::getCachedFile::hash` | no | high | map lookup key | keep current (high) | — |
| `CachedParser::getHash::filePath` | no | high | passed to util::wave::hash | keep current (high) | — |
| `CachedParser::CacheEntry::name_` | no | high | original-source name | keep current (high) | — |
| `CachedParser::CacheEntry::filePath_` | no | high | path of cached `.wave` ELF | keep current (high) | — |
| `CachedParser::CacheEntry::fileSize_` | yes | medium | budget bytes, not file size | `byteSize_`, keep current | — |
| `CachedParser::CacheEntry::timestamp_` | no | high | LRU `time(nullptr)` touch | keep current (high) | — |
| `CachedParser::CacheEntry::hash_` | no | high | map key for this entry | keep current (high) | — |
| `CachedParser::CacheEntry::valid_` | yes | medium | acts as evict-pin / accessed bit | `pinned_`, `accessed_` | — |
| `CachedParser::CacheEntry::CacheEntry::name` (5-arg) | no | high | initializes `name_` | keep current (high) | — |
| `CachedParser::CacheEntry::CacheEntry::filePath` (5-arg) | no | high | initializes `filePath_` | keep current (high) | — |
| `CachedParser::CacheEntry::CacheEntry::fileSize` (5-arg) | yes | medium | bytes-budget, not file size | `byteSize` | — |
| `CachedParser::CacheEntry::CacheEntry::hash` (5-arg) | no | high | initializes `hash_` | keep current (high) | — |
| `CachedParser::CacheEntry::CacheEntry::valid` (5-arg) | yes | medium | matches misnamed field | `pinned`, `accessed` | — |
| `CachedParser::CacheEntry::operator=::other` | no | high | conventional self-assign param | keep current (high) | — |
| `CachedParser::CacheEntry::serialize::ar` | no | high | boost archive convention | keep current (high) | — |
| `CachedParser::CachedFile::channel_` | no | high | `.channels` first int32 low16 | keep current (high) | — |
| `CachedParser::CachedFile::markerBits_` | no | high | bytes from `.marker_bits` | keep current (high) | — |
| `CachedParser::CachedFile::samples_` | no | high | doubles from `.data` | keep current (high) | — |
| `CachedParser::CachedFile::markers_` | no | high | bytes from `.marker` | keep current (high) | — |
| `CachedParser::removeOldFiles::keptPinned` (local) | no | medium | named after pin semantic | keep current (medium) | — |
| `CachedParser::removeOldFiles::entries` (local) | no | high | local copy for sort | keep current (high) | — |
| `CachedParser::cacheFile::elfw` (local) | no | high | the `ElfWriter` instance | keep current (high) | — |
| `CachedParser::cacheFile::filePath` / `filePathStr` (locals) | no | high | the cached-file path | keep current (high) | — |
| `CachedParser::cacheFile::configStr` (local) | no | medium | text of `.config` section | keep current (medium) | — |
| `CachedParser::loadCacheIndex::ifs` / `ia` / `total` (locals) | no | high | conventional | keep current (high) | — |
| `CachedParser::saveCacheIndex::ofs` / `oa` (locals) | no | high | conventional | keep current (high) | — |
| `CachedParser::getCachedFile::result` / `it` / `entry` (locals) | no | high | conventional | keep current (high) | — |
| `CachedParser::getCachedFile::loadBytes` / `loadDoubles` (locals) | no | high | reconstructed lambdas | keep current (high) | — |

## 3. Detailed findings

### CachedParser::index_  [no / high / not-misnomer]

Evidence:
- `cached_parser.hpp:201` `std::map<std::vector<unsigned int>, CacheEntry> index_;  // +0x00 (0x30 bytes on libc++)`
- `cached_parser.hpp:13-16` header notes the map is the boost-serialized
  payload: `boost::archive::detail::iserializer<text_iarchive,
  std::map<vector<uint>, CachedParser::CacheEntry>>`.
- `cached_parser.cpp:113-130` `loadCacheIndex` deserializes into
  `index_` and walks it to recompute `currentSize_`.
- `cached_parser.cpp:177` `oa << index_;` — serialized whole on save.
- `cached_parser.cpp:194` `index_.clear();` in `cleanCache`.
- `cached_parser.cpp:434-441` `getCachedFile` does `index_.find(hash)`
  / `index_.erase(it)`.
- The on-disk file pointed to by `indexFilePath_` is literally named
  `"index"` (`cached_parser.cpp:81`).

Interpretation:
- The field is the in-memory representation of the on-disk file
  named `"index"`. Both producer (`saveCacheIndex`) and consumer
  (`loadCacheIndex`) treat it as the index of cached files.

Judgement:
- The name matches both the on-disk artefact and the in-code role;
  not a misnomer.

Proposals:
- keep current  (high)

Locations consulted:
- declared: include/zhinst/cached_parser.hpp:201
- used:     src/cached_parser.cpp:76,113-138,164-177,194,229-247,348,434-441

### CachedParser::enabled_  [no / high / not-misnomer]

Evidence:
- `cached_parser.hpp:203` `bool enabled_;  // +0x18`
- `cached_parser.cpp:77` `, enabled_(cacheSize != 0)` — set true iff
  the user requested non-zero capacity.
- `cached_parser.cpp:83-86` `if (enabled_) { create_directories…;
  loadCacheIndex(); }` — gates filesystem setup.
- `cached_parser.cpp:110, 163, 291, 431, 493` early-return
  `if (!enabled_) return …` in every public/private method except
  the dtor and trivial accessors.
- `cached_parser.cpp:180` `enabled_ = false;` in `saveCacheIndex`
  catch — explicitly described in the surrounding comment as the
  "sticky-disable mechanism".

Interpretation:
- A single boolean that gates whether the cache is operational; set
  on construction, cleared on persistent I/O failure. Every API
  entry-point checks it.

Judgement:
- Name matches semantics across construction, every method, and the
  documented disable path; not a misnomer.

Proposals:
- keep current  (high)

Locations consulted:
- declared: include/zhinst/cached_parser.hpp:203
- used:     src/cached_parser.cpp:77,83,110,163,180,291,431,493

### CachedParser::cacheSize_ / currentSize_  [no / high / —]

Evidence:
- `cached_parser.hpp:205-206` documented as "cache capacity in BYTES
  (0 = disabled)" and "current total size of cached files in BYTES".
- `cached_parser.cpp:78-79` initializers: capacity ← ctor arg, current
  ← 0.
- `cached_parser.cpp:130` `currentSize_ = total;` summed from
  `entry.fileSize_`.
- `cached_parser.cpp:137,239,246,295,300` arithmetic always treats
  both as same-unit bytes (`currentSize_ += budget`,
  `currentSize_ -= entry.fileSize_`, `currentSize_ > cacheSize_`).
- Cross-batch: `Cache::size_` (batch 36) is similarly named for
  total cache memory.

Interpretation:
- Two paired counters in identical units (bytes), one fixed (cap),
  one running. Both consistently used as such.

Judgement:
- Names match; not misnomers. Unit ("bytes") is documented in the
  header rather than the name, but no contradicting use exists.

Proposals:
- keep current  (high) — both

Locations consulted:
- declared: include/zhinst/cached_parser.hpp:205-206
- used:     src/cached_parser.cpp:78,79,128-138,239-246,295-300

### CachedParser::cachePath_ / indexFilePath_  [no / high / —]

Evidence:
- `cached_parser.hpp:207-208` "boost::filesystem::path", with comment
  "= cachePath_ / "index"".
- `cached_parser.cpp:80-81` initializers: `cachePath_(cachePath)`,
  `indexFilePath_(cachePath / "index")`.
- `cached_parser.cpp:84,196,197` `create_directories(cachePath_)`,
  `remove_all(cachePath_)`, `create_directory(cachePath_)`.
- `cached_parser.cpp:113,167` `ifstream/ofstream(indexFilePath_.string()…)`.
- `cached_parser.cpp:310` `cachePath_ / oss.str()` to build per-entry
  filename.

Interpretation:
- One is the cache directory; the other is the path of the index file
  inside it. Both names accurately describe the values.

Judgement:
- Not misnomers.

Proposals:
- keep current  (high) — both

Locations consulted:
- declared: include/zhinst/cached_parser.hpp:207-208
- used:     src/cached_parser.cpp:80-84,113,167,196,197,310

### CachedParser::cacheFile::markers / markerBitsVec  [yes / medium / coordinated-rename]

Evidence:
- `cached_parser.hpp:165-171` declaration order:
  `name, hash, sampleFormat, markers, markerBits, samples, markerBitsVec`.
- `cached_parser.cpp:286-289` definition matches.
- `cached_parser.cpp:326-328`
  ```
  // ".marker_bits" — from `markers` parameter (not markerBitsVec!)
  elfw.addData(reinterpret_cast<const char*>(markers.data()),
               markers.size(), std::string(".marker_bits"));
  ```
- `cached_parser.cpp:334-336`
  ```
  // ".marker" — from `markerBitsVec` parameter (not markers!)
  elfw.addData(reinterpret_cast<const char*>(markerBitsVec.data()),
               markerBitsVec.size(), std::string(".marker"));
  ```
- Symmetric reads in `getCachedFile`:
  - `cached_parser.cpp:475` `result.markerBits_ = loadBytes(".marker_bits");`
  - `cached_parser.cpp:477` `result.markers_    = loadBytes(".marker");`
- ELF section table in `notes/elf_format.md:115-118` confirms the
  two distinct sections `.marker_bits` and `.marker`, both consumed
  by `getCachedFile`.

Interpretation:
- Section `.marker_bits` is *fed by* the parameter named `markers`,
  while section `.marker` is *fed by* the parameter named
  `markerBitsVec`. Both reconstruction comments explicitly flag the
  swap ("not markerBitsVec!", "not markers!"), and the receiving
  fields in `CachedFile` (`markerBits_`, `markers_`) carry the same
  names as the sections, which is the *opposite* of the writer
  parameters.
- The naming inconsistency therefore lives in the `cacheFile`
  parameter list, not in the section names or the `CachedFile`
  fields. The two parameter names are swapped relative to the data
  they carry into the cache.

Judgement:
- Yes — the parameter name `markers` corresponds to marker-bits
  payload, and `markerBitsVec` corresponds to the marker payload.
  Swapping them would restore consistency with `.marker_bits` /
  `.marker` and with `CachedFile::markerBits_` / `markers_`. This
  must be done as a coordinated pair so that callers in
  `csv_parser.cpp` (currently unreconstructed) update both
  arguments in lockstep.

Proposals (coordinated):
- swap names: `markers` → `markerBits`, `markerBitsVec` → `markers`
  (medium)
- keep current names but document the swap loudly  (low)

Cross-reference:
- `CachedFile::markerBits_`, `CachedFile::markers_` (this batch,
  routinely fine) anchor the correct direction.
- Future batch on `csv_parser.cpp` `csvFileToWaveform` must follow
  the same rename.

Locations consulted:
- declared: include/zhinst/cached_parser.hpp:165-171
- used:     src/cached_parser.cpp:282-289,326-336,475-477
- related:  notes/elf_format.md:115-118

### CachedParser::cacheFile::sampleFormat  [unsure / low / —]

Evidence:
- `cached_parser.hpp:167` parameter `int sampleFormat`.
- `cached_parser.cpp:323-324`
  ```
  elfw.addData(reinterpret_cast<const char*>(&sampleFormat),
               sizeof(sampleFormat), std::string(".channels"));
  ```
- `cached_parser.cpp:455-460` `getCachedFile` reads `.channels`,
  takes the first 4 bytes, casts to `uint16_t`, stores in
  `result.channel_`.
- `cached_parser.hpp:137` `CachedFile::channel_` documented as
  "first 16 bits of .channels section".

Interpretation:
- The value written is a 4-byte int, only the low 16 bits of which
  are read back as `channel_`. Either the full int encodes
  channel-format bits (only some of which are consumed), or the
  parameter is genuinely the channel count/mask and the name
  `sampleFormat` is misleading.
- No call site for `cacheFile` is yet reconstructed to confirm
  intent (`csv_parser.cpp:470` is only a comment).

Judgement:
- Unsure — section name (`.channels`) and downstream field
  (`channel_`) both point to a channel concept, not a sample format,
  but no caller is available to confirm which.

Proposals:
- keep current     (medium)
- `channelMask`    (low)
- `channelInfo`    (low)

Locations consulted:
- declared: include/zhinst/cached_parser.hpp:167
- used:     src/cached_parser.cpp:323-324,455-460
- related:  cached_parser.hpp:137

### CachedParser::cacheFile::markerBits  [unsure / low / —]

Evidence:
- `cached_parser.hpp:169` parameter `int markerBits`.
- `cached_parser.cpp:339-340`
  ```
  std::string configStr = std::to_string(markerBits);
  elfw.addData(configStr.data(), configStr.size(), std::string(".config"));
  ```
- The decoded value is never read back by `getCachedFile`
  (`.config` is not in the read-back set at lines 455-477).

Interpretation:
- An int written as decimal text into `.config`. The name suggests
  a count of marker bits, but the section is named `.config`, and
  the value is never read. The use is too thin to either confirm or
  refute the name.

Judgement:
- Unsure.

Proposals:
- keep current   (medium)
- `configValue`  (low)
- `markerCount`  (low)

Locations consulted:
- declared: include/zhinst/cached_parser.hpp:169
- used:     src/cached_parser.cpp:339-340

### CachedParser::cacheFile::budget (local)  [yes / medium / —]

Evidence:
- `cached_parser.cpp:294`
  `std::size_t budget = samples.size() * sizeof(double) / 4;`
  (= `samples.size() * 2`, i.e. *bytes per sample × samples / 4*).
- `cached_parser.cpp:295` `currentSize_ += budget;`
- `cached_parser.cpp:300` `currentSize_ -= budget;`
- `cached_parser.cpp:347`
  `CacheEntry entry(name, filePathStr, budget, hash, /*valid=*/true);`
  — `budget` becomes `CacheEntry::fileSize_`.
- Header comment `cached_parser.hpp:206`: `currentSize_` is in BYTES;
  `cacheSize_` likewise.
- `cached_parser.cpp:346` author comment:
  "Budget is used as fileSize (not boost::filesystem::file_size)".

Interpretation:
- The variable holds a byte count: it is added to and subtracted
  from `currentSize_` (bytes) and stored as `fileSize_` (bytes). The
  word "budget" is a vague accounting term; "byte cost" or
  "entry size" would be more precise.

Judgement:
- Yes — `budget` does not describe what the value is; it is the
  per-entry byte cost.

Proposals:
- `numBytes`   (medium)
- `byteCost`   (low)
- `entrySize`  (low)
- keep current (low)

Cross-reference:
- Same byte/sample confusion observed in batch 36 (`Cache::numSamples`).

Locations consulted:
- declared: src/cached_parser.cpp:294
- used:     src/cached_parser.cpp:294,295,300,347

### CachedParser::cacheFileOutdated::cachedElfPath  [no / high / not-misnomer]

Evidence:
- `cached_parser.hpp:175` declaration uses `name`:
  `bool cacheFileOutdated(const std::string& name) const;`
- `cached_parser.cpp:371` definition renames to `cachedElfPath`
  with comment at lines 357-370 explaining: "despite the parameter
  being named `name`, this function does NOT search index_. It
  opens `name` as a path to a cached ELF artifact…"
- `cached_parser.cpp:375,390` it is passed to
  `ElfReader reader(cachedElfPath)` and
  `last_write_time(cachedElfPath)`.
- `cached_parser.cpp:439` lone caller passes `entry.filePath_` (the
  on-disk cached `.wave` ELF path).

Interpretation:
- The header's `name` is the original (and likely binary-faithful)
  parameter name, which the implementation file already overrode to
  the more descriptive `cachedElfPath`. Function uses it as a path,
  not as a logical name.
- The reconstruction's local rename (`cachedElfPath`) is a positive
  improvement and matches every use site. The header's `name` is
  the misleading one.

Judgement:
- The reconstructed local name is correct; the header parameter name
  `name` is misleading. Recording both: the cpp-side parameter is
  not a misnomer; the hpp-side declaration is, and they should be
  unified.

Proposals:
- header: rename `name` → `cachedElfPath`  (high)
- keep cpp `cachedElfPath`  (high)

Locations consulted:
- declared: include/zhinst/cached_parser.hpp:175
- defined:  src/cached_parser.cpp:371-398
- used:     src/cached_parser.cpp:439

### CachedParser::CacheEntry::fileSize_  [yes / medium / —]

Evidence:
- `cached_parser.hpp:124` `std::size_t fileSize_;  // +0x30`
- `cached_parser.cpp:347`
  `CacheEntry entry(name, filePathStr, budget, hash, /*valid=*/true);`
  — initialized from `budget`, which is
  `samples.size() * sizeof(double) / 4` (see preceding finding), not
  `boost::filesystem::file_size`.
- `cached_parser.cpp:128,246` arithmetic:
  `total += kv.second.fileSize_;`,
  `currentSize_ -= entry.fileSize_;` — accumulated into
  `currentSize_`, which is in BYTES per the header.
- `cached_parser.cpp:346` author comment: "Budget is used as fileSize
  (not boost::filesystem::file_size)".

Interpretation:
- The field stores a byte budget derived from sample count, not the
  on-disk size of the cached `.wave` ELF (which would be larger,
  ELF headers + section overhead included). The name `fileSize_`
  invites readers to think of disk size.

Judgement:
- Yes — name implies disk-file size; value is an accounting byte
  budget (sample-bytes / 4).

Proposals:
- `byteSize_`    (medium)
- `entryBytes_`  (low)
- `cacheBytes_`  (low)
- keep current   (low)

Cross-reference:
- 5-arg ctor parameter `CacheEntry::CacheEntry::fileSize` — same
  finding, must rename together (coordinated).

Locations consulted:
- declared: include/zhinst/cached_parser.hpp:124
- used:     src/cached_parser.cpp:41,58,128,246,347; cached_parser.hpp:97

### CachedParser::CacheEntry::valid_ (and ctor param `valid`)  [yes / medium / —]

Evidence:
- `cached_parser.hpp:127` `bool valid_;  // +0x58`
- Header comment at `cached_parser.hpp:110-111`:
  "valid_ is NOT serialized — it is reconstructed at load time
  (set to false by default, set to true on cache hit in
  getCachedFile)."
- `cached_parser.cpp:240-244` in `removeOldFiles`:
  ```
  if (entry.valid_) {
      // Pinned entry — stop evicting (binary sets r15b=1 here).
      keptPinned = true;
      break;
  }
  ```
- `cached_parser.cpp:347` `CacheEntry entry(name, …, /*valid=*/true);`
  i.e. fresh entries written by `cacheFile` start `valid_ = true`,
  which means "do not evict" (pinned).
- `cached_parser.cpp:447` `entry.valid_ = true;` — set on every
  cache hit in `getCachedFile`. Combined with the
  serializer-skip-this-field design, this acts as an LRU/pin bit.
- Author comment at `cached_parser.cpp:222-223`:
  "valid_ == true means 'do not evict' (pinned). The flag was named
  `valid_` in the header; semantically it acts as a pin/lock bit."

Interpretation:
- The field is set true on access (load-time hits and freshly cached
  entries) and read in eviction as "do not evict". A boolean named
  "valid" normally denotes data integrity; here it denotes
  pin-status / recently-accessed.

Judgement:
- Yes — `valid_` is a misleading name for what is actually an
  evict-pin / accessed bit.

Proposals:
- `pinned_`     (medium)
- `accessed_`   (medium)
- `inUse_`      (low)
- keep current  (low)

Cross-reference:
- Same finding for the 5-arg `CacheEntry::CacheEntry::valid`
  parameter (same row in §2): rename together.

Locations consulted:
- declared: include/zhinst/cached_parser.hpp:127, also hpp:96,99
- used:     src/cached_parser.cpp:44,61,240,347,447;
  cached_parser.cpp:222-223 (author comment)

## 4. Symbols inspected and judged routinely fine

- `CachedParser::CachedParser::cacheSize`, `…::cachePath` —
  ctor parameters initializing identically-named fields; names
  match.
- `CachedParser::getCachedFile::hash` — vector<uint> used only as
  the `index_.find(hash)` key; matches name.
- `CachedParser::getHash::filePath` — passed straight into
  `util::wave::hash(filePath)`; matches name.
- `CachedParser::cacheFile::name` — written verbatim into the
  `.file_name` ELF section, used to identify the original source.
- `CachedParser::cacheFile::hash` — used as map key and in the
  generated filename via `util::wave::hash2str(hash)`; matches name.
- `CachedParser::cacheFile::samples` — `vector<double>` of audio
  samples written to `.data` section.
- `CacheEntry::name_` — original-source name; mirrors the
  `.file_name` section.
- `CacheEntry::filePath_` — path of the cached `.wave` ELF; passed
  to `boost::filesystem::remove`, `ElfReader`, etc.
- `CacheEntry::timestamp_` — `std::time(nullptr)` in ctor; refreshed
  on cache hit (`getCachedFile`); used as the sort key for LRU
  eviction. Matches name.
- `CacheEntry::hash_` — copy of map key; matches name.
- `CacheEntry::CacheEntry::name`, `…::filePath`, `…::hash` —
  initialize like-named fields.
- `CacheEntry::operator=::other` — conventional self-assignment
  parameter.
- `CacheEntry::serialize::ar` — boost-archive convention.
- `CachedFile::channel_` — `uint16_t` derived from first int32 of
  `.channels`; matches usage.
- `CachedFile::markerBits_` — `vector<uint8_t>` populated from the
  `.marker_bits` section in `getCachedFile`.
- `CachedFile::samples_` — `vector<double>` from `.data`; the
  caller uses `samples_.empty()` as the cache-miss indicator.
- `CachedFile::markers_` — `vector<uint8_t>` from `.marker`.
- `removeOldFiles::keptPinned` — local bool flag echoing the
  `valid_` pin semantic; explicitly named for the eviction-stop
  case.
- `removeOldFiles::entries` — local `vector<CacheEntry>` for sort.
- `cacheFile::elfw` — local `ElfWriter` instance.
- `cacheFile::filePath`, `cacheFile::filePathStr` — locals
  composing the cache filename via `cachePath_ / "csv<hash>.wave"`.
- `cacheFile::configStr` — `std::to_string(markerBits)` for the
  `.config` section.
- `loadCacheIndex::ifs`, `::ia`, `::total` — short-named locals in
  conventional roles.
- `saveCacheIndex::ofs`, `::oa` — same.
- `getCachedFile::result`, `::it`, `::entry` — conventional.
- `getCachedFile::loadBytes`, `::loadDoubles` — reconstructed
  helper lambdas; both names accurately describe the body.
- `getCachedFile::ch`, `::s`, `::n`, `::v` — small locals in
  the lambdas; conventional and limited in scope.

## 5. Coverage

- **Fully covered:** every in-scope symbol in `cached_parser.hpp`
  and `cached_parser.cpp` — all data members of `CachedParser`,
  `CacheEntry`, `CachedFile`; all method parameters; locals where
  they had non-trivial roles.
- **Deferred:** none.
- **Not covered (out of scope per RULES §2/§3):**
  - Type names `CachedParser`, `CacheEntry`, `CachedFile` —
    appear in mangled symbols (e.g.
    `zhinst::CachedParser::CacheEntry`,
    `zhinst::CachedParser::CachedFile::~CachedFile()`). Excluded.
  - All method names listed in §1 — present in the binary's
    mangled symbol table. Excluded.
  - Template parameter `Archive` of `CacheEntry::serialize` — out
    of scope per §2.
  - Third-party / boost types referenced in signatures
    (`boost::filesystem::path`, `boost::archive::text_iarchive`,
    etc.) — out of scope per §2.
- **Cross-batch arbitration pending:** none.
  Coordinated renames flagged within this batch:
  - `cacheFile::markers` ↔ `cacheFile::markerBitsVec` (swap pair).
  - `CacheEntry::valid_` ↔ `CacheEntry::CacheEntry::valid`.
  - `CacheEntry::fileSize_` ↔ `CacheEntry::CacheEntry::fileSize`.
  - `cacheFileOutdated::name` (header) ↔ `cacheFileOutdated::cachedElfPath`
    (cpp) — header should adopt cpp's name.
- **Status:** complete.
