# WaveformFront Field Map — Detailed Analysis

## Object Sizing

- `__shared_ptr_emplace<WaveformFront>::__on_zero_shared_weak()` calls
  `operator delete(this, 0x110)` — total allocation = 0x110 = 272 bytes.
- shared_ptr_emplace header = 0x18 bytes (vtptr + shared_count + weak_count).
- **Object size = 0xF8 = 248 bytes.**

## Inheritance

`WaveformFront` inherits from `Waveform`.
- `Waveform::Waveform(shared_ptr<Waveform>, string)` @ 0x114f10 handles base init.
- `Waveform::~Waveform()` @ 0x1152e0 handles base destruction.
- The copy-with-rename ctor at 0x2a2510 first calls `Waveform::Waveform(shared_ptr, string)`
  to initialize the base, then sets WaveformFront-specific fields (+0xD8 onward).

## Complete Field Map

### Waveform Base (+0x00 to +0xD7, 0xD8 = 216 bytes)

| Offset | Size | Type | Evidence | Field Name |
|--------|------|------|----------|------------|
| +0x00 | 24 | std::string | name moved in Waveform ctor; printed in toString | `name` |
| +0x18 | 4 | int32 (enum) | copied at 0x114f43; switch in toString (0=CSV,1=RAW,2=GEN) | `waveformType` |
| +0x1C | 4 | padding | — | — |
| +0x20 | 24 | std::string | copy-constructed at 0x114f4a-0x114f70 | `secondaryName` |
| +0x38 | 16 | shared_ptr | copied+refcounted at 0x114f78-0x114f89; freed in dtor | `file` |
| +0x48 | 1 | bool | movzbl at 0x114f91 | `flagAt48` |
| +0x49 | 3 | padding | — | — |
| +0x4C | 4 | int32 | mov at 0x114f98 | `fieldAt4C` |
| +0x50 | 24 | std::string | copy-constructed at 0x114fa2-0x114fca; freed in dtor | `thirdString` |
| +0x68 | 8 | uint64_t | mov qword at 0x114fd2 (but newEmpty writes two int32s here) | `playWord`(4) + `playIndex`(4) |
| +0x6C | 4 | int32 | newEmpty sets to -1 (0x29af7a: movl $0xffffffff, r15+0x84) | `playIndex` |
| +0x70 | 4 | int32 | mov at 0x114fda; newEmpty: from DevConst+0x40 | `fieldAt70` |
| +0x74 | 4 | int32 | mov at 0x114fe0; newEmpty: =0 | `fieldAt74` |
| +0x78 | 8 | ptr | mov at 0x114fe6; newEmpty stores DeviceConstants* | `deviceConstants` |
| +0x80 | 48 | Signal | copy ctor called at 0x114ff9 with src=parent+0x80 | `signal` |
| +0x80 | 24 | vector<T> | freed in dtor (0x11532e) | signal.data1 |
| +0x98 | 24 | vector<T> | freed in dtor (0x11530c) | signal.data2 |
| +0xB0 | 24 | vector<uint8_t> | freed in dtor (0x1152ea); accessed in genPlayConfig | `markerData` |
| +0xC8 | 2 | uint16_t | movzwl in toString (0x2c525c); genPlayConfig (0x2789d7) | `sampleWidth` (printed as "channels") |
| +0xCA | 6 | unknown | zeroed by overlapping stores in newEmpty | padding/unknown |
| +0xD0 | 4 | int32 | printed in toString (0x2c521e) as "N samples" | `numSamples` |
| +0xD4 | 4 | unknown | part of zeroed region (movq 0 at obj+0xD0 covers 8 bytes) | padding |

### WaveformFront-Specific (+0xD8 to +0xF7, 0x20 = 32 bytes)

| Offset | Size | Type | Evidence | Field Name |
|--------|------|------|----------|------------|
| +0xD8 | 4 | int32 | set to 1 in copy ctor (0x2a25b9) | `fieldAtD8` (playCount?) |
| +0xDC | 1 | bool | set to 0 in copy ctor (0x2a25c3) | `flagAtDC` |
| +0xDD | 1 | bool | copied from source in copy ctor (0x2a25cd-0x2a25d4) | `flagAtDD` |
| +0xDE | 2 | padding | — | — |
| +0xE0 | 24 | vector<Value> | copy-constructed at 0x2a25da-0x2a2671; dtor iterates | `values` |

## Destructor Flow

1. `__on_zero_shared()` (0x2a1300):
   - Iterates vector at +0xE0 (begin=+0xE0, end=+0xE8, cap=+0xF0)
   - Each element is 0x28 bytes; checks field at elem-0x20 (int, sar+xor>3)
   - Frees heap string at elem-0x18 if bit 0 set
   - Frees the vector buffer itself
   - Falls through to `Waveform::~Waveform()`

2. `Waveform::~Waveform()` (0x1152e0):
   - Frees vector at +0xB0 (markerData)
   - Frees vector at +0x98 (signal.data2)
   - Frees vector at +0x80 (signal.data1)
   - Frees string at +0x50 (thirdString)
   - Releases shared_ptr at +0x38 (file) via control block at +0x40
   - Frees string at +0x20 (secondaryName)
   - Frees string at +0x00 (name)

## toString() Format

```
"Name: " + name + ", " + typeStr + ") " + numSamples + " samples & " + sampleWidth + " channels"
```

Where typeStr is: CSV(0), RAW(1), GEN(2), UNDEF(default).

## genPlayConfig Key Accesses

- +0xC8 (uint16_t sampleWidth): used to compute channelMask = `(1 << sampleWidth) - 1`
  - If sampleWidth == 1, mask = 2 (special case)
- +0xB0/+0xB8 (markerData vector): length computed, iterated for marker bits

## newEmptyWaveform Default Values

| Offset | Default Value | Notes |
|--------|--------------|-------|
| +0x18 | 2 (GEN) | waveformType |
| +0x20..+0x6B | 0 | zeroed (strings, shared_ptr, etc.) |
| +0x6C | -1 | playIndex (unassigned) |
| +0x70 | DeviceConstants[+0x40] | from device constants |
| +0x74 | 0 | |
| +0x78 | DeviceConstants* | pointer to device constants |
| +0x80..+0xCA | 0 | signal, markerData, sampleWidth zeroed |
| +0xD0 | 0 | numSamples (8 bytes zeroed) |
| +0xD8 | 1 | fieldAtD8 |
| +0xDC | 0 | flagAtDC + flagAtDD (movw 0) |
| +0xE0..+0xF7 | 0 | values vector empty |
