# Prefetch Scheduling {#notes_prefetch_scheduling}

The **prefetcher** is the sub-pipeline that decides *when* each
waveform's sample data must be loaded into the sequencer's
on-device cache so that every `playWave` instruction finds its
samples already resident at execution time.  It is step 9 of
the top-level compiler pipeline (see \ref notes_pipeline) and
the bridge between the program's logical waveform references
and the physical wave-memory layout written into the
`.wavemem` / `.wf_<name>` ELF sections (see \ref
notes_elf_format).

The work is owned by the `Prefetch` class
(`reconstructed/include/zhinst/codegen/prefetch.hpp`) and
driven by `Compiler::runPrefetcher()` against the lowered
waveform-IR AST produced by the earlier passes.

[TOC]

## Phases

`Compiler::runPrefetcher()` invokes `Prefetch` in three ordered
phases.  Each phase walks the AST and stores its decisions in
`Prefetch::nodeStates_` (a `Node → PrefetcherNodeState`
map), which the next phase consumes.

### 1. `preparePlays`

Three back-to-back AST walks that classify nodes and gather
the sizing information the placement step needs:

- **`prepareTree`** descends the AST, classifies each node
  (play / load / branch / loop / cwvf / wvfs / …), inserts
  *load placeholders* in front of consuming plays so later
  phases have a stable rewrite site, and creates the
  `PrefetcherNodeState` entry for every node it visits.
- **`countBranches`** records, per node, how many branches
  of the surrounding control flow reach it.  `maxBranches_`
  (initialised to `1` and grown as branches are walked) is
  the divisor used later when partitioning cache capacity
  between competing branches.
- **`definePlaySize`** computes the number of cache pages each
  load occupies (`pagesNeeded` on the node state) and assigns
  the per-load registers — one slot for the Hirzel emit path
  and one for the Cervino emit path (see \ref notes_cervino_vs_hirzel
  for the family split).

### 2. `placeLoads` (with optional `determineFixedWaves`)

Decides *where* in the program to emit each load.  This is the
heart of the prefetcher: it merges duplicate loads, hoists
loads out of loops where profitable, splits plays that cross a
cache line, and assigns each load a `Cache::Pointer` that
reserves its slot in the on-device cache model.

When the active device uses **cache type 1**,
`runPrefetcher` first calls `determineFixedWaves` to *pin*
specific waveforms into the cache before `placeLoads` runs;
those waveforms cannot be evicted by the placement algorithm.

`placeLoads` is also where the cache size is allocated.  It
constructs the root `Cache` from
`devConst.waveformMemorySize`, then walks the AST with the
recursive `allocate(node, cache)` driver.  At branch and loop
nodes `allocate` creates a *scoped child cache* via
`Cache::getScope()` so the branches' allocations do not
permanently consume capacity in the parent scope.

Within `allocate`, each load goes through one of three paths:

1. **Already cached, still valid** — the node carries a
   `Cache::Pointer` whose `state ≥ 2`; copy it forward.
2. **Reusable from earlier** — `Cache::stillInMemory` reports
   the load is still available; call `Cache::reuse`, then
   fold the load into the existing one with `mergeLoads`.
3. **Fresh allocation** — call `Cache::allocate(waveIR,
   pageSize, …)`; on failure raise `WaveNotFittingCache`
   (multi-core HDAWG) or `WaveNotFittingCacheGapless` (every
   other device family).

After every load is placed, `allocate` calls `Cache::play()`
on the load's cache pointer to advance its lifetime so the
next pass can see what is still live.

### 3. `fillInPlaceholders`

Rewrites the input `AsmList`, replacing the placeholder
instructions the front-end inserted with the concrete
load / play / cwvf / wvfs sequences chosen above.  Per-play
emission goes through `placeCommands` →
`placeSingleCommand`, which dispatch to a family of helpers
(`wvfImpl`, `wvfRegImpl`, `wvfs`, `splitPlay`, `insertPlay`,
`clampToCache`) depending on node kind, indexed vs
non-indexed play, and whether the load straddles a cache
line.

## The cache model

The cache abstraction lives in
`reconstructed/include/zhinst/runtime/cache.hpp`.  Three
properties matter at the prefetcher level:

- **Capacity** comes from `DeviceConstants::waveformMemorySize`,
  varies per device family, and is divided into fixed-size
  pages (one waveform occupies `pagesNeeded` pages).  See
  \ref notes_device_constants for the per-family numbers.
- **Replacement** is not strictly LRU: `Cache::reuse` /
  `Cache::stillInMemory` cooperate with `Cache::play` to
  track which loads are still "fresh enough" to skip a
  reload, while `Cache::allocate` evicts when it must.
- **Scoping** at branch / loop boundaries via
  `Cache::getScope` makes allocations inside the scope
  reversible — capacity consumed by a not-taken branch is
  returned when the scope is destroyed.

`Cache::Pointer` is the per-allocation handle stored on every
load's `PrefetcherNodeState::cachePtr` once `allocate` has
reserved a slot for it.

## Cache-line crossing

When a single load straddles a cache-line boundary, the
sequencer cannot satisfy it with one fetch.  The relevant
flag (`WaveformIR::crossesCacheLine_`) is copied during
`assignLoad` onto the corresponding
`PrefetcherNodeState::crossesCacheLine`, and on **Hirzel**
devices `placeSingleCommand` consults it to emit the
cache-clamped `prf` prefetch sequence required to load the
two halves separately.  See \ref notes_cervino_vs_hirzel.

## `PlayConfig` and CWVF tracking

The prefetcher does not just plan loads — it also tracks the
*current* play configuration so it can omit redundant CWVF
register writes.  Two members are dedicated to this:

- `cwvfConfig_` (a `PlayConfig`; see \ref notes_play_config)
  records the play-word currently believed to be in the
  CWVF register.
- `globalCwvfValid_` says whether `cwvfConfig_` can be
  trusted across the whole program (i.e. no branching has
  occurred that would force a re-emit on every path).

`needsNewCwvf(node)` consults these against the node's own
`PlayConfig` to decide whether a fresh CWVF write is
required before the play.  `optimizeCwvf` and `globalCwvf`
are the AST-rewrite passes that exploit this to elide
unnecessary writes; `mergeLoads`, `removeBranches`, and
`expandSetVar` are the supporting peephole rewrites used
during placement.

## Outputs consumed by the rest of the compiler

After `runPrefetcher` returns, the `Compiler` reads back two
prefetcher-side aggregates for the ELF metadata sections:

- `getUsedChannels()` — bitmask of physical channels touched
  by any play; written to the `.channels` section.
- `getUsedFourChannelMode()` — whether any play used the
  4-channel encoding; also written to `.channels`.

`getUsedCache(node)` is available on individual nodes for
diagnostic / accounting purposes and contributes to the
`.wavemem` cache-usage breakdown.

`collectUsedWaves(node)` walks the planned tree and emits the
final `.wf_<name>` waveform set — a waveform is only written
to the ELF if at least one surviving play references it.

## See also

- \ref notes_pipeline — overall compiler pipeline; prefetch is step 9.
- \ref notes_play_config — `PlayConfig` bit layout, CWVF word.
- \ref notes_cervino_vs_hirzel — the device-family split that
  drives the two register slots and the cache-line-crossing logic.
- \ref notes_device_constants — per-device-family waveform
  memory size and cache geometry.
- \ref notes_elf_format — `.wavemem`, `.wf_<name>`,
  `.channels` sections written from prefetcher outputs.
- \ref notes_optimization_passes — the wider optimisation
  context that the prefetcher's `optimize*` helpers fit into.
