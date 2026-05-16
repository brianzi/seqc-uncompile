// ============================================================================
// Reconstructed from disassembly of _seqc_compiler.so
// AWGCompiler pimpl facade + AWGCompilerImpl
//
// AWGCompiler methods are all thin wrappers confirmed by disassembly:
//   each loads impl_ from [this], then jumps to the corresponding Impl method.
//   Exception: setCancelCallback/setProgressCallback do weak_ptr refcount
//   management around the forwarding call.
//   Exception: getCompileReport/getJsonWaveformMemoryInfo use sret pattern
//   (rdi=return string, rsi=this → load impl from [rsi]).
// ============================================================================

#include "zhinst/codegen/awg_compiler.hpp"
#include "zhinst/codegen/awg_compiler_config.hpp"
#include "zhinst/codegen/awg_assembler.hpp"
#include "zhinst/core/callbacks.hpp"
#include "zhinst/infra/calver.hpp"
#include "zhinst/codegen/compile_seqc.hpp"
#include "zhinst/codegen/compiler.hpp"
#include "zhinst/core/compiler_message.hpp"
#include "zhinst/device/device_constants.hpp"
#include "zhinst/io/elf_writer.hpp"
#include "zhinst/core/error_messages.hpp"
#include "zhinst/core/exception.hpp"
#include "zhinst/core/format_time.hpp"
#include "zhinst/infra/logging.hpp"
#include "zhinst/runtime/node_map_data.hpp"
#include "zhinst/ast/seqc_parser_context.hpp"
#include "zhinst/waveform/signal.hpp"
#include "zhinst/waveform/wavetable_front.hpp"
#include "zhinst/waveform/wavetable_ir.hpp"
#include "zhinst/waveform/waveform_ir.hpp"

#include <boost/filesystem.hpp>
#include <boost/json.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>

#include <zlib.h>

#include <ctime>
#include <fstream>
#include <ostream>
#include <set>
#include <sstream>

namespace zhinst {

// Forward declarations for helpers not yet reconstructed
namespace util { namespace wave {
//! \brief Decode a single 14-bit-signed + 2-marker-bit AWG sample
//!        word into a normalised double in `[-1.0, +1.0)`.
//! \details Masks off the two marker bits (`& 0xFFFC`),
//! sign-extends as `int16_t`, then divides by `32767.0`. Definition
//! lives in `src/core/stubs.cpp`.
//! \param raw 16-bit packed AWG sample word.
//! \return Sample as floating-point amplitude.
double awg2double(uint16_t raw);                 // @0x2996d0 — extract 18-bit sample as double
//! \brief Extract the two marker bits from an AWG sample word.
//! \details Returns `raw & 0x3` as a `uint8_t`.
//! \param raw 16-bit packed AWG sample word.
//! \return Marker bits in the low two bits of the return value.
uint8_t awg2marker(uint16_t raw);                // @0x2996f0 — extract marker bits from raw word
//! \brief Decode an HDAWG 32-bit sample word (shifted-and-packed)
//!        into a normalised double in `[-1.0, +1.0)`.
//! \details Right-shifts by 2, truncates to `uint16_t`, sign-extends
//! as `int16_t`, then divides by `32767.0`. Definition lives in
//! `src/core/stubs.cpp`.
//! \param raw 32-bit packed HDAWG sample word.
//! \return Sample as floating-point amplitude.
double awg2double16(uint32_t raw);               // @0x299740 — 32-bit HDAWG sample to double
}} // namespace util::wave
using util::wave::awg2double;
using util::wave::awg2marker;
using util::wave::awg2double16;

// Compress source string helper used by writeToStream (anonymous namespace in binary)
// @0x109e90 — zlib deflate level 9, 0x8000-byte output chunks, Z_FINISH flush
namespace {
std::string compressSourceString(std::string const& source, std::string const& outputName) {
    z_stream strm{};
    strm.next_in = reinterpret_cast<Bytef*>(const_cast<char*>(source.data()));
    strm.avail_in = static_cast<uInt>(source.size());

    int ret = deflateInit(&strm, 9);  // @0x109f12: level 9
    if (ret != Z_OK) {
        throw ZIAWGCompilerException(
            ErrorMessages::format(ErrorMessageT(0x1e), outputName));
    }

    std::string result;
    uint8_t chunk[0x8000];  // @0x109f7f: 0x8000-byte output buffer

    do {
        strm.next_out = chunk;
        strm.avail_out = sizeof(chunk);
        ret = deflate(&strm, Z_FINISH);  // @0x109f91: flush=4=Z_FINISH
        if (ret == Z_STREAM_ERROR) {
            deflateEnd(&strm);
            throw ZIAWGCompilerException(
                ErrorMessages::format(ErrorMessageT(0x1e), outputName));
        }
        size_t have = sizeof(chunk) - strm.avail_out;
        result.append(reinterpret_cast<char*>(chunk), have);
    } while (strm.avail_out == 0);

    deflateEnd(&strm);  // @0x109fc8

    if (ret != Z_STREAM_END) {
        throw ZIAWGCompilerException(
            ErrorMessages::format(ErrorMessageT(0x1e), outputName));
    }

    return result;
}
} // anonymous namespace

// ============================================================================
// AWGCompilerImpl — the real implementation (0x2C0 bytes)
// ============================================================================
//! \brief Pimpl behind the public `AWGCompiler` facade; orchestrates
//!        the full SeqC → ELF compile pipeline.
//!
//! \details Owns the per-device `DeviceConstants`, the
//! `WavetableFront` waveform registry, the inner `Compiler` (which
//! drives parse / lower / optimise / codegen), an embedded
//! `AWGAssembler` (used to encode the `Compiler`'s
//! `vector<Assembler>` into opcode words and for post-pipeline
//! `.asm` round-trips), the diagnostic `compileMessages_` vector,
//! the cached source / assembler text, the externally-supplied
//! waveform-file paths, and the cancel / progress weak callbacks.
//! `compileString` / `compileFile` drive the front end and produce
//! `wavetableIR_`; `addWaveforms` registers external waveforms
//! before compile; `writeToStream` / `writeToFile` /
//! `writeAssemblerToFile` serialise the result; `getCompileReport`
//! / `getJsonWaveformMemoryInfo` / `getBinVersion` /
//! `getJsonVersion` / `getJsonArguments` expose status and metadata.
class AWGCompilerImpl {
public:
    //! \brief Construct the impl and prime every owned subordinate
    //!        for a single AWG sequencer.
    //! \details Captures `config` by raw pointer (caller-owned
    //! lifetime), copies the matching `DeviceConstants` block via
    //! `getDeviceConstants(config.deviceType)`, allocates a fresh
    //! `WavetableFront` shared via `make_shared`, default-constructs
    //! the inner `Compiler` against the captured config / constants
    //! / wavetable, and default-constructs the embedded
    //! `AWGAssembler`, the four scratch strings, the message vector,
    //! the wave-path list, and the two weak callbacks.
    //! \param config  Compilation configuration; the impl borrows
    //!                a raw pointer and the caller must keep it
    //!                alive.
    explicit AWGCompilerImpl(AWGCompilerConfig const& config);  // @0x103b40
    //! \brief Release the embedded `Compiler`, the `AWGAssembler`
    //!        pimpl, the wavetable handles, the message vector,
    //!        and the wave-path list in reverse construction order.
    ~AWGCompilerImpl();                                          // @0x103400

    //! \brief Drive the full SeqC → opcode pipeline on an in-memory
    //!        source string.
    //! \details Validates `config_->deviceType` against
    //! `deviceConstants_`; stashes `source` into `sourceText_`;
    //! clears `compileMessages_` and resets `wavetableIR_`; calls
    //! `compiler_.compile(source)` and moves the resulting
    //! `vector<Assembler>` and `WavetableIR` into local state;
    //! pretty-prints the assembly into `assemblerText_`; appends
    //! the inner pipeline's messages; runs `assembler_.assembleAsmList`
    //! to produce opcode words; enforces the device's
    //! opcode-memory and waveform-program limits; and finally
    //! invokes `progressCallback_` with `1.0`.  Any exception
    //! escaping `Compiler::compile` is wrapped in
    //! `ZIAWGCompilerException` after appending the inner
    //! pipeline's messages.
    //! \param source  SeqC program text.
    //! \throws zhinst::Exception  On invalid device type, opcode
    //!         overflow, waveform overflow, or wrapped inner
    //!         exceptions.
    //! \warning Resets `wavetableIR_` and clears
    //!             `compileMessages_` before running, so any
    //!             previous state is silently dropped on re-entry.
    void compileString(std::string const& source);

    // ------------------------------------------------------------------
    // T5b.5 — pipeline step methods.  `compileString()` is a one-liner
    // dispatcher calling these in sequence; the seqcc driver
    // (`SeqcDriver::compile()`) calls them individually via
    // `AWGCompiler`'s public forwarders to honour `--to=<stage>`
    // semantics (closes IF-304 partially — back-end work is skipped
    // when --to=lower/asm; front-end work inside Compiler::compile()
    // still runs since AWGCompiler does not expose Compiler internals).
    //
    // The binary has no equivalent member methods.  The recon
    // partitioning lands on verified binary step boundaries:
    //
    //   stepInnerCompile      0x106cb0..0x107341  (validate, source
    //                                              capture, message
    //                                              clear, Compiler
    //                                              compile, asm-text
    //                                              build)
    //   stepAssembleOpcodes   0x107341..0x10739e  (assembler_
    //                                              .assembleAsmList)
    //   stepCheckLimits       0x10739e..0x107478  (opcode + wavemem
    //                                              limit checks +
    //                                              progress callback)
    //
    // Each step reads from / writes to the lifted
    // `compileString_asmList_` member so intermediate state survives
    // between calls.  See IF-306 for sanction.
    // ------------------------------------------------------------------

    //! \brief Steps 1-8 of `compileString()`: validate device type
    //!        against `deviceConstants_`, stash `source` into
    //!        `sourceText_`, clear `compileMessages_`, reset
    //!        `wavetableIR_`, call `compiler_.compile(source)`,
    //!        pretty-print the resulting `AsmList` into
    //!        `assemblerText_`, and append the inner pipeline's
    //!        messages.  Populates `compileString_asmList_` for the
    //!        next two steps.  Exception escaping `compiler_.compile`
    //!        is wrapped in `ZIAWGCompilerException` after appending
    //!        the inner pipeline's messages.
    void stepInnerCompile(std::string const& source);

    //! \brief Step 9 of `compileString()`: hand the captured
    //!        `compileString_asmList_` to `assembler_.assembleAsmList`
    //!        to produce the device-encoded opcode word vector.
    void stepAssembleOpcodes();

    //! \brief Steps 10-12 of `compileString()`: enforce the device's
    //!        opcode-memory limit (`deviceConstants_.maxProgramSize`)
    //!        and waveform-program limit
    //!        (`deviceConstants_.maxSequenceLen` against the
    //!        non-null waveform count in `wavetableIR_`), then invoke
    //!        `progressCallback_` with `1.0` on success.
    void stepCheckLimits();

    //! \brief Read-only view of the pretty-printed assembler text
    //!        produced by `stepInnerCompile`.
    //! \details Tooling accessor added in T5b.5; no counterpart in the
    //! original binary.  Returns a reference to the cached
    //! `assemblerText_` member so the seqcc driver can route
    //! `--to=asm` without going through `writeToStream` (which would
    //! require running `stepAssembleOpcodes` + `stepCheckLimits` to
    //! produce an ELF and parsing it back).  Returns an empty string
    //! before the first successful `stepInnerCompile`.
    std::string const& assemblerText() const { return assemblerText_; }
    //! \brief Slurp a SeqC source file from disk and forward to
    //!        `compileString`.
    //! \details `boost::filesystem::status`-checks the path
    //! (raising `ZIAWGCompilerException("File not found: ...")`
    //! when the entry is missing), reads the whole file via
    //! `ifstream::rdbuf`, records `path` in `sourceFilename_` (used
    //! by `getAssemblerHeader` and `getJsonArguments`), then
    //! delegates to `compileString`.
    //! \param path  Filesystem path to the SeqC source file.
    //! \throws zhinst::Exception  When the file is missing or the
    //!         delegated `compileString` raises.
    void compileFile(std::string const& path);
    //! \brief Pre-load externally-provided binary or text
    //!        waveforms into `wavetable_` for later SeqC reference.
    //! \details Appends the supplied paths to the persistent
    //! `wavePaths_` list (later surfaced through the
    //! `getJsonArguments` `"waves"` array), then iterates each
    //! path and dispatches on extension: `.csv`/`.txt` go through
    //! `WavetableFront::newWaveformFromFile` directly, `.bin`/`.wave`
    //! decode 16-bit packed sample/marker words via `awg2double` /
    //! `awg2marker` into a fresh `Signal`, and `.bin16`/`.wave16`
    //! decode HDAWG 32-bit words via `awg2double16`.  Other
    //! extensions are warned-and-skipped.  Between entries the
    //! `cancelCallback_` is polled and the loop exits early on
    //! cancellation.
    //! \param paths  Filesystem paths to register, in order.
    //! \throws zhinst::Exception  On per-file I/O failure
    //!         (`ErrorMessageT(0xE3)`).
    //! \note Always extends `wavePaths_` even for
    //!             unrecognised extensions; the warning-and-skip
    //!             path leaves the entry recorded for the
    //!             arguments JSON.
    void addWaveforms(std::vector<std::string> const& paths);
    //! \brief Serialise the most recent compile to a custom 32-bit
    //!        ELF and write it to `os`.
    //! \details Short-circuits silently when the parser flagged a
    //! syntax error; raises `ZIAWGCompilerException(EmptyInput)`
    //! when no opcodes were produced.  Otherwise constructs an
    //! `ElfWriter`, emits each used waveform in either mapped or
    //! absolute mode (depending on `deviceConstants_.hasPrecomp`),
    //! adds the opcode `.text`, and appends the trailing metadata
    //! sections (`.filename`, `.c`, `.asm`, `.linenr`, `.nodes` /
    //! `.nodes_json`, optional `.channels` and
    //! `.required_sample_rate`, `.waveforms`, `.wavemem`,
    //! `.arguments`, `.version_json`, `.version_bin`) in fixed
    //! order before flushing via `ElfWriter::writeFile`.
    //! `compressSource` toggles whether `.c` and `.asm` are zlib
    //! compressed.
    //! \param os      Output stream receiving the binary ELF.
    //! \param format  Originating filename used for the
    //!                `.filename` and `.arguments` sections.
    //! \throws zhinst::Exception  On `EmptyInput`.
    //! \warning Returns silently with no output when the most
    //!             recent compile recorded a syntax error — no
    //!             exception is raised.
    void writeToStream(std::ostream& os, std::string const& format);
    //! \brief Open `path` and emit the compiled ELF to it.
    //! \details Opens an `std::ofstream` in
    //! `std::ios::binary | std::ios::trunc` mode and forwards to
    //! `writeToStream(ofs, path)`; the stream is closed by its
    //! destructor on return.  All caveats and exceptions of
    //! `writeToStream` apply unchanged.
    //! \param path  Destination file path; existing contents are
    //!              truncated.
    //! \throws zhinst::Exception  Propagated from `writeToStream`.
    void writeToFile(std::string const& path);
    //! \brief Write the human-readable assembler listing for the
    //!        most recent compile to a text file.
    //! \details Returns silently when the parser flagged a syntax
    //! error or when `assemblerText_` is empty.  Otherwise builds
    //! the output by prefixing `getAssemblerHeader(path)` (the
    //! "do not edit" banner with source filename, hard-coded
    //! compiler version, and a local timestamp) ahead of
    //! `assemblerText_` and a trailing newline, then truncates and
    //! writes the destination as a plain text file.  This output
    //! does not pass through the ELF writer.
    //! \param path  Destination file path; existing contents are
    //!              truncated.
    //! \note No-ops when `assemblerText_` is empty (i.e. no
    //!       prior compile or all-empty compile) — silently
    //!       leaving any pre-existing file unmodified.
    void writeAssemblerToFile(std::string const& path);

    //! \brief Build a multi-line text report from
    //!        `compileMessages_` and the embedded assembler.
    //! \details Iterates each `CompilerMessage` in insertion order,
    //! emitting `msg.str(false)` (line-number-prefixed form)
    //! followed by `"\n"`, then concatenates
    //! `assembler_.getReport()`.
    //! \return  Concatenated report; empty when neither subordinate
    //!          produced output.
    std::string getCompileReport() const;           // @0x104030
    //! \brief Serialise per-waveform memory accounting for the
    //!        most recent compile as a JSON object string.
    //! \details Returns `"{}"` when `wavetableIR_` is null;
    //! otherwise iterates every used waveform via
    //! `wavetableIR_->forEachUsedWaveform(..., WaveOrder::ByIndex)`,
    //! tracking per-waveform contributions to FPGA waveform memory
    //! while honouring cache-line crossings and the per-device cap
    //! of `waveformAlignment * (isHirzel ? maxBlocks :
    //! cachePageCount)`.  Returns a JSON object with
    //! `"exceedsFpgaMemory"` and `"fpgaMemoryUsed"` keys.
    //! \return  JSON object string (always valid JSON).
    std::string getJsonWaveformMemoryInfo() const;  // @0x10a1b0
    //! \brief Build the 16-byte binary version blob for the
    //!        `.version_bin` ELF section.
    //! \details Concatenates LabOne version (`asBinary(getLaboneVersion())`,
    //! 4 bytes), a 4-byte device-family suffix tag (`hirz`, `grim`,
    //! `gurn`, `malo`, or `cerv` depending on `config_->deviceType`),
    //! `deviceConstants_.waveformRegBase` (4 bytes), and four zero
    //! pad bytes.
    //! \return  16-byte binary blob (not human-readable).
    std::string getBinVersion() const;              // @0x10b830

    //! \brief Install (or detach) the cancellation hook polled by
    //!        the compile pipeline.
    //! \details Stores `cb` into `cancelCallback_` and forwards
    //! the same handle to `compiler_.setCancelCallback` so both
    //! the outer waveform-loading stage and the inner compile
    //! observe the same cancel hook.
    //! \param cb  Weak handle; pass an empty `weak_ptr` to detach.
    void setCancelCallback(std::weak_ptr<CancelCallback> cb);      // @0x103eb0
    //! \brief Install (or detach) the progress-reporting hook
    //!        invoked by the compile pipeline.
    //! \details Stores `cb` into `progressCallback_` and forwards
    //! the same handle to `compiler_.setProgressCallback`.  The
    //! impl invokes the held callback with `1.0` once
    //! `compileString` finishes successfully.
    //! \param cb  Weak handle; pass an empty `weak_ptr` to detach.
    void setProgressCallback(std::weak_ptr<ProgressCallback> cb);  // @0x103f90

    //! \brief Return the embedded `Compiler`'s lowered AST root.
    //! \details Tooling accessor added in T3c; no counterpart in the
    //! original binary.  Wraps the friend-access read of
    //! `compiler_.ast_`.  Returns an empty `shared_ptr` before the
    //! first successful compile.
    std::shared_ptr<Node const> getLoweredAst() const {
        return compiler_.ast_;
    }

    //! \brief Return the embedded `Compiler`'s front-end wavetable
    //!        (registered waveforms + per-waveform IR + name index).
    //! \details Tooling accessor added in T4a; no counterpart in the
    //! original binary.  Wraps the friend-access read of
    //! `compiler_.wavetable_`.  Returns an empty `shared_ptr` when
    //! the compile failed before constructing the wavetable.
    std::shared_ptr<WavetableFront const> getWavetable() const {
        return compiler_.wavetable_;
    }

    //! \brief Return a snapshot of the embedded `Compiler`'s
    //!        `AsmList` at compile completion.
    //! \details Tooling accessor added in T3d; no counterpart in the
    //! original binary.  Returns a freshly-allocated `shared_ptr`
    //! holding a *copy* of `compiler_.asmList_` — the on-compiler
    //! member is owned by-value and would otherwise be destroyed
    //! when the AWGCompiler unwinds.  The copy is the same
    //! `AsmList` content that `Node::toJson()`'s id densification
    //! is computed against, captured *before* the wavetable-aware
    //! rewrite reshapes the list.  Returns an empty `shared_ptr`
    //! when the compile failed before producing an AsmList of
    //! any entries; an AsmList with zero entries still allocates
    //! and returns a non-empty handle so callers can distinguish
    //! "no compile" from "compile produced empty list".
    std::shared_ptr<AsmList const> getAsmList() const {
        return std::make_shared<AsmList>(compiler_.asmList_);
    }

    //! \brief Build the multi-line "do not edit" header prepended
    //!        to assembler-listing files by `writeAssemblerToFile`.
    //! \details Combines the banner, the destination path, the
    //! recorded `sourceFilename_` (when non-empty), the hard-coded
    //! `"ziAWG Compiler Version 26.01.3.9"` string, and a
    //! `formatTime`-rendered local timestamp.
    //! \param path  Destination filename, written into the second
    //!              line of the banner.
    //! \return  Header text including trailing blank line.
    std::string getAssemblerHeader(std::string const& path) const;  // @0x1083d0
    //! \brief Build the JSON object emitted as the `.arguments`
    //!        ELF section.
    //! \details Uses `boost::property_tree::ptree` with
    //! `"destination"` set to the supplied filename, an optional
    //! `"source"` entry from `sourceFilename_`, and a `"waves"`
    //! array built from `wavePaths_`.
    //! \param destination  Output filename written into the
    //!                     `"destination"` key.
    //! \return  Serialised JSON document.
    std::string getJsonArguments(std::string const& destination) const;  // @0x10a3c0
    //! \brief Build the JSON object emitted as the `.version_json`
    //!        ELF section.
    //! \details Carries `"compiler"` (LabOne binary version),
    //! `"target"` (device-family code name: `hirzel`, `grimsel`,
    //! `gurnigel`, `maloja`, or `cervino`), `"bitstream"`
    //! (`waveformRegBase`), an optional `"external_triggering"`
    //! key (`"dio"` or `"zsync"` depending on
    //! `compiler_.customFunctions_->externalTriggeringMode_`), and
    //! an optional `"required_options"` array sourced from
    //! `customFunctions_->usedFeatures_`.
    //! \return  Serialised JSON document.
    std::string getJsonVersion() const;                            // @0x10ac60

    //! \brief Build the `.nodes_json` body for non-UHF devices.
    //! \details Walks the compiler's node-access list, calls
    //! `NodeMapItem::toJson()` to obtain the per-node object,
    //! and looks each node up in `modeMap`.  When the node is
    //! present, attaches a `"modes"` array whose entries are the
    //! string forms (`"soft"` / `"direct"` / `"custom"`) of the
    //! associated `AccessMode` values, in `set<AccessMode>` sort
    //! order.  Each per-node object is appended to the returned
    //! object's `"nodes"` array.  Called from `writeToStream`
    //! after compilation succeeds; the returned object is
    //! `boost::json::serialize`d into the `.nodes_json` ELF
    //! section.
    //! \param nodes    Compiler-side node-access list (already
    //!                 ordered by registration).
    //! \param modeMap  Per-node access-mode set; nodes absent from
    //!                 the map produce a `"modes"`-less entry.
    //! \return         A `boost::json::object` of the form
    //!                 `{"nodes": [ {...}, ... ]}`.
    boost::json::object nodeListToJson(
        std::vector<NodeMapItem> const& nodes,
        std::unordered_map<NodeMapItem, std::set<AccessMode>> const& modeMap)
        const;                                                     // @0x1088d0

private:
    //! \brief Compilation configuration captured by the ctor (raw
    //! pointer; caller-owned).
    AWGCompilerConfig const* config_;                    // +0x000  //!< Compilation configuration captured by the ctor (raw pointer; caller-owned).
    //! \brief Per-device profile copied from
    //! `getDeviceConstants(config_->deviceType)`; consulted across
    //! the pipeline for limits and feature flags.
    DeviceConstants deviceConstants_;                     // +0x008 (0x90 bytes)  //!< Per-device profile copied from `getDeviceConstants(config_->deviceType)`; consulted across the pipeline for limits and feature flags.
    //! \brief Waveform registry constructed in the ctor; mutated
    //! by `addWaveforms` and read by the inner `Compiler` and
    //! `writeToStream`.
    std::shared_ptr<WavetableFront> wavetable_;          // +0x098  //!< Waveform registry constructed in the ctor; mutated by `addWaveforms` and read by the inner `Compiler` and `writeToStream`.
    //! \brief Optimised waveform IR moved out of
    //! `Compiler::compile`; consumed by `writeToStream` and
    //! `getJsonWaveformMemoryInfo`. Reset on each `compileString`.
    std::shared_ptr<WavetableIR> wavetableIR_;           // +0x0A8  //!< Optimised waveform IR moved out of `Compiler::compile`; consumed by `writeToStream` and `getJsonWaveformMemoryInfo`. Reset on each `compileString`.
    //! \brief Alignment padding ahead of the embedded `Compiler`.
    char pad_0B8_[8];                                    // +0x0B8  //!< Alignment padding ahead of the embedded `Compiler`.
    //! \brief Inner pipeline driver (lex / parse / lower / optimise
    //! / codegen); produces `vector<Assembler>` consumed by
    //! `assembler_`.
    Compiler compiler_;                                  // +0x0C0 (0x138 bytes)  //!< Inner pipeline driver (lex / parse / lower / optimise / codegen); produces `vector<Assembler>` consumed by `assembler_`.
    // Compiler ends at +0x1F8; next is +0x200
    //! \brief Alignment padding between `compiler_` and the string
    //! block.
    char pad_1F8_[8];                                    // +0x1F8  //!< Alignment padding between `compiler_` and the string block.
    //! \brief Path of the most recent `compileFile` source;
    //! surfaced in the assembler banner and the `.arguments` JSON.
    std::string sourceFilename_;                             // +0x200  //!< Path of the most recent `compileFile` source; surfaced in the assembler banner and the `.arguments` JSON.
    //! \brief 24-byte `std::string` slot at +0x218 default-
    //! constructed by the `AWGCompilerImpl` constructor and
    //! destructed at end of life. Never assigned, queried, or
    //! read by any reconstructed-or-binary code path. Retained
    //! for ABI fidelity.
    std::string pad_218_;                                // +0x218  //!< 24-byte `std::string` slot at +0x218 default-constructed and destructed; never read or written by any code path. Retained for ABI fidelity.
    //! \brief Cached SeqC source from the most recent
    //! `compileString`; emitted as the `.c` ELF section.
    std::string sourceText_;                             // +0x230  //!< Cached SeqC source from the most recent `compileString`; emitted as the `.c` ELF section.
    //! \brief Pretty-printed assembler listing produced by
    //! `compileString`; emitted as the `.asm` ELF section and
    //! consumed by `writeAssemblerToFile`.
    std::string assemblerText_;                             // +0x248  //!< Pretty-printed assembler listing produced by `compileString`; emitted as the `.asm` ELF section and consumed by `writeAssemblerToFile`.
    //! \brief Accumulated diagnostics from the inner `Compiler`
    //! and the limit checks; rendered by `getCompileReport`.
    std::vector<CompilerMessage> compileMessages_;       // +0x260  //!< Accumulated diagnostics from the inner `Compiler` and the limit checks; rendered by `getCompileReport`.
    //! \brief Embedded standalone assembler used for opcode
    //! emission (consuming the `Compiler`'s `vector<Assembler>`)
    //! and for post-pipeline `.asm` round-trips.
    AWGAssembler assembler_;                              // +0x278  //!< Embedded standalone assembler used for opcode emission (consuming the `Compiler`'s `vector<Assembler>`) and for post-pipeline `.asm` round-trips.
    //! \brief Persistent list of waveform-file paths registered
    //! via `addWaveforms`; surfaced in the `.arguments` JSON.
    std::vector<std::string> wavePaths_;                  // +0x280  //!< Persistent list of waveform-file paths registered via `addWaveforms`; surfaced in the `.arguments` JSON.
    //! \brief User cancel hook polled between `addWaveforms`
    //! entries and forwarded to `compiler_`.
    std::weak_ptr<CancelCallback> cancelCallback_;       // +0x298  //!< User cancel hook polled between `addWaveforms` entries and forwarded to `compiler_`.
    //! \brief User progress hook invoked with `1.0` when
    //! `compileString` finishes successfully and forwarded to
    //! `compiler_`.
    std::weak_ptr<ProgressCallback> progressCallback_;   // +0x2A8  //!< User progress hook invoked with `1.0` when `compileString` finishes successfully and forwarded to `compiler_`.
    //! \brief Trailing alignment padding to round the impl size to
    //! 0x2C0.
    char pad_2B8_[8];                                    // +0x2B8  //!< Trailing alignment padding to round the impl size to 0x2C0.

    // ------------------------------------------------------------------
    // T5b.5 — lifted cross-step local (recon-only; not part of binary
    // layout).  The original `compileString()` declares `asmList` as a
    // stack local that's populated inside the try-block by
    // `compiler_.compile()`, then consumed after the try by both the
    // asm-text pretty-printer (step 6) and the assembler
    // (`stepAssembleOpcodes`).  When `compileString()` is split into
    // step methods, this state must survive across the boundary —
    // hence it is lifted to a member.  Cleared at the start of
    // `stepInnerCompile` (analogous to `compileMessages_` /
    // `wavetableIR_` resets), so a fresh call starts clean.
    // ------------------------------------------------------------------
    std::vector<Assembler> compileString_asmList_;
};

// ============================================================================
// AWGCompilerImpl ctor @0x103b40
//
// 1. Store config pointer at +0x000
// 2. Call getDeviceConstants(config->deviceType) into +0x008
// 3. Allocate WavetableFront via shared_ptr_emplace (0x220 bytes)
//    - ctor args: deviceConstants, config->addressImpl, config->wavetableSize, config->searchPath
// 4. Store wavetable shared_ptr at +0x098/+0x0A0
// 5. Zero wavetableIR shared_ptr at +0x0A8
// 6. Construct Compiler at +0x0C0 with (config, deviceConstants, wavetable copy)
// 7. Zero 0x70 bytes at +0x200..+0x26F (4 strings + vector<CompilerMessage>)
// 8. Construct AWGAssembler at +0x278 with deviceConstants
// 9. Zero 0x30 bytes at +0x280..+0x2AF (output vector + weak_ptrs)
// 10. Zero pad at +0x2B0
// ============================================================================
//! \brief Construct the impl, look up `deviceConstants_`, allocate
//!        `wavetable_`, and default-construct every other owned
//!        subordinate.
AWGCompilerImpl::AWGCompilerImpl(AWGCompilerConfig const& config)  // @0x103b40
    : config_(&config),
      deviceConstants_(getDeviceConstants(config.deviceType)),            // @0x103ba4
      wavetable_(std::make_shared<WavetableFront>(                       // @0x103bdd: shared_ptr_emplace 0x220
          deviceConstants_, config.addressImpl,
          static_cast<size_t>(config.wavetableSize), config.searchPath)),
      wavetableIR_(),
      pad_0B8_{},
      compiler_(config, deviceConstants_, wavetable_),                   // @0x103c5f: Compiler ctor with shared_ptr copy
      pad_1F8_{},
      sourceFilename_(),
      pad_218_(),
      sourceText_(),
      assemblerText_(),
      compileMessages_(),
      assembler_(deviceConstants_),                                       // @0x103cc0
      wavePaths_(),
      cancelCallback_(),
      progressCallback_(),
      pad_2B8_{}
{
    // Binary ctor @0x103b40 sequence:
    // 1. config_ = &config                       (+0x000)
    // 2. getDeviceConstants(config.deviceType)    (+0x008, 0x90 bytes)
    // 3. make_shared<WavetableFront>(devConst, addressImpl, wavetableSize, searchPath)
    // 4. Compiler(config, deviceConstants_, wavetable_) at +0x0C0
    // 5. xorps zero 0x70 bytes at +0x200..+0x26F (4 strings + vector)
    // 6. AWGAssembler(deviceConstants_) at +0x278
    // 7. xorps zero 0x30 bytes at +0x280..+0x2AF (outputData_ + weak_ptrs)
    //
    // The member-initializer-list above reproduces this sequence.
    // Member init order follows declaration order which matches binary layout.
}

//! \brief Default destructor — releases every owned subordinate in
//!        reverse construction order.
AWGCompilerImpl::~AWGCompilerImpl() = default;  // @0x103400 — releases all members in reverse order

// ============================================================================
// AWGCompilerImpl::getBinVersion @0x10b830
//
// Binary flow:
//   1. Call getLaboneVersion() → CalVer      @0x100270
//   2. Call asBinary(calVer) → uint32_t      @0x1007c0
//   3. Allocate 4-byte buffer, store binary version
//   4. Switch on config_->deviceType to select a version suffix string:
//      - deviceType 2..32 → various static strings at rodata offsets
//      - deviceType 0x40  → another string
//      - deviceType 0x80  → another string
//      - deviceType 0x100 → another string
//   5. Combine labone binary version + device suffix into 8 bytes
//   6. Read config_->addressImpl (at config+0x10), pack into 16-byte result
//   7. Return as std::string (binary blob, not human-readable)
// ============================================================================
//! \brief Build the 16-byte binary version blob written to the
//!        `.version_bin` ELF section.
std::string AWGCompilerImpl::getBinVersion() const {  // @0x10b830
    // 1. Get LabOne version as binary u32
    uint32_t laboneVer = asBinary(getLaboneVersion());

    // 2. Device-type suffix — 4-byte string tag from rodata switch table
    // @0x10b89a: switch on config_->deviceType
    const char* suffix;
    switch (config_->deviceType) {
    case AwgDeviceType::HDAWG:                                     // 2 → "hirz" @0x8ff55b
        suffix = "hirz";
        break;
    case AwgDeviceType::SHFQA:                                     // 8
    case AwgDeviceType::SHFSG:                                     // 16
    case AwgDeviceType::SHFQC_SG:                                  // 32
    case AwgDeviceType::SHFLI:                                     // 64 → "grim" @0x8ff567
        suffix = "grim";
        break;
    case AwgDeviceType::GHFLI:                                     // 128 → "gurn" @0x8ff574
        suffix = "gurn";
        break;
    case AwgDeviceType::VHFLI:                                     // 256 → "malo" @0x8ff582
        suffix = "malo";
        break;
    default:                                                        // UHFLI(1), UHFQA(4), etc. → "cerv" @0x8ff54e
        suffix = "cerv";
        break;
    }

    // 3. Build binary blob: [laboneVer:4][suffix:4][waveformRegBase:4][zero:4]
    // @0x10b8f0..0x10b920: string append sequence growing 4→8→16 bytes
    // [rbp-0x30]+0x10 = this+0x10 = deviceConstants_.waveformRegBase
    uint32_t regBase = deviceConstants_.waveformRegBase;  // this+0x10 @0x10b934
    std::string result;
    result.append(reinterpret_cast<char const*>(&laboneVer), 4);
    result.append(suffix, 4);
    result.append(reinterpret_cast<char const*>(&regBase), 4);
    result.append(4, '\0');  // zero-pad to 16 bytes

    return result;
}

// ============================================================================
// AWGCompilerImpl::getJsonWaveformMemoryInfo @0x10a1b0
//
// Binary flow:
//   1. Check wavetableIR_ (at +0xA8) — if null, return empty JSON object
//   2. Call wavetableIR_->forEachUsedWaveform(lambda, WaveOrder::ByIndex)
//      The lambda accumulates:
//        - usedSlots (set<size_t>) — tracks which waveform indices are used
//        - hasPlayback (bool) — OR'd with each waveform's playback flag
//        - totalSamples (double pair) — sum of waveform sample counts
//   3. Read deviceConstants_.waveformAlignment (at +0x14 of devConst = this+0x14)
//   4. Construct boost::json::object with:
//        ["has_playback"] = bool(hasPlayback)
//        ["usage_fraction"] = totalSamples / waveformAlignment (as double)
//   5. Return the JSON value as string
// ============================================================================
//! \brief Aggregate per-waveform memory usage and serialise it as
//!        the JSON object surfaced through `getJsonWaveformMemoryInfo`.
std::string AWGCompilerImpl::getJsonWaveformMemoryInfo() const {  // @0x10a1b0
    // If no wavetableIR, return empty JSON object "{}"
    if (!wavetableIR_) {
        boost::json::object obj;
        return boost::json::serialize(obj);
    }

    // Accumulate waveform usage info via lambda @0x10a225
    //
    // The binary's lambda is much more complex than a simple sum of
    // allocationByteSize.  It maintains a std::set of aligned base addresses
    // and caps each waveform's contribution at
    //   waveformAlignment * (isHirzel ? maxBlocks : cachePageCount).
    //
    // Pseudocode from disassembly at 0x1190c0..0x1191e9:
    //   1. Skip waveforms with allocationByteSize == 0.
    //   2. Compute alignedAddr = addressValue - (addressValue % alignment).
    //   3. If crossesCacheLine_: insert alignedAddr into set, then accumulate.
    //      Else: search set for alignedAddr; if found accumulate, else set exceedsFpga.
    //   4. Accumulate: totalBytes += min(allocationByteSize,
    //                                    alignment * multiplier)
    //      where multiplier = isHirzel ? maxBlocks : cachePageCount.
    bool exceedsFpga = false;
    uint64_t totalBytes = 0;
    std::set<uint64_t> alignedAddresses;                          // tree at rbp-0x50

    uint32_t alignment = deviceConstants_.waveformAlignment;      // DC+0x14 @0x1190e4
    uint8_t cacheType = config_->cacheType;                        // config+0x19 @0x119198
    uint32_t multiplier = cacheType
        ? deviceConstants_.maxBlocks                              // DC+0x1C
        : deviceConstants_.cachePageCount;                        // DC+0x18
    uint32_t maxContribution = alignment * multiplier;            // @0x11919f

    wavetableIR_->forEachUsedWaveform(
        [&](std::shared_ptr<WaveformIR> const& wf) {             // @0x10a225 lambda
            if (wf->allocationByteSize == 0)                      // 0x1190d8: test ecx
                return;

            uint32_t addr = static_cast<uint32_t>(wf->addressValue);  // +0x4C
            uint32_t alignedAddr = addr - (addr % alignment);         // 0x1190f0: div+sub

            if (wf->crossesCacheLine_) {                              // 0x119101: cmpb 0xDA
                // Insert into set, then always accumulate
                alignedAddresses.insert(alignedAddr);                 // 0x119144..0x119182
                uint32_t contrib = std::min(
                    static_cast<uint32_t>(wf->allocationByteSize),
                    maxContribution);                                  // 0x1191a5: cmovae
                totalBytes += contrib;                                 // 0x1191b0: add
            } else {
                // Search set; if found accumulate, else set exceedsFpga
                auto it = alignedAddresses.find(alignedAddr);         // 0x1191b5..0x1191d7
                if (it != alignedAddresses.end()) {                   // 0x1191e0: jbe
                    uint32_t contrib = std::min(
                        static_cast<uint32_t>(wf->allocationByteSize),
                        maxContribution);
                    totalBytes += contrib;
                } else {
                    exceedsFpga = true;                               // 0x1191e2
                }
            }
        },
        WaveOrder::ByIndex);

    uint32_t memSize = deviceConstants_.waveformMemorySize;  // this+0x14 = DC+0x0C @0x10a287
    if (memSize > 0 && totalBytes > memSize) {
        exceedsFpga = true;
    }

    double usage = (memSize > 0)
        ? static_cast<double>(totalBytes) / static_cast<double>(memSize)
        : 0.0;

    // Build JSON result                                             // @0x10a2a6
    boost::json::object result;
    result["exceedsFpgaMemory"] = exceedsFpga;                      // @0x10a2a6
    result["fpgaMemoryUsed"] = usage;                               // @0x10a2df

    return boost::json::serialize(result);
}

// ============================================================================
// AWGCompilerImpl::compileString @0x106cb0
//
// Binary flow (~6KB, 0x106cb0..0x107d10):
//   1. Load config_ (at +0x000), check config->deviceType validity:
//      - If config->isHirzel (+0x19 of config via +0x18 cmp) is true AND
//        this->deviceConstants_ byte at +0x0C (relative to this) is 0:
//        → throw ZIAWGCompilerException with ErrorMessages::format(0xDA, deviceTypeString)
//      - If config->isHirzel is false AND that byte is 0:
//        → throw ZIAWGCompilerException with ErrorMessages::format(0xDB, deviceTypeString)
//   2. Copy source into sourceText_ (at this+0x230)
//   3. Clear compileMessages_ vector (destroy old entries, reset end pointer)
//   4. Reset wavetableIR_ shared_ptr (release old)
//   5. Call compiler_.compile(source) @0x11f150
//      Returns vector<Assembler> (sret)
//   6. Move result into local asmList, move wavetableIR from compiler
//   7. Create ostringstream, iterate asmList entries:
//      - For each Assembler entry with type != -1:
//        - If type == 2 (error): compute indent, write indented assembly text
//        - Otherwise: write 8-space indent + assembly text + "\n"
//   8. Extract ostringstream content → store into assemblerText_ (at +0x248)
//   9. Get compile messages from compiler, append to compileMessages_
//  10. Call assembler_.assembleAsmList(asmList) @0x2850f0
//  11. Get opcodes, check size vs deviceConstants_.maxSequenceLen (+0x60)
//      - If too large: add error message, throw ZIAWGCompilerException
//  12. Iterate wavetableIR waveforms, count total used waveforms
//      - If count exceeds deviceConstants_.maxProgramSize (+0x68):
//        add error message, throw ZIAWGCompilerException
//  13. Lock progressCallback weak_ptr, call with 1.0 (100% done)
//  14. Clean up temporaries
//
// Exception handler (@0x10791b):
//   On exception from compiler_.compile():
//   - catch block: get compile messages, append to compileMessages_
//   - Re-throw as ZIAWGCompilerException with the caught exception's message
//     (or empty string if what() is null)
// ============================================================================
//! \brief Drive the full SeqC → opcode pipeline (validate device,
//!        compile, pretty-print, assemble, enforce limits, signal
//!        progress).
//!
//! T5b.5: this is now a 3-line dispatcher calling the named step
//! methods in sequence.  The driver-side `SeqcDriver::compile()`
//! also calls these steps individually (through public forwarders
//! on `AWGCompiler`) so it can short-circuit between them based on
//! `--to=<stage>`.  Behaviour preserved by construction — the
//! steps execute the same blocks in the same order as the original
//! monolithic body.
void AWGCompilerImpl::compileString(std::string const& source) {  // @0x106cb0
    stepInnerCompile(source);
    stepAssembleOpcodes();
    stepCheckLimits();
}

// ----------------------------------------------------------------------------
// T5b.5 step methods (verbatim partition of the pre-T5b.5 compileString
// body).  See AWGCompilerImpl class declaration for the public-facing
// brief on each step.
// ----------------------------------------------------------------------------

void AWGCompilerImpl::stepInnerCompile(std::string const& source) {  // @0x106cb0..0x107341
    // 1. Validate device type
    bool isHirzel = config_->isHirzel;                 // config+0x18
    bool hasDeviceConstants = (deviceConstants_.deviceType != kDevNone); // simplified check

    if (isHirzel && !hasDeviceConstants) {
        // @0x106cdb: throw for Hirzel device with unsupported type
        std::string devStr = AWGCompilerConfig::getAwgDeviceTypeString(
            static_cast<AwgDeviceType>(config_->deviceType));
        throw ZIAWGCompilerException(
            ErrorMessages::format(ErrorMessageT(0xDA), devStr));
    }
    if (!isHirzel && !hasDeviceConstants) {
        // @0x107730: throw for non-Hirzel device with unsupported type
        std::string devStr = AWGCompilerConfig::getAwgDeviceTypeString(
            static_cast<AwgDeviceType>(config_->deviceType));
        throw ZIAWGCompilerException(
            ErrorMessages::format(ErrorMessageT(0xDB), devStr));
    }

    // 2. Store source into sourceText_
    sourceText_ = source;

    // 3. Clear compileMessages_
    compileMessages_.clear();

    // 4. Reset wavetableIR_
    wavetableIR_.reset();

    // T5b.5: clear lifted asmList for the next stepAssembleOpcodes call
    compileString_asmList_.clear();

    // 5. Compile
    try {
        auto compileResult = compiler_.compile(source);   // @0x11f150
        compileString_asmList_ = std::move(compileResult.asmList);
        wavetableIR_ = std::move(compileResult.wavetableIR);  // @0x106ebc: store sret+0x18 into this+0xA8
    } catch (...) {
        // @0x10791b: exception handler
        // Get compile messages from compiler and append
        auto msgs = compiler_.getCompileMessages();
        compileMessages_.insert(compileMessages_.end(), msgs.begin(), msgs.end());

        // Re-throw as ZIAWGCompilerException
        try {
            std::rethrow_exception(std::current_exception());
        } catch (std::exception const& e) {
            const char* what = e.what();
            throw ZIAWGCompilerException(
                what ? std::string(what) : std::string());
        }
    }

    // 6. Build assembly text from asmList into ostringstream
    std::ostringstream oss;
    bool afterLabel = false;
    for (auto const& instr : compileString_asmList_) {
        if (instr.cmd == Assembler::INVALID) continue;  // skip invalid entries

        // Write assembly text representation
        std::string s = instr.str(/*verbose=*/true);
        if (instr.cmd == Assembler::LABEL) {
            // Label: right-pad to 8 chars, no newline — next instr continues on same line
            int indent = 8 - static_cast<int>(s.size());
            if (indent < 0) indent = 0;
            oss << s << std::string(indent, ' ');
            afterLabel = true;
        } else {
            if (!afterLabel) {
                oss << "        ";                         // 8 spaces indent
            }
            oss << s << "\n";
            afterLabel = false;
        }
    }

    // 7. Store assembler text
    assemblerText_ = oss.str();

    // 8. Get compile messages from compiler, append to ours
    {
        auto msgs = compiler_.getCompileMessages();
        compileMessages_.insert(compileMessages_.end(), msgs.begin(), msgs.end());
    }
}

void AWGCompilerImpl::stepAssembleOpcodes() {  // @0x107341 (entry to assembleAsmList block)
    // 9. Assemble the ASM list
    assembler_.assembleAsmList(compileString_asmList_);
}

void AWGCompilerImpl::stepCheckLimits() {  // @0x10739e..0x107478
    // 10. Check opcode size vs hardware instruction-memory depth.
    //
    // Binary check at AWGCompilerImpl::compileString @0x107341..0x10739e:
    //   r15 = (getOpcode().end() - getOpcode().begin()) >> 2  (== vec.size() for vector<uint32_t>)
    //   limit = *(uint64_t*)(this + 0x60)  == DeviceConstants[+0x58] == maxProgramSize
    //   if (r15 > limit) → format(ErrorMessageT(0xC), count, limit), throw.
    //
    // The DC field at +0x58 is currently named `maxProgramSize` but its
    // actual semantics is "max opcode words" / instruction-memory depth.
    // Values: UHFAWG=1024, UHFQA=1024, HDAWG=16384, SHF*=32768.  (See IF-195
    // for the field-rename follow-up.)
    //
    // Pre-IF-192 had two compounding bugs here: wrong DC field
    // (`maxSequenceLen` = 16000) AND a bogus `/4` (the opcode vector is
    // already vector<uint32_t>).  Combined, the effective UHFQA limit was
    // 64000 instead of 1024, so 2106-instruction programs compiled
    // silently.  Both fixed here.
    auto const& opcodes = assembler_.getOpcode();
    size_t opcodeCount = opcodes.size();                    // already in words
    size_t maxOpcodes  = deviceConstants_.maxProgramSize;  // devConst+0x58

    if (opcodeCount > maxOpcodes) {  // @0x10739e
        CompilerMessage msg;
        msg.type = CompilerMessage::Error;
        msg.lineNr = -1;
        msg.message = ErrorMessages::format(ErrorMessageT(0x0C),
            static_cast<uint64_t>(opcodeCount),
            static_cast<uint64_t>(maxOpcodes));
        compileMessages_.push_back(std::move(msg));
        throw ZIAWGCompilerException("Compiler error while generating assembly");
    }

    // 11. Check waveform memory usage (if wavetableIR_ exists)
    if (wavetableIR_) {
        size_t nonNullWaveformCount = 0;
        for (auto it = wavetableIR_->begin(); it != wavetableIR_->end(); ++it) {
            auto const& wf = *it;
            auto const* ptr = wf.get();
            if (ptr) {
                // Check if waveform is a "playback" type and count it
                nonNullWaveformCount++;
            }
        }

        // Binary loads from [this+0x68] = [deviceConstants_+0x60] = maxSequenceLen
        // (IF-196 confirmed: pre-fix this read maxProgramSize at DC+0x58, the same
        // field check 10 was also wrongly reading pre-IF-192 — i.e. the two checks
        // had their DC fields swapped in the original recon.  Correct semantics:
        // this is a wavetable-entry-count limit, which is `maxSequenceLen` (=16000
        // universally), NOT the opcode-memory limit.)
        size_t maxWaveforms = deviceConstants_.maxSequenceLen;   // devConst+0x60
        if (nonNullWaveformCount > maxWaveforms) {
            CompilerMessage msg;
            msg.type = CompilerMessage::Error;
            msg.message = ErrorMessages::format(ErrorMessageT(0xF1),
                nonNullWaveformCount, maxWaveforms);
            compileMessages_.push_back(std::move(msg));
            throw ZIAWGCompilerException("Waveform memory exceeded");
        }
    }

    // 12. Signal completion via progress callback
    // @0x10744e: lock weak_ptr at +0x2A8, call setProgress(1.0)
    if (auto progress = progressCallback_.lock()) {                            // @0x10744e
        progress->setProgress(1.0);                                            // @0x107468
    }
}

// ============================================================================
// AWGCompilerImpl::compileFile @0x106690
//
// Binary flow (~1.5KB):
//   1. Copy path string into local
//   2. Call boost::filesystem::status(path) — if type <= 1 (not_found/unknown):
//      throw ZIAWGCompilerException
//   3. Open boost::filesystem::basic_ifstream with path
//   4. Create ostringstream, stream file contents: oss << filebuf.rdbuf()
//   5. Close file
//   6. Store path into sourceFilename_ (at this+0x200) — the source filename
//   7. Extract ostringstream string → call compileString(str)  @0x106cb0
//   8. Clean up streams
// ============================================================================
//! \brief Slurp a SeqC source file from disk and forward to
//!        `compileString` after recording the path in
//!        `sourceFilename_`.
void AWGCompilerImpl::compileFile(std::string const& path) {  // @0x106690
    // 1-2. Check file exists
    boost::filesystem::path fpath(path);
    auto st = boost::filesystem::status(fpath);
    if (st.type() <= boost::filesystem::file_not_found) {
        // File doesn't exist — the binary throws here
        // (actual error handling delegates to compileString which checks further)
        throw ZIAWGCompilerException("File not found: " + path);
    }

    // 3-4. Read file contents
    std::ifstream ifs(fpath.string(), std::ios::in);
    std::ostringstream oss;
    oss << ifs.rdbuf();
    ifs.close();

    // 5. Store filename
    sourceFilename_ = path;

    // 6. Compile the string content
    std::string contents = oss.str();
    compileString(contents);
}

// ============================================================================
// AWGCompilerImpl::addWaveforms @0x104660
//
// Binary flow (~8KB, 0x104660..0x106690):
//   1. Append paths to outputData_ vector (at +0x280) — actually this is
//      inserting into the string vector at +0x280. Wait — re-reading: the
//      binary calls vector<string>::insert at +0x280. So outputData_ is
//      actually vector<string> not vector<uint8_t>.
//      Actually: at 0x104681: rdi = this+0x280, calls vector<string>::insert
//      with the input paths. So +0x280 is a vector<string>.
//   2. For each path in the input vector:
//      a. Check cancel callback — if cancelled, break
//      b. Copy path string, extract stem (boost::filesystem::path::stem)
//         and extension (boost::filesystem::path::extension)
//      c. Match extension (length 4-7 chars):
//         - ".csv" or ".txt": call wavetable_->newWaveformFromFile(stem, path, Type=0)
//           then allocate_shared<WaveformIR>(waveformFront)
//         - ".bin": open binary ifstream, read raw data
//         - ".bin16": similar binary read with 16-bit format
//         - ".wave16": similar
//         - ".wave": similar
//         - Unrecognized: log warning, skip
//      d. For ".csv"/".txt": create WaveformIR from WaveformFront
//      e. For binary formats: read file, create waveform data directly
//   3. After processing all paths:
//      - Lock cancel callback, check if cancelled
//      - (Various cleanup of temporary shared_ptrs)
// ============================================================================
//! \brief Append the supplied paths to `wavePaths_`, then dispatch
//!        on file extension to register each waveform with
//!        `wavetable_`.
void AWGCompilerImpl::addWaveforms(std::vector<std::string> const& paths) {  // @0x104660
    // 1. Append paths to internal wave path list
    // @0x104681: vector<string>::insert at +0x280
    wavePaths_.insert(wavePaths_.end(), paths.begin(), paths.end());

    // 2. Process each waveform file
    for (auto const& pathStr : paths) {
        // 2a. Check cancel callback                                            // @0x1046e0
        if (auto cancel = cancelCallback_.lock()) {
            if (cancel->isCancelled()) {
                break;  // @0x104700: exits loop if cancelled
            }
        }

        boost::filesystem::path fpath(pathStr);
        std::string stem = fpath.stem().string();       // @0x1047b4: stem_v3
        std::string ext = fpath.extension().string();    // @0x104830: extension_v3

        // 2c. Match extension
        if (ext.size() >= 4 && ext.size() <= 7) {
            if (ext == ".csv" || ext == ".txt") {
                // @0x104b17: wavetable_->newWaveformFromFile(stem, pathStr, Type=0)
                wavetable_->newWaveformFromFile(stem, pathStr,
                    Waveform::File::Type(0));
                // Then create WaveformIR from front — allocate_shared<WaveformIR>
                // @0x104b44: allocate_shared<WaveformIR>(waveformFront)
            } else if (ext == ".bin" || ext == ".wave") {
                // @0x104a4e: Binary format — open file, read raw uint16 pairs,
                // convert via awg2double/awg2marker, construct Signal
                std::ifstream ifs;
                ifs.open(fpath.string(), std::ios::in | std::ios::binary | std::ios::ate);  // mode=0xe @0x104c21
                if (!ifs) {
                    throw ZIAWGCompilerException(
                        ErrorMessages::format(ErrorMessageT(0xe3), pathStr));
                }

                auto fileSize = static_cast<int32_t>(ifs.tellg());  // @0x104c5c

                std::vector<char> rawBuf;
                if (fileSize >= 2) {
                    rawBuf.resize(static_cast<size_t>(fileSize), 0);
                }
                ifs.seekg(0);                                        // @0x104cff
                ifs.read(rawBuf.data(), fileSize);                   // @0x104d15
                ifs.close();

                // Convert uint16 words → samples + markers
                std::vector<double> samples;
                std::vector<uint8_t> markers;
                uint8_t allMarkerBits = 0;

                uint16_t const* p = reinterpret_cast<uint16_t const*>(rawBuf.data());
                uint16_t const* end = reinterpret_cast<uint16_t const*>(
                    rawBuf.data() + rawBuf.size());
                for (; p < end; ++p) {                               // @0x104e20..0x104fa0
                    samples.push_back(awg2double(*p));               // @0x2996d0
                    uint8_t m = awg2marker(*p);                      // @0x2996f0
                    markers.push_back(m);
                    allMarkerBits |= m;
                }

                // Build Signal from converted data
                MarkerBitsPerChannel markerBits({allMarkerBits});    // @0x105060
                Signal signal(samples, markers, markerBits);         // @0x106340 via 0x105089

                // Create waveform via WavetableFront
                wavetable_->newWaveformFromFile(
                    stem, signal, pathStr,
                    Waveform::File::Type(1));                        // @0x29b520 via 0x1050b3
            } else if (ext == ".bin16" || ext == ".wave16") {
                // @0x104991: HDAWG 32-bit sample format
                // Each sample is a uint32_t: bits[31:2] = 16-bit signed sample
                // (after >>2 and int16_t truncation), bits[1:0] = marker bits.
                std::ifstream ifs;
                ifs.open(fpath.string(), std::ios::in | std::ios::binary | std::ios::ate);
                if (!ifs) {
                    throw ZIAWGCompilerException(
                        ErrorMessages::format(ErrorMessageT(0xe3), pathStr));
                }

                auto fileSize = static_cast<int32_t>(ifs.tellg());
                std::vector<char> rawBuf;
                if (fileSize >= 4) {
                    rawBuf.resize(static_cast<size_t>(fileSize), 0);
                }
                ifs.seekg(0);
                ifs.read(rawBuf.data(), fileSize);
                ifs.close();

                std::vector<double> samples;
                std::vector<uint8_t> markers;
                uint8_t allMarkerBits = 0;

                uint32_t const* p = reinterpret_cast<uint32_t const*>(rawBuf.data());
                uint32_t const* end = reinterpret_cast<uint32_t const*>(
                    rawBuf.data() + rawBuf.size());
                for (; p < end; ++p) {
                    samples.push_back(awg2double16(*p));
                    uint8_t m = static_cast<uint8_t>(*p & 0x3);
                    markers.push_back(m);
                    allMarkerBits |= m;
                }

                MarkerBitsPerChannel markerBits({allMarkerBits});
                Signal signal(samples, markers, markerBits);

                wavetable_->newWaveformFromFile(
                    stem, signal, pathStr,
                    Waveform::File::Type(1));
            } else {
                // Unrecognized extension — log warning and skip
                // @0x1049c0: LogRecord dtor (log message about skipping)
            }
        }
        // else: extension too short/long — skip (falls through to cleanup)
    }

    // 3. Post-processing: check cancel, clean up
    if (auto cancel = cancelCallback_.lock()) {                                // @0x105470
        if (cancel->isCancelled()) {
            return;                                                            // @0x105490
        }
    }
}

// ============================================================================
// AWGCompilerImpl::nodeListToJson @0x1088d0
//
// Builds the `.nodes_json` body for non-UHF devices.  Iterates the
// supplied node-access list, calls NodeMapItem::toJson() (@0x1c54f0)
// for each node, then looks the node up in the mode-map.  When found,
// attaches a "modes" array whose entries are toString(AccessMode) for
// each AccessMode in the (already-sorted) set value.
//
// Binary structurally builds a temporary local `std::set<AccessMode>`
// from the unordered_map's value-set (@0x108992 set::insert(begin,end))
// and iterates that local copy when emitting "modes".  Since the value
// is *already* a sorted set<AccessMode>, this intermediate copy is a
// behavioural no-op and is omitted here — the emitted JSON byte
// sequence is identical.  See incidental_findings.md (IF entry) for
// the structural divergence note.
// ============================================================================
boost::json::object AWGCompilerImpl::nodeListToJson(
    std::vector<NodeMapItem> const& nodes,
    std::unordered_map<NodeMapItem, std::set<AccessMode>> const& modeMap)
    const {                                                       // @0x1088d0
    boost::json::object root;
    boost::json::array items;
    for (auto const& node : nodes) {
        boost::json::object entry = node.toJson().as_object();    // @0x1c54f0
        auto it = modeMap.find(node);                              // @0x1153f0
        if (it != modeMap.end()) {
            boost::json::array modesArr;
            for (auto m : it->second) {
                modesArr.push_back(boost::json::string(toString(m)));
            }
            entry["modes"] = std::move(modesArr);
        }
        items.emplace_back(std::move(entry));
    }
    root["nodes"] = std::move(items);
    return root;
}

// ============================================================================
// AWGCompilerImpl::writeToStream @0x108cc0
//
// Binary flow (~5KB, 0x108cc0..0x10a1b0):
//   1. Check hadSyntaxError via SeqcParserContext at this+0x1C0
//      (= compiler_.parserContext_ at Compiler+0x100)
//      If true, return immediately (no output)                    @0x108ce4
//   2. Get opcodes from assembler_ — if empty, return             @0x108d05
//   3. Construct ElfWriter(machineType=2)                         @0x108d1f
//   4. Set memory offset from config_->addressImpl                @0x108d30
//   5. Check config_->optimizationFlags (bool at config+0x88):
//      - If true (mapped mode):
//        Call writeWavesToElfMapped via lambda + forEachUsedWaveform @0x108db1
//      - If false (absolute mode):
//        Get firstWaveformOffset, call writeWavesToElfAbsolute     @0x108e50
//   6. Get nextSegmentAddress from wavetableIR                    @0x108eae
//   7. Call elfWriter.addCode(opcodes)                            @0x108ec6
//   8. Extract filename from format path, add as ".filename" data @0x108f11
//   9. If config has source embedding flag (config+0x9D is 1):
//      - Compress sourceText_ → add as ".c" section               @0x108f7d
//      - Compress assemblerText_ → add as ".asm" section             @0x108ff7
//      Else:
//      - Add raw sourceText_ as ".c" section                      @0x1090a0
//      - Add raw assemblerText_ as ".asm" section                    @0x1090fa
//  10. Get lineMap from compiler, add as ".linenr" section        @0x109166
//  11. Check deviceType == 4 or 1:
//      - Add node access list as ".nodelist" section
//      - Add node-to-mode map as serialized data
//  12. Add various additional metadata sections (version, args, etc.)
//  13. Call elfWriter.writeFile(os)                               @0x109930
//  14. Clean up ElfWriter
// ============================================================================
//! \brief Serialise the most recent compile (opcode stream,
//!        waveform data, and metadata sections) into a 32-bit ELF
//!        on `os`.
void AWGCompilerImpl::writeToStream(std::ostream& os, std::string const& format) {  // @0x108cc0
    // 1. Check for syntax errors — parserContext at Compiler+0x100, byte +0x03
    // @0x108ce4: if hadSyntaxError, return immediately
    if (compiler_.hadSyntaxError()) {
        return;
    }

    // 2. Get opcodes — if empty, raise the EmptyInput error.
    //    Binary: @0x108d0b cmp begin/end of opcodes vector;
    //    @0x108d0f je 0x109a00 → __cxa_allocate_exception(0x60) →
    //    throws ErrorMessageT::EmptyInput (id 43).  Earlier reconstruction
    //    silently `return`-ed here, which is incorrect.
    auto const& opcodes = assembler_.getOpcode();
    if (opcodes.empty()) {
        throw ZIAWGCompilerException(
            ErrorMessages::format(EmptyInput));                    // @0x109a00..0x109a14
    }

    // 3. Create ELF writer with machine type 2
    ElfWriter elfWriter(2);                                        // @0x108d1f

    // 4. Set memory offset from config
    elfWriter.setMemoryOffset(config_->addressImpl);               // @0x108d30

    // 5. Write waveform data to ELF
    if (wavetableIR_) {
        // The binary checks AWGCompilerImpl+0x88 which is deviceConstants_.hasPrecomp
        // @0x108d4e: cmpb $0x1, 0x88(%rax) where rax = this (NOT config)
        // hasPrecomp == 1 → mapped mode; else → absolute mode
        bool mappedMode = deviceConstants_.hasPrecomp;
        if (mappedMode) {
            // Mapped mode: padSize=0, ByWaveIndex order                  @0x108db1
            wavetableIR_->forEachUsedWaveform(
                [&](std::shared_ptr<WaveformIR> const& wf) {
                    elfWriter.addWaveform(wf, SampleFormat(config_->sampleFormat),
                        /*mapped=*/true, /*padSize=*/0);
                },
                WaveOrder::ByWaveIndex);
        } else {
            // Absolute mode: compute padding, ByIndex order             @0x108e50
            uint32_t currentOffset = wavetableIR_->getFirstWaveformOffset();
            wavetableIR_->forEachUsedWaveform(
                [&](std::shared_ptr<WaveformIR> const& wf) {
                    WaveformIR* wfPtr = wf.get();
                    if (!wfPtr->used) return;
                    // 0x10e1aa: cmpq $0x0, 0xd0(%rax) — checks Signal+0x50 = length_
                    // NOT signal.data() (samples_). Placeholder waveforms have empty
                    // samples_ but non-zero length_ — must not be skipped.
                    if (wfPtr->signal.length() == 0) return;
                    uint32_t gap = wfPtr->addressValue - currentOffset;
                    uint32_t alignMask = static_cast<uint32_t>(-wfPtr->elfAlignment_);
                    uint32_t padding = gap & alignMask;
                    auto rawData = elfWriter.addWaveform(wf, SampleFormat(config_->sampleFormat),
                        /*mapped=*/false, padding);
                    currentOffset = wfPtr->addressValue + static_cast<uint32_t>(rawData->size());
                },
                WaveOrder::ByIndex);
        }
    }

    // 6. Update memory offset to account for waveform data          @0x108eae
    if (wavetableIR_) {
        elfWriter.setMemoryOffset(wavetableIR_->getNextSegmentAddress());
    }

    // 7. Add code section
    elfWriter.addCode(opcodes);                                    // @0x108ec6

    // 8. Add filename section
    {
        boost::filesystem::path fmtPath(format);
        std::string filename = fmtPath.filename().string();
        std::string sectionName = ".filename";                     // @0x108f1a: ".filenam" + "e"
        elfWriter.addData(filename.data(), filename.size(), sectionName);
    }

    // 9. Add source code sections
    // @0x108f48: check config->compressSource for source embedding compression flag
    bool compressSource = config_->compressSource;
    if (compressSource) {
        // @0x108f7d: compress sourceText_ → ".c"
        std::string compC = compressSourceString(sourceText_, format);
        elfWriter.addData(compC.data(), compC.size(), std::string(".c"));
        // @0x108ff7: compress assemblerText_ → ".asm"
        std::string compAsm = compressSourceString(assemblerText_, format);
        elfWriter.addData(compAsm.data(), compAsm.size(), std::string(".asm"));
    } else {
        // @0x1090a0: raw ".c"
        elfWriter.addData(sourceText_.data(), sourceText_.size(),
            std::string(".c"));
        // @0x1090fa: raw ".asm"
        elfWriter.addData(assemblerText_.data(), assemblerText_.size(),
            std::string(".asm"));
    }

    // 10. Add line number map
    {
        auto lineMap = compiler_.getLineMap(0);                    // @0x109166
        std::string sectionName = ".linenr";
        elfWriter.addData(reinterpret_cast<char const*>(lineMap.data()),
            lineMap.size() * sizeof(int), sectionName);
    }

    // 11-12. Additional metadata sections
    // @0x1091d1: switch on deviceType for node data format
    {
        int devType = config_->deviceType;
        if (devType == AwgDeviceType::UHFQA || devType == AwgDeviceType::UHFLI) {
            // OLD PATH: pack node addresses as int32 pairs → ".nodes"
            auto const* nodeList = compiler_.getNodeAccessList();
            std::vector<int32_t> packed;
            if (nodeList) {
                for (auto const& item : *nodeList) {
                    // Each NodeMapItem: extract address + size via toJson/size
                    // For DirectAddrNodeMapData items, pack (address, size) pairs
                    auto* direct = dynamic_cast<DirectAddrNodeMapData*>(item.data);
                    int32_t addr = direct ? static_cast<int32_t>(direct->addr_) : 0;
                    packed.push_back(addr);
                    packed.push_back(static_cast<int32_t>(item.size()));
                }
            }
            // Always add .nodes section, even if empty (binary 0x1091d1
            // unconditionally calls addData for the old path)
            elfWriter.addData(
                reinterpret_cast<char const*>(packed.data()),
                packed.size() * sizeof(int32_t),
                std::string(".nodes"));
        } else {
            // NEW PATH: nodeListToJson → serialize → ".nodes_json"
            auto const* nodeList = compiler_.getNodeAccessList();
            auto const* modeMap = compiler_.getNodeToModeMap();
            if (nodeList && modeMap) {
                boost::json::object root = nodeListToJson(*nodeList, *modeMap);  // @0x1088d0
                std::string json = boost::json::serialize(root);
                elfWriter.addData(json.data(), json.size(),
                    std::string(".nodes_json"));
            }
        }
    }

    // ".channels" — compiler channel info
    {
        auto chanInfo = compiler_.getChannelInfo();                 // @0x109590
        if (!chanInfo.empty()) {
            elfWriter.addData(
                reinterpret_cast<char const*>(chanInfo.data()),
                chanInfo.size() * sizeof(int),
                std::string(".channels"));
        }
    }

    // ".required_sample_rate" — conditional on usedDeviceSampleRate + non-NaN
    {
        double sr = config_->deviceSampleRate;
        if (!std::isnan(sr) && compiler_.usedDeviceSampleRate()) {
            float srf = static_cast<float>(sr);
            elfWriter.addData(
                reinterpret_cast<char const*>(&srf), sizeof(srf),
                std::string(".required_sample_rate"));
        }
    }

    // ".waveforms" — wavetableIR JSON index
    if (wavetableIR_) {
        std::string wfJson = wavetableIR_->getJsonIndex(config_->sampleFormat);
        elfWriter.addData(wfJson.data(), wfJson.size(),
            std::string(".waveforms"));
    }

    // ".wavemem" — waveform memory info JSON
    {
        std::string wmJson = getJsonWaveformMemoryInfo();
        elfWriter.addData(wmJson.data(), wmJson.size(),
            std::string(".wavemem"));
    }

    // ".arguments" — JSON arguments @0x10a3c0
    {
        boost::filesystem::path fmtPath2(format);
        std::string filename2 = fmtPath2.filename().string();
        std::string argsJson = getJsonArguments(filename2);
        elfWriter.addData(argsJson.data(), argsJson.size(),
            std::string(".arguments"));
    }

    // ".version_json" — JSON version info @0x10ac60
    {
        std::string verJson = getJsonVersion();
        elfWriter.addData(verJson.data(), verJson.size(),
            std::string(".version_json"));
    }

    // ".version_bin" — binary version blob (16 bytes)
    {
        std::string binVer = getBinVersion();
        elfWriter.addData(binVer.data(), binVer.size(),
            std::string(".version_bin"));
    }

    // 13. Write ELF to output stream
    elfWriter.writeFile(os);                                       // @0x109930

    // 14. ElfWriter dtor handles cleanup
}

// ============================================================================
// AWGCompilerImpl::writeToFile @0x10b9b0
//
// Binary flow (~0x650 bytes):
//   1. Copy path into local string
//   2. Construct boost::filesystem::basic_ofstream with path, mode=binary|trunc (0x14)
//   3. Call writeToStream(ofstream, path)                         @0x10ba79
//   4. Destroy ofstream (close + dtor chain)
// ============================================================================
//! \brief Open `path` in `binary | trunc` mode and forward to
//!        `writeToStream`.
void AWGCompilerImpl::writeToFile(std::string const& path) {  // @0x10b9b0
    // 1-2. Open output file
    std::ofstream ofs(
        path,
        std::ios::binary | std::ios::trunc);                       // mode = 0x14

    // 3. Delegate to writeToStream
    writeToStream(ofs, path);

    // 4. ofstream dtor closes file
}

// ============================================================================
// AWGCompilerImpl::writeAssemblerToFile @0x107d10
//
// Binary flow (~0x6C0 bytes):
//   1. Check hadSyntaxError via SeqcParserContext at this+0x1C0
//      If true, return immediately (no output)                    @0x107d31
//   2. Check assemblerText_ (at +0x248) is non-empty
//      If empty, return                                           @0x107d54
//   3. Create ostringstream
//   4. Call getAssemblerHeader(path) → string                     @0x107d76
//   5. Write header to ostringstream
//   6. Write assemblerText_ to ostringstream
//   7. Write "\n"
//   8. Open boost::filesystem::basic_ofstream with path, mode=trunc (0x10)
//   9. Extract ostringstream content → write to ofstream
//  10. Close file, clean up
// ============================================================================
//! \brief Write the human-readable assembler listing (banner +
//!        `assemblerText_`) to `path` as plain text.
void AWGCompilerImpl::writeAssemblerToFile(std::string const& path) {  // @0x107d10
    // 1. Check syntax error — @0x107d31: if hadSyntaxError, return
    if (compiler_.hadSyntaxError()) {
        return;
    }

    // 2. Check assembler text is non-empty
    if (assemblerText_.empty()) {
        return;  // @0x107d54: empty string → return
    }

    // 3-6. Build output with header + assembler text
    std::ostringstream oss;

    // 4-5. Get and write assembler header
    // @0x107d76: calls getAssemblerHeader(path)
    oss << getAssemblerHeader(path);

    // 6. Write assembler text
    oss << assemblerText_ << "\n";                                    // @0x107dcb + @0x107dfb

    // 7-9. Write to file
    std::ofstream ofs(
        path,
        std::ios::trunc);                                          // mode = 0x10

    std::string content = oss.str();
    ofs << content;

    // 10. Close + cleanup (handled by dtors)
}

// ============================================================================
// AWGCompilerImpl::getAssemblerHeader @0x1083d0 (~0x4B0 bytes)
//
// Generates a decorated comment block header for assembler output files.
// Binary flow:
//   1. Write "//****..." banner (0x51 bytes from rodata 0x8ff382)
//   2. Write "// " + path + "\n"
//   3. Write "//------..." separator
//   4. Write "//\n"
//   5. Write "// This file was generated automatically, do not edit!\n"
//   6. Write "//\n"
//   7. If sourceFilename_ non-empty: write "// Source file : " + sourceFilename_ + "\n"
//   8. Write "// Compiler    : ziAWG Compiler Version " + "26.01.3.9" + "\n"
//   9. Write "// Created     : " + formatTime(now, false) + "\n"
//  10. Write "//\n" + "//****..." banner + "\n\n"
//  11. Extract ostringstream → return string
// ============================================================================
//! \brief Build the multi-line "do not edit" banner that
//!        `writeAssemblerToFile` prepends to its output.
std::string AWGCompilerImpl::getAssemblerHeader(std::string const& path) const {  // @0x1083d0
    static const char banner[] =
        "//*************************************************************"
        "***************\n";  // 0x51 bytes at 0x8ff382
    static const char separator[] =
        "//-------------------------------------------------------------"
        "---------------\n";

    std::ostringstream oss;

    oss << banner;                                                 // @0x108430
    oss << "// " << path << "\n";                                  // @0x10846c
    oss << separator;                                              // @0x1084b0
    oss << "//\n";                                                 // @0x1084e0
    oss << "// This file was generated automatically, do not edit!\n"; // @0x108510
    oss << "//\n";                                                 // @0x108540

    // Conditionally write source file line
    if (!sourceFilename_.empty()) {                                    // @0x10856e
        oss << "// Source file : " << sourceFilename_ << "\n";
    }

    // Compiler version — hardcoded "26.01.3.9" at rodata 0x8fecf3
    oss << "// Compiler    : ziAWG Compiler Version " << "26.01.3.9" << "\n";

    // Timestamp — formatTime(localtime(now), false)
    auto now = boost::posix_time::second_clock::local_time();
    oss << "// Created     : " << formatTime(now, false) << "\n";

    oss << "//\n";
    oss << banner;
    oss << "\n";                                                   // trailing blank line

    return oss.str();                                              // @0x108665
}

// ============================================================================
// AWGCompilerImpl::getJsonArguments @0x10a3c0 (~0x89A bytes)
//
// Builds a JSON object via boost::property_tree with keys:
//   "destination" — the output filename (parameter)
//   "source"      — sourceFilename_ (source filename, if non-empty)
//   "waves"       — array from outputData_ (wave file paths)
// Serialized via write_json to ostringstream.
//
// Uses boost::property_tree (not boost::json). Full implementation
// deferred until property_tree dependency is integrated into the build.
// ============================================================================
//! \brief Build the JSON object emitted as the `.arguments` ELF
//!        section (destination filename, source filename, wave
//!        paths).
std::string AWGCompilerImpl::getJsonArguments(std::string const& destination) const {  // @0x10a3c0
    boost::property_tree::ptree root;

    // "destination" — the output filename
    root.put("destination", destination);                           // @0x10a4b0

    // "source" — sourceFilename_ (source filename), only if non-empty
    if (!sourceFilename_.empty()) {                                     // @0x10a530
        root.put("source", sourceFilename_);
    }

    // "waves" — array of wave file paths from wavePaths_ (+0x280)
    boost::property_tree::ptree wavesArr;                           // @0x10a5c0
    for (auto const& wp : wavePaths_) {
        boost::property_tree::ptree child;
        child.put("", wp);
        wavesArr.push_back(std::make_pair("", child));
    }
    root.add_child("waves", wavesArr);                              // @0x10a700

    std::ostringstream oss;
    boost::property_tree::json_parser::write_json(oss, root, false);
    return oss.str();
}

// ============================================================================
// AWGCompilerImpl::getJsonVersion @0x10ac60 (~0xBD0 bytes)
//
// Builds a JSON object via boost::property_tree with keys:
//   "compiler"             — asBinary(getLaboneVersion()) as unsigned int
//   "target"               — device family code name from deviceType switch
//   "external_triggering"  — "dio" or "zsync" (conditional)
//   "required_options"     — array (conditional)
// Serialized via write_json to ostringstream.
//
// The "external_triggering" and "required_options" fields depend on
// AWGCompilerImpl fields at offsets not yet fully mapped (likely accessed
// through compiler_.customFunctions_ at this+0x190). The confirmed
// fields ("compiler" and "target") are implemented; the others are
// conditionally omitted until the source data is identified.
// ============================================================================
//! \brief Build the JSON object emitted as the `.version_json`
//!        ELF section (compiler version, target family, bitstream
//!        register base, optional triggering mode and required
//!        feature set).
std::string AWGCompilerImpl::getJsonVersion() const {  // @0x10ac60
    boost::property_tree::ptree root;

    // "compiler" — LabOne version as binary uint32
    CalVer ver = getLaboneVersion();                                // @0x100270
    uint32_t binVer = asBinary(ver);                                // @0x1007c0
    root.put<unsigned int>("compiler", binVer);                     // @0x10ad60

    // "target" — device family code name from deviceType switch
    // Same mapping as getBinVersion suffix but with full names
    const char* targetName;
    switch (config_->deviceType) {
    case AwgDeviceType::HDAWG:                                      // 2
        targetName = "hirzel";
        break;
    case AwgDeviceType::SHFQA:                                      // 8
    case AwgDeviceType::SHFSG:                                      // 16
    case AwgDeviceType::SHFQC_SG:                                   // 32
    case AwgDeviceType::SHFLI:                                      // 64
        targetName = "grimsel";
        break;
    case AwgDeviceType::GHFLI:                                      // 128
        targetName = "gurnigel";
        break;
    case AwgDeviceType::VHFLI:                                      // 256
        targetName = "maloja";
        break;
    default:                                                         // UHFLI(1), UHFQA(4)
        targetName = "cervino";
        break;
    }
    root.put("target", targetName);                                 // @0x10aef0

    // "bitstream" — deviceConstants_.waveformRegBase as unsigned int  // @0x10af08
    root.put<unsigned int>("bitstream", deviceConstants_.waveformRegBase);  // @0x10af63

    // "external_triggering" — conditional on customFunctions_->externalTriggeringMode_  // @0x10af9a
    // externalTriggeringMode_ at CustomFunctions+0x1C0: Dio=1, ZSync=2, None=0
    {
        auto* cf = compiler_.customFunctions_.get();                            // @0x10af9a: mov 0x190(%r14)
        auto trigMode = cf->externalTriggeringMode_;                            // @0x10afa1: mov 0x1c0(%rax)
        if (trigMode == ExternalTriggeringMode::ZSync) {
            root.put("external_triggering", "zsync");                           // @0x10b004: "zsync" at 0x8ff36b
        } else if (trigMode == ExternalTriggeringMode::Dio) {
            root.put("external_triggering", "dio");                             // @0x10b072: "dio" at 0x8ff367
        }
    }

    // "required_options" — array from customFunctions_->usedFeatures_ set     // @0x10b0c1
    {
        auto* cf = compiler_.customFunctions_.get();                            // @0x10b0c1: mov 0x190(%r14)
        if (!cf->usedFeatures_.empty()) {                                       // @0x10b0c8: cmpq $0, 0x1d8(%r13)
            boost::property_tree::ptree optionsArray;
            for (auto const& feature : cf->usedFeatures_) {                     // @0x10b12f..10b391
                boost::property_tree::ptree elem;
                elem.put_value(feature);                                        // @0x10b1f7
                optionsArray.push_back(std::make_pair("", elem));               // @0x10b223
            }
            root.add_child("required_options", optionsArray);                   // @0x10b371 (rodata "required_options" at 0x8ff371)
        }
    }

    std::ostringstream oss;
    boost::property_tree::json_parser::write_json(oss, root, false);
    return oss.str();
}

//! \brief Concatenate the formatted `compileMessages_` followed by
//!        `assembler_.getReport()` into the user-facing report
//!        string.
std::string AWGCompilerImpl::getCompileReport() const {  // @0x104030
    // Binary: creates ostringstream, iterates compileMessages_, calls
    // each message's str(false), writes to stream + "\n", then appends
    // assembler_.getReport(), and returns oss.str().
    std::ostringstream oss;

    // Iterate compileMessages_ vector (+0x260)
    for (auto const& msg : compileMessages_) {
        oss << msg.str(/*hideLine=*/false) << "\n";  // @0x1040a1: str(false), @0x1040d4: "\n"
    }

    // Append assembler report
    // pad_278_ is AWGAssembler at +0x278
    oss << assembler_.getReport();  // @0x104101: call AWGAssembler::getReport

    return oss.str();
}


//! \brief Store `cb` into `cancelCallback_` and forward to
//!        `compiler_.setCancelCallback`.
void AWGCompilerImpl::setCancelCallback(std::weak_ptr<CancelCallback> cb) {  // @0x103eb0
    // @0x103ec0: store cb into this+0x298                                     // @0x103ec0
    cancelCallback_ = cb;
    // @0x103f0b: forward to compiler_                                         // @0x103f0b
    compiler_.setCancelCallback(cb);
    // Binary: wavetable_->cancelCallback_ at +0x1C0 is also set,
    // but WavetableFront doesn't expose a setter yet.
}

//! \brief Store `cb` into `progressCallback_` and forward to
//!        `compiler_.setProgressCallback`.
void AWGCompilerImpl::setProgressCallback(std::weak_ptr<ProgressCallback> cb) {  // @0x103f90
    // @0x103fa0: store cb into this+0x2A8                                     // @0x103fa0
    progressCallback_ = cb;
    // @0x103ff0: forward to compiler_                                         // @0x103ff0
    compiler_.setProgressCallback(cb);
}

// ============================================================================
// AWGCompiler — pimpl facade implementation
// ============================================================================

AWGCompiler::AWGCompiler(AWGCompilerConfig const& config)  // @0x103210
    : impl_(new AWGCompilerImpl(config))
{
}

AWGCompiler::~AWGCompiler() {  // @0x103260
    if (impl_) {
        impl_->~AWGCompilerImpl();
        ::operator delete(impl_, sizeof(AWGCompilerImpl));
        impl_ = nullptr;
    }
}

void AWGCompiler::compileString(std::string const& source) {  // @0x1032b0
    impl_->compileString(source);
}

// T5b.5: forwarders to the AWGCompilerImpl step methods.  No
// counterpart in the binary; added so `SeqcDriver::compile()` can
// drive the back end stage by stage, honouring `--to=<stage>`
// without paying for stages past the requested stop point.  Each
// forwarder is a one-line tail-call.
void AWGCompiler::stepInnerCompile(std::string const& source) {
    impl_->stepInnerCompile(source);
}

void AWGCompiler::stepAssembleOpcodes() {
    impl_->stepAssembleOpcodes();
}

void AWGCompiler::stepCheckLimits() {
    impl_->stepCheckLimits();
}

std::string const& AWGCompiler::assemblerText() const {
    return impl_->assemblerText();
}

void AWGCompiler::compileFile(std::string const& path) {  // @0x1032a0
    impl_->compileFile(path);
}

void AWGCompiler::addWaveforms(std::vector<std::string> const& paths) {  // @0x1032c0
    impl_->addWaveforms(paths);
}

void AWGCompiler::writeToStream(std::ostream& os, std::string const& format) {  // @0x1032d0
    impl_->writeToStream(os, format);
}

void AWGCompiler::writeToFile(std::string const& path) {  // @0x1032e0
    impl_->writeToFile(path);
}

void AWGCompiler::writeAssemblerToFile(std::string const& path) {  // @0x1032f0
    impl_->writeAssemblerToFile(path);
}

std::string AWGCompiler::getCompileReport() {  // @0x1033c0
    return impl_->getCompileReport();
}

std::string AWGCompiler::getJsonWaveformMemoryInfo() {  // @0x1033e0
    return impl_->getJsonWaveformMemoryInfo();
}

void AWGCompiler::setCancelCallback(std::weak_ptr<CancelCallback> cb) {  // @0x103300
    impl_->setCancelCallback(std::move(cb));
}

void AWGCompiler::setProgressCallback(std::weak_ptr<ProgressCallback> cb) {  // @0x103360
    impl_->setProgressCallback(std::move(cb));
}

// ------------------------------------------------------------------
// Introspection accessor — not present in the original binary.
// Added in T3c; refactored in T4 to be the single tooling entry
// point for capturing compiler IR into CompileSeqcIntrospection.
// Declared in compile_seqc.hpp; friend grant lives in
// awg_compiler.hpp.  Defined here (rather than in compile_seqc.cpp)
// because AWGCompilerImpl is only complete in this TU.  Future
// Phase-T IR captures extend the body of this function plus the
// CompileSeqcIntrospection struct — no per-stage friend grant is
// required.
// ------------------------------------------------------------------
void
fillIntrospection(AWGCompiler const& c,
                  CompileSeqcIntrospection& out) noexcept {
    out.loweredAst = c.impl_->getLoweredAst();
    out.wavetable  = c.impl_->getWavetable();
    out.asmList    = c.impl_->getAsmList();
}

}  // namespace zhinst
