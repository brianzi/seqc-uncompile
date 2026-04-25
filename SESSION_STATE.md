# Session State — SeqC Compiler Reverse Engineering

## Goal

Reverse-engineer the `zhinst` namespace from `_seqc_compiler.so` (ELF 64-bit, x86-64, libc++ ABI) into reconstructed C++ source under `/home/brian/zhinst/seqc_compiler/reconstructed/`. Currently executing **Phase 22d** — implementing evaluate() method bodies from binary disassembly. 51/54 evaluate overrides completed. Next: **SeqCForLoop** @0x21b680 (9794B).

## Instructions

- **AGENTS.md process**: TODO.md top-down -> update OVERVIEW.md/notes -> propose changes at sub-phase wrap-up. TODO.md = actionable; notes/ = technical detail; OVERVIEW.md = high-level snapshot.
- **CRITICAL**: Don't ask for confirmation when executing a committed plan. Single-char "ok"/"k"/"continue"/"go on" = "proceed with next planned phase".
- **No half-implementations**: when unclear, comment out + add big TODO.
- **Per-file syntax check**: `/usr/bin/g++ -DBOOST_CONTAINER_DYN_LINK -DBOOST_CONTAINER_NO_LIB -DBOOST_FILESYSTEM_DYN_LINK -DBOOST_FILESYSTEM_NO_LIB -DBOOST_JSON_DYN_LINK -DBOOST_JSON_NO_LIB -Iinclude -std=gnu++17 -fsyntax-only src/FILE.cpp`
- **All headers flat in `reconstructed/include/zhinst/`**.
- **Dual build**: g++/libstdc++ (`build/`) primary; clang++/libc++ (`build-libcxx/`) ABI-correct validation. Both must be clean.
- **Real build at sub-phase wrap-up**: `cd reconstructed/build && cmake --build .` and same in `build-libcxx/`.
- **Cached disasm**: `/tmp/disasm_full.txt` (2.1M lines Intel syntax). Function-specific caches in `/tmp/*.disasm`.
- **CMakeLists.txt**: `file(GLOB CONFIGURE_DEPENDS src/*.cpp)` -- new files auto-picked up.
- **TODO items only in TODO.md** -- notes files are reference only, never carry actionable backlogs.
- **Access notes**: `type_` is protected on SeqCAstNode -- use `type()` accessor when accessing through child pointers (e.g., `cond()->type()`). `AsmList::Asm` node field is `node` not `node_`.

## Current Task: SeqCForLoop @0x21b680 (9794B)

### Remaining evaluate overrides (3 of 54)
1. **SeqCForLoop** @0x21b680 (9794B) -- **NEXT**
2. **SeqCSwitchCase** @0x217a80 (11506B)
3. **Complex batch**: SeqCCondExpr (11007B), SeqCFunction (5080B), SeqCFunctionCall (15220B), SeqCArray (ICF)

## Completed Evaluate Overrides (51/54)

### Just completed (this session)
- **SeqCRepeat** @0x221c10 (8567B): ~250 lines, repeat loop semantics:
  - Two children: count() (evaluated once) and body()
  - Cvar/Const path: toInt() count → error 0xb7 if negative
  - Cvar countInt < 2: single/zero-iteration inline path
  - Cvar countInt >= 2: channelGrouping limit check (error 0x7b), first body in "maybe_unroll" scope with atScopeBoundary + lineNr save/restore, then "unroll" loop for remaining iterations, node chaining
  - Var path: register-based counter loop — addi(0) to init counter from count reg, brz(endLabel), asmLoopNode, setState(Active), body eval, addi(-1) decrement, brgz(loopLabel), endLabel, loopArgNodeAppend + loopBodyNodeAppend
  - Name: "repeat(" + countResult->name_ + ")"
  - Key distinction from WhileLoop/DoWhile: counter register pattern (addi+brgz) instead of condition re-evaluation

### Previously completed control-flow evaluates
- **SeqCWhileLoop** @0x21e130 (7117B): Two-path (Cvar: compile-time unrolling with toInt(); Var: runtime loop with jumpIfZero+asmLoopNode+br). loopBodyRunsAtLeastOnce = jumpAsms.empty(). Two helpers: loopArgNodeAppend @0x21dcd0, loopBodyNodeAppend @0x21dfa0.
- **SeqCIfCondition** @0x214e30 (4360B): Two-path (Var: branch+jumpIfZero; Const/Cvar: compile-time toInt()). jumpIfZero helper @0x2149f0.
- **SeqCIfElse** @0x215330 (7214B): Three labels, Var/Cvar paths, atScopeBoundary toggle, dead-branch eval.
- **SeqCCaseEntry** @0x219c50 (2845B): inSwitch_ guard, dynamic_cast<SeqCValue*>, integrality warning.
- **SeqCReturnStatement** @0x21a1f0 (6800B): 5-way return type dispatch, br(state.strings.back()).

### Remaining evaluate overrides (3 of 54)
1. **SeqCForLoop** @0x21b680 (9794B)
2. **SeqCSwitchCase** @0x217a80 (11506B)
3. **Complex batch**: SeqCCondExpr (11007B), SeqCFunction (5080B), SeqCFunctionCall (15220B), SeqCArray (ICF)

## Key Technical Knowledge (Essential for implementation)

### EvalResults layout (0x80 bytes, offsets from object start)
- `values_` (+0x00): `std::vector<EvalResultValue>` (stride 0x38=56, magic multiply 0x6db6db6db6db6db7)
- `assemblers_` (+0x18): `std::vector<AsmList::Asm>` (stride 0xa8=168, magic multiply 0xcf3cf3cf3cf3cf3d)
- `hasError_` (+0x30): bool
- `node_` (+0x38): `std::shared_ptr<Node>`
- `waveformFront_` (+0x48): `std::shared_ptr<WaveformFront>`
- `name_` (+0x58): `std::string`
- `arrayBacking_` (+0x70): something

### VarType enum
- Unset=0, Void=1, Var=2, String=3, Const=4, Wave=5, Cvar=6

### Node layout (selected offsets)
- `asmId` (+0x14): int32
- `next` (+0xB8): `std::shared_ptr<Node>`
- `loop` (+0xE0): `std::shared_ptr<Node>`
- `loopBodyRunsAtLeastOnce` (+0x108): bool

### FrontendLoweringContext (0x50 bytes)
- `messages` (+0x00): `CompilerMessageCollection*`
- `asmCommands` (+0x08): `AsmCommands*`
- `wavetable` (+0x38): `WavetableFront*`  (setLineNr uses offset +0x38)
- `channelGrouping` (+0x48): int32 -- used as iteration limit for Cvar loop unrolling

### FrontendLoweringState (0x38 bytes)
- `strings` (vector of label strings, used by ReturnStatement)
- `inLoop_` (+0x30): bool
- `inSwitch_` (+0x31): bool

### Resources class (key methods)
- `createSubScope(string)` @0x1e36a0 -- returns shared_ptr<Resources>
- `setState(State)` @0x1e35f0 -- State::Active = 1
- `setAtScopeBoundary(bool)` -- sets flags_88_ byte at offset +0x88
- `newLabel(string)` @0x1ec6b0 -- static method, returns unique label string

### AsmCommands (key methods)
- `asmLabel(string)` @0x2774e0 -- returns AsmList::Asm
- `asmLoopNode()` @0x277ad0 -- returns AsmList::Asm with .node field
- `br(string, bool)` @0x271df0 -- unconditional branch
- `brnz(AsmRegister, string, bool)` @0x271f30 -- branch if not zero
- `brz(AsmRegister, string, bool)` -- branch if zero (used by jumpIfZero)
- `setWavetableFrontIndex(int)` -- sets line number on asmCommands (offset +0x50 on the pointed-to object at ctx+0x08)

### ErrorMessages
- `ErrorMessages::format<T>(ErrorMessageT, T)` @0x15c0d0 -- template format function
- `ErrorMessages::messages` @0xb84c38 -- static `std::map<int, std::string>`
- Error 0x27: null condition result (format with "while"/"do"/"for" etc.)
- Error 0x7b: too many iterations (Cvar unrolling limit exceeded, vs ctx.channelGrouping)
- Error 0x7e: bad condition type (from jumpIfZero, or inline in loop Var paths)

### Helper functions (anonymous namespace in seqc_ast_nodes_evaluate.cpp)
- `jumpIfZero(shared_ptr<EvalResults>, string, FrontendLoweringContext&)` @0x2149f0 -- returns vector<AsmList::Asm>
- `loopArgNodeAppend(shared_ptr<Node>, shared_ptr<Node>)` @0x21dcd0 -- walks loop->next chain, copies asmId
- `loopBodyNodeAppend(shared_ptr<Node>, shared_ptr<Node>)` @0x21dfa0 -- walks loop->next chain, no asmId copy
- `floatEqual(double, double)` @0x2ec050 -- forward-declared, defined in waveform_generator.cpp

### Common prologue pattern (all evaluate overrides)
```cpp
const int lineNr = type_;
ctx.messages->setLineNr(lineNr);
ctx.asmCommands->setWavetableFrontIndex(lineNr);
ctx.wavetable->setLineNr(lineNr);
auto result = std::make_shared<EvalResults>();
```

### Common Cvar path pattern
1. Initialize accumulatedNode = null, iterCount = 0
2. Loop: evaluate body in "unroll" sub-scope
3. Node chaining: walk bodyResult->node_->next to leaf, attach new bodyEval->node_
4. Track accumulatedNode (first bodyResult's node_ that's non-null)
5. Merge assemblers, check hasError
6. Re-evaluate condition (WhileLoop/DoWhile) or decrement counter (Repeat?)
7. Iteration limit check vs ctx.channelGrouping (error 0x7b)
8. Post-loop: copy accumulatedNode or bodyResult->node_ to result->node_

### Common Var path pattern
1. Create labels, emit asmLabel
2. jumpIfZero or brnz/br for condition
3. asmLoopNode() with loopBodyRunsAtLeastOnce flag
4. Evaluate body in sub-scope, merge assemblers
5. Unconditional br back to loop top
6. asmLabel(endLabel)
7. loopArgNodeAppend + loopBodyNodeAppend

## Relevant Files

### Process files
- `/home/brian/zhinst/seqc_compiler/TODO.md` -- Phase 22d section at ~line 2223
- `/home/brian/zhinst/seqc_compiler/OVERVIEW.md` -- Current status at line 187
- `/home/brian/zhinst/seqc_compiler/AGENTS.md` -- Process rules

### Source file being modified
- `reconstructed/src/seqc_ast_nodes_evaluate.cpp` -- **7539 lines total**
  - SeqCWhileLoop at ~line 6626
  - SeqCDoWhile at ~line 6883
  - **SeqCRepeat stub at line 7201** (to be replaced)
  - SeqCForLoop stub further down (~line 7212)
  - loopArgNodeAppend/loopBodyNodeAppend helpers at ~line 2508 area
  - jumpIfZero helper at ~line 2434 area
  - floatEqual forward-declaration at line 44

### Key headers
- `reconstructed/include/zhinst/seqc_ast_node.hpp` -- SeqCRepeat at line 359 (count+body children)
- `reconstructed/include/zhinst/frontend_lowering.hpp` -- FrontendLoweringContext, FrontendLoweringState
- `reconstructed/include/zhinst/node.hpp` -- Node layout
- `reconstructed/include/zhinst/resources.hpp` -- Resources, newLabel, createSubScope, setState, setAtScopeBoundary
- `reconstructed/include/zhinst/eval_results.hpp` -- EvalResults layout (0x80 bytes)
- `reconstructed/include/zhinst/error_messages.hpp` -- ErrorMessages::format<>(), messages map
- `reconstructed/include/zhinst/asm_commands.hpp` -- AsmCommands methods
- `reconstructed/include/zhinst/value.hpp` -- Value::toInt(), Value::toDouble()

### Disasm caches
- `/tmp/SeqCRepeat.disasm` -- **1896 lines**, function at 0x221c10 (NOT YET ANALYZED)
- `/tmp/SeqCDoWhile.disasm` -- 1530 lines (reference)
- `/tmp/SeqCWhileLoop.disasm` -- 2000 lines (reference)
- `/tmp/disasm_full.txt` -- 2.1M lines Intel syntax (master cache)

### Build directories
- `reconstructed/build/` -- g++/libstdc++ primary build
- `reconstructed/build-libcxx/` -- clang++/libc++ ABI-correct validation (use timeout 300000ms)

### Notes
- `reconstructed/notes/frontend_lowering.md` -- Evaluate override status table
- `reconstructed/notes/struct_layouts.md` -- Raw byte-offset tables for all structs

## Workflow for SeqCForLoop

1. **Extract disasm** for SeqCForLoop @0x21b680 (9794B) → `/tmp/SeqCForLoop.disasm`
2. **Analyze** structure: SeqCForLoop has 4 children (init, cond, incr, body)
3. **Implement** replacing stub in seqc_ast_nodes_evaluate.cpp
4. **Syntax check** + **Full build** (both build/ and build-libcxx/)
5. **Sub-phase wrap-up**: Update TODO.md (51→52/54), OVERVIEW.md, notes/frontend_lowering.md
