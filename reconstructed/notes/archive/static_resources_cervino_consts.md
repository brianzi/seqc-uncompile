# StaticResources::init() — Cervino (device type 2) addConst Calls {#notes_static_resources_cervino_consts}

\note **Reverse-engineering reference material.** This page is part of
the `reconstructed/notes/` set: deep-dive technical notes for
contributors working on the reconstruction. It cites binary addresses,
opcodes, and disassembly observations directly so they remain
discoverable from the rendered site. The standard documentation-voice
rules for API briefs (no binary citations outside `\binarynote`) do
**not** apply to this page.

Binary: `_seqc_compiler.so`, function at 0x1ec8f0 (size 0x4854)

## Branching Summary

The function dispatches on `AWGCompilerConfig.deviceType`:
- Types 1, 4 (HD/SHFSG) → 0x1ec925 (14 AWG_RATE + 3 integration/trigger)
- Type 2 (Cervino/SHF) → 0x1ecd55 (14 AWG_RATE)
- Types 8, 16, 32 (various) → 0x1ed14a
- Type 4 sub-dispatch → 0x1ed4ff (QA_INT/QA_GEN)
- Type 8 sub-dispatch → 0x1ed87b (QA_INT/QA_GEN)
- Types 0x40, 0x100 → 0x1ee1f9
- Type 0x80 → 0x1ee309
- Types 2, 16, 32 → 0x1ee430 (ZSYNC_DATA section)
- Common section → 0x1ee69b (all types)

For device type 2 (Cervino), execution path:
1. Skip HD branch (jne 0x1ecd4c)
2. Enter type-2 AWG_RATE at 0x1ecd55 (14 calls)
3. Skip type 8/16/32 bitmap checks (jae to 0x1ed4ec)
4. Skip type-4-only (jne 0x1ed872), type-8-only (jne 0x1ee1e9),
   type-0x100/0x40 (jne 0x1ee2fe), type-0x80 (jne 0x1ee411)
5. Enter ZSYNC_DATA section at 0x1ee430 (bitmap 0x100010004 matches bit 2)
6. Skip type-0x20-only tail (jne 0x1ee69b at 0x1ee602)
7. Enter common section at 0x1ee69b (102 calls)
   - Within common: type 8 skips AWG_CHAN/AWG_MARKER (0x1efcaa je)
     and AWG_OSC_PHASE/USERREG_SWEEP (0x1f00d6 je).
     Type 2 executes ALL of these.

## Important Correction

AWG_INTEGRATION_ARM, AWG_INTEGRATION_TRIGGER, and AWG_MONITOR_TRIGGER
are **NOT** in the Cervino path. They are at 0x1eccfe–0x1ecd44, which is
in the HD (type 1/4) branch only. Type 2 enters at 0x1ecd55 via the
`cmp $0x2` at 0x1ecd4c, bypassing these entirely.

---

## Section 1: AWG_RATE (type-2 specific, 0x1ecd55–0x1ed114)

*Already extracted — 14 entries, 2400MHz base, values 0.0–13.0*

## Section 2: ZSYNC_DATA (types 2/16/32, 0x1ee430–0x1ee5fd)

These values are computed from `DeviceConstants[0x78]`:
- `n = DeviceConstants.field_0x78`  (byte)
- `base = 1 << n`

| Address | Name | Value | Notes |
|---------|------|-------|-------|
| 0x1ee46c | ZSYNC_DATA_RAW | (double)(1 << n) | cvtsi2sd |
| 0x1ee4dc | ZSYNC_DATA_PQSC_REGISTER | (double)((1 << n) + 1) | heap string, 24 chars |
| 0x1ee527 | ZSYNC_DATA_PROCESSED_A | (double)((1 << n) + 1) | same value as above |
| 0x1ee597 | ZSYNC_DATA_PQSC_DECODER | (double)((1 << n) + 2) | heap string, 23 chars |
| 0x1ee5e2 | ZSYNC_DATA_PROCESSED_B | (double)((1 << n) + 2) | same value as above |

Type-0x20-only (skipped by type 2):
| 0x1ee639 | QA_DATA_RAW | (double)((1 << n) + 3) | |
| 0x1ee67f | QA_DATA_PROCESSED | (double)((1 << n) + 4) | |

## Section 3: Common section (all types, 0x1ee69b–0x1f05ed)

### 3a. Device sample rate (0x1ee69b–0x1ee6b7)
| Address | Name | Value |
|---------|------|-------|
| 0x1ee6b7 | DEVICE_SAMPLE_RATE | AWGCompilerConfig[0x8] (double from config) |

Uses global string `constDeviceSampleRate` @ 0x95b240.
NaN check at 0x1ee6a1: if NaN and type==2, uses hardcoded value from rodata (0x1ee6bc path).

### 3b. Analog/Digital trigger indices (0x1ee6bc–0x1ee7c2)
| Address | Name | Value |
|---------|------|-------|
| 0x1ee6ec | AWG_ANA_TRIGGER1_INDEX | 0.0 |
| 0x1ee73a | AWG_ANA_TRIGGER2_INDEX | 1.0 |
| 0x1ee77e | AWG_DIG_TRIGGER1_INDEX | 2.0 |
| 0x1ee7c2 | AWG_DIG_TRIGGER2_INDEX | 3.0 |

### 3c. Demod trigger indices (0x1ee81f–0x1eeae6)
| Address | Name | Value |
|---------|------|-------|
| 0x1ee81f | AWG_DEMOD_TRIGGER1_INDEX | 4.0 |
| 0x1ee87c | AWG_DEMOD_TRIGGER2_INDEX | 5.0 |
| 0x1ee8e3 | AWG_DEMOD_TRIGGER3_INDEX | 6.0 |
| 0x1ee94a | AWG_DEMOD_TRIGGER4_INDEX | 7.0 |
| 0x1ee9b1 | AWG_DEMOD_TRIGGER5_INDEX | 8.0 |
| 0x1eea18 | AWG_DEMOD_TRIGGER6_INDEX | 9.0 |
| 0x1eea7f | AWG_DEMOD_TRIGGER7_INDEX | 10.0 |
| 0x1eeae6 | AWG_DEMOD_TRIGGER8_INDEX | 11.0 |

### 3d. Demod rate trigger indices (0x1eeb4a–0x1eee6a)
| Address | Name | Value |
|---------|------|-------|
| 0x1eeb4a | AWG_DEMODRATEMAX_TRIGGER1_INDEX | 12.0 |
| 0x1eebae | AWG_DEMODRATE_TRIGGER1_INDEX | 13.0 |
| 0x1eec12 | AWG_DEMODRATE_TRIGGER2_INDEX | 14.0 |
| 0x1eec76 | AWG_DEMODRATE_TRIGGER3_INDEX | 15.0 |
| 0x1eecda | AWG_DEMODRATE_TRIGGER4_INDEX | 16.0 |
| 0x1eed3e | AWG_DEMODRATE_TRIGGER5_INDEX | 17.0 |
| 0x1eeda2 | AWG_DEMODRATE_TRIGGER6_INDEX | 18.0 |
| 0x1eee06 | AWG_DEMODRATE_TRIGGER7_INDEX | 19.0 |
| 0x1eee6a | AWG_DEMODRATE_TRIGGER8_INDEX | 20.0 |

### 3e. MDS/ZSYNC/CNT/MAP/WAIT/TS/TIME trigger indices (0x1eeed1–0x1ef1f8)
| Address | Name | Value |
|---------|------|-------|
| 0x1eeed1 | AWG_MDS_RE_TRIGGER_INDEX | 21.0 |
| 0x1eef2e | AWG_MDS_FE_TRIGGER_INDEX | 22.0 |
| 0x1eef92 | AWG_MDS_LVL1_TRIGGER_INDEX | 23.0 |
| 0x1eeff6 | AWG_MDS_LVL0_TRIGGER_INDEX | 24.0 |
| 0x1ef053 | AWG_ZSYNC_TRIGGER_INDEX | 25.0 |
| 0x1ef0a1 | AWG_CNT_TRIGGER0_INDEX | 26.0 |
| 0x1ef0e5 | AWG_CNT_TRIGGER1_INDEX | 27.0 |
| 0x1ef129 | AWG_MAP_TRIGGER_INDEX | 28.0 |
| 0x1ef16d | AWG_WAIT_TRIGGER_INDEX | 29.0 |
| 0x1ef1b4 | AWG_TS_TRIGGER_INDEX | 30.0 |
| 0x1ef1f8 | AWG_TIME_TRIGGER_INDEX | 31.0 |

### 3f. Grid trigger (0x1ef24e)
| Address | Name | Value |
|---------|------|-------|
| 0x1ef24e | AWG_GRID_TRIGGER | 32.0 |

### 3g. Trigger bitmask values (0x1ef298–0x1ef458)
| Address | Name | Value |
|---------|------|-------|
| 0x1ef298 | AWG_TRIGGER1 | 1.0 |
| 0x1ef2d8 | AWG_TRIGGER2 | 2.0 |
| 0x1ef318 | AWG_TRIGGER3 | 4.0 |
| 0x1ef358 | AWG_TRIGGER4 | 8.0 |
| 0x1ef398 | AWG_TRIGGER5 | 16.0 |
| 0x1ef3d8 | AWG_TRIGGER6 | 32.0 |
| 0x1ef418 | AWG_TRIGGER7 | 64.0 |
| 0x1ef458 | AWG_TRIGGER8 | 128.0 |

### 3h. Analog/Digital trigger bitmask values (0x1ef498–0x1ef558)
| Address | Name | Value |
|---------|------|-------|
| 0x1ef498 | AWG_ANA_TRIGGER1 | 1.0 |
| 0x1ef4d8 | AWG_ANA_TRIGGER2 | 2.0 |
| 0x1ef518 | AWG_DIG_TRIGGER1 | 4.0 |
| 0x1ef558 | AWG_DIG_TRIGGER2 | 8.0 |

### 3i. Demod trigger bitmask values (0x1ef59e–0x1ef788)
| Address | Name | Value |
|---------|------|-------|
| 0x1ef59e | AWG_DEMOD_TRIGGER1 | 16.0 |
| 0x1ef5e4 | AWG_DEMOD_TRIGGER2 | 32.0 |
| 0x1ef62a | AWG_DEMOD_TRIGGER3 | 64.0 |
| 0x1ef670 | AWG_DEMOD_TRIGGER4 | 128.0 |
| 0x1ef6b6 | AWG_DEMOD_TRIGGER5 | 256.0 |
| 0x1ef6fc | AWG_DEMOD_TRIGGER6 | 512.0 |
| 0x1ef742 | AWG_DEMOD_TRIGGER7 | 1024.0 |
| 0x1ef788 | AWG_DEMOD_TRIGGER8 | 2048.0 |

### 3j. Demod rate trigger bitmask values (0x1ef7ec–0x1efa5c)
| Address | Name | Value |
|---------|------|-------|
| 0x1ef7ec | AWG_DEMODRATEMAX_TRIGGER1 | 4096.0 |
| 0x1ef83a | AWG_DEMODRATE_TRIGGER1 | 8192.0 |
| 0x1ef888 | AWG_DEMODRATE_TRIGGER2 | 16384.0 |
| 0x1ef8d6 | AWG_DEMODRATE_TRIGGER3 | 32768.0 |
| 0x1ef924 | AWG_DEMODRATE_TRIGGER4 | 65536.0 |
| 0x1ef972 | AWG_DEMODRATE_TRIGGER5 | 131072.0 |
| 0x1ef9c0 | AWG_DEMODRATE_TRIGGER6 | 262144.0 |
| 0x1efa0e | AWG_DEMODRATE_TRIGGER7 | 524288.0 |
| 0x1efa5c | AWG_DEMODRATE_TRIGGER8 | 1048576.0 |

### 3k. MDS/special trigger bitmask values (0x1efaa2–0x1efc8e)
| Address | Name | Value |
|---------|------|-------|
| 0x1efaa2 | AWG_MDS_RE_TRIGGER | 2097152.0 |
| 0x1efae8 | AWG_MDS_FE_TRIGGER | 4194304.0 |
| 0x1efb2f | AWG_MDS_LVL1_TRIGGER | 8388608.0 |
| 0x1efb76 | AWG_MDS_LVL0_TRIGGER | 16777216.0 |
| 0x1efbc7 | AWG_MAP_TRIGGER | 268435456.0 |
| 0x1efc07 | AWG_WAIT_TRIGGER | 536870912.0 |
| 0x1efc4e | AWG_TS_TRIGGER | 1073741824.0 |
| 0x1efc8e | AWG_TIME_TRIGGER | -2147483648.0 |

### 3l. Channel/Marker (type-2 only, skipped by type 8) (0x1efcdd–0x1efda2)
| Address | Name | Value |
|---------|------|-------|
| 0x1efcdd | AWG_CHAN1 | 1.0 |
| 0x1efd18 | AWG_CHAN2 | 2.0 |
| 0x1efd62 | AWG_MARKER1 | 1.0 |
| 0x1efda2 | AWG_MARKER2 | 2.0 |

### 3m. Suppress/Enable (0x1efe06–0x1f00ba)
| Address | Name | Value |
|---------|------|-------|
| 0x1efe06 | AWG_SUPPRESS_CHAN1_SIGOUT1 | 1.0 |
| 0x1efe6a | AWG_SUPPRESS_CHAN1_SIGOUT2 | 2.0 |
| 0x1efece | AWG_SUPPRESS_CHAN2_SIGOUT1 | 4.0 |
| 0x1eff32 | AWG_SUPPRESS_CHAN2_SIGOUT2 | 8.0 |
| 0x1eff99 | AWG_ENABLE_CHAN1_SIGOUT1 | 1.0 |
| 0x1f0000 | AWG_ENABLE_CHAN1_SIGOUT2 | 2.0 |
| 0x1f005d | AWG_ENABLE_CHAN2_SIGOUT1 | 4.0 |
| 0x1f00ba | AWG_ENABLE_CHAN2_SIGOUT2 | 8.0 |

### 3n. Oscillator phase (type-2 only, skipped by type 8) (0x1f010b–0x1f021e)
| Address | Name | Value |
|---------|------|-------|
| 0x1f010b | AWG_OSC_PHASE_START | 1.0 |
| 0x1f0150 | AWG_OSC_PHASE_MIDDLE | 0.0 |
| 0x1f01b7 | AWG_USERREG_SWEEP_COUNT0 | 35.0 |
| 0x1f021e | AWG_USERREG_SWEEP_COUNT1 | 36.0 |

### 3o. Math constants (0x1f0256–0x1f0571)
| Address | Name | Value |
|---------|------|-------|
| 0x1f0256 | M_E | 2.718281828459045 |
| 0x1f0299 | M_LOG2E | 1.4426950408889634 |
| 0x1f02dc | M_LOG10E | 0.4342944819032518 |
| 0x1f031a | M_LN2 | 0.6931471805599453 |
| 0x1f035c | M_LN10 | 2.302585092994046 |
| 0x1f0398 | M_PI | 3.141592653589793 |
| 0x1f03da | M_PI_2 | 1.5707963267948966 |
| 0x1f041c | M_PI_4 | 0.7853981633974483 |
| 0x1f045e | M_1_PI | 0.3183098861837907 |
| 0x1f04a0 | M_2_PI | 0.6366197723675814 |
| 0x1f04e9 | M_2_SQRTPI | 1.1283791670955126 |
| 0x1f052c | M_SQRT2 | 1.4142135623730951 |
| 0x1f0571 | M_SQRT1_2 | 0.7071067811865476 |

### 3p. Boolean constants (0x1f05b0–0x1f05ed)
| Address | Name | Value | VarSubType |
|---------|------|-------|------------|
| 0x1f05b0 | true | 1.0 | VarSubType(1) |
| 0x1f05ed | false | 0.0 | VarSubType(1) |

**Note:** These are the only two addConst calls with VarSubType(1) instead of VarSubType(0).

---

## Total count for Cervino (type 2)

| Section | Count |
|---------|-------|
| AWG_RATE (type-2 specific) | 14 |
| ZSYNC_DATA (types 2/16/32) | 5 |
| Common: DEVICE_SAMPLE_RATE | 1 |
| Common: Trigger indices | 32 |
| Common: Grid trigger | 1 |
| Common: Trigger bitmasks (1–8) | 8 |
| Common: Ana/Dig trigger bitmasks | 4 |
| Common: Demod trigger bitmasks | 8 |
| Common: Demod rate trigger bitmasks | 9 |
| Common: MDS/special trigger bitmasks | 8 |
| Common: Channel/Marker (non-type-8) | 4 |
| Common: Suppress | 4 |
| Common: Enable | 4 |
| Common: OSC phase/USERREG (non-type-8) | 4 |
| Common: Math constants | 13 |
| Common: Booleans | 2 |
| **Total** | **121** |
