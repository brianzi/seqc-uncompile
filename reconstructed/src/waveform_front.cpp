// ============================================================================
// Reconstructed from disassembly of _seqc_compiler.so
// WaveformFront method implementations
// ============================================================================

#include "zhinst/waveform_front.hpp"

#include <sstream>
#include <string>

namespace zhinst {

// ============================================================================
// WaveformFront::toString() const
// Binary address: 0x2c5120 – 0x2c53d3
//
// Formats:  "Name: <name> (<type>) <numSamples> samples & <sampleWidth> channels"
// where <type> is one of "CSV", "RAW", "GEN", "UNDEF"
// ============================================================================
std::string WaveformFront::toString() const  // 0x2c5120
{
    std::ostringstream oss;

    oss << "Name: " << name << " (";

    std::string typeStr;
    switch (waveformType) {
        case Waveform::File::Type::CSV: typeStr = "CSV"; break;
        case Waveform::File::Type::RAW: typeStr = "RAW"; break;
        case Waveform::File::Type::GEN: typeStr = "GEN"; break;
        default:                        typeStr = "UNDEF"; break;
    }
    oss << typeStr;

    oss << ") " << signal.length_ << " samples & "
        << signal.channels_ << " channels";

    return oss.str();
}

// ============================================================================
// WaveformFront::WaveformFront(shared_ptr<WaveformFront> source, string const& newName)
// Binary address: 0x2a2510 – 0x2a2686
//
// Copy-with-rename constructor. Delegates base class construction to
// Waveform::Waveform(shared_ptr<Waveform>, string) at 0x114f10, passing
// the source (implicitly sliced to shared_ptr<Waveform>) and a copy of
// newName. Then initializes extension fields:
//   - frontField1 = 1
//   - frontBool1  = false
//   - frontBool2  = source->frontBool2   (copied)
//   - values      = source->values       (element-wise copy via uninitialized_copy)
// ============================================================================
WaveformFront::WaveformFront(std::shared_ptr<WaveformFront> source,
                             std::string const& newName)  // 0x2a2510
    : Waveform(std::shared_ptr<Waveform>(source), std::string(newName))
{
    // --- Extension field initialization ---
    frontField1 = 1;            // mov DWORD PTR [rbx+0xd8], 1
    frontBool1  = false;        // mov BYTE PTR [rbx+0xdc], 0

    // Copy frontBool2 from source
    frontBool2 = source->frontBool2;  // movzx ecx, BYTE PTR [rax+0xdd]
                                       // mov BYTE PTR [rbx+0xdd], cl

    // Copy the values vector from source (element-wise, each Value is 0x28 bytes)
    // The binary zero-inits the vector then allocates and uses
    // __uninitialized_allocator_copy_impl to deep-copy elements.
    values = std::vector<Value>(source->values.begin(), source->values.end());
}

// ============================================================================
// WaveformFront::~WaveformFront()
// Binary address: via __on_zero_shared at 0x2a1300
//
// Destroys vector<Value> at +0xE0, then tail-calls Waveform::~Waveform().
// (Implicitly generated; included for documentation.)
// ============================================================================
WaveformFront::~WaveformFront()
{
    // vector<Value> destructor runs automatically for 'values' at +0xE0.
    // Then Waveform::~Waveform() destroys base class fields.
}

} // namespace zhinst
