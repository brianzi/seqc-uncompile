# Batch 08 — error_messages

## 1. Files considered

- `reconstructed/include/zhinst/core/error_messages.hpp`
- `reconstructed/src/core/error_messages.cpp`

Binary symbol table consulted:
`nm --demangle /home/brian/zhinst/seqc_compiler/_seqc_compiler.so`.

Reference report cross-checked: `52_compiler_message.md` (adjacent
diagnostic plumbing). Note: error-message *string keys* in this batch
are integer enum values, not string identifiers, so the tier-2
"identifier == rodata key" check does not apply to enumerators here.
What *can* be checked against rodata is the format-string text bound
to each enumerator (done below).

Authoritatively present in the symbol table (excluded from rename per §3):

- Type names: `zhinst::ErrorMessages` (instantiated as
  `ErrorMessages::messages` and via `format<>` template names),
  `zhinst::ErrorMessageT` (appears in every `format<…>(ErrorMessageT, …)`
  mangling), `zhinst::ResourcesException`.
- Method names: `ErrorMessages::operator[](ErrorMessageT) const`
  (verified at 0x108380 — see binary annotation in source),
  `ErrorMessages::format<…>` (~54 instantiations, all in symbol table),
  `ResourcesException::ResourcesException(string const&)` (0x1e3a20),
  `ResourcesException::~ResourcesException()` (0x1e3a80, 0x1f12f0),
  `ResourcesException::what() const` (0x1f1340 — see source).
- Free function names: `getApiErrorMessage(ZIResult_enum)` (0x2e4820),
  `getApiErrorBase(ZIResult_enum)` (0x2e4980),
  `make_error_code(ZIResult_enum)` (0x2e4550),
  `isApiError(boost::system::error_code const&)` (0x2e4490),
  `isApiError(RemoteErrorCode const&)` (0x2e44f0).
- **Static data members (tier-1 per §3):**
  `zhinst::ErrorMessages::messages` (0xb84c38) — excluded.
- **Namespace data symbols** present in `nm` (excluded for the *name*
  itself, even though semantics may still merit observation):
  `zhinst::errMsg` (0x95de60),
  `zhinst::constAwgIntegrationTrigger`,
  `zhinst::zsyncDataPqscRegister`,
  `zhinst::zsyncDataPqscDecoder`
  (each appears multiple times — per-TU `static` duplicates,
  un-suffixed, demangled identifier matches source verbatim).
- `zhinst::(anonymous namespace)::unknownError` — anonymous-namespace
  constant; its identifier appears in the demangled symbol table.
  Name is faithful to the binary.

In-scope per §2: enum members of `ErrorMessageT`, parameter names of
`operator[]`, `get`, `format` overloads, and `getApiErrorMessage`,
the local `m` in the static initializer, and the synthetic
helper-struct name `ErrorMessagesInitializer`.

## 2. Overview

| Symbol | Misnomer? | Conf | Justification (≤ 5 words) | Proposal(s) | Status |
|---|---|---|---|---|---|
| `ErrorMessageT` (enum members 0–254, 16384+, 32768+, 36864+) | no | medium | format strings match names | keep current | not-misnomer |
| `ErrorMessageT::UnknownError47` | unsure | medium | placeholder for missing key | keep current, drop entry | in-scope (UnknownError47 not in nm or strings) |
| `ErrorMessageT::InvalidRegister` (alias) | no | medium | recon-only alias, verified | keep current | — |
| `ErrorMessageT::ValueOverflow` (alias) | no | medium | recon-only alias, verified | keep current | — |
| `ErrorMessageT::UnsupportedOp` (alias) | no | medium | recon-only alias, verified | keep current | — |
| `ErrorMessages::operator[]::id` | no | high | enum-typed lookup key | keep current | — |
| `ErrorMessages::get::id` | no | medium | int-typed lookup key | keep current | — |
| `ErrorMessages::format(ErrorMessageT,...)::id` | no | high | enum-typed format key | keep current | — |
| `ErrorMessages::format(ErrorMessageT,...)::args` | no | high | variadic format args | keep current | — |
| `ErrorMessages::format(BF&,...)::fmt` | no | high | the boost::format object | keep current | — |
| `ErrorMessages::format(BF&,T,Args...)::arg` | no | medium | one feed arg per call | keep current | — |
| `ErrorMessages::format(BF&,T,Args...)::args` | no | medium | remaining variadic args | keep current | — |
| `getApiErrorMessage::ziResultCode` | no | high | type literally `ZIResult_enum` | keep current | — |
| `ResourcesException::ResourcesException::msg` | no | medium | stored as message text | keep current | — |
| (anon)`unknownError` | no | high | name appears verbatim in `nm` | keep current | not-misnomer |
| `ErrorMessagesInitializer` (synth struct) | no | low | recon-only init helper | keep current | — |
| `ErrorMessagesInitializer::ErrorMessagesInitializer::m` (local) | no | medium | alias for `messages` | keep current | — |

(Field `ErrorMessages::messages` and globals `errMsg`,
`zsyncDataPqscDecoder`, `zsyncDataPqscRegister`,
`constAwgIntegrationTrigger`: out of scope per §3 — see §5.)

## 3. Detailed findings

### `ErrorMessageT` enumerators (whole enum)  [no / medium / not-misnomer]

Evidence:
- For every enumerator declared in error_messages.hpp:55-418, the
  paired comment / format string in error_messages.cpp:135-448 is
  consistent with the name. Spot examples:
  - `CmdWithoutRegister = 0` ↔ `m[0] = "%1% command without valid register"`
  - `ValueOutOfRange = 5` ↔ `m[5] = "value %1% is out of range for %2% bits"`
  - `DivisionByZero = 41` ↔ `m[41] = "division by zero"` (note: header
    comment swap fixed in §3 below — semantic still correct).
  - `InvalidDeviceNr = 42` ↔ `m[40] = "invalid device nr %1% …"` —
    **mismatch**: see "header table swap" finding in §4 routine notes
    (this is a code-comment numbering issue, not a name issue).
  - `WaveformNotExist = 227` ↔ `m[227] = "waveform '%1%' does not exist"`
  - `ApiSuccess = 16384` ↔ `m[16384] = "Success (no error)"`
  - `FwBadAddress = 36866` ↔ `m[36866] = "Requested block address does not belong to any known aperture"`
- Use sites in src/asm/asm_commands.cpp, src/runtime/custom_functions_play.cpp,
  src/waveform/waveform_generator.cpp, etc. consistently call
  `ErrorMessages::format(EnumName, …)` with arguments matching the
  format string's `%1%/%2%` positional specifiers — e.g.
  src/asm/asm_commands.cpp:91 `format(InvalidRegister, "prf")` against the
  `"%1% command without valid register"` template.

Interpretation:
- Enumerator names describe the semantic content of the bound format
  string in nearly every case. The enum is essentially a string-key
  catalog whose keys (the names) are descriptive of the catalog's
  contents (the format strings). Per §4d tier 2, the format string
  text is `.rodata` from the binary — strong positive evidence the
  name is faithful.
- A handful of names are abbreviated (`Exactly2Args`, `JoinMin2`,
  `FuncExactly2Args`, `FuncArgs2or3`) but the abbreviations are
  unambiguous against their format strings.

Judgement:
- Not misnomers, as a class. Individual enumerators may merit later
  attention if a stricter convention is wanted, but no member sends
  a misleading semantic signal.

Proposals:
- keep current (high) for the enum overall.

Locations consulted:
- declared: include/zhinst/core/error_messages.hpp:55-418
- defined messages: src/core/error_messages.cpp:135-448
- representative use sites: src/asm/asm_commands.cpp:91-657,
  src/runtime/custom_functions_play.cpp:151-2306,
  src/waveform/waveform_generator.cpp:328-831, src/codegen/prefetch.cpp:490-2321,
  src/runtime/csv_parser.cpp:161-1028.

### `ErrorMessageT::UnknownError47`  [unsure / medium / verify-not-original]

Evidence:
- include/zhinst/core/error_messages.hpp:112
  `UnknownError47 = 47, // used at custom_functions_io.cpp — message string unknown`
- src/core/error_messages.cpp:182-183 has no `m[47] = …` entry — line 182 is
  `m[46] = …` and the next assignment is `m[48] = …`. Comment at line
  133 confirms "Keys 0-255 with gaps at 47 and 53".
- The header itself documents at line 45 "Gaps: 46, 52 are unassigned"
  — but the *assigned* enumerators show `ExecTableInvalidConst = 46`
  and `DeprecatedConst = 52`, with the actual gap at **47** and
  **53**. So the enum has an enumerator at the position the comment
  calls a gap.

Interpretation:
- The name `UnknownError47` is a self-declared placeholder meaning
  "the binary references key 47 from `custom_functions_io.cpp` but we
  have not recovered the format string". It is *honest* about being a
  placeholder, but it is also the only enumerator in the file whose
  name encodes the integer key, and the symbol table contains no
  evidence either way (enumerators are never in the symbol table).
- The header's textual comment about which keys are gaps is internally
  inconsistent with the enum and with the .cpp gap pattern; that is
  noted in incidental findings turf, not a naming issue per se.

Judgement:
- The name is not misleading — it explicitly flags itself as
  uncertain. Whether the symbol should exist at all (or be removed
  pending message recovery) is a separate question that this audit
  cannot resolve from headers alone.

Proposals:
- keep current (medium) — the `47` suffix is a useful breadcrumb.
- drop the enumerator and let callers use `ErrorMessageT(47)` (low)
  — only after confirming no source file references the name.

Locations consulted:
- declared: include/zhinst/core/error_messages.hpp:112
- defined messages: src/core/error_messages.cpp:182-183
- header gap comment: include/zhinst/core/error_messages.hpp:45,118

### `ErrorMessageT::InvalidRegister` / `ValueOverflow` / `UnsupportedOp` (aliases)  [no / medium / —]

Evidence:
- include/zhinst/core/error_messages.hpp:416-418 declares three duplicate
  enumerators with the same numeric values as
  `CmdWithoutRegister=0`, `ValueOutOfRange=5`, `InvalidOpcode=11`.
- The header comment at lines 405-415 explains these are recon-side
  aliases used by `asm_commands.cpp` (which throws with `esi=0/5/0xb`).
- src/asm/asm_commands.cpp uses `ErrorMessageT::InvalidRegister` 25+ times,
  always with the format string for `CmdWithoutRegister` ("%1% command
  without valid register"). The argument passed (e.g. `"prf"`,
  `"wvf"`, `"brz"`) is the assembler mnemonic that *does* fit the
  `"<mnemonic> command without valid register"` reading, so the alias
  *is* descriptive in context: the assembler-side mnemonic-throw is a
  "register is missing/invalid" condition.

Interpretation:
- These three names are recon-only — the binary has only the canonical
  enumerator values (enumerator names are not in the symbol table at
  all). The aliases are kept for source-level compatibility per the
  comment. They describe a *use case* (assembler register error) that
  is consistent with the underlying message.

Judgement:
- Not misleading: the aliased name fits the call sites that use it.

Proposals:
- keep current (medium).

Locations consulted:
- declared: include/zhinst/core/error_messages.hpp:405-418
- used: src/asm/asm_commands.cpp:91-657 (all `InvalidRegister`/`ValueOverflow`).

### `ErrorMessages::format(BF&, T, Args...)::arg` and `args`  [no / medium / —]

Evidence:
- include/zhinst/core/error_messages.hpp:455-459
  `template <typename T, typename... Args>
   static std::string format(boost::format& fmt, T arg, Args... args) {
       fmt % arg;
       return format(fmt, args...);
   }`
- The comment at lines 453-454 explicitly describes the split: "Binary
  splits the first arg out of the pack (affects mangling): `format<T,
  Args...>(BF&, T, Args...)` not `format<Args...>(BF&, Args...)`".
  This split is verified by the `nm` output: instantiations like
  `format<int, std::string>(boost::basic_format&, int, std::string)`
  are present, indicating one head-arg followed by the variadic tail.

Interpretation:
- `arg` (singular) is the head element being fed into the format
  object via `operator%`. `args` (variadic pack) is the remainder of
  the parameter list. Both names exactly describe what the recursion
  does. The split is required to match the binary mangling, so
  renaming would not change ABI but would change the meaning of the
  identifiers relative to the documented split.

Judgement:
- Not misnomers.

Proposals:
- keep current (high) for both.

Locations consulted:
- declared: include/zhinst/core/error_messages.hpp:455-471
- mangling evidence: `nm --demangle … | grep ErrorMessages::format`.

### `getApiErrorMessage::ziResultCode`  [no / high / —]

Evidence:
- include/zhinst/core/error_messages.hpp:495 `std::string const& getApiErrorMessage(int ziResultCode);`
- src/core/error_messages.cpp:104-115 looks the value up in
  `ErrorMessages::messages` and returns `unknownError` on miss.
- Symbol table: `zhinst::getApiErrorMessage(ZIResult_enum)` — the
  parameter type in the binary is `ZIResult_enum`, not bare `int`.
  The recon weakens the type to `int`, but the *name* `ziResultCode`
  preserves the binary semantics. The keys looked up (16384+, 32768+,
  36864+) are exactly the `ZIResult_enum` ranges.

Interpretation:
- The parameter name precisely matches the binary parameter type
  semantics. The name is more accurate than the recon's `int` type
  declaration. Per §2a, the type narrowing is a side observation
  (would be `ZIResult_enum`), but the name itself is solid.

Judgement:
- Not a misnomer.

Proposals:
- keep current (high).

Locations consulted:
- declared: include/zhinst/core/error_messages.hpp:495
- defined:  src/core/error_messages.cpp:104-115
- type evidence: `nm --demangle … | grep getApiErrorMessage`.

### (anon)`unknownError`  [no / high / not-misnomer]

Evidence:
- src/core/error_messages.cpp:102 `static const std::string unknownError = "unknownError";`
- `nm --demangle` line:
  `0000000000962ba8 r zhinst::(anonymous namespace)::unknownError`
- The `.rodata` value of the symbol is the string literal
  `"unknownError"` (header comment at line 100 records the binary
  address `0x962ba8`).

Interpretation:
- The recon identifier matches the demangled symbol name verbatim,
  and the value of the constant is the same string. Per §4d tier 1
  this is authoritative positive evidence.

Judgement:
- Not a misnomer.

Proposals:
- keep current (high).

Locations consulted:
- declared: src/core/error_messages.cpp:102
- symbol-table entry: `nm --demangle _seqc_compiler.so`.

## 4. Symbols inspected and judged routinely fine

- `ErrorMessages::operator[]::id` — `ErrorMessageT` lookup key passed
  to `messages.at(static_cast<int>(id))`. Name matches role.
- `ErrorMessages::get::id` — same role on the static int-typed
  overload at hpp:438; name matches.
- `ErrorMessages::format(ErrorMessageT, Args...)::id` — selector for
  the format string; name fits.
- `ErrorMessages::format(ErrorMessageT, Args...)::args` — variadic
  pack forwarded into the inner helper; name fits.
- `ErrorMessages::format(BF&)::fmt` — base case with no args, returns
  `fmt.str()`; idiomatic.
- `ResourcesException::ResourcesException::msg` — copied into `msg_`,
  returned by `what()`. Idiomatic shorthand.
- `ErrorMessagesInitializer` (anonymous-namespace struct in cpp:128)
  and its instance `errorMessagesInit_` — recon-side scaffolding to
  emulate `__cxx_global_var_init`. Names are descriptive.
- `ErrorMessagesInitializer::ErrorMessagesInitializer::m` (local at
  cpp:130) — short alias for `ErrorMessages::messages` to keep the
  300+ assignments readable. Internal consistency: every usage is
  `m[N] = "…"`. Acceptable as a one-letter alias scoped to a single
  function body.
- Header-file comment tags `BSS at 0x…` etc. — not symbols.

Incidental observation worth flagging at synthesis (not a naming
issue, so no block):
- include/zhinst/core/error_messages.hpp:45 says "Gaps: 46, 52 are
  unassigned" but the actual gaps in the .cpp are at keys **47** and
  **53** (and `ExecTableInvalidConst=46`, `DeprecatedConst=52` are
  populated). The header comment block at lines 117-118 is correct;
  line 45 contradicts it. This is a *comment* defect, not a
  symbol-name defect, and per §11 cannot be fixed during the audit.

## 5. Coverage

- **Fully covered:** every parameter, enumerator, and local in
  `error_messages.hpp` / `error_messages.cpp` that is in scope
  per §2/§3.
- **Deferred:** none.
- **Not covered (out of scope per RULES §2/§3):**
  - Type names `ErrorMessages`, `ErrorMessageT`, `ResourcesException`
    — present in the symbol table (excluded by §3).
  - Method names `ErrorMessages::operator[]`, `ErrorMessages::get`,
    `ErrorMessages::format` (~54 instantiations),
    `ResourcesException::ResourcesException`, `~ResourcesException`,
    `what` — all present in `nm --demangle` (excluded).
  - Free function `getApiErrorMessage` — present in `nm` (excluded).
  - **Static data member** `ErrorMessages::messages` — tier-1
    excluded per §3 ("`nm` shows the static member as a data
    symbol").
  - Namespace globals `errMsg`, `zsyncDataPqscDecoder`,
    `zsyncDataPqscRegister`, `constAwgIntegrationTrigger` — all
    appear with these exact demangled names in `nm` (multiple
    per-TU duplicates due to `static`-namespace mangling), so the
    *names* are excluded from rename. (Their declared type
    `extern const std::string` vs the binary's `static`
    per-TU duplication is an ABI/storage observation, not a
    naming issue.)
  - `make_error_code`, `getApiErrorBase`, `isApiError` — referenced
    in `nm` and not declared in this batch's files.
- Symbols not located: none. The full content of both files was
  enumerated.
