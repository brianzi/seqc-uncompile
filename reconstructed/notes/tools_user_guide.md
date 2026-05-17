# Toolchain User Guide {#notes_tools_user_guide}

User-facing reference manual for the `seqcc` / `seqas` / `seqdump`
command-line tools as shipped in the current build (driver version
**`0.11.0-T10a`**).  Anything not listed here is either not yet
implemented or has been deferred — see \ref notes_tools_design for
the full forward-looking design and the `tools/seqcc/DESIGN.md`
source-tree document for the deferred roadmap.

[TOC]

## NAME

`seqcc` — SeqC toolchain driver.
`seqas` — assembler entry alias (`seqcc -x asm`; placeholder symlink).
`seqdump` — SeqC compiler ELF inspector.

## SYNOPSIS

```
seqcc   [options] <input.seqc>
seqdump [options] <elf-file>
seqdump --diff <other.elf> <elf-file>
```

`seqcc` accepts **exactly one** positional input.  The output path
defaults to `<input-basename>.elf` and can be overridden with
`-o <path>` (use `-` for stdout).

## DESCRIPTION

`seqcc` compiles a SeqC source file to a SeqC compiler ELF (a custom
ELF32-LE container — *not* a standard executable; see \ref
notes_elf_format).  It is a thin orchestrator over the reconstructed
`zhinst::Compiler` and `zhinst::AWGCompiler` pipeline — every stage
is reached by calling existing reconstructed code.

`seqdump` decodes a compiled ELF and prints each section in a
human-readable form.  It mirrors the layout of `tests/dump_elf.py`
and is read-only.

The optional `--to=<stage>` flag short-circuits the pipeline and
emits the named intermediate IR instead of the linked ELF.  The
`-S` and `-E` flags are clang-style sugar for the two most common
short-circuits.

## OPTIONS — `seqcc`

### Input and output

| Flag                  | Description |
|-----------------------|---|
| *(positional)*        | Input `.seqc` source file (exactly one). |
| `-o`, `--output FILE` | Output path.  Default: `<input>.elf` (or stage-appropriate suffix when `--to=` is used).  Use `-` for stdout. |

### Target architecture

| Flag                            | Description |
|---------------------------------|---|
| `--march DEVTYPE`               | Target device type.  One of: `HDAWG4`, `HDAWG8`, `SHFQA2`, `SHFQA4`, `SHFSG2`, `SHFSG4`, `SHFSG8`, `SHFQC`, `SHFLI`, `UHFLI`, `UHFQA`, `UHFAWG`, `GHFLI`, `VHFLI`.  Also accepts the gcc-style `-march=DEVTYPE` form.  **Required.** |
| `--sequencer {auto,qa,sg}`      | Sequencer selector for SHF-family devices.  Default `auto`. |
| `--samplerate HZ`               | Device sample rate in Hz.  **HDAWG only** — the binary rejects this field for any other device family with `"'samplerate' is relevant for HDAWG only."` |
| `--index UINT`                  | AWG core index (zero-based).  Default `0`. |
| `--mdevopt FEATURE`             | Device feature flag; repeatable.  Example: `-mdevopt=MF -mdevopt=ME` for HDAWG multi-frequency + multi-execution. |
| `--mdevopts STR`                | Deprecated.  Packed device options (newline-separated).  Prefer the repeatable `-mdevopt=FEATURE` form. |
| `--mtune KEY=VALUE`             | Escape-hatch JSON kwarg passed through to the underlying `compile_seqc()` call; repeatable.  Prefer the dedicated `--sequencer` / `--samplerate` / `--filename` / `--wave-path` / `--waveforms` flags when available. |

### Optimisation

The `-O` and `-f` flags are gcc-style and are consumed before option
parsing (they will not appear in `--help`).  Run `seqcc --print-passes`
for the live pass list and bit values.

| Flag                                   | Effect |
|----------------------------------------|---|
| `-O0`                                  | `optimizationFlags = 0x00` — every pass disabled. |
| `-O1`                                  | `optimizationFlags = 0x05` — `jump-elim` + `dead-code-elim`. |
| `-O2`                                  | `optimizationFlags = 0x1F` — all currently-defined passes. |
| `-O3`                                  | `optimizationFlags = 0xFF` — gcc parity (alias for `-O2` today; reserved for future passes). |
| `-f<pass>` / `-fno-<pass>`             | Toggle an individual optimisation pass on/off. |

Pass names accepted by `-f` / `-fno-`:

| Name             | Bit  | Effect                                  |
|------------------|------|-----------------------------------------|
| `jump-elim`      | 0x01 | Collapse trivial branch targets.        |
| `label-cleanup`  | 0x02 | Drop unreferenced asm labels.           |
| `dead-code-elim` | 0x04 | Mark unused instructions `INVALID`.     |
| `reg-zero-merge` | 0x08 | Coalesce R0/RW0 reads.                  |
| `reg-alloc`      | 0x10 | Assign physical registers.              |

Multiple toggles compose with `-O<n>`; rightmost wins on conflict
(e.g. `-O0 -fdead-code-elim` ⇒ `0x04`).

### Pipeline stop point

The pipeline is staged.  By default `seqcc` runs to `link` and emits
an ELF; the flags below stop earlier and emit the named IR instead.

| Flag                  | Effect |
|-----------------------|---|
| `-c` *(default)*      | Compile to ELF.  Equivalent to `--to=link`.  Provided for symmetry and to override an earlier `-S` / `-E`. |
| `-S`                  | Stop after assembly.  Emit the `.seqasm` text from the ELF's `.asm` section.  Sugar for `--to=asm`. |
| `-E`                  | Stop after frontend lowering.  Emit the lowered SeqC AST as JSON.  Sugar for `--to=lower`. |
| `--to STAGE`          | Stop after `STAGE`.  See *Pipeline stages* below. |

#### Pipeline stages

Run `seqcc --print-stages` for the live list with current
supported/unsupported status.  As of `0.11.0-T10a`:

| Stage      | Status        | Description                                                                       |
|------------|---------------|-----------------------------------------------------------------------------------|
| `parse`    | unsupported   | flex + bison parse → expression tree.                                             |
| `astgen`   | unsupported   | Build SeqC AST from parsed expressions.                                           |
| `lower`    | **supported** | Frontend lowering → Node IR.  Alias: `-E`.  Output: JSON.                         |
| `opt-pre`  | unsupported   | Pre-waveform `AsmOptimize` passes.                                                |
| `prefetch` | unsupported   | Waveform prefetch + `WavetableIR` build.                                          |
| `opt-post` | unsupported   | Post-waveform `AsmOptimize` passes.                                               |
| `asm-pre`  | **supported** | Pre-prefetch `AsmList` serialised via `AsmList::serialize()` (natural round-trip cut). |
| `asm`      | **supported** | Assembled `.seqasm` text (extracted from the ELF's `.asm` section).  Alias: `-S`. |
| `assemble` | unsupported   | `AsmList` → encoded opcode bytes.                                                 |
| `link`     | **supported** | Default: full ELF compile.                                                        |

Stages flagged `unsupported` are reserved for future driver releases.

### Inputs and waveform search

| Flag                  | Description |
|-----------------------|---|
| `--filename PATH`     | Source-filename hint recorded in the compile report.  Defaults to the basename of the input file. |
| `--wave-path DIR`     | Waveform CSV search directory.  Default: built-in `ZiData/awg/waves`. |
| `--waveforms A;B;C`   | Semicolon-separated list of CSV waveform names to preload from `--wave-path`. |

### Dump artefacts

`--dump=<kind>` emits an intermediate artefact alongside the compile
output.  Each dump is written to `<output-basename>.<kind>.<ext>`
unless an explicit `:PATH` suffix is given.  Repeatable.

| Flag                      | Description |
|---------------------------|---|
| `--dump KIND[:PATH]`      | Emit an intermediate artefact.  See *Dump kinds* below. |
| `--dump-dir DIR`          | Directory for `--dump` outputs that have no explicit `:PATH`.  Default: alongside the compile output. |
| `--dump-format FMT`       | Format hint: `auto` (default), `json`, `text`.  Currently advisory — each implemented dump kind emits its native format regardless. |

#### Dump kinds (shipped)

| Kind             | Native format | Filename suffix         | Content |
|------------------|---------------|-------------------------|---|
| `compile-report` | JSON          | `.compile-report.json`  | Same shape as the Python `info_dict` returned by `compile_seqc()`. |
| `asm`            | text          | `.asm.asm`              | Assembler listing extracted from the ELF's `.asm` section. |
| `waveforms`      | JSON          | `.waveforms.json`       | Per-waveform metadata index (channels, marker bits, play config), from `.waveforms` ELF section. |
| `wavemem`        | JSON          | `.wavemem.json`         | Wave-memory accounting, from `.wavemem` ELF section. |

Additional kinds (`ast-lowered`, `wavetable-ir`, `prefetch`, `cache`,
`resources`, individual `asm-*` cuts, per-pass `--dump-after=` /
`--dump-before=` taps) are designed but not yet wired in this build.

### Diagnostics

| Flag                | Effect |
|---------------------|---|
| `-h`, `--help`      | Print usage and exit. |
| `--version`         | Print driver version (e.g. `seqcc 0.11.0-T10a`) and exit. |
| `--print-stages`    | List pipeline stages (with supported/unsupported flag) and exit. |
| `--print-passes`    | List `AsmOptimize` passes (with bit values) and the curated `-O` levels, then exit. |

## OPTIONS — `seqdump`

`seqdump` is an ELF inspector.  It never invokes the compiler.

| Flag                    | Description |
|-------------------------|---|
| *(positional)*          | Path to a compiled ELF. |
| `--section NAME`        | Dump only the named section.  Repeatable.  Default: every section. |
| `--all`                 | Dump every section (explicit form). |
| `--diff OTHER.elf`      | Compare against another ELF; print only differing sections side-by-side. |
| `--max-lines N`         | Truncate text-section dumps to *N* lines.  Default `200`. |
| `-h`, `--help`          | Print usage and exit. |
| `--version`             | Print `seqdump` version. |

## INPUT FORMAT

### `.seqc` source

UTF-8 SeqC source code.  The compiler unifies line endings
(`\r\n` / `\r` → `\n`) before parsing.  Language reference is the
LabOne user manual; the grammar is reconstructed in
\ref notes_seqc_parser_grammar.

### Waveform CSVs

When the source references `placeholder()`-style preloaded
waveforms, `seqcc` resolves them through `--wave-path` and
`--waveforms` exactly like the Python `compile_seqc()`
`wavepath` / `waveforms` kwargs.

## OUTPUT FORMAT

### Default: SeqC compiler ELF (`--to=link`)

Custom **ELF32 little-endian** container (EI_CLASS=1, EI_DATA=1).
The most-touched sections:

| Section          | Format                                                          | Content                              |
|------------------|-----------------------------------------------------------------|--------------------------------------|
| `.text`          | binary, 4-byte LE words                                         | Sequencer machine code.              |
| `.asm`           | text (optionally zlib-compressed level 9)                       | Assembly listing of `.text`.         |
| `.linenr`        | binary, 16-byte records (`int32 × 4`)                           | Source-map; one record per emitted instruction. |
| `.waveforms`     | JSON                                                            | Per-waveform metadata index.         |
| `.wf_<name>`     | binary, format determined by `SampleFormat`                     | Raw waveform sample bytes.           |
| `.wavemem`       | JSON                                                            | Wave-memory accounting.              |
| `.channels`      | binary, array of int32 (conditional on non-empty channel info)  | Channel info.                        |

`e_entry` encodes the sequencer instruction memory entry point —
typically `0x80000000 + instruction_offset` for HDAWG and SHF
families.  See \ref notes_elf_format and \ref notes_elf_reader for
authoritative section-by-section schemas.

### Short-circuit outputs

| Flag         | Format                                                       |
|--------------|--------------------------------------------------------------|
| `-S` / `--to=asm`     | Plain `.seqasm` text (from the ELF's `.asm` section). |
| `-E` / `--to=lower`   | JSON object/array (`Node::toJson` shape).             |
| `--to=asm-pre`        | Plain `.seqasm` text (`AsmList::serialize()` form; pre-prefetch cut). Round-trippable. |

## EXIT STATUS

| Code | Meaning |
|------|---------|
| `0`  | Success. |
| `2`  | Usage error (e.g. missing required `--march`, unknown flag, wrong number of positionals). |
| ≠0   | Compilation error.  Compiler diagnostics are written to stderr. |

## EXAMPLES

### Compile to ELF

```bash
seqcc --march=HDAWG8 --samplerate=2.4e9 program.seqc -o program.elf
```

### Inspect the ELF

```bash
seqdump program.elf
seqdump --section=.asm --section=.waveforms program.elf
seqdump --diff baseline.elf program.elf
```

### Get the lowered AST

```bash
seqcc -E --march=HDAWG8 --samplerate=2.4e9 program.seqc -o program.ast.json
```

### Get the `.seqasm` listing

```bash
seqcc -S --march=HDAWG8 --samplerate=2.4e9 program.seqc -o program.asm
```

### Compile with no optimisation

```bash
seqcc --march=HDAWG8 --samplerate=2.4e9 -O0 program.seqc -o program.O0.elf
```

### Disable a single pass under default `-O`

```bash
seqcc --march=HDAWG8 --samplerate=2.4e9 -fno-jump-elim program.seqc -o program.elf
```

### Compile for an SHFQA in QA mode

```bash
seqcc --march=SHFQA4 --sequencer=qa program.seqc -o program.elf
```

### Capture every shipped dump artefact

```bash
seqcc --march=HDAWG8 --samplerate=2.4e9 program.seqc -o program.elf \
      --dump=compile-report --dump=asm \
      --dump=waveforms --dump=wavemem
# Produces program.elf, program.compile-report.json, program.asm.asm,
#          program.waveforms.json, program.wavemem.json
```

### Preload pre-compiled waveforms

```bash
seqcc --march=HDAWG8 --samplerate=2.4e9 \
      --wave-path=$HOME/labone/waves \
      --waveforms="pulse_a;pulse_b;chirp" \
      program.seqc -o program.elf
```

## ENVIRONMENT

`seqcc` consults no environment variables.  Waveform discovery uses
`--wave-path`; if unset, the built-in `ZiData/awg/waves` default
applies (same default as the Python binding).

## FILES

| Path                                   | Role |
|----------------------------------------|------|
| `<input>.elf`                          | Default ELF output. |
| `<output-basename>.<kind>.<ext>`       | `--dump=` artefacts (see *Dump kinds* above). |
| `<wave-path>/<name>.csv`               | Preloaded waveform CSVs resolved via `--wave-path` + `--waveforms`. |

## SEE ALSO

- \ref notes_tools_design — design rationale, scope, and internals.
- \ref notes_tools_testing — test harness and CI gates.
- \ref notes_pipeline — full compilation pipeline.
- \ref notes_optimization_passes — `-O` / `-f` pass semantics.
- \ref notes_elf_format — ELF output format.
- \ref notes_elf_reader — `.linenr` record format.
- `tools/seqcc/DESIGN.md` (source tree) — full upstream design
  document, including the forward-looking roadmap.

## BUGS

This is a build of a reverse-engineered compiler.  Differential
testing against the original binary is the primary correctness
backstop — see \ref notes_tools_testing.  Report issues against the
project's tracker; include the failing input, the `--march` / device
flags, and the driver version (`seqcc --version`).
