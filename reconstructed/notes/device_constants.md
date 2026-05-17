# DeviceConstants {#notes_device_constants}

`DeviceConstants` is the per-device hardware-configuration block
the compiler threads through every pass.  It carries the values
that vary between sequencer generations: waveform-memory geometry,
length and alignment limits, sequencer-register addresses,
sampling rate, and per-feature availability flags.

Built once per compile by `getDeviceConstants(AwgDeviceType)` and
passed around as `const DeviceConstants*`.  The pass-the-pointer
discipline is what lets the front-end, prefetch scheduler,
wavetable IR, assembler, and ELF writer share a single
consistent view of the target device.

Header / source:

- `reconstructed/include/zhinst/device/device_constants.hpp`
- `reconstructed/src/device/device_constants.cpp`

For higher-level device metadata (path patterns, ELF size limits,
on-wire sample format) see \ref notes_awg_device_props.  For the
sequencer-register-address constants nested under
`DeviceConstants::SuserAddr`, see \ref notes_special_registers.

## Per-device values

Constants that are identical across all nine device families are
listed once below the table to keep the comparison readable.

### Waveform memory & cache

| Field                | UHFLI    | UHFQA    | HDAWG     | SHFQA     | SHFSG    | SHFQC_SG | SHFLI    | GHFLI    | VHFLI    |
|----------------------|---------:|---------:|----------:|----------:|---------:|---------:|---------:|---------:|---------:|
| `waveformRegBase`    | `0x95b2` | `0x95b2` | `0x11203` | `0x110f6` | `0x11202`| `0x11202`| `0x110f6`| `0x110f6`| `0x110f6`|
| `waveformMemorySize` |`0x20000` |`0x20000` |`0x100000` |`0x100000` |`0x60000` |`0x60000` |`0x60000` |`0x60000` |`0x60000` |
| `sampleLength`       | 32       | 32       | 64        | 64        | 64       | 64       | 16       | 48       | 16       |
| `waveformAlignment`  | 4096     | 4096     | 4096      | 4096      | 4096     | 4096     | 16       | 48       | 16       |
| `cachePageCount`     | 4        | 4        | 4         | 4         | 104      | 104      | `0x6000` | `0x2000` | `0x6000` |

### Waveform-length / play-length limits

| Field                | UHFLI | UHFQA | HDAWG | SHFQA | SHFSG | SHFQC_SG | SHFLI | GHFLI | VHFLI |
|----------------------|------:|------:|------:|------:|------:|---------:|------:|------:|------:|
| `maxWaveformLength`  | 16    | 16    | 32    | 32    | 32    | 32       | 32    | 96    | 32    |
| `grainSize`          | 8     | 8     | 16    | 16    | 16    | 16       | 16    | 48    | 16    |
| `playMinSamples`     | 0     | 0     | 128   | 128   | 128   | 128      | 128   | 384   | 128   |
| `waveformMinSamples` | 16    | 16    | 32    | 32    | 16    | 16       | 16    | 96    | 16    |

### Sequencer / clock

| Field                  | UHFLI    | UHFQA    | HDAWG    | SHFQA    | SHFSG    | SHFQC_SG | SHFLI    | GHFLI     | VHFLI    |
|------------------------|---------:|---------:|---------:|---------:|---------:|---------:|---------:|----------:|---------:|
| `sequencerRegBase`     | `0x115c` | `0x115c` | `0x0d05` | `0x0d05` | `0x0d05` | `0x0d05` | `0x0d05` | `0x0d05`  | `0x0d05` |
| `maxProgramSize`       | 1024     | 1024     | 16384    | 16384    | 32768    | 32768    | 32768    | 32768     | 32768    |
| `seqClockDivider`      | 0        | 0        | 1165     | 1165     | 1165     | 1165     | 1165     | 1165      | 1165     |
| `samplingRate` (Hz)    | 1.8e9    | 1.8e9    | 2.4e9    | 2.0e9    | 2.0e9    | 2.0e9    | 2.0e9    | 12.0e9    | 2.0e9    |
| `execTableIndexBits`   | 0        | 0        | 10       | 10       | 12       | 12       | 12       | 12        | 12       |
| `numAWGCores`          | 0        | 0        | 5        | 0        | 3        | 3        | 3        | 3         | 3        |

### Feature flags

| Field             | UHFLI | UHFQA | HDAWG | SHFQA | SHFSG | SHFQC_SG | SHFLI | GHFLI | VHFLI |
|-------------------|:-----:|:-----:|:-----:|:-----:|:-----:|:--------:|:-----:|:-----:|:-----:|
| `hasExtendedReg`  |   ·   |   ·   |   ✓   |   ·   |   ·   |    ·     |   ·   |   ·   |   ·   |
| `hasDIO`          |   ·   |   ·   |   ✓   |   ✓   |   ✓   |    ✓     |   ✓   |   ✓   |   ✓   |
| `numDIOBits`      | 0     | 0     | 8     | 6     | 8     | 8        | 8     | 8     | 8     |
| `numCounters`     | 0     | 0     | 2     | 2     | 2     | 2        | 2     | 2     | 2     |
| `hasPrecomp`      |   ·   |   ·   |   ✓   |   ·   |   ✓   |    ✓     |   ✓   |   ✓   |   ✓   |

### Constants identical across every device

| Field                  | Value  | Notes                                                  |
|------------------------|-------:|--------------------------------------------------------|
| `maxBlocks`            | 2      | Cache page count for `cacheType=1`                     |
| `sineNodeBase`         | 16     | Base offset for the oscillator / sine-node index space |
| `waveformElfAlignment` | 64     | ELF segment alignment for waveform data                |
| `registerDepth`        | 16     | Sequencer register-index upper bound                   |
| `memoryDepth`          | 1024   | User-memory address upper bound                        |
| `triggerLatencyCycles` | 6      | Trigger / wait-instruction latency, in cycles          |
| `bitsPerSample`        | 16     | Bits per waveform sample                               |
| `maxSequenceLen`       | 16000  | Max sequence length (IR nodes)                         |

## Convenience accessors

| Method                    | Aliases                                                  |
|---------------------------|----------------------------------------------------------|
| `maxDioTableEntries()`    | `waveformMemorySize`                                     |
| `maxWaveIndex()`          | `static_cast<uint32_t>(maxSequenceLen)`                  |

## Unsupported devices

`getDeviceConstants(d)` throws `ZIAWGCompilerException` with the
message *"Instantiated compiler for unsupported device type"* when
`d` is not one of the nine supported families above.

## See also

- \ref notes_awg_device_props — per-device path patterns, ELF size
  limits, on-wire sample format, `isHirzel` flag.
- \ref notes_special_registers — addresses listed under
  `DeviceConstants::SuserAddr`.
- \ref notes_cervino_vs_hirzel — which device maps to which
  assembler back-end.
- \ref notes_custom_functions — SeqC builtins (`getUserReg`,
  `setUserReg`, `getCnt`, `getPRNGValue`, ...) that consult these
  per-device limits during validation.
