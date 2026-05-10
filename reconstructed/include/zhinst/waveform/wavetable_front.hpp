// ============================================================================
// Reconstructed from disassembly of _seqc_compiler.so
// WavetableFront and WavetableManager<WaveformFront>
// ============================================================================
#pragma once

#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

#include "zhinst/core/types.hpp"
#include "zhinst/asm/address_impl.hpp"
#include "zhinst/waveform/signal.hpp"
#include "zhinst/ast/value.hpp"

#include "zhinst/waveform/waveform.hpp"

#include <boost/filesystem/path.hpp>

namespace zhinst {
class WaveformFront;  // used as template argument in WavetableManager<WaveformFront>*
}  // namespace zhinst

namespace zhinst {

namespace detail {

// Offset  Size  Type                                       Name
// +0x00   4     int                                        numDefs_
// +0x04   4     int                                        numDefs2_
// +0x08   40    unordered_map<string, size_t>              nameToIndex_
// +0x28   4     float                                      amplitudeDefault_  (init=1.0f)
// +0x2C   4     (padding)
// +0x30   24    vector<shared_ptr<WaveformT>>              waveforms_
// sizeof(WavetableManager) = 0x48
//! Storage and lookup for a collection of waveforms keyed by name
//! and indexed in registration order.
//!
//! Templated on the waveform element type so it can hold either
//! `WaveformFront` (used by `WavetableFront` during the front-end
//! pass) or `WaveformIR` (used by `WavetableIR` after lowering).
//! The `nameToIndex_` map provides O(1) lookup by name; the
//! `waveforms_` vector preserves the registration order needed when
//! emitting indices into the output ELF.
template <typename WaveformT>
class WavetableManager {
public:
    int numDefs_;           // +0x00
    int numDefs2_;          // +0x04
    std::unordered_map<std::string, size_t> nameToIndex_;   // +0x08 (40B)
    float amplitudeDefault_ = 1.0f;                         // +0x28
    // +0x2C: 4B padding
    std::vector<std::shared_ptr<WaveformT>> waveforms_;     // +0x30

    // --- Methods ---
    WavetableManager() = default;
    ~WavetableManager();                                    // 0x29fa40

    std::shared_ptr<WaveformT> newEmptyWaveform(
        const std::string& name,
        const DeviceConstants& dc);                         // 0x29aec0

    std::shared_ptr<WaveformT> newWaveformFromFile(
        const std::string& name,
        const std::string& filename,
        Waveform::File::Type type,
        const DeviceConstants& dc);                         // 0x29b110

    std::shared_ptr<WaveformT> newWaveformFromFile(
        const std::string& name,
        const Signal& signal,
        AddressImpl<uint32_t> addr,
        const std::string& filename,
        Waveform::File::Type type,
        const DeviceConstants& dc);                         // 0x29b560

    std::shared_ptr<WaveformT> newWaveform(
        const std::string& name,
        const Signal& signal,
        const std::string& funName,
        const std::vector<Value>& args,
        const DeviceConstants& dc);                         // 0x29ba00

    std::shared_ptr<WaveformT> getWaveformForFront(
        const std::string& funName,
        const std::vector<Value>& args) const;              // 0x29c210

    std::shared_ptr<WaveformT> copyWaveform(
        std::shared_ptr<WaveformT> src);                    // 0x29c440

    void updateWave(
        std::shared_ptr<WaveformT> wf,
        const std::string& newName);                        // 0x29ccf0

    void insertWaveform(std::shared_ptr<WaveformT> wf);    // 0x2a1200

    // IR-specialization methods (declared here, defined in wavetable_manager_ir.cpp)
    WavetableManager(int numDefs, int numDefs2, const std::vector<Waveform>& waveforms);

    std::shared_ptr<WaveformT> newWaveform(
        const std::string& name,
        const Signal& signal,
        const std::string& fillName,
        const DeviceConstants& dc);

    boost::json::value toJson() const;
    static WavetableManager fromJson(const boost::json::value& json, const DeviceConstants& dc);
    bool operator==(const WavetableManager& other) const;

    void setLineNr(int nr);                                 // (inlined via WavetableFront)
};

} // namespace detail

// Offset  Size   Type                                    Name
// +0x000  8      DeviceConstants const*                  deviceConstants_
// +0x008  4      uint32_t                                address_
// +0x00C  4      uint32_t                                address2_
// +0x010  0x108  std::ostringstream                      oss_
// +0x118  0x60   CachedParser (opaque)                   cachedParser_
// +0x178  24     std::map<size_t, size_t>                dioTableUsage_
// +0x190  32     std::function<void(string,int)>         warningCallback_
// +0x1B0  32     (internal warningCallback storage/pad)  warningCallbackStorage_
// +0x1D0  8      WavetableManager<WaveformFront>*        manager_
// +0x1D8  0x28   WaveIndexTracker (opaque)               waveIndexTracker_
// sizeof(WavetableFront) = 0x200
//! Front-end wavetable: owns the `WaveformFront` collection for one
//! AWG core during compilation, plus the on-disk waveform parser
//! cache, the DIO-table usage map, and the wave-index tracker.
//!
//! Construction binds the wavetable to a concrete `DeviceConstants`
//! and an address allocator, so subsequent `newWaveform*` calls can
//! immediately compute placement metadata. The wavetable acts as a
//! façade in front of the underlying `WavetableManager` template:
//! callers go through methods like `waveformExists()`,
//! `getWaveformByName()`, `getWaveformByFunDescr()`, and
//! `assignWaveIndex()` rather than touching the manager directly.
//!
//! Warnings emitted during waveform creation (rate quantisation,
//! truncation, deprecated-alias usage) are routed through
//! `warningCallback_`.
class WavetableFront {
public:
    const DeviceConstants* deviceConstants_;            // +0x000
    uint32_t address_;                                  // +0x008
    uint32_t address2_;                                 // +0x00C
    std::ostringstream oss_;                            // +0x010 (0x108B)
    char cachedParser_[0x60];                           // +0x118
    std::map<size_t, size_t> dioTableUsage_;            // +0x178
    std::function<void(const std::string&, int)> warningCallback_;  // +0x190
    char warningCallbackStorage_[0x20];                 // +0x1B0
    detail::WavetableManager<WaveformFront>* manager_;  // +0x1D0
    char waveIndexTracker_[0x28];                       // +0x1D8

    // --- Constructor / Destructor ---
    /*! \brief Build a fresh front-end wavetable bound to one
     *  AWG core's device constants and address space.
     *
     *  \details Captures the configuration that subsequent
     *  `newWaveform*` calls need (device constants, base
     *  address, sample-cache root path, line-number seed for
     *  synthetic-name minting) and constructs an empty
     *  `WavetableManager<WaveformFront>` to hold the
     *  registered waveforms.  The warning callback is
     *  initialised to `dummyWarning` until `setWarningCallback`
     *  installs one from the Compiler driver.
     *
     *  \param dc        Device-constants snapshot for the
     *                   target core.  The wavetable stores
     *                   the pointer; the constants must
     *                   outlive the wavetable.
     *  \param addr      Per-core wave-memory base address.
     *  \param lineNr    Initial value for the manager's
     *                   line-number counter used by
     *                   `getUniqueName`.
     *  \param path      Project directory used by the
     *                   embedded CSV parser cache to
     *                   resolve relative waveform paths.
     */
    WavetableFront(
        const DeviceConstants& dc,
        detail::AddressImpl<uint32_t> addr,
        size_t lineNr,
        const boost::filesystem::path& path);                 // 0x29ab10

    //! Tears down the wavetable: destroys the wave-index
    //! tracker, deletes the heap-allocated manager, and
    //! releases the warning callback, DIO-table-usage map,
    //! sample-parser cache, and string stream in reverse
    //! declaration order.
    ~WavetableFront();                                      // 0x29a940

    // --- Accessors ---
    //! No-op default warning sink installed by the
    //! constructor.
    //!
    //! Used as the initial value of `warningCallback_` so
    //! that warnings emitted before the Compiler installs
    //! the real callback do not crash on a null target.
    //! Logs the message to the boost log stream at WARNING
    //! severity but otherwise discards it.
    //!
    //! \param msg    Warning text.
    //! \param level  Warning level (currently unused).
    static void dummyWarning(const std::string& msg, int level); // 0x29ac60

    //! Pointer to the first waveform in the underlying
    //! manager's vector.  Pairs with `end()` for whole-list
    //! iteration in registration order.
    //! \return First-element pointer.
    const std::shared_ptr<WaveformFront>* begin() const;    // 0x29ad00
    //! One-past-the-end pointer for whole-list iteration.
    //! \return Sentinel pointer.
    const std::shared_ptr<WaveformFront>* end() const;      // 0x29ad20

    //! Install the warning sink invoked by waveform
    //! generators when they need to surface a non-fatal
    //! diagnostic (rate quantisation, truncation, deprecated
    //! alias).  The previous callback is dropped.
    //!
    //! \param cb  New warning sink.  Receives the formatted
    //!            warning message and a severity level.
    void setWarningCallback(
        std::function<void(const std::string&, int)> cb);   // 0x29ad40

    /*! \brief Estimated wave-memory footprint of every used
     *  waveform in the table.
     *
     *  \details Iterates the manager's waveform list and
     *  for each entry whose `used_` flag is set, computes
     *  the same per-waveform allocation size that
     *  `WavetableIR::assignWaveformAllocationSizes` will
     *  later compute: rounds the sample count up to
     *  `DeviceConstants::grainSize`, raises the result to
     *  `DeviceConstants::maxWaveformLength` when smaller,
     *  multiplies by `signal.channels()`, and converts to
     *  bytes.  Zero-length signals contribute zero so
     *  collapsed waveforms do not inflate the estimate.
     *
     *  Used by the front end to budget-check the working
     *  set against the device's wave memory before lowering
     *  to IR.
     *
     *  \return Total estimated bytes.
     */
    size_t getMemorySize() const;                           // 0x29adc0

    // --- Waveform creation (delegates to manager_) ---
    std::shared_ptr<WaveformFront> newEmptyWaveform(
        const std::string& name);                           // 0x29ae90

    std::shared_ptr<WaveformFront> newWaveformFromFile(
        const std::string& name,
        const std::string& filename,
        Waveform::File::Type type);                         // 0x29b0e0

    std::shared_ptr<WaveformFront> newWaveformFromFile(
        const std::string& name,
        const Signal& signal,
        const std::string& filename,
        Waveform::File::Type type);                         // 0x29b520

    std::shared_ptr<WaveformFront> newWaveform(
        const std::string& name,
        const Signal& signal,
        const std::string& funName,
        const std::vector<Value>& args);                    // 0x29b9d0

    std::shared_ptr<WaveformFront> newWaveform(
        const Signal& signal,
        const std::string& funName,
        const std::vector<Value>& args);                    // 0x29bce0

    // --- Query / Utility ---
    //! Concatenate every waveform's `toString()` output for
    //! debug printing.  Used by the Compiler driver's verbose
    //! dump path; not consumed by any production output.
    //! \return Concatenated waveform descriptions.
    std::string toString() const;                           // 0x29bd90

    /*! \brief Materialise a CSV-backed waveform's sample data.
     *
     *  \details Lazy-load helper used by waveform generators
     *  that need a placeholder's samples up front.  Skips
     *  the load when:
     *    - the waveform's `waveformType` is not `CSV` (so
     *      in-memory and binary-file waveforms pass through
     *      untouched), or
     *    - the sample buffer is already populated.
     *
     *  Otherwise routes through `CsvParser::csvFileToWaveform`
     *  via the embedded `cachedParser_` so repeated CSV
     *  references share parser state.  Parser exceptions
     *  are rethrown as `WavetableException` so callers see
     *  a wavetable-level diagnostic.
     *
     *  \param wf  Waveform to hydrate.  Held by value so the
     *             parser's downstream consumers can keep
     *             their own owners.
     *  \throws WavetableException  Wraps any `std::exception`
     *             raised by the parser.
     */
    void loadWaveform(std::shared_ptr<WaveformFront> wf);   // 0x29bff0

    bool waveformExists(const std::string& name) const;     // 0x29c160

    std::shared_ptr<WaveformFront> getWaveformByName(
        const std::optional<std::string>& name) const;      // 0x29c180

    std::shared_ptr<WaveformFront> getWaveformByFunDescr(
        const std::string& funName,
        const std::vector<Value>& args) const;              // 0x29c1f0

    std::shared_ptr<WaveformFront> copyWaveform(
        const std::shared_ptr<WaveformFront>& src);         // 0x29c3b0

    void checkWaveformInitialized(
        const std::string& name);                           // 0x29c540

    uint32_t getWaveformSampleLength(
        const std::string& name);                           // 0x29c860

    bool updateDioTableUsage(size_t key, size_t value);     // 0x29ca10

    void assignWaveIndex(
        std::shared_ptr<WaveformFront> wf, int index);      // 0x29cb40

    void updateWave(
        std::shared_ptr<WaveformFront> wf,
        const std::string& newName);                        // 0x29cc70

    //! Reseed the manager's line-number counter used by
    //! `getUniqueName` when minting synthetic waveform
    //! names.  Called by the front-end at every new SeqC
    //! statement so that synthesised names like
    //! `__sin_<line>_<counter>` carry the user-visible
    //! source line.
    //!
    //! \param nr  New line-number seed (typically the
    //!            current `Token::lineNr`).
    void setLineNr(int nr);                                 // 0x29ce10
};

} // namespace zhinst
