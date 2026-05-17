# Cervino vs Hirzel Differences {#notes_cervino_vs_hirzel}

The SeqC assembler dispatches every instruction through one of two
ISA back-ends, **Cervino** (legacy) or **Hirzel** (current
generation).  The choice is made per device.

## Device Type Mapping

| Device Type | Codename      | Implementation |
|-------------|---------------|----------------|
| UHFLI       | cervino       | Cervino        |
| HDAWG       | hirzel        | Hirzel         |
| UHFQA       | klausen       | Cervino        |
| SHFQA       | grimsel_qa    | Hirzel         |
| SHFSG       | grimsel_sg    | Hirzel         |
| SHFQC_SG    | grimsel_qc_sg | Hirzel         |
| SHFLI       | grimsel_li    | Hirzel         |
| GHFLI       | gurnigel      | Cervino        |
| VHFLI       | maloja        | Cervino        |

## Virtual Method Differences

| Method            | Cervino                              | Hirzel                              |
|-------------------|--------------------------------------|-------------------------------------|
| `isCWVFRSupported`| `false`                              | `true`                              |
| `wwvfq`           | **throws** (unsupported)             | opcode `0xF0000000`                 |
| `wprf`            | opcode `0xF0000000`                  | opcode `0xFFFFFFFF` (no-op)         |
| `wvf` (no marker) | opcode `0x20000000`, reg in dst+2nd  | opcode `0xFA000000`, reg in slot 0  |
| `wvf` (marker)    | opcode `0x20000000`                  | opcode `0x20000000` (same)          |
| `wvfi`            | opcode `0x30000000` (markerReg = R0) | **throws** (unsupported)            |
| `wvfs`            | **throws** (unsupported)             | opcode `0x30000001`                 |
| `wvft`            | **throws** (unsupported)             | opcode `0xFC000000`                 |
| `brz` (reg==R0)   | opcode `0xF3000000` (with reg)       | opcode `0xFE000000` (no reg)        |
| `brz` (reg!=R0)   | opcode `0xF3000000`                  | opcode `0xF3000000` (same)          |
| `ssl`             | reg in both dst and src slots        | reg in dst, R0 in src               |
| `ssr`             | reg in both dst and src slots        | reg in dst, R0 in src               |
| `ldiotrig`        | LD from address `0x60`               | LD from address `0x68`              |

## Summary

Hirzel devices have:
- Extended waveform commands (`wvfs`, `wvft`, `wwvfq`) that Cervino
  lacks.
- A dedicated unconditional branch opcode (`0xFE000000`) vs Cervino
  using `BRZ` with reg.
- Different shift encoding (`src=R0` vs `src=same reg`).
- Different I/O trigger address (`0x68` vs `0x60`).
- `CWVFR` support.
- Single-register `wvf` variant (`0xFA000000`).
- No `wvfi` (interleaved) support.

Cervino devices have:
- Interleaved waveform playback (`wvfi`) that Hirzel lacks.
- A more limited instruction set overall.

## See also

- \ref notes_opcode_encoding — instruction word formats.
- \ref notes_opcode_map — full mnemonic table.
- \ref notes_fb_instruction — Hirzel-only `fb` instruction.
- \ref notes_custom_functions — SeqC builtins whose lowering
  diverges across the two back-ends (e.g. `wait`, `syncCervino`,
  `setSinePhase`, `resetOscPhase`).

