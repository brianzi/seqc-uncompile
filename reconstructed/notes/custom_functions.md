# SeqC Custom Functions {#notes_custom_functions}

SeqC built-in functions (informally "custom functions") are the
runtime-facing primitives a SeqC program calls directly:
`playWave`, `wait`, `setTrigger`, `setUserReg`, `configureFeedbackProcessing`,
and so on.  They are *not* statements of the language — the parser
treats every call site as an ordinary function call and the
front-end dispatches the name through `CustomFunctions::funcMap_`
to one of 76 implementation methods (plus 5 string aliases).

This page catalogues the registered built-ins by topic and points
each one at the wider documentation for the ISA primitive it
ultimately emits.  Function semantics — what users see — are
covered in the public SeqC programming guide.

## Dispatch overview

The class `CustomFunctions` owns two hash maps:

- `funcMap_` — `name → FuncType` for the 76 unique built-ins and
  their 5 aliases.
- `aliasMap_` — reserved for runtime alias injection; empty in
  the shipping binary.

When the front-end lowers a call expression it:

1. Resolves the name against `aliasMap_`, then `funcMap_`, then
   the standalone `MathCompiler` table (for `sin`, `cos`,
   `sqrt`, …).
2. Invokes the bound method with the parsed argument list
   (`std::vector<EvalResultValue>`) and the per-compilation
   `Resources` handle.
3. Receives back an `EvalResults` (zero-or-more output values
   plus any assembler text emitted as a side effect via the
   `AsmCommands` interface).

Each built-in therefore has three concerns: argument validation
(types, ranges, compile-time-constant requirements), per-device
availability (Cervino vs Hirzel — see
\ref notes_cervino_vs_hirzel), and the actual assembler text it
appends.

## Catalogue (by topic)

The unique built-ins, grouped.  Counts in parentheses are unique
methods; the registered set adds 5 aliases.

### Waveform playback (12)

| Built-in              | Notes                                                                  |
|-----------------------|------------------------------------------------------------------------|
| `playWave`            | Primary playback entry; emits `wvf` (Cervino) / `wvfe` (Hirzel single-reg) |
| `playWaveNow`         | `playWave` variant emitted on the "now" timeline                        |
| `playWaveIndexed`     | Indexed playback (Cervino: `wvfi`)                                     |
| `playWaveIndexedNow`  | `playWaveIndexed` on the "now" timeline                                |
| `playAuxWave`         | Auxiliary playback path                                                |
| `playAuxWaveIndexed`  | Indexed auxiliary playback                                             |
| `playDIOWave`         | Playback triggered by DIO state                                        |
| `playWaveDIO`         | Playback gated on a DIO mask; emits `wvft` (Hirzel)                    |
| `playWaveZSync`       | Playback triggered by ZSync; emits `wvft` (Hirzel)                     |
| `playWaveDigTrigger`  | Playback gated on a digital-trigger index                              |
| `playZero`            | Idle samples (zeros) for a given length                                |
| `playHold`            | Hold the last sample for a given length                                |

All twelve update the **PlayConfig** register before the playback
opcode is emitted; the bitfield layout is covered in
\ref notes_play_config.

### Wave-table management (3)

| Built-in           | Notes                                                                  |
|--------------------|------------------------------------------------------------------------|
| `assignWaveIndex`  | Bind a name → wave-table slot (compile-time, no asm)                   |
| `prefetch`         | Schedule a wave for prefetch into cache; emits `prf`                   |
| `prefetchIndexed`  | Indexed prefetch                                                       |

See \ref notes_prefetch_scheduling for the prefetch sub-pipeline.

### Waits & barriers (13)

| Built-in              | Notes                                                                                 |
|-----------------------|---------------------------------------------------------------------------------------|
| `wait`                | Wait `n` cycles; cycle count goes via the wait-cycles register (`suser …,0x69`)        |
| `waitWave`            | Wait for current playback to finish; emits `wwvf`                                     |
| `waitTrigger`         | Wait for trigger register match                                                       |
| `waitAnaTrigger`      | Wait for analog-trigger match                                                         |
| `waitDigTrigger`      | Wait for digital-trigger index match                                                  |
| `waitOnGrid`          | Wait until a sample-grid boundary                                                     |
| `waitOnSync`          | Wait on the sync register (`st R0, 0x92` on LI-family)                                |
| `waitDIOTrigger`      | Wait for DIO trigger                                                                  |
| `waitZSyncTrigger`    | Wait for ZSync trigger                                                                |
| `waitCntTrigger`      | Wait for hardware-counter trigger                                                     |
| `waitDemodOscPhase`   | Wait for demodulator oscillator phase                                                 |
| `waitSineOscPhase`    | Wait for sine-osc phase                                                               |
| `waitTimestamp`       | Wait until absolute timestamp (HDAWG, register `0x1B`)                                |

The `waitDIOTrigger`, `waitZSyncTrigger`, `waitDigTrigger`,
`waitCntTrigger`, `waitOnGrid` and `waitSineOscPhase` family lower
into the `st(R0, 0x40 + N)` "wait-trigger placeholder" pattern; see
\ref notes_special_registers §6.

### Triggers & I/O (10)

| Built-in              | Notes                                                                  |
|-----------------------|------------------------------------------------------------------------|
| `setTrigger`          | Write trigger register `0x22`                                          |
| `getTrigger`          | Read trigger register `0x22`                                           |
| `setInternalTrigger`  | Write internal-trigger register `0x23` (LI-family)                     |
| `getAnaTrigger`       | Read analog-trigger value                                              |
| `getDigTrigger`       | Read digital-trigger value                                             |
| `setDIO`              | Write DIO register `0x20` (or `0x1FE` high bank)                       |
| `getDIO`              | Read DIO register                                                      |
| `getDIOTriggered`     | Read I/O-trigger register `0x60`/`0x68` (Cervino / Hirzel)             |
| `setID`               | Write Set-ID register `0x21` (or `0x1FF` high bank)                    |
| `setWaveDIO`          | DIO-gated waveform set-up                                              |

Aliases registered for compatibility:

| Alias                          | Resolves to        |
|--------------------------------|--------------------|
| `setSeqIndex`                  | `setID`            |
| `setReadoutRegisterAddress`    | `setID`            |
| `setUser`                      | `setUserReg`       |
| `getUser`                      | `getUserReg`       |
| `waitOscPhaseOfDemod`          | `waitDemodOscPhase`|

### User / sweeper registers (3)

| Built-in            | Notes                                                                  |
|---------------------|------------------------------------------------------------------------|
| `setUserReg`        | Write user-register slot `0..0x3FF`                                    |
| `getUserReg`        | Read user-register slot                                                |
| `getSweeperLength`  | Read sweeper length from user-register space                           |

### Node-tree access (2)

| Built-in   | Notes                                                                          |
|------------|--------------------------------------------------------------------------------|
| `setInt`   | Write a node-tree path with an integer value (multi-instruction write protocol) |
| `setDouble`| Write a node-tree path with a double value (extra high-word store)              |

Both lower through the `writeToNode` helper, emitting up to four
`st` instructions against registers `0x10`/`0x11`/`0x12`/`0x13`/
`0x16`/`0x17`/`0x19` (the write protocol; see
\ref notes_special_registers §1).

### Oscillator & phase (4)

| Built-in              | Notes                                                                  |
|-----------------------|------------------------------------------------------------------------|
| `resetOscPhase`       | Pulse register `0x5F` (Hirzel)                                         |
| `setSinePhase`        | Write `0x70`/`0x71`                                                    |
| `incrementSinePhase`  | Write `0x72`/`0x73`                                                    |
| `setOscFreq`          | Write the sweep-freq register pair (SHF+)                              |

### Frequency sweep (2)

| Built-in           | Notes                                                                  |
|--------------------|------------------------------------------------------------------------|
| `configFreqSweep`  | Configure SHF+ sweep (writes `0x8C`..`0x91`)                           |
| `setSweepStep`     | Step the sweep                                                         |

### Quantum-analyzer pipeline (5)

| Built-in              | Notes                                                                  |
|-----------------------|------------------------------------------------------------------------|
| `startQA`             | Composite start (preferred entry)                                      |
| `startQAResult`       | Start result accumulation only                                         |
| `startQAMonitor`      | Start monitor channel only                                             |
| `getQAResult`         | Read QA result register (UHFQA, `0x61`)                                |
| `waitQAResultTrigger` | Wait for QA result trigger                                             |

### Feedback (2)

| Built-in                          | Notes                                                  |
|-----------------------------------|--------------------------------------------------------|
| `configureFeedbackProcessing`     | Emit `fb` instruction; see \ref notes_fb_instruction   |
| `getFeedback`                     | Read one of the feedback registers `0x6A..0x6C`/`0xC0`/`0xC1` |

### ZSync (1)

| Built-in        | Notes                                                                        |
|-----------------|------------------------------------------------------------------------------|
| `getZSyncData`  | Read ZSync data; routes through the same register set as `getFeedback`       |

### PRNG (3)

| Built-in        | Notes                                                                        |
|-----------------|------------------------------------------------------------------------------|
| `setPRNGSeed`   | Write PRNG seed register `0x74`                                              |
| `setPRNGRange`  | Write PRNG range registers `0x75`/`0x76`                                     |
| `getPRNGValue`  | Read PRNG value register `0x77`                                              |
| `randomSeed`    | Compile-time RNG seed for the `generate` family                              |
| `generate`      | Compile-time waveform generation (no asm)                                    |

### Counters, timing & misc (8)

| Built-in                      | Notes                                                                  |
|-------------------------------|------------------------------------------------------------------------|
| `getCnt`                      | Read hardware loop counter `0x64`/`0x65` (HDAWG)                       |
| `executeTableEntry`           | Execute command-table entry; emits `wvft`                              |
| `setRate`                     | Set playback sample rate                                               |
| `setPrecompClear`             | Toggle pre-compensation clear flag                                     |
| `waitDemodSample`             | Wait for next demod sample                                             |
| `waitPlayQueueEmpty`          | Wait for play queue drain                                              |
| `now`                         | Return current "now" timestamp                                         |
| `at`                          | Schedule an RT-logger output                                           |
| `error`                       | Compile-time error                                                     |
| `info`                        | Compile-time info / diagnostic                                         |
| `lock` / `unlock`             | Pair guarding critical sections                                        |
| `sync`                        | Multi-core sync (Cervino: `0x44`/`0x45`; Hirzel: `0x6E`)               |
| `resetRTLoggerTimestamp`      | Reset RT logger time base (`0x6D`, UHFQA: `0x62`)                      |

## Validation patterns

Three validation helpers recur across the catalogue:

- **`checkPlayMinLength`** — minimum sample-count for playback;
  device-dependent.
- **`checkPlayAlignment`** — sample-count alignment grid; device-
  dependent.
- **`checkOffspecWaveLength`** — warning when a waveform's length is
  outside the device's documented operating range but still legal.

`checkFunctionSupported(devType)` is consulted at the head of every
built-in that is not available on every device family — the table
of supported devices is built into each method.

## Special argument forms

- **PlayArgs** — wraps the argument list for the `playWave` family.
  Handles channel-group expansion (explicit `1,w1, 2,w2` vs implicit
  channel-per-arg), marker handling (sub-type 2 waveforms), and
  wave-name → wave-table-index assignment.
- **`parseOptionalRate` / `getPlayRate`** — extracts the optional
  trailing `AWG_RATE_xxx` argument from a playback call and updates
  the PlayConfig rate field.
- **`parseOptionalString`** — pops a trailing string literal from the
  argument list, used by labelling / diagnostic built-ins.

## Source

`reconstructed/include/zhinst/runtime/custom_functions.hpp` declares
the full class and the per-method signatures.  The registration
block is in `reconstructed/src/runtime/custom_functions.cpp` (the
`CustomFunctions::CustomFunctions()` body).  Per-family
implementations are split across:

| File                                | Topic                                |
|-------------------------------------|--------------------------------------|
| `custom_functions.cpp`              | Dispatch, validation helpers, misc   |
| `custom_functions_play.cpp`         | Playback family                      |
| `custom_functions_playback.cpp`     | Playback support (PlayArgs etc.)     |
| `custom_functions_wait.cpp`         | Wait family                          |
| `custom_functions_dio.cpp`          | DIO, triggers, ZSync, feedback       |
| `custom_functions_registers.cpp`    | User registers, node-tree writes     |

## See also

- \ref notes_seqc_parser_grammar — how a `playWave(...)` call site
  is parsed.
- \ref notes_frontend_lowering — how a parsed call is dispatched
  into the `CustomFunctions` map.
- \ref notes_play_config — the PlayConfig register that every
  playback-related built-in updates.
- \ref notes_fb_instruction — the `fb` opcode emitted by
  `configureFeedbackProcessing`.
- \ref notes_special_registers — every ld/st address mentioned
  above, with the originating built-in.
- \ref notes_waveform_generator_funcmap — the *DSL* side
  (`zeros`, `sine`, `gauss`, …) — distinct from custom functions:
  DSL functions build waveform sample arrays at compile time;
  custom functions emit runtime sequencer instructions.
