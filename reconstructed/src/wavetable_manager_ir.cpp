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

#include "zhinst/wavetable_ir.hpp"
#include "zhinst/wavetable_front.hpp"
#include "zhinst/device_constants.hpp"

#include <boost/json.hpp>
#include <string>
#include <vector>

namespace zhinst {
namespace detail {

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
    this->lineNr_ = numDefs;
    this->waveformCounter_ = numDefs2;
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
// 4. Copies fillName into waveform->secondaryName (+0x50)
// 5. Inserts the waveform into the target manager (rsi parameter, called on r14)
// 6. Returns shared_ptr<WaveformIR> (sret via rdi)
template<>
std::shared_ptr<WaveformIR> WavetableManager<WaveformIR>::newWaveform(
    const std::string& name,
    const Signal& signal,
    const std::string& fillName,
    const DeviceConstants& dc)  // 0x2a9fe0
{
    // Create the WaveformIR with file type = 2 (generated)
    // TODO: WaveformIR has no (name, Type, DC) ctor. The binary likely
    // constructs a Waveform first then wraps. Using Waveform→WaveformIR path:
    auto baseWf = std::make_shared<Waveform>();  // TODO: needs proper Waveform ctor
    baseWf->name = name;
    auto wf = std::make_shared<WaveformIR>(std::move(baseWf));

    WaveformIR* raw = wf.get();

    // Copy signal data if source != dest
    if (&raw->signal != &signal) {
        raw->signal.samples_.assign(signal.samples_.begin(), signal.samples_.end());
        raw->signal.markers_.assign(signal.markers_.begin(), signal.markers_.end());
        raw->signal.markerBits_.assign(signal.markerBits_.begin(), signal.markerBits_.end());
    }

    // TODO: Copy signal metadata — Signal has channels_ and reserveOnly_ at +0x48,
    // but no 'metadata' aggregate field. Exact field mapping TBD.
    raw->signal.channels_ = signal.channels_;
    raw->signal.reserveOnly_ = signal.reserveOnly_;
    raw->signal.length_ = signal.length_;

    // Copy secondary name (fill pattern name)
    raw->secondaryName = fillName;

    // Insert into this manager
    insertWaveform(wf);

    return wf;
}

// NOTE: insertWaveform<WaveformIR> specialization removed — uses the general
// template definition from wavetable_front.hpp (the body is identical).
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
        {"numDefs", lineNr_},
        {"numDefs2", waveformCounter_},
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
    if (lineNr_ != other.lineNr_)
        return false;
    if (waveformCounter_ != other.waveformCounter_)
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
