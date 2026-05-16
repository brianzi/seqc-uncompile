// ============================================================================
// Reconstructed from disassembly of _seqc_compiler.so
// Classes: zhinst::AWGCompiler (pimpl wrapper, 8 bytes)
//          zhinst::AWGCompilerImpl (0x2C0 bytes)
//
// AWGCompiler is the public-facing facade. It holds a single raw pointer
// to an AWGCompilerImpl, allocated with `new` (size 0x2C0) in the ctor.
// All methods are thin wrappers: `mov rdi,[rdi]; jmp Impl::method`.
//
// AWGCompilerImpl layout (total 0x2C0 = 704 bytes):
//   +0x000  AWGCompilerConfig const*      config_
//   +0x008  DeviceConstants               deviceConstants_ (0x90 bytes, populated by getDeviceConstants)
//   +0x098  shared_ptr<WavetableFront>    wavetable_
//   +0x0A8  shared_ptr<WavetableIR>       wavetableIR_ (zeroed in ctor)
//   +0x0B8  (padding, 8 bytes)
//   +0x0C0  Compiler                      compiler_ (0x138 bytes, base class embedded)
//   +0x200  std::string[4]                strings_ (4 × 24 = 0x60 bytes, zeroed)
//   +0x260  vector<CompilerMessage>        compileMessages_
//   +0x278  AWGAssembler                  assembler_ (8 bytes, pimpl)
//   +0x280  vector<?>                     outputData_ (24 bytes)
//   +0x298  weak_ptr<CancelCallback>      cancelCallback_
//   +0x2A8  weak_ptr<ProgressCallback>    progressCallback_
//   +0x2B8  (padding, 8 bytes)
// ============================================================================
#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace zhinst {

class AWGCompilerConfig;
class AWGCompilerImpl;
class CancelCallback;
struct CompileSeqcIntrospection;  // for T3c/T4 fillIntrospection friend
class ProgressCallback;

// ============================================================================
// AWGCompiler — thin pimpl wrapper (8 bytes: one raw pointer)
//
// Binary layout confirmed by:
//   ctor @0x103210: `new(0x2C0)` → AWGCompilerImpl ctor → store ptr at [this]
//   dtor @0x103260: load [this], null it, call Impl dtor, `delete(ptr, 0x2C0)`
//   all methods: `mov rdi,[rdi]; jmp Impl::method`
// ============================================================================
//! \brief Public-facing facade for compiling SeqC source into an AWG ELF.
//!
//! This is the entry point exposed to Python (`_seqc_compiler.compile_seqc`)
//! and to C++ embedders.  It is a thin pimpl over `AWGCompilerImpl`,
//! which in turn embeds the inner `Compiler` orchestrator and an
//! `AWGAssembler` for any post-pipeline `.asm` round-trips.  Source is
//! supplied via `compileString` / `compileFile`; precompiled binary
//! waveforms can be merged in via `addWaveforms`; the resulting ELF is
//! emitted by `writeToFile` / `writeToStream`; and human-readable
//! status is exposed through `getCompileReport` and
//! `getJsonWaveformMemoryInfo`.  Long-running compiles can be
//! interrupted or progress-reported via the optional weak callbacks.
class AWGCompiler {
public:
    //! \brief Construct an `AWGCompiler` ready to compile SeqC for a
    //!        single AWG sequencer.
    //!
    //! \details Allocates the 0x2C0-byte `AWGCompilerImpl` heap object
    //! via plain `operator new` and stores its address in the sole
    //! pimpl pointer.  The impl ctor in turn:
    //!  - copies `config` into `config_` (raw pointer; `config` must
    //!    outlive this `AWGCompiler`),
    //!  - looks up the per-device `DeviceConstants` table for
    //!    `config.deviceType` and copies the 0x90-byte block into
    //!    `deviceConstants_`,
    //!  - default-constructs the inner `Compiler`, the embedded
    //!    `AWGAssembler` pimpl, the `compileMessages_` vector, and
    //!    the four scratch strings,
    //!  - leaves `wavetable_`, `wavetableIR_`, and the cancel /
    //!    progress weak-callbacks empty.
    //!
    //! No source is parsed and no waveform memory is touched until
    //! one of `compileString` / `compileFile` is called.
    //!
    //! \param config  Compilation configuration (device type, sample
    //!                rate override, debug flags, source-compression
    //!                toggle, waveform cache mode, …).  Captured by
    //!                raw pointer; the caller owns the lifetime and
    //!                must keep it alive for the full lifetime of
    //!                this object.
    explicit AWGCompiler(AWGCompilerConfig const& config);  // @0x103210
    //! \brief Destroy the compiler and release every owned resource.
    //!
    //! \details Runs `AWGCompilerImpl`'s destructor (which releases
    //! the embedded `Compiler`, the `AWGAssembler` pimpl, the
    //! `wavetable_` / `wavetableIR_` shared handles, the message
    //! vector, the wave-path list, and the two weak-callback slots
    //! in reverse construction order), then frees the impl block
    //! with the matching `operator delete(impl_, sizeof(...))`.
    //! `impl_` is nulled before return so a double-destroy is a
    //! no-op rather than a use-after-free.
    ~AWGCompiler();                                          // @0x103260

    // --- Compilation ---
    //! \brief Compile a SeqC program supplied directly as a string.
    //!
    //! \details This is the canonical entry point; `compileFile`
    //! ultimately delegates here after slurping the file.  The
    //! impl performs the full pipeline:
    //!   1. **Device-type validation** — if the configured device
    //!      type has no entry in the `DeviceConstants` table the
    //!      call throws `ZIAWGCompilerException` with either
    //!      `ErrorMessageT(0xDA)` (Hirzel-class device) or
    //!      `ErrorMessageT(0xDB)` (other devices).
    //!   2. **State reset** — the source string is stashed in
    //!      `sourceText_`, `compileMessages_` is cleared, and the
    //!      previous `wavetableIR_` is dropped.
    //!   3. **Compile** — `Compiler::compile(source)` runs the
    //!      lex / parse / lower / optimise / codegen pipeline,
    //!      producing a `vector<Assembler>` and a fresh
    //!      `WavetableIR` which are moved into the impl.  Any
    //!      exception escaping the inner compile is wrapped in a
    //!      `ZIAWGCompilerException` carrying the original
    //!      `what()` text (or the empty string when `what()` is
    //!      null), after first appending the inner pipeline's
    //!      messages to `compileMessages_`.
    //!   4. **Pretty-print the assembler text** into
    //!      `assemblerText_` for `getAssemblerHeader` /
    //!      `writeAssemblerToFile` consumers.  Labels are
    //!      right-padded to 8 characters and the next instruction
    //!      shares their line; all other instructions are emitted
    //!      with an 8-space indent.
    //!   5. **Append messages** from the inner `Compiler` to
    //!      `compileMessages_`.
    //!   6. **Assemble** the opcode stream via
    //!      `AWGAssembler::assembleAsmList`.
    //!   7. **Hardware-limit checks** — opcode count vs
    //!      `DeviceConstants::maxProgramSize` and waveform count
    //!      vs the per-device waveform-program limit; either
    //!      overflow appends an error message and throws
    //!      `ZIAWGCompilerException` (see IF-192 / IF-195 for the
    //!      historical `maxSequenceLen` mis-binding).
    //!   8. **Progress notification** — the held
    //!      `progressCallback_` (if any) is invoked with `1.0`.
    //!
    //! On a syntax error the parser records the diagnostic in
    //! `compileMessages_` but does not throw; subsequent calls to
    //! `writeToStream` / `writeAssemblerToFile` short-circuit on
    //! `Compiler::hadSyntaxError()`.
    //!
    //! \param source  SeqC program text.  Empty input is legal but
    //!                will surface `EmptyInput` later when output
    //!                is requested.
    //! \throws ZIAWGCompilerException  on invalid device type,
    //!                opcode-memory overflow, waveform-program
    //!                overflow, or any wrapped exception escaping
    //!                the inner compile.
    void compileString(std::string const& source);            // @0x1032b0
    //! \brief Read a SeqC source file from disk and compile it.
    //!
    //! \details The path is `boost::filesystem::status`-checked;
    //! a missing or unreadable entry raises
    //! `ZIAWGCompilerException` directly.  On success the file is
    //! slurped via `ifstream::rdbuf` into a string, `path` is
    //! recorded in `sourceFilename_` (used later for the
    //! `getAssemblerHeader` "Source file :" line and the
    //! `getJsonArguments` `"source"` field), and the contents
    //! are forwarded to `compileString`, which performs the rest
    //! of the pipeline.
    //!
    //! \param path  Filesystem path to the SeqC source file.
    //! \throws ZIAWGCompilerException  When the file does not
    //!                exist, plus all exceptions documented for
    //!                `compileString`.
    void compileFile(std::string const& path);                // @0x1032a0
    //! \brief Pre-load externally-provided binary or text
    //!        waveforms into the wavetable so SeqC code can
    //!        reference them by name.
    //!
    //! \details Each entry is appended to the persistent
    //! `wavePaths_` list (used later by the `getJsonArguments`
    //! `"waves"` array).  The file extension (4..7 characters
    //! including the leading dot) selects the loader:
    //!   - `.csv`, `.txt` — text format; delegates to
    //!     `WavetableFront::newWaveformFromFile` with type 0.
    //!   - `.bin`, `.wave` — packed `uint16_t` words; each word
    //!     is decoded by `awg2double` (sample) and `awg2marker`
    //!     (marker bits) into a freshly built `Signal` which is
    //!     registered with type 1.
    //!   - `.bin16`, `.wave16` — packed `uint32_t` words (HDAWG
    //!     32-bit format); the high 16 bits are decoded by
    //!     `awg2double16` and the low 2 bits are the marker
    //!     bits.
    //!   - any other extension — emits a warning and skips the
    //!     entry.
    //!
    //! Per-entry I/O failures throw `ZIAWGCompilerException`
    //! formatted with `ErrorMessageT(0xE3)`.  Before each entry
    //! the held `cancelCallback_` is polled; if it reports
    //! cancellation the loop breaks early and any remaining
    //! paths are silently skipped.
    //!
    //! \param paths  Filesystem paths to the waveform files to
    //!               register.  Order is preserved in
    //!               `wavePaths_`.
    //! \throws ZIAWGCompilerException  When a file cannot be
    //!               opened (`ErrorMessageT(0xE3)`) or when the
    //!               wavetable rejects the registration.
    void addWaveforms(std::vector<std::string> const& paths); // @0x1032c0

    // --- Output ---
    /*! \brief Serialise the most recently compiled program into a
     *  custom 32-bit ELF and write the bytes to `os`.
     *
     *  \details Forwards to the inner implementation, which:
     *    1. Returns immediately when the parser recorded a syntax
     *       error during the last `compileString` / `compileFile`
     *       (no output is produced and no exception is raised).
     *    2. Throws `ZIAWGCompilerException` formatted with
     *       `EmptyInput` when the assembler holds no opcodes.
     *    3. Constructs an `ElfWriter` initialised with the device's
     *       `addressImpl` memory offset.
     *    4. Emits each used waveform: in **mapped** mode (when the
     *       device exposes pre-compiled waveform memory) waveforms
     *       are added in `WaveOrder::ByWaveIndex`; in **absolute**
     *       mode they are added in `WaveOrder::ByIndex` with
     *       per-waveform alignment-padding computed from
     *       `addressValue` and `elfAlignment_` and waveforms whose
     *       `signal.length()` is zero are skipped.
     *    5. Updates the writer's memory offset to
     *       `WavetableIR::getNextSegmentAddress()` and appends the
     *       opcode stream as `.text`.
     *    6. Adds the trailing metadata sections in fixed order:
     *       `.filename`, `.c` and `.asm` (raw or zlib-compressed
     *       depending on `AWGCompilerConfig::compressSource`),
     *       `.linenr`, the device-conditional node-access section
     *       (`.nodes` for UHFQA / UHFLI, `.nodes_json` otherwise),
     *       optional `.channels`, optional `.required_sample_rate`
     *       (only when `Compiler::usedDeviceSampleRate()` is true
     *       and the configured sample rate is finite), `.waveforms`,
     *       `.wavemem`, `.arguments`, `.version_json`, and
     *       `.version_bin`.
     *    7. Calls `ElfWriter::writeFile(os)` to flush the assembled
     *       binary.
     *
     *  The ELF schema and per-section formats are documented in
     *  `notes/elf_format.md`.
     *
     *  \param os      Output stream that receives the binary ELF
     *                 bytes.  Caller is responsible for opening the
     *                 stream in binary mode if writing to disk.
     *  \param format  The originating filename (used for the
     *                 `.filename` and `.arguments` sections and as
     *                 the source name passed to the source
     *                 compressor).  Pass the path supplied to
     *                 `compileFile`, or an empty string for
     *                 in-memory compiles.
     *  \throws ZIAWGCompilerException  When the assembler has no
     *                 opcodes (i.e. `compileString` / `compileFile`
     *                 was never called or produced no output).
     */
    void writeToStream(std::ostream& os, std::string const& format);  // @0x1032d0
    //! \brief Open `path` and emit the compiled ELF to it.
    //!
    //! \details Opens an `std::ofstream` in binary-truncate mode
    //! (`std::ios::binary | std::ios::trunc`) and forwards to
    //! `writeToStream(ofs, path)`, so all caveats and exceptions
    //! documented for `writeToStream` apply unchanged.  The
    //! stream is closed by its destructor on return.
    //!
    //! \param path  Destination file path.  Existing contents
    //!              are truncated.
    //! \throws ZIAWGCompilerException  Propagated from
    //!              `writeToStream`.
    void writeToFile(std::string const& path);                         // @0x1032e0
    //! \brief Write the human-readable assembler listing to a
    //!        text file.
    //!
    //! \details Short-circuits on two no-output conditions: a
    //! recorded syntax error from the most recent compile, or
    //! an empty `assemblerText_` (i.e. `compileString` was never
    //! called or produced no instructions).  When output is
    //! warranted, the impl prepends the multi-line banner
    //! produced by `getAssemblerHeader(path)` (a "do not edit"
    //! notice plus the source filename, the hard-coded
    //! `"ziAWG Compiler Version 26.01.3.9"` string, and a local
    //! timestamp) followed by `assemblerText_` and a trailing
    //! newline, then opens the destination as `std::ios::trunc`
    //! (text mode) and writes the assembled string.  Unlike
    //! `writeToFile` this output is plain text and does not
    //! pass through the ELF writer.
    //!
    //! \param path  Destination file path.  Existing contents
    //!              are truncated; nothing is written when the
    //!              short-circuit conditions hold.
    void writeAssemblerToFile(std::string const& path);                // @0x1032f0

    // --- Reporting (sret pattern: rdi=return, rsi=this) ---
    //! \brief Build a multi-line text report combining all
    //!        compile messages and the assembler's own report.
    //!
    //! \details Iterates `compileMessages_` in insertion order
    //! and appends each entry's `str(false)` (i.e. with the
    //! line-number prefix included) followed by `"\n"`, then
    //! concatenates `AWGAssembler::getReport()` (which surfaces
    //! the assembler's own diagnostics, statistics, and
    //! optimisation summaries).  The result is suitable for
    //! display in a status pane or for being routed through the
    //! Python binding's report channel.
    //!
    //! \return  Concatenated report string.  Empty when neither
    //!          the pipeline nor the assembler produced output.
    std::string getCompileReport();              // @0x1033c0
    //! \brief Serialise per-waveform memory accounting for the
    //!        most recent compile as a JSON object string.
    //!
    //! \details Returns `"{}"` when `wavetableIR_` is null
    //! (i.e. no successful compile yet).  Otherwise iterates
    //! every used waveform via
    //! `wavetableIR_->forEachUsedWaveform(..., WaveOrder::ByIndex)`,
    //! tracking per-waveform contributions to the on-FPGA
    //! waveform memory while honouring cache-line crossings
    //! and the per-device cap of `waveformAlignment *
    //! (isHirzel ? maxBlocks : cachePageCount)`.  Waveforms
    //! whose `crossesCacheLine_` flag is unset and whose
    //! aligned base address is not already in the seen-set are
    //! counted as exceeding the FPGA waveform window — the
    //! resulting JSON carries `"has_playback"` and
    //! `"usage_fraction"` keys reflecting these aggregates.
    //!
    //! \return  JSON object literal as a string (always valid
    //!          JSON, never empty).
    std::string getJsonWaveformMemoryInfo();     // @0x1033e0

    // --- Callbacks (weak_ptr refcount management) ---
    //! \brief Install (or detach) the cancellation hook polled by
    //!        the compile pipeline and the inner `Compiler`.
    //!
    //! \details Stores `cb` into `cancelCallback_` and forwards
    //! the same handle to `Compiler::setCancelCallback`, so both
    //! the outer ELF-emission stage (which polls between waveform
    //! file reads in `addWaveforms`) and the inner compile stages
    //! observe the same cancel hook.  Pass an empty `weak_ptr` to
    //! detach.
    //!
    //! \param cb  Weak handle to a user-provided
    //!            `CancelCallback`.  The compiler does not extend
    //!            its lifetime; the caller is responsible for
    //!            keeping the underlying `shared_ptr` alive for as
    //!            long as cancellation may be requested.
    void setCancelCallback(std::weak_ptr<CancelCallback> cb);      // @0x103300
    //! \brief Install (or detach) the progress-reporting hook
    //!        invoked by the compile pipeline.
    //!
    //! \details Stores `cb` into `progressCallback_` and forwards
    //! the same handle to `Compiler::setProgressCallback`.  The
    //! impl invokes the held callback with `1.0` once
    //! `compileString` finishes successfully; intermediate
    //! progress values are emitted by the inner pipeline.  Pass
    //! an empty `weak_ptr` to detach.
    //!
    //! \param cb  Weak handle to a user-provided
    //!            `ProgressCallback`.  Caller-owned lifetime.
    void setProgressCallback(std::weak_ptr<ProgressCallback> cb);  // @0x103360

private:
    //! \brief Heap-allocated implementation owned by this pimpl
    //!        facade.
    //! \details Allocated with `operator new(0x2C0)` in the
    //! constructor and released by the destructor; every public
    //! method tail-calls the matching `AWGCompilerImpl` member
    //! via `impl_`.  Never `nullptr` for the lifetime of the
    //! enclosing `AWGCompiler` (the constructor either succeeds
    //! or propagates `std::bad_alloc` before storing here).
    AWGCompilerImpl* impl_;  // +0x00, sole member

    // T3c/T4: additive friend grant for the seqcc introspection
    // helper declared in compile_seqc.hpp.  Not present in the
    // original binary; lets `fillIntrospection()` populate a
    // `CompileSeqcIntrospection` from `impl_`'s private accessors
    // without widening AWGCompiler's public API.  All future Phase-T
    // IR captures route through this single helper rather than
    // adding more friend grants per stage.
    friend void
    fillIntrospection(AWGCompiler const&,
                      CompileSeqcIntrospection&) noexcept;
};

}  // namespace zhinst
