# Prefetch Scheduling {#notes_prefetch_scheduling}

\unclear This page is a Phase D7-A stub.  Phase B6 will populate it
with the design of the waveform-prefetch sub-pipeline (step 9 of
`Compiler::compile()`).

## Scope

The prefetcher decides *when* each waveform's sample data is loaded
into the sequencer's local waveform cache, ahead of the `playWave`
instruction that needs it.  It is the bridge between the program's
*logical* waveform references and the physical wave-memory layout
emitted into the `.wavemem` / `.wf_<name>` ELF sections.

This page will cover:

- The prefetch sub-pipeline (preparation, slot assignment, instruction
  emission) and how it composes with the surrounding optimisation
  passes.
- The waveform cache model: capacity, replacement policy, alignment
  constraints, per-device-family variation.
- Slot assignment: how waveforms get a stable cache slot across all
  uses within a command list.
- Emit protocol: which sequencer instructions are produced to issue
  the prefetch (and when they are issued relative to the consumer
  instruction).
- Interaction with wave-memory accounting (the `.wavemem` JSON section,
  see \ref notes_elf_format).

## Source material

Implementation lives under `reconstructed/src/codegen/`; the
top-level entry point is `Compiler::runPrefetcher()`.  Header:
`reconstructed/include/zhinst/codegen/prefetch.hpp`.

## See also

- \ref notes_pipeline — pipeline context; prefetch is step 9.
- \ref notes_play_config — PlayConfig bits the prefetcher honours.
- \ref notes_elf_format — `.wavemem`, `.wf_<name>` sections.
