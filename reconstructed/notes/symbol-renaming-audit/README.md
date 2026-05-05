# Symbol Renaming Audit — Assignment Index

This folder collects per-batch reports for the symbol renaming audit
described in `@RULES-symbol-renaming.md` (repo root) and summarized in
`AGENTS.md` under "Symbol renaming audits".

**Hard rule for the duration of the audit:** no edits to any file
outside this folder.

## How to read this index

- One row per batch assignment.
- "Files" lists the header(s) and source(s) the batch covers.
- "Report" links the batch's report file inside this folder.
- "Status" values:
  - `pending` — not yet dispatched
  - `in-progress` — subagent running or partial report on disk
  - `partial` — subagent stopped early; a follow-up batch is needed
  - `complete` — full coverage of the assignment landed
  - `skipped` — assignment dropped (with reason in the report file)

A batch's report file may exist with `partial` status; in that case
add a follow-up row below it (e.g. `waveform.md` followed by
`waveform_part2.md`) rather than overwriting.

## Out-of-scope by default

These files reconstruct standard library, build infrastructure, or
trivial glue and are deprioritized / skipped unless evidence of
naming problems surfaces:

- `calver.hpp` / `calver.cpp` — calendar version helpers
- `format_time.hpp` / `format_time.cpp` — time formatting
- `log_macros.hpp`, `logging.hpp`/`.cpp`, `log_exception.cpp` — logging glue
- `tracing.hpp` / `tracing.cpp` — tracing macros
- `exception.hpp` / `exception.cpp` — exception base classes
- `serial_predicates.hpp` / `serial_predicates.cpp` — small predicates
- `zi_folder.hpp` / `zi_folder.cpp` — folder path helpers
- `platform.hpp` / `platform.cpp` — platform abstraction shims
- `stubs.cpp` — link stubs
- `pybind_seqc.cpp` — Python bindings boilerplate
- `seqc_parser_fwd.hpp (and asm_parser_fwd.hpp)` — generated parser forward decls
- `csv_parser.cpp` — small text parser
- `compile_seqc.cpp` — top-level entry glue (will be revisited if needed)

These can be added back as batches if a different batch's findings
suggest naming problems leak in from them.

## Assignments

| # | Assignment | Files | Report | Status |
|---|---|---|---|---|
| 01 | types | include/zhinst/core/types.hpp | `01_types.md` | complete (6 yes, ~12 not-misnomer) |
| 02 | memory_allocator | include/zhinst/codegen/memory_allocator.hpp; src/codegen/memory_allocator.cpp | `02_memory_allocator.md` | complete (4 yes; bytes-vs-samples cross→14/36/46; alloc/template params) |
| 03 | waveform_generator | include/zhinst/waveform/waveform_generator.hpp; src/waveform/waveform_generator.cpp | `03_waveform_generator.md` | complete (2 yes/H, 3 yes/M; `createdNames_` is allow-list bypass; `readInt::minVal`→`argIndex` header/body mismatch; `interpolateLinear::xPoints/yPoints` are ramp values) |
| 04a | seqc_ast_node base | include/zhinst/ast/seqc_ast_node.hpp; src/ast/seqc_ast_node.cpp; src/ast/seqc_ast.cpp | `04a_seqc_ast_node.md` | complete (12 yes/H — `SeqCAstNode::type` param/accessor stores `lineNr_`; cascades to 53 derived ctors; `first_`/`second_` cluster; cross→42) |
| 04b | ast_evaluate_helpers | src/ast/seqc_ast_nodes_evaluate.cpp lines 1–2610 | `04b_ast_evaluate_helpers.md` | complete (3 yes/M — `scaleFactor` dead/inverted; `kRangeLo`/`kRangeHi` hide MAX values) |
| 04c | ast_evaluate_arith | src/ast/seqc_ast_nodes_evaluate.cpp lines 2611–4690 | `04c_ast_evaluate_arith.md` | complete (1 yes/M `SeqCAssign::aux`; 2 yes/L; arithmetic evaluators mostly clean) |
| 04d | ast_evaluate_logical | src/ast/seqc_ast_nodes_evaluate.cpp lines 4691–5800 | `04d_ast_evaluate_logical.md` | complete (3 yes/M — `lhsHas1`/`rhsHas1` mean "exactly one"; `asmCtx` collides; `asm1..asm6` numeric suffixes) |
| 04e | ast_evaluate_control | src/ast/seqc_ast_nodes_evaluate.cpp lines 5801–10137 | `04e_ast_evaluate_control.md` | complete (3 yes — `childHadError`, `lastEval`, `hasErrorOrNull`; cross→34) |
| 05a | custom_functions header+main | include/zhinst/runtime/custom_functions.hpp; src/runtime/custom_functions.cpp | `05a_custom_functions.md` | complete (8 yes/H — `mergeWaveforms::paramN`, `field_80/88/A8/B0`; 7 yes/M; cross→05b/05c, 23) |
| 05b | custom_functions_play | src/runtime/custom_functions_play.cpp | `05b_custom_functions_play.md` | complete (5 yes/M — `play::playLength` is DigTrigger, `playIndexed::waveIndex` is length, `writeLS64bit::reg1/reg2` are address codes) |
| 05c | custom_functions_io | src/runtime/custom_functions_io.cpp | `05c_custom_functions_io.md` | complete part1 (lines 1-2649, 46/56 methods; 1 yes/H, 11 yes/M) |
| 05c2 | custom_functions_io part2 | src/runtime/custom_functions_io.cpp lines 2650-3433 | `05c_custom_functions_io_part2.md` | complete (2 yes/H cross→31 on `numDIOBits`/`numOutputPorts` semantic mismatches; ~14 yes/M; setPRNGSeed likely logic bug) |
| 05d | custom_functions_playback | src/runtime/custom_functions_playback.cpp | `05d_custom_functions_playback.md` | complete (3 yes/M — `playWaveZSync::shift` is data pattern, `playDIOWave::dryRun` is `isHold`, `maxSampleLen` dead) |
| 06 | asm_register | include/zhinst/asm/asm_register.hpp | `06_asm_register.md` | complete (4 yes/M, 11 not-misnomer; tautological `Reg(0)` factory) |
| 07 | compiler | include/zhinst/codegen/compiler.hpp; src/codegen/compiler.cpp | `07_compiler.md` | complete (1 yes/H `channelMode_`→`usedFourChannelMode_`; 1 yes/M `channelGrouping` propagation; cross→19, 23) |
| 08 | error_messages | include/zhinst/core/error_messages.hpp; src/core/error_messages.cpp | `08_error_messages.md` | complete (0 yes; ~250 enumerators tier-2-anchored; comment defect at hpp:45 noted) |
| 09 | prefetch | include/zhinst/codegen/prefetch.hpp; src/codegen/prefetch.cpp; src/codegen/prefetch_helpers.cpp; src/codegen/prefetch_emit.cpp; src/codegen/prefetch_print.cpp; src/codegen/prefetch_prepare.cpp; src/codegen/prefetch_placesingle.cpp; src/codegen/prefetch_splitplay.cpp | `09_prefetch.md` | complete (part 1 — class members + cross-batch arbitrations) |
| 09b | prefetch part 2 | (same as 09) | `09_prefetch_part2.md` | complete (method/free-fn params + useDA local sweep; minIndexedSize confirmed §3-excluded) |
| 10 | asm_commands | include/zhinst/asm/asm_commands.hpp; src/asm/asm_commands.cpp | `10_asm_commands.md` | complete (4 yes/H, ~10 yes/M, 3 cross→38; play_config arbitrations resolved here; `flag`→`isWaveformCmd` cluster confirmed) |
| 11 | value | include/zhinst/ast/value.hpp; src/ast/value.cpp | `11_value.md` | complete (1 yes/H, 3 yes/M, 9 not-misnomer; cross→12 on `pad_04_`) |
| 12 | eval_result_value | include/zhinst/ast/eval_result_value.hpp; src/ast/eval_result_value.cpp | `12_eval_result_value.md` | complete (0 yes; 3 not-misnomer) |
| 13 | awg_assembler_impl | include/zhinst/codegen/awg_assembler_impl.hpp; src/codegen/awg_assembler_impl.cpp; src/codegen/awg_assembler_impl_pipeline.cpp; src/codegen/awg_assembler_opcodes.cpp | `13_awg_assembler_impl.md` | complete (1 yes/H `printOpcode::format`→`startIndex`; 3 yes/M; pervasive header/impl param-name divergence) |
| 14 | waveform | include/zhinst/waveform/waveform.hpp; src/waveform/waveform.cpp; src/waveform/util_wave.cpp | `14_waveform.md` | complete (3 yes/H — `WaveformFile::data` is SHA-1, `secondaryName`→`functionArgs`, `playWord`→`playConfig`; 2 yes/M; cross→38, 31, 02) |
| 15 | cached_parser | include/zhinst/io/cached_parser.hpp; src/io/cached_parser.cpp | `15_cached_parser.md` | complete (5 yes/M — `cacheFile::markers`/`markerBitsVec` swap; `valid_` is pin-bit; `fileSize_` is byte budget; coordinated-rename) |
| 16 | waveform_ir | include/zhinst/waveform/waveform_ir.hpp; src/waveform/waveform_ir.cpp | `16_waveform_ir.md` | complete (1 yes/H — `getSampleCount()` returns bytes; 4 not-misnomer; cross→31) |
| 17 | waveform_front | include/zhinst/waveform/waveform_front.hpp; src/waveform/waveform_front.cpp | `17_waveform_front.md` | complete (1 yes/M — `WaveformFront::values`→`genArgs_`; positives on `dirty_`, `hasDuplicate_`) |
| 18 | seqc_parser_context | include/zhinst/ast/seqc_parser_context.hpp; src/ast/seqc_parser_context.cpp | `18_seqc_parser_context.md` | complete (0 yes; 2 unsure on `blockComment_`/`lineComment_` boolean-prefix consistency vs batch 50) |
| 19a | resources main | include/zhinst/runtime/resources.hpp; src/runtime/resources.cpp | `19a_resources.md` | complete (2 yes/H — `Resources::parent_` is grandparent, `VarSubType_Bool` legacy alias; 7 yes/M; cross→07, 19b) |
| 19b | resources supplementary | src/runtime/resources_function.cpp; src/runtime/resources_static_global.cpp; src/runtime/static_resources.cpp; src/runtime/global_resources.cpp | `19b_resources_supplementary.md` | complete (1 yes/L `init::n`; pass-through batch) |
| 20 | node | include/zhinst/ast/node.hpp; src/ast/node.cpp | `20_node.md` | complete (1 yes/M `Node::swap::devIdx`→`ancestorAsmId`; 1 yes/L; 17 JSON-anchored fields not-misnomer) |
| 21 | elf_reader | include/zhinst/io/elf_reader.hpp; src/io/elf_reader.cpp | `21_elf_reader.md` | complete (3 yes; `Line::addr` 8-byte read suspect; `ddSectionIndex_` always-zero) |
| 22 | device_factories | include/zhinst/device/device_factories.hpp; src/device/device_factories.cpp | `22_device_factories.md` | complete (0 yes; dead `DeviceOpts` namespace dup of anon-ns `k*` set; cross→29 on `opts`/`options`) |
| 23 | awg_compiler_config | include/zhinst/codegen/awg_compiler_config.hpp; src/codegen/awg_compiler_config.cpp | `23_awg_compiler_config.md` | complete (4 yes; alias-method cluster `appendMode/splitIndex/syncVersion` cross→09/36; `channelGrouping` 3rd leg) |
| 24 | asm_expression | include/zhinst/asm/asm_expression.hpp; src/asm/asm_expression.cpp | `24_asm_expression.md` | complete (1 yes/H, 2-3 yes/M; alias-method cluster `lineNumber()`/`labelType()` etc.) |
| 25 | asm_optimize | include/zhinst/asm/asm_optimize.hpp; src/asm/asm_optimize.cpp | `25_asm_optimize.md` | complete (1 yes/H reinforces 26 `isWaveformCmd` semantic inversion; 4 yes/M; cross→26, 10, 49, 44, 06) |
| 26 | assembler | include/zhinst/asm/assembler.hpp; src/asm/assembler.cpp | `26_assembler.md` | complete (6 yes; `isWaveformCmd` semantic-inversion confirmed owner side; `AssemblerInstr`/namespace recompose cross→33) |
| 27 | node_map_data | include/zhinst/runtime/node_map_data.hpp; src/runtime/node_map_data.cpp; src/runtime/get_node_map.cpp | `27_node_map_data.md` | complete (5 yes; `NodeMapItem::hasFast` int/AccessMode conflation; data-table file mostly literals) |
| 28 | awg_compiler | include/zhinst/codegen/awg_compiler.hpp; src/codegen/awg_compiler.cpp | `28_awg_compiler.md` | complete (4 yes/H — `string_200_/230_/248_` are filename/source/asm; `compileString::opcodeCount` counts words not opcodes) |
| 29 | device_type | include/zhinst/device/device_type.hpp; src/device/device_type.cpp | `29_device_type.md` | complete (4 yes/H — `clone()`→`doClone()`, `TenG`/`Sixteen_W` placeholders, 39 `sfc::*Option::Bit0xNNNN` placeholders; coordinated-rename) |
| 30 | awg_device_props | include/zhinst/device/awg_device_props.hpp; src/device/awg_device_props.cpp | `30_awg_device_props.md` | complete (0 yes; producer-side vindication of batch-23 cluster; 1 unsure on `fpgaRevisionPattern`) |
| 31 | device_constants | include/zhinst/device/device_constants.hpp; src/device/device_constants.cpp | `31_device_constants.md` | complete (4 yes; `numOutputPorts`→`execTableIndexBits`; `waveformGranularity/PageSize` swap; cross→36) |
| 32 | frontend_lowering | include/zhinst/ast/frontend_lowering.hpp; src/ast/frontend_lowering.cpp | `32_frontend_lowering.md` | complete (1 yes/H `pad10_` is in-function flag; 2 yes/M `channelGrouping` (3rd leg cluster), `strings` is return-label stack) |
| 33 | awg_assembler | include/zhinst/codegen/awg_assembler.hpp; src/codegen/awg_assembler.cpp | `33_awg_assembler.md` | complete (1 yes/H — `AssemblerInstr` is `Assembler`; reinforces batch-26 type-decomposition bug) |
| 34 | eval_results | include/zhinst/ast/eval_results.hpp; src/ast/eval_results.cpp | `34_eval_results.md` | complete (1 yes/M `hasError_` is return-encountered flag; cross-validated with batch 12) |
| 35 | rawwave | include/zhinst/waveform/rawwave.hpp; src/waveform/rawwave.cpp | `35_rawwave.md` | complete (0 yes, 5 not-misnomer/H; sample units verified) |
| 36 | cache | include/zhinst/runtime/cache.hpp; src/runtime/cache.cpp | `36_cache.md` | complete (5 yes/M, 3 not-misnomer, 2 cross-batch→09) |
| 37 | signal | include/zhinst/waveform/signal.hpp; src/waveform/signal.cpp | `37_signal.md` | complete (0 yes; 3 unsure on numSamples/newLength sample-vs-frame units; 7 not-misnomer w/ tier-2 JSON keys) |
| 38 | play_config | include/zhinst/waveform/play_config.hpp; src/waveform/play_config.cpp | `38_play_config.md` | complete (1 yes/L, 5 not-misnomer, 3 cross-batch→49) |
| 39 | math_compiler | include/zhinst/codegen/math_compiler.hpp; src/codegen/math_compiler.cpp | `39_math_compiler.md` | complete (1 yes; `functionExists::strict` polarity-inverted bool) |
| 40 | generic_device_type | include/zhinst/device/generic_device_type.hpp; src/device/generic_device_type.cpp | `40_generic_device_type.md` | complete (0 yes; trivial pImpl wrapper) |
| 41 | device_subclasses | include/zhinst/device/device_subclasses.hpp; src/device/device_hdawg.cpp; src/device/device_shf.cpp; src/device/device_uhf.cpp; src/device/device_vhf.cpp; src/device/device_ghf.cpp; src/device/device_mf.cpp; src/device/device_hf2.cpp; src/device/device_pqsc.cpp; src/device/device_qhub.cpp; src/device/device_shfacc.cpp; src/device/device_hwmock.cpp; src/device/device_unknown.cpp | `41_device_subclasses.md` | complete (33 `clone()`→`doClone()` overrides coordinated with batch 29; 4 inline-bit helper naming nits) |
| 42 | expression | include/zhinst/ast/expression.hpp; src/ast/expression.cpp | `42_expression.md` | complete (3 yes/H — `valueType` is `EDirection`, `createFunction::nameExpr`/`params` swapped roles; 2 yes/M) |
| 43 | wavetable_helpers | include/zhinst/waveform/wavetable_helpers.hpp | `43_wavetable_helpers.md` | complete (1 yes/M — `getUniqueName::index`→`lineNr`; cross→46) |
| 44 | asm_list | include/zhinst/asm/asm_list.hpp; src/asm/asm_list.cpp | `44_asm_list.md` | complete (1 yes/H — `Asm::isWaveformCmd` semantic-inversion confirmed from owner side; cross→26, 25, 10, 49, 24) |
| 45 | wavetable_front | include/zhinst/waveform/wavetable_front.hpp; src/waveform/wavetable_front.cpp; src/waveform/wavetable_manager_front.cpp | `45_wavetable_front.md` | complete (3 yes/M — `address2_` likely misnomer; `lineNr_`/`waveformCounter_` cross-batch-arbitration vs JSON keys `numDefs`/`numDefs2`) |
| 46 | wavetable_ir | include/zhinst/waveform/wavetable_ir.hpp; src/waveform/wavetable_ir.cpp; src/waveform/wavetable_manager_ir.cpp | `46_wavetable_ir.md` | complete (4 yes/H — `WaveOrder::ByName`/`ByIndex` swap, `totalSamples`/`memorySizeInSamples` byte-vs-sample lies; 5 yes/M; cross→16, 31, 45) |
| 47 | elf_writer | include/zhinst/io/elf_writer.hpp; src/io/elf_writer.cpp; src/waveform/write_waves_to_elf.cpp | `47_elf_writer.md` | complete (1 yes/H — `addWaveform::useAbsolute` polarity inverted; should be `useMapped`) |
| 48 | address_impl | include/zhinst/asm/address_impl.hpp | `48_address_impl.md` | complete (0 yes; structural note re. type holding non-address values) |
| 49 | asm_commands_impl | include/zhinst/asm/asm_commands_impl.hpp; src/asm/asm_commands_impl.cpp; src/asm/asm_commands_impl_hirzel.cpp; src/asm/asm_commands_impl_cervino.cpp | `49_asm_commands_impl.md` | complete (5 yes/M, 1 yes/H — `flag`→`isWaveformCmd` pattern; play_config arbitrations re-routed to batch 10) |
| 50 | asm_parser_context | include/zhinst/asm/asm_parser_context.hpp; src/asm/asm_parser_context.cpp | `50_asm_parser_context.md` | complete (2 yes/M — `addCommand::cmd`/`args` swap; cross→24) |
| 51 | callbacks | include/zhinst/core/callbacks.hpp; src/core/callbacks.cpp | `51_callbacks.md` | complete (0 yes; only in-scope symbol is `setProgress::progress`; routine fine) |
| 52 | compiler_message | include/zhinst/core/compiler_message.hpp; src/core/compiler_message.cpp | `52_compiler_message.md` | complete (1 yes/H — `showLine` polarity inverted) |
| 53 | wave_index_tracker | include/zhinst/waveform/wave_index_tracker.hpp; src/waveform/wave_index_tracker.cpp | `53_wave_index_tracker.md` | complete (0 yes; 4 unsure stylistic on `maxIndex` underscore consistency) |
| 54 | mf_sfc | src/device/mf_sfc.cpp | `54_mf_sfc.md` | complete (1 yes/M — `bitIf::b`→`set`/`present`; tied to batch 29 sfc placeholders) |
