# Compilation Pipeline {#notes_pipeline}

\note **Reverse-engineering reference material.** This page is part of
the `reconstructed/notes/` set: deep-dive technical notes for
contributors working on the reconstruction. It cites binary addresses,
opcodes, and disassembly observations directly so they remain
discoverable from the rendered site. The standard documentation-voice
rules for API briefs (no binary citations outside `\binarynote`) do
**not** apply to this page.

Reconstructed from `Compiler::compile()` at 0x11f150 (~13KB) and
`Compiler::runPrefetcher()` at 0x11dff0 (~2.8KB).

## High-Level Flow

```
Source Code (string)
  │
  ├─ 1. unifyLineEndings()          — normalize \r\n / \r → \n
  │
  ├─ 2. parse()                     — flex/bison (seqc_lex/seqc_parse)
  │     └→ shared_ptr<Expression>      AST of original SeqC syntax
  │
  ├─ 3. toSeqCAst()                 — free function, converts Expression → SeqCAstNode
  │     └→ shared_ptr<SeqCAstNode>     typed AST for lowering
  │
  ├─ 4. FrontEndLoweringFacade::lower()  — virtual dispatch on SeqCAstNode
  │     ├─ Inputs: Resources, AsmCommands, CustomFunctions,
  │     │          WaveformGenerator, WavetableFront
  │     └→ Populates AsmList with instructions + Node tree
  │
  ├─ 5. Build assembly preamble     — labels, load placeholder
  │
  ├─ 6. Linearize nodes             — deque<shared_ptr<Node>> → AsmList
  │     └─ Append end/wwvf/nop
  │
  ├─ 7. AsmOptimize::optimizePreWaveform()  — dead code elimination
  │
  ├─ 8. Serialize/deserialize       — round-trip (validation/canonical form)
  │
  ├─ 9. runPrefetcher()             — waveform placement pipeline (see below)
  │
  ├─ 10. AsmOptimize::optimizePostWaveform() — jump elim, label cleanup,
  │      register zeroing, register allocation, reportUserMessages
  │
  ├─ 11. unsyncCervino()            — platform-specific sync teardown
  │
  └─ 12. Build output vector<Assembler>  — final instruction list
```

## runPrefetcher Pipeline (Step 9)

```
runPrefetcher(wavetableIR, asmList, asmCommands, placeholder, deviceConstants, config)
  │
  ├─ (Optional) Serialize WavetableIR to JSON file (debug)
  ├─ (Optional) WavetableIR JSON round-trip (debug/validation)
  │
  ├─ Construct Prefetch(config, deviceConstants, asmCommands, rootNode,
  │                     wavetableIR, warningCallback, cancelCallback)
  │
  ├─ Prefetch::preparePlays()
  │
  ├─ Prefetch::getUsedWavesForDevice(deviceIndex)
  ├─ WavetableIR::setUsedWaveforms()
  ├─ WavetableIR::assignWaveIndexImplicit()
  ├─ WavetableIR::alignWaveformSizes()
  ├─ WavetableIR::assignWaveformAllocationSizes()
  │
  ├─ (Conditional) Prefetch::determineFixedWaves()
  ├─ WavetableIR::updateWaveforms(fixedFlag, anotherFlag)
  │
  ├─ Prefetch::placeLoads()
  │
  ├─ (Optional debug) Prefetch::print(nullptr, 0)
  │
  ├─ Prefetch::fillInPlaceholders(asmList)
  │
  └─ Copy result AsmList + channel info (getUsedChannels, getUsedFourChannelMode)
```

## Key Participants

| Class | Role | Created by |
|-------|------|-----------|
| Compiler | Orchestrator — owns all state, drives pipeline | User code |
| SeqcParserContext | flex/bison parser state (embedded at Compiler+0x100) | Compiler ctor |
| Expression | Raw AST from parser | parse() |
| SeqCAstNode | Typed AST for lowering | toSeqCAst() |
| FrontEndLoweringFacade | Static dispatch → SeqCAstNode::lower() virtual | compile() step 4 |
| FrontendLoweringContext | Holds shared_ptrs passed to lower() | FrontEndLoweringFacade |
| FrontendLoweringState | Accumulates lowering state (vector<string>) | FrontEndLoweringFacade |
| Resources | Scope/symbol table — vars, consts, functions, registers | Compiler ctor |
| AsmCommands | Instruction emitter (83 methods) | Compiler ctor |
| CustomFunctions | SeqC built-in function handlers | Compiler ctor |
| WaveformGenerator | Waveform DSL (54 methods) | Compiler ctor |
| WavetableFront | Front-end waveform table management | External (passed in) |
| WavetableIR | IR waveform table — allocation, sizing | Created during compile |
| AsmOptimize | Peephole optimizer + register allocator | compile() steps 7, 10 |
| Prefetch | Waveform prefetch scheduling + cache | runPrefetcher() |
| Cache | Waveform cache model | Prefetch ctor |
| StaticResources | Global init (device-specific constants) | compile() step 5 |
| AWGAssembler | Not used in compile() — separate entry point for .seqc → ELF |

## Compiler Layout (~0x138 bytes)

| Offset | Size | Type | Name | Notes |
|--------|------|------|------|-------|
| +0x00 | 8 | AWGCompilerConfig const* | config_ | |
| +0x08 | 8 | DeviceConstants const* | deviceConstants_ | |
| +0x10 | 4 | int32_t | lineNr_ | Propagated to AsmCommands+WavetableFront |
| +0x14 | 2 | uint16_t | flags_ | |
| +0x18 | 8 | (reserved) | | |
| +0x20 | 4 | int32_t | channelCount_ | Set by Prefetch::getUsedChannels() |
| +0x24 | 1 | uint8_t | usedFourChannelMode_ | Set by Prefetch::getUsedFourChannelMode() |
| +0x25 | 1 | uint8_t | usedSampleRate_ | |
| +0x28 | 16 | shared_ptr<Node> | ast_ | |
| +0x38 | 32 | CompilerMessageCollection | messages_ | Inline (0x20 bytes) |
| +0x58 | 24 | vector<string> | sourceFiles_ | |
| +0x70 | 24 | vector<string> | sourceLines_ | Filled by parse() |
| +0x88 | 24 | AsmList | asmList_ | Working assembly list |
| +0xA0 | 16 | shared_ptr<WavetableFront> | wavetable_ | From ctor arg |
| +0xB0 | 16 | shared_ptr<AsmCommands> | asmCommands_ | Created in ctor |
| +0xC0 | 16 | shared_ptr<WaveformGenerator> | waveformGen_ | Created in ctor |
| +0xD0 | 16 | shared_ptr<CustomFunctions> | customFunctions_ | Created in ctor |
| +0xE0 | 16 | weak_ptr<CancelCallback> | cancelCallback_ | |
| +0xF0 | 16 | weak_ptr<ProgressCallback> | progressCallback_ | |
| +0x100 | 56 | SeqcParserContext | parserContext_ | Contains std::function for error callback |

## TLS Counter Resets

Both TLS counters are reset at the start of `compile()`:
- TLS+0x40 (Asm nextID): unconditionally reset to 0 at 0x11f19f
- TLS+0x44 (Node nodeId): conditionally reset to 0 at 0x1209c6

This confirms that sequence IDs restart from 0 for each compilation.

## Error Handling

- `CompilerMessageCollection` at Compiler+0x38 accumulates errors/warnings
- `hadCompilerError()` checked after lowering (step 4) and at the end (step 12)
- `CompilerException` thrown on fatal parse errors
- `runPrefetcher` catches `zhinst::Exception` → rethrows as `CompilerException`
- Duplicate messages are filtered by `compilerMessage()` (same line + same text)

## Debug Flags

The `AWGCompilerConfig` debug flags field (at +0x90, checked as byte):
- Bit 1 (0x02): Print old Expression AST after parsing
- Bit 2 (0x04): Print SeqC AST after conversion
- Bit 3 (0x08): Print generated tree after prefetch + final assembly
