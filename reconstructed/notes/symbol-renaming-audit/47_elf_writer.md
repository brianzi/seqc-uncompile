# Batch 47 — elf_writer

## 1. Files considered

- `reconstructed/include/zhinst/elf_writer.hpp`
- `reconstructed/src/elf_writer.cpp`
- `reconstructed/src/write_waves_to_elf.cpp`

Symbol-table check (per RULES §3) via
`nm --demangle /home/brian/zhinst/seqc_compiler/_seqc_compiler.so`:

Excluded from rename (tier-1, names appear verbatim in the binary
symbol table):

- Type `zhinst::ElfWriter`
- `ElfWriter::ElfWriter(unsigned short)` @0x2934a0
- `ElfWriter::prepareHeader(unsigned short)` @0x2936b0
- `ElfWriter::addCode(vector<unsigned int> const&)` @0x293710
- `ElfWriter::addData(char const*, unsigned long, string const&)` @0x293990
- `ElfWriter::addWaveform(shared_ptr<WaveformIR>, SampleFormat, bool, AddressImpl<unsigned int>)` @0x2939f0
- `ElfWriter::writeFile(ostream&)` @0x294030
- `ElfWriter::writeFile(string const&)` @0x2942a0
- `ElfWriter::setMemoryOffset(unsigned long)` @0x294410
- Free function `zhinst::(anonymous namespace)::writeWavesToElfMapped(AWGCompilerConfig const&, shared_ptr<WavetableIR>, ElfWriter&)`
  (mangled into the lambda typeinfo at `b02640`)
- Free function `zhinst::(anonymous namespace)::writeWavesToElfAbsolute(AWGCompilerConfig const&, shared_ptr<WavetableIR>, ElfWriter&)`
  (mangled into the lambda typeinfo at `b026c0`)

In-scope symbols are: parameters of those methods/free functions,
`ElfWriter::memoryOffset_`, lambda capture variables, and a small
number of locals.

## 2. Overview

| Symbol | Misnomer? | Conf | Justification (≤ 5 words) | Proposal(s) | Status |
|---|---|---|---|---|---|
| `ElfWriter::memoryOffset_` | no | medium | name matches usage | keep current | not-misnomer |
| `ElfWriter::ElfWriter::machineType` | no | medium | becomes e_machine value | keep current | not-misnomer |
| `ElfWriter::prepareHeader::machineType` | no | medium | same as ctor param | keep current | not-misnomer |
| `ElfWriter::addCode::opcodes` | no | high | `.text` instruction words | keep current | not-misnomer |
| `ElfWriter::addData::data` | no | low | generic but unambiguous | keep current | — |
| `ElfWriter::addData::size` | no | low | matches usage | keep current | — |
| `ElfWriter::addData::sectionName` | no | high | passed to `sections.add` | keep current | not-misnomer |
| `ElfWriter::addWaveform::waveform` | no | high | shared_ptr<WaveformIR> | keep current | not-misnomer |
| `ElfWriter::addWaveform::format` | no | medium | sample format | keep current | not-misnomer |
| `ElfWriter::addWaveform::useAbsolute` | yes | high | semantics inverted | `useMapped`, `isMapped` | cross-batch-arbitration |
| `ElfWriter::addWaveform::padSize` | no | high | bytes of `.dd_` padding | keep current | not-misnomer |
| `ElfWriter::addWaveform::rawDataSize` (local) | no | low | `rawData->size()` cache | keep current | — |
| `ElfWriter::addWaveform::wfName2` (local) | yes | medium | numeric suffix for reuse | merge with `wfName` | — |
| `ElfWriter::writeFile(ostream&)::os` | no | low | output stream | keep current | — |
| `ElfWriter::writeFile(string&)::filename` | no | low | filename | keep current | — |
| `ElfWriter::setMemoryOffset::offset` | no | low | matches field | keep current | — |
| `writeWavesToElfMapped::config` | no | medium | AWGCompilerConfig | keep current | — |
| `writeWavesToElfMapped::wavetable` | no | high | shared_ptr<WavetableIR> | keep current | not-misnomer |
| `writeWavesToElfMapped::elfWriter` | no | high | the ElfWriter& sink | keep current | not-misnomer |
| `writeWavesToElfMapped` lambda `waveform` | no | high | shared_ptr<WaveformIR> | keep current | — |
| `writeWavesToElfAbsolute::config` | no | medium | AWGCompilerConfig | keep current | — |
| `writeWavesToElfAbsolute::wavetable` | no | high | shared_ptr<WavetableIR> | keep current | not-misnomer |
| `writeWavesToElfAbsolute::elfWriter` | no | high | the ElfWriter& sink | keep current | not-misnomer |
| `writeWavesToElfAbsolute::currentOffset` (local) | no | high | tracks cumulative position | keep current | not-misnomer |
| `writeWavesToElfAbsolute` lambda `gap`, `alignMask`, `padding` (locals) | no | medium | match arithmetic | keep current | — |

## 3. Detailed findings

### `ElfWriter::addWaveform::useAbsolute`  [yes / high / cross-batch-arbitration]

Evidence:

- Declaration: `include/zhinst/elf_writer.hpp:104` —
  `bool useAbsolute,` (with comment "if true AND waveform is reserveOnly,
  sets segment as NOBITS with address metadata").
- Use: `src/elf_writer.cpp:192` —
  `if (useAbsolute && wfPtr->signal.reserveOnly_) { ... SHT_NOBITS ... }`.
- Caller `writeWavesToElfMapped` at `src/write_waves_to_elf.cpp:79` passes
  `/*mapped=*/true` to that parameter (i.e. argument value `true`).
- Caller `writeWavesToElfAbsolute` at `src/write_waves_to_elf.cpp:160`
  passes `/*mapped=*/false` (argument value `false`).
- Binary disassembly @0x293dbc:
  `cmpb $0x0,-0x48(%rbp); je → PROGBITS branch`,
  i.e. when the bool is **non-zero** the NOBITS path is taken.
- `notes/elf_format.md:32` — `.wf_<name>` is `SHT_NOBITS` when
  `useAbsolute && reserveOnly` (this is the existing recon-side wording;
  it labels the same branch the binary takes when the param is true).
- The two caller-side inline comments (`/*mapped=*/true`, `/*mapped=*/false`)
  in `write_waves_to_elf.cpp` were written by the recon author and
  describe the caller's *own* mode (`Mapped` vs `Absolute`).

Interpretation:

- The caller in the **mapped** code path passes `true`; the caller in
  the **absolute** code path passes `false`. When the parameter is
  `true`, the function takes the NOBITS / "reserve only" branch. The
  parameter name `useAbsolute` therefore reads as the inverse of the
  semantics the binary actually implements: it is `true` exactly in
  the mapped case.
- The `notes/elf_format.md` phrasing "`useAbsolute && reserveOnly`"
  reads with current naming; under the corrected reading it means
  "in mapped mode AND signal is reserve-only".

Judgement:

- Yes. The parameter name is inverted relative to the actual control
  flow at every call site and inside the function.

Proposals:

- `useMapped`        (high)
- `isMapped`         (medium)
- `mappedAddressing` (low)
- keep current       (low — only if synthesis decides the surrounding
  documentation should also flip; not recommended)

Cross-reference:

- The lambda local capture comments (`/*mapped=*/true|false`) in
  `write_waves_to_elf.cpp` and `notes/elf_format.md` line 32 will need
  to follow whichever rename synthesis picks. Those files are not in
  this batch, so this is flagged for arbitration.

Locations consulted:

- declared: `include/zhinst/elf_writer.hpp:101-105`
- defined: `src/elf_writer.cpp:141-208`
- used (callers): `src/write_waves_to_elf.cpp:76-80, 157-161`
- binary verification: `_seqc_compiler.so` @0x293dbc–0x293e0a
- docs: `reconstructed/notes/elf_format.md:32`

### `ElfWriter::memoryOffset_`  [no / medium / not-misnomer]

Evidence:

- Declaration: `include/zhinst/elf_writer.hpp:123` —
  `uint64_t memoryOffset_;`.
- Used as PT_LOAD virtual/physical base in `addCode`
  (`src/elf_writer.cpp:90-91`).
- Used as ELF entry point in both `writeFile` overloads
  (`src/elf_writer.cpp:219, 237`: `set_entry(memoryOffset_)`).
- Setter `setMemoryOffset(uint64_t offset)`
  (`src/elf_writer.cpp:248-250`) — mangled symbol exists with that
  exact method name, so the public verb is fixed.

Interpretation:

- The field is the base address used for both code load address and
  ELF entry point — i.e. a "memory offset". The publicly-named setter
  `setMemoryOffset` further constrains the field to share its noun.

Judgement:

- No. The field name fits what the binary does with it and is
  consistent with the symbol-table-fixed setter name.

Locations consulted:

- declared: `include/zhinst/elf_writer.hpp:123`
- used:     `src/elf_writer.cpp:36, 90-91, 158-159, 219, 237, 250`

### `ElfWriter::ElfWriter::machineType`, `ElfWriter::prepareHeader::machineType`  [no / medium / not-misnomer]

Evidence:

- Both signatures take `uint16_t machineType` (`include/zhinst/elf_writer.hpp:68, 74`).
- Only call site in repo: `src/awg_compiler.cpp:756` —
  `ElfWriter elfWriter(2);`.
- `notes/elf_format.md:6-10` — main compiler output uses
  `e_machine = 2`, cache uses `e_machine = 3`. The values passed to
  this parameter are exactly the per-variant `e_machine` codes.
- The recon source comment in `prepareHeader` (`src/elf_writer.cpp:55-65`)
  notes a discrepancy in which ELFIO vtable slot the value goes to,
  but does not affect the parameter's *meaning*.

Interpretation:

- The parameter carries the value that ends up in the ELF header's
  e_machine field. "machineType" is a literal description of that value.

Judgement:

- No. Name reflects the role.

Locations consulted:

- declared: `include/zhinst/elf_writer.hpp:68, 74`
- defined:  `src/elf_writer.cpp:34-43, 55-65`
- caller:   `src/awg_compiler.cpp:756`
- docs:     `reconstructed/notes/elf_format.md:1-20`

### `ElfWriter::addWaveform::padSize`  [no / high / not-misnomer]

Evidence:

- Declaration: `include/zhinst/elf_writer.hpp:105` —
  `detail::AddressImpl<uint32_t> padSize`.
- Used at `src/elf_writer.cpp:158-159` —
  `set_virtual_address(wfPtr->addressValue - padSize);`.
- Used at `src/elf_writer.cpp:166-178` — gates emission of `.dd_<name>`
  section and is the size of its zero-fill payload
  (`std::string padding(padSize, '\0'); ddSec->append_data(padding.data(), padding.size());`).
- Caller-side derivation in `writeWavesToElfAbsolute`
  (`src/write_waves_to_elf.cpp:148-150`):
  `gap = wf->addressValue - currentOffset; alignMask = -elfAlignment_; padding = gap & alignMask;`
  i.e. an aligned padding count in bytes.

Interpretation:

- The value is exactly the byte size of the descriptor-data padding
  section that fills the gap between `currentOffset` and the
  waveform's target address. "padSize" is precise.

Judgement:

- No.

Locations consulted:

- declared: `include/zhinst/elf_writer.hpp:101-105`
- defined:  `src/elf_writer.cpp:141-208`
- caller:   `src/write_waves_to_elf.cpp:117-176`

### `ElfWriter::addCode::opcodes`  [no / high / not-misnomer]

Evidence:

- Declaration: `include/zhinst/elf_writer.hpp:81` —
  `std::vector<uint32_t> const& opcodes`.
- Used at `src/elf_writer.cpp:83-85`: data is reinterpret-cast to
  `char const*` and stored in a `.text` section with
  `SHF_ALLOC|SHF_EXECINSTR` and 64-byte alignment.
- `notes/elf_format.md:39` — `.text` content is described as
  "Assembled opcodes (`vector<uint32_t>`)".

Interpretation:

- The `vector<uint32_t>` is the assembled instruction stream of the
  AWG sequencer. The audit notes file uses the very same word.

Judgement:

- No.

Locations consulted:

- declared: `include/zhinst/elf_writer.hpp:81`
- defined:  `src/elf_writer.cpp:75-97`
- docs:     `reconstructed/notes/elf_format.md:39`

### `ElfWriter::addData::sectionName`  [no / high / not-misnomer]

Evidence:

- Declaration: `include/zhinst/elf_writer.hpp:84-85` —
  `std::string const& sectionName`.
- Used at `src/elf_writer.cpp:111` — `sections.add(sectionName)`
  (parameter is forwarded directly to ELFIO's section factory, which
  sets the section's name).
- Caller-side strings observed in the codebase that flow into this
  parameter include the well-known ELF section names listed in
  `AGENTS.md` (`.asm`, `.linenr`, `.waveforms`, `.wavemem`,
  `.channels`, `.nodes_json`, `.arguments`, `.version_json`,
  `.version_bin`, `.filename`, `.c`).

Interpretation:

- Parameter is literally the name of the ELF section being added.

Judgement:

- No.

Locations consulted:

- declared: `include/zhinst/elf_writer.hpp:84-85`
- defined:  `src/elf_writer.cpp:108-115`

### `ElfWriter::addWaveform::wfName2` (local)  [yes / medium / —]

Evidence:

- `src/elf_writer.cpp:168-185` — two locals are declared:
  `std::string wfName = wfPtr->name;` (inside the `padSize > 0` block)
  and `std::string wfName2 = wfPtr->name;` (after the block) for the
  `.wf_` section name.

Interpretation:

- The numeric suffix exists only because the first `wfName` is scoped
  inside an `if` block. It carries no additional meaning.

Judgement:

- Yes — `wfName2` is misleading-by-default ("…2" suggests a different
  value); it is identical to the first `wfName`. This is a local
  cleanup, not a binary-behaviour issue.

Proposals:

- Hoist `wfName` out of the `if` block and drop `wfName2`  (medium).
- Rename to `wfNameOuter`  (low).
- Keep current  (low).

Locations consulted:

- defined: `src/elf_writer.cpp:168, 185`

### `writeWavesToElfMapped::elfWriter`, `writeWavesToElfAbsolute::elfWriter`  [no / high / not-misnomer]

Evidence:

- Free-function names `writeWavesToElfMapped` and
  `writeWavesToElfAbsolute` are in the binary symbol table (mangled
  into the captured-lambda typeinfo strings at `0xb02640`,
  `0xb026c0`); the parameter is the `ElfWriter&` they target.

Interpretation:

- The parameter is the ElfWriter the wavetable is being serialized
  into; name matches that role and matches the type.

Judgement:

- No.

Locations consulted:

- declared/defined: `src/write_waves_to_elf.cpp:59-87, 117-173`

### `writeWavesToElfMapped::wavetable`, `writeWavesToElfAbsolute::wavetable`  [no / high / not-misnomer]

Evidence:

- Declared as `std::shared_ptr<WavetableIR> wavetable`
  (`src/write_waves_to_elf.cpp:61, 119`).
- Used as the receiver of `wavetable->forEachUsedWaveform(...)`
  (`src/write_waves_to_elf.cpp:86, 172`) and
  `wavetable->getFirstWaveformOffset()` (`:124`).

Interpretation:

- The parameter is exactly a WavetableIR, used through its public API.

Judgement:

- No.

Locations consulted:

- defined: `src/write_waves_to_elf.cpp:59-87, 117-173`

### `writeWavesToElfAbsolute::currentOffset` (local)  [no / high / not-misnomer]

Evidence:

- Initialized at `src/write_waves_to_elf.cpp:124` from
  `wavetable->getFirstWaveformOffset()`.
- Updated each iteration to
  `wf->addressValue + rawData->size()` (`:166`), i.e. a running cursor
  into the absolute address space across waveforms.

Interpretation:

- The variable is exactly a "current cursor offset" used to compute
  per-waveform padding; the name is descriptive.

Judgement:

- No.

Locations consulted:

- defined: `src/write_waves_to_elf.cpp:124, 148, 166`

## 4. Symbols inspected and judged routinely fine

- `ElfWriter::addData::data`, `::size` — generic but unambiguous in
  the role of "byte buffer + length to put in the section".
- `ElfWriter::writeFile(ostream&)::os` — standard short name for an
  `std::ostream&`.
- `ElfWriter::writeFile(string&)::filename` — matches usage.
- `ElfWriter::setMemoryOffset::offset` — matches the field it writes
  to.
- `ElfWriter::addWaveform::format` — `SampleFormat` parameter, used
  immediately as `Signal::getRawData(format)`.
- `ElfWriter::addWaveform::waveform` — the `shared_ptr<WaveformIR>`
  being serialized.
- `ElfWriter::addWaveform::rawDataSize` (local) — caches
  `rawData->size()` for the NOBITS branch; matches usage.
- Locals in `addWaveform`: `seg`, `sec`, `wfSec`, `ddSec`, `signal`,
  `wfPtr`, `alignment`, `gap`, `alignMask`, `padding`,
  `ddSectionName`, `wfSectionName` — all immediately readable in
  context.
- `writeWavesToElfMapped::config`, `writeWavesToElfAbsolute::config`
  — `AWGCompilerConfig const&`; only used to read `config.sampleFormat`,
  which itself is `not-misnomer` per batch 23.
- Lambda parameter `waveform` in both lambdas — `shared_ptr<WaveformIR>`,
  unambiguous.
- Lambda local `wf` (raw pointer alias for `waveform.get()`) — short
  but customary.
- `ElfWriter` itself, all method names, both free function names — out
  of scope (in symbol table).

## 5. Coverage

- **Fully covered:** all in-scope symbols of
  `include/zhinst/elf_writer.hpp`, `src/elf_writer.cpp`, and
  `src/write_waves_to_elf.cpp` — i.e. the field `memoryOffset_`, all
  parameters of all eight `ElfWriter` methods, all parameters of the
  two free functions, both lambda capture variables, and the
  noteworthy locals.
- **Deferred:** none.
- **Not covered (out of scope per RULES §2/§3):**
  - Type name `zhinst::ElfWriter` and all eight method names of
    `ElfWriter` — present verbatim in the binary symbol table
    (tier-1 exclusion, §3).
  - Free function names `writeWavesToElfMapped` and
    `writeWavesToElfAbsolute` — present verbatim in the binary
    symbol table (mangled into the lambda `__func` typeinfo),
    tier-1 exclusion.
  - All template parameters and member type aliases (none of
    interest in these files).
  - Cross-batch fields touched only as use-site evidence:
    `WaveformIR::elfAlignment_`, `WaveformIR::addressValue`,
    `WaveformIR::name`, `WaveformIR::used`, `WaveformIR::signal`
    (batch 16); `Signal::reserveOnly_`, `Signal::getRawData`
    (batch 37); `AWGCompilerConfig::sampleFormat` (batch 23);
    `RawWave::size`/`ptr` (batch 35); `WavetableIR::*` (separate
    batch); `WaveOrder::ByName`/`ByIndex` (separate batch). All
    are recorded only as evidence here, not re-judged.
