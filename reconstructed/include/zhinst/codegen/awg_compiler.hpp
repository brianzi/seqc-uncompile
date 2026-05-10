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
    explicit AWGCompiler(AWGCompilerConfig const& config);  // @0x103210
    ~AWGCompiler();                                          // @0x103260

    // --- Compilation ---
    void compileString(std::string const& source);            // @0x1032b0
    void compileFile(std::string const& path);                // @0x1032a0
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
    void writeToFile(std::string const& path);                         // @0x1032e0
    void writeAssemblerToFile(std::string const& path);                // @0x1032f0

    // --- Reporting (sret pattern: rdi=return, rsi=this) ---
    std::string getCompileReport();              // @0x1033c0
    std::string getJsonWaveformMemoryInfo();     // @0x1033e0

    // --- Callbacks (weak_ptr refcount management) ---
    void setCancelCallback(std::weak_ptr<CancelCallback> cb);      // @0x103300
    void setProgressCallback(std::weak_ptr<ProgressCallback> cb);  // @0x103360

private:
    AWGCompilerImpl* impl_;  // +0x00, sole member
};

}  // namespace zhinst
