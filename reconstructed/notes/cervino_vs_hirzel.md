# Cervino vs Hirzel Differences {#notes_cervino_vs_hirzel}

\note **Reverse-engineering reference material.** This page is part of
the `reconstructed/notes/` set: deep-dive technical notes for
contributors working on the reconstruction. It cites binary addresses,
opcodes, and disassembly observations directly so they remain
discoverable from the rendered site. The standard documentation-voice
rules for API briefs (no binary citations outside `\binarynote`) do
**not** apply to this page.

## Device Type Mapping

`AsmCommandsImpl::getInstance()` selects the implementation based on `AwgDeviceType`:

| Device Type | Value | Codename      | Implementation |
|-------------|-------|---------------|----------------|
| UHFLI       | 1     | cervino       | Cervino        |
| HDAWG       | 2     | hirzel        | Hirzel         |
| UHFQA       | 4     | klausen       | Cervino        |
| SHFQA       | 8     | grimsel_qa    | Hirzel         |
| SHFSG       | 16    | grimsel_sg    | Hirzel         |
| SHFQC_SG    | 32    | grimsel_qc_sg | Hirzel         |
| SHFLI       | 64    | grimsel_li    | Hirzel         |
| GHFLI       | 128   | gurnigel      | Cervino        |
| VHFLI       | 256   | maloja        | Cervino        |

Selection uses bitmask `0x4000000040004041` on `(val - 2)` for values 2..64,
plus explicit checks for 128 and 256.

## Virtual Method Differences

| Method            | Cervino                           | Hirzel                              |
|-------------------|-----------------------------------|-------------------------------------|
| `isCWVFRSupported`| `false`                           | `true`                              |
| `wwvfq`           | **throws** (unsupported)          | opcode `0xF0000000`                 |
| `wprf`            | opcode `0xF0000000`               | opcode `0xFFFFFFFF` (no-op)         |
| `wvf` (no marker) | opcode `0x20000000`, reg in dst+2nd | opcode `0xFA000000`, reg in slot 0  |
| `wvf` (marker)    | opcode `0x20000000`               | opcode `0x20000000` (same)          |
| `wvfi`            | opcode `0x30000000` (markerReg must be R0) | **throws** (unsupported)   |
| `wvfs`            | **throws** (unsupported)          | opcode `0x30000001`                 |
| `wvft`            | **throws** (unsupported)          | opcode `0xFC000000`                 |
| `brz` (reg==R0)   | opcode `0xF3000000` (with reg)    | opcode `0xFE000000` (no reg)        |
| `brz` (reg!=R0)   | opcode `0xF3000000`               | opcode `0xF3000000` (same)          |
| `ssl`             | reg in both dst and src slots     | reg in dst, R0 in src               |
| `ssr`             | reg in both dst and src slots     | reg in dst, R0 in src               |
| `ldiotrig`        | LD from address `0x60`            | LD from address `0x68`              |

## Summary

Hirzel devices have:
- Extended waveform commands (wvfs, wvft, wwvfq) that Cervino lacks
- A dedicated unconditional branch opcode (0xFE000000) vs Cervino using BRZ with reg
- Different shift encoding (src=R0 vs src=same reg)
- Different I/O trigger address (0x68 vs 0x60)
- CWVFR support
- Single-register wvf variant (0xFA000000)
- No wvfi (interleaved) support

Cervino devices have:
- Interleaved waveform playback (wvfi) that Hirzel lacks
- More limited instruction set overall
