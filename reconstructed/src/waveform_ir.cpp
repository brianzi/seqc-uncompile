// ============================================================================
// Reconstructed from disassembly of _seqc_compiler.so
// WaveformIR method implementations
// ============================================================================

#include "zhinst/waveform_ir.hpp"
#include "zhinst/waveform_front.hpp"

#include <sstream>

namespace zhinst {

// 0x114da0 — WaveformIR::WaveformIR(shared_ptr<WaveformFront>)
//
// Constructs WaveformIR from a WaveformFront.
// 1. Extracts the name string from the source shared_ptr's pointee
// 2. Calls Waveform(shared_ptr<Waveform>, string) base ctor (copy-rename ctor at 0x114f10)
// 3. Initializes IR-specific fields:
//    - irField1 = 0 (uint16_t at +0xD8)
//    - irBool1 = false (bool at +0xDA)
//    - irField2 = source->deviceConstants->someField (int at +0xDC)
//      Specifically: source[0]->field_at_0x78->field_at_0x24
// 4. If this->file is non-null:
//    - Clears file->data vector (the vector at file+0x28)
//    - Sets file->field18 = 0, file->field1C = 1
//    - Sets file->field20 = 1
WaveformIR::WaveformIR(std::shared_ptr<WaveformFront> source)  // 0x114da0
    : Waveform(std::shared_ptr<Waveform>(source), std::string(source->name))
{
    irField1 = 0;
    irBool1 = false;
    irField2 = source->deviceConstants->/*offset 0x24*/0;  // source->deviceConstants[0x24]

    if (file) {
        // Clear the file's data vector
        file->data.clear();
        file->data.shrink_to_fit();
        // Set file metadata: field18=0, field1C=1, field20=1
        file->field18 = 0;
        file->field1C = 1;
        file->field20 = 1;
    }
}

// 0x2a9240 — WaveformIR::WaveformIR(shared_ptr<Waveform>)
//
// Identical logic to the WaveformFront ctor above, just takes a base Waveform ptr.
WaveformIR::WaveformIR(std::shared_ptr<Waveform> source)  // 0x2a9240
    : Waveform(source, std::string(source->name))
{
    irField1 = 0;
    irBool1 = false;
    irField2 = source->deviceConstants->/*offset 0x24*/0;

    if (file) {
        file->data.clear();
        file->data.shrink_to_fit();
        file->field18 = 0;
        file->field1C = 1;
        file->field20 = 1;
    }
}

// 0x2c5440 — WaveformIR::toJsonElement(SampleFormat)
//
// Builds a boost::property_tree with keys:
//   "name"         <- this->name (string at +0x00)
//   "filename"     <- this->file ? file->name : ""  (file at +0x38)
//   "function"     <- this->secondaryName (string at +0x20)
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

    // "function" <- secondaryName
    result.put("function", secondaryName);

    // "channels" <- signal.channels (uint16_t)
    uint16_t channels = signal.channels;  // at Waveform+0xC8
    result.put("channels", channels);

    // Build marker_bits string
    std::ostringstream oss;
    for (uint16_t i = 0; i < channels; ++i) {
        if (i != 0) {
            oss << ",";
        }
        // Compute marker bits for channel i from signal sample data
        // signal.sampleData is vector at Waveform+0xB0 (Signal+0x30)
        const uint8_t* data = signal.sampleData.data();
        size_t size = signal.sampleData.size();

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
    result.put("length", signal.length);

    return result;
}

} // namespace zhinst
