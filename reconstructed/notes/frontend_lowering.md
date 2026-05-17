# Frontend Lowering {#notes_frontend_lowering}

**Frontend lowering** is the stage that turns the parsed SeqC
abstract syntax tree (AST) into the back-end *node graph* the
optimisation passes, prefetcher, and assembler emitter all
consume.  It is the single largest pass in the compiler and the
*only* polymorphic AST walk — every later pass operates on the
back-end `Node` graph, not on the SeqC AST.

It is step 6 of `Compiler::compile()` (see \ref notes_pipeline)
and is driven by the free function `FrontEndLoweringFacade::lower`
in `reconstructed/include/zhinst/codegen/compiler.hpp`.

[TOC]

## The facade entry point

```cpp
namespace FrontEndLoweringFacade {
    struct LowerResult {
        std::shared_ptr<Node>        astResult;   // lowered back-end root
        std::shared_ptr<EvalResults> evalResult;  // top-level evaluation result
    };

    LowerResult lower(std::shared_ptr<Resources>     resources,
                      SeqCAstNode&                   ast,
                      CompilerMessageCollection&     messages,
                      std::shared_ptr<AsmCommands>   asmCommands,
                      std::shared_ptr<CustomFunctions> customFunctions,
                      std::shared_ptr<WaveformGenerator> waveformGen,
                      std::shared_ptr<WavetableFront>    wavetable,
                      int                            loopUnrollLimit);
}
```

The caller (`Compiler::compile`) supplies a resource environment
and the parsed AST root, and receives back a lowered `Node`
graph plus the top-level `EvalResults`.  The lowered root is
stored into `Compiler::ast_` and threaded into every subsequent
pass.

Internally, `lower()` does three things:

1. Builds a stack-local `FrontendLoweringContext` from the
   resource arguments (the read-only environment).
2. Creates an empty `FrontendLoweringState` (the mutable
   accumulator).
3. Dispatches the AST root's `evaluate(resources, context, state)`
   virtual.  The polymorphic walk does *all* the real work;
   the facade just returns `{state.result, top-level evalResult}`.

## The context: `FrontendLoweringContext`

Header: `reconstructed/include/zhinst/ast/frontend_lowering.hpp`.

`FrontendLoweringContext` is the **read-only environment** every
`evaluate()` override sees.  It bundles the compiler subsystems
an evaluation may need to consult:

| Member            | Role                                                                  |
|-------------------|-----------------------------------------------------------------------|
| `messages`        | Diagnostics sink for warnings / errors (non-owning).                  |
| `asmCommands`     | Assembler-instruction registry; targets of opcode placeholders.       |
| `customFunctions` | Registry of SeqC built-ins (`playWave`, `wait`, math, …).             |
| `waveformGen`     | Factory for waveform-generator built-ins (`zeros`, `sin`, …).         |
| `wavetable`       | Front-end wavetable recording every waveform reference.               |
| `loopUnrollLimit` | Iteration cap when constant-folding `repeat (N)` / `for (...)` loops. |

The context lives on `lower()`'s stack frame and is shared by
reference across the entire AST traversal.  No `evaluate()`
override mutates it — see \ref notes_custom_functions for the
built-ins resolved through `customFunctions`.

## The state: `FrontendLoweringState`

`FrontendLoweringState` is the **mutable accumulator** threaded
through the same traversal:

| Member             | Role                                                                                 |
|--------------------|--------------------------------------------------------------------------------------|
| `result`           | Lowered back-end `Node` root; grows as statements are processed.                     |
| `inFunctionDef_`   | Nesting counter; zero outside any function body.                                     |
| `labelStack`       | Active break / continue label scope used by `break` / `continue`.                    |
| `inLoop_`          | Non-zero while inside a loop body; consulted by `break` / `continue` validation.     |
| `inSwitch_`        | Non-zero while inside a `switch` body; consulted by `SeqCCaseEntry`.                 |

After dispatch returns, `state.result` is the lowered root that
goes into `LowerResult::astResult` and ultimately into
`Compiler::ast_`.

## The evaluation result: `EvalResults`

Header: `reconstructed/include/zhinst/ast/eval_results.hpp`.

`EvalResults` is the **universal return type** of the lowering
dispatch.  Every override of `SeqCAstNode::evaluate()` and every
entry in the `CustomFunctions` built-in registry returns a
`std::shared_ptr<EvalResults>`.

A single instance carries:

- `values_` — a vector of typed `EvalResultValue`s.  Plural to
  support tuple-producing built-ins.
- `assemblers_` — assembler instructions emitted as a side
  effect of the evaluation.
- `node_` — an optional lowered `Node` subtree (the structural
  contribution this evaluation makes to `state.result`).
- `waveformFront_` — an optional waveform descriptor when the
  evaluation produced one.
- `name_` — an optional name binding.
- `arrayBacking_` — link to a backing array when the value
  participates in a larger array expression.
- `returnEncountered_` — set by `SeqCReturnStatement` so
  enclosing scopes can short-circuit further statements.

## `Value` and `EvalResultValue`

`Value` (in `reconstructed/include/zhinst/ast/value.hpp`) is the
tagged-union scalar type that flows through the evaluator.
`EvalResultValue` wraps a `Value` with type/subtype tags and is
the element type of `EvalResults::values_`.

`Immediate` (sibling in the same header) is the analogous
tagged-union used as an assembly-instruction *operand* — an
unsigned address, a signed 32-bit integer, or a string label —
and is the bridge between evaluated values and what the
assembler emission needs.

## The dispatch model

`SeqCAstNode::evaluate(resources, context, state)` is virtual
and overridden once per AST node kind.  Every node:

1. Recursively `evaluate()`s its children, threading the same
   `context` and `state` through.
2. Combines the children's `EvalResults`.
3. Optionally appends to `state.result` (statements) and / or
   to its own returned `EvalResults::node_` (expressions).
4. Optionally emits assembler placeholders into
   `context.asmCommands` and / or into its returned
   `EvalResults::assemblers_`.
5. Returns its own `std::shared_ptr<EvalResults>`.

This is the *only* polymorphic walk over the SeqC AST in the
compiler.  Every later pass operates on the back-end `Node`
graph and the `AsmList` produced here.

A few node kinds are worth calling out because they manage
state:

- **Loops** (`SeqCForLoop`, `SeqCWhileLoop`, repeat-style
  built-ins) set `state.inLoop_` around the body and push /
  pop `state.labelStack`.  Their bodies may be unrolled when
  the trip count is constant and within `loopUnrollLimit`.
- **`SeqCSwitchCase` / `SeqCCaseEntry`** set `state.inSwitch_`
  around the body so `case` / `default` entries can validate
  they appear in a legal context.
- **`SeqCReturnStatement`** sets `returnEncountered_` on its
  result so enclosing scopes can stop iterating siblings.
- **`SeqCBreakStatement` / `SeqCContinueStatement`** consult
  `state.inLoop_` / `state.inSwitch_` and emit jumps to the
  innermost label on `state.labelStack`.

## Built-in function dispatch

When an `evaluate()` of a function-call AST node fires, the
node consults `context.customFunctions` to resolve the call:

- Each SeqC built-in is registered in the `CustomFunctions`
  table by name and bound to a method on the corresponding
  C++ implementation class.
- The dispatched method receives the call's argument
  `EvalResults`, applies its own evaluation (which may emit
  assembler placeholders, register a waveform with
  `context.wavetable`, or call `context.waveformGen` to
  materialise samples), and returns its own
  `std::shared_ptr<EvalResults>`.

See \ref notes_custom_functions for the catalogue of registered
built-ins.

## Outputs handed to the back end

After `lower()` returns, `Compiler::compile` has:

- A **lowered `Node` graph** rooted at `Compiler::ast_` — the
  intermediate representation every subsequent optimisation,
  the prefetcher, and the placeholder fill-in operate on (see
  \ref notes_node_tree_structure).
- An **`AsmList`** of assembler placeholders accumulated in
  `Compiler::asmList_` (via `context.asmCommands`) — the
  prefetcher rewrites these in place during
  `fillInPlaceholders` (see \ref notes_prefetch_scheduling).
- A **wavetable** in `Compiler::wavetable_` with every
  waveform reference recorded for later resolution into
  `WavetableIR`.
- The top-level **`EvalResults`** in `LowerResult::evalResult`,
  consulted briefly by the caller for top-level diagnostics.

## See also

- \ref notes_pipeline — frontend lowering is step 6.
- \ref notes_node_tree_structure — the back-end `Node` graph
  the lowered AST is written into.
- \ref notes_custom_functions — built-ins resolved through
  `CustomFunctions` during evaluation.
- \ref notes_seqc_parser_grammar — produces the SeqC AST
  that lowering consumes.
- \ref notes_waveform_generator_funcmap — the DSL whose
  built-ins `WaveformGenerator` materialises during
  evaluation.
- \ref notes_prefetch_scheduling — consumes the `AsmList`
  and `Node` graph this pass emits.
