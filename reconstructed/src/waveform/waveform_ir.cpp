// ============================================================================
// Reconstructed from disassembly of _seqc_compiler.so
// WaveformIR method implementations
// ============================================================================

#include "zhinst/waveform/waveform_ir.hpp"
#include "zhinst/waveform/waveform_front.hpp"
#include "zhinst/device/device_constants.hpp"

#include <sstream>
#include <boost/property_tree/ptree.hpp>

namespace zhinst {

// 0x114da0 — WaveformIR::WaveformIR(shared_ptr<WaveformFront>)
//
// Constructs WaveformIR from a WaveformFront.
// 1. Extracts the name string from the source shared_ptr's pointee
// 2. Calls Waveform(shared_ptr<Waveform>, string) base ctor (copy-rename ctor at 0x114f10)
// 3. Initializes IR-specific fields:
//    - irField1 = 0 (uint16_t at +0xD8)
//    - crossesCacheLine_ = false (bool at +0xDA)
//    - elfAlignment_ = source->deviceConstants->someField (int at +0xDC)
//      Specifically: source[0]->field_at_0x78->field_at_0x24
// 4. If this->file is non-null:
//    - Clears file->fileHash vector (the vector at file+0x28)
//    - Sets file->formatType = 0, file->columnMode = 1
//    - Sets file->isIntegerFormat = 1
WaveformIR::WaveformIR(std::shared_ptr<WaveformFront> source)  // 0x114da0
    : Waveform(std::shared_ptr<Waveform>(source), std::string(source->name))
{
    // Ctor zeroes WORD at +0xD8, BYTE at +0xDA, then writes DWORD at +0xDC.
    // Two bools at +0xD8 (markedForLoad) and +0xD9 (fixed_) zeroed via WORD store.
    markedForLoad = false;
    fixed_ = false;
    crossesCacheLine_ = false;
    elfAlignment_ = source->deviceConstants->waveformElfAlignment;  // source->deviceConstants[0x24]

    if (file) {
        // Clear the file's data vector
        file->fileHash.clear();
        file->fileHash.shrink_to_fit();
        // Set file metadata: formatType=0, columnMode=1, isIntegerFormat=1
        file->formatType = 0;
        file->columnMode = 1;
        file->isIntegerFormat = 1;
    }
}

// 0x2a9240 — WaveformIR::WaveformIR(shared_ptr<Waveform>)
//
// Identical logic to the WaveformFront ctor above, just takes a base Waveform ptr.
WaveformIR::WaveformIR(std::shared_ptr<Waveform> source)  // 0x2a9240
    : Waveform(source, std::string(source->name))
{
    markedForLoad = false;
    fixed_ = false;
    crossesCacheLine_ = false;
    elfAlignment_ = source->deviceConstants->waveformElfAlignment;

    if (file) {
        file->fileHash.clear();
        file->fileHash.shrink_to_fit();
        file->formatType = 0;
        file->columnMode = 1;
        file->isIntegerFormat = 1;
    }
}

// Inlined into 0x2aa170-0x2aa20f (allocate_shared<WaveformIR> dispatcher,
// called by WavetableManager<WaveformIR>::newWaveform at 0x2aa004).
//
// The dispatcher emits, on the freshly-allocated 0xE0-byte WaveformIR:
//   +0x00..+0x17  Waveform::name           = copy of arg `name`
//   +0x18         Waveform::waveformType   = arg `type` (4 bytes; +0x1C padding)
//   +0x20..+0x37  Waveform::functionArgs  = empty string (libc++ SSO zeroed)
//   +0x38..+0x47  Waveform::file           = empty shared_ptr (both ptrs null)
//   +0x48         Waveform::used           = 0
//   +0x49..+0x4B  padding zeroed
//   +0x4C         Waveform::addressValue   = 0
//   +0x50..+0x67  Waveform::funDescrName    = empty string
//   +0x68         Waveform::playConfig       = 0
//   +0x6C         Waveform::waveIndex      = -1                  ← explicit
//   +0x70         Waveform::minLengthSamples    = dc.maxWaveformLength (dc+0x40)
//   +0x74         Waveform::allocationByteSize = 0
//   +0x78         Waveform::deviceConstants = &dc
//   +0x80..+0xCF  Waveform::signal         = all-zero (empty vectors etc.)
//   +0xD0         Signal::length_          = 0
//   +0xD8         markedForLoad            = 0  (word-stored with fixed_)
//   +0xD9         fixed_                   = 0
//   +0xDA         crossesCacheLine_        = 0
//   +0xDC         elfAlignment_                 = dc.waveformElfAlignment (dc+0x24)
//
// This constructor has no standalone symbol in the binary — the dispatcher
// inlines it. This body is the field-equivalent of the dispatcher's writes;
// it does not need to produce byte-identical instructions, only byte-identical
// observable state at end of construction.
WaveformIR::WaveformIR(const std::string& name,
                       Waveform::File::Type type,
                       const DeviceConstants& dc)  // inlined at 0x2aa170
{
    // Waveform base — reproduce dispatcher's writes verbatim.
    this->name             = name;
    this->waveformType     = type;
    this->functionArgs.clear();
    this->file.reset();
    this->used             = false;
    this->addressValue     = 0;
    this->funDescrName.clear();
    this->playConfig         = 0;
    this->waveIndex        = -1;
    this->minLengthSamples      = static_cast<int>(dc.maxWaveformLength);  // dc+0x40
    this->allocationByteSize = 0;
    this->deviceConstants  = &dc;
    // Signal default-constructs (vectors empty, scalars zero); explicit zero of
    // length_ matches the dispatcher's `mov QWORD PTR [rbx+0xe8],0x0`.
    this->signal.length_   = 0;

    // IR-specific extension fields.
    this->markedForLoad     = false;   // +0xD8 (word-stored together with fixed_)
    this->fixed_            = false;   // +0xD9
    this->crossesCacheLine_ = false;   // +0xDA
    this->elfAlignment_          = static_cast<int32_t>(dc.waveformElfAlignment);  // +0xDC ← dc+0x24
}

// 0x2c5440 — WaveformIR::toJsonElement(SampleFormat)
//
// Builds a boost::property_tree with keys:
//   "name"         <- this->name (string at +0x00)
//   "filename"     <- this->file ? file->name : ""  (file at +0x38)
//   "function"     <- this->functionArgs (string at +0x20)
//   "channels"     <- this->signal.channels (uint16_t at +0xC8, i.e. base+0x80+0x48)
//                     (written via stream_translator<uint16_t>)
//
// Then builds marker_bits string:
//   - Creates ostringstream
//   - For each channel i in 0..channels-1:
//     - Computes the "marker bits used" for that channel from the signal's
//       sample data (vector<uint8_t> at signal+0x30/+0x38, i.e. Waveform+0xB0/+0xB8)
//       by OR-ing the low 2 bits of each byte, finding highest set bit
//     - Converts to string via std::to_string
//     - Appends to ostringstream with "," separator
//   - Puts result as "marker_bits" key
//
//   "length"       <- this->signal.length (int at +0xD0, i.e. base+0x80+0x50)
//
// Returns the ptree (sret).
WaveformIR::ptree WaveformIR::toJsonElement(SampleFormat format) const  // 0x2c5440
{
    ptree result;

    // "name" <- this->name
    result.put("name", name);

    // "filename" <- file->name if file exists, else ""
    if (file) {
        result.put("filename", file->name);
    } else {
        result.put("filename", std::string());
    }

    // "function" <- functionArgs
    result.put("function", functionArgs);

    // "channels" <- signal.channels (uint16_t)
    uint16_t channels = signal.channels();  // at Waveform+0xC8
    result.put("channels", channels);

    // Build marker_bits string
    std::ostringstream oss;
    for (uint16_t i = 0; i < channels; ++i) {
        if (i != 0) {
            oss << ";";
        }
        // Compute marker bits for channel i from Signal::markerBits_
        // (vector<uint8_t> at WaveformIR+0xB0..+0xB8 = Signal+0x30..+0x38).
        // Verified at 0x2c571b: `mov rax,[r14+0xb0]; mov rsi,[r14+0xb8]`.
        const uint8_t* data = signal.markerBits_.data();
        size_t size = signal.markerBits_.size();

        // OR together all low 2 bits (mask 0x03) of each byte
        uint8_t orResult = 0;
        for (size_t j = 0; j < size; ++j) {
            orResult |= (data[j] & 0x03);
        }

        int markerBits;
        if (orResult == 0) {
            markerBits = 0;
        } else {
            // Count leading zeros, compute bit width
            int clz = __builtin_clz(orResult) - 24;  // for 8-bit
            markerBits = 8 - clz;
        }

        std::string s = std::to_string(markerBits);
        oss << s;
    }

    // "marker_bits" <- oss.str()
    result.put("marker_bits", oss.str());

    // "length" <- signal.length (int at Waveform+0xD0)
    result.put("length", signal.length());

    // "timestamp" <- constant "0000000000000000"
    // Binary 0x2c5a8b: lea 0x644dfb(%rip),%rdx → literal "0000000000000000"
    result.put("timestamp", "0000000000000000");

    // "play_config" <- this->playConfig (int at Waveform+0x68)
    // Binary 0x2c5aef: add $0x68,%r14 (this+0x68), then put<int>
    result.put("play_config", static_cast<int>(playConfig));

    return result;
}

} // namespace zhinst
