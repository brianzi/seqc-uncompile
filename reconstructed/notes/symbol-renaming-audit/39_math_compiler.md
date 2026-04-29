# Batch 39 — math_compiler

## 1. Files considered

- `reconstructed/include/zhinst/math_compiler.hpp`
- `reconstructed/src/math_compiler.cpp`

Cross-references read for context:
- `reconstructed/src/custom_functions.cpp:280–319` (sole call site of
  `MathCompiler::functionExists` and `MathCompiler::call`)

Binary verification: `nm --demangle _seqc_compiler.so | grep MathCompiler`
(see Locations consulted in §3 blocks).

## 2. Overview

| Symbol | Misnomer? | Conf | Justification (≤ 5 words) | Proposal(s) | Status |
|---|---|---|---|---|---|
| `MathCompiler` (type) | no | high | name in binary symbol table | keep current | not-misnomer |
| `MathCompiler::singleArgFns_` | no | medium | matches stored callable shape | keep current | — |
| `MathCompiler::multiArgFns_` | no | medium | matches stored callable shape | keep current | — |
| `MathCompiler::abs` … `MathCompiler::tanh` (single-arg methods) | no | high | names in binary symbol table | keep current | not-misnomer |
| `MathCompiler::avg` … `MathCompiler::sum` (multi-arg methods) | no | high | names in binary symbol table | keep current | not-misnomer |
| `MathCompiler::abs::x` (and all single-arg method params named `x`) | no | medium | math arg, conventional | keep current | — |
| `MathCompiler::avg::v` (and all `vector<double> const&` params named `v`) | no | low | generic but adequate | keep current, `args` | — |
| `MathCompiler::functionExists` | no | high | name in binary symbol table | keep current | not-misnomer |
| `MathCompiler::functionExists::name` | no | medium | matches usage as map key | keep current | — |
| `MathCompiler::functionExists::argCount` | no | medium | passed in as arg count | keep current | — |
| `MathCompiler::functionExists::strict` | yes | medium | inverted semantics | `nameOnly`, `ignoreArgCount`, keep current | in-scope (parameter name; nm preserves only the bool type) |
| `MathCompiler::functionExists::found` (local) | unsure | low | overloaded meaning | `argCountMatches`, keep current | — |
| `MathCompiler::call::name` | no | high | matches usage as map key | keep current | — |
| `MathCompiler::call::args` | no | high | matches usage | keep current | — |
| `MathCompilerException` (type) | no | high | name in binary symbol table | keep current | not-misnomer |
| `MathCompilerException::msg_` | no | medium | stores exception message | keep current | — |
| `MathCompilerException::MathCompilerException::msg` | no | medium | stored as message | keep current | — |
| `MathCompiler::MathCompiler::bind1` (local lambda) | no | low | binds 1-arg member fn | keep current | — |
| `MathCompiler::MathCompiler::bindN` (local lambda) | no | low | binds N-arg member fn | keep current | — |

## 3. Detailed findings

### MathCompiler::functionExists::strict  [yes / medium / verify-not-original]

Evidence:
- src/math_compiler.cpp:137 declaration:
  `bool MathCompiler::functionExists(std::string const& name,
   size_t argCount, bool strict) const`
- src/math_compiler.cpp:138–157, dasm comment + reconstructed body:
  ```
  //   bl = strict (input)
  //   it = singleArgFns_.find(name)
  //   if (it != end)  al = (argCount == 1)
  //   else            it2 = multiArgFns_.find(name)
  //                   if (it2 != end)  al = (argCount != 0)
  //                   else             return false
  //   return bl | al
  ```
- src/custom_functions.cpp:301 only call site:
  `if (mathCompiler_.functionExists(name, argCount, false)) {`

Interpretation:
- The reconstructed return is `strict | found`, where `found` already
  encodes "name present **and** arg count is plausible". Therefore:
  - `strict == false` → result = `(name present AND argcount matches)`
  - `strict == true`  → result = `(name present)` regardless of argcount
- In ordinary English, "strict" suggests *more* checking; here, setting
  it to `true` performs *less* checking (suppresses the arg-count gate).
  The semantics are inverted relative to the name.
- Only one call site exists in the reconstructed tree, and it passes
  `false`. The `true` path is unreachable in observed code, so the
  intended natural-language label cannot be confirmed from usage.
- `nm --demangle` does not encode parameter names, so the binary symbol
  table is silent on this. The original may have used a name with the
  opposite polarity (e.g. `nameOnly`/`anyArgCount`) that the
  reconstruction inverted, or the name truly was `strict` in upstream
  (in which case it was always misleading). The judgement below cites
  the inversion that exists today regardless of provenance.

Judgement:
- yes: the name reads as "be more strict" but the implementation makes
  the check **less** strict when set to true, which is the canonical
  inverted-boolean misnomer pattern.

Proposals:
- `nameOnly`           (medium) — true ⇒ check only that the name
  exists in either map.
- `ignoreArgCount`     (medium) — explicit about what the flag turns off.
- `anyArgCount`        (low)
- keep current         (low) — only if synthesis decides the original
  binary documentation actually used "strict" with this inverted
  meaning.

Cross-reference:
- Status `verify-not-original` because the original parameter name is
  not recoverable from `nm`. Synthesis should check any docs/headers
  that may have been preserved, and choose between the proposals above.

Locations consulted:
- declared: include/zhinst/math_compiler.hpp:90
- defined:  src/math_compiler.cpp:137–158
- used:     src/custom_functions.cpp:301
- binary:   `nm --demangle _seqc_compiler.so` shows
  `MathCompiler::functionExists(...) const` at 0x1c3e50
  (method name authoritative; param name not encoded).

### MathCompiler::functionExists::found  [unsure / low / —]

Evidence:
- src/math_compiler.cpp:146–157:
  ```
  bool found = false;
  auto it = singleArgFns_.find(name);
  if (it != singleArgFns_.end()) {
      found = (argCount == 1);
  } else {
      auto it2 = multiArgFns_.find(name);
      if (it2 == multiArgFns_.end()) {
          return false;
      }
      found = (argCount != 0);
  }
  return strict | found;
  ```

Interpretation:
- `found` is set only after the name is already known to exist in one
  map; it is then overwritten with a *boolean about argument count*
  (`argCount == 1` or `argCount != 0`). It does not record whether the
  function was found, but whether the call signature is compatible.
- The "name not found" branch returns directly without touching `found`.
- The current name is mildly misleading because by the time `found` is
  written, the name has already been found.

Judgement:
- unsure: it is a local in a small function and the misalignment is
  modest; reasonable readers may accept the loose use of "found" for
  "this is a usable hit". Worth flagging but not strongly.

Proposals:
- `argCountMatches` (low)
- `argCountOk`      (low)
- `usable`          (low)
- keep current      (low)

Locations consulted:
- defined:  src/math_compiler.cpp:146–157

### MathCompiler::singleArgFns_, MathCompiler::multiArgFns_  [no / medium / —]

Evidence:
- include/zhinst/math_compiler.hpp:93–94:
  ```
  std::map<std::string, std::function<double(double)>>
      singleArgFns_;  // +0x00
  std::map<std::string,
      std::function<double(std::vector<double> const&)>>
      multiArgFns_;   // +0x18
  ```
- src/math_compiler.cpp:41–69 (ctor): keys for `singleArgFns_` are 23
  one-argument math functions (`abs`, `cos`, …); keys for
  `multiArgFns_` are 5 multi-arg functions (`avg`, `max`, `min`, `pow`,
  `sum`).
- src/math_compiler.cpp:147,151,168,176 (lookup sites): both maps are
  searched by name and the resulting `function<…>` is invoked with
  either a single double or a `vector<double> const&`.

Interpretation:
- Field types and contents map exactly to the names: a map of
  single-argument callables and a map of variadic-list callables.
  Trailing underscore matches the project's member-naming convention.

Judgement:
- no: the names accurately describe stored callables.

Proposals:
- keep current (high)

Locations consulted:
- declared: include/zhinst/math_compiler.hpp:93–94
- used:     src/math_compiler.cpp:41–69, 147, 151, 168, 176;
            ctor/dtor offsets noted in header comment.

## 4. Symbols inspected and judged routinely fine

- `MathCompiler` (type), all 23 single-arg method names, all 5 multi-arg
  method names, `MathCompiler::call`, `MathCompiler::functionExists`,
  `MathCompilerException`, `MathCompilerException::what`, both
  ctors/dtors — all appear verbatim in `nm --demangle` output (binary
  symbol table is authoritative; see Tier-1 evidence in §4d/§3 of
  RULES). Out of scope for rename per RULES §3.
- `MathCompiler::abs::x` and the parameter `x` of every other
  single-arg math wrapper — single-letter argument name is the
  established mathematical convention for the input of `f(x)`.
- `MathCompiler::avg::v` (and the `v` parameter of `max`, `min`, `pow`,
  `sum`) — generic but conventional for a `vector<double> const&` in a
  small wrapper; usage is read-only sequence access. `args` would be a
  reasonable alternative; not flagged.
- `MathCompiler::pow::v` — same as above; size-2 enforcement is in the
  body, not implied by parameter name.
- `MathCompiler::call::name`, `MathCompiler::call::args`,
  `MathCompiler::functionExists::name`,
  `MathCompiler::functionExists::argCount` — each is used exactly
  consistently with its name (map key / size accessor).
- `MathCompilerException::MathCompilerException::msg`,
  `MathCompilerException::msg_`,
  `MathCompilerException::what` (return value path) — `msg` is stored
  in `msg_` and returned via `c_str()`; trailing-underscore convention
  matches other classes in the codebase.
- `MathCompiler::MathCompiler::bind1`, `MathCompiler::MathCompiler::bindN`
  — local lambdas in the ctor; names describe arity of the bound
  pointer-to-member; only used in this scope.
- `MathCompilerException::what::(none)` — no parameters.
- `MathCompiler::avg::s`, `MathCompiler::sum::s` (locals) —
  conventional single-letter accumulator name in two-line reduce loops.
- `MathCompiler::call::it`, `MathCompiler::call::it2`,
  `MathCompiler::functionExists::it`,
  `MathCompiler::functionExists::it2` — conventional iterator names;
  `it`/`it2` distinguishes the two consecutive map lookups.

## 5. Coverage

- **Fully covered:** all symbols in
  `include/zhinst/math_compiler.hpp` and `src/math_compiler.cpp`,
  including type names, all method names, all method parameters, all
  data members, and all named locals. `nm --demangle` was consulted
  for every type, method, and free-function name in the file; no
  static data members exist for either class (verified via
  `nm | grep MathCompiler` — no `d`/`D`/`B`/`b`/`r` symbols other
  than std-library `__function::__func` instantiations).
- **Deferred:** none.
- **Not covered (out of scope per RULES §2/§3):**
  - All method names and the two type names (`MathCompiler`,
    `MathCompilerException`) — present in the binary symbol table,
    excluded per §3.
  - Template parameters on the bind lambdas (none declared explicitly).
  - `std::__1::*` symbols generated by `std::function`/`std::bind`
    instantiations — third-party / generated code per §2.
- **Status:** complete.
