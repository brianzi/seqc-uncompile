# SeqC Custom Functions {#notes_custom_functions}

\unclear This page is a Phase D7-A stub.  Phase B4 will populate it
with the catalogue of SeqC built-in (custom) functions registered in
`CustomFunctions::funcMap_`, their signatures, the machine-code
emission strategy for each, and how arguments lower into PlayConfig
bits, immediate operands, or runtime register writes.

## Scope

SeqC built-ins are the runtime-facing functions a SeqC program calls
directly — `playWave`, `setTrigger`, `wait`, `waitWave`, `setUserReg`,
`getUserReg`, `playZero`, `setOscFreq`, `setDIO`, `getDIOTriggered`,
and so on.  They are not statements of the language; the parser sees
them as ordinary function calls, and `CustomFunctions` decides what
each one emits.

This page will cover, per function family:

- The SeqC-visible signature(s) and overloads.
- The emit strategy: which assembly instructions are produced (and
  under what conditions: e.g. immediate vs. register operand).
- Per-device-family availability and behaviour differences (Cervino
  vs Hirzel — see \ref notes_cervino_vs_hirzel).
- Interaction with the PlayConfig / CWVF register
  (\ref notes_play_config) for waveform-playback builtins.

## Source material

Implementation lives under `reconstructed/src/runtime/`.  Authoritative
file is `reconstructed/src/runtime/custom_functions.cpp` with the
function-pointer map populated in the `CustomFunctions` constructor.

## See also

- \ref notes_seqc_parser_grammar — how a `playWave(...)` call site is
  parsed.
- \ref notes_frontend_lowering — how a parsed call is dispatched into
  the `CustomFunctions` map.
- \ref notes_play_config — the PlayConfig register that many
  playback-related builtins update.
- \ref notes_waveform_generator_funcmap — the *DSL* side
  (`zeros`, `sine`, `gauss`, …), distinct from custom functions:
  DSL functions build waveform sample arrays at compile time;
  custom functions emit runtime sequencer instructions.
