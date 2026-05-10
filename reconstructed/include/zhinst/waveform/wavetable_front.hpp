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
//! \brief Storage and lookup for a collection of waveforms keyed by
//!        name and indexed in registration order.
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
    //! \brief Line-number seed used by `getUniqueName` when minting
    //!        synthetic names.
    int numDefs_;           // +0x00
    //! \brief Monotonic counter appended to synthetic names to
    //!        guarantee per-line uniqueness.
    int numDefs2_;          // +0x04
    //! \brief Name → index lookup into `waveforms_`.
    std::unordered_map<std::string, size_t> nameToIndex_;   // +0x08 (40B)
    //! \brief Default per-waveform amplitude (`1.0f`).
    float amplitudeDefault_ = 1.0f;                         // +0x28
    // +0x2C: 4B padding
    //! \brief Registered waveforms in insertion order; the index
    //!        within this vector is the waveform's stable identity
    //!        for the rest of the pipeline.
    std::vector<std::shared_ptr<WaveformT>> waveforms_;     // +0x30

    // --- Methods ---
    //! \brief Default-construct an empty manager: zero counters,
    //! empty name index, empty waveform vector, and the
    //! `amplitudeDefault_` initialised to `1.0f` via the in-class
    //! initialiser.
    WavetableManager() = default;
    //! \brief Release every owned waveform (in reverse insertion
    //!        order) and the name-index hash table.
    //!
    //! Compiler-generated destruction order otherwise.
    ~WavetableManager();                                    // 0x29fa40

    /*! \brief Register a fresh placeholder waveform under an
     *  auto-minted name.
     *
     *  \details Used by the front-end whenever it needs to
     *  reserve a waveform slot before the corresponding
     *  generator has produced sample data.  Mints
     *  `getUniqueName(name, numDefs_, numDefs2_++)` so
     *  every empty-waveform request gets a fresh
     *  identifier even when callers pass the same `name`
     *  argument.  The waveform is created with file
     *  type `GEN`, `waveIndex == -1`, and bound to the
     *  given device constants.
     *
     *  \param name  Caller-supplied prefix for the synthetic
     *               name.
     *  \param dc    Device constants the new waveform binds
     *               to.
     *  \return Newly registered waveform.
     */
    std::shared_ptr<WaveformT> newEmptyWaveform(
        const std::string& name,
        const DeviceConstants& dc);                         // 0x29aec0

    /*! \brief Register a file-backed waveform without
     *  pre-supplied sample data.
     *
     *  \details Allocates a `WaveformFront` bound to `name`,
     *  attaches a `Waveform::File` describing the source
     *  file, and inserts it under `name`.  When `name`
     *  collides with an existing registration both entries
     *  are flagged with `setHasDuplicate(true)` so later
     *  passes can detect the ambiguity.
     *
     *  \param name      Waveform identifier.
     *  \param filename  Path to the source file (relative
     *                   to the wavetable's parser root).
     *  \param type      File type (CSV / binary / ...).
     *  \param dc        Device constants.
     *  \return Newly registered waveform.
     */
    std::shared_ptr<WaveformT> newWaveformFromFile(
        const std::string& name,
        const std::string& filename,
        Waveform::File::Type type,
        const DeviceConstants& dc);                         // 0x29b110

    /*! \brief Register a file-backed waveform whose `Signal`
     *  is already populated.
     *
     *  \details As above, but additionally copies the
     *  caller's `Signal` into the new waveform and stores
     *  `addr` as the placement-time address slot.  Used
     *  when the front-end has pre-parsed the file and only
     *  needs the manager to record the registration plus
     *  duplicate-detection state.
     *
     *  \param name      Waveform identifier.
     *  \param signal    Pre-built signal payload.
     *  \param addr      Address slot recorded on the new
     *                   waveform.
     *  \param filename  Source file path.
     *  \param type      File type.
     *  \param dc        Device constants.
     *  \return Newly registered waveform.
     */
    std::shared_ptr<WaveformT> newWaveformFromFile(
        const std::string& name,
        const Signal& signal,
        AddressImpl<uint32_t> addr,
        const std::string& filename,
        Waveform::File::Type type,
        const DeviceConstants& dc);                         // 0x29b560

    /*! \brief Register a generator-produced waveform under an
     *  explicit name.
     *
     *  \details Allocates a `WaveformFront` with file type
     *  `GEN`, copies the generator-produced `Signal`,
     *  records the `(funName, args)` descriptor for later
     *  dedup lookups via `getWaveformForFront`, and
     *  inserts under `name`.
     *
     *  \param name     User-visible waveform identifier.
     *  \param signal   Generator output.
     *  \param funName  Generator function name (dedup key).
     *  \param args     Generator arguments (dedup key).
     *  \param dc       Device constants.
     *  \return Newly registered waveform.
     */
    std::shared_ptr<WaveformT> newWaveform(
        const std::string& name,
        const Signal& signal,
        const std::string& funName,
        const std::vector<Value>& args,
        const DeviceConstants& dc);                         // 0x29ba00

    /*! \brief Look up a previously registered generator
     *  invocation for deduplication.
     *
     *  \details Linear scan over the registered waveforms.
     *  A waveform matches when all of:
     *    - its file type is `GEN`,
     *    - its `funDescrName()` equals `funName`,
     *    - its generator-args vector size equals
     *      `args.size()` and every element compares equal
     *      via `Value::operator==`,
     *    - it has not been marked modified (`isModified()`
     *      false).
     *
     *  Used by the front-end so that repeated calls with
     *  identical generator arguments share one underlying
     *  waveform.
     *
     *  \param funName  Generator function name.
     *  \param args     Generator arguments.
     *  \return Matching waveform, or null when no
     *          unmodified previous match exists.
     */
    std::shared_ptr<WaveformT> getWaveformForFront(
        const std::string& funName,
        const std::vector<Value>& args) const;              // 0x29c210

    /*! \brief Register a copy of `src` under an auto-minted
     *  name.
     *
     *  \details Mints `getUniqueName(src->name, numDefs_,
     *  numDefs2_++)` and constructs the copy via
     *  `allocate_shared` using the source-plus-name copy
     *  constructor of `WaveformT`, so the copy shares
     *  initial sample state with `src` but carries its
     *  own metadata identity.  Used for per-iteration
     *  mutation under loop unrolling.
     *
     *  \param src  Source waveform.  Must be non-null.
     *  \return Newly registered copy.
     */
    std::shared_ptr<WaveformT> copyWaveform(
        std::shared_ptr<WaveformT> src);                    // 0x29c440

    /*! \brief Rename `wf` to `newName`, updating the name
     *  index in place.
     *
     *  \details Removes the waveform's existing entry from
     *  `nameToIndex_` (if present), updates the waveform's
     *  internal name to `newName`, and re-inserts the
     *  same vector index under the new key.  The
     *  underlying `waveforms_` vector is not reordered.
     *
     *  \param wf       Waveform to rename.
     *  \param newName  New identifier.
     */
    void updateWave(
        std::shared_ptr<WaveformT> wf,
        const std::string& newName);                        // 0x29ccf0

    //! \brief Append `wf` to the end of `waveforms_` and record
    //!        its position in `nameToIndex_` under `wf->name`.
    //!
    //! Insertion order is therefore the waveform's stable index
    //! for the rest of the pipeline.
    //!
    //! \param wf  Waveform to register.
    void insertWaveform(std::shared_ptr<WaveformT> wf);    // 0x2a1200

    // IR-specialization methods (declared here, defined in wavetable_manager_ir.cpp)
    /*! \brief IR-side constructor that rebuilds the manager
     *  from a list of plain `Waveform` records.
     *
     *  \details Used by `fromJson` to materialise a
     *  `WavetableManager<WaveformIR>` from a deserialised
     *  waveform list.  Sets the counters from `numDefs` /
     *  `numDefs2`, then for each entry constructs a
     *  fresh `WaveformIR` wrapping a copy of the source
     *  `Waveform` and inserts it via `insertWaveform`.
     *
     *  \param numDefs    Initial value for `numDefs_`.
     *  \param numDefs2   Initial value for `numDefs2_`.
     *  \param waveforms  Source waveforms; each is copied
     *                    into the new manager.
     */
    WavetableManager(int numDefs, int numDefs2, const std::vector<Waveform>& waveforms);

    /*! \brief IR-side waveform factory used by the
     *  `WavetableIR` placement pipeline.
     *
     *  \details Allocates a `WaveformIR` with file type
     *  `GEN` bound to `dc`, copies the supplied `Signal`
     *  (samples / markers / marker-bit vectors plus the
     *  trailing channel/length scalar block), and stores
     *  `fillName` in the waveform's `functionArgs` slot
     *  before inserting the waveform under `name`.
     *
     *  \param name      User-visible identifier.
     *  \param signal    Source signal payload.  Copied.
     *  \param fillName  Stored as the waveform's
     *                   `functionArgs` text — used by the
     *                   filler synthesis path in
     *                   `assignWaveIndexImplicit` to label
     *                   reserve-only entries.
     *  \param dc        Device constants.
     *  \return Newly registered waveform.
     */
    std::shared_ptr<WaveformT> newWaveform(
        const std::string& name,
        const Signal& signal,
        const std::string& fillName,
        const DeviceConstants& dc);

    /*! \brief Serialise the manager's contents for round-trip
     *  via `fromJson()`.
     *
     *  \details Emits an object of the form
     *  `{ "numDefs": <int>, "numDefs2": <int>,
     *     "waveforms": [<waveform>, ...] }` where each
     *  waveform entry is the result of
     *  `WaveformIR::toJson()`.  The name-index map is not
     *  serialised because `fromJson` rebuilds it via
     *  `insertWaveform`.
     *
     *  \return JSON object payload.
     */
    boost::json::value toJson() const;

    /*! \brief Deserialise a manager previously emitted by
     *  `toJson()`.
     *
     *  \details Reads the `numDefs`, `numDefs2`, and
     *  `waveforms` fields and constructs the result via
     *  the 3-argument IR constructor, which re-inserts
     *  every waveform and rebuilds the name index.
     *
     *  \param json  JSON object as produced by `toJson()`.
     *  \param dc    Device constants used by
     *               `Waveform::fromJson` to interpret
     *               sample-format-dependent fields.
     *  \return Reconstructed manager (returned by value).
     *  \throws boost::system::system_error  When the JSON
     *               is missing required keys or fails the
     *               waveform-side validation.
     */
    static WavetableManager fromJson(const boost::json::value& json, const DeviceConstants& dc);

    /*! \brief Structural equality for cache-key comparisons.
     *
     *  \details Compares (in order):
     *    - the waveform lists element-wise via
     *      `WaveformIR::operator==`,
     *    - `numDefs_` and `numDefs2_`,
     *    - the size and key/value contents of
     *      `nameToIndex_`.
     *
     *  Two managers are equal iff all four match.
     *
     *  \param other  Manager to compare against.
     *  \return True when both managers would produce
     *          identical placement output.
     */
    bool operator==(const WavetableManager& other) const;

    //! \brief Reseed the manager's `numDefs_` line-number counter
    //!        used by `getUniqueName`.
    //!
    //! Inlined into `WavetableFront::setLineNr`; provided
    //! here for direct manager-level access.
    //!
    //! \param nr  New value for `numDefs_`.
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
    //! \brief Device-constants snapshot for the bound AWG core
    //!        (caller-owned).
    const DeviceConstants* deviceConstants_;            // +0x000
    //! \brief Per-core wave-memory base address.
    uint32_t address_;                                  // +0x008
    //! \brief Secondary address slot used by dual-channel devices.
    uint32_t address2_;                                 // +0x00C
    //! \brief Scratch string stream used by waveform diagnostics.
    std::ostringstream oss_;                            // +0x010 (0x108B)
    //! \brief Opaque storage for the CSV parser cache; resolves
    //!        relative waveform paths against the project root.
    char cachedParser_[0x60];                           // +0x118
    //! \brief Running DIO-table usage map keyed by caller-chosen
    //!        identifier (typically SeqC source line).
    std::map<size_t, size_t> dioTableUsage_;            // +0x178
    //! \brief Sink invoked by waveform generators for non-fatal
    //!        diagnostics (rate quantisation, truncation, etc.).
    std::function<void(const std::string&, int)> warningCallback_;  // +0x190
    //! \brief Inline storage backing `warningCallback_`'s small-
    //!        functor optimisation; not addressed directly.
    char warningCallbackStorage_[0x20];                 // +0x1B0
    //! \brief Heap-allocated underlying manager holding the
    //!        waveform vector and name index.
    detail::WavetableManager<WaveformFront>* manager_;  // +0x1D0
    //! \brief Opaque storage for the front-end wave-index tracker
    //!        placeholder; `WavetableIR` rebuilds its own.
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

    //! \brief Tear down the wavetable.
    //!
    //! Destroys the wave-index tracker, deletes the heap-
    //! allocated manager, and releases the warning callback,
    //! DIO-table-usage map, sample-parser cache, and string
    //! stream in reverse declaration order.
    ~WavetableFront();                                      // 0x29a940

    // --- Accessors ---
    //! \brief No-op default warning sink installed by the
    //!        constructor.
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

    //! \brief Pointer to the first waveform in the underlying
    //!        manager's vector.
    //!
    //! Pairs with `end()` for whole-list iteration in
    //! registration order.
    //! \return First-element pointer.
    const std::shared_ptr<WaveformFront>* begin() const;    // 0x29ad00
    //! \brief One-past-the-end pointer for whole-list iteration.
    //! \return Sentinel pointer.
    const std::shared_ptr<WaveformFront>* end() const;      // 0x29ad20

    //! \brief Install the warning sink invoked by waveform
    //!        generators when surfacing a non-fatal diagnostic.
    //!
    //! Receives rate-quantisation, truncation, and
    //! deprecated-alias notices.  The previous callback is
    //! dropped.
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
    //! \brief Register a placeholder waveform with no sample data.
    //!
    //! Forwards to `manager_->newEmptyWaveform` with the
    //! wavetable's bound device constants.  The returned
    //! waveform has no signal payload yet — callers fill it
    //! in via the waveform-generator framework before
    //! marking it `used`.
    //!
    //! \param name  User-visible identifier.  Must be unique
    //!              within this wavetable.
    //! \return Newly registered waveform.
    std::shared_ptr<WaveformFront> newEmptyWaveform(
        const std::string& name);                           // 0x29ae90

    //! \brief Register a file-backed waveform without a pre-
    //!        supplied `Signal`.
    //!
    //! Forwards to `manager_->newWaveformFromFile`; the
    //! manager creates an unloaded `WaveformFront` and
    //! defers the parse until `loadWaveform` is called.
    //!
    //! \param name      User-visible identifier.
    //! \param filename  Path to the file (resolved against
    //!                  the cached parser's project root).
    //! \param type      File type (CSV / binary / ...).
    //! \return Newly registered waveform.
    std::shared_ptr<WaveformFront> newWaveformFromFile(
        const std::string& name,
        const std::string& filename,
        Waveform::File::Type type);                         // 0x29b0e0

    //! \brief Register a file-backed waveform whose initial
    //!        `Signal` is already known.
    //!
    //! Forwards to `manager_->newWaveformFromFile`, passing
    //! the wavetable's current `address_` so the new
    //! waveform records the placement-time address slot.
    //!
    //! \param name      User-visible identifier.
    //! \param signal    Pre-built signal metadata.
    //! \param filename  Path to the file.
    //! \param type      File type.
    //! \return Newly registered waveform.
    std::shared_ptr<WaveformFront> newWaveformFromFile(
        const std::string& name,
        const Signal& signal,
        const std::string& filename,
        Waveform::File::Type type);                         // 0x29b520

    //! \brief Register a generator-produced waveform under an
    //!        explicit name.
    //!
    //! Forwards to `manager_->newWaveform` with the
    //! generator descriptor (`funName` + `args`) so that
    //! later `getWaveformByFunDescr` calls can deduplicate
    //! identical generator invocations.
    //!
    //! \param name     User-visible identifier.
    //! \param signal   Generator-produced signal payload.
    //! \param funName  Generator function name (e.g.
    //!                 `"gauss"`, `"sin"`).
    //! \param args     Generator arguments, forming the
    //!                 dedup key together with `funName`.
    //! \return Newly registered waveform.
    std::shared_ptr<WaveformFront> newWaveform(
        const std::string& name,
        const Signal& signal,
        const std::string& funName,
        const std::vector<Value>& args);                    // 0x29b9d0

    //! \brief Register a generator-produced waveform under an
    //!        auto-generated name.
    //!
    //! Mints a unique name via
    //! `getUniqueName(funName, manager_->numDefs_,
    //! manager_->numDefs2_++)` and then delegates to the
    //! named overload.  Used for anonymous generator
    //! invocations like `playWave(gauss(64, 32, 16))` where
    //! the user did not assign the waveform to a `wave`
    //! variable.
    //!
    //! \param signal   Generator-produced signal payload.
    //! \param funName  Generator function name.
    //! \param args     Generator arguments.
    //! \return Newly registered waveform with a synthetic
    //!         `__<funName>_<line>_<counter>` name.
    std::shared_ptr<WaveformFront> newWaveform(
        const Signal& signal,
        const std::string& funName,
        const std::vector<Value>& args);                    // 0x29bce0

    // --- Query / Utility ---
    //! \brief Concatenate every waveform's `toString()` output
    //!        for debug printing.
    //!
    //! Used by the Compiler driver's verbose dump path; not
    //! consumed by any production output.
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

    //! \brief Test whether a waveform with the given name has
    //!        been registered.
    //!
    //! \param name  User-visible identifier.
    //! \return True iff `name` is in the manager's name index.
    bool waveformExists(const std::string& name) const;     // 0x29c160

    //! \brief Look up a waveform by source-language name.
    //!
    //! Resolves through the manager's `nameToIndex_` map.
    //! Empty optional and unknown names both return null
    //! rather than throwing.
    //!
    //! \param name  Optional waveform name.
    //! \return Matching waveform, or null when not found.
    std::shared_ptr<WaveformFront> getWaveformByName(
        const std::optional<std::string>& name) const;      // 0x29c180

    //! \brief Look up a waveform by its generator descriptor for
    //!        deduplication.
    //!
    //! Forwards to `manager_->getWaveformForFront`, which
    //! scans previously registered waveforms for one whose
    //! `(funName, args)` pair matches.  Returning the same
    //! waveform for repeated calls with identical arguments
    //! lets the front-end avoid re-emitting identical
    //! generator-produced sample buffers.
    //!
    //! \param funName  Generator function name.
    //! \param args     Generator arguments.
    //! \return Matching waveform, or null when no previous
    //!         registration matches.
    std::shared_ptr<WaveformFront> getWaveformByFunDescr(
        const std::string& funName,
        const std::vector<Value>& args) const;              // 0x29c1f0

    //! \brief Register a deep copy of `src` under a new
    //!        auto-generated name.
    //!
    //! Forwards to `manager_->copyWaveform` after bumping
    //! the source's refcount so the manager can move from
    //! the temporary safely.  Used by code paths that need
    //! a private mutable waveform sharing only the initial
    //! sample state of an existing one (typically for
    //! per-iteration mutation inside loop unrolling).
    //!
    //! \param src  Source waveform.  Must be non-null.
    //! \return Newly registered copy.
    std::shared_ptr<WaveformFront> copyWaveform(
        const std::shared_ptr<WaveformFront>& src);         // 0x29c3b0

    /*! \brief Verify that a named waveform exists and has
     *  associated source data.
     *
     *  \details Looks the name up in the manager's index
     *  and applies two checks:
     *    1. The waveform must exist — otherwise raises
     *       a "waveform not found" diagnostic.
     *    2. The waveform must have either a backing file
     *       or a generator descriptor — otherwise raises
     *       an "uninitialized waveform" diagnostic.
     *
     *  The second check catches the corner case where a
     *  waveform variable was registered but its generator
     *  threw before producing samples (for example
     *  `marker(32, 1, 1)` with an out-of-range bit
     *  argument).  Such waveforms must not reach the
     *  lowering pass.
     *
     *  \param name  Waveform identifier to validate.
     *  \throws WavetableException  When either check fails.
     */
    void checkWaveformInitialized(
        const std::string& name);                           // 0x29c540

    //! \brief Read the raw sample length of a registered waveform.
    //!
    //! Looks up `name` and returns
    //! `WaveformFront::signal.length()` directly — does not
    //! apply grain-rounding or other allocation-side
    //! adjustments.  Used by the front-end when SeqC code
    //! reads a waveform's `length` property.
    //!
    //! \param name  Waveform identifier.
    //! \return Raw sample count of the waveform's signal.
    uint32_t getWaveformSampleLength(
        const std::string& name);                           // 0x29c860

    /*! \brief Track DIO-table entries used by a `playWaveDIO`
     *  call and report whether the budget is still under the
     *  device's limit.
     *
     *  \details Records `value` against `key` in
     *  `dioTableUsage_` (replacing any prior value for that
     *  key), sums the values, and compares against the
     *  device's `maxDioTableEntries`.  Returns true while
     *  the running total stays strictly below the limit so
     *  callers can detect overflow before emitting the
     *  next entry.
     *
     *  \param key    Caller-chosen identifier (typically
     *                the SeqC source line) so subsequent
     *                calls with the same key replace
     *                rather than accumulate.
     *  \param value  Number of DIO-table entries this call
     *                site contributes.
     *  \return True while total usage remains under the
     *          device limit; false once it would meet or
     *          exceed it.
     */
    bool updateDioTableUsage(size_t key, size_t value);     // 0x29ca10

    /*! \brief Bind a waveform to an explicit wave-index slot.
     *
     *  \details Used when SeqC source assigns an
     *  `assignWaveIndex(wave, n)` mapping.  The call is a
     *  no-op when the waveform already holds the requested
     *  index.  When it currently holds a *different*
     *  index, raises a `WavetableException` (waveform may
     *  not be reassigned to a new index once placed).
     *  Otherwise stores `index` on the waveform.
     *
     *  The set-side of the wave-index tracker is not
     *  updated here because the front-end tracker is a
     *  layout-incompatible placeholder; `WavetableIR`
     *  rebuilds its own tracker from the waveform indices
     *  during lowering.
     *
     *  \param wf     Waveform to bind.
     *  \param index  Target wave-index slot.
     *  \throws WavetableException  When `wf` already has a
     *                different explicit index.
     */
    void assignWaveIndex(
        std::shared_ptr<WaveformFront> wf, int index);      // 0x29cb40

    //! \brief Rename a registered waveform.
    //!
    //! Forwards to `manager_->updateWave`, which updates
    //! the manager's name index so subsequent lookups
    //! resolve `newName` to the same waveform object.
    //! Used by waveform-generator framework when a
    //! user-visible name is finalised after the underlying
    //! waveform has already been registered under a
    //! synthetic name.
    //!
    //! \param wf       Waveform to rename.  Held by value
    //!                 so the manager can move from it.
    //! \param newName  New user-visible identifier.
    void updateWave(
        std::shared_ptr<WaveformFront> wf,
        const std::string& newName);                        // 0x29cc70

    //! \brief Reseed the manager's line-number counter used by
    //!        `getUniqueName` when minting synthetic names.
    //!
    //! Called by the front-end at every new SeqC statement
    //! so that synthesised names like
    //! `__sin_<line>_<counter>` carry the user-visible
    //! source line.
    //!
    //! \param nr  New line-number seed (typically the
    //!            current `Token::lineNr`).
    void setLineNr(int nr);                                 // 0x29ce10
};

} // namespace zhinst
