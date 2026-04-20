# Cervino vs Hirzel Differences

## Device Type Mapping

`AsmCommandsImpl::getInstance()` selects the implementation based on `AwgDeviceType`:

| Device Type | Value | Implementation |
|-------------|-------|----------------|
| HDAWG8      | 2     | Hirzel         |
| HDAWG4      | 8     | Hirzel         |
| UHFQA       | 16    | Hirzel         |
| UHFAWG      | 32    | Hirzel         |
| UHFLI       | 64    | Hirzel         |
| SHFQA       | 128   | Hirzel         |
| SHFSG       | 256   | Hirzel         |
| (all other) | *     | Cervino        |

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
