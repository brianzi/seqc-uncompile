# `fb` — Feedback Configuration Instruction {#notes_fb_instruction}

`fb` configures the sequencer's feedback-processing pipeline: which
source channel to read from, how many bits to extract, where to
shift, and what threshold to apply.  It is the single ISA-level
configuration knob behind the SeqC `configureFeedbackProcessing`
built-in.

## Opcode & Properties

| Property        | Value |
|-----------------|-------|
| Mnemonic        | `fb` |
| Enum            | `Assembler::FB` |
| Opcode          | `0xFF000000` |
| Size            | 4 bytes |
| Cycles          | 1 |
| Format          | `opcode4`, 0 children — full word is `0xFF000000 \| immediate` |
| Device support  | Hirzel only (HDAWG, SHFQA, SHFSG, SHFQC_SG, SHFLI, GHFLI, VHFLI) |

## SeqC Surface

Emitted exclusively by the built-in function:

```c
configureFeedbackProcessing(source, shift, numBits, threshold)
```

- `source` — one of the SeqC feedback constants (see below).  May
  be a variable.
- `shift`, `numBits`, `threshold` — must all be **compile-time
  constants** (not register-bound).

## Source Argument — SeqC Constants to Mode Encoding

The `source` argument takes one of the named SeqC constants defined in
`static_resources.cpp`. Their numeric values are derived from
`base = 1 << numOutputPorts` (a per-device constant):

| Device | AwgDeviceType | numOutputPorts | base |
|--------|---------------|----------------|------|
| HDAWG | 2 | 10 | 1024 |
| SHFQA | 8 | 10 | 1024 |
| SHFSG | 16 | 12 | 4096 |
| SHFQC\_SG | 32 | 12 | 4096 |
| SHFLI | 64 | 12 | 4096 |
| GHFLI | 128 | 12 | 4096 |
| VHFLI | 256 | 12 | 4096 |

The valid source constants and their mapping to the 2-bit `mode` field:

| SeqC constant | Alias | Value | mode | Availability |
|---|---|---|---|---|
| `ZSYNC_DATA_PROCESSED_A` | `ZSYNC_DATA_PQSC_REGISTER` | base + 1 | 0 | All Hirzel devices |
| `ZSYNC_DATA_PROCESSED_B` | `ZSYNC_DATA_PQSC_DECODER` | base + 2 | 1 | All Hirzel devices |
| `QA_DATA_PROCESSED` | — | base + 4 | 2 | SHFQC\_SG only |

Note: `ZSYNC_DATA_RAW` (= base) and `QA_DATA_RAW` (= base + 3) are **not**
valid sources for `configureFeedbackProcessing`. They are accepted by
`getFeedback()` but rejected here — only processed feedback channels can
have their processing pipeline configured.

The SHFQC\_SG-exclusive third source makes sense: the SHFQC is the combo
QA+SG device, so its SG sequencer has an additional feedback path to
receive processed data from the QA side.

## Immediate Bitfield Layout

The 23-bit immediate packed into the instruction word (`0xFF000000 | imm`):

| Bits | Width | Field | SeqC arg | Constraints | Encoding |
|------|-------|-------|----------|-------------|----------|
| 22:21 | 2 | mode | derived from `source` | 0, 1, or 2 | `(mode & 0x3) << 21` |
| 20:16 | 5 | shift | `shift` | [0, 32) | `(shift & 0x1f) << 16` |
| 15:12 | 4 | numBits − 1 | `numBits` | numBits ∈ (0, 17) | `(uint16_t)((numBits << 12) + 0xf000)` |
| 11:0 | 12 | threshold | `threshold` | [0, 4096) | `threshold & 0xfff` |

The numBits encoding is slightly obfuscated in the binary: `(numBits << 12) + 0xf000`
masked to 16 bits is equivalent to `((numBits - 1) & 0xf) << 12`.

## Relationship to `getFeedback()`

`configureFeedbackProcessing` and `getFeedback` are complementary:

- **`fb`** (this instruction) **configures** the feedback processing pipeline:
  which source to read, how many bits to extract, where to shift, and what
  threshold to apply.
- **`getFeedback(source)`** **reads** feedback data by emitting `ld`
  instructions from special registers:

  | Source constant | Register | Notes |
  |---|---|---|
  | `ZSYNC_DATA_RAW` | `ldiotrig` / `ld reg, 0x6a` | 0x6a on SHFQA; ldiotrig otherwise |
  | `ZSYNC_DATA_PROCESSED_A` | `ld reg, 0x6b` | |
  | `ZSYNC_DATA_PROCESSED_B` | `ld reg, 0x6c` | |
  | `QA_DATA_RAW` | `ld reg, 0xc0` | SHFQC\_SG only |
  | `QA_DATA_PROCESSED_D` | `ld reg, 0xc1` | SHFQC\_SG only |

## See also

- \ref notes_opcode_encoding — `opcode4` family the `fb` word
  belongs to.
- \ref notes_special_registers — `0x6a`/`0x6b`/`0x6c`/`0xc0`/`0xc1`
  feedback registers read by `getFeedback`.
- \ref notes_custom_functions — `configureFeedbackProcessing`
  and `getFeedback` SeqC built-ins.
