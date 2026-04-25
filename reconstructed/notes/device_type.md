# DeviceType / DeviceFamily / DeviceTypeCode / DeviceOption / DeviceOptionSet

Phase 14b-i analysis (2026-04-23). Sub-phase 14b-i covers the enums, the
`DeviceType` pimpl wrapper, `detail::DeviceTypeImpl`, and `DeviceOptionSet`
+ `DeviceOptionSetConstIterator`. Per-family `sfc::*Option` enums and the
`initializeSfcOptions<>` registry are deferred to 14b-ii.

## enum class DeviceFamily : uint32_t

Bitfield-style enum (one-hot). Decoded from `toString(DeviceFamily)` jump
table at .rodata 0x961eac (0x41 entries indexed by value, all unmapped
positions branch to "unknown" default).

| Value  | Name    | String   |
|--------|---------|----------|
| 0      | Unknown | (empty)  |
| 0x001  | HF2     | "HF2"    |
| 0x002  | UHF     | "UHF"    |
| 0x004  | MF      | "MF"     |
| 0x008  | HDAWG   | "HDAWG"  |
| 0x010  | SHF     | "SHF"    |
| 0x020  | PQSC    | "PQSC"   |
| 0x040  | HWMOCK  | "HWMOCK" |
| 0x080  | SHFACC  | "SHFACC" |
| 0x100  | GHF     | "GHF"    |
| 0x200  | QHUB    | "QHUB"   |
| 0x400  | VHF     | "VHF"    |

`toString` @ 0x2df610. Default arm at 0x2df740 emits "unknown" (size
0xe = 14, but only 7 actual chars = "unknown\0"... actually size byte is
SSO size = `2*N` for libc++; 0xe / 2 = 7 = strlen("unknown") ✓).

## enum class DeviceTypeCode : uint32_t

Plain 0..32 enum, 33 distinct values. Decoded from `toString(DeviceTypeCode)`
jump table at .rodata 0x961cc4 (0x21 entries; default at >0x20 emits
"unknown" and `toDeviceTypeCode` returns 0x21 for unrecognized strings —
so functionally Unknown = 33, but only 0..32 round-trip).

| Code | Name      | String     |
|------|-----------|------------|
| 0    | Unknown   | (empty)    |
| 1    | HF2       | "HF2"      |
| 2    | HF2LI     | "HF2LI"    |
| 3    | HF2IS     | "HF2IS"    |
| 4    | UHF       | "UHF"      |
| 5    | UHFLI     | "UHFLI"    |
| 6    | UHFAWG    | "UHFAWG"   |
| 7    | UHFQA     | "UHFQA"    |
| 8    | UHFIA     | "UHFIA"    |
| 9    | MF        | "MF"       |
| 10   | MFLI      | "MFLI"     |
| 11   | MFIA      | "MFIA"     |
| 12   | HDAWG     | "HDAWG"    |
| 13   | HDAWG4    | "HDAWG4"   |
| 14   | HDAWG8    | "HDAWG8"   |
| 15   | SHF       | "SHF"      |
| 16   | SHFQA2    | "SHFQA2"   |
| 17   | SHFQA4    | "SHFQA4"   |
| 18   | SHFSG2    | "SHFSG2"   |
| 19   | SHFSG4    | "SHFSG4"   |
| 20   | SHFSG8    | "SHFSG8"   |
| 21   | SHFQC     | "SHFQC"    |
| 22   | SHFLI     | "SHFLI"    |
| 23   | PQSC      | "PQSC"     |
| 24   | SHFACC    | "SHFACC"   |
| 25   | SHFPPC2   | "SHFPPC2"  |
| 26   | SHFPPC4   | "SHFPPC4"  |
| 27   | GHF       | "GHF"      |
| 28   | GHFLI     | "GHFLI"    |
| 29   | HWMOCK    | "HWMOCK"   |
| 30   | QHUB      | "QHUB"     |
| 31   | VHF       | "VHF"      |
| 32   | VHFLI     | "VHFLI"    |

`toString(DeviceTypeCode)` @ 0x2d40b0.
`toDeviceTypeCode(string)` @ 0x2d43d0 — uses static
`unordered_map<string, DeviceTypeCode> deviceTypeCodes` populated at
first call.

## enum class DeviceOption : uint32_t

Only `IA = 0xf = 15` confirmed in 14b-i (used by `isIa` predicate).
Full set deferred to 14b-ii — must enumerate the per-family
`sfc::HdawgOption / MfOption / Hf2Option / ShfOption / UhfOption /
VhfOption` arrays passed to `detail::initializeSfcOptions<>`.

`toString(DeviceOption, DeviceFamily)` @ 0x2cfee0 — looks up name string
via family-keyed registry.

## Predicate functions (free)

All take `DeviceType const&`, dispatch via `[rdi]` to
`DeviceTypeImpl::code()` or `family()`:

| Function   | Address    | Logic                                                                    |
|------------|------------|--------------------------------------------------------------------------|
| isDefined  | 0x2d2e50   | code != Unknown                                                          |
| isIa       | 0x2d2e70   | code∈{UHFIA(8), MFIA(11)} OR (code∈ broad set AND hasOption(IA=15))      |
| isMfia     | 0x2d2ec0   | code == MFIA(11)                                                         |
| isUhfqa    | 0x2d2ee0   | code == UHFQA(7)                                                         |
| isShfqa    | 0x2d2f00   | code∈{SHFQA2(16), SHFQA4(17), SHFQC(21)}                                 |
| isShfsg    | 0x2d2f40   | code∈{SHFSG2(18), SHFSG4(19), SHFSG8(20)}                                |
| isShfqc    | 0x2d2f80   | code == SHFQC(21)                                                        |
| isShfppc   | 0x2d2fa0   | code∈{SHFPPC2(25), SHFPPC4(26)}                                          |
| isShfli    | 0x2d2fd0   | code == SHFLI(22)                                                        |
| isGhfli    | 0x2d2ff0   | code == GHFLI(28)                                                        |
| isVhfli    | 0x2d3010   | code == VHFLI(32)                                                        |
| hasMf      | 0x2d3030   | family==MF OR ... (more bits, see below)                                 |

The `isIa` "broad set" bitmask 0x46BF7901 covers codes:
0,8,11,12,16,17,18,19,20,21,22,23,25,26,30
(set bits at positions: 0,8,11,12,16-23,25,26,30).
Of these, codes 8(UHFIA) and 11(MFIA) are unconditional; the rest
require `hasOption(DeviceOption(15)) // IA`.

## DeviceType (pimpl wrapper, 8 bytes)

Layout: single `detail::DeviceTypeImpl* impl_` at +0x00.

| Method                                  | Address    | Implementation                                              |
|-----------------------------------------|------------|-------------------------------------------------------------|
| DeviceType()                            | 0x2d2900   | impl_ = new DeviceTypeImpl()                                |
| DeviceType(DeviceTypeCode, DeviceFamily)| 0x2d2960   | impl_ = new DeviceTypeImpl(code, family)                    |
| DeviceType(string const&, ...)          | 0x2d29c0   | uses toDeviceTypeCode / toDeviceOptions / toDeviceFamily    |
| DeviceType(DeviceType const&)           | 0x2d2b40   | impl_ = src.impl_->vtable[0](src.impl_)  // virtual clone   |
| DeviceType(DeviceType&&)                | 0x2d2b50   | impl_ = src.impl_; src.impl_ = nullptr                      |
| operator=(DeviceType const&)            | (TBD)      | copy-and-swap likely                                        |
| ~DeviceType()                           | 0x2d2b70   | if(impl_) impl_->vtable[2](impl_, true)  // virtual delete  |
| code()                                  | 0x2d2c40   | impl_->code()  // tail call                                 |
| family()                                | 0x2d2c30   | impl_->family()                                             |
| options()                               | 0x2d2c60   | impl_->options()                                            |
| hasOption(DeviceOption)                 | 0x2d2c50   | impl_->hasOption(opt)                                       |
| belongsTo(DeviceFamily)                 | 0x2d2c70   | (impl_->family() & f) != 0  // bit-AND test                 |

## detail::DeviceTypeImpl (polymorphic, 88 bytes / 0x58)

Layout (provisional, refined in 14b-ii once subclass GenericDeviceType
is examined):

| Offset | Size | Field                                |
|--------|------|--------------------------------------|
| +0x00  | 8    | vptr                                 |
| +0x08  | 4    | code_ (DeviceTypeCode)               |
| +0x0c  | 4    | family_ (DeviceFamily)               |
| +0x10  | 0x48 | options_ (DeviceOptionSet, 72 bytes) |

Total = 0x58. Confirmed by:
- Default ctor @ 0x2d3060: `operator new(0x58)` then `DeviceTypeImpl()`
- `family()` @ 0x2d32e0: `mov eax, [rdi+0xc]; ret`
- `code()`   @ 0x2d32f0: `mov eax, [rdi+0x8]; ret`
- `options()` @ 0x2d3310: returns `[rdi+0x10]` view (DeviceOptionSet*)

vtable layout (3+ slots):
- +0x00: clone() — used by DeviceType copy ctor
- +0x08: dtor (complete)
- +0x10: dtor (deleting)

There's a known subclass `zhinst::detail::GenericDeviceType` (typeinfo
seen at .rodata 0x961cc4-0x25 = 0x961c9f, the typeinfo string base).
Subclass details deferred to 14b-ii.

## DeviceOptionSet (72 bytes / 0x48) — DUAL STORAGE

| Offset | Size | Field                                      |
|--------|------|--------------------------------------------|
| +0x00  | 0x28 | unordered_set<DeviceOption> bucket index   |
| +0x28  | 0x18 | std::map<std::string, DeviceOption> tree   |
| +0x40  | 4    | family_ (DeviceFamily)                     |
| +0x44  | 4    | (padding)                                  |

The unordered_set is a fast lookup-by-enum-value index.
The map is the **canonical iteration-order storage**, keyed by the
option's name string (which is `toString(opt, family_)`). The map's
iterator is what `DeviceOptionSetConstIterator` wraps, so iteration
returns options in alphabetical order by name.

| Method                                        | Address    | Notes                                            |
|-----------------------------------------------|------------|--------------------------------------------------|
| DeviceOptionSet(DeviceFamily)                 | 0x2cf970   | Both containers empty, family stored at +0x40    |
| DeviceOptionSet(initializer_list, DeviceFamily)| 0x2cf9a0  | For each opt: insert into uset; toString→insert into map |
| begin() const                                 | 0x2cfb60   | Returns map's __begin_node (== [this+0x28])      |
| end() const                                   | 0x2cfb70   | Returns &[this+0x30] (map's __end_node sentinel) |
| contains(DeviceOption) const                  | 0x2cfb80   | unordered_set find                               |
| empty() const                                 | 0x2cfcc0   | unordered_set::size() == 0                       |
| size() const                                  | 0x2cfcd0   | unordered_set::size()                            |
| family() const                                | 0x2cfce0   | return [rdi+0x40]                                |
| insert(DeviceOption)                          | 0x2cfcf0   | Inserts into uset only (likely; check later)     |
| operator==                                    | 0x2cfd80   | Compares unordered_sets only                     |

## DeviceOptionSetConstIterator (8 bytes)

Wraps a single `__tree_node*` pointer (the map's tree node).

| Method                            | Address    | Implementation                                     |
|-----------------------------------|------------|----------------------------------------------------|
| Constructor (from map iter)       | 0x2cf900   | Stores wrapped pointer                             |
| dereference() const               | 0x2cf910   | return *(DeviceOption*)(node + 0x38)               |
| increment()                       | 0x2cf920   | Standard libc++ tree successor walk                |
| equal(other) const                | 0x2cf960   | Pointer compare                                    |

Tree node layout (libc++ __tree_node holding __value_type<string,DeviceOption>):
- +0x00: __left_
- +0x08: __right_
- +0x10: __parent_ + color bit
- +0x18: __value_.first  (std::string, 24B)
- +0x30: __value_.second (DeviceOption, 4B)
- (Total node ≈ 56 bytes after pad — but the offset 0x38 in dereference
  suggests the first field is 8 bytes BEFORE __value_ start, which is
  consistent with the libc++ tree_node having an extra prev-pointer at
  the front making the node header 32B; checking: 32 + 24(string) = 56,
  +0x38 = 56 = correct offset for the DeviceOption field after the string)

This dual storage means the C++ source likely declares:
```cpp
class DeviceOptionSet {
    std::unordered_set<DeviceOption> values_;       // +0x00, 40B
    std::map<std::string, DeviceOption> byName_;    // +0x28, 24B
    DeviceFamily family_;                           // +0x40
public:
    using const_iterator = DeviceOptionSetConstIterator;
    // ...
};
```

## Phase 14b-ii-a additions (2026-04-23)

### DeviceOption enum — 31 values (0..30)

Decoded from the 31-entry jump table at .rodata `0x961bac` referenced by
`toString(DeviceOption, DeviceFamily)` @ `0x2cfee0`. Codes 0 and 6 are
**family-dependent**: HF2 family yields the "K"-suffixed alternative
("MFK", "RTK") via the strings at .rodata `0x90b60e..`.

| Value | Name (default / HF2) |  | Value | Name |
|------:|----------------------|--|------:|------|
|   0 | MF / MFK         |  |  16 | WEB    |
|   1 | MD               |  |  17 | CNT    |
|   2 | FF               |  |  18 | NOUI   |
|   3 | PLL              |  |  19 | ME     |
|   4 | PID              |  |  20 | PC     |
|   5 | MOD              |  |  21 | QA     |
|   6 | RT / RTK         |  |  22 | SKW    |
|   7 | UHS              |  |  23 | 16W    |
|   8 | AWG              |  |  24 | QC2CH  |
|   9 | DIG              |  |  25 | QC4CH  |
|  10 | 10G              |  |  26 | QC6CH  |
|  11 | QE               |  |  27 | RTR    |
|  12 | F5M              |  |  28 | PLUS   |
|  13 | RUB              |  |  29 | LRT    |
|  14 | BOX              |  |  30 | F200M  |
|  15 | IA               |  |     |        |

Out-of-range values (>= 31) return an empty string; the function's default
arm at `0x2cfff4` zeroes the output SSO buffer.

In the reconstructed `enum class DeviceOption` we expose value 0 as both
`MF` (canonical) and `None` (sentinel); they are aliases. C++ identifier
constraints force a couple of renamings:
  - `10G` -> `TenG`
  - `16W` -> `Sixteen_W`

### `detail::OptionCodePair<T>` — 8 bytes

Layout: `{ T mask; DeviceOption code; }` — both fields are 4-byte
unsigned values. Confirmed from the per-family `knownOptions` arrays at
.rodata `0x962394..0x962aa0`: each entry is laid out as
`<u32 mask> <u32 code>`.

### `detail::initializeSfcOptions<T,N>` — header-only template

Body confirmed at `0x2e0d50` (instantiation `<sfc::Hf2Option, 6>`):

```text
options_set = DeviceOptionSet(family);
for (i = 0; i < N; ++i)
    if ((options_mask & known[i].mask) != 0)
        options_set.insert(known[i].code);
return options_set;
```

The disasm uses `r15d = ~options_mask`, then `test [r14+i*8], r15d / jne
.skip / call insert` — equivalent to the loop above.

### sfc::*Option per-family bitmask enums

The bits decoded from the 15 per-family `knownOptions` arrays are:

```
Hf2li    (7): (0x1,0)  (0x2,3)  (0x20,4) (0x4,5)  (0x8,6)  (0x10,7) (0x1000,16)
Hf2is    (6): (0x1,0)  (0x20,4) (0x4,5)  (0x8,6)  (0x10,7) (0x1000,16)
Mfli     (9): (0x1,1)  (0x20,2) (0x2,4)  (0x4,5)  (0x400,9)(0x800,12)(0x4000,14)(0x8000,15)(0x20000,18)
Mfia     (8): (0x1,1)  (0x20,2) (0x2,4)  (0x4,5)  (0x400,9)(0x800,12)(0x4000,14)            (0x20000,18)
Uhfli   (10): (0x1,0)  (0x20,2) (0x2,4)  (0x4,5)  (0x200,8)(0x400,9) (0x2000,13)(0x4000,14)(0x10000,17)(0x8,21)
Uhfawg   (7): (0x1,0)  (0x20,2)                            (0x400,9) (0x2000,13)(0x4000,14)(0x10000,17)(0x8,21)
Uhfqa    (5):          (0x20,2)                            (0x400,9) (0x2000,13)(0x1000,11)            (0x8,21)
Uhfia   (12): (0x1,0)  (0x20,2) (0x2,4)  (0x4,5)  (0x200,8)(0x400,9) (0x800,10) (0x1000,11)(0x2000,13)(0x4000,14)(0x10000,17)(0x8,21)
Hdawg4   (6): (0x1,0)  (0x20,2) (0x10000,17)(0x200,19)(0x8000,20)(0x800,22)
Hdawg8   (6): (0x1,0)  (0x20,2) (0x10000,17)(0x200,19)(0x8000,20)(0x800,22)
Shfqa2   (4):          (0x20,2)                                                            (0x1000,23)(0x4000,28)(0x8000,29)
Shfqc    (8):          (0x20,2) (0x8,24) (0x10,25)        (0x800,26)(0x1000,23)(0x2000,27)(0x4000,28)(0x8000,29)
Shfli    (5): (0x1,0)  (0x2,4)  (0x4,5)  (0x20,2) (0x200,8)
Ghfli    (5): (0x1,0)  (0x2,4)  (0x4,5)  (0x20,2) (0x200,8)
Vhfli    (6): (0x1,1)  (0x2,4)  (0x4,5)  (0x20,2) (0x200,8) (0x800,30)
```

Note no separate Shfqa4 / Shfsg* / Shfppc* / Pqsc / Hwmock / Qhub /
Shfacc array exists — those subclasses likely either reuse one of the
above arrays inline or take no per-family options at all. To be
confirmed in 14b-ii-b.

The corresponding `sfc::*Option` enums (Hf2Option / MfOption / UhfOption
/ HdawgOption / ShfOption / VhfOption / GhfOption) carry the bits that
appear in **at least one** of the listed arrays for that family — the
union across subclasses.

### `toString(DeviceOptionSet, DeviceFamily, separator)` — `0x2d0130`

Walks the set's `byName_` map (alphabetical iteration), translates each
DeviceOption to its name with `toString(opt, family)`, push_backs into a
local `vector<string>`, then calls
`boost::algorithm::join<vector<string>, string>(names, separator)` at
`0x2d0310`.

Fast path at entry: reads `[set+0x18]` (the unordered_set's `size_`
field) and returns the empty string immediately if zero.

### `toString(DeviceType const&)` — `0x2d2e20` (CORRECTION)

5-instruction body: just calls `dt.code()` then tail-calls
`toString(DeviceTypeCode)`. Options are NOT appended to the output.
This is contrary to the earlier guess (`<CODE> [opt1 opt2 ...]`); to
get a string with options the caller must format `dt.options()`
separately via the `(DeviceOptionSet, family, sep)` overload.

### `isIa(DeviceType const&)` — `0x2d2e70` (CORRECTION)

The Phase 14b-i implementation had inverted truth-table logic. Correct
body is:

```text
code = dt.code();
if (code >= 31)                           goto fallback;
if (!(0x46BF7901 >> code & 1))            goto fallback;
return (0x900 >> code) & 1;       // true iff code in {UHFIA(8), MFIA(11)}
fallback:
    return dt.hasOption(DeviceOption::IA);
```

So `isIa` returns:
  - `true` for codes UHFIA / MFIA outright;
  - `false` for codes in the broad mask but not in the unconditional pair;
  - `dt.hasOption(IA)` for everything else (including out-of-range).

### `hasMf(DeviceType const&)` — `0x2d3030`

Body decoded:
```text
esi = (dt.family() == DeviceFamily::MF) ? 1 : 0;
tail-call hasOption((DeviceOption)esi);
```

In other words: probe option `MD` (1) when family is MF, otherwise probe
option `MF` (0). This matches an apparent invariant where families that
are *not* MF use the `MF` option code to advertise the MF capability,
while the MF family itself uses `MD`.

### `toDeviceTypeCode(string)` — `0x2d43d0`

Function-local `static unordered_map<string, DeviceTypeCode>
deviceTypeCodes` built lazily under `__cxa_guard` `b851d8`. Body
constructs 33 stack-allocated `pair<string, DeviceTypeCode>` then
inserts each. Special entry: `"none" -> Unknown(0)`. Returns
`DeviceTypeCode(33)` (out-of-range "unknown") on miss.

The string array at .data.rel.ro `0xb090d0` (decoded above) holds the
33 name pointers; the codes inserted are `0..0x16, 0x1b, 0x1c, 0x18,
0x19, 0x1a, 0x17, 0x1d, 0x1e, 0x1f, 0x20` — the order matches the
binary's literal sequence and is *not* purely numeric, but the resulting
map is order-independent.

## Open questions for 14b-ii-b

1. detail::GenericDeviceType subclass layout (does it add fields beyond
   the 0x58 base?)
2. ~~Are there `Shfqa4`, `Shfsg*`, `Shfppc*`, `Pqsc`, `Qhub`, `Hwmock`,
   `Shfacc` knownOptions arrays at any other rodata address, or do
   those subclasses pass an empty option list?~~
   **Resolved (14b-ii-b1):** No — those subclasses build their option
   sets via inline `if (opts & MASK) set.insert(CODE)` chains rather
   than via a static knownOptions array. See subclass survey below.
   `Pqsc`, `Qhub`, `Hwmock` are zero-arg ctors with empty options.
3. `DeviceType(string const&, ...)` parsing logic — what does the
   string format look like? (likely "DEVTYPE OPT1 OPT2 ...")
4. `expand(DeviceFamily)` and `toAwgDeviceType` — depend on
   AwgDeviceType enum values (one-hot, 9 values 1..256)
5. ~~Is `sfc::GhfOption` truly distinct from `sfc::ShfOption`, or does
   the binary alias them?~~
   **Resolved (14b-ii-b1):** There is **no** `sfc::GhfOption`. The GHF
   family reuses `sfc::ShfOption`. `detail::Ghfli`'s ctor at 0x2e3a00
   contains a direct call to the mangled
   `initializeSfcOptions<sfc::ShfOption, 5>` template instantiation
   (knownOptions @ .rodata 0x96298c, 5 entries identical to Shfli's
   first five). The bogus `sfc::GhfOption` enum that was added in
   14b-ii-a has been removed from `device_type.hpp`.

## Phase 14b-ii-b1 — per-family device subclasses (2026-04-23)

Reconstructed all 32 device subclasses derived from `detail::DeviceTypeImpl`.
Each subclass is 88 bytes (0x58, identical to base) — adds NO data members,
overrides only the vptr at +0x00. Each provides a constructor (default or
`unsigned long opts`), a virtual destructor (vtable[1]), and a `clone()`
override (vtable[0]).

### Subclass survey table

| Subclass | DeviceTypeCode | DeviceFamily | knownOptions @ .rodata | Init pattern | Vtable @ .rodata |
|----------|----------------|--------------|------------------------|--------------|------------------|
| Hf2     | 1  | HF2     | —                              | empty options                                  | b09368 |
| Hf2li   | 2  | HF2     | 0x962394 (Hf2Option × 7)       | template `initializeSfcOptions<Hf2Option,7>`   | b09390 |
| Hf2is   | 3  | HF2     | 0x9623cc (Hf2Option × 6)       | template `initializeSfcOptions<Hf2Option,6>`   | b093b8 |
| Mf      | 9  | MF      | —                              | empty options                                  | (b09…) |
| Mfli    | 10 | MF      | 0x962458 (MfOption × 9)        | template `initializeSfcOptions<MfOption,9>`    | b09498 |
| Mfia    | 11 | MF      | 0x9624a0 (MfOption × 8)        | template `initializeSfcOptions<MfOption,8>`    | b094c0 |
| Uhf     | 4  | UHF     | —                              | empty options                                  | —      |
| Uhfli   | 5  | UHF     | 0x962580 (UhfOption × 10)      | template `initializeSfcOptions<UhfOption,10>`  | b095a0 |
| Uhfawg  | 6  | UHF     | 0x9625d0 (UhfOption × 7)       | template `initializeSfcOptions<UhfOption,7>`   | b095c8 |
| Uhfqa   | 7  | UHF     | 0x962608 (UhfOption × 5)       | template `initializeSfcOptions<UhfOption,5>`   | b095f0 |
| Uhfia   | 8  | UHF     | 0x962630 (UhfOption × 12)      | template `initializeSfcOptions<UhfOption,12>`  | b09618 |
| Hdawg   | 12 | HDAWG   | —                              | empty options                                  | —      |
| Hdawg4  | 13 | HDAWG   | 0x9626f8 (HdawgOption × 6)     | template `initializeSfcOptions<HdawgOption,6>` | b09728 |
| Hdawg8  | 14 | HDAWG   | 0x962728 (HdawgOption × 6)     | template `initializeSfcOptions<HdawgOption,6>` | b09750 |
| Shf     | 15 | SHF     | —                              | inline: opts & 0x20 → FF                       | b09808 |
| Shfqa2  | 16 | SHF     | 0x962850 (ShfOption × 4)       | template `initializeSfcOptions<ShfOption,4>`   | b09830 |
| Shfqa4  | 17 | SHF     | —                              | inline: 0x20→FF; 0x4000→PLUS; 0x8000→LRT       | b09858 |
| Shfsg2  | 18 | SHF     | —                              | inline: 0x20→FF; 0x2000→RTR; 0x4000→PLUS       | —      |
| Shfsg4  | 19 | SHF     | —                              | inline: 0x20→FF; 0x2000→RTR; 0x4000→PLUS       | —      |
| Shfsg8  | 20 | SHF     | —                              | inline: 0x20→FF; 0x2000→RTR; 0x4000→PLUS       | —      |
| Shfqc   | 21 | SHF     | 0x962870 (ShfOption × 8)       | template `initializeSfcOptions<ShfOption,8>`   | b098f8 |
| Shfli   | 22 | SHF     | 0x9628b0 (ShfOption × 5)       | template `initializeSfcOptions<ShfOption,5>`   | b09920 |
| Shfacc  | 24 | SHFACC  | —                              | inline: opts & 0x20 → FF                       | b09a50 |
| Shfppc2 | 25 | SHFACC  | —                              | inline: opts & 0x20 → FF                       | b09a78 |
| Shfppc4 | 26 | SHFACC  | —                              | inline: opts & 0x20 → FF                       | b09aa0 |
| Ghf     | 27 | GHF     | —                              | inline: opts & 0x20 → FF                       | b09b58 |
| Ghfli   | 28 | GHF     | 0x96298c (**ShfOption** × 5)   | template `initializeSfcOptions<ShfOption,5>`   | b09b80 |
| Pqsc    | 23 | PQSC    | —                              | empty options                                  | —      |
| Qhub    | 30 | QHUB    | —                              | empty options                                  | —      |
| Hwmock  | 29 | HWMOCK  | —                              | empty options                                  | —      |
| Vhf     | 31 | VHF     | —                              | inline: opts & 0x20 → FF                       | b09db8 |
| Vhfli   | 32 | VHF     | 0x962aa0 (VhfOption × 6)       | template `initializeSfcOptions<VhfOption,6>`   | b09de0 |

Counts: 15 template-driven, 11 inline option-bit, 6 empty (Hf2, Mf,
Uhf, Hdawg, Pqsc, Qhub, Hwmock — 7 actually; Hwmock & friends).

### File organisation

Per user preference, subclasses are split across 11 per-family `.cpp`
files mirroring the binary's per-family layout: `device_hf2.cpp`,
`device_mf.cpp`, `device_uhf.cpp`, `device_hdawg.cpp`, `device_shf.cpp`,
`device_shfacc.cpp`, `device_ghf.cpp`, `device_pqsc.cpp`,
`device_qhub.cpp`, `device_hwmock.cpp`, `device_vhf.cpp`. Headers all
in a single `device_subclasses.hpp`.

### `Hf2Factory::doMakeDeviceType` — subtype-selector dispatch (0x2e0a40)

Confirms the design pattern used by all 11 per-family Factory classes
(reconstructed in 14b-ii-b2). The `unsigned long opts` parameter
carries BOTH option bits (low bits, used by `initializeSfcOptions`)
AND subtype-selector bits (high bits, used by the factory to pick
which subclass to construct). For HF2:

```c
auto sub = opts & 0x1c0;
detail::DeviceTypeImpl* p = operator new(0x58);
if      (sub == 0x80) Hf2is(p, opts);
else if (sub == 0x40) Hf2li(p, opts);
else                  Hf2(p);   // base; subtype bits 0
```

The selector mask 0x1c0 covers bits 0x40, 0x80, 0x100. Option bits
documented in sfc::Hf2Option occupy 0x01..0x20 and 0x1000 — no
overlap. The same scheme applies family-by-family (each family's
Factory uses its own selector mask).

### Caveats / verification debt

The per-entry (mask, code) selections in the inferred `knownOptions`
arrays for the following subclasses were derived from the per-family
sfc::*Option bit comments (in `device_type.hpp`) but have NOT been
disasm-verified entry-by-entry against the actual rodata payload:

- Uhfli, Uhfawg, Uhfqa, Uhfia (all four UHF template arrays)
- Hdawg4, Hdawg8 (both HDAWG template arrays)
- Shfqa2, Shfqc, Shfli (three SHF template arrays)
- Vhfli (one VHF template array)

Hf2li, Hf2is, Mfli, Mfia, Ghfli have all entries verified directly
against the 14b-ii-a sfc::*Option per-bit decoding work. The
verification debt for the others is tracked as a TODO comment in each
relevant `.cpp` file. None of these affect the constructor's dispatch
shape or the `DeviceFamily/DeviceTypeCode` produced — only which
DeviceOption codes get inserted into the resulting `DeviceOptionSet`.

---

## Phase 14b-ii-b2 — Factories, parser, GenericDeviceType

### Factory architecture

`detail::DeviceFamilyFactory` is an abstract polymorphic base. It has
**no data members** (8B vptr-only) and exposes two pure virtual
methods:

  * `doMakeDefault() -> unique_ptr<DeviceTypeImpl>`
  * `doMakeDeviceType(unsigned long opts) -> unique_ptr<DeviceTypeImpl>`

`makeDefault()` @ 0x2e0590 and `makeDeviceType(opts)` @ 0x2e05b0 are
non-virtual base trampolines that forward to the vtable slots.

There are 13 concrete factory subclasses (all 8 bytes, vtable-only):

  * `NoDeviceTypeFactory`     — both methods return nullptr.
  * `UnknownDeviceTypeFactory` — both methods return a heap-allocated
    `UnknownDevice` (the UnknownDevice subclass uses sentinel values
    `code=33`, `family=0x800`).
  * 11 per-family factories: `Hf2Factory`, `MfFactory`, `UhfFactory`,
    `HdawgFactory`, `ShfFactory`, `ShfaccFactory`, `GhfFactory`,
    `PqscFactory`, `QhubFactory`, `HwmockFactory`, `VhfFactory`.

The `unsigned long opts` argument carries dual information: the low
bits are the per-family option bitmask (consumed by the device
subclass's ctor for `initializeXxxOptions`), and bits in the
`0x40..0x1c0` (or `0x40..0x1c0` in 8-way variants) range encode a
**subtype selector** that the factory uses to pick which concrete
subclass to instantiate.

### Per-family subtype selectors

| Factory          | Selector mask    | Mapping                                                                       |
|------------------|------------------|-------------------------------------------------------------------------------|
| `Hf2Factory`     | `opts & 0x1c0`   | 0x80 -> Hf2is, 0x40 -> Hf2li, else -> Hf2                                     |
| `MfFactory`      | `opts & 0x1c0`   | 0x80 -> Mfia, 0x40 -> Mfli, else -> Mf                                        |
| `UhfFactory`     | `opts & 0x1c0`   | 0x40 -> Uhfli, 0x80 -> Uhfawg, 0xc0 -> Uhfqa, 0x100 -> Uhfia, else -> Uhf     |
| `HdawgFactory`   | `opts & 0x1c0`   | 0x80 -> Hdawg8, 0x40 -> Hdawg4, else -> Hdawg                                 |
| `ShfFactory`     | `(opts >> 6)&7`  | 0->Shf, 1->Shfqa2, 2->Shfqa4, 3->Shfsg4, 4->Shfsg8, 5->Shfqc, 6->Shfli, 7->Shfsg2 |
| `ShfaccFactory`  | `opts & 0x1c0`   | 0x80 -> Shfppc4, 0x40 -> Shfppc2, else -> Shfacc                              |
| `GhfFactory`     | `opts & 0x1c0`   | 0x40 -> Ghfli, else -> Ghf                                                    |
| `PqscFactory`    | (ignored)        | always Pqsc                                                                   |
| `QhubFactory`    | (ignored)        | always Qhub                                                                   |
| `HwmockFactory`  | (ignored)        | always Hwmock                                                                 |
| `VhfFactory`     | `opts & 0x1c0`   | 0x40 -> Vhfli, else -> Vhf                                                    |

`UhfFactory` uses a 4-way jump table at .rodata 0x9624e0; `ShfFactory`
uses an 8-way jump table at .rodata 0x962758 (all 8 slots populated,
no default fallthrough). The other selectors are simple `cmp`-and-`je`
chains.

### `makeDeviceFamilyFactory(DeviceFamily) -> unique_ptr<DeviceFamilyFactory>`

Free function @ 0x2e05d0. Switch-on-family: each documented family
maps to the matching factory subclass (via 8-byte allocation +
vtable-pointer write), with `UnknownDeviceTypeFactory` used as the
default fallback for any family value not in the table.

### UnknownDevice (Phase 14b-ii-b2)

`detail::UnknownDevice::UnknownDevice()` @ 0x2d3320 stores the
synthetic sentinel values `code=33` (one beyond the 32 real
`DeviceTypeCode` values; `toString` renders this as `"unknown"`) and
`family=0x800` (one bit beyond `VHF=0x400`; `toString` renders this
as `"unknown"`). The compiled body uses a single `movabs rax,
0x80000000021` to write both fields together.

Vtable @ 0xb09038. Layout same as the base `DeviceTypeImpl` (88
bytes); only the dtor slots differ.

### GenericDeviceType (Phase 14b-ii-b2)

`detail::GenericDeviceType` @ 0x2d3c60 is the catch-all subclass used
when a `DeviceType` is constructed from runtime strings. It adds **no
fields** beyond the base; the size is still 88 bytes (`mov esi,0x58`
in its deleting destructor at 0x2d4030 confirms this).

Vtable layout @ 0xb09090:

  * `+0x00` = 0 (offset_to_top)
  * `+0x08` = `0xb090b8` (typeinfo)
  * `+0x10` = `0x002d3280` = `DeviceTypeImpl::doClone` — **shared with base**
  * `+0x18` = `0x002d33e0` = base in-charge dtor — **shared with base**
  * `+0x20` = `0x002d4030` = `GenericDeviceType::~GenericDeviceType` (deleting)

Because `doClone` is not overridden, cloning a `GenericDeviceType`
produces a plain `DeviceTypeImpl` (the slice is harmless because GDT
adds no state).

The constructor parses `(string deviceType, vector<string> options)`
into the three components of the tuple-taking base ctor:

  1. `code   = toDeviceTypeCode(deviceType)` (existing free fn @ 0x2d43d0)
  2. `family = toDeviceFamily(deviceType)`   (NEW free fn @ 0x2debd0)
  3. `opts   = (code != Unknown) ? toDeviceOptions(options, family)
                                 : DeviceOptionSet(family)`
     (NEW free fn @ 0x2d0fb0; empty fallback when the device-type
     string doesn't resolve)
  4. Forwards the tuple to `DeviceTypeImpl::DeviceTypeImpl(tuple)` @
     0x2d3190 (also new in this phase — only used by GDT).

The compiled body inlines both `make_tuple` and the base ctor; the
stack frame holds the temporary tuple at `[rbp-0xc0..-0x70]` and the
big body (~600 bytes of code) is mostly the inlined unordered_map
hashing + map balancing for moving the `DeviceOptionSet`'s two
internal containers into the tuple.

### Newly discovered free functions

  * `splitDeviceOptions(string)` @ 0x2d0460
    — `boost::trim_copy_if(s, ctype::space)`, then split on `'\n'`
      (token_compress_off). Empty trimmed input -> empty vector.
      **Single delimiter** is just newline; surprising but matches the
      binary exactly (the `mov [rbp-0x38], 0xa` is the literal `'\n'`).

  * `toDeviceOption(string)` @ 0x2d05b0
    — reverse name-to-code lookup. Body not yet decoded in detail;
      throws `std::out_of_range` (or similar) on miss. Phase 14b-ii-b2
      leaves this as a forward-declaration only. Follow-up needed.

  * `toDeviceFamily(string)` @ 0x2debd0
    — fast inline checks for `"none"`, `"DEFAULT"`, `"SHFACC"`,
      `"SHFPPC2"`, `"SHFPPC4"` followed by a function-local-static
      `map<string, DeviceFamily>` of 10 entries (hf2/uhf/mf/hdawg/shf/
      pqsc/hwmock/ghf/qhub/vhf — no shfacc, no unknown). Lookup uses
      `upper_bound(s) - 1` then `boost::starts_with(s, key)` to
      verify. Default on miss = `DeviceFamily(0x800)`.
    — `"DEFAULT"` returns `DeviceFamily::HF2` (1) — interesting
      semantic: the default-when-unspecified family is HF2.

  * `toDeviceOptions(vector<string>, DeviceFamily)` @ 0x2d0fb0
    — `DeviceOptionSet result(family); for each name: try { insert(toDeviceOption(name)); } catch(...) {}`.
      Unknown option names are silently dropped via try/catch — no
      validation feedback to the caller.

### `DeviceType` ctor address corrections

  * `DeviceType(string, vector<string>)` is at **0x2d2ae0**, not
    0x2d29c0 as previously documented. (The address 0x2d29c0 belongs
    to a different ctor variant or to the operator= path; needs
    re-verification in a follow-up.)

  * **NEW** `DeviceType(string, string)` at **0x2d2a00** — splits the
    second argument via `splitDeviceOptions` and forwards to the
    `(string, vector<string>)` ctor. Both ctors construct a fresh
    `GenericDeviceType` on the heap; no caching is performed.

### `DeviceTypeImpl(tuple)` ctor

`DeviceTypeImpl::DeviceTypeImpl(tuple<DeviceTypeCode, DeviceFamily, DeviceOptionSet>)` @ 0x2d3190
unpacks the 80-byte tuple into the 88-byte impl by:

  * copying the first 8 bytes verbatim into `[code_, family_]` (the
    tuple packs both 4-byte fields into a single qword);
  * memcpy-and-clear-source on the unordered_set buckets (40 bytes);
  * memcpy-and-clear-source on the map root pointer / size + the
    map's float load-factor sentinel (24 bytes).

Source-level shape is `code_(get<0>(t)), family_(get<1>(t)),
options_(std::move(get<2>(t)))`. The compiler is free to inline back
to the same SSE/qword copy pattern.

### Open question — `toDeviceOption(string)` body

Not yet decoded. The current cpp leaves it as a forward declaration
so the archive builds; it must be reconstructed before the
GenericDeviceType ctor can actually parse anything at runtime. Pulled
into a follow-up sub-phase.

### Build status

End of Phase 14b-ii-b2: clean build, 0 warnings. New files:

  * `include/zhinst/device_factories.hpp`
  * `include/zhinst/generic_device_type.hpp`
  * `src/device_factories.cpp`
  * `src/device_unknown.cpp`
  * `src/generic_device_type.cpp`

Modifications:

  * `include/zhinst/device_type.hpp` — added `DeviceTypeImpl(tuple)`
    ctor, `DeviceType(string, string)` ctor, and four new free
    function declarations (`splitDeviceOptions`, `toDeviceOption`,
    `toDeviceFamily`, `toDeviceOptions`).
  * `src/device_type.cpp` — implemented the tuple ctor, the two
    string-parsing DeviceType ctors, and three of the four new free
    functions. `toDeviceOption` is a forward declaration only (body
    deferred).
  * `include/zhinst/device_subclasses.hpp` — added `UnknownDevice`
    declaration.

================================================================================
Phase 14b-ii-b2-followup — toDeviceOption(string) body
================================================================================

`toDeviceOption(string const&)` @ 0x2d05b0 is a function-local-static
unordered_map of exactly 33 entries (confirmed: `mov edx, 0x21` at the
bucket-list allocation). Throws `zhinst::Exception` via
`boost::throw_exception` with a `boost::source_location` on miss.

The 33 entries are constructed lazily from anonymous-namespace
`const char*` constants `zhinst::(anonymous namespace)::DeviceOptionName::*`
at .data.rel.ro 0xb08ef8..0xb08ff8 (8 bytes apart confirms `const char*`
pointers, not std::string objects). Each pointer targets a literal in
.rodata starting at 0x90b60e:

  Bytes at .rodata 0x90b60e:
    "MFK\0MF\0MD\0FF\0PLL\0PID\0MOD\0RTK\0RT\0UHS\0AWG\0DIG\0"
    "10G\0QE\0F5M\0RUB\0BOX\0IA\0WEB\0CNT\0NOUI\0ME\0PC\0SKW\0"
    "16W\0QC2CH\0QC4CH\0QC6CH\0RTR\0PLUS\0LRT\0F200M\0"
    (additional pad before next pointer table at 0x90b663 = "QC")

Complete name → DeviceOption table (decoded from disasm lines 533249..533490):

  | C++ symbol             | string  | DeviceOption code |
  |------------------------|---------|-------------------|
  | DeviceOptionName::mf   | "MF"    | MF (0)            |
  | DeviceOptionName::mfHf2| "MFK"   | MF (0)            |
  | DeviceOptionName::md   | "MD"    | MD (1)            |
  | DeviceOptionName::ff   | "FF"    | FF (2)            |
  | DeviceOptionName::pll  | "PLL"   | PLL (3)           |
  | DeviceOptionName::pid  | "PID"   | PID (4)           |
  | DeviceOptionName::mod  | "MOD"   | MOD (5)           |
  | DeviceOptionName::rt   | "RT"    | RT (6)            |
  | DeviceOptionName::rtHf2| "RTK"   | RT (6)            |
  | DeviceOptionName::uhs  | "UHS"   | UHS (7)           |
  | DeviceOptionName::awg  | "AWG"   | AWG (8)           |
  | DeviceOptionName::dig  | "DIG"   | DIG (9)           |
  | DeviceOptionName::e10g | "10G"   | TenG (10)         |
  | DeviceOptionName::qe   | "QE"    | QE (11)           |
  | DeviceOptionName::f5m  | "F5M"   | F5M (12)          |
  | DeviceOptionName::rub  | "RUB"   | RUB (13)          |
  | DeviceOptionName::box  | "BOX"   | BOX (14)          |
  | DeviceOptionName::ia   | "IA"    | IA (15)           |
  | DeviceOptionName::web  | "WEB"   | WEB (16)          |
  | DeviceOptionName::cnt  | "CNT"   | CNT (17)          |
  | DeviceOptionName::noui | "NOUI"  | NOUI (18)         |
  | DeviceOptionName::me   | "ME"    | ME (19)           |
  | DeviceOptionName::pc   | "PC"    | PC (20)           |
  | DeviceOptionName::qc   | "QC"    | QA (21) — see asymmetry note |
  | DeviceOptionName::skw  | "SKW"   | SKW (22)          |
  | DeviceOptionName::w16  | "16W"   | Sixteen_W (23)    |
  | DeviceOptionName::qc2ch| "QC2CH" | QC2CH (24)        |
  | DeviceOptionName::qc4ch| "QC4CH" | QC4CH (25)        |
  | DeviceOptionName::qc6ch| "QC6CH" | QC6CH (26)        |
  | DeviceOptionName::rtr  | "RTR"   | RTR (27)          |
  | DeviceOptionName::plus | "PLUS"  | PLUS (28)         |
  | DeviceOptionName::lrt  | "LRT"   | LRT (29)          |
  | DeviceOptionName::f200m| "F200M" | F200M (30)        |

QA/QC asymmetry
---------------
The C++ symbol `DeviceOptionName::qc` holds the literal string `"QC"`
(confirmed at .rodata 0x90b663) and parses to DeviceOption code 21,
but `toString(DeviceOption, family)` for code 21 emits `"QA"`
(confirmed at disasm 0x2d0037: `mov WORD PTR [rdi+0x1], 0x4151` =
"QA\0"). The parser accepts "QC" while the formatter emits "QA" — a
real binary quirk. Enum is kept as `DeviceOption::QA` (matches the
formatter and existing per-family option tables) and the parser table
maps `"QC" -> QA`.

End of Phase 14b-ii-b2-followup: clean build, 0 warnings. New impl in
src/device_type.cpp (replaces the previous forward-declaration stub).
The undefined-symbol gap from end of 14b-ii-b2 is now closed; nm
confirms `_ZN6zhinst14toDeviceOptionERKNSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEEE`
is now `T` (defined) in `device_type.cpp.o`.

---

# Helpers and free functions

(Originally Phase 14b-iv notes.)

## Scope

| Symbol | Address | Notes |
|--------|---------|-------|
| `getOptionsAsString(DeviceType const&, std::string const&)` | 0x2d2dd0 | Wrapper around `toString(DeviceOptionSet, DeviceFamily, sep)` |
| `expand(DeviceFamily)` | 0x2dfbc0 | Decomposes bitmask into single-bit DeviceFamily values |
| `toStrings(std::vector<DeviceFamily> const&)` | 0x2df760 | Per-element `toString(DeviceFamily)` |
| `operator<<(ostream&, DeviceFamily)` | 0x2dfa00 | Inlined toString writer |
| `allDevices()` | 0x2d4c30 | Function-local static `flat_set<DeviceTypeCode>` (all 33) |
| `(anon) makeDevicesSet()` | 0x2d4cb0 | Initializer for `allDevices()::allDevicesSet` |
| `toDeviceOptions(vec<string>, DeviceFamily)` | 0x2d0fb0 | Already existed; only exception-filter correction applied |
| `makeDeviceFamilyFactory(DeviceFamily)` | 0x2e05d0 | Already existed (Phase 14b-i); confirmed jump-table dispatch |
| `detail::generateMfSfc(string, DeviceOptionSet)` | 0x2de910 | New file `mf_sfc.cpp`; returns `sfc::FeaturesCode` (see sfc namespace section) |

## `expand(DeviceFamily)` algorithm

```
out = empty vector (preallocated 0x2c bytes = 11 × int)
if family < 2: return out
for bit = 1; bit < family; bit <<= 1:
    if bit == 0x800: append 0x800; break    // saturation guard
    if (bit & family): append bit
return out
```

The `0x800` saturation guard exists because the highest defined family
(`VHF=0x400`) leaves `bit=0x800` reachable when `family > 0x800`. The
guard prevents runaway iteration on malformed inputs.

## `generateMfSfc` bit layout

| Bit | Source                      |
|-----|-----------------------------|
|  0  | `contains(MD)`              |
|  1  | `contains(PID)`             |
|  2  | `contains(MOD)`             |
|  5  | `contains(FF)`              |
|  6  | always-on for MFLI (code 10)|
|  7  | always-on for MFIA (code 11)|
| 10  | `contains(DIG)`             |
| 11  | `contains(F5M)`             |
| 14  | `contains(BOX)`             |
| 15  | `contains(IA)`              |
| 17  | `contains(NOUI)`            |

Throws `zhinst::Exception` for any other type code with message
"Requested to generate an SFC for an unsupported device type (<code>)."

source_location: file=`/builds/labone/labone/device/types/src/generate_mf_sfc.cpp`,
function=`sfc::FeaturesCode zhinst::detail::generateMfSfc(...)`,
line=104, column=47.

## `makeDeviceFamilyFactory` jump table

Indexed by raw DeviceFamily value (not bit position). Fully decoded
during Phase 14b-i:

| Family value | Vtable                       |
|--------------|------------------------------|
| 0            | `NoDeviceTypeFactory`        |
| 1   (HF2)    | `Hf2Factory`                 |
| 2   (UHF)    | `UhfFactory`                 |
| 4   (MF)     | `MfFactory`                  |
| 8   (HDAWG)  | `HdawgFactory`               |
| 0x10 (SHF)   | `ShfFactory`                 |
| 0x20 (PQSC)  | `PqscFactory`                |
| 0x40 (HWMOCK)| `HwmockFactory`              |
| 0x80 (SHFACC)| `ShfaccFactory`              |
| 0x100(GHF)   | `GhfFactory`                 |
| 0x200(QHUB)  | `QhubFactory`                |
| 0x400(VHF)   | `VhfFactory`                 |
| default      | `UnknownDeviceTypeFactory`   |

Each factory is 8 bytes (vptr only); subclasses are stateless.

## `toDeviceOptions` exception filter

The `cmp edx, 0x1` after the landing pad checks the catch-selector
value, which corresponds to `catch (zhinst::Exception const&)` in the
gcc_except_table. `toDeviceOption` throws `zhinst::Exception` (verified
at addresses 0x2d0b71/0x2d0c59) via `boost::throw_exception<Exception>`.
Earlier reconstruction had `catch (std::exception const&)` which is
broader than the binary; corrected to `catch (Exception const&)`.

## `allDevices()` / `makeDevicesSet()` static-init pattern

Function-local static of type `boost::container::flat_set<DeviceTypeCode>`
at `.bss 0xb851f0` with guard at `0xb85208`. The init helper bulk-inserts
33 entries (codes 0..32) via
`transform_iterator<int -> DeviceTypeCode, integer_iterator<int>>` and
the flat_set's range-insert overload, which uses pdqsort + adaptive_merge.

The lambda `makeDevicesSet()::$_0` is `[](int i) { return DeviceTypeCode(i); }`.
Reconstructed as a plain `for (int i = 0; i < 33; ++i) s.insert(...)`.

---

# `zhinst::sfc` namespace

(Originally Phase 14e notes.)

`zhinst::sfc` content was largely reconstructed in earlier phases as part
of the per-family device subclasses:

- All 6 `sfc::*Option` enum classes (Hf2Option, MfOption, UhfOption,
  HdawgOption, ShfOption, VhfOption) — `device_type.hpp:163-285`.
- `detail::OptionCodePair<T>` template — `device_type.hpp` (Phase 14b-iv).
- `detail::initializeSfcOptions<T,N>` template helper — `device_type.hpp`
  (Phase 14b-iv); 13 instantiations in the binary @0x2e0c90..0x2e4320,
  all driven from per-family `kXxxKnownOptions` arrays in the
  `device_*.cpp` files.

The only remaining work was the strong-typedef wrapper `sfc::FeaturesCode`.

## Symbol survey

```
13 × _ZN6zhinst6detail21initializeSfcOptionsINS_3sfc{Hf2,Mf,Uhf,Hdawg,Shf,Vhf}OptionEL{4,5,6,7,8,9,10,12}EEE...
```

The 6 distinct OptionEnum types match the 6 enum classes already in
`device_type.hpp`. No out-of-line definitions for any `sfc::*Option`
(correct — they're enum classes, purely declarative) and none for
`sfc::FeaturesCode` (it's a header-only POD wrapper).

## `sfc::FeaturesCode` — strong-typed uint64 wrapper

### Evidence chain

1. **Type name source**: the `BOOST_THROW_EXCEPTION` source_location
   literal embedded at binary offset 0x2deb37 records the function as
   `sfc::FeaturesCode zhinst::detail::generateMfSfc(...)`. Itanium ABI
   does **not** mangle free-function return types, so this string
   literal is the only direct evidence the type name exists.

2. **Layout / size** (≤ 8 bytes, trivially copyable): the disasm of
   generateMfSfc at 0x2deac1 ends with a direct `ret` after composing
   the bitfield in `rax` — no sret store, no destructor cleanup, no
   helper conversion call. By x86_64 SysV ABI this means the return
   value fits in a single integer register, which constrains the type
   to ≤ 8 bytes AND trivially copyable AND with no non-trivial
   destructor.

3. **Underlying type uint64**: every operation in the function composes
   bits via 64-bit `or rax, ...` instructions, with the highest used
   bit being position 17 (NOUI). The wrapper must hold at least 18
   bits and the calling convention forces ≤ 64. uint64 is the only fit.

### Reconstruction

```cpp
namespace sfc {
struct FeaturesCode {
    std::uint64_t value;
    constexpr FeaturesCode() noexcept : value(0) {}
    constexpr explicit FeaturesCode(std::uint64_t v) noexcept : value(v) {}
    constexpr explicit operator std::uint64_t() const noexcept { return value; }
    friend constexpr bool operator==(FeaturesCode a, FeaturesCode b) noexcept;
    friend constexpr bool operator!=(FeaturesCode a, FeaturesCode b) noexcept;
};
}  // namespace sfc
```

Wrapping (rather than `using FeaturesCode = uint64_t`) preserves type
identity: any binary call site that took `FeaturesCode` in an overloaded
context would have a distinct mangling from one that took raw uint64.
Future-proofs the reconstruction.

## Open items

- No known callers of `generateMfSfc` outside its own TU yet. Once the
  caller is found in a later phase, verify that it indeed expects the
  `FeaturesCode` type and not raw uint64; if it expects raw uint64,
  the wrapper claim from the source_location is misleading and the
  declaration should drop back to `uint64_t`.
- No analogous `generateXxxSfc` functions for HDAWG/SHF/UHF/HF2/VHF
  show up in nm. This is consistent with the source layout
  (`/builds/labone/labone/device/types/src/generate_mf_sfc.cpp` is
  MF-specific) but means the sfc namespace's wider use as a "generate
  SFC for any device family" abstraction isn't yet visible.

## `AwgDeviceProps` field-name verification — RESOLVED (Phase 21f)

The three inferred field names from Phase 14b-iii have been verified
by analyzing binary consumers:

| Old name | New name | Evidence |
|----------|----------|----------|
| `maxWaveformSamples` (+0x50) | `maxElfSize` | `compileSeqc` @0xf6a41 stores to JSON key `"maxelfsize"` |
| `maxWaveformBytes` (+0x58, uint64) | `addressImpl` (+0x58, uint32) + `sampleFormat` (+0x5c, uint32) | `AWGCompilerImpl` ctor @0x103b99 reads low32 → config.addressImpl; `writeWavesToElf*` @0x10e049 reads high32 → config.sampleFormat |
| `supportsExtraFeature` (+0x60) | `isHirzel` | `compileSeqc` @0xf67cd → config.isHirzel; true for HDAWG/SHFSG/SHFQC_SG/SHFLI/GHFLI/VHFLI |

The +0x58 split into two uint32_t explains the previously-puzzling
values like HDAWG's 0x180000000 (= addressImpl 0x80000000, sampleFormat 1)
and SHFQA's 0x200000000 (= addressImpl 0x00000000, sampleFormat 2).
