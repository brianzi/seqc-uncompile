# Special Register Address Map {#notes_special_registers}

Complete tabulation of all memory-mapped register addresses used by
`ld` / `st` (and their aliases `luser` / `suser`) in the SeqC
sequencer.  Each entry lists the assembler primitive that emits the
access and the SeqC-level function(s) the access originates from.

## Device Families

- **Cervino**: UHFLI(1), UHFQA(4), GHFLI(128), VHFLI(256)
- **Hirzel**: HDAWG(2), SHFQA(8), SHFSG(16), SHFQC_SG(32), SHFLI(64)
- **SHF+**: SHFSG(16), SHFQC_SG(32), SHFQA(8) — subset of Hirzel with
  frequency-sweep support.

See \ref notes_cervino_vs_hirzel for the broader ISA split.

---

## 1. Write Protocol Registers (`writeToNode` / `setUserReg` / `setInt` / `setDouble`)

| Addr   | Dir | Asm Primitive        | SeqC Function(s)                                         | Devices | Purpose                                 |
|--------|-----|----------------------|----------------------------------------------------------|---------|-----------------------------------------|
| `0x10` | ST  | `suser(reg, 0x10)`   | `setUserReg`, `setInt`, `setDouble` (via `writeToNode`)  | All     | Multi-word write: low word / node ID    |
| `0x11` | ST  | `suser(reg, 0x11)`   | `setUserReg`, `setInt`, `setDouble` (via `writeToNode`)  | All     | Multi-word write: mid word / reg index  |
| `0x12` | ST  | `suser(reg, 0x12)`   | `writeToNode` block D                                    | All     | Multi-word write: high word             |
| `0x13` | ST  | `suser(reg, 0x13)`   | `writeToNode` block D (double-precision)                 | All     | Double-precision high 32 bits           |
| `0x14` | ST  | `suser(reg, 0x14)`   | `writeToNode` (slow-path commit)                         | All     | Slow-path commit                        |
| `0x16` | ST  | `suser(reg, 0x16)`   | `writeToNode` block D                                    | All     | Write commit/finalize                   |
| `0x17` | ST  | `suser(reg, 0x17)`   | `writeToNode` block D (fast path)                        | All     | Direct single-value write               |
| `0x18` | ST  | `suser(reg, 0x18)`   | `writeToNode` (frequency commit, typeIdx=4)              | All     | Frequency commit                        |
| `0x19` | ST  | `suser(reg, 0x19)`   | `writeToNode` block D (stereo/Q-channel)                 | All     | Direct write B (companion channel)      |

## 2. User Register Space

| Addr          | Dir   | Asm Primitive              | SeqC Function(s)                              | Devices      | Purpose                               |
|---------------|-------|----------------------------|------------------------------------------------|--------------|---------------------------------------|
| `0x00`        | LD/ST | `luser(reg,0)`/`suser(reg,0)` | `setUserReg` (HDAWG sync), `getUserReg`    | HDAWG        | Generic user reg 0 (sync handshake)   |
| `0x00`–`0x3FF`| LD/ST | `luser(reg,N)`/`suser(reg,N)` | `setUserReg(idx,val)`, `getUserReg(idx)`, `getSweeperLength` | All | General user register space (depth=1024) |

## 3. Trigger & Wait Registers

| Addr   | Dir | Asm Primitive        | SeqC Function(s)                                               | Devices                   | Purpose                              |
|--------|-----|----------------------|----------------------------------------------------------------|---------------------------|--------------------------------------|
| `0x1A` | ST  | `suser(reg, 0x1A)`   | `wait` (non-simple devices), switch-case AST eval              | Non-simple Cervino        | Trigger value load for `wtrig`       |
| `0x1B` | ST  | `st(R0, 0x1B)`       | `waitTimestamp()`                                              | HDAWG only                | Timestamp wait register              |
| `0x1C` | ST  | `suser(reg, 0x1C)`   | playback internals (`setNowTimestamp`)                         | All                       | Current sequencer timestamp ("now")  |
| `0x22` | LD  | `ltrig(reg)`          | `getTrigger`, `getAnaTrigger`, `getDigTrigger`                 | All                       | Read trigger register                |
| `0x22` | ST  | `strig(reg)`          | `setTrigger`, `startQAResult`, `startQAMonitor`                | Pre-SHFLI                 | Write trigger register               |
| `0x23` | ST  | `sinttrig(reg)`       | `setInternalTrigger(val)`                                      | LI-family                 | Internal trigger register            |

## 4. Digital I/O Registers

| Addr    | Dir | Asm Primitive          | SeqC Function(s) | Devices                   | Purpose               |
|---------|-----|------------------------|-------------------|---------------------------|-----------------------|
| `0x20`  | LD  | `ldio(reg, false)`     | `getDIO()`        | All                       | DIO read (low bank)   |
| `0x20`  | ST  | `sdio(reg, false)`     | `setDIO(val)`     | All                       | DIO write (low bank)  |
| `0x21`  | ST  | `sid(reg, false)`      | `setID(val)`      | Hirzel                    | Set ID (low bank)     |
| `0x1FE` | LD  | `ldio(reg, true)`      | `getDIO()`        | SHFLI, VHFLI, GHFLI      | DIO read (high bank)  |
| `0x1FE` | ST  | `sdio(reg, true)`      | `setDIO(val)`     | SHFLI, VHFLI, GHFLI      | DIO write (high bank) |
| `0x1FF` | ST  | `sid(reg, true)`       | `setID(val)`      | SHFLI, VHFLI, GHFLI      | Set ID (high bank)    |

## 5. I/O Trigger (Device-Dependent Address)

| Addr   | Dir | Asm Primitive                     | SeqC Function(s)                                                 | Devices  | Purpose          |
|--------|-----|-----------------------------------|------------------------------------------------------------------|----------|------------------|
| `0x60` | LD  | `ldiotrig(reg)` (Cervino impl)    | `getDIOTriggered`, `getZSyncData(RAW)`, `getFeedback(RAW)`      | Cervino  | I/O trigger read |
| `0x68` | LD  | `ldiotrig(reg)` (Hirzel impl)     | `getDIOTriggered`, `getZSyncData(RAW)`, `getFeedback(RAW)`      | Hirzel   | I/O trigger read |

## 6. Wait Trigger Placeholders (`st(R0, value+0x40)`)

| Addr     | Dir | Asm Primitive                | SeqC Function(s)                                             | Devices                              | Purpose                            |
|----------|-----|------------------------------|--------------------------------------------------------------|--------------------------------------|------------------------------------|
| `0x40+N` | ST  | `asmWtrigLSPlaceholder(N)`   | `waitDIOTrigger`, `waitZSyncTrigger`, `waitDigTrigger(idx)`, `waitCntTrigger(idx)`, `waitOnGrid`, `waitSineOscPhase(osc)` | Various (per-function device checks) | Encoded wait-trigger; N from `readConst` |

The value N is resolved at compile time from resource constants such as
`AWG_MAP_TRIGGER_INDEX`, `AWG_ZSYNC_TRIGGER_INDEX`, `AWG_DIG_TRIGGER{1,2}_INDEX`,
`AWG_CNT_TRIGGER{0,1}_INDEX`, `AWG_DEMOD_TRIGGER{1,2}_INDEX`.

## 7. Sync Registers

| Addr   | Dir | Asm Primitive        | SeqC Function(s)                           | Devices  | Purpose                |
|--------|-----|----------------------|---------------------------------------------|----------|------------------------|
| `0x44` | ST  | `suser(reg, 0x44)`   | `syncCervino(flag=true)`, `unsyncCervino()` | Cervino  | Sync register A        |
| `0x45` | ST  | `suser(reg, 0x45)`   | `syncCervino(flag=false)`, `unsyncCervino()`| Cervino  | Sync register B        |
| `0x6E` | ST  | `suser(R0, 0x6E)`    | `asmSyncHirzel()` (via `addSyncCommand`)    | Hirzel   | Sync register (Hirzel) |
| `0x92` | ST  | `st(R0, 0x92)`       | `waitOnSync()`                              | LI-family| Sync wait register     |

## 8. Oscillator Phase Registers

| Addr   | Dir | Asm Primitive        | SeqC Function(s)                                         | Devices                    | Purpose                        |
|--------|-----|----------------------|----------------------------------------------------------|----------------------------|---------------------------------|
| `0x5F` | ST  | `st(reg, 0x5F)` ×2  | `resetOscPhase(mask)` — pulse (write mask, then clear)   | Hirzel (HDAWG, SHFSG, SHFQC) | Oscillator phase reset         |
| `0x70` | ST  | `suser(reg, 0x70)`   | `setSinePhase(0,phase)` or `setSinePhase(phase)` (SHF)  | HDAWG (osc 0), SHFSG, SHFQC\_SG | Sine phase, oscillator 0    |
| `0x71` | ST  | `suser(reg, 0x71)`   | `setSinePhase(1,phase)`                                  | HDAWG (osc 1)              | Sine phase, oscillator 1       |
| `0x72` | ST  | `suser(reg, 0x72)`   | `incrementSinePhase(0,inc)` or `incrementSinePhase(inc)` | HDAWG (osc 0), SHFSG, SHFQC\_SG | Sine phase increment, osc 0 |
| `0x73` | ST  | `suser(reg, 0x73)`   | `incrementSinePhase(1,inc)`                              | HDAWG (osc 1)              | Sine phase increment, osc 1    |

## 9. Map Registers

| Addr   | Dir | Asm Primitive       | SeqC Function(s)                            | Devices | Purpose          |
|--------|-----|---------------------|----------------------------------------------|---------|------------------|
| `0x62` | ST  | `st(r1, 0x62)`      | `smap(r1,r2,arg)` (command table mapping)    | All     | Map key register |
| `0x63` | ST  | `st(r2, 0x63)`      | `smap(r1,r2,arg)` (command table mapping)    | All     | Map value register|

## 10. Loop Counter Registers

| Addr       | Dir | Asm Primitive                    | SeqC Function(s)       | Devices    | Purpose                          |
|------------|-----|----------------------------------|------------------------|------------|----------------------------------|
| `0x64+N`   | LD  | `lcnt(reg,N)` → `ld(reg,0x64+N)`| `getCnt(counterIdx)`   | HDAWG only | HW loop counter (N=0..1 → 0x64..0x65) |

## 11. Wait Cycles / AWG Core Count

| Addr   | Dir | Asm Primitive        | SeqC Function(s)                                                               | Devices | Purpose                         |
|--------|-----|----------------------|---------------------------------------------------------------------------------|---------|---------------------------------|
| `0x69` | ST  | `suser(reg, 0x69)`   | `wait(val)`, `getUserReg` (HDAWG), `resetOscPhase` (multi-core), `getZSyncData`, `getFeedback`, `executeTableEntry` (via `setWaitCyclesReg`) | Various | Wait-cycles / AWG core count    |
| `0x6F` | ST  | `suser(reg, 0x6F)`   | `play` internals (`custom_functions_play.cpp`)                                 | All     | Wait register (legacy, play-internal) |

## 12. RT Logger

| Addr   | Dir | Asm Primitive       | SeqC Function(s)              | Devices                         | Purpose                  |
|--------|-----|---------------------|-------------------------------|---------------------------------|--------------------------|
| `0x1D` | ST  | `suser(reg, 0x1D)`  | `at(val)`                     | Cervino                         | RT logger output data    |
| `0x62` | ST  | `st(R0, 0x62)`      | `resetRTLoggerTimestamp()`    | UHFQA only                      | RT logger timestamp reset|
| `0x6D` | ST  | `st(R0, 0x6D)`      | `resetRTLoggerTimestamp()`    | All except UHFQA                | RT logger timestamp reset|

Note: `0x62` is overloaded — it is the map-key register in `smap` context but
the RT-logger-timestamp-reset register when used by `resetRTLoggerTimestamp` on
UHFQA. The hardware disambiguates by context (different devices).

## 13. ZSync Data Registers

| Addr   | Dir | Asm Primitive    | SeqC Function(s)                             | Devices                                      | Purpose                        |
|--------|-----|------------------|----------------------------------------------|----------------------------------------------|---------------------------------|
| `0x6A` | LD  | `ld(reg, 0x6A)`  | `getZSyncData(RAW)`, `getFeedback(RAW)`      | SHFQA (special path)                          | ZSync data raw                 |
| `0x6B` | LD  | `ld(reg, 0x6B)`  | `getZSyncData(PROCESSED_A)`, `getFeedback(PROCESSED_A)` | HDAWG, SHFSG, SHFQC\_SG, SHFLI, GHFLI, VHFLI | ZSync processed A              |
| `0x6C` | LD  | `ld(reg, 0x6C)`  | `getZSyncData(PROCESSED_B)`, `getFeedback(PROCESSED_B)` | HDAWG, SHFSG, SHFQC\_SG, SHFLI, GHFLI, VHFLI | ZSync processed B              |
| `0xC0` | LD  | `ld(reg, 0xC0)`  | `getFeedback(QA_DATA_RAW)`                   | SHFQC\_SG only                                | QA data raw (extended channel) |
| `0xC1` | LD  | `ld(reg, 0xC1)`  | `getFeedback(QA_DATA_PROCESSED_D)`           | SHFQC\_SG only                                | QA data processed (extended)   |

## 14. PRNG Registers

| Addr   | Dir | Asm Primitive        | SeqC Function(s)        | Devices | Purpose             |
|--------|-----|----------------------|--------------------------|---------|---------------------|
| `0x74` | ST  | `suser(reg, 0x74)`   | `setPRNGSeed(val)`       | Hirzel  | PRNG seed           |
| `0x75` | ST  | `suser(reg, 0x75)`   | `setPRNGRange(lo,hi)`    | Hirzel  | PRNG range low      |
| `0x76` | ST  | `suser(reg, 0x76)`   | `setPRNGRange(lo,hi)`    | Hirzel  | PRNG range span     |
| `0x77` | LD  | `luser(reg, 0x77)`   | `getPRNGValue()`         | Hirzel  | PRNG read value     |

## 15. QA (Quantum Analyzer) Registers

| Addr   | Dir | Asm Primitive        | SeqC Function(s)                                     | Devices        | Purpose                               |
|--------|-----|----------------------|-------------------------------------------------------|----------------|----------------------------------------|
| `0x61` | LD  | `ld(reg, 0x61)`      | `getQAResult()`                                       | UHFQA          | QA result register                     |
| `0x78` | ST  | `suser(reg, 0x78)`   | `startQA(...)`, `resetOscPhase()` (non-SHFQA SHF)    | UHFQA, SHF     | QA weights + result addr / osc reset   |
| `0x79` | ST  | `suser(reg, 0x79)`   | `startQA(...)`                                        | UHFQA, SHFQA   | QA trigger/monitor composite           |
| `0x7A` | ST  | `suser(reg, 0x7A)`   | `startQA(...)` (UHFQA), `resetOscPhase()` (SHFQA)    | UHFQA, SHFQA   | QA result length / osc phase reset     |

## 16. Frequency Sweep Registers (SHF+ only)

| Addr   | Dir | Asm Primitive        | SeqC Function(s)                                          | Devices | Purpose                       |
|--------|-----|----------------------|------------------------------------------------------------|---------|-------------------------------|
| `0x8C` | ST  | `suser(reg, 0x8C)`   | `configFreqSweep`, `setSweepStep`, `setOscFreq`           | SHF+    | Sweep oscillator index        |
| `0x8D` | ST  | `suser(reg, 0x8D)`   | `setSweepStep`, `setOscFreq`                               | SHF+    | Sweep control (step/reset)    |
| `0x8E` | ST  | `suser(reg, 0x8E)`   | `configFreqSweep`, `setOscFreq` (via `writeLS64bit`)      | SHF+    | Sweep start freq low 32b     |
| `0x8F` | ST  | `suser(reg, 0x8F)`   | `configFreqSweep`, `setOscFreq` (via `writeLS64bit`)      | SHF+    | Sweep start freq high 32b    |
| `0x90` | ST  | `suser(reg, 0x90)`   | `configFreqSweep` (via `writeLS64bit`)                     | SHF+    | Sweep step freq low 32b      |
| `0x91` | ST  | `suser(reg, 0x91)`   | `configFreqSweep` (via `writeLS64bit`)                     | SHF+    | Sweep step freq high 32b     |

---

## Address Map (sorted numerically)

| Addr     | Dir   | Category              | Brief                            |
|----------|-------|-----------------------|----------------------------------|
| `0x00`   | LD/ST | User reg              | Generic user 0 / sync (HDAWG)   |
| `0x00`–`0x3FF` | LD/ST | User reg       | General user register space      |
| `0x10`   | ST    | Write protocol        | Node write low word              |
| `0x11`   | ST    | Write protocol        | Node write mid word              |
| `0x12`   | ST    | Write protocol        | Node write high word             |
| `0x13`   | ST    | Write protocol        | Double-precision high bits       |
| `0x14`   | ST    | Write protocol        | Slow-path commit                 |
| `0x16`   | ST    | Write protocol        | Write commit                     |
| `0x17`   | ST    | Write protocol        | Direct single-value write        |
| `0x18`   | ST    | Write protocol        | Frequency commit                 |
| `0x19`   | ST    | Write protocol        | Direct write B (Q-channel)       |
| `0x1A`   | ST    | Trigger               | Trigger value load               |
| `0x1B`   | ST    | Wait                  | Timestamp wait (HDAWG)           |
| `0x1C`   | ST    | Timestamp             | Current timestamp ("now")        |
| `0x1D`   | ST    | RT logger             | Logger output data               |
| `0x20`   | LD/ST | DIO                   | DIO low bank                     |
| `0x21`   | ST    | DIO                   | Set ID low bank                  |
| `0x22`   | LD/ST | Trigger               | Trigger register                 |
| `0x23`   | ST    | Trigger               | Internal trigger                 |
| `0x40+N` | ST    | Wait placeholder      | Encoded wait-trigger             |
| `0x44`   | ST    | Sync                  | Sync register A (Cervino)        |
| `0x45`   | ST    | Sync                  | Sync register B (Cervino)        |
| `0x5F`   | ST    | Oscillator            | Osc phase reset (pulse)          |
| `0x60`   | LD    | I/O trigger           | I/O trigger (Cervino)            |
| `0x61`   | LD    | QA                    | QA result                        |
| `0x62`   | ST    | Map / RT logger       | Map key / RT logger reset (UHFQA)|
| `0x63`   | ST    | Map                   | Map value                        |
| `0x64`–`0x65` | LD | Loop counter        | HW loop counter 0..1 (HDAWG)    |
| `0x68`   | LD    | I/O trigger           | I/O trigger (Hirzel)             |
| `0x69`   | ST    | Wait / core count     | Wait-cycles / AWG core count     |
| `0x6A`   | LD    | ZSync                 | ZSync data raw (SHFQA)           |
| `0x6B`   | LD    | ZSync                 | ZSync processed A                |
| `0x6C`   | LD    | ZSync                 | ZSync processed B                |
| `0x6D`   | ST    | RT logger             | RT logger reset (non-UHFQA)      |
| `0x6E`   | ST    | Sync                  | Sync register (Hirzel)           |
| `0x6F`   | ST    | Wait (legacy)         | Play-internal wait               |
| `0x70`   | ST    | Oscillator            | Sine phase, osc 0                |
| `0x71`   | ST    | Oscillator            | Sine phase, osc 1                |
| `0x72`   | ST    | Oscillator            | Sine phase increment, osc 0      |
| `0x73`   | ST    | Oscillator            | Sine phase increment, osc 1      |
| `0x74`   | ST    | PRNG                  | Seed                             |
| `0x75`   | ST    | PRNG                  | Range low                        |
| `0x76`   | ST    | PRNG                  | Range span                       |
| `0x77`   | LD    | PRNG                  | Read value                       |
| `0x78`   | ST    | QA / osc reset        | QA weights+addr / osc reset      |
| `0x79`   | ST    | QA                    | QA trigger/monitor               |
| `0x7A`   | ST    | QA / osc reset        | QA result length / osc reset     |
| `0x8C`   | ST    | Freq sweep            | Sweep osc index                  |
| `0x8D`   | ST    | Freq sweep            | Sweep control                    |
| `0x8E`   | ST    | Freq sweep            | Sweep start freq low             |
| `0x8F`   | ST    | Freq sweep            | Sweep start freq high            |
| `0x90`   | ST    | Freq sweep            | Sweep step freq low              |
| `0x91`   | ST    | Freq sweep            | Sweep step freq high             |
| `0x92`   | ST    | Sync                  | Sync wait (LI-family)            |
| `0xC0`   | LD    | ZSync (extended)      | QA data raw (SHFQC\_SG)         |
| `0xC1`   | LD    | ZSync (extended)      | QA data processed (SHFQC\_SG)   |
| `0x1FE`  | LD/ST | DIO                   | DIO high bank                    |
| `0x1FF`  | ST    | DIO                   | Set ID high bank                 |

## See also

- \ref notes_custom_functions — the SeqC-level functions named in
  the "SeqC Function(s)" column above.  Each entry there lists the
  runtime helper that emits the `suser` / `luser` / `st` / `ld`
  pair for the register documented here.
- \ref notes_opcode_encoding — bit-level encoding of `ld` / `st`
  / `wtrig` and the other primitives referenced in the
  "Asm Primitive" column.
- \ref notes_cervino_vs_hirzel — which device-family-specific
  registers are emitted by which assembler back-end.
