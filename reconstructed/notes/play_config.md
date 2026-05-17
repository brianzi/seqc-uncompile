# PlayConfig and the CWVF Register {#notes_play_config}

\unclear This page is a Phase D7-A stub.  Phase B6 will populate it
with the description of the PlayConfig bitfield, its mapping to the
sequencer's CWVF (current-waveform configuration) memory-mapped
register, and how the frontend builds PlayConfig values from
`playWave`-family arguments.

## Scope

PlayConfig is a bitmask describing *how* a waveform is to be played:
channel routing, marker bit assertion, hold / latch behaviour,
amplitude modulation source, output enable per port, etc.  It is
synthesised by the frontend during lowering of `playWave` (and
related custom functions), travels through the optimisation passes
attached to the playback instruction, and is finally written to the
CWVF special register by the emitted sequencer code.

This page will cover:

- The PlayConfig bitfield layout and the semantics of each bit / field.
- The CWVF special register: address, width, write semantics,
  per-device-family variation.
- How `playWave` arguments (`AWG_CHANNEL_*`, marker masks,
  amplitude-routing flags, hold-down etc.) are translated into
  PlayConfig bits.
- Cross-references to `\suser` writes that materialise PlayConfig at
  runtime.

## Source material

Implementation primarily under `reconstructed/src/waveform/play_config.cpp`
and `reconstructed/include/zhinst/waveform/play_config.hpp`.

## See also

- \ref notes_special_registers — CWVF in the special-register map.
- \ref notes_custom_functions — `playWave` and friends that populate
  PlayConfig.
- \ref notes_opcode_map — `suser` / `luser` instructions used to
  write / read CWVF.
- \ref notes_magic_numbers_proposal — proposed PlayConfig bit names.
