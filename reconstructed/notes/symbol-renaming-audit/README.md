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
- `yy_fwd.hpp` — generated parser forward decls
- `csv_parser.cpp` — small text parser
- `compile_seqc.cpp` — top-level entry glue (will be revisited if needed)

These can be added back as batches if a different batch's findings
suggest naming problems leak in from them.

## Assignments

| # | Assignment | Files | Report | Status |
|---|---|---|---|---|
| 01 | types | include/zhinst/types.hpp | `01_types.md` | complete (6 yes, ~12 not-misnomer) |
| 02 | memory_allocator | include/zhinst/memory_allocator.hpp; src/memory_allocator.cpp | `02_memory_allocator.md` | in-progress |
| 03 | waveform_generator | include/zhinst/waveform_generator.hpp; src/waveform_generator.cpp | `03_waveform_generator.md` | complete (2 yes/H, 3 yes/M; `createdNames_` is allow-list bypass; `readInt::minVal`→`argIndex` header/body mismatch; `interpolateLinear::xPoints/yPoints` are ramp values) |
| 04 | seqc_ast_node | include/zhinst/seqc_ast_node.hpp; src/seqc_ast_node.cpp; src/seqc_ast.cpp; src/seqc_ast_nodes_evaluate.cpp | `04_seqc_ast_node.md` | pending |
| 05 | custom_functions | include/zhinst/custom_functions.hpp; src/custom_functions.cpp; src/custom_functions_play.cpp; src/custom_functions_io.cpp; src/custom_functions_playback.cpp | `05_custom_functions.md` | pending |
| 06 | asm_register | include/zhinst/asm_register.hpp | `06_asm_register.md` | complete (4 yes/M, 11 not-misnomer; tautological `Reg(0)` factory) |
| 07 | compiler | include/zhinst/compiler.hpp; src/compiler.cpp | `07_compiler.md` | complete (1 yes/H `channelMode_`→`usedFourChannelMode_`; 1 yes/M `channelGrouping` propagation; cross→19, 23) |
| 08 | error_messages | include/zhinst/error_messages.hpp; src/error_messages.cpp | `08_error_messages.md` | complete (0 yes; ~250 enumerators tier-2-anchored; comment defect at hpp:45 noted) |
| 09 | prefetch | include/zhinst/prefetch.hpp; src/prefetch.cpp; src/prefetch_helpers.cpp; src/prefetch_emit.cpp; src/prefetch_print.cpp; src/prefetch_prepare.cpp; src/prefetch_placesingle.cpp; src/prefetch_splitplay.cpp | `09_prefetch.md` | complete (part 1 — class members + cross-batch arbitrations) |
| 09b | prefetch part 2 | (same as 09) | `09_prefetch_part2.md` | complete (method/free-fn params + useDA local sweep; minIndexedSize confirmed §3-excluded) |
| 10 | asm_commands | include/zhinst/asm_commands.hpp; src/asm_commands.cpp | `10_asm_commands.md` | complete (4 yes/H, ~10 yes/M, 3 cross→38; play_config arbitrations resolved here; `flag`→`isWaveformCmd` cluster confirmed) |
| 11 | value | include/zhinst/value.hpp; src/value.cpp | `11_value.md` | complete (1 yes/H, 3 yes/M, 9 not-misnomer; cross→12 on `pad_04_`) |
| 12 | eval_result_value | include/zhinst/eval_result_value.hpp; src/eval_result_value.cpp | `12_eval_result_value.md` | complete (0 yes; 3 not-misnomer) |
| 13 | awg_assembler_impl | include/zhinst/awg_assembler_impl.hpp; src/awg_assembler_impl.cpp; src/awg_assembler_impl_pipeline.cpp; src/awg_assembler_opcodes.cpp | `13_awg_assembler_impl.md` | pending |
| 14 | waveform | include/zhinst/waveform.hpp; src/waveform.cpp; src/util_wave.cpp | `14_waveform.md` | complete (3 yes/H — `WaveformFile::data` is SHA-1, `secondaryName`→`functionArgs`, `playWord`→`playConfig`; 2 yes/M; cross→38, 31, 02) |
| 15 | cached_parser | include/zhinst/cached_parser.hpp; src/cached_parser.cpp | `15_cached_parser.md` | complete (5 yes/M — `cacheFile::markers`/`markerBitsVec` swap; `valid_` is pin-bit; `fileSize_` is byte budget; coordinated-rename) |
| 16 | waveform_ir | include/zhinst/waveform_ir.hpp; src/waveform_ir.cpp | `16_waveform_ir.md` | complete (1 yes/H — `getSampleCount()` returns bytes; 4 not-misnomer; cross→31) |
| 17 | waveform_front | include/zhinst/waveform_front.hpp; src/waveform_front.cpp | `17_waveform_front.md` | complete (1 yes/M — `WaveformFront::values`→`genArgs_`; positives on `dirty_`, `hasDuplicate_`) |
| 18 | seqc_parser_context | include/zhinst/seqc_parser_context.hpp; src/seqc_parser_context.cpp | `18_seqc_parser_context.md` | pending |
| 19 | resources | include/zhinst/resources.hpp; src/resources.cpp; src/resources_function.cpp; src/resources_static_global.cpp; src/static_resources.cpp; src/global_resources.cpp | `19_resources.md` | pending |
| 20 | node | include/zhinst/node.hpp; src/node.cpp | `20_node.md` | complete (1 yes/M `Node::swap::devIdx`→`ancestorAsmId`; 1 yes/L; 17 JSON-anchored fields not-misnomer) |
| 21 | elf_reader | include/zhinst/elf_reader.hpp; src/elf_reader.cpp | `21_elf_reader.md` | in-progress |
| 22 | device_factories | include/zhinst/device_factories.hpp; src/device_factories.cpp | `22_device_factories.md` | pending |
| 23 | awg_compiler_config | include/zhinst/awg_compiler_config.hpp; src/awg_compiler_config.cpp | `23_awg_compiler_config.md` | in-progress |
| 24 | asm_expression | include/zhinst/asm_expression.hpp; src/asm_expression.cpp | `24_asm_expression.md` | complete (1 yes/H, 2-3 yes/M; alias-method cluster `lineNumber()`/`labelType()` etc.) |
| 25 | asm_optimize | include/zhinst/asm_optimize.hpp; src/asm_optimize.cpp | `25_asm_optimize.md` | pending |
| 26 | assembler | include/zhinst/assembler.hpp; src/assembler.cpp | `26_assembler.md` | in-progress |
| 27 | node_map_data | include/zhinst/node_map_data.hpp; src/node_map_data.cpp; src/get_node_map.cpp | `27_node_map_data.md` | in-progress |
| 28 | awg_compiler | include/zhinst/awg_compiler.hpp; src/awg_compiler.cpp | `28_awg_compiler.md` | pending |
| 29 | device_type | include/zhinst/device_type.hpp; src/device_type.cpp | `29_device_type.md` | pending |
| 30 | awg_device_props | include/zhinst/awg_device_props.hpp; src/awg_device_props.cpp | `30_awg_device_props.md` | pending |
| 31 | device_constants | include/zhinst/device_constants.hpp; src/device_constants.cpp | `31_device_constants.md` | in-progress |
| 32 | frontend_lowering | include/zhinst/frontend_lowering.hpp; src/frontend_lowering.cpp | `32_frontend_lowering.md` | pending |
| 33 | awg_assembler | include/zhinst/awg_assembler.hpp; src/awg_assembler.cpp | `33_awg_assembler.md` | pending |
| 34 | eval_results | include/zhinst/eval_results.hpp; src/eval_results.cpp | `34_eval_results.md` | pending |
| 35 | rawwave | include/zhinst/rawwave.hpp; src/rawwave.cpp | `35_rawwave.md` | complete (0 yes, 5 not-misnomer/H; sample units verified) |
| 36 | cache | include/zhinst/cache.hpp; src/cache.cpp | `36_cache.md` | complete (5 yes/M, 3 not-misnomer, 2 cross-batch→09) |
| 37 | signal | include/zhinst/signal.hpp; src/signal.cpp | `37_signal.md` | pending |
| 38 | play_config | include/zhinst/play_config.hpp; src/play_config.cpp | `38_play_config.md` | complete (1 yes/L, 5 not-misnomer, 3 cross-batch→49) |
| 39 | math_compiler | include/zhinst/math_compiler.hpp; src/math_compiler.cpp | `39_math_compiler.md` | in-progress |
| 40 | generic_device_type | include/zhinst/generic_device_type.hpp; src/generic_device_type.cpp | `40_generic_device_type.md` | pending |
| 41 | device_subclasses | include/zhinst/device_subclasses.hpp; src/device_hdawg.cpp; src/device_shf.cpp; src/device_uhf.cpp; src/device_vhf.cpp; src/device_ghf.cpp; src/device_mf.cpp; src/device_hf2.cpp; src/device_pqsc.cpp; src/device_qhub.cpp; src/device_shfacc.cpp; src/device_hwmock.cpp; src/device_unknown.cpp | `41_device_subclasses.md` | pending |
| 42 | expression | include/zhinst/expression.hpp; src/expression.cpp | `42_expression.md` | pending |
| 43 | wavetable_helpers | include/zhinst/wavetable_helpers.hpp | `43_wavetable_helpers.md` | pending |
| 44 | asm_list | include/zhinst/asm_list.hpp; src/asm_list.cpp | `44_asm_list.md` | pending |
| 45 | wavetable_front | include/zhinst/wavetable_front.hpp; src/wavetable_front.cpp; src/wavetable_manager_front.cpp | `45_wavetable_front.md` | pending |
| 46 | wavetable_ir | include/zhinst/wavetable_ir.hpp; src/wavetable_ir.cpp; src/wavetable_manager_ir.cpp | `46_wavetable_ir.md` | pending |
| 47 | elf_writer | include/zhinst/elf_writer.hpp; src/elf_writer.cpp; src/write_waves_to_elf.cpp | `47_elf_writer.md` | pending |
| 48 | address_impl | include/zhinst/address_impl.hpp | `48_address_impl.md` | complete (0 yes; structural note re. type holding non-address values) |
| 49 | asm_commands_impl | include/zhinst/asm_commands_impl.hpp; src/asm_commands_impl.cpp; src/asm_commands_impl_hirzel.cpp; src/asm_commands_impl_cervino.cpp | `49_asm_commands_impl.md` | complete (5 yes/M, 1 yes/H — `flag`→`isWaveformCmd` pattern; play_config arbitrations re-routed to batch 10) |
| 50 | asm_parser_context | include/zhinst/asm_parser_context.hpp; src/asm_parser_context.cpp | `50_asm_parser_context.md` | complete (2 yes/M — `addCommand::cmd`/`args` swap; cross→24) |
| 51 | callbacks | include/zhinst/callbacks.hpp; src/callbacks.cpp | `51_callbacks.md` | in-progress |
| 52 | compiler_message | include/zhinst/compiler_message.hpp; src/compiler_message.cpp | `52_compiler_message.md` | complete (1 yes/H — `showLine` polarity inverted) |
| 53 | wave_index_tracker | include/zhinst/wave_index_tracker.hpp; src/wave_index_tracker.cpp | `53_wave_index_tracker.md` | pending |
| 54 | mf_sfc | src/mf_sfc.cpp | `54_mf_sfc.md` | pending |
