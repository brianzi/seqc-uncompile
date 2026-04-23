// ============================================================================
// Reconstructed from disassembly of _seqc_compiler.so
// Class: zhinst::CsvParser
//
// The binary contains TWO non-template csvFileToWaveform implementations:
//   void CsvParser::csvFileToWaveform<WaveformFront>(shared_ptr<WaveformFront>, AwgDeviceType)  @0x2ba8b0
//   void CsvParser::csvFileToWaveform<WaveformIR>   (shared_ptr<WaveformIR>,    AwgDeviceType)  @0x2be830
//
// Each is ~7000 bytes (stack frame 0x528). They:
//   1. Compute file SHA-1 via CachedParser::getHash(filePath).
//   2. Look up the cached parsed file via CachedParser::getCachedFile(hash).
//   3. If cache miss: open the CSV, parse columns into doubles + markers,
//      apply waveform-specific transformations (different per Front/IR),
//      and store the result back in the cache.
//   4. Update the waveform's data buffer from the cached parsed file.
//
// Full reconstruction is deferred — these are not on the hot path for
// SeqC code that doesn't load CSV waveforms. Only the WaveformIR variant
// has an outstanding undefined-symbol reference (from
// WavetableIR::loadWaveform @0x29f377).
//
// Behaviour of this stub: throws so that the caller fails loudly if a
// CSV-loaded waveform is ever encountered. We do NOT silently no-op
// because that would corrupt waveform data with whatever zero-init the
// caller left in the waveform buffer.
// ============================================================================

#include "zhinst/types.hpp"
#include "zhinst/waveform_ir.hpp"
#include "zhinst/exception.hpp"

#include <memory>
#include <stdexcept>

namespace zhinst {

// Mirror of the inline class declared in wavetable_ir.cpp:32. Keeping the
// declaration here in the same TU as the definition lets the explicit
// instantiation generate a real symbol.
class CsvParser {
public:
    template <typename WfT>
    static void csvFileToWaveform(std::shared_ptr<WfT> wf, AwgDeviceType deviceType);
};

// ---------------------------------------------------------------------------
// CsvParser::csvFileToWaveform<WaveformIR>   — 0x2be830 (~7000 bytes)
//
// STUB: full reconstruction deferred. Throws to surface CSV-loaded waveform
// usage during testing rather than silently producing zero-filled output.
// ---------------------------------------------------------------------------
template <>
void CsvParser::csvFileToWaveform<WaveformIR>(
        std::shared_ptr<WaveformIR> /*wf*/,
        AwgDeviceType /*deviceType*/)
{
    // TODO(phase 20+): full reconstruction. ~7000 bytes; uses CachedParser
    // pipeline (getHash → getCachedFile → on miss, parse CSV columns to
    // samples+markers, store back). Not on hot path for typical SeqC
    // programs that don't reference .csv waveforms.
    throw std::runtime_error(
        "zhinst::CsvParser::csvFileToWaveform<WaveformIR>: "
        "CSV waveform loading is not implemented in the reconstructed build "
        "(stub at src/csv_parser.cpp). Original binary symbol at 0x2be830.");
}

}  // namespace zhinst
