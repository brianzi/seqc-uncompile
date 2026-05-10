// ============================================================================
// Reconstructed from disassembly of _seqc_compiler.so
// CustomFunctions — SeqC built-in function dispatch and device-specific logic.
//
// Size: 0x1E0 (480 bytes) — from __on_zero_shared_weak allocation 0x200
//       minus 0x20 control block.
//
// Ctor: 0x12bcf0    Dtor: 0x127c90
//
// Contains ~80 SeqC built-in function implementations (playWave, wait, etc.)
// bound as std::function entries in an unordered_map at +0x60.
//
// Source path (inferred): CustomFunctions.cpp
//
//  file split (audit §C1): extracted classes moved to dedicated
// headers:
//   - EvalResultValue                       → eval_result_value.hpp
//   - NodeMapData / VirtAddr / DirectAddr   → node_map_data.hpp
//   - NodeMapItem (+ std::hash spec)         → node_map_data.hpp
//   - MathCompiler / MathCompilerException  → math_compiler.hpp
// CustomFunctions[Value]Exception remain here.
// ============================================================================
#pragma once

#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <boost/json.hpp>

#include "zhinst/asm/asm_register.hpp"
#include "zhinst/ast/eval_result_value.hpp"
#include "zhinst/codegen/math_compiler.hpp"
#include "zhinst/runtime/node_map_data.hpp"
#include "zhinst/ast/value.hpp"

namespace zhinst {

// Forward declarations
class AsmCommands;
class AWGCompilerImpl;
struct AWGCompilerConfig;
struct DeviceConstants;
class EvalResults;
class Resources;
class WavetableFront;
class WaveformFront;
class WaveformGenerator;
enum AwgDeviceType : int;
//! \brief Ordered registry of the parameter-tree node entries
//! resolvable from SeqC code.
//!
//! Wraps a `std::map<std::string, NodeMapItem>` keyed by the node's
//! string path.  `CustomFunctions` owns a single `NodeMap` instance
//! at `+0xF8` (as `unique_ptr<NodeMap>`) and consults it through
//! `lookupNode` whenever a SeqC built-in references a parameter
//! node.  The class also exposes two static encoding helpers
//! (`toPhase`, `toFrequency`) used by the node-write path to
//! convert user-facing degrees / Hz values into the on-device fixed
//! point format.
class NodeMap {
public:
    NodeMap() = default;
    ~NodeMap() = default;
    //! \brief Look a parameter-tree node up by its slash-delimited
    //! string path.
    //!
    //! \param path  The node path as it appears in SeqC source
    //!              (e.g. `"sigouts/0/amplitude"`).
    //! \return  The matching `NodeMapItem` when `path` is in
    //!          `entries_`; otherwise a default-constructed
    //!          `NodeMapItem` with `data == nullptr`, which the
    //!          caller treats as "not registered".
    NodeMapItem retrieve(std::string const& path) const;  // @0x1c55d0
    //! \brief Encode a phase-in-degrees value as the device's
    //! 23-bit fixed-point representation.
    //!
    //! Multiplies `value` by `2^23 / 360` (≈23301.689f), rounds to
    //! the nearest integer, then folds the result into 23-bit
    //! two's-complement form (sign-extending the top bit when set,
    //! masking to 23 bits otherwise; the lone-sign-bit pattern
    //! `0x400000` is left untouched).
    //!
    //! \param value  Phase in degrees (positive or negative).
    //! \return  The encoded 23-bit phase ready for a node write.
    static int toPhase(float value);  // @0x1c5680
    //! \brief Encode a frequency-in-Hz value as the device's
    //! 48-bit phase-increment representation.
    //!
    //! Computes `(int64_t)(freq * 2^48 / sampleClock)` and
    //! reinterprets the bit pattern as `uint64_t` for protocol
    //! encoding.
    //!
    //! \param freq        Frequency in Hertz.
    //! \param sampleClock Device sample clock in Hertz.
    //! \return  The encoded 48-bit phase increment ready for a
    //!          node write.
    static uint64_t toFrequency(double freq, double sampleClock);  // @0x1c5630
    std::map<std::string, NodeMapItem> entries_;
};

// ============================================================================
// ExternalTriggeringMode — mutual-exclusion latch for DIO vs ZSync I/O.
//
// Binary evidence: error message for mixing (code 79) reads
//   "DIO and ZSync external triggering can't be mixed in the same program"
// and the JSON output key is "external_triggering" (values "dio"/"zsync").
//
// Underlying storage is int32_t at CustomFunctions+0x1C0.
// ============================================================================
enum class ExternalTriggeringMode : int32_t {
    None  = 0,   // unset — no I/O function compiled yet
    Dio   = 1,   // DIO family (setDIO, getDIO, getDIOTriggered, waitDIOTrigger, playDIOWave, playWaveDIO)
    ZSync = 2,   // ZSync family (getZSyncData, getFeedback, waitZSyncTrigger, playWaveZSync)
};

// ============================================================================
// AccessMode — comparable enum-like type used in set<AccessMode>
//
// Has a free function toString(AccessMode) with static table @0x9573c0.
// ============================================================================
enum class AccessMode : int32_t {
    // Verified from static SSO string table at .rodata 0x9573c0:
    //   [0] = "soft", [1] = "direct", [2] = "custom"
    // Bounds check at 0x108a3f: cmp rax, 3; jae throw
    Soft   = 0,
    Direct = 1,
    Custom = 2,
};

// Free function: toString(AccessMode)
// Static table at .rodata 0x9573c0
//! \brief Render an `AccessMode` value as one of the lowercase
//! strings `"soft"` / `"direct"` / `"custom"` (or `"unknown"` for
//! out-of-range inputs).
std::string toString(AccessMode mode);

inline bool operator<(AccessMode a, AccessMode b) {
    return static_cast<int32_t>(a) < static_cast<int32_t>(b);
}

// ============================================================================
// CustomFunctionsException — 0x20 bytes
//   vtable(+0x00) + string message_(+0x08)
//   what() returns message_ or "CustomFunctions Exception" if empty.
// ============================================================================
//! \brief Diagnostic raised by `CustomFunctions` for SeqC built-in
//! invocation errors that aren't tied to a specific argument value
//! (unsupported function, mode mismatch, missing resource, etc.).
//!
//! Carries a free-form `message_` string returned by `what()`; an
//! empty message degrades to the literal `"CustomFunctions Exception"`.
class CustomFunctionsException : public std::exception {
    std::string message_;  // +0x08
public:
    //! \brief Construct an exception carrying the rendered
    //! diagnostic `msg`.
    //!
    //! \param msg  Pre-rendered diagnostic stored verbatim and
    //!             returned by `what()`.
    explicit CustomFunctionsException(std::string const& msg);  // @0x15a4c0
    //! \brief Destroy the exception, releasing the owned
    //! `message_` string.
    ~CustomFunctionsException() override;                        // @0x15a520, D0 @0x16e6c0
    //! \brief Return the stored message as a C string, or the
    //! literal `"CustomFunctions Exception"` when `message_` is
    //! empty.
    //!
    //! \return  C-string view of `message_` (or the fallback
    //!          literal).
    const char* what() const noexcept override;                  // @0x16e710
};

// ============================================================================
// CustomFunctionsValueException — 0x40 bytes
//   vtable(+0x00) + string message_(+0x08) + size_t argIndex_(+0x20)
//   + string varName_(+0x28)
// ============================================================================
//! \brief Diagnostic raised by `CustomFunctions` when a specific
//! argument to a SeqC built-in is invalid (out-of-range, wrong type,
//! malformed value).
//!
//! Extends `CustomFunctionsException`'s plain message with the
//! offending `argIndex_` (zero-based position in the call's argument
//! list) and an optional `varName_` filled in by the caller via
//! `setVarName` once the bound variable's name is known — the SeqC
//! frontend uses both fields to render the user-facing error
//! message with the correct source location and variable label.
class CustomFunctionsValueException : public std::exception {
    std::string message_;   // +0x08
    size_t      argIndex_;  // +0x20
    std::string varName_;   // +0x28
public:
    //! \brief Construct a per-argument value diagnostic carrying
    //! `msg` and the offending argument's zero-based position.
    //!
    //! The `varName_` field is left empty; callers fill it via
    //! `setVarName` once the bound variable's name is available.
    //!
    //! \param msg       Pre-rendered diagnostic stored verbatim
    //!                  and returned by `what()`.
    //! \param argIndex  Zero-based position of the offending
    //!                  argument in the source call list.
    CustomFunctionsValueException(std::string const& msg, size_t argIndex);  // @0x163d00
    //! \brief Destroy the exception, releasing both `message_`
    //! and `varName_`.
    ~CustomFunctionsValueException() override;                                // @0x163d70, D0 @0x172f70
    //! \brief Return the stored message as a C string.
    //!
    //! Unlike `CustomFunctionsException::what`, an empty
    //! `message_` is returned as an empty string rather than
    //! substituted with a literal — callers always populate
    //! `message_` for value exceptions.
    //!
    //! \return  C-string view of `message_`.
    const char* what() const noexcept override;                               // @0x172fd0
    //! \brief Record the source variable's name for inclusion in
    //! the rendered diagnostic.
    //!
    //! \param name  Variable name to store in `varName_`.
    void setVarName(std::string const& name);                                 // @0x210750
    //! \brief Read the bound variable's name (empty until
    //! `setVarName` has been called).
    //!
    //! \return  Reference to the stored `varName_`.
    std::string const& varName() const { return varName_; }                    // inline accessor
    //! \brief Read the zero-based argument index supplied at
    //! construction.
    //!
    //! \return  The stored `argIndex_`.
    size_t argIndex() const { return argIndex_; }                              // inline accessor
};

// ============================================================================
// PlayArgs — helper for parsing play-function arguments
//
// parse @0x15d7b0, addChannelWave @0x170ec0
// ============================================================================
// PlayArgs — helper struct for parsing play-function arguments
//
// Ctor @0x15d600   Dtor @0x15efe0   Size: 0x80 (128 bytes)
//
// Field layout (fully reconstructed from ctor + dtor):
//
// Offset  Size  Type                                     Name            Init
// ------  ----  ----                                     ----            ----
// 0x00    16    shared_ptr<WavetableFront>               wavetable_      copied from arg
// 0x10    32    std::function<void(string const&)>       reportWarning_  copied from arg
//               (inline buf at +0x10, invoker ptr at +0x30)
// 0x30    8     (part of std::function — invoker/manager ptr)
// 0x38    8     (padding / std::function tail)
// 0x40    24    std::string                              cmdName_        copied from arg
// 0x58    2     uint16_t                                 channelsPerGroup_  config[0x14 + indexed*2]
// 0x5A    2     uint16_t                                 totalChannels_  channelsPerGroup_ * numChannelGroups
// 0x5C    4     (padding)
// 0x60    24    vector<vector<WaveAssignment>>            waveAssignments_  numChannelGroups empty inner vecs
// 0x78    1     bool                                     hasMarker_      false
//               (read/written by parse() at 0x15d7d5/0x15d819)
// 0x79    7     (padding to 0x80)
//
// Ctor logic (0x15d600):
//   1. Copy shared_ptr<WavetableFront> → +0x00, atomic inc refcount
//   2. Copy std::function (callback) → +0x10..+0x37
//   3. Copy string cmdName → +0x40..+0x57 (SSO or heap)
//   4. channelsPerGroup_ = config.channelsPerGroup[indexed]
//   5. totalChannels_    = channelsPerGroup_ * config.numChannelGroups
//   6. Allocate waveAssignments_ with numChannelGroups empty inner vectors
//      (element size 0x18 = sizeof(vector<WaveAssignment>)), memset to 0
//   7. hasMarker_ = false
//
// Dtor (0x15efe0) destroys in reverse:
//   1. waveAssignments_ (+0x60): destroy inner vecs, free backing array
//   2. cmdName_ (+0x40): free if heap-allocated
//   3. reportWarning_ (+0x10/+0x30): invoke function manager destructor
//   4. wavetable_ (+0x00/+0x08): decrement shared_ptr refcount
// ============================================================================
//! \brief Per-call helper that parses a `playWave`-family invocation's
//! argument list into a per-channel set of waveform assignments.
//!
//! Constructed fresh by each `playWave` / `playWaveIndexed` /
//! `playAuxWave` / etc. dispatch in `CustomFunctions` from the
//! current `AWGCompilerConfig`, the wavetable front, a warning
//! callback, the textual command name, and an `indexed` flag that
//! selects the per-group channel-count column.  The entry point
//! `parse` walks the argument vector, splitting it into either
//! implicit-channel or explicit-channel form and populating
//! `waveAssignments_` (one inner vector per channel group, each
//! holding `WaveAssignment` records that pair an `EvalResultValue`
//! with a list of channel-bit slots) plus the `hasMarker_` flag.
//! `getMaxSampleLength` and `addChannelWave` are the remaining
//! per-channel helpers used by the dispatchers after parsing
//! completes.
struct PlayArgs {
    //! \brief Initialise per-call parsing state from the device
    //! configuration, the shared wavetable, a warning sink, the
    //! source command name, and the indexed-vs-default selector.
    //!
    //! `channelsPerGroup_` is read from
    //! `config.channelsPerGroup[indexed]`, `totalChannels_` is
    //! `channelsPerGroup_ * config.numChannelGroups`, and
    //! `waveAssignments_` is sized to `numChannelGroups` empty
    //! inner vectors.  `hasMarker_` starts `false`.
    //!
    //! \param config         Device configuration supplying the
    //!                       per-group channel count and the
    //!                       group count.
    //! \param wavetable      Shared wavetable consulted by
    //!                       `secureLoadWaveform` and
    //!                       `getMaxSampleLength`.
    //! \param reportWarning  Callback invoked when a CSV-duplicate
    //!                       warning fires during waveform load.
    //! \param cmdName        Source command name, embedded in
    //!                       diagnostics raised by `parse`.
    //! \param indexed        Selects the indexed column of
    //!                       `config.channelsPerGroup`.
    // Constructor @0x15d600
    PlayArgs(AWGCompilerConfig const& config,
             std::shared_ptr<WavetableFront> wavetable,
             std::function<void(std::string const&)> reportWarning,
             std::string const& cmdName,
             bool indexed);
    //! \brief Release the captured wavetable, warning callback,
    //! command name, and per-group `WaveAssignment` storage in
    //! reverse declaration order.
    ~PlayArgs();  // @0x15efe0

    /*! \brief Parse a `playWave`-family argument list, populating
     *  `waveAssignments_` and `hasMarker_`, and return the
     *  iterator one past the last consumed argument.
     *
     *  \details A pre-scan walks every argument once: every entry
     *  with `varSubType_ == 2` flips `hasMarker_` to `true`, and
     *  the boundary between the parseable head of the call and an
     *  optional trailing arg (for example, an explicit rate) is
     *  set to one past the last `Wave`/`String`-typed entry.
     *  Dispatch on the first argument's `varType_` then chooses
     *  between two parsers:
     *    - `parseImplicitChannels` when the first arg is `Wave`
     *      or `String` (waveforms named in source order, with
     *      multi-channel waveforms expanding into synthetic
     *      continuation entries);
     *    - `parseExplicitChannels` otherwise (channel-number
     *      `Cvar` args interleaved with waveform args).
     *  The returned channel count is validated against
     *  `totalChannels_` and the parse boundary is returned to the
     *  caller as the resume cursor for any optional trailing
     *  argument.
     *
     *  \param args  Already-evaluated argument list from the
     *               source call site.
     *  \return  Iterator into `args` one past the last consumed
     *           argument.
     *  \throws  `CustomFunctionsException` when `args` is empty.
     *  \throws  `CustomFunctionsValueException` when the channel
     *           count exceeds `totalChannels_` or one of the
     *           per-channel parsers rejects an argument
     *           (mixed numbering, duplicate channel,
     *           missing waveform, wrong channel count).
     */
    std::vector<EvalResultValue>::const_iterator
    parse(std::vector<EvalResultValue> const& args);                  // @0x15d7b0

    //! \brief Return the maximum sample length across every
    //! waveform referenced in `waveAssignments_`.
    //!
    //! Walks each per-group `WaveAssignment`, skips marker and
    //! `Const` entries, looks the waveform up by name through
    //! `wavetable_->getWaveformByName`, and returns the largest
    //! `signal.length()` encountered (zero when the table is
    //! empty).
    //!
    //! \return  The largest waveform sample length, sign-extended
    //!          to `int64_t`.
    //! \throws  `CustomFunctionsValueException` (`WaveformNotFound`)
    //!          when a referenced waveform is missing from the
    //!          table, or `UninitializedWaveform` when a found
    //!          waveform has neither a backing file nor a
    //!          generator descriptor.
    int64_t getMaxSampleLength() const;                               // @0x15d9f0
    //! \brief Append a `WaveAssignment` for a single explicit
    //! channel slot.
    //!
    //! Computes the destination group as `channel /
    //! channelsPerGroup_` and the in-group slot number as
    //! `(channel % channelsPerGroup_) + 1`, then pushes a
    //! `WaveAssignment` carrying a deep copy of `val` (with a
    //! variant-aware copy of the embedded `Value`) and a
    //! single-element `bits` vector containing the slot number.
    //! Channel indices that meet or exceed `totalChannels_` are
    //! silently ignored.
    //!
    //! \param channel  Zero-based absolute channel index.
    //! \param val      Source argument copied into the new
    //!                 assignment's `value` slot.
    void addChannelWave(int channel, EvalResultValue const& val);     // @0x170ec0

    struct WaveAssignment {
        // Stride 0x50 (80 bytes) per entry — confirmed by playAuxWave's
        // inner copy loop @0x135990..0x1359d6 which copies exactly 0x38
        // bytes from WA+0..WA+0x38 into a vector<EvalResultValue> slot.
        //
        // The WaveAssignment STARTS with the EvalResultValue directly;
        // the leading 8 bytes are EvalResultValue::varType_ and
        // varSubType_.  Confirmed by:
        //   * @0x135854: `lea rsi, [rbx+0x8]` then call Value::toString
        //     ⇒ wa.value.value_ lives at WA+0x08 ⇒ wa.value lives at WA+0
        //   * @0x135990 inner copy: 0x38 bytes from WA+0 → vec<EvalResultValue>
        EvalResultValue value;       // +0x00 (0x38 bytes)
        std::vector<int> bits;       // +0x38, channel bit assignments
        // total = 0x50 ✓

        WaveAssignment() = default;
        //! \brief Variant-aware deep copy of `o`.
        //!
        //! Copies the leading `EvalResultValue` field-by-field —
        //! `varType_`, `varSubType_`, `reg_`, the `Value`'s
        //! `type_` / `which_` discriminators, and the storage
        //! union dispatched on `which_` (`int`/`bool` as 8
        //! bytes, `double` as 8 bytes, `std::string` via
        //! placement new) — then copy-constructs `bits` from
        //! `o.bits`.
        WaveAssignment(WaveAssignment const& o);  // @0x171c00
        WaveAssignment& operator=(WaveAssignment const&) = default;
        WaveAssignment(WaveAssignment&&) = default;
        WaveAssignment& operator=(WaveAssignment&&) = default;
    };
    // 0x50 on libc++ (binary), larger on libstdc++ due to Value's string union.
    static_assert(sizeof(WaveAssignment) >= 0x50,
                  "PlayArgs::WaveAssignment must be at least 0x50 bytes");

    // --- Fields (0x80 bytes total) ---
    std::shared_ptr<WavetableFront>              wavetable_;          // +0x00
    std::function<void(std::string const&)>      reportWarning_;      // +0x10 (48B in libc++, but only 32B+8B used here; +0x30 = invoker)
    std::string                                  cmdName_;            // +0x40
    uint16_t                                     channelsPerGroup_;   // +0x58
    uint16_t                                     totalChannels_;      // +0x5A
    // 4 bytes padding                                                // +0x5C
    std::vector<std::vector<WaveAssignment>>     waveAssignments_;    // +0x60
    bool                                         hasMarker_;          // +0x78

private:
    // parse() helpers
    int parseImplicitChannels(
        std::vector<EvalResultValue>::const_iterator begin,
        std::vector<EvalResultValue>::const_iterator end);            // @0x16fb30
    int parseExplicitChannels(
        std::vector<EvalResultValue>::const_iterator begin,
        std::vector<EvalResultValue>::const_iterator end);            // @0x170000
    std::shared_ptr<WaveformFront> secureLoadWaveform(
        std::string const& name, size_t argIndex) const;             // @0x1711a0
};

// Free functions related to CustomFunctions argument parsing
//! \brief Pop and return the trailing `String`-typed argument
//! from `args`, or `std::nullopt` when the last argument has any
//! other type (or `args` is empty).
//!
//! Used by built-ins that accept an optional textual suffix (a
//! waveform name, channel label, etc.) at the tail of their call
//! list.  When the trailing element is a `String`, it is removed
//! from `args` in place and its rendered text is returned;
//! otherwise `args` is left unchanged and `nullopt` is returned.
std::optional<std::string> parseOptionalString(std::vector<EvalResultValue>& args);  // @0x15d3e0
//! \brief Decode a rate argument from `val` and return its
//! integer rate code.
//!
//! The argument must be either `Const` or `Cvar`.  When `strict`
//! is `true` the returned value is the user-facing rate minus
//! two; when `strict` is `false` the raw integer is returned
//! unchanged.
//!
//! \param val     Argument carrying the rate value.
//! \param name    Source command name, used for diagnostics.
//! \param strict  Selects the strict (subtract-two) decoding.
//! \return  The decoded rate code.
//! \throws  `CustomFunctionsException` (error 0x9f) when `val`'s
//!          type is neither `Const` nor `Cvar`.
int getPlayRate(EvalResultValue const& val, std::string const& name, bool strict);     // @0x163730

//! \brief Parse the optional rate argument that may follow the
//! main `playWave` argument list, validating that the parse
//! cursor is left at `end`.
//!
//! `parseEnd` is the cursor returned by `PlayArgs::parse`; the
//! function expects either no remaining arguments or exactly one
//! `Const`/`Cvar` rate value.  When one rate value remains it is
//! decoded through `getPlayRate(*parseEnd, cmdName, strict)`;
//! when none remain the "absent rate" sentinel is returned —
//! `5` in strict mode, `0xffffffff` otherwise.
//!
//! \param begin     Iterator at the start of the original argument
//!                  list (used only for error positioning).
//! \param end       Iterator at one past the last argument.
//! \param parseEnd  Cursor returned by `PlayArgs::parse`.
//! \param cmdName   Source command name, used for diagnostics.
//! \param strict    Forwarded to `getPlayRate` and selects the
//!                  no-rate sentinel.
//! \return  The decoded rate code, or the "absent rate" sentinel.
//! \throws  `CustomFunctionsValueException`
//!          (`MixedChannelNumbering`) when more than one
//!          unparsed argument remains between `parseEnd` and
//!          `end`.
//! \throws  `CustomFunctionsException` propagated from
//!          `getPlayRate` when the rate argument has the wrong
//!          type.
// parseOptionalRate — parse optional rate argument from arg list     @0x163980
int parseOptionalRate(
    std::vector<EvalResultValue>::const_iterator begin,
    std::vector<EvalResultValue>::const_iterator end,
    std::vector<EvalResultValue>::const_iterator parseEnd,
    std::string const& cmdName,
    bool strict);

// ============================================================================
// CustomFunctions — 0x1E0 (480) bytes
//
// Member layout verified from dtor @0x127c90 and ctor @0x12bcf0.
// Cross-checked: lookupNode @0x15c530, addNodeAccess @0x15c6c0,
// getNodeAddress @0x16ba10.
//
// Offset  Size  Type                                              Name
// ------  ----  ----                                              ----
// 0x00    8     AWGCompilerConfig const*                          config_
// 0x08    8     DeviceConstants const*                            devConst_
// 0x10    16    shared_ptr<Resources>                             resources_
// 0x20    16    shared_ptr<WavetableFront>                        wavetableFront_
// 0x30    16    shared_ptr<WavetableFront>                        wavetableFrontPrivate_
// 0x40    16    shared_ptr<WaveformGenerator>                     waveformGen_
// 0x50    16    shared_ptr<AsmCommands>                           asmCommands_
// 0x60    32    unordered_map<string, function<shared_ptr<EvalResults>(
//                  vector<EvalResultValue> const&,
//                  shared_ptr<Resources>)>>                        funcMap_
// 0x80    4     float                                             funcMap_maxLoadFactor_ (1.0f max_load_factor)
// 0x84    4     (padding)
// 0x88    32    unordered_map<string, vector<string>>             aliasMap_
// 0xA8    4     float                                             aliasMap_maxLoadFactor_ (1.0f max_load_factor)
// 0xAC    4     (padding)
// 0xB0    24    set<string>                                       unusedStringSet_B0_
// 0xC8    48    MathCompiler                                      mathCompiler_
// 0xF8    8     unique_ptr<NodeMap>                               nodeMap_
//                  (CONFIRMED from lookupNode @0x15c530: reads [this+0xf8] as
//                   single ptr, passes to NodeMap::retrieve(string const&) const.
//                   NodeMap dtor @128110 size-24, so NodeMap == { map<string,NodeMapItem> }.)
// 0x100   40    unordered_map<NodeMapItem, uint32_t>              nodeIndexMap_
//                  (CONFIRMED from addNodeAccess @0x15c6c0 + getNodeAddress @0x16ba10:
//                   value is the index of the NodeMapItem within nodeList_.)
// 0x128   40    unordered_map<NodeMapItem, set<AccessMode>>       accessModeMap_
//                  (CONFIRMED from addNodeAccess hash-table emplace at 15c6f5.)
// 0x150   24    vector<NodeMapItem>                               nodeList_
//                  (CONFIRMED from addNodeAccess emplace_back_slow_path call
//                   at 15c7e4 with 24-byte stride.)
// 0x168   40    unordered_set<string>                             assignedWaveNames_
//                  (Resolved dtor at 127cf2 walks bucket array with
//                   string-node destruction; 1.0f max_load_factor at +0x188 in
//                   ctor at 12bec9 confirms unordered_set. Spans +0x168..+0x190.)
// 0x190   48    std::function<void(string const&)>                warningCallback_
//                  (CONFIRMED from dtor pattern at 127cb0: small-buffer check
//                   `cmp [rbx+0x1b0], (rbx+0x190)` and virtual destroy via vtable.)
// 0x1C0   4     ExternalTriggeringMode (int32_t)                    externalTriggeringMode_
//                  (None=0, Dio=1, ZSync=2; mutual-exclusion latch for I/O bus family)
// 0x1C4   4     padding
// 0x1C8   24    set<string>                                       usedFeatures_
// ============================================================================

//! \brief Dispatch table and execution context for every SeqC built-in
//! function (`playWave`, `wait`, `setDIO`, `setOscFreq`, ...) the
//! compiler recognises.
//!
//! Constructed once per compilation by `Compiler::compile` from the
//! device's `AWGCompilerConfig` and `DeviceConstants`, the shared
//! wavetable / waveform-generator / asm-commands singletons, and the
//! caller's warning callback.  The constructor populates `funcMap_`
//! (name → `std::function` returning `EvalResults`) with bound
//! member-function entries for the ~80 built-ins declared below;
//! `aliasMap_` records device-specific name aliases so the same
//! call site can be resolved across SHF / HDAWG / UHF families.
//! Once built, `CustomFunctions::call(name, args, res)` is the
//! single entry point used by the SeqC AST evaluator: it checks
//! `aliasMap_` for ambiguous-overload reporting, looks `name` up in
//! `funcMap_`, falls back to `MathCompiler` for built-in scalar
//! math, and finally delegates to `generateWaveform` (which routes
//! the call through `generate()`) for waveform-generator builtins.
//!
//! Beyond the dispatch table the class accumulates the
//! cross-compilation state that built-ins need to coordinate:
//! `nodeMap_` / `nodeIndexMap_` / `nodeList_` track parameter-tree
//! node references (consumed by the host via
//! `Compiler::getNodeAccessList`), `accessModeMap_` records the
//! `Soft` / `Direct` / `Custom` access mode each node was used in,
//! `assignedWaveNames_` deduplicates `assignWaveIndex` registrations,
//! `usedFeatures_` collects feature tags surfaced in the JSON
//! output, and `externalTriggeringMode_` is the mutual-exclusion
//! latch that prevents mixing DIO and ZSync I/O families in one
//! program.  `mathCompiler_` is the embedded scalar-math
//! constant-folder consulted on each `call` for built-ins that have
//! a closed-form compile-time evaluation.  The `Compiler` and
//! `AWGCompilerImpl` `friend` declarations expose `resources_`,
//! `externalTriggeringMode_`, and `usedFeatures_` to the JSON
//! emission path that builds the compile-result metadata.
class CustomFunctions {
    friend class Compiler;  // Compiler::compile() directly writes resources_ (+0x10)
    friend class AWGCompilerImpl;  // getJsonVersion reads externalTriggeringMode_ and usedFeatures_ directly
public:
    // SubFunc — enum for play() / playIndexed() dispatch
    // Confirmed from binary: playWave passes 1, playWaveNow passes 3.
    enum class SubFunc : int {
        Prefetch   = 0,   // prefetch (prefetch-only path, no asmPlay)
        Default    = 1,   // playWave, playWaveIndexed
        Aux        = 2,   // playAuxWaveIndexed (via playIndexed)
        Now        = 3,   // playWaveNow, playWaveIndexedNow
        DigTrigger = 4,   // playWaveDigTrigger (via play)
        // Note: playAuxWave, playDIOWave, playWaveDIO, playWaveZSync
        // are complex wrappers that do NOT delegate to play()/playIndexed().
    };

    // --- Ctor / Dtor ---
    //! \brief Build the dispatch context, register every SeqC
    //! built-in into `funcMap_`, and capture the per-compilation
    //! configuration / resources.
    //!
    //! Stores raw pointers to `config` and `devConst`, moves the
    //! shared `wavetable`, `waveformGen`, and `asmCommands`
    //! singletons into the corresponding fields, copies
    //! `warningCb` into `warningCallback_`, allocates the private
    //! per-instance `WavetableFront` slot, default-constructs
    //! `mathCompiler_`, and inserts ~80 named built-ins (one
    //! `std::bind` entry per member function) into `funcMap_`.
    //! `nodeMap_` starts empty and is filled lazily on the first
    //! `lookupNode` call; `aliasMap_`, `unusedStringSet_B0_`,
    //! `nodeIndexMap_`, `accessModeMap_`, `nodeList_`,
    //! `assignedWaveNames_`, and `usedFeatures_` start empty;
    //! `externalTriggeringMode_` starts at `None`.
    //!
    //! \param config       Owning compilation's device config.
    //! \param devConst     Per-device constants table.
    //! \param wavetable    Shared front-side wavetable singleton.
    //! \param waveformGen  Shared waveform-generator singleton.
    //! \param asmCommands  Shared assembler-command registry.
    //! \param warningCb    Sink invoked when a built-in raises a
    //!                     non-fatal warning (length clamps,
    //!                     duplicate-CSV waveform names, etc.).
    CustomFunctions(AWGCompilerConfig const& config,
                    DeviceConstants const& devConst,
                    std::shared_ptr<WavetableFront> wavetable,
                    std::shared_ptr<WaveformGenerator> waveformGen,
                    std::shared_ptr<AsmCommands> asmCommands,
                    std::function<void(std::string const&)> const& warningCb);  // @0x12bcf0
    //! \brief Release every owned subsystem `shared_ptr`,
    //! `funcMap_` / `aliasMap_` entries, the lazy `NodeMap`, and
    //! all per-compilation tracking sets in reverse declaration
    //! order.
    ~CustomFunctions();  // @0x127c90

    // --- Public API ---
    //! \brief Test whether `name` is registered in `funcMap_`.
    //!
    //! \param name  Function name to look up.
    //! \return  `true` when `funcMap_` contains a binding for
    //!          `name`, `false` otherwise.  Does not consult
    //!          `aliasMap_` or the math compiler.
    bool functionExists(std::string const& name) const;  // @0x159410
    /*! \brief Resolve and invoke a SeqC function call, returning its
     *  evaluated result.
     *
     *  \details Three-stage dispatch:
     *    1. Look `name` up in `aliasMap_`.  When the alias entry
     *       carries one alias, format a `DeprecatedFunc`
     *       diagnostic and report it through `errorCallback_`;
     *       with two aliases, format the `DeprecatedFunc2`
     *       variant.  In the current binary `aliasMap_` is empty,
     *       so this branch never fires; the path exists for
     *       future built-ins that may need overload resolution.
     *    2. Look the (possibly aliased) name up in `funcMap_`.  On
     *       hit, invoke the bound `std::function` with `args` and
     *       `res` and return its `EvalResults`.
     *    3. On miss, query `mathCompiler_.functionExists(name,
     *       argCount, false)`.  When the math compiler claims the
     *       name, coerce every argument to `double` via
     *       `Value::toDouble`, call `mathCompiler_.call`, and wrap
     *       the scalar result in a fresh `EvalResults` whose value
     *       slot is set to `(VarType_Cvar, Value(double))`.
     *    4. On a final miss, delegate to
     *       `generateWaveform(name, args, res)`, which prepends
     *       `name` as the first argument and dispatches into
     *       `WaveformGenerator::call` for built-in waveform
     *       generators (`zeros`, `sin`, etc.).
     *
     *  Used by every SeqC function-call AST node during lowering;
     *  the returned `EvalResults` becomes the value of the call
     *  expression in the surrounding statement.
     *
     *  \param name  Function name as it appears in source.
     *  \param args  Already-evaluated arguments in source order.
     *  \param res   The current resource / variable environment
     *               (forwarded to whichever backend ultimately
     *               handles the call).
     *  \return      `EvalResults` produced by the resolved backend
     *               (`funcMap_`, `mathCompiler_`, or
     *               `generateWaveform`).
     *  \throws      Implementation-defined exceptions (e.g.
     *               `WaveformGeneratorValueException`,
     *               `CustomFunctionsValueException`,
     *               `ZIAWGCompilerException`) propagated from the
     *               resolved backend; this method itself does not
     *               raise.
     */
    std::shared_ptr<EvalResults> call(
        std::string const& name,
        std::vector<EvalResultValue> const& args,
        std::shared_ptr<Resources> res);                  // @0x159470

    // --- Utility methods ---
    //! \brief Verify that the current device's `deviceType` bit is
    //! covered by the `allowedDevTypes` mask, otherwise throw.
    //!
    //! `config_->deviceType` is a single power-of-two `AwgDeviceType`
    //! enumerator; `allowedDevTypes` is a bitmask of one or more
    //! enumerators OR'd together.  When the device's bit lies outside
    //! the mask the method formats `FuncNotSupported` (with the
    //! call-site `name` and the device's textual name from
    //! `AWGCompilerConfig::getAwgDeviceTypeString`) and raises a
    //! `CustomFunctionsException`.
    //!
    //! \param name             Calling built-in's name, embedded in
    //!                         the diagnostic.
    //! \param allowedDevTypes  Bitmask of permitted device types.
    //! \throws  `CustomFunctionsException` (`FuncNotSupported`)
    //!          when the current device is not in the allowed set.
    void checkFunctionSupported(std::string const& name, AwgDeviceType allowedDevTypes);  // @0x15aeb0

    //! \brief Throw when a triggered waveform is shorter than the
    //! device's minimum trigger-play sample length.
    //!
    //! No-op when `wf` is null.  Reads
    //! `devConst_->playMinSamples` and `wf->signal.length_`; if the
    //! wave has fewer samples, formats `MinWaveformLength` with the
    //! minimum-sample count as a string and raises
    //! `CustomFunctionsException`.
    //!
    //! \param wf  Waveform to validate; may be null.
    //! \throws  `CustomFunctionsException` (`MinWaveformLength`)
    //!          when `wf` is below the per-device trigger minimum.
    void checkWaveformMinLengthTrig(std::shared_ptr<WaveformFront> wf);           // @0x15b000

    //! \brief Clamp `length` to the device-specific minimum playback
    //! length and warn if clamping was needed.
    //!
    //! Compares `length` against `devConst_->maxWaveformLength`
    //! (treated as the minimum valid play length here per the binary
    //! convention).  When `length < min`, invokes `warningCallback_`
    //! with `PlayLenBelowMin` and returns `min`; otherwise returns
    //! `length` unchanged.
    //!
    //! \param length  Requested play length in samples.
    //! \return  `length`, or the clamped minimum when below it.
    int  checkPlayMinLength(int length);                                           // @0x15b100

    //! \brief Round `length` up to the next multiple of the device's
    //! sample-grain alignment and warn if rounding was needed.
    //!
    //! Reads `devConst_->grainSize` as the alignment.  When `length`
    //! is not a multiple of the grain, computes the next multiple,
    //! invokes `warningCallback_` with `PlayLenNotAligned`, and
    //! returns the rounded length; otherwise returns `length`
    //! unchanged.
    //!
    //! \param length  Requested play length in samples.
    //! \return  `length`, or the next-grain-aligned length.
    int  checkPlayAlignment(int length);                                           // @0x15b190

    //! \brief Warn when a named waveform is shorter than its
    //! requested length or is not grain-aligned.
    //!
    //! No-op when `wf` is null.  Two checks:
    //!   - When `expected > wf->signal.length_`, formats
    //!     `WaveformBelowMin` with the wave's name, actual length,
    //!     and the requested length, and warns via
    //!     `warningCallback_`.
    //!   - Otherwise, when `wf->signal.length_` is not a multiple of
    //!     `devConst_->grainSize`, computes the next multiple
    //!     (clamped at `wf->minLengthSamples`), formats
    //!     `WaveNotAligned`, and warns via `warningCallback_`.
    //!
    //! Both diagnostics are warnings, never exceptions.
    //!
    //! \param wf        Waveform whose CSV / placeholder length is
    //!                  being validated; may be null.
    //! \param expected  Requested length in samples.
    void checkOffspecWaveLength(std::shared_ptr<WaveformFront> wf, int expected);  // @0x15b370

    //! \brief Test whether `option` was passed in the compiler's
    //! include-paths / option list, recording it as a used feature
    //! when present.
    //!
    //! Linearly scans `config_->includePaths` for an exact match.
    //! On hit, inserts `option` into `usedFeatures_` (consumed by
    //! the JSON `required_options` array) and returns `true`; on
    //! miss returns `false` without modifying state.
    //!
    //! \param option  Option / feature tag to look up.
    //! \return  `true` when present, `false` otherwise.
    bool optionAvailable(std::string const& option);                               // @0x15b9c0

    //! \brief Test whether `mask` fits within the Grimsel device's
    //! oscillator-bit count.
    //!
    //! Returns `true` exactly when every bit set in `mask` lies
    //! within the low `devConst_->numDIOBits` bits.
    //!
    //! \param mask  Oscillator-selection bitmask.
    //! \return  `true` when `mask` fits, `false` otherwise.
    bool oscMaskCheckGrimsel(unsigned int mask);  // @0x15ba90

    //! \brief Test whether `mask` is a legal oscillator-selection
    //! for the Hirzel-family device given its channel grouping and
    //! the optional `MF` (multi-frequency) feature.
    //!
    //! Scans `config_->includePaths` for the literal `"MF"` and,
    //! when present, inserts it into `usedFeatures_` and uses the
    //! wider 16-bit oscillator window; otherwise the 4-bit window
    //! applies.  Within the chosen window the legal subset depends
    //! on `(numChannelGroups, awgIndex)` per the per-channel-group
    //! masks (e.g. group 0 takes the low half, group 1 the high
    //! half, etc.).  Issues a `LOG_WARNING` for invalid `awgIndex`
    //! values and returns `false`.
    //!
    //! \param mask  Oscillator-selection bitmask.
    //! \return  `true` when `mask` is legal for the current channel
    //!          group, `false` otherwise.
    bool oscMaskCheckHirzel(unsigned int mask);   // @0x15bab0

    //! \brief Compute the all-oscillators bitmask for the
    //! Hirzel-family device given its channel grouping.
    //!
    //! When `MF` is in `config_->includePaths`, inserts `"MF"` into
    //! `usedFeatures_` and selects from the 16-bit window:
    //! `0xFFFF` for `numChannelGroups==4`, `0xFF` shifted by
    //! `awgIndex*8` for `==2`, and `0xF` shifted by `awgIndex*4`
    //! for `==1`.  Without `MF`, selects from the 4-bit window:
    //! `0xF` for `==4`, `0x3` shifted by `awgIndex*2` for `==2`,
    //! `1u << awgIndex` for `==1`.  Returns `0` when the channel
    //! grouping is unrecognised.
    //!
    //! \return  Bitmask covering every oscillator owned by the
    //!          current channel group.
    unsigned int oscMaskSetAllHirzel();            // @0x15bf50

    //! \brief Compute the all-oscillators bitmask for the Grimsel
    //! device.
    //!
    //! Returns `(1u << devConst_->numDIOBits) - 1`, i.e. every bit
    //! up to (but not including) the device's DIO-bit count.
    //!
    //! \return  Mask with the low `numDIOBits` bits set.
    unsigned int oscMaskSetAllGrimsel();           // @0x15c0b0

    //! \brief Resolve a parameter-tree path string to its
    //! `NodeMapItem`, lazily initialising the device's node map on
    //! first use.
    //!
    //! Calls `initNodeMap()` to populate `nodeMap_` if needed, then
    //! delegates to `NodeMap::retrieve(path)`.  When `retrieve`
    //! returns a valid item, returns it by value (with the data
    //! pointer cloned via the `NodeMapData` vtable).  When the path
    //! is not found, formats `NodeNotExist` and raises
    //! `CustomFunctionsValueException` with `argIndex == 0`.
    //!
    //! \param path  Parameter-tree path to resolve.
    //! \return  The resolved `NodeMapItem`.
    //! \throws  `CustomFunctionsValueException` (`NodeNotExist`)
    //!          when `path` does not name a known node.
    NodeMapItem lookupNode(std::string const& path);  // @0x15c530

    //! \brief Record an access-mode use of `item`, adding it to the
    //! flat `nodeList_` on first encounter.
    //!
    //! Inserts `mode` into `accessModeMap_[item]`, then attempts to
    //! emplace `item → nodeList_.size()` into `nodeIndexMap_`.
    //! When the emplace inserts (the item was previously unseen),
    //! pushes a copy of `item` onto `nodeList_` so that subsequent
    //! `getNodeAddress` calls can return the stored index.
    //!
    //! \param item  Resolved parameter-tree node being accessed.
    //! \param mode  Access mode (`Soft`, `Direct`, or `Custom`).
    void addNodeAccess(NodeMapItem const& item, AccessMode mode);  // @0x15c6c0

    //! \brief Implement the `setWaitCyclesReg` SeqC built-in by
    //! emitting an `addi` (when needed) plus a `suser` to the
    //! legacy wait-cycle register.
    //!
    //! Only runs on devices in the supported-device bitmask
    //! (`HDAWG`, `UHFQA`, `SHFQA`, `SHFSG`, `SHFQC` SG,
    //! `SHFLI`, `VHFLI`, `GHFLI`); other devices skip the body and
    //! return without emission.  When `args.size() == 2` and
    //! `args[1]` is a `Var`, the existing register is reused;
    //! otherwise allocates a fresh register, emits
    //! `addi(reg, R0, Imm(value))`, and finally appends
    //! `suser(reg, kSuserWaitLegacy)` (`0x6F`).
    //!
    //! \param args     Argument vector: `[name, value]`.
    //! \param results  Output assemblers / node container.
    //! \param res      Resource scope (forwarded to the caller).
    void setWaitCyclesReg(std::vector<EvalResultValue> const& args,
                          std::shared_ptr<EvalResults> results,
                          std::shared_ptr<Resources> res);  // @0x15ca90

    //! \brief Merge a list of waveform-name arguments into a single
    //! `WaveformFront`, registering it with the wavetable and
    //! delegating to `WaveformGenerator::merge` /
    //! `interleave` / `grow` as needed.
    //!
    //! Iterates `args`, looking each named waveform up in
    //! `wavetableFront_->getWaveformSampleLength` and accumulating
    //! both the maximum observed length and a local
    //! `vector<Value>` of the names.  Throws
    //! `CustomFunctionsValueException` (`NoWaveformInFunc`) when
    //! the values vector is empty after the scan.  Appends a
    //! length-Value (either `requestedLength` when it exceeds the
    //! max or `0`), builds a `funDescr` name from the inputs,
    //! resolves or creates the merged waveform via
    //! `getWaveformByName` / `getWaveformByFunDescr` /
    //! `newWaveform`, then calls into `WaveformGenerator` to fill
    //! the sample buffer (`grow`, `merge`, `interleave`).
    //!
    //! \param args              Argument vector containing waveform
    //!                          names and (optionally) generator
    //!                          parameters.
    //! \param channelCount      Number of channels per group, used
    //!                          for interleaving.
    //! \param useYSuffix        When `true`, append a `_y` suffix
    //!                          to the generated name for the
    //!                          quadrature half of the waveform.
    //! \param callerName        Name of the calling built-in,
    //!                          embedded in error messages.
    //! \param requestedLength   Caller-requested sample length;
    //!                          used as the merged length when it
    //!                          exceeds every input.
    //! \param useFunDescrPath   When `true`, look the merged
    //!                          waveform up by function descriptor
    //!                          rather than by name.
    //! \return  The merged `WaveformFront`, registered with the
    //!          wavetable.
    //! \throws  `CustomFunctionsValueException` (`NoWaveformInFunc`)
    //!          when no waveform names were collected.
    //! \throws  `CustomFunctionsException` (`ChannelCountMismatch`)
    //!          when the inputs disagree on channel count.
    std::shared_ptr<WaveformFront> mergeWaveforms(
                        std::vector<EvalResultValue> const& args,
                        short channelCount, bool useYSuffix,
                        std::string const& callerName,
                        int requestedLength, bool useFunDescrPath);  // @0x15e060
                        // Mangled: ...sbRK<string>ib (last param is bool, not int64_t)

    //! \brief Implement `playWave` / `playWaveNow` /
    //! `playWaveDigTrigger` / prefetch-only dispatch.
    //!
    //! `subFunc` selects the command name (`"playWave"`,
    //! `"playWaveNow"`, `"playWaveDigTrigger"`, or `"prefetch"`)
    //! and minor behaviour tweaks.  The body:
    //!   1. Validates `args` is non-empty.
    //!   2. For `DigTrigger`, extracts the leading const-int play
    //!      length (must be ≥ 3) and strips it from a working copy
    //!      of `args`.
    //!   3. Constructs a `PlayArgs(*config_, wavetableFront_,
    //!      warningCallback_, cmdName, /*indexed=*/false)`,
    //!      invokes `parse()`, then `parseOptionalRate()` for the
    //!      tail rate argument.
    //!   4. Returns immediately when `PlayArgs::hasMarker_` is set
    //!      (function-body / first-pass evaluation context).
    //!   5. For each channel group, calls `mergeWaveforms` to
    //!      combine the assignments, then runs
    //!      `checkWaveformMinLengthTrig` (DigTrigger only) and
    //!      `checkOffspecWaveLength`.
    //!   6. Emits `asmPrefetch` (Cervino) and `asmPlay` (all paths
    //!      except prefetch-only on non-Hirzel, which throws
    //!      `0xa5`); for Hirzel `prefetch`, emits `asmPrefetch`
    //!      only.
    //!
    //! \param args     SeqC call arguments in source order.
    //! \param res      Current resource scope.
    //! \param subFunc  Selects the command variant.
    //! \return  `EvalResults` whose `assemblers_` / `node_` slots
    //!          carry the emitted instructions.
    //! \throws  `CustomFunctionsException` on argument-count, type,
    //!          or device-support errors.
    std::shared_ptr<EvalResults> play(
        std::vector<EvalResultValue> const& args,
        std::shared_ptr<Resources> res,
        SubFunc subFunc);  // @0x15f090

    //! \brief Implement `playWaveIndexed` / `playAuxWaveIndexed` /
    //! `playWaveIndexedNow`.
    //!
    //! Mirrors `play()` but threads a runtime wave-index through
    //! the emitted assembly:
    //!   1. Selects the command name from `subFunc`
    //!      (1=`playWaveIndexed`, 2=`playAuxWaveIndexed`,
    //!      3=`playWaveIndexedNow`); unknown values log a warning
    //!      and continue with an empty name.
    //!   2. Requires `args.size() >= 3` (`FuncMinArgs` otherwise).
    //!   3. Constructs `PlayArgs` with `indexed = (subFunc ==
    //!      Aux)`, parses the channel-assignment block, and reads
    //!      the trailing optional rate.
    //!   4. Per channel: gathers args, requests the wave's sample
    //!      length, falls back to `WaveformGenerator::call("zeros",
    //!      ...)` for empty channels, and merges via
    //!      `mergeWaveforms`.
    //!   5. Emits an `addi(indexReg, R0, waveIndex)` to load the
    //!      wave-index argument followed by `asmPlay`.
    //!
    //! \param args     SeqC call arguments in source order.
    //! \param res      Current resource scope.
    //! \param subFunc  Selects the indexed command variant.
    //! \return  `EvalResults` whose `assemblers_` / `node_` slots
    //!          carry the emitted instructions.
    //! \throws  `CustomFunctionsException` on argument-count, type,
    //!          or device-support errors.
    std::shared_ptr<EvalResults> playIndexed(
        std::vector<EvalResultValue> const& args,
        std::shared_ptr<Resources> res,
        SubFunc subFunc);  // @0x160e00

    //! \brief Convert a sample count and rate divisor into the
    //! cycle count consumed by the device's wait machine.
    //!
    //! Clamps `rate` to non-negative.  On HDAWG, computes
    //! `max(((samples + 7) << rate) >> 3, 4) - 3`; on every other
    //! device, computes `((samples + 3) << rate) >> 2`.
    //!
    //! \param samples  Sample count to wait for.
    //! \param rate     `playRate` divisor (positive: divide clock).
    //! \return  Equivalent wait-machine cycle count.
    int getWaitTime(int samples, int rate);  // @0x163930

    //! \brief Resolve a parameter-tree write to one of the
    //! supported per-channel sub-paths and emit the assembly
    //! sequence that performs the write.
    //!
    //! Steps:
    //!   1. Stops with an empty result when `path.varSubType_ ==
    //!      FunctionArg` (function-body / first-pass context).
    //!   2. Strips an optional absolute-device prefix
    //!      `"/<deviceId>/..."` (regex `absDevRegex`) and
    //!      validates the device id against
    //!      `config_->deviceIndex`; mismatches return an empty
    //!      result.
    //!   3. Calls `lookupNode(pathStr)` for the `NodeMapItem`.
    //!   4. Tries `awgs/<channel>/...` (`awgNodeRegex`) and
    //!      `sines/<oscIdx>/...` (`sineNodeRegex`); on match,
    //!      converts the index to the AWG-relative form and throws
    //!      `SequencerCantDrive` when it doesn't equal
    //!      `config_->awgIndex`.
    //!   5. For `sines/<idx>/oscselect` (`oscselNodeRegex`),
    //!      records the `"MF"` feature and dispatches into the
    //!      per-`NodeMapType` codegen jump tables.
    //!   6. Calls `addNodeAccess(node, AccessMode(node.hasFast))`
    //!      and emits the resolved address and `val`-typed payload
    //!      via a sequence of `addi` / `suser` instructions appended
    //!      to the returned `EvalResults`.
    //!
    //! \param path  Parameter-tree path argument (string-typed
    //!              `EvalResultValue`).
    //! \param val   Value to write (typed `EvalResultValue`).
    //! \param type  Type hint argument from the SeqC call.
    //! \param res   Current resource scope.
    //! \return  `EvalResults` containing the emitted assembly, or
    //!          an empty result when the write is silently
    //!          skipped.
    //! \throws  `CustomFunctionsValueException` (`SequencerCantDrive`)
    //!          when the path targets a different sequencer than the
    //!          current one.
    std::shared_ptr<EvalResults> writeToNode(EvalResultValue path, EvalResultValue val,
                                              EvalResultValue type,
                                              std::shared_ptr<Resources> res);  // @0x164550

    //! \brief Lazily populate `nodeMap_` for the current device
    //! type.  Subsequent calls are no-ops.
    void initNodeMap();  // @0x16b740

    //! \brief Return the runtime address used to drive `item` from
    //! generated assembly.
    //!
    //! Fast path: when `item.data` is a `DirectAddrNodeMapData`,
    //! returns its embedded `addr_`.  Slow path: returns
    //! `nodeIndexMap_.at(item)` (the index into `nodeList_`
    //! recorded by `addNodeAccess`).
    //!
    //! \param item  Resolved parameter-tree node.
    //! \return  Direct address (fast) or `nodeList_` index (slow).
    //! \throws  `std::out_of_range` when `item` was never registered
    //!          via `addNodeAccess` and is not direct-addressed.
    uint32_t getNodeAddress(NodeMapItem const& item) const;  // @0x16ba10

    //! \brief Return the device's effective sample-clock frequency
    //! in Hertz.
    //!
    //! When `resources_` carries a `DEVICE_SAMPLE_RATE` constant,
    //! returns its `double` value; otherwise falls back to
    //! `devConst_->samplingRate`.
    //!
    //! \return  Sample clock in Hz.
    double getSampleClock() const;  // @0x16ba80

    //! \brief Append the device-appropriate cross-AWG sync
    //! instruction (`asmSyncHirzel` for HDAWG, placeholder
    //! `asmSyncPlaceholderCervino` for UHFLI) to the result
    //! assembler list.  No-op for other devices.
    //!
    //! For UHFLI the placeholder `node` is also propagated into
    //! `results->node_` so a later pass can resolve the actual
    //! sync target.
    //!
    //! \param results  Output assembler / node container.
    //! \param res      Current resource scope (forwarded).
    void addSyncCommand(std::shared_ptr<EvalResults> results,
                        std::shared_ptr<Resources> res);  // @0x16bb30

    //! \brief Return the set of `AccessMode`s recorded for `item`
    //! by previous `addNodeAccess` calls.
    //!
    //! \param item  Resolved parameter-tree node.
    //! \return  Reference to the live set of access modes.
    //! \throws  `std::out_of_range` when `item` was never recorded.
    std::set<AccessMode> const& getAccessModes(NodeMapItem const& item) const;  // @0x16be50

    //! \brief Implement the SeqC `printf`-style built-in.
    //!
    //! The first `args` entry is the format string.  Remaining
    //! entries are fed to a `boost::format` formatter:
    //! `String` arguments are forwarded as `std::string`,
    //! `Const` / `Cvar` numeric arguments are forwarded as `long
    //! long` when bit-identical to their rounded value and as
    //! `double` otherwise; any other type raises
    //! `CustomFunctionsValueException` (`FuncInvalidArgType`).
    //! Empty `args` raises `CustomFunctionsException`
    //! (`FuncSingleArg`).  `boost::io::too_few_args` is converted
    //! to `FormatMoreArgs`; `too_many_args` to
    //! `FormatCantInterpret`.
    //!
    //! \param args  Format string followed by interpolation
    //!              arguments.
    //! \param fmt   Calling built-in's name, embedded in
    //!              error messages.  (Despite the parameter
    //!              name, this is the function-call name from
    //!              source, not the format string itself —
    //!              `args[0]` carries the format string.)
    //! \return  The formatted string.
    //! \throws  `CustomFunctionsException` for missing arguments or
    //!          format-arity mismatches.
    //! \throws  `CustomFunctionsValueException` for invalid
    //!          per-argument types or excess arguments.
    std::string printF(std::vector<EvalResultValue> const& args,
                       std::string const& fmt);  // @0x16c470

    //! \brief Emit `addi(reg, R0, cycles)` followed by
    //! `suser(reg, kSuserWaitCycles)` (`0x69`) into `results`,
    //! where `reg` is a freshly allocated scratch register.
    //!
    //! \param cycles   Wait-cycle count to load.
    //! \param results  Output assembler container.
    //! \param res      Current resource scope (forwarded).
    void addWaitCycles(int cycles,
                       std::shared_ptr<EvalResults> results,
                       std::shared_ptr<Resources> res);  // @0x16d8c0

    //! \brief Split a 64-bit `value` into two 32-bit halves and
    //! emit them as a sequence of
    //! `addi(reg, R0, low) ; suser(reg, reg1) ;
    //!  addi(reg, R0, high) ; suser(reg, reg2)` instructions.
    //!
    //! \param value    64-bit payload to write.
    //! \param reg1     User-register address receiving the low 32
    //!                 bits.
    //! \param reg2     User-register address receiving the high 32
    //!                 bits.
    //! \param results  Output assembler container.
    //! \param res      Current resource scope (forwarded).
    void writeLS64bit(unsigned long value, int reg1, int reg2,
                      std::shared_ptr<EvalResults> results,
                      std::shared_ptr<Resources> res);  // @0x16dbb0

    //! \brief Forward an unknown function call into the waveform
    //! generator, prepending `name` as a string-typed first
    //! argument.
    //!
    //! Copies `args`, inserts a synthesised `EvalResultValue`
    //! containing `name` at the front, and dispatches to
    //! `generate(newArgs, res)` (the internal back-end that
    //! resolves built-in waveform constructors such as `zeros`,
    //! `sin`, `gauss`).  Re-throws
    //! `CustomFunctionsValueException` and
    //! `CustomFunctionsException` after a fresh
    //! allocation so that the call site sees a clean exception
    //! object.
    //!
    //! \param name  Function name from the SeqC source.
    //! \param args  Already-evaluated arguments.
    //! \param res   Current resource scope.
    //! \return  `EvalResults` from the waveform generator.
    //! \throws  `CustomFunctionsValueException` or
    //!          `CustomFunctionsException` propagated from
    //!          `generate()`.
    std::shared_ptr<EvalResults> generateWaveform(
        std::string const& name,
        std::vector<EvalResultValue> const& args,
        std::shared_ptr<Resources> res);  // @0x15a9f0

    // --- SeqC built-in function implementations ---
    // All share signature: shared_ptr<EvalResults>(vector<EvalResultValue> const&, shared_ptr<Resources>)
    // Registered in funcMap_ during construction. Bodies stubbed with addresses.

    //! \brief Implement the SeqC `setDIO` built-in: write a single
    //! value to the device's DIO output bus.
    //!
    //! Latches the DIO/ZSync mutual-exclusion mode to `Dio` (throws
    //! if already set to `ZSync`).  Requires exactly one argument.
    //! When the argument is `Var`, emits `sdio(reg, isShf)`
    //! directly using its register; when `Const` / `Cvar`,
    //! allocates a scratch register, emits
    //! `addi32(reg, R0, Imm(value))` followed by
    //! `sdio(reg, isShf)`.  On non-SHF devices, also looks up
    //! `_/dios/0/output` and records `Custom` access (silently
    //! ignored when the node is absent for the current device).
    //!
    //! \param args  Single-element argument list with the value
    //!              to write.
    //! \param res   Current resource scope (unused).
    //! \return  `EvalResults` (`VarType_Void`) carrying the emitted
    //!          assembly.
    //! \throws  `CustomFunctionsException` (`FuncExpectsSingleArg`)
    //!          on argument-count or type mismatch.
    std::shared_ptr<EvalResults> setDIO(std::vector<EvalResultValue> const& args, std::shared_ptr<Resources> res);                // @0x130780

    //! \brief Implement the SeqC `getDIO` built-in: read the
    //! device's DIO input bus into a fresh register.
    //!
    //! Latches the DIO/ZSync mutual-exclusion mode to `Dio`.
    //! Requires zero arguments.  Allocates a register, emits
    //! `ldio(reg, isShf)`, and returns an `EvalResults` whose
    //! value slot is `(VarType_Var, regNum)`.
    //!
    //! \param args  Must be empty.
    //! \param res   Current resource scope (unused).
    //! \return  `EvalResults` whose value slot is the new DIO read
    //!          register.
    //! \throws  `CustomFunctionsException` (`FuncExpectsNoArgs`)
    //!          when `args` is non-empty.
    std::shared_ptr<EvalResults> getDIO(std::vector<EvalResultValue> const& args, std::shared_ptr<Resources> res);                // @0x131040

    //! \brief Implement the SeqC `getDIOTriggered` built-in: read
    //! the trigger-latched DIO input value.
    //!
    //! Latches the DIO/ZSync mutual-exclusion mode to `Dio`.
    //! Requires zero arguments.  Allocates a register, emits
    //! `ldiotrig(reg)`, and returns an `EvalResults` whose value
    //! slot is `(VarType_Var, regNum)`.
    //!
    //! \param args  Must be empty.
    //! \param res   Current resource scope (unused).
    //! \return  `EvalResults` whose value slot is the new
    //!          trigger-latched DIO register.
    //! \throws  `CustomFunctionsException` (`FuncExpectsNoArgs`)
    //!          when `args` is non-empty.
    std::shared_ptr<EvalResults> getDIOTriggered(std::vector<EvalResultValue> const& args, std::shared_ptr<Resources> res);       // @0x131410

    //! \brief Implement the SeqC `getZSyncData` built-in: read a
    //! ZSync-bus payload into a fresh register, dispatching on
    //! which `ZSYNC_DATA_*` constant the caller named.
    //!
    //! Calls `checkFunctionSupported("getZSyncData", kDevAllButUHF)`
    //! and latches the DIO/ZSync mode to `ZSync`.  UHFQA requires
    //! exactly one argument; every other supported device accepts
    //! one or two (the optional second is forwarded into
    //! `setWaitCyclesReg`).  The first argument is matched against
    //! `ZSYNC_DATA_RAW`, and on HDAWG / SHFSG / SHFQC_SG / SHFLI /
    //! GHFLI / VHFLI also against `ZSYNC_DATA_PROCESSED_A` and
    //! `ZSYNC_DATA_PROCESSED_B`.  An unmatched value raises
    //! `CustomFunctionsException` (`InvalidZSyncData`).  Emits
    //! `ld(reg, 0x6a)` for SHFQA, `ldiotrig` for `ZSYNC_DATA_RAW`,
    //! `ld(reg, 0x6b)` for `..._PROCESSED_A`, and
    //! `ld(reg, 0x6c)` for `..._PROCESSED_B`.
    //!
    //! \param args  ZSync-data variant constant, optionally
    //!              followed by the wait-cycles argument.
    //! \param res   Resource scope used to read the variant
    //!              constants and to thread `setWaitCyclesReg`.
    //! \return  `EvalResults` whose value slot is the new ZSync
    //!          read register.
    //! \throws  `CustomFunctionsException` on argument-count or
    //!          unknown-variant errors.
    std::shared_ptr<EvalResults> getZSyncData(std::vector<EvalResultValue> const& args, std::shared_ptr<Resources> res);          // @0x1316f0

    //! \brief Implement the SeqC `getFeedback` built-in: read a
    //! ZSync- or QA-feedback payload into a fresh register.
    //!
    //! Same dispatch shape as `getZSyncData` but additionally
    //! recognises the QA-feedback constants `QA_DATA_RAW` and
    //! `QA_DATA_PROCESSED_D` on `SHFQC_SG`.  Emits
    //! `ld(reg, 0xc0)` for `QA_DATA_RAW` and
    //! `ld(reg, 0xc1)` for `QA_DATA_PROCESSED_D`; the rest of the
    //! emit table mirrors `getZSyncData`.
    //!
    //! \param args  Feedback-variant constant, optionally
    //!              followed by the wait-cycles argument.
    //! \param res   Resource scope used to read the variant
    //!              constants and to thread `setWaitCyclesReg`.
    //! \return  `EvalResults` whose value slot is the new feedback
    //!          read register.
    //! \throws  `CustomFunctionsException` on argument-count or
    //!          unknown-variant errors.
    std::shared_ptr<EvalResults> getFeedback(std::vector<EvalResultValue> const& args, std::shared_ptr<Resources> res);           // @0x132420
    std::shared_ptr<EvalResults> setID(std::vector<EvalResultValue> const& args, std::shared_ptr<Resources> res);                 // @0x1334a0
    std::shared_ptr<EvalResults> assignWaveIndex(std::vector<EvalResultValue> const& args, std::shared_ptr<Resources> res);       // @0x133c40
    std::shared_ptr<EvalResults> prefetch(std::vector<EvalResultValue> const& args, std::shared_ptr<Resources> res);              // @0x1351d0
    std::shared_ptr<EvalResults> prefetchIndexed(std::vector<EvalResultValue> const& args, std::shared_ptr<Resources> res);       // @0x135290
    //! \brief Implement the SeqC `playWave` built-in.
    //!
    //! Verifies the device is in `kDevHirzelAll` and forwards to
    //! `play(args, res, SubFunc::Default)`.
    //!
    //! \param args  SeqC call arguments in source order.
    //! \param res   Current resource scope.
    //! \return  The `EvalResults` produced by `play()`.
    //! \throws  `CustomFunctionsException` when the device is not
    //!          supported, plus anything propagated from `play()`.
    std::shared_ptr<EvalResults> playWave(std::vector<EvalResultValue> const& args, std::shared_ptr<Resources> res);              // @0x1352f0

    //! \brief Implement the SeqC `playWaveNow` built-in.
    //!
    //! Verifies device support against the
    //! `HDAWG | UHFAWG` mask and forwards to
    //! `play(args, res, SubFunc::Now)` so the underlying `asmPlay`
    //! is emitted with the immediate-play flag set.
    //!
    //! \param args  SeqC call arguments in source order.
    //! \param res   Current resource scope.
    //! \return  The `EvalResults` produced by `play()`.
    //! \throws  `CustomFunctionsException` when the device is not
    //!          supported, plus anything propagated from `play()`.
    std::shared_ptr<EvalResults> playWaveNow(std::vector<EvalResultValue> const& args, std::shared_ptr<Resources> res);           // @0x1353b0

    //! \brief Implement the SeqC `playWaveIndexed` built-in.
    //!
    //! Rejects HDAWG with `DeprecatedFuncFifo` (HDAWG uses the
    //! FIFO architecture, which deprecates indexed playback at
    //! source level), then verifies device support against the
    //! `HDAWG | UHFAWG` mask and forwards to
    //! `playIndexed(args, res, SubFunc::Default)`.
    //!
    //! \param args  SeqC call arguments in source order.
    //! \param res   Current resource scope.
    //! \return  The `EvalResults` produced by `playIndexed()`.
    //! \throws  `CustomFunctionsException` (`DeprecatedFuncFifo`)
    //!          on HDAWG, or unsupported-device, plus anything
    //!          propagated from `playIndexed()`.
    std::shared_ptr<EvalResults> playWaveIndexed(std::vector<EvalResultValue> const& args, std::shared_ptr<Resources> res);       // @0x135480

    //! \brief Implement the SeqC `playWaveIndexedNow` built-in.
    //!
    //! Verifies device support against the `HDAWG | UHFAWG` mask
    //! and forwards to `playIndexed(args, res, SubFunc::Now)`.
    //!
    //! \param args  SeqC call arguments in source order.
    //! \param res   Current resource scope.
    //! \return  The `EvalResults` produced by `playIndexed()`.
    //! \throws  `CustomFunctionsException` when the device is not
    //!          supported, plus anything propagated from
    //!          `playIndexed()`.
    std::shared_ptr<EvalResults> playWaveIndexedNow(std::vector<EvalResultValue> const& args, std::shared_ptr<Resources> res);    // @0x135550
    std::shared_ptr<EvalResults> playAuxWave(std::vector<EvalResultValue> const& args, std::shared_ptr<Resources> res);           // @0x135610

    //! \brief Implement the SeqC `playAuxWaveIndexed` built-in.
    //!
    //! Verifies device support against the `HDAWG | UHFAWG` mask
    //! and forwards to `playIndexed(args, res, SubFunc::Aux)`,
    //! which selects the auxiliary-wave variant of indexed
    //! playback.
    //!
    //! \param args  SeqC call arguments in source order.
    //! \param res   Current resource scope.
    //! \return  The `EvalResults` produced by `playIndexed()`.
    //! \throws  `CustomFunctionsException` when the device is not
    //!          supported, plus anything propagated from
    //!          `playIndexed()`.
    std::shared_ptr<EvalResults> playAuxWaveIndexed(std::vector<EvalResultValue> const& args, std::shared_ptr<Resources> res);    // @0x136930
    std::shared_ptr<EvalResults> playDIOWave(std::vector<EvalResultValue> const& args, std::shared_ptr<Resources> res);           // @0x1369f0
    std::shared_ptr<EvalResults> playWaveDIO(std::vector<EvalResultValue> const& args, std::shared_ptr<Resources> res);           // @0x137740
    std::shared_ptr<EvalResults> playWaveZSync(std::vector<EvalResultValue> const& args, std::shared_ptr<Resources> res);         // @0x137a50

    //! \brief Implement the SeqC `playWaveDigTrigger` built-in.
    //!
    //! Verifies device support against the `HDAWG | UHFAWG` mask
    //! and forwards to `play(args, res, SubFunc::DigTrigger)`,
    //! which expects a leading const-int play length (≥ 3) and
    //! routes the play through the digital-trigger path.
    //!
    //! \param args  SeqC call arguments in source order.
    //! \param res   Current resource scope.
    //! \return  The `EvalResults` produced by `play()`.
    //! \throws  `CustomFunctionsException` when the device is not
    //!          supported, plus anything propagated from `play()`
    //!          (including invalid-leading-arg errors).
    std::shared_ptr<EvalResults> playWaveDigTrigger(std::vector<EvalResultValue> const& args, std::shared_ptr<Resources> res);    // @0x1386a0
    std::shared_ptr<EvalResults> playZero(std::vector<EvalResultValue> const& args, std::shared_ptr<Resources> res);              // @0x1387f0
    std::shared_ptr<EvalResults> playHold(std::vector<EvalResultValue> const& args, std::shared_ptr<Resources> res);              // @0x139030
    std::shared_ptr<EvalResults> wait(std::vector<EvalResultValue> const& args, std::shared_ptr<Resources> res);                  // @0x139760
    std::shared_ptr<EvalResults> waitWave(std::vector<EvalResultValue> const& args, std::shared_ptr<Resources> res);              // @0x13a980
    std::shared_ptr<EvalResults> waitTrigger(std::vector<EvalResultValue> const& args, std::shared_ptr<Resources> res);           // @0x13abf0
    std::shared_ptr<EvalResults> waitAnaTrigger(std::vector<EvalResultValue> const& args, std::shared_ptr<Resources> res);        // @0x13b4b0
    std::shared_ptr<EvalResults> waitDigTrigger(std::vector<EvalResultValue> const& args, std::shared_ptr<Resources> res);        // @0x13c110
    std::shared_ptr<EvalResults> waitOnGrid(std::vector<EvalResultValue> const& args, std::shared_ptr<Resources> res);            // @0x13d000
    std::shared_ptr<EvalResults> waitOnSync(std::vector<EvalResultValue> const& args, std::shared_ptr<Resources> res);            // @0x13d3a0
    std::shared_ptr<EvalResults> waitDIOTrigger(std::vector<EvalResultValue> const& args, std::shared_ptr<Resources> res);        // @0x13d630
    std::shared_ptr<EvalResults> waitZSyncTrigger(std::vector<EvalResultValue> const& args, std::shared_ptr<Resources> res);      // @0x13dcf0
    std::shared_ptr<EvalResults> waitCntTrigger(std::vector<EvalResultValue> const& args, std::shared_ptr<Resources> res);        // @0x13e460
    std::shared_ptr<EvalResults> waitDemodOscPhase(std::vector<EvalResultValue> const& args, std::shared_ptr<Resources> res);     // @0x13eba0
    std::shared_ptr<EvalResults> waitSineOscPhase(std::vector<EvalResultValue> const& args, std::shared_ptr<Resources> res);      // @0x13f790
    std::shared_ptr<EvalResults> waitTimestamp(std::vector<EvalResultValue> const& args, std::shared_ptr<Resources> res);         // @0x1401c0
    std::shared_ptr<EvalResults> resetOscPhase(std::vector<EvalResultValue> const& args, std::shared_ptr<Resources> res);         // @0x1403b0
    std::shared_ptr<EvalResults> setSinePhase(std::vector<EvalResultValue> const& args, std::shared_ptr<Resources> res);          // @0x141df0
    std::shared_ptr<EvalResults> incrementSinePhase(std::vector<EvalResultValue> const& args, std::shared_ptr<Resources> res);    // @0x142da0
    std::shared_ptr<EvalResults> waitDemodSample(std::vector<EvalResultValue> const& args, std::shared_ptr<Resources> res);       // @0x143d50
    std::shared_ptr<EvalResults> waitPlayQueueEmpty(std::vector<EvalResultValue> const& args, std::shared_ptr<Resources> res);    // @0x145240
    std::shared_ptr<EvalResults> setTrigger(std::vector<EvalResultValue> const& args, std::shared_ptr<Resources> res);            // @0x1454c0
    std::shared_ptr<EvalResults> getTrigger(std::vector<EvalResultValue> const& args, std::shared_ptr<Resources> res);            // @0x145ad0
    std::shared_ptr<EvalResults> setInternalTrigger(std::vector<EvalResultValue> const& args, std::shared_ptr<Resources> res);    // @0x146140
    std::shared_ptr<EvalResults> getAnaTrigger(std::vector<EvalResultValue> const& args, std::shared_ptr<Resources> res);         // @0x146740
    std::shared_ptr<EvalResults> getDigTrigger(std::vector<EvalResultValue> const& args, std::shared_ptr<Resources> res);         // @0x147420
    std::shared_ptr<EvalResults> setInt(std::vector<EvalResultValue> const& args, std::shared_ptr<Resources> res);                // @0x1480d0
    std::shared_ptr<EvalResults> setDouble(std::vector<EvalResultValue> const& args, std::shared_ptr<Resources> res);             // @0x148ac0
    std::shared_ptr<EvalResults> randomSeed(std::vector<EvalResultValue> const& args, std::shared_ptr<Resources> res);            // @0x1497c0
    std::shared_ptr<EvalResults> generate(std::vector<EvalResultValue> const& args, std::shared_ptr<Resources> res);              // @0x149940
    std::shared_ptr<EvalResults> setUserReg(std::vector<EvalResultValue> const& args, std::shared_ptr<Resources> res);            // @0x14a420
    std::shared_ptr<EvalResults> getUserReg(std::vector<EvalResultValue> const& args, std::shared_ptr<Resources> res);            // @0x14b480
    std::shared_ptr<EvalResults> getSweeperLength(std::vector<EvalResultValue> const& args, std::shared_ptr<Resources> res);      // @0x14bca0
    std::shared_ptr<EvalResults> setRate(std::vector<EvalResultValue> const& args, std::shared_ptr<Resources> res);               // @0x14c370
    std::shared_ptr<EvalResults> setPrecompClear(std::vector<EvalResultValue> const& args, std::shared_ptr<Resources> res);       // @0x14c720
    std::shared_ptr<EvalResults> setWaveDIO(std::vector<EvalResultValue> const& args, std::shared_ptr<Resources> res);            // @0x14cae0
    std::shared_ptr<EvalResults> now(std::vector<EvalResultValue> const& args, std::shared_ptr<Resources> res);                   // @0x14cbc0
    std::shared_ptr<EvalResults> at(std::vector<EvalResultValue> const& args, std::shared_ptr<Resources> res);                    // @0x14ce30
    std::shared_ptr<EvalResults> error(std::vector<EvalResultValue> const& args, std::shared_ptr<Resources> res);                 // @0x14d830
    std::shared_ptr<EvalResults> info(std::vector<EvalResultValue> const& args, std::shared_ptr<Resources> res);                  // @0x14da50
    std::shared_ptr<EvalResults> lock(std::vector<EvalResultValue> const& args, std::shared_ptr<Resources> res);                  // @0x14dc70
    std::shared_ptr<EvalResults> unlock(std::vector<EvalResultValue> const& args, std::shared_ptr<Resources> res);                // @0x14e180
    std::shared_ptr<EvalResults> sync(std::vector<EvalResultValue> const& args, std::shared_ptr<Resources> res);                  // @0x14e690
    std::shared_ptr<EvalResults> getCnt(std::vector<EvalResultValue> const& args, std::shared_ptr<Resources> res);                // @0x14e8d0
    std::shared_ptr<EvalResults> waitQAResultTrigger(std::vector<EvalResultValue> const& args, std::shared_ptr<Resources> res);   // @0x14edc0
    std::shared_ptr<EvalResults> getQAResult(std::vector<EvalResultValue> const& args, std::shared_ptr<Resources> res);           // @0x14f380
    std::shared_ptr<EvalResults> startQAResult(std::vector<EvalResultValue> const& args, std::shared_ptr<Resources> res);         // @0x14f620
    std::shared_ptr<EvalResults> startQAMonitor(std::vector<EvalResultValue> const& args, std::shared_ptr<Resources> res);        // @0x1500b0
    std::shared_ptr<EvalResults> executeTableEntry(std::vector<EvalResultValue> const& args, std::shared_ptr<Resources> res);     // @0x150900
    std::shared_ptr<EvalResults> setPRNGSeed(std::vector<EvalResultValue> const& args, std::shared_ptr<Resources> res);           // @0x1513e0
    std::shared_ptr<EvalResults> getPRNGValue(std::vector<EvalResultValue> const& args, std::shared_ptr<Resources> res);          // @0x151a70
    std::shared_ptr<EvalResults> setPRNGRange(std::vector<EvalResultValue> const& args, std::shared_ptr<Resources> res);          // @0x151ce0
    std::shared_ptr<EvalResults> startQA(std::vector<EvalResultValue> const& args, std::shared_ptr<Resources> res);               // @0x152690
    std::shared_ptr<EvalResults> resetRTLoggerTimestamp(std::vector<EvalResultValue> const& args, std::shared_ptr<Resources> res); // @0x153f90
    std::shared_ptr<EvalResults> configFreqSweep(std::vector<EvalResultValue> const& args, std::shared_ptr<Resources> res);       // @0x154240
    std::shared_ptr<EvalResults> setSweepStep(std::vector<EvalResultValue> const& args, std::shared_ptr<Resources> res);          // @0x155640
    std::shared_ptr<EvalResults> setOscFreq(std::vector<EvalResultValue> const& args, std::shared_ptr<Resources> res);            // @0x156a70
    std::shared_ptr<EvalResults> configureFeedbackProcessing(std::vector<EvalResultValue> const& args, std::shared_ptr<Resources> res);  // @0x157e60

    // --- Accessors for Compiler::getNodeAccessList / getNodeToModeMap ---
    // The binary accesses these via raw pointer arithmetic (this+0x150, this+0x128).
    const std::vector<NodeMapItem>& nodeList() const { return nodeList_; }
    const std::unordered_map<NodeMapItem, std::set<AccessMode>>& accessModeMap() const { return accessModeMap_; }

private:
    // --- Fields (see layout table above) ---
    AWGCompilerConfig const*                           config_;                 // +0x00
    DeviceConstants const*                             devConst_;               // +0x08
    std::shared_ptr<Resources>                         resources_;              // +0x10
    std::shared_ptr<WavetableFront>                    wavetableFront_;         // +0x20
    std::shared_ptr<WavetableFront>                    wavetableFrontPrivate_;  // +0x30
    std::shared_ptr<WaveformGenerator>                 waveformGen_;            // +0x40
    std::shared_ptr<AsmCommands>                       asmCommands_;            // +0x50

    // Function dispatch map: function name → bound member function
    using FuncType = std::function<std::shared_ptr<EvalResults>(
        std::vector<EvalResultValue> const&, std::shared_ptr<Resources>)>;
    std::unordered_map<std::string, FuncType>          funcMap_;                // +0x60

    // Binary: On libc++, unordered_map is 0x28 bytes (0x20 hash table + 4B max_load_factor + 4B pad).
    // The floats below are the trailing max_load_factor of the preceding map.
    float                                              funcMap_maxLoadFactor_{1.0f}; // +0x80  (libc++ internal)
    char                                               pad_84_[4];             // +0x84

    std::unordered_map<std::string, std::vector<std::string>> aliasMap_;         // +0x88

    float                                              aliasMap_maxLoadFactor_{1.0f}; // +0xA8  (libc++ internal)
    char                                               pad_AC_[4];             // +0xAC

    // +0xB0: set<string> — no reconstructed consumer found; may be populated by
    // a not-yet-reconstructed method. Kept for layout fidelity.
    std::set<std::string>                              unusedStringSet_B0_;    // +0xB0

    MathCompiler                                       mathCompiler_;           // +0xC8

    // Binary: 8-byte pointer at +0xF8, passed to NodeMap::retrieve() — from lookupNode @0x15c54e.
    std::unique_ptr<NodeMap>                           nodeMap_;                // +0xF8 (8B)

    std::unordered_map<NodeMapItem, uint32_t>           nodeIndexMap_;        // +0x100
    std::unordered_map<NodeMapItem, std::set<AccessMode>> accessModeMap_;      // +0x128
    std::vector<NodeMapItem>                           nodeList_;               // +0x150

    // RESOLVED (#101) via deeper dtor analysis at ~CustomFunctions @0x127cf2:
    // The full dtor sequence (node-walk reached via `jne` branch when the
    // first node ptr is non-null):
    //
    //   127cf2: rdi = bucket_array_ptr  ([rbx+0x168])
    //   127cf9: store nullptr there
    //   127d04: if rdi == 0 skip to 127d19
    //   127d09: rsi = bucket_count      ([rbx+0x170])
    //   127d10: rsi <<= 3               (size = N * sizeof(void*))
    //   127d14: operator delete[](rdi, size)  ← bucket array
    //   127d40-127d70 (reached via 127cf0 jne): node-walk loop:
    //     each node = 0x28 (40) bytes = 16B header (next + hash) + 24B std::string
    //     destroys std::string at node+0x10/+0x20 then `operator delete` of node
    //
    // → assignedWaveNames_ IS `std::unordered_set<std::string>` (libc++ 40B container).
    //   Spans +0x168..+0x190 (next field warningCallback_ starts at +0x190).
    //   The 1.0f at +0x188 (set in ctor at 12bec9) is the max_load_factor.
    //
    // Tracks waveform names registered via assignWaveIndex().
    // Tracks waveform names registered via assignWaveIndex().
    std::unordered_set<std::string>                    assignedWaveNames_;      // +0x168 (40B, ends just before +0x190)

    std::function<void(std::string const&)>            warningCallback_;        // +0x190 (48B)

    // External triggering mode: None=unset, Dio=1, ZSync=2.
    // Guards against mixing DIO and ZSync operations in one program.
    // Throw ErrorMessageT(0x4f) if externalTriggeringMode_ != None and != expected.
    ExternalTriggeringMode                             externalTriggeringMode_{ExternalTriggeringMode::None}; // +0x1C0
    char                                               pad_1C4_[4];             // +0x1C4..+0x1C7

    std::set<std::string>                              usedFeatures_;           // +0x1C8

    // --- Extracted helpers (not in binary — refactoring for readability) ---
    void checkExternalTriggeringMode(ExternalTriggeringMode expected);
    bool isShfFamily() const;
};


} // namespace zhinst
