// ============================================================================
// Reconstructed from disassembly of _seqc_compiler.so
// WavetableManager<WaveformIR> method implementations
//
// WavetableManager<WaveformIR> layout — 0x48 bytes:
//
// Offset  Size  Type                                    Name
// ------  ----  ----                                    ----
// 0x00     4    int                                     numDefs (or samplesPerWave)
// 0x04     4    int                                     numDefs2 (or second int field)
// 0x08    0x28  unordered_map<string, size_t>           nameIndex_
//               (libc++ hash table: buckets ptr, bucket_count,
//                node chain ptr, element count, hash/eq)
// 0x30     8    shared_ptr<WaveformIR>*                 waveforms_.begin_
// 0x38     8    shared_ptr<WaveformIR>*                 waveforms_.end_
// 0x40     8    shared_ptr<WaveformIR>*                 waveforms_.cap_
// 0x48          END
//
// The unordered_map stores name -> index mappings for O(1) lookup.
// waveforms_ is a vector of shared_ptr<WaveformIR>.
// ============================================================================

#include "zhinst/waveform/wavetable_ir.hpp"
#include "zhinst/waveform/wavetable_front.hpp"
#include "zhinst/device/device_constants.hpp"

#include <boost/json.hpp>
#include <string>
#include <vector>

namespace zhinst {
namespace detail {

// Forward-declare the explicit specialization so all implicit uses below
// (in the ctor and newWaveform) bind to it instead of triggering implicit
// instantiation of the primary template — which would conflict with the
// specialization definition further down. Required by C++14 [temp.expl.spec]/6.
template<>
void WavetableManager<WaveformIR>::insertWaveform(
    std::shared_ptr<WaveformIR> wf);

// 0x2a5260 — WavetableManager<WaveformIR>::WavetableManager(int, int, const vector<Waveform>&)
//
// Constructor that builds from a vector of plain Waveform objects:
// 1. Stores numDefs and numDefs2 at +0x00, +0x04
// 2. Initializes nameIndex_ (unordered_map) to empty
// 3. Zeroes waveforms_ vector at +0x30
// 4. Iterates the source vector (stride 0xD8 = sizeof(Waveform)):
//    a. Copy-constructs a temporary Waveform on stack
//    b. Allocates shared_ptr_emplace<Waveform> (0xF0 bytes)
//    c. Copy-constructs into the emplaced object
//    d. Creates shared_ptr<WaveformIR> via allocate_shared (from shared_ptr<Waveform>)
//    e. Calls insertWaveform to add to manager
//    f. Releases temporaries
// 5. Destructs the temporary Waveform
template<>
WavetableManager<WaveformIR>::WavetableManager(
    int numDefs, int numDefs2,
    const std::vector<Waveform>& waveforms)  // 0x2a5260
{
    this->numDefs_ = numDefs;
    this->numDefs2_ = numDefs2;
    // nameIndex_ is default-constructed (empty)
    // waveforms_ vector is zeroed

    for (const auto& wf : waveforms) {
        Waveform copy(wf);
        auto sharedWf = std::make_shared<Waveform>(copy);
        auto wfIR = std::make_shared<WaveformIR>(std::move(sharedWf));
        insertWaveform(std::move(wfIR));
    }
}

// 0x29dfa0 — WavetableManager<WaveformIR>::~WavetableManager()
//
// Destruction order:
// 1. Destroy waveforms_ vector (release all shared_ptrs at +0x30..+0x38, free buffer)
// 2. Destroy nameIndex_ unordered_map:
//    - Walk the linked-list chain of hash nodes (at +0x18)
//    - For each node: free key string if heap-allocated, then free node (0x30 bytes)
//    - Free bucket array (buckets ptr at +0x08, size = bucket_count * 8)
template<>
WavetableManager<WaveformIR>::~WavetableManager()  // 0x29dfa0
{
    // Destroy waveforms vector (release shared_ptrs)
    for (auto it = waveforms_.rbegin(); it != waveforms_.rend(); ++it) {
        it->reset();  // decrement refcount
    }
    // Free vector storage
    // ... (compiler generated)

    // Destroy nameIndex_ unordered_map (walk and free nodes)
    // ... (compiler generated)
}

// 0x2a9fe0 — WavetableManager<WaveformIR>::newWaveform(
//     const string& name, const Signal& signal, const string& fillName,
//     const DeviceConstants& dc)
//
// Creates a new WaveformIR with:
// 1. Allocates shared_ptr<WaveformIR> via allocate_shared with:
//    - name (from parameter)
//    - Waveform::File::Type = 2 (synthetic/generated)
//    - DeviceConstants reference
// 2. Copies signal data into the new waveform:
//    - signal.data (vector<double> at Signal+0x00) -> waveform->signal.data (+0x80)
//    - signal.markers (vector<uint8_t> at Signal+0x18) -> waveform->signal.markers (+0x98)
//    - signal.playMarkers (vector<uint8_t> at Signal+0x30) -> waveform->signal.playMarkers (+0xB0)
// 3. Copies signal metadata (16 bytes at Signal+0x48 -> waveform+0xC8)
// 4. Copies fillName into waveform->functionArgs (+0x50)
// 5. Inserts the waveform into the target manager (rsi parameter, called on r14)
// 6. Returns shared_ptr<WaveformIR> (sret via rdi)
// Disasm 0x2a9fe0..0x2aa0d3 details:
//   1. allocate_shared<WaveformIR>(allocator<WaveformIR>{}, name, type=2, dc)
//      → 0x2aa004 calls 0x2aa170 (dispatcher); the dispatcher inlines a
//      WaveformIR(name, File::Type, DC&) ctor (no separate ctor symbol).
//   2. Identity-guard: if &raw->signal != &signal, copy the three vector
//      members (samples_/markers_/markerBits_) via __assign_with_size.
//   3. Block-copy the 16 bytes at Signal+0x48..+0x57 (channels_/reserveOnly_/
//      padding/length_low) using a single movups xmm0 — i.e. one 16-byte
//      memcpy of the trailing Signal scalar block.
//   4. If &raw->functionArgs != &fillName, basic_string copy-assign.
//   5. insertWaveform(this, wf).
template<>
std::shared_ptr<WaveformIR> WavetableManager<WaveformIR>::newWaveform(
    const std::string& name,
    const Signal& signal,
    const std::string& fillName,
    const DeviceConstants& dc)  // 0x2a9fe0
{
    // Type 2 = synthetic/generated waveform (the dispatcher hard-codes this)
    constexpr auto kType = static_cast<Waveform::File::Type>(2);
    auto wf = std::allocate_shared<WaveformIR>(
        std::allocator<WaveformIR>{}, name, kType, dc);

    WaveformIR* raw = wf.get();

    // Copy signal data if source != dest (binary's identity guard at 0x2aa01e)
    if (&raw->signal != &signal) {
        raw->signal.samples_.assign(signal.samples_.begin(), signal.samples_.end());
        raw->signal.markers_.assign(signal.markers_.begin(), signal.markers_.end());
        raw->signal.markerBits_.assign(signal.markerBits_.begin(), signal.markerBits_.end());
    }

    // 0x2aa073-0x2aa079: single 16-byte block copy of Signal+0x48..+0x57.
    // This covers (in order): channels_ (uint16, +0x48), reserveOnly_ (bool,
    // +0x4A), 5 bytes of padding, and the low 8 bytes of length_ (+0x50).
    // Reproduce as field-by-field assignment for readability.
    raw->signal.channels_    = signal.channels_;
    raw->signal.reserveOnly_ = signal.reserveOnly_;
    raw->signal.length_      = signal.length_;

    // Copy the secondary "fill" name (binary at 0x2aa088 also has identity guard)
    if (&raw->functionArgs != &fillName) {
        raw->functionArgs = fillName;
    }

    insertWaveform(wf);
    return wf;
}

// 0x29d140 — WavetableManager<WaveformIR>::insertWaveform(shared_ptr<WaveformIR>)
//
// Mirror of the WaveformFront specialization at 0x2a1200. Body is identical:
// the index becomes waveforms_.size() before the push_back, then we record
// name->idx in nameToIndex_.
template<>
void WavetableManager<WaveformIR>::insertWaveform(
    std::shared_ptr<WaveformIR> wf)
{
    size_t idx = waveforms_.size();
    waveforms_.emplace_back(wf);
    const std::string& name = wf.get()->name;
    nameToIndex_.emplace(name, idx);
}

// NOTE: previous comment claimed the IR insertWaveform "uses the general
// template definition" — but no such generic body exists; only the Front
// specialization is defined in wavetable_manager_front.cpp.  added
// the IR specialization above to satisfy the link-time U reference.
// Original address: 0x29d140

// 0x29dd10 — WavetableManager<WaveformIR>::fromJson(const value&, const DeviceConstants&)
//
// Deserializes from JSON:
// 1. Gets json.as_object().at("waveforms").as_array()
//    - Each element is a Waveform JSON
// 2. Gets json.as_object().at("waveforms").as_array() again (second array)
//    - Gets element count for size info
// 3. Transforms the array elements into a vector<Waveform> using a lambda
// 4. Gets json.as_object().at("numDefs").as_int64() -> numDefs
// 5. Gets json.as_object().at("numDefs2").as_int64() -> numDefs2
// 6. Constructs WavetableManager(numDefs, numDefs2, waveformVector)
// 7. Destroys temporary vector
// Returns: WavetableManager<WaveformIR> (sret via rdi)
template<>
WavetableManager<WaveformIR> WavetableManager<WaveformIR>::fromJson(
    const boost::json::value& json,
    const DeviceConstants& dc)  // 0x29dd10
{
    const auto& obj = json.as_object();

    // Get waveforms array
    const auto& waveformsJson = obj.at("waveforms").as_array();

    // Transform JSON array to vector<Waveform>
    std::vector<Waveform> waveforms;
    for (const auto& elem : waveformsJson) {
        waveforms.emplace_back(Waveform::fromJson(elem, dc));
    }

    // Get scalar fields
    int numDefs = static_cast<int>(obj.at("numDefs").as_int64());
    int numDefs2 = static_cast<int>(obj.at("numDefs2").as_int64());

    // Construct the manager
    WavetableManager<WaveformIR> result(numDefs, numDefs2, waveforms);
    return result;
}

// 0x29d780 — WavetableManager<WaveformIR>::toJson() const
//
// Serializes to JSON:
// 1. Iterates waveforms_ vector, calls each waveform->toJson()
//    and collects into a vector<boost::json::value>
// 2. Constructs a JSON object with:
//    { "numDefs": numDefs_, "numDefs2": numDefs2_, "waveforms": [array] }
// 3. Uses boost::json::array from the collected values
// Returns: boost::json::value (sret via rdi)
template<>
boost::json::value WavetableManager<WaveformIR>::toJson() const  // 0x29d780
{
    // Collect waveform JSONs
    std::vector<boost::json::value> wfJsons;
    for (auto it = waveforms_.begin(); it != waveforms_.end(); ++it) {
        wfJsons.push_back((*it)->toJson());
    }

    // Build the waveforms array
    boost::json::array arr(wfJsons.begin(), wfJsons.end());

    // Build result object
    return boost::json::value{
        {"numDefs", numDefs_},
        {"numDefs2", numDefs2_},
        {"waveforms", std::move(arr)}
    };
}

// 0x29e0e0 — WavetableManager<WaveformIR>::operator==(const WavetableManager&) const
//
// Compares:
// 1. Iterates both waveforms_ vectors in parallel:
//    - Checks if both have same length (via xor of begin/end pointers)
//    - For each pair, calls Waveform::operator==(*lhs, *rhs)
//    - If any mismatch, returns false
// 2. Compares numDefs_ (at +0x00)
// 3. Compares numDefs2_ (at +0x04)
// 4. Compares nameIndex_ size (at +0x20)
// 5. If sizes match, iterates nameIndex_ linked list:
//    - For each entry in LHS, finds matching key in RHS
//    - Compares key strings and values (size_t indices)
//    - If any mismatch, returns false
// Returns combined result of all checks.
template<>
bool WavetableManager<WaveformIR>::operator==(
    const WavetableManager<WaveformIR>& other) const  // 0x29e0e0
{
    // Compare waveform vectors element-wise
    bool waveformsEqual = true;
    auto* lIt = waveforms_.data();
    auto* lEnd = waveforms_.data() + waveforms_.size();
    auto* rIt = other.waveforms_.data();
    auto* rEnd = other.waveforms_.data() + other.waveforms_.size();

    // Check if both empty or both non-empty with same count
    if ((lIt == lEnd) != (rIt == rEnd))
        waveformsEqual = false;

    while (lIt != lEnd && rIt != rEnd) {
        if (!(**lIt == **rIt)) {
            waveformsEqual = false;
            break;
        }
        ++lIt; ++rIt;
    }

    // Compare scalar fields
    if (numDefs_ != other.numDefs_)
        return false;
    if (numDefs2_ != other.numDefs2_)
        return false;

    // Compare nameToIndex_ maps
    if (nameToIndex_.size() != other.nameToIndex_.size())
        return waveformsEqual && false;

    // Walk nameToIndex_ and compare entries
    bool mapsEqual = true;
    for (const auto& [key, value] : nameToIndex_) {
        auto it = other.nameToIndex_.find(key);
        if (it == other.nameToIndex_.end()) {
            mapsEqual = false;
            break;
        }
        if (key != it->first || value != it->second) {
            mapsEqual = false;
            break;
        }
    }

    return waveformsEqual && mapsEqual;
}

} // namespace detail
} // namespace zhinst
