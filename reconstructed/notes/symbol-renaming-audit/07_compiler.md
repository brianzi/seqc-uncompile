# Batch 07 — compiler

## 1. Files considered

- `reconstructed/include/zhinst/compiler.hpp`
- `reconstructed/src/compiler.cpp`

Cross-referenced (read-only):
- `reconstructed/include/zhinst/prefetch.hpp` (Prefetch::getUsedFourChannelMode etc.)
- `reconstructed/src/awg_compiler.cpp` (consumer of all `Compiler` getters)
- `reconstructed/src/resources_static_global.cpp` (StaticResources::usedSampleRate_)
- `reconstructed/notes/symbol-renaming-audit/23_awg_compiler_config.md`
  (canonical names of `AWGCompilerConfig` fields used here)
- `reconstructed/notes/symbol-renaming-audit/09_prefetch.md`,
  `09_prefetch_part2.md` (Prefetch types)

Symbol-table verification (`nm --demangle _seqc_compiler.so`):

The following Compiler members appear by name in the symbol table and
are therefore §3-excluded as method names:

```
zhinst::Compiler::Compiler(AWGCompilerConfig const&, DeviceConstants const&,
                           shared_ptr<WavetableFront>)               @0x11d080
zhinst::Compiler::~Compiler()                                        @0x103660
zhinst::Compiler::compile(string const&)                             @0x11f150
zhinst::Compiler::parse(string const&)                               @0x11d9b0
zhinst::Compiler::reset()                                            @0x11dfe0
zhinst::Compiler::printAST(shared_ptr<Expression>, string const&)    @0x122640
zhinst::Compiler::unifyLineEndings(string const&) const              @0x11d720
zhinst::Compiler::runPrefetcher(...)                                 @0x11dff0
zhinst::Compiler::setCancelCallback(weak_ptr<CancelCallback>)        @0x123480
zhinst::Compiler::setProgressCallback(weak_ptr<ProgressCallback>)    @0x123510
zhinst::Compiler::getNodeAccessList() const                          @0x123550
zhinst::Compiler::getNodeToModeMap() const                           @0x123570
zhinst::Compiler::getChannelInfo() const                             @0x123590
zhinst::Compiler::usedDeviceSampleRate() const                       @0x1235e0
zhinst::Compiler::getCompileMessages() const                         @0x1235f0
zhinst::Compiler::setLineNr(int)                                     @0x123640
zhinst::Compiler::getLineMap(int) const                              @0x123660
```

`Compiler` (the class name itself) is also encoded in every mangled
symbol above, so the type name is §3-authoritative-excluded.

`zhinst::FrontEndLoweringFacade::lower(...)` @0x1c1da0 is also in the
table → free-function name §3-excluded. Same for the helper types
`FrontendLoweringContext` and `FrontendLoweringState` (their
destructors appear with those qualified names).

`Compiler::hadSyntaxError()` does **not** appear in the symbol table
— consistent with the in-source comment that this is an inline
delegator written by the recon, not present in the binary. Treated
as in-scope (see §3 below).

No `Compiler::` static data members appear in `nm` output, so no
tier-1 data-symbol exclusions apply to fields in this batch.

## 2. Overview

| Symbol | Misnomer? | Conf | Justification (≤ 5 words) | Proposal(s) | Status |
|---|---|---|---|---|---|
| `Compiler` (type) | no | high | name in binary symbol table | keep current (high) | not-misnomer |
| `Compiler::config_` | no | high | binds to AWGCompilerConfig& ctor arg | keep current (high) | — |
| `Compiler::deviceConstants_` | no | high | binds to DeviceConstants& ctor arg | keep current (high) | — |
| `Compiler::lineNr_` | no | medium | set by `setLineNr(int)` | keep current (medium) | — |
| `Compiler::flags_` | unsure | low | never read; placeholder | `unknownFlags_`, keep current | in-scope (non-static member; no positive nm/strings evidence of original name) |
| `Compiler::pad16_` | no | medium | layout pad, conventional | keep current (high) | — |
| `Compiler::reserved18_` | unsure | low | only written zero, role unknown | keep current, `unknown18_` | in-scope (non-static member; no positive nm/strings evidence) |
| `Compiler::channelCount_` | no | medium | from `Prefetch::getUsedChannels()` | keep current (medium) | — |
| `Compiler::channelMode_` | yes | high | actually `usedFourChannelMode` bool | `usedFourChannelMode_`, `fourChannelMode_` | — |
| `Compiler::usedSampleRate_` | unsure | low | never written here, mirrors Resources field | `usedDeviceSampleRate_`, keep current | cross-batch-arbitration |
| `Compiler::pad26_` | no | medium | layout pad, conventional | keep current (high) | — |
| `Compiler::ast_` | no | high | written from lowered AST root Node | keep current (high) | — |
| `Compiler::messages_` | no | high | CompilerMessageCollection | keep current (high) | — |
| `Compiler::sourceFiles_` | unsure | low | declared but unused locally | keep current | in-scope (non-static member; no positive nm/strings evidence) |
| `Compiler::sourceLines_` | no | medium | filled from `getline(source)` | keep current (medium) | — |
| `Compiler::asmList_` | no | high | the AsmList being assembled | keep current (high) | — |
| `Compiler::wavetable_` | no | high | shared WavetableFront | keep current (high) | — |
| `Compiler::asmCommands_` | no | high | shared AsmCommands | keep current (high) | — |
| `Compiler::waveformGen_` | no | medium | shared WaveformGenerator | keep current (medium) | — |
| `Compiler::customFunctions_` | no | high | shared CustomFunctions | keep current (high) | — |
| `Compiler::cancelCallback_` | no | high | weak_ptr<CancelCallback> | keep current (high) | — |
| `Compiler::progressCallback_` | no | high | weak_ptr<ProgressCallback> | keep current (high) | — |
| `Compiler::parserContext_` | no | high | SeqcParserContext sub-object | keep current (high) | — |
| `Compiler::Compiler::config` | no | high | bound to `config_` | keep current | — |
| `Compiler::Compiler::deviceConstants` | no | high | bound to `deviceConstants_` | keep current | — |
| `Compiler::Compiler::wavetable` | no | high | moved into `wavetable_` | keep current | — |
| `Compiler::compile::source` | no | high | SeqC source text | keep current | — |
| `Compiler::unifyLineEndings::input` | no | medium | source text | keep current | — |
| `Compiler::parse::source` | no | high | SeqC source text | keep current | — |
| `Compiler::printAST::expr` | no | medium | the Expression to dump | keep current | — |
| `Compiler::printAST::label` | no | medium | indent prefix string | keep current | — |
| `Compiler::runPrefetcher::wavetableIR` | no | high | shared_ptr<WavetableIR> | keep current | — |
| `Compiler::runPrefetcher::asmList` | no | high | matches AsmList type | keep current | — |
| `Compiler::runPrefetcher::asmCommands` | no | high | shared_ptr<AsmCommands> | keep current | — |
| `Compiler::runPrefetcher::placeholder` | no | medium | the asmLoadPlaceholder Asm | keep current, `placeholderAsm` | — |
| `Compiler::runPrefetcher::deviceConstants` | no | high | DeviceConstants ref | keep current | — |
| `Compiler::runPrefetcher::config` | no | high | AWGCompilerConfig ref | keep current | — |
| `Compiler::setCancelCallback::cb` | no | medium | the callback weak_ptr | keep current, `callback` | — |
| `Compiler::setProgressCallback::cb` | no | medium | the callback weak_ptr | keep current, `callback` | — |
| `Compiler::setLineNr::nr` | no | medium | the line number | keep current, `lineNr` | — |
| `Compiler::getLineMap::offset` | no | medium | added to each pc | keep current | — |
| `Compiler::getLineMap::counter` (local) | no | medium | instruction-count accumulator | keep current | — |
| `Compiler::getLineMap::seq` (local) | unsure | low | label-counted sequence id | keep current, `labelSeq` | — |
| `Compiler::compile::normalized` (local) | no | high | unifyLineEndings output | keep current | — |
| `Compiler::compile::expr` (local) | no | high | parse() output | keep current | — |
| `Compiler::compile::seqcAst` (local) | no | high | toSeqCAst output | keep current | — |
| `Compiler::compile::lowerResult` (local) | no | high | LowerResult struct | keep current | — |
| `Compiler::compile::staticResources` (local) | no | high | StaticResources shared_ptr | keep current | — |
| `Compiler::compile::resources` (local) | no | high | GlobalResources shared_ptr | keep current | — |
| `Compiler::compile::startLabel` (local) | no | high | "\nstart" label string | keep current | — |
| `Compiler::compile::labelAsm` (local) | no | high | asmLabel result | keep current | — |
| `Compiler::compile::placeholderAsm` (local) | no | high | asmLoadPlaceholder result | keep current | — |
| `Compiler::compile::rootNode` (local) | no | high | root Node of tree | keep current | — |
| `Compiler::compile::hasMainAndAst` (local) | no | medium | predicate naming readable | keep current | — |
| `Compiler::compile::worklist` (local) | no | medium | BFS deque | keep current | — |
| `Compiler::compile::current` (local) | no | medium | BFS work item | keep current | — |
| `Compiler::compile::cancelLocked` (local) | no | medium | locked weak_ptr | keep current | — |
| `Compiler::compile::optimizer` (local) | no | high | AsmOptimize instance | keep current | — |
| `Compiler::compile::wavetableIR` (local) | no | high | constructed WavetableIR | keep current | — |
| `Compiler::compile::result` (local) | no | medium | output AssemblerInstr vector | keep current | — |
| `Compiler::compile::errorCb`/`infoCb`/`warningCb` (locals) | no | medium | callback closures | keep current | — |
| `Compiler::compile::serialized` (local) | no | medium | serialize() output bytes | keep current | — |
| `Compiler::compile::ofs` (local) | no | medium | output file stream | keep current | — |
| `Compiler::compile::unsyncEntries`, `entry`, `insertPos` (locals) | no | medium | obvious | keep current | — |
| `Compiler::runPrefetcher::prefetch` (local) | no | high | the Prefetch instance | keep current | — |
| `Compiler::runPrefetcher::waves` (local) | no | medium | prefetched waveform list | keep current | — |
| `Compiler::runPrefetcher::warningCb` (local) | no | medium | warning callback closure | keep current | — |
| `Compiler::parse::scanner` (local) | no | high | flex scanner handle | keep current | — |
| `Compiler::parse::buf` (local) | no | medium | flex buffer state | keep current | — |
| `Compiler::parse::rawResult` (local) | no | medium | bison out-param ptr | keep current | — |
| `Compiler::parse::ctx` (local) | no | medium | ptr to parserContext_ | keep current | — |
| `Compiler::parse::iss`/`line` (locals) | no | high | line-split helpers | keep current | — |
| `CompileResult` (type) | no | medium | return-by-sret aggregate | keep current (medium) | — |
| `CompileResult::asmList` | no | high | vector<AssemblerInstr> | keep current | — |
| `CompileResult::wavetableIR` | no | high | shared_ptr<WavetableIR> | keep current | — |
| `FrontEndLoweringFacade` (namespace) | no | high | mangled into `lower` symbol | keep current (high) | not-misnomer |
| `FrontEndLoweringFacade::lower` | no | high | name in binary symbol table | keep current (high) | not-misnomer |
| `FrontEndLoweringFacade::lower::resources` | no | high | shared_ptr<Resources> | keep current | — |
| `FrontEndLoweringFacade::lower::ast` | no | high | SeqCAstNode& | keep current | — |
| `FrontEndLoweringFacade::lower::messages` | no | high | CompilerMessageCollection& | keep current | — |
| `FrontEndLoweringFacade::lower::asmCommands` | no | high | shared_ptr<AsmCommands> | keep current | — |
| `FrontEndLoweringFacade::lower::customFunctions` | no | high | shared_ptr<CustomFunctions> | keep current | — |
| `FrontEndLoweringFacade::lower::waveformGen` | no | high | shared_ptr<WaveformGenerator> | keep current | — |
| `FrontEndLoweringFacade::lower::wavetable` | no | high | shared_ptr<WavetableFront> | keep current | — |
| `FrontEndLoweringFacade::lower::channelGrouping` | yes | medium | propagated misnomer (loop limit) | `loopUnrollLimit`, keep current | coordinated-rename |
| `LowerResult` (type) | unsure | low | recon-only aggregate, not in nm | keep current, `LowerOutput` | in-scope (no LowerResult/LowerOutput in nm or strings) |
| `LowerResult::astResult` | no | medium | shared_ptr<Node> from state.result | keep current | — |
| `LowerResult::evalResult` | no | medium | shared_ptr<EvalResults> from evaluate sret | keep current | — |
| `Compiler::hadSyntaxError()` (method) | no | medium | mirrors SeqcParserContext::hadSyntaxError | keep current | — |

## 3. Detailed findings

### Compiler::flags_  [unsure / low / verify-not-original]

Evidence:
- `compiler.hpp:187` `uint16_t flags_;                               // +0x14`
- `compiler.cpp:52` `, flags_(0)` — only initialization in this batch.
- `grep flags_` in `compiler.cpp` returns no other read or write site.
- The header comment inventory (lines 8–9) documents the field at
  +0x14 as `uint16_t flags_`.

Interpretation:
- The reconstruction reserves a 2-byte slot at +0x14 and labels it
  `flags_` but no Compiler method consumes it. The name `flags_` is
  therefore a guess about role, not a verified description.
- The adjacent `pad16_[2]` at +0x16 makes the same +0x14 slot a
  natural alignment-only slot for the following `reserved18_` u64.

Judgement:
- Unsure: cannot tell whether the name "flags" is misleading or
  prescient — there is no observed usage in this batch to weigh it
  against.

Proposals:
- `unknownFlags_`  (low) — make the speculative nature explicit.
- keep current  (low) — if a future batch finds a real flags-bitmask
  use site.

Locations consulted:
- declared: `include/zhinst/compiler.hpp:187`
- used:     `src/compiler.cpp:52` (init only)

### Compiler::reserved18_  [unsure / low / verify-not-original]

Evidence:
- `compiler.hpp:189` `uint64_t reserved18_;                          // +0x18`
- `compiler.cpp:53` `, reserved18_(0)`
- `compiler.cpp:262-263`
  `// Step 5d: Clear reserved field (binary writes 0 to [r15+0x18])`
  `reserved18_ = 0;`

Interpretation:
- The slot is touched only with stores of 0. The recon comment
  records the binary observation (`writes 0 to [r15+0x18]`) but
  draws no functional conclusion. The name `reserved18_` is a
  pure layout placeholder ("offset 0x18, role unknown").

Judgement:
- Unsure: name accurately describes "we know the offset, not the
  meaning" but does not describe a *role*. Whether it's a misnomer
  depends on whether the slot has a real purpose elsewhere in the
  binary that this batch cannot see.

Proposals:
- keep current  (medium) — telegraphs "unknown" honestly.
- `unknown18_`  (low) — slightly more conventional for placeholders.

Locations consulted:
- declared: `include/zhinst/compiler.hpp:189`
- used:     `src/compiler.cpp:53,263`

### Compiler::channelMode_  [yes / high / —]

Evidence:
- `compiler.hpp:191` `uint8_t channelMode_;                          // +0x24`
- `compiler.cpp:55` `, channelMode_(0)`
- `compiler.cpp:561`
  `channelMode_ = prefetch.getUsedFourChannelMode();  // binary 0x11e5e0: direct store`
- `compiler.cpp:593-598`
  ```
  std::vector<int> Compiler::getChannelInfo() const {
      std::vector<int> result;
      result.push_back(static_cast<int>(channelCount_));
      result.push_back(static_cast<int>(channelMode_));
      return result;
  }
  ```
- `prefetch.hpp:244`
  `bool getUsedFourChannelMode() const;                               // 0x1df400`
- `nm --demangle` confirms
  `zhinst::Prefetch::getUsedFourChannelMode() const   @0x1df400`
  — the method name is binary-authoritative and returns `bool`.

Interpretation:
- The single write site assigns the value of `getUsedFourChannelMode()`,
  which is a `bool` (4-channel-mode flag) per its declaration and per
  the symbol-table-attested method name. The single read packs that
  byte into a `vector<int>` for the `.channels` ELF section.
- The current name `channelMode_` suggests an enumeration over multiple
  channel modes; the actual stored value is a single boolean predicate
  ("did Prefetch detect four-channel mode?"). The misnomer is the
  same direction as the `play_config` arbitrations resolved in batch
  10 / 38: a name implying an enum but holding a flag.

Judgement:
- Yes — `channelMode_` overstates the value's domain; it is a single
  binary flag whose source is `Prefetch::getUsedFourChannelMode()`.

Proposals:
- `usedFourChannelMode_`  (high) — exact mirror of the source method
  name and matches the `usedSampleRate_` / `getUsedDeviceSampleRate`
  pattern already in this class.
- `fourChannelMode_`  (medium)
- `usedFourChannelMode` declared as `bool`  (medium) — type
  observation per §2a; the byte storage is fine but `bool` would
  make the binary semantics self-evident.
- keep current  (low)

Locations consulted:
- declared: `include/zhinst/compiler.hpp:191`
- used:     `src/compiler.cpp:55,561,596`
- source:   `include/zhinst/prefetch.hpp:244`
- consumer: `src/awg_compiler.cpp:892` (packs into `.channels` section)

### Compiler::usedSampleRate_  [unsure / low / cross-batch-arbitration]

Evidence:
- `compiler.hpp:192` `uint8_t usedSampleRate_;                       // +0x25`
- `compiler.cpp:56,602`
  `, usedSampleRate_(0)` and
  `bool Compiler::usedDeviceSampleRate() const { return usedSampleRate_ != 0; }`
- No write site exists in `compiler.cpp` after the initializer.
- `resources_static_global.cpp:39,52,166`
  `bool usedSampleRate_;        // +0xD8`  in `StaticResources`,
  set to true when the sequence references `DEVICE_SAMPLE_RATE`.
- `awg_compiler.cpp:904`
  `if (!std::isnan(sr) && compiler_.usedDeviceSampleRate()) { ... }`
  uses the getter to decide whether to emit `.required_sample_rate`.

Interpretation:
- The field name `usedSampleRate_` mirrors `StaticResources::usedSampleRate_`
  exactly. The getter uses the more specific name `usedDeviceSampleRate()`
  (binary-attested method name). This is a consistent he-said/she-said
  between the field naming on Compiler vs. its public getter — and the
  field is also one half of a pair with `StaticResources::usedSampleRate_`
  which the recon could not yet identify a write path for in `compiler.cpp`.
- The local field appears never to be written from this batch's code,
  yet the getter is consumed externally — strongly implying a missing
  write site (a method on Compiler that copies `Resources::usedSampleRate_`
  into `Compiler::usedSampleRate_`) not yet reconstructed.

Judgement:
- Unsure for this batch. The field name itself isn't wrong, but it
  is inconsistent with its own getter (`usedDeviceSampleRate`).
  Arbitration with `StaticResources::usedSampleRate_` (batch 19)
  is required.

Proposals:
- `usedDeviceSampleRate_`  (medium) — aligns with the getter and the
  consumer's `.required_sample_rate` semantics.
- keep current  (medium) — matches `StaticResources::usedSampleRate_`
  if that name is the true canonical one.

Cross-reference:
- Counterpart `StaticResources::usedSampleRate_` in batch 19
  (resources). Batch 19 should decide which side keeps the
  unqualified name.

Locations consulted:
- declared: `include/zhinst/compiler.hpp:192`
- used:     `src/compiler.cpp:56,602`
- source:   `include/zhinst/resources.hpp:513`,
            `src/resources_static_global.cpp:166`
- consumer: `src/awg_compiler.cpp:904`

### Compiler::sourceFiles_  [unsure / low / verify-not-original]

Evidence:
- `compiler.hpp:196` `std::vector<std::string> sourceFiles_;         // +0x58`
- `grep sourceFiles_ src/compiler.cpp` → no read or write site in the
  batch (only the layout comment at `compiler.hpp:13`).
- `sourceLines_` (the adjacent vector) is filled by `parse()` from
  `getline(source)` — a clear write/role.

Interpretation:
- `sourceFiles_` is reserved by layout but has no observed
  read or write in the batch. The plural noun suggests a list of
  filenames (e.g. for `#include` chains), which is plausible but
  unverified.

Judgement:
- Unsure: name is plausible but unsupported by usage observed here.

Proposals:
- keep current  (medium) — the recon's working hypothesis.
- (would-be lower-confidence alternates omitted; nothing else fits
  better without more evidence.)

Locations consulted:
- declared: `include/zhinst/compiler.hpp:196`

### FrontEndLoweringFacade::lower::channelGrouping  [yes / medium / coordinated-rename]

Evidence:
- `compiler.cpp:672,681`
  `int channelGrouping)` and `context.channelGrouping = channelGrouping;`
- The argument is supplied at the call site in
  `compiler.cpp:283` as `config_->channelGrouping  // [config+0x98]`.
- Batch 23 (`23_awg_compiler_config.md`) flags
  `AWGCompilerConfig::channelGrouping` as `yes / medium` — its
  actual semantic role is a loop-unroll iteration limit
  (`if (iterCount > ctx.channelGrouping) { error 0x7b }`), not a
  channel-grouping count.

Interpretation:
- This parameter and `FrontendLoweringContext::channelGrouping` are
  faithful copies of the misnamed config field. They will need to
  be renamed in lockstep with the `AWGCompilerConfig::channelGrouping`
  decision.

Judgement:
- Yes — same misnomer as in batch 23, propagated through the
  facade's parameter and the context field.

Proposals:
- `loopUnrollLimit`  (medium) — match batch 23's leading proposal.
- `unrollLimit`  (low)
- keep current  (low) — only if batch 23 ultimately keeps the name.

Cross-reference:
- `AWGCompilerConfig::channelGrouping` and the equivalent field on
  `FrontendLoweringContext::channelGrouping` (batch 04 /
  frontend_lowering — not yet covered as its own batch). All three
  must be renamed together.

Locations consulted:
- declared: `include/zhinst/compiler.hpp:92`
- defined:  `src/compiler.cpp:672,681`
- caller:   `src/compiler.cpp:283`
- batch 23: `notes/symbol-renaming-audit/23_awg_compiler_config.md:348`

### LowerResult  [unsure / low / verify-not-original]

Evidence:
- `compiler.hpp:75-78` `struct LowerResult { ... };`
- `compiler.hpp:62-74` documents this as a *recon-only aggregate*:
  > "CORRECTION 2026-04-23 (Phase 15a-i): lower() was declared void
  > but actually returns a 32B struct via sret."
- `nm --demangle` does not show any symbol named
  `FrontEndLoweringFacade::LowerResult` or `zhinst::LowerResult`.
  The struct is not encoded in the binary symbol table.

Interpretation:
- The name `LowerResult` is the recon's invention to describe the
  sret aggregate of `lower()`. Since it isn't tier-1 (not in `nm`)
  the name is in scope. Internally it carries a `shared_ptr<Node>`
  and a `shared_ptr<EvalResults>` — i.e. an AST root and an
  evaluate output. The name is generic but accurate.

Judgement:
- Unsure: the name is reasonable for an internal aggregate, but a
  more descriptive name (`LoweredAst`, `LoweringOutput`) might be
  clearer. No recordable evidence either way.

Proposals:
- keep current  (medium)
- `LowerOutput`  (low)
- `LoweredProgram`  (low)

Locations consulted:
- declared: `include/zhinst/compiler.hpp:75-78`
- used:     `src/compiler.cpp:286,694-697`

### Compiler::hadSyntaxError() (method)  [no / medium / —]

Evidence:
- `compiler.hpp:171`
  `bool hadSyntaxError() const;                                   // inline, delegates to parserContext_`
- `compiler.cpp:605-609`
  ```
  bool Compiler::hadSyntaxError() const {
      // Binary reads byte at Compiler+0x100+0x03 = parserContext_[3]
      return parserContext_.hadSyntaxError();
  }
  ```
- `nm --demangle` does NOT list `Compiler::hadSyntaxError()` — i.e.
  this is **not** a binary-attested method, despite siblings like
  `setLineNr` etc. being in the table. Likely the binary inlines the
  byte read at every call site rather than calling a method.
- Consumer: `awg_compiler.cpp:745,995`
  `if (compiler_.hadSyntaxError()) { ... }` — uses it as a one-shot
  predicate after parse.

Interpretation:
- The method is a pure recon-introduced thin wrapper that names what
  the binary does inline. The name `hadSyntaxError` is verbatim from
  `SeqcParserContext::hadSyntaxError()` (which IS in `nm` per batch 18)
  and matches every call site's intent. Recording as `not-misnomer`-
  style positive evidence even though the method itself is in scope.

Judgement:
- No — the wrapper's name faithfully describes the underlying byte
  it returns and matches both the source field's authoritative name
  and the consumer's use.

Proposals:
- keep current  (high)

Locations consulted:
- declared: `include/zhinst/compiler.hpp:171`
- defined:  `src/compiler.cpp:605-609`
- source:   `src/seqc_parser_context.cpp:86`
- consumer: `src/awg_compiler.cpp:745,995`

## 4. Symbols inspected and judged routinely fine

Fields:
- `config_`, `deviceConstants_`, `lineNr_`, `pad16_`, `pad26_`,
  `channelCount_`, `ast_`, `messages_`, `sourceLines_`, `asmList_`,
  `wavetable_`, `asmCommands_`, `waveformGen_`, `customFunctions_`,
  `cancelCallback_`, `progressCallback_`, `parserContext_` — every
  read/write site uses the field in a way that matches its name and
  declared type (see Overview table). `channelCount_` is set from
  `Prefetch::getUsedChannels() → uint32_t` and packed into the
  `.channels` ELF section as the channel count — exact match.

Methods (all §3-excluded by symbol table):
- `Compiler::Compiler`, `~Compiler`, `compile`, `parse`, `reset`,
  `printAST`, `unifyLineEndings`, `runPrefetcher`, `setCancelCallback`,
  `setProgressCallback`, `getNodeAccessList`, `getNodeToModeMap`,
  `getChannelInfo`, `usedDeviceSampleRate`, `getCompileMessages`,
  `setLineNr`, `getLineMap`. Type name `Compiler` likewise.

Free function (§3-excluded):
- `FrontEndLoweringFacade::lower` and the `FrontEndLoweringFacade`
  namespace name. Type names `FrontendLoweringContext`,
  `FrontendLoweringState` (their dtors are in `nm`).

Parameters (after one-pass usage check):
- All ctor params (`config`, `deviceConstants`, `wavetable`) bind
  directly to the like-named member; clear.
- `compile::source`, `parse::source`, `unifyLineEndings::input` —
  unambiguous string inputs.
- `printAST::expr`, `printAST::label` — the AST and its indent prefix.
- `runPrefetcher::*` — every parameter is forwarded into the
  `Prefetch` ctor or `WavetableIR` calls under the same name; no
  rebinding mismatches.
- `setCancelCallback::cb`, `setProgressCallback::cb` — `cb` is
  conventional for callbacks of this kind across the codebase.
- `setLineNr::nr` — terse but obvious in a 1-line setter.
- `getLineMap::offset` — added to the per-instruction pc; matches.
- `FrontEndLoweringFacade::lower::*` — `resources`, `ast`, `messages`,
  `asmCommands`, `customFunctions`, `waveformGen`, `wavetable` all
  match the call site at `compiler.cpp:275-283`.

Locals in `compile()` and `runPrefetcher()`:
- `normalized`, `expr`, `seqcAst`, `lowerResult`, `staticResources`,
  `resources`, `startLabel`, `labelAsm`, `placeholderAsm`,
  `rootNode`, `hasMainAndAst`, `worklist`, `current`,
  `cancelLocked`, `optimizer`, `wavetableIR`, `result`,
  `errorCb`, `infoCb`, `warningCb`, `serialized`, `ofs`,
  `unsyncEntries`, `entry`, `insertPos`, `prefetch`, `waves`,
  `scanner`, `buf`, `rawResult`, `ctx`, `iss`, `line`, `endAsm`,
  `wwvfAsm`, `nopAsm`, `initEntries`, `assemblers`, `childLabel`,
  `_i`, `chanInfo`(N/A — actually in awg_compiler), `counter`,
  `seq` — every local has either an obvious meaning (`scanner`,
  `placeholderAsm`, `rootNode`) or matches the value of the
  expression that produced it. `seq` in `getLineMap` is the only
  one that is mildly cryptic (a label-incremented sequence id) but
  no recordable evidence to warrant a §3 block.

`CompileResult::asmList`, `CompileResult::wavetableIR` — both
exactly match the field comments at `compiler.hpp:100-101` and the
`return CompileResult{...}` aggregate at `compiler.cpp:482`.

`LowerResult::astResult`, `LowerResult::evalResult` — names match
the corresponding sret slots documented at `compiler.hpp:76-77`
and `compiler.cpp:660-665`.

## 5. Coverage

**Fully covered:**
- All fields of `Compiler` (+0x00 through +0x100, including pads).
- All methods declared in `compiler.hpp` and defined in `compiler.cpp`
  (parameter and local survey).
- The `CompileResult` and `LowerResult` aggregate types and their
  members.
- The `FrontEndLoweringFacade::lower` free function (parameters and
  locals).

**Deferred / cross-batch:**
- `Compiler::usedSampleRate_` — flagged `cross-batch-arbitration`
  vs. `StaticResources::usedSampleRate_` (batch 19).
- `FrontEndLoweringFacade::lower::channelGrouping` — flagged
  `coordinated-rename` with `AWGCompilerConfig::channelGrouping`
  already proposed in batch 23 and the matching field on
  `FrontendLoweringContext` (batch covering `frontend_lowering.hpp`).

**Not covered (out of scope per RULES §2/§3):**
- All `Compiler::` method names and the `Compiler` type name —
  appear as `nm --demangle` text symbols (§3 tier-1 exclusion).
  Listed in §1 for traceability.
- `FrontEndLoweringFacade` namespace name and `lower` free-function
  name — `nm` shows the mangled symbol; §3-excluded.
- `FrontendLoweringContext` / `FrontendLoweringState` type names —
  destructors appear in `nm` so the type names are tier-1; the
  fields and members of those structs are out of this batch's
  files (live in `frontend_lowering.hpp/.cpp`) and were not opened.
- Stdlib types (`shared_ptr`, `weak_ptr`, `vector`, `string`,
  `deque`, etc.) and boost (`replace_all_copy`).
- Generated parser glue (`seqc_lex_init_extra`, `seqc__scan_string`,
  `seqc__delete_buffer`, `seqc_lex_destroy`, `seqc_parse`,
  `yy_buffer_state`).
- `printAST` references to `EOperator`, `ECommandType`, `VarType`,
  `EOperationType`, `VarType_Unset` and the `str()` overloads —
  belong to other batches (types / operators).
