// ============================================================================
// Reconstructed from disassembly of _seqc_compiler.so
//
// Free functions in zhinst::(anonymous namespace):
//   writeWavesToElfMapped   — inlined into writeToStream @ ~0x108d55-0x108dbb
//   writeWavesToElfAbsolute — inlined into writeToStream @ ~0x108dee-0x108e6e
//
// Lambda operator() implementations:
//   writeWavesToElfMapped::$_0::operator()   @ 0x10e020
//   writeWavesToElfAbsolute::$_0::operator() @ 0x10e190
//
// Lambda vtables:
//   Mapped:   0xb025c0 (vtable+0x10 = 0xb025d0)
//   Absolute: 0xb02650 (vtable+0x10 = 0xb02660)
//
// Calling context:
//   Both functions are inlined into AWGCompilerImpl::writeToStream() @ 0x108cc0.
//   writeToStream branches on AWGCompilerImpl+0x88 (bool mappedWaveforms):
//     if (mappedWaveforms)  → writeWavesToElfMapped  path
//     else                  → writeWavesToElfAbsolute path
//
// Signature (from lambda typeinfo names):
//   void writeWavesToElfMapped(AWGCompilerConfig const&, shared_ptr<WavetableIR>, ElfWriter&)
//   void writeWavesToElfAbsolute(AWGCompilerConfig const&, shared_ptr<WavetableIR>, ElfWriter&)
// ============================================================================

#include "zhinst/awg_compiler_config.hpp"
#include "zhinst/elf_writer.hpp"
#include "zhinst/waveform_ir.hpp"
#include "zhinst/wavetable_ir.hpp"

#include <functional>
#include <memory>

namespace zhinst {
namespace {

// ============================================================================
// writeWavesToElfMapped — Mapped waveform addressing
// Inlined at: 0x108d62 .. 0x108dbb (within writeToStream)
// Lambda operator(): 0x10e020
//
// Lambda captures (0x18 bytes total — __func object size from ~__func dtor):
//   +0x08: ElfWriter*             elfWriter
//   +0x10: AWGCompilerConfig*     config
//
// Behavior:
//   Iterates waveforms in ByWaveIndex order (WaveOrder::ByWaveIndex = 1).
//   For each waveform, calls:
//     elfWriter->addWaveform(waveform, config->sampleFormat, /*mapped=*/true, /*padSize=*/0);
//   The returned segment is discarded (unique_ptr destroyed immediately).
//
// Key difference from Absolute:
//   - Uses mapped addressing (bool param = true)
//   - No padding calculation — padSize is always 0
//   - Does not track cumulative offset
//   - Iterates in ByWaveIndex order (vs ByIndex for absolute)
// ============================================================================
void writeWavesToElfMapped(
    AWGCompilerConfig const& config,        // r13 (from writeToStream)
    std::shared_ptr<WavetableIR> wavetable, // r12 = AWGCompilerImpl+0xa8
    ElfWriter& elfWriter)                   // r15 = stack ElfWriter
{
    // Lambda captures: elfWriter ptr and config ptr
    // Capture layout matches __clone: 2 pointers at +0x08, +0x10 (16 bytes)
    auto callback = [&elfWriter, &config](
        std::shared_ptr<WaveformIR> const& waveform)  // 0x10e020
    {
        // 0x10e049: ecx = config->unknown_04  (AWGCompilerConfig+0x04 = SampleFormat)
        //   The field at config+0x04 is the sample format (int).
        SampleFormat format = static_cast<SampleFormat>(config.sampleFormat);

        // 0x10e05d: call addWaveform(sret, elfWriter, waveform, format, true, 0)
        //   r8d = 1 (mapped = true)
        //   r9d = 0 (padSize = 0)
        auto rawData = elfWriter.addWaveform(
            waveform,
            format,
            /*mapped=*/true,
            /*padSize=*/0);
        // rawData (unique_ptr<RawWave>) destroyed immediately at 0x10e062-0x10e076
    };

    // 0x108db1: edx = 1 → WaveOrder::ByWaveIndex
    // 0x108db6: call forEachUsedWaveform
    wavetable->forEachUsedWaveform(callback, WaveOrder::ByWaveIndex);
}

// ============================================================================
// writeWavesToElfAbsolute — Absolute waveform addressing
// Inlined at: 0x108dee .. 0x108e6e (within writeToStream)
// Lambda operator(): 0x10e190
//
// Lambda captures (0x20 bytes — from __func dtor at 0x10e100: size=0x20):
//   +0x08: uint32_t*              currentOffset  (pointer to local in parent)
//   +0x10: ElfWriter*             elfWriter
//   +0x18: AWGCompilerConfig*     config
//
// Behavior:
//   Iterates waveforms in ByIndex order (WaveOrder::ByIndex = 2).
//   Before the loop, calls getFirstWaveformOffset() to initialize currentOffset.
//   For each waveform:
//     1. Skip if !waveform->used (offset 0x48) or signal data is null (offset 0xd0)
//     2. Compute padding = (waveform->addressValue - *currentOffset) & (-waveform->elfAlignment_)
//        This aligns the gap between current position and target address
//     3. Call addWaveform(waveform, config->sampleFormat, false, padding)
//     4. Update *currentOffset = waveform->addressValue + rawData->size()
//        where rawData is the unique_ptr<RawWave> returned by addWaveform
//
// Key difference from Mapped:
//   - Uses absolute addressing (bool param = false)
//   - Computes per-waveform padding from address gap and alignment
//   - Tracks cumulative offset across waveforms
//   - Iterates in ByIndex order
//   - Skips waveforms that aren't used or have no signal data
// ============================================================================
void writeWavesToElfAbsolute(
    AWGCompilerConfig const& config,        // r13 from writeToStream
    std::shared_ptr<WavetableIR> wavetable, // r12 = AWGCompilerImpl+0xa8
    ElfWriter& elfWriter)                   // r15 = stack ElfWriter
{
    // 0x108e10: call getFirstWaveformOffset
    // 0x108e15: store result to stack local (rbp-0x40)
    uint32_t currentOffset = wavetable->getFirstWaveformOffset();

    // Lambda captures: &currentOffset, &elfWriter, &config
    // Capture layout: 3 pointers at +0x08, +0x10, +0x18 (24 bytes)
    auto callback = [&currentOffset, &elfWriter, &config](
        std::shared_ptr<WaveformIR> const& waveform)  // 0x10e190
    {
        WaveformIR* wf = waveform.get();

        // 0x10e1a0: cmpb $0x1, 0x48(%rax) — check waveform->used
        if (!wf->used)
            return;

        // 0x10e1aa: cmpq $0x0, 0xd0(%rax) — check signal data pointer
        // Waveform+0x80 = Signal, Signal+0x50 = some data ptr at absolute 0xd0
        // If null, skip this waveform (no actual waveform data loaded)
        if (wf->signal.data().empty())  // offset 0xd0 in Waveform
            return;

        // 0x10e1bb-0x10e1ce: compute padding
        //   ecx = wf->addressValue (0x4c)
        //   ecx -= *currentOffset
        //   r9d = 0 - wf->elfAlignment_ (0xdc)   → negation gives alignment mask
        //   r9d &= ecx                       → aligned padding
        uint32_t gap = wf->addressValue - currentOffset;
        uint32_t alignMask = static_cast<uint32_t>(-wf->elfAlignment_);
        uint32_t padding = gap & alignMask;

        // 0x10e1f2: ecx = config->unknown_04 (SampleFormat via config+0x04)
        SampleFormat format = static_cast<SampleFormat>(config.sampleFormat);

        // 0x10e200: call addWaveform(sret, elfWriter, waveform, format, false, padding)
        //   Returns unique_ptr<RawWave> via sret — used for size tracking
        auto rawData = elfWriter.addWaveform(
            waveform,
            format,
            /*mapped=*/false,
            padding);

        // 0x10e234: ebx = wf->addressValue
        // 0x10e23b-0x10e241: call rawData->size() (RawWave vtable+0x10), add to ebx
        // 0x10e247: *currentOffset = ebx
        currentOffset = wf->addressValue + static_cast<uint32_t>(rawData->size());
        // rawData (unique_ptr<RawWave>) destroyed here via deleting dtor (0x10e25a)
    };

    // 0x108e53: edx = 2 → WaveOrder::ByIndex
    // 0x108e58: call forEachUsedWaveform
    wavetable->forEachUsedWaveform(callback, WaveOrder::ByIndex);
}

} // anonymous namespace
} // namespace zhinst
