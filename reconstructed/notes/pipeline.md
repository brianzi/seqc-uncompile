# Compilation Pipeline {#notes_pipeline}

`Compiler::compile()` is the single entry point.  It accepts a
SeqC source string and produces a final `vector<Assembler>`
ready for ELF emission.  The full pipeline is 12 steps, plus an
embedded prefetch sub-pipeline.

The high-level shape and the directory / class map appear on the
[mainpage](index.html#pipeline-overview).  This page documents
the contract of each step in turn.

[TOC]

## Step 1 — `unifyLineEndings`

The source string is normalised in-place: `\r\n` and bare `\r`
are replaced by `\n`.  Every subsequent line-number bookkeeping
step assumes Unix line endings.

## Step 2 — `parse`

A flex/bison front-end (`seqc_lex` / `seqc_parse`, both
configured by `SeqcParserContext`) consumes the normalised source
and produces a `shared_ptr<Expression>` — the *raw* parser AST,
still in the syntactic shape of the original SeqC text.  Grammar
and token definitions are documented in
\ref notes_seqc_parser_grammar.

A parse error throws `CompilerException` with the offending
line number and message; `Compiler::compile()` does not catch
this and propagates it straight to the caller.

## Step 3 — `toSeqCAst`

The raw `Expression` tree is rewritten into a typed
`SeqCAstNode` hierarchy.  Operator overloads and shorthand syntax
are desugared here; every node carries the type information the
front-end needs to dispatch.

## Step 4 — `FrontEndLoweringFacade::lower`

The front-end traverses the typed AST and dispatches virtually
on each `SeqCAstNode` to emit:

- `AsmList` instructions (assignments, control flow, function
  calls, register I/O).
- A **node tree** (rooted at `Compiler::ast_`) that captures the
  structural shape — `Loop`, `Branch`, `Play`, `Load`, … — used
  later by the prefetcher.

`FrontEndLoweringFacade` is documented in
\ref notes_frontend_lowering.  SeqC built-in functions
(`playWave`, `setTrigger`, `wait`, …) reach the system through
`CustomFunctions`; see \ref notes_custom_functions.

If lowering raises a compiler error
(`Compiler::hadCompilerError()`), the pipeline stops here and
returns the accumulated messages without proceeding to step 5.

## Step 5 — Assembly preamble

The compiler builds the fixed prologue of the program: entry
label, placeholder-load instruction (resolved later by the
prefetcher), and any device-specific initialisation that runs
before user instructions.

## Step 6 — Linearise the node tree

The node tree built during lowering is walked into a flat
`AsmList`.  Sibling chains define execution order;
\ref notes_node_tree_structure documents the link semantics in
detail.  The list is closed off with terminator instructions
(`end` / `wwvf` / `nop`).

## Step 7 — `AsmOptimize::optimizePreWaveform`

A first optimisation pass runs *before* waveform placement.  It
performs dead-code elimination on instructions whose results are
provably unused.  See \ref notes_optimization_passes.

## Step 8 — Serialise / deserialise

The `AsmList` is round-tripped through its textual canonical
form (see \ref notes_asm_parser_grammar).  This both validates
that the in-memory list is reversible and forces every subsequent
pass to operate on a known-good representation.

## Step 9 — `runPrefetcher`

The waveform placement sub-pipeline.  It is large enough to
deserve its own section:

```
runPrefetcher(wavetableIR, asmList, asmCommands, placeholder,
              deviceConstants, config)
  │
  ├─ (debug) optionally serialise / round-trip WavetableIR JSON
  │
  ├─ Construct Prefetch(config, deviceConstants, asmCommands,
  │                     rootNode, wavetableIR,
  │                     warningCallback, cancelCallback)
  │
  ├─ Prefetch::preparePlays
  │
  ├─ Prefetch::getUsedWavesForDevice(deviceIndex)
  ├─ WavetableIR::setUsedWaveforms
  ├─ WavetableIR::assignWaveIndexImplicit
  ├─ WavetableIR::alignWaveformSizes
  ├─ WavetableIR::assignWaveformAllocationSizes
  │
  ├─ (conditional) Prefetch::determineFixedWaves
  ├─ WavetableIR::updateWaveforms(fixedFlag, anotherFlag)
  │
  ├─ Prefetch::placeLoads
  │
  ├─ (debug) Prefetch::print
  │
  ├─ Prefetch::fillInPlaceholders(asmList)
  │
  └─ Copy result AsmList + channel info
     (getUsedChannels, getUsedFourChannelMode)
```

The prefetcher decides which waveforms reside in waveform
memory at any given point in the program and inserts the
corresponding `wvf`-family loads.  See
\ref notes_prefetch_scheduling for the algorithm.

Exceptions raised inside the prefetcher are caught and rethrown
as `CompilerException`.

## Step 10 — `AsmOptimize::optimizePostWaveform`

A second, larger pass runs *after* placement.  In order:

1. Dead-jump elimination and label cleanup.
2. Register zeroing — replace `R0`-using moves with the
   canonical form so the register allocator sees a normalised
   list.
3. Register allocation.
4. `reportUserMessages` — accumulate any warnings the passes
   produced.

See \ref notes_optimization_passes for each sub-pass.

## Step 11 — `unsyncCervino`

On Cervino-family devices a sync teardown sequence is appended
to undo any earlier `syncCervino` setup.  No-op on Hirzel.

## Step 12 — Build the output

The final `AsmList` is materialised as `vector<Assembler>`.
After the build, `hadCompilerError()` is consulted one last
time: if any pass enqueued an error message after step 4 (most
commonly the post-waveform optimiser) the result is rejected.

## Cross-cutting concerns

### Error messages

`Compiler::messages_` accumulates errors and warnings throughout
the pipeline.  `hadCompilerError()` is checked after lowering
(step 4) and after the build (step 12); a fatal parser error
short-circuits via `CompilerException`.  Duplicate messages — same
line *and* same text — are suppressed by the message-collection
layer.

### Debug toggles

`AWGCompilerConfig` carries a small set of debug bits that, when
set, dump intermediate IR to stderr at well-defined cut points:

| Bit  | Dump                                                             |
|------|------------------------------------------------------------------|
| 0x02 | Raw `Expression` tree, after step 2                              |
| 0x04 | Typed `SeqCAstNode` tree, after step 3                           |
| 0x08 | Node tree after prefetch + final assembly listing, after step 12 |

### Cancellation and progress

`Compiler` holds weak references to two optional callbacks:

- `CancelCallback` — polled at coarse points; raises
  `CompilerException("cancelled")` if it fires.
- `ProgressCallback` — invoked with stage-completion notifications
  for UI / IDE integrations.

Both are no-ops when the weak reference is empty.

## See also

- \ref notes_frontend_lowering — front-end data model
  (`FrontEndLoweringFacade`, `EvalResults`, `Value`,
  `LowerResult`).
- \ref notes_node_tree_structure — node-tree link semantics.
- \ref notes_prefetch_scheduling — waveform-prefetch algorithm.
- \ref notes_optimization_passes — pre- and post-waveform
  passes.
- \ref notes_custom_functions — SeqC built-in functions.
- \ref notes_asm_parser_grammar — `.seqasm` text grammar used
  for the round-trip in step 8.
