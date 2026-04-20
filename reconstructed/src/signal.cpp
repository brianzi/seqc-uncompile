// ============================================================================
// Reconstructed from disassembly of _seqc_compiler.so
// Binary: /home/brian/zhinst/seqc_compiler/_seqc_compiler.so
// Class:  zhinst::Signal — all 17 public methods
// ============================================================================

#include "zhinst/signal.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <boost/json.hpp>

namespace zhinst {

namespace util { namespace wave {
    // External: converts a double sample + marker to 16-bit AWG encoding
    // Located at 0x299630
    uint16_t double2awg(double sample, unsigned int marker);
}}

// ==========================================================================
// 1. Signal(size_t) — 0x25dd60
//    Allocates capacity for `length` samples and markers.
//    Initializes markerBits_ with a single zero byte.
//    Sets channels_=1, reserveOnly_=false, length_=length.
// ==========================================================================
Signal::Signal(size_t length)
    : samples_(), markers_(), markerBits_(), channels_(1), reserveOnly_(false), length_(length)
{
    // markerBits_ gets exactly 1 byte allocated and set to 0
    markerBits_.push_back(0);

    if (length > 0) {
        // Reserve capacity (no initialization) for samples and markers
        samples_.reserve(length);
        markers_.reserve(length);
    }
}

// ==========================================================================
// 2. Signal(size_t, double, uint8_t, uint16_t) — 0x25eac0
//    Fills `numSamples` samples with `value`, markers with `marker`.
//    Allocates markerBits_ of size `channels` and distributes marker bits.
//    length_ = numSamples / channels.
// ==========================================================================
Signal::Signal(size_t numSamples, double value, uint8_t marker, uint16_t channels)
    : samples_(), markers_(), markerBits_(), channels_(channels), reserveOnly_(false), length_(0)
{
    // Fill samples
    if (numSamples > 0) {
        samples_.resize(numSamples, value);
    }

    // Fill markers
    if (numSamples > 0) {
        markers_.resize(numSamples, marker);
    }

    // Allocate markerBits_ with `channels` entries, zero-filled
    if (channels > 0) {
        markerBits_.resize(channels, 0);
    }

    // Compute length
    length_ = numSamples / static_cast<size_t>(channels);

    // Distribute marker bits: iterate over (channels + (channels > 1 ? 1 : 0)) indices,
    // OR the marker into markerBits_[i % markerBits_.size()]
    size_t numEntries = static_cast<size_t>(channels);
    if (channels > 1)
        numEntries++;

    size_t mbSize = markerBits_.size();
    for (size_t i = 0; i < numEntries; ++i) {
        markerBits_[i % mbSize] |= marker;
    }
}

// ==========================================================================
// 3. Signal(size_t, MarkerBitsPerChannel const&) — 0x25f1a0
//    Copies markerBitsPerChannel into markerBits_.
//    channels_ = markerBitsPerChannel.size().
//    Reserves capacity for (channels * length) samples and markers.
// ==========================================================================
Signal::Signal(size_t length, MarkerBitsPerChannel const& markerBitsPerChannel)
    : samples_(), markers_(), markerBits_(), channels_(0), reserveOnly_(false), length_(length)
{
    // Copy marker bits per channel
    if (!markerBitsPerChannel.empty()) {
        markerBits_.assign(markerBitsPerChannel.begin(), markerBitsPerChannel.end());
    }

    channels_ = static_cast<uint16_t>(markerBitsPerChannel.size());

    size_t totalSamples = static_cast<size_t>(channels_) * length;
    if (totalSamples > 0) {
        samples_.reserve(totalSamples);
        markers_.reserve(totalSamples);
    }
}

// ==========================================================================
// 4. Signal(vector<double>, vector<uint8_t>, vector<uint8_t>, uint16_t, bool, size_t) — 0x2a8940
//    Move constructor from individual components.
//    Moves samples and markers; copies markerBits (3rd vector passed by value,
//    its contents are memcpy'd into markerBits_).
// ==========================================================================
Signal::Signal(std::vector<double> samples, std::vector<uint8_t> markers,
               std::vector<uint8_t> markerBits, uint16_t channels,
               bool reserveOnly, size_t length)
    : samples_(std::move(samples)),
      markers_(std::move(markers)),
      markerBits_(),
      channels_(channels),
      reserveOnly_(reserveOnly),
      length_(length)
{
    // markerBits is passed by value; copy its contents into markerBits_
    if (!markerBits.empty()) {
        markerBits_.assign(markerBits.begin(), markerBits.end());
    }
}

// ==========================================================================
// 5. Signal(ReserveOnly const&, size_t, MarkerBitsPerChannel const&) — 0x25ef50
//    Reserve-only construction: sets reserveOnly_=true, copies markerBits,
//    does NOT allocate samples_ or markers_.
// ==========================================================================
Signal::Signal(ReserveOnly const& /*tag*/, size_t length,
               MarkerBitsPerChannel const& markerBitsPerChannel)
    : samples_(), markers_(), markerBits_(), channels_(0), reserveOnly_(true), length_(length)
{
    if (!markerBitsPerChannel.empty()) {
        markerBits_.assign(markerBitsPerChannel.begin(), markerBitsPerChannel.end());
    }

    channels_ = static_cast<uint16_t>(markerBitsPerChannel.size());
}

// ==========================================================================
// 6. Signal(vector<double> const&, vector<uint8_t> const&, MarkerBitsPerChannel const&) — 0x106340
//    Copy-constructs all three vectors.
//    Derives channels_ from markerBitsPerChannel.size().
//    length_ = samples.size() / channels_.
// ==========================================================================
Signal::Signal(std::vector<double> const& samples,
               std::vector<uint8_t> const& markers,
               MarkerBitsPerChannel const& markerBitsPerChannel)
    : samples_(samples), markers_(markers), markerBits_(markerBitsPerChannel),
      channels_(static_cast<uint16_t>(markerBitsPerChannel.size())),
      reserveOnly_(false),
      length_(samples.size() / markerBitsPerChannel.size())
{
}

// ==========================================================================
// 7. Signal(vector<double> const&, uint16_t) — 0x25db90
//    Copies samples, creates zero-filled markers of same size,
//    creates zero-filled markerBits_ of size `channels`.
//    length_ = samples.size() / channels.
// ==========================================================================
Signal::Signal(std::vector<double> const& samples, uint16_t channels)
    : samples_(samples), markers_(), markerBits_(), channels_(channels), reserveOnly_(false), length_(0)
{
    // markers_ same size as samples, zero-filled
    markers_.resize(samples.size(), 0);

    // markerBits_ has `channels` entries, zero-filled
    if (channels > 0) {
        markerBits_.resize(channels, 0);
    }

    length_ = samples.size() / static_cast<size_t>(channels);
}

// ==========================================================================
// 8. Signal(Signal const&) — copy constructor — 0x1150e0
//    Deep-copies all three vectors and the scalar fields.
//    The last 16 bytes (+0x48..+0x57) are copied as a single 128-bit move.
// ==========================================================================
Signal::Signal(Signal const& other)
    : samples_(other.samples_),
      markers_(other.markers_),
      markerBits_(other.markerBits_),
      channels_(other.channels_),
      reserveOnly_(other.reserveOnly_),
      length_(other.length_)
{
}

// ==========================================================================
// 9. ~Signal() — 0x106520
//    Destroys vectors in order: markerBits_, markers_, samples_.
//    Each: if data ptr != null, deallocate with sized delete.
// ==========================================================================
Signal::~Signal() {
    // Compiler-generated vector destruction (shown explicitly for documentation):
    // markerBits_ destroyed first (offset +0x30)
    // markers_ destroyed second (offset +0x18)
    // samples_ destroyed last (offset +0x00)
}

// ==========================================================================
// 10. append(double, uint8_t) — 0x25de80
//     Appends one sample and one marker byte.
//     ORs the marker into markerBits_[index % markerBits_.size()].
//     Recomputes length_ = samples_.size() / channels_.
// ==========================================================================
void Signal::append(double sample, uint8_t marker) { // 0x25de80
    samples_.push_back(sample);
    markers_.push_back(marker);

    // OR marker into the appropriate markerBits_ slot
    size_t sampleIndex = samples_.size() - 1;
    size_t mbSize = markerBits_.size();
    markerBits_[sampleIndex % mbSize] |= marker;

    // Recompute length
    length_ = samples_.size() / static_cast<size_t>(channels_);
}

// ==========================================================================
// 11. append(Signal&) — 0x25f310
//     Appends another signal's data. Early-returns if other.length_ == 0.
//     Calls checkAllocation() on other to materialize reserve-only signals.
//     Inserts samples and markers at end; ORs markerBits element-wise.
//     Recomputes length_.
// ==========================================================================
void Signal::append(Signal& other) { // 0x25f310
    if (other.length_ == 0)
        return;

    auto insertPos = samples_.end();

    // Materialize other's data if it was reserve-only
    other.checkAllocation();
    auto otherSamplesBegin = other.samples_.data();
    other.checkAllocation();

    // Insert other's samples at our end
    samples_.insert(insertPos, other.samples_.begin(), other.samples_.end());

    // Insert other's markers at our end
    auto markerInsertPos = markers_.end();
    other.checkAllocation();
    other.checkAllocation();
    markers_.insert(markerInsertPos, other.markers_.begin(), other.markers_.end());

    // OR markerBits element-wise
    size_t mbSize = markerBits_.size();
    for (size_t i = 0; i < mbSize; ++i) {
        markerBits_[i] |= other.markerBits_[i];
    }

    // Recompute length
    length_ = samples_.size() / static_cast<size_t>(channels_);
}

// ==========================================================================
// 12. resizeSamples(size_t) — 0x1dff70
//     If reserveOnly_, just updates length_ and returns.
//     Otherwise resizes samples_ and markers_ to (channels_ * newLength).
//     Grows with zeros; shrinks by truncating.
// ==========================================================================
void Signal::resizeSamples(size_t newLength) { // 0x1dff70
    if (reserveOnly_) {
        length_ = newLength;
        return;
    }

    size_t totalSamples = static_cast<size_t>(channels_) * newLength;

    // Resize samples_
    size_t currentSize = samples_.size();
    if (totalSamples > currentSize) {
        samples_.resize(totalSamples);  // appends 0.0
    } else if (totalSamples < currentSize) {
        samples_.resize(totalSamples);  // truncates
    }

    // Resize markers_
    size_t currentMarkerSize = markers_.size();
    if (totalSamples > currentMarkerSize) {
        markers_.resize(totalSamples, 0);
    } else if (totalSamples < currentMarkerSize) {
        markers_.resize(totalSamples);
    }

    length_ = newLength;
}

// ==========================================================================
// 13. checkAllocation() — 0x246950
//     Materializes a reserve-only signal: if reserveOnly_ is true,
//     ensures samples_ and markers_ are sized to (channels_ * length_),
//     filling new entries with 0.0 / 0x00.
//     Does NOT clear reserveOnly_ flag.
// ==========================================================================
void Signal::checkAllocation() { // 0x246950
    if (!reserveOnly_)
        return;

    size_t totalSamples = static_cast<size_t>(channels_) * length_;

    // Grow samples_ to required size (fill with 0.0)
    if (totalSamples > samples_.size()) {
        double zero = 0.0;
        samples_.resize(totalSamples, zero);
    }

    // Grow markers_ to required size (fill with 0)
    if (totalSamples > markers_.size()) {
        markers_.resize(totalSamples, 0);
    }
}

// ==========================================================================
// 14. toJson() const — 0x2a3e40
//     Serializes to a boost::json::value object with keys:
//       "length"      → uint64
//       "channels"    → uint16
//       "reserveOnly" → bool
//       "data"        → array of doubles
//       "marker"      → array of uint8 (as integers)
//       "markerBits"  → array of uint8 (as integers)
// ==========================================================================
boost::json::value Signal::toJson() const { // 0x2a3e40
    // Build arrays
    boost::json::array dataArray;
    for (size_t i = 0; i < samples_.size(); ++i) {
        dataArray.push_back(samples_[i]);
    }

    boost::json::array markerArray;
    for (size_t i = 0; i < markers_.size(); ++i) {
        markerArray.push_back(static_cast<std::uint64_t>(markers_[i]));
    }

    boost::json::array markerBitsArray;
    for (size_t i = 0; i < markerBits_.size(); ++i) {
        markerBitsArray.push_back(static_cast<std::uint64_t>(markerBits_[i]));
    }

    // Construct object via initializer_list
    return boost::json::value{
        {"length", length_},
        {"channels", static_cast<std::uint64_t>(channels_)},
        {"reserveOnly", static_cast<bool>(reserveOnly_)},
        {"data", std::move(dataArray)},
        {"marker", std::move(markerArray)},
        {"markerBits", std::move(markerBitsArray)}
    };
}

// ==========================================================================
// 15. fromJson(boost::json::value const&) — 0x2a65d0
//     Static factory: deserializes from JSON.
//     Reads "data" → vector<double>, "marker" → vector<uint8_t>,
//     "markerBits" → vector<uint8_t>, "channels" → uint16_t,
//     "reserveOnly" → bool, "length" → uint64_t.
//     Constructs via the move-from-components constructor (#4).
// ==========================================================================
Signal Signal::fromJson(boost::json::value const& json) { // 0x2a65d0
    auto const& obj = json.as_object();

    // Parse "data" array into vector<double>
    auto const& dataArr = obj.at("data").as_array();
    std::vector<double> samples;
    samples.reserve(dataArr.size());
    std::transform(dataArr.begin(), dataArr.end(), std::back_inserter(samples),
        [](boost::json::value const& v) { return v.as_double(); });

    // Parse "marker" array into vector<uint8_t>
    auto const& markerArr = obj.at("marker").as_array();
    std::vector<uint8_t> markers;
    markers.reserve(markerArr.size());
    std::transform(markerArr.begin(), markerArr.end(), std::back_inserter(markers),
        [](boost::json::value const& v) { return static_cast<uint8_t>(v.as_int64()); });

    // Parse "markerBits" array into vector<uint8_t>
    auto const& mbArr = obj.at("markerBits").as_array();
    std::vector<uint8_t> markerBits;
    markerBits.reserve(mbArr.size());
    std::transform(mbArr.begin(), mbArr.end(), std::back_inserter(markerBits),
        [](boost::json::value const& v) { return static_cast<uint8_t>(v.as_int64()); });

    // Parse scalar fields
    uint16_t channels = static_cast<uint16_t>(obj.at("channels").to_number<uint64_t>());
    bool reserveOnly = obj.at("reserveOnly").as_bool();
    uint64_t length = obj.at("length").to_number<uint64_t>();

    return Signal(std::move(samples), std::move(markers), std::move(markerBits),
                  channels, reserveOnly, length);
}

// ==========================================================================
// 16. getRawData(SampleFormat) const — 0x293ec0
//     If reserveOnly_: returns RawWavePlaceHolder with byteSize = channels*length*2.
//     If Hirzel16: constructs RawWaveHirzel16 from samples_, markers_, markerBits_.
//     Otherwise (Cervino): converts each sample via double2awg() into uint16 vector.
// ==========================================================================
std::unique_ptr<RawWaveData> Signal::getRawData(SampleFormat format) const { // 0x293ec0
    if (reserveOnly_) {
        // Allocate RawWavePlaceHolder (size 0x28, vtable @ 0xb077c8)
        // byteSize_ at offset +0x08 = channels_ * length_ * 2
        auto placeholder = std::make_unique<RawWavePlaceHolder>();
        placeholder->byteSize_ = static_cast<size_t>(channels_) * length_ * 2;
        return placeholder;
    }

    if (format == SampleFormat::Hirzel16) {
        // Construct RawWaveHirzel16 (vtable set inside ctor at 0x297140)
        return std::make_unique<RawWaveHirzel16>(samples_, markers_, markerBits_);
    }

    // Default: Cervino format
    // RawWaveCervino has vtable @ 0xb07868, vector<uint16_t> at +0x08
    auto raw = std::make_unique<RawWaveCervino>();
    size_t numSamples = samples_.size();

    if (numSamples > 0) {
        raw->data_.resize(numSamples);
    }

    for (size_t i = 0; i < numSamples; ++i) {
        raw->data_[i] = util::wave::double2awg(samples_[i], markers_[i]);
    }

    return raw;
}

// ==========================================================================
// 17. operator==(Signal const&) const — 0x2a9750
//     Compares with fuzzy floating-point tolerance on samples_.
//     Exact byte comparison on markers_ and markerBits_.
//     Exact comparison on channels_, reserveOnly_, length_.
//
//     Tolerance formula: |a[i] - b[i]| <= |b[i]| * epsilon + epsilon
//     where epsilon is a constant (loaded from 0x956350, likely ~1e-7).
// ==========================================================================
bool Signal::operator==(Signal const& other) const { // 0x2a9750
    // Fuzzy comparison on samples
    // Note: if samples_.size()==0, the disasm still ensures >=1 iteration via
    //   cmp $1,%rcx; adc $0,%rcx — but with size 0, loop count is 1.
    //   In practice this only matters when both are empty (trivially passes).
    static constexpr double epsilon = 1e-12; // constant at address 0x956350

    size_t numSamples = samples_.size();
    for (size_t i = 0; i < numSamples; ++i) {
        double diff = samples_[i] - other.samples_[i];
        double absDiff = std::abs(diff);
        double absBi = std::abs(other.samples_[i]);
        double tolerance = absBi * epsilon + epsilon;
        if (absDiff > tolerance)
            return false;
    }

    // Exact comparison on markers_
    if (markers_.size() != other.markers_.size())
        return false;
    if (std::memcmp(markers_.data(), other.markers_.data(),
                    markers_.size()) != 0)
        return false;

    // Exact comparison on markerBits_
    if (markerBits_.size() != other.markerBits_.size())
        return false;
    if (std::memcmp(markerBits_.data(), other.markerBits_.data(),
                    markerBits_.size()) != 0)
        return false;

    // Scalar fields
    if (channels_ != other.channels_)
        return false;
    if (reserveOnly_ != other.reserveOnly_)
        return false;
    if (length_ != other.length_)
        return false;

    return true;
}

} // namespace zhinst
