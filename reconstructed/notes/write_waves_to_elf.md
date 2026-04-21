# writeWavesToElf Analysis

## Functions

Two anonymous-namespace free functions that write waveform data into ELF output:

| Function | Lambda operator() | Lambda vtable | Captures (bytes) |
|---|---|---|---|
| `writeWavesToElfMapped` | 0x10e020 | 0xb025c0 | 0x18 (ElfWriter*, config*) |
| `writeWavesToElfAbsolute` | 0x10e190 | 0xb02650 | 0x20 (uint32_t*, ElfWriter*, config*) |

## Parent Function Locations

Both functions are **fully inlined** into `AWGCompilerImpl::writeToStream()` @ 0x108cc0.
There are no standalone function bodies — only the lambda vtable/operator() code exists.

The inlining sites within writeToStream:
- **Mapped path**: 0x108d55 → 0x108dbb (branch: `AWGCompilerImpl+0x88 == true`)
- **Absolute path**: 0x108dee → 0x108e6e (branch: `AWGCompilerImpl+0x88 != true`)

## Key Differences

| Aspect | Mapped | Absolute |
|---|---|---|
| `addWaveform` mapped param | `true` | `false` |
| padSize param | Always 0 | Computed per-waveform |
| WaveOrder | ByName (1) | ByIndex (2) |
| Tracks offset | No | Yes (cumulative) |
| Skips unused | No (relies on forEachUsed) | Yes (explicit check) |
| Uses return value | No (destroys rawData) | Yes (rawData->size() for offset tracking) |

## Absolute Padding Calculation

```
gap = waveform->addressValue - currentOffset
alignMask = -waveform->irField2    // e.g., irField2=64 → mask=0xFFFFFFC0
padding = gap & alignMask          // rounds gap down to alignment boundary
```

After addWaveform: `currentOffset = waveform->addressValue + rawData->size()`

## AWGCompilerConfig::unknown_04 = SampleFormat

Both lambdas load `config+0x04` as the SampleFormat parameter (ecx) to addWaveform.
This field was previously listed as `unknown_04` — it is actually the sample format.

## Signal+0x50 = Data Pointer Check

The absolute lambda checks `waveformIR+0xd0` (= Waveform base 0x80 + Signal offset 0x50)
for null. If null, the waveform has no loaded data and is skipped.

## addWaveform Return Type (RESOLVED in Phase 10.5b)

`ElfWriter::addWaveform` returns `std::unique_ptr<RawWave>` via sret. This is the
same unique_ptr produced by `Signal::getRawData(format)` — it passes through
addWaveform and is returned to the caller. The returned object:
- Has a vtable (RawWave: slot 2 = size(), slot 3 = ptr())
- vtable+0x08 = deleting destructor
- vtable+0x10 = size() → returns byte count of the raw waveform data

The absolute lambda uses `rawData->size()` to update the cumulative offset.
The mapped lambda simply destroys the returned unique_ptr immediately.

Additionally, the NOBITS path in addWaveform uses `set_size(rawDataSize)` on the
section (not `set_link(sectionCount)` as previously reconstructed).

## writeToStream Context

```
writeToStream(ostream& os, string const& filename):
  1. Check for syntax errors
  2. Get opcodes from AWGAssembler
  3. Create ElfWriter(machineType=2)
  4. Set memory offset from config
  5. Branch on AWGCompilerImpl+0x88 (bool mappedWaveforms):
     if mapped → writeWavesToElfMapped(config, wavetableIR, elfWriter)
     else      → writeWavesToElfAbsolute(config, wavetableIR, elfWriter)
  6. Call elfWriter.addCode(opcodes)
  7. Set filename, write to stream
```

## forEachUsedWaveform Callers

Address 0x29e5e0 is called from:
- 0x108db6 — writeWavesToElfMapped (inlined in writeToStream)
- 0x108e58 — writeWavesToElfAbsolute (inlined in writeToStream)
- 0x10a23e — getJsonWaveformMemoryInfo
- 0x29e3ec, 0x29e4b1 — within WavetableIR methods
- 0x29e8df — assignWaveIndexImplicit
- 0x29ee0d, 0x29ee67 — updateWaveforms / allocateWaveformsForFifo
- 0x29f177, 0x29f234 — alignWaveformSizes / assignWaveformAllocationSizes
- 0x29f539 — unknown caller
