// ============================================================================
// Reconstructed from disassembly of _seqc_compiler.so
// Phase 7d: Prefetch — command emission and query methods
//
// Methods reconstructed:
//   clampToCache         0x1d6c40
//   getUsedChannels      0x1df2f0
//   getUsedFourChannelMode 0x1df400
//   findPlaceholder      0x1d6b50
//   placeCommands        0x1d6680
//   fillInPlaceholders   0x1d65c0
//   getUsedCache         0x1c7eb0
//   getMemoryHighWatermark 0x1cc650
//   getRequiredMemory    0x1cc930
//   needsNewCwvf         0x1dc620
//   wvfImpl              0x1d6ca0
//   wvfRegImpl           0x1d7020
//   wvfs                 0x1d73e0
//   insertPlay           0x1def50
// ============================================================================

#include "zhinst/prefetch.hpp"
#include "zhinst/asm_commands.hpp"
#include "zhinst/resources.hpp"
#include "zhinst/value.hpp"
#include "zhinst/awg_compiler_config.hpp"
#include "zhinst/device_constants.hpp"
#include "zhinst/asm_list.hpp"
#include "zhinst/node.hpp"
#include "zhinst/waveform_ir.hpp"
#include "zhinst/wavetable_ir.hpp"
#include "zhinst/cache.hpp"
#include "zhinst/error_messages.hpp"
#include "zhinst/play_config.hpp"
#include "zhinst/callbacks.hpp"

#include <algorithm>
#include <cstdint>

namespace zhinst {

extern ErrorMessages errMsg;

// ============================================================================
// clampToCache — clamp an address to the available cache size
// 0x1d6c40
//
// For Hirzel devices: clamp to min(cacheSizes[cacheType] * pageSize, addr, 0xFFFFF)
// then if cacheType==Aligned, round up to pageSize alignment.
// For non-Hirzel: just clamp to 0xFFFFF.
//
// config_+0x18: isHirzel (bool)
// config_+0x19: cacheType (uint8_t, 0=Normal, 1=Aligned)
// devConst_+0x14: waveformAlignment (pageSize for cache)
// devConst_+0x18 + cacheType*4: array of cache page counts
//   (devConst_+0x18 = cachePageCount, devConst_+0x1c = maxBlocks)
// ============================================================================
detail::AddressImpl<uint32_t> Prefetch::clampToCache(detail::AddressImpl<uint32_t> addr) const {  // 0x1d6c40
    uint32_t addrVal = addr.value;
    if (!config_->isHirzel) {
        return detail::AddressImpl<uint32_t>{std::min(addrVal, 0xFFFFFu)};
    }

    uint8_t cacheType = config_->cacheType;
    uint32_t pageSize = devConst_->waveformAlignment;  // +0x14

    // Array at devConst_+0x18, indexed by cacheType
    const uint32_t* cachePagesArr = reinterpret_cast<const uint32_t*>(
        &devConst_->cachePageCount);
    uint32_t cachePages = cachePagesArr[cacheType];

    uint32_t limit = cachePages * pageSize;
    if (limit > addrVal) limit = addrVal;
    if (limit > 0xFFFFF) limit = 0xFFFFF;

    if (cacheType == 1 /* Aligned */) {
        limit = (limit + pageSize - 1) & ~(pageSize - 1);
    }
    return detail::AddressImpl<uint32_t>{limit};
}

// getUsedChannels (0x1df2f0) and getUsedFourChannelMode (0x1df400) are
// defined in prefetch_helpers.cpp to avoid ODR violations.

// ============================================================================
// findPlaceholder — find AsmList entry matching node's asmId
// 0x1d6b50
//
// Linear scan of AsmList (vector<Asm>, element size 0xA8).
// Compares entry.sequenceId (at +0x00) with node->asmId (Node+0x14).
// Returns the matching `Asm*` (an iterator into the underlying vector) so
// callers can reach into the assembler-command metadata directly.
// Throws ZIAWGCompilerException(errMsg[0xA3]) if not found.
// ============================================================================
AsmList::Asm* Prefetch::findPlaceholder(                        // 0x1d6b50
    AsmList* asmList, std::shared_ptr<Node> node) {
    int targetId = node->asmId;  // Node+0x14

    for (auto& entry : *asmList) {
        if (entry.sequenceId == targetId) {
            return &entry;
        }
    }

    // Not found — throw error
    std::string msg = ErrorMessages::format(ErrorMessageT(0xA3));
    throw ZIAWGCompilerException(msg);
}

// ============================================================================
// fillInPlaceholders — create output AsmList by walking the node tree
// 0x1d65c0
//
// Returns AsmList by sret. Reserves space matching input size,
// then calls placeCommands(out, root_).
// ============================================================================
AsmList Prefetch::fillInPlaceholders(AsmList const& asmList) {  // 0x1d65c0
    AsmList out;
    out.reserve(asmList.size());

    std::shared_ptr<Node> root = root_;
    placeCommands(&out, root);

    return out;
}

// ============================================================================
// placeCommands — recursively walk node linked list, emitting asm for each node
// 0x1d6680
//
// If the node is the root AND globalCwvfValid_ is false, emits a cwvf (or
// addi+cwvfr for large values) instruction at the position just past any
// leading type-4 (placeholder) entries in the output list.
//
// Then iterates through the node linked list (following node->next), calling
// placeSingleCommand on each node. Checks cancellation callback each iteration.
// ============================================================================
void Prefetch::placeCommands(AsmList* out, std::shared_ptr<Node> node) {  // 0x1d6680
    Node* rawNode = node.get();
    if (!rawNode)
        return;

    // If this is the root node and we haven't emitted a global cwvf yet,
    // emit the initial cwvf instruction.
    if (rawNode == root_.get() && !globalCwvfValid_) {
        // Compute the combined rate/suppress value from PlayConfig defaults
        uint32_t rateValue = (1u << PlayConfig::defaultRateShift) & PlayConfig::defaultRateMask;
        uint32_t suppressValue = (0x3FFFu << PlayConfig::suppressShift) & PlayConfig::suppressMask;
        uint32_t cwvfValue = rateValue | suppressValue;

        // Find insert position: skip past leading entries whose attached
        // Node has type == 4 (NodeType::Loop placeholder). Verified at
        // 0x1d66eb: walks `asmList[i].node->type` (a int loaded from
        // Node+0x10) until non-4. The previous reconstruction had a
        // misnamed `insertPos->nodeType` field; the real field is
        // `node->type` (Asm has no nodeType — see asm_list.hpp:54).
        auto insertPos = out->begin();
        while (insertPos != out->end() && insertPos->node &&
               static_cast<int>(insertPos->node->type) == 4) {
            ++insertPos;
        }

        if (cwvfValue >= 0x1000000) {                                   // 0x1d6716
            // Value too large for immediate cwvf — use addi + cwvfr
            AsmRegister reg(resources_->getRegisterNumber());            // 0x1d671c
            AsmRegister zero(0);
            Immediate imm(static_cast<int>(cwvfValue));

            AsmList addiAsm = asmCommands_->addi(reg, zero, imm);       // 0x1d6770
            out->insert(insertPos, addiAsm.begin(), addiAsm.end());

            AsmList cwvfrAsm = asmCommands_->cwvfr(reg);                // 0x1d68a8
            out->insert(insertPos, cwvfrAsm.begin(), cwvfrAsm.end());
        } else {
            AsmList::Asm cwvfAsm = asmCommands_->cwvf(cwvfValue);       // 0x1d6830
            out->insert(insertPos, cwvfAsm);
        }
    }

    // Lock the cancellation callback weak_ptr                           // 0x1d6902
    std::shared_ptr<CancelCallback> cancelLock;
    if (auto ctrl = cancelCb_.lock()) {
        cancelLock = ctrl;
    }

    // Walk the node linked list                                         // 0x1d692d
    std::shared_ptr<Node> current = node;
    while (current) {
        // Copy current into a local for placeSingleCommand              // 0x1d696d
        std::shared_ptr<Node> tmp = current;
        placeSingleCommand(out, tmp);                                    // 0x1d698c

        // Check cancellation                                            // 0x1d69b1
        if (cancelLock && cancelLock->isCancelled()) {
            break;
        }

        // Advance to next node: next is shared_ptr<Node> at +0xB8       // 0x1d69cb
        current = current->next;  // shared_ptr at Node+0xB8 (ptr) / +0xC0 (ctrl)
    }
    // shared_ptrs cleaned up on scope exit                              // 0x1d6a57
}

// ============================================================================
// Helper: compute waveform memory size in cache pages
//
// Used by getUsedCache, getMemoryHighWatermark, getRequiredMemory.
// Given a WaveformIR, computes:
//   channels = wfm->signal.channels_    (WaveformIR+0xC8, uint16_t)
//   length   = wfm->signal.length_      (WaveformIR+0xD0, uint64_t — numRepeats)
//   dc       = wfm->deviceConstants     (WaveformIR+0x78, DeviceConstants*)
//   waveGranularity = dc->waveformGranularity   (DC+0x40)
//   maxPages        = dc->waveformPageSize      (DC+0x44)
//   bitsPerSample   = dc->bitsPerSample         (DC+0x50)
//
// If length != 0:
//   numPages = ceil(length / maxPages) * maxPages, capped at waveGranularity
// Else:
//   numPages = 0
// totalBits = numPages * channels * bitsPerSample
// bytes = ceil(totalBits / 8)
// ============================================================================
static int computeWaveformMemoryBytes(const WaveformIR* wfm) {
    uint16_t channels = wfm->signal.channels_;       // +0xC8
    uint32_t length = wfm->signal.length_;            // +0xD0 (lower 32 bits)
    const DeviceConstants* dc = wfm->deviceConstants; // +0x78

    uint32_t waveGranularity = dc->waveformGranularity;  // DC+0x40
    uint32_t maxPages = dc->waveformPageSize;          // DC+0x44

    uint32_t numPages;
    if (length != 0) {
        // ceil(length / maxPages) * maxPages, capped at waveGranularity
        numPages = ((length + maxPages - 1) / maxPages) * maxPages;
        if (waveGranularity > numPages)
            numPages = waveGranularity;
    } else {
        numPages = 0;
    }

    uint32_t bitsPerSample = dc->bitsPerSample;  // DC+0x50
    uint64_t totalBits = static_cast<uint64_t>(numPages) *
                         static_cast<uint64_t>(channels) *
                         static_cast<uint64_t>(bitsPerSample);
    int bytes = static_cast<int>((totalBits + 7) / 8);
    return bytes;
}

// getUsedCache (0x1c7eb0), getMemoryHighWatermark (0x1cc650), and
// getRequiredMemory (0x1cc930) are defined in prefetch_helpers.cpp.

// ============================================================================
// needsNewCwvf — check if a new CWVF (configure waveform) instruction is needed
// 0x1dc620 – 0x1dd1a0
//
// Walks backward through the node's parent (weak_ptr at +0xF0/+0xF8) chain,
// comparing currentCwvf (PlayConfig at Node+0x68) fields between the input
// node and ancestor nodes. Returns true if the configs differ and a new CWVF
// instruction must be emitted.
//
// Inline PlayConfig comparisons check fields at Node+0x68:
//   channelMask, rate, suppress, is4Channel, markerBits, trigger,
//   precompFlags, dummy — always compared.
//   hold — only compared when rate > 0.
//
// The function is recursive: it calls itself at 0x1dce3f for certain
// tree structure paths (when node is found in parent's loop link and
// the parent's parent needs checking).
// ============================================================================
bool Prefetch::needsNewCwvf(std::shared_ptr<Node> node) const {  // 0x1dc620
    Node* n = node.get();                                         // 0x1dc638

    // Lock the node's parent weak_ptr (+0xF0/+0xF8) to get the parent node.
    std::shared_ptr<Node> parentSp;  // stored at -0x30/-0x28 on stack
    Node* parentRaw = nullptr;
    {
        auto locked = n->parent.lock();                           // 0x1dc642–0x1dc653
        if (locked) {
            parentRaw = locked.get();                             // 0x1dc65c: mov 0xf0(%r14)
            parentSp = locked;
        }
    }

    Node* d = node.get();                                         // 0x1dc667: mov (%rbx),%rdx

    // --- Early exit for Play nodes with dummy/hold and hasPrecomp ---
    // 0x1dc66a–0x1dc6bd
    if (d->type == NodeType::Play) {                              // 0x1dc66a: cmpl $0x2,0x44(%rdx)
        bool hasPrecomp = devConst_->hasPrecomp;                  // 0x1dc67f: mov 0x8(%r15),%rsi; movzbl 0x88
        if (d->config.dummy || d->config.hold) {                  // 0x1dc68a/0x1dc690: +0x66/+0x65
            int rate = d->config.rate;                            // 0x1dc696: mov 0x4c(%rdx)
            bool rateOk = (rate == 0) ||                          // 0x1dc699
                           (rate == -1 && d->globalRate <= 0);    // 0x1dc69d/0x1dc6a2: +0x100
            if (rateOk && hasPrecomp) {                           // 0x1dc6ab
                // Return false — no new CWVF needed for this                 // 0x1dc6b1: xor r14d
                // precomp dummy/hold node. Clean up parentSp.               // 0x1dc6b4–0x1dc6bd
                return false;                                     // -> 0x1dd112
            }
        }
    }

    // --- Main loop: walk up the parent chain comparing currentCwvf ---
    // Set up iteration state.
    // curNode tracks the "previous" node in the chain (starts as input node).
    // prevLoad tracks the parent being examined (starts as parentSp).
    std::shared_ptr<Node> curNode = node;                         // 0x1dc6c2–0x1dc6ce: -0x60/-0x58
    std::shared_ptr<Node> prevLoad = parentSp;                    // 0x1dc6d8–0x1dc6e5: -0x50/-0x48
    bool seenDifference = false;                                  // 0x1dc6f3: xor r12d

    // If no parent, skip to tree-walk section.                   // 0x1dc6ea–0x1dc6ed
    while (Node* prev = prevLoad.get()) {                         // 0x1dc700–0x1dc707
        NodeType prevType = prev->type;                           // 0x1dc70d: mov 0x44(%rcx)

        if (prevType == NodeType::Loop) {                         // 0x1dc710: cmp $0x8
            // --- Loop node: inline PlayConfig comparison ---    // 0x1dc770
            Node* cur = node.get();                               // 0x1dc770: mov (%rbx),%rax

            // First block: compare channelMask and rate.
            // If either differs → return true immediately.
            if (prev->currentCwvf.channelMask != cur->currentCwvf.channelMask)  // 0x1dc773–0x1dc77c
                return true;
            int prevRate = prev->currentCwvf.rate;                              // 0x1dc782
            if (prevRate != cur->currentCwvf.rate)                              // 0x1dc785–0x1dc788
                return true;

            // Second block: compare suppress through dummy.
            // If any of these differ, jump to secondCheck with
            // seenDifference carried forward from previous iteration.
            bool firstBlockDiff = false;
            if (prev->currentCwvf.suppress != cur->currentCwvf.suppress)        // 0x1dc78e
                firstBlockDiff = true;
            else if (prev->currentCwvf.is4Channel != cur->currentCwvf.is4Channel) // 0x1dc796
                firstBlockDiff = true;
            else if (prev->currentCwvf.markerBits != cur->currentCwvf.markerBits) // 0x1dc7a0
                firstBlockDiff = true;
            else if (prev->currentCwvf.trigger != cur->currentCwvf.trigger)      // 0x1dc7a8
                firstBlockDiff = true;
            else if (prev->currentCwvf.precompFlags != cur->currentCwvf.precompFlags) // 0x1dc7b0
                firstBlockDiff = true;
            else if (prev->currentCwvf.dummy != cur->currentCwvf.dummy)          // 0x1dc7be
                firstBlockDiff = true;

            bool newSeenDifference;
            if (firstBlockDiff) {
                // Jump to second comparison with seenDifference unchanged.   // 0x1dc810
                newSeenDifference = seenDifference;
            } else {
                // Hold comparison (only if rate > 0).                         // 0x1dc7ce
                bool holdDiffers = (prevRate > 0) &&
                    (prev->currentCwvf.hold != cur->currentCwvf.hold);        // 0x1dc7d2–0x1dc7e4

                if (prev->loop.get() == curNode.get()) {                      // 0x1dc7e8–0x1dc7f3
                    // Loop points back to curNode — use holdDiffers as        // (je 1dc810)
                    // new seenDifference.
                    newSeenDifference = holdDiffers;
                } else if (holdDiffers) {
                    // Hold differs but loop doesn't match — use old value.   // 0x1dc7f5–0x1dc7f8
                    newSeenDifference = seenDifference;
                } else {
                    // No field differences. Check loopBodyRunsAtLeastOnce.
                    if (!prev->loopBodyRunsAtLeastOnce) {                     // 0x1dc7fd–0x1dc804
                        // Loop body might not run — mark seenDifference.     // 0x1dc7fa: sil=1
                        newSeenDifference = true;
                    } else {
                        // loopBodyRunsAtLeastOnce=true → skip to             // 0x1dc806 -> 0x1dc955
                        // seenDifference path (check full config).
                        goto checkSeenDifference;
                    }
                }
            }

            // --- Second comparison block (suppress through hold) ---         // 0x1dc813
            // Compare suppress, is4Channel, markerBits, trigger,
            // precompFlags, dummy of prev->currentCwvf vs node->currentCwvf.
            // If any differ → return true.
            if (prev->currentCwvf.suppress != cur->currentCwvf.suppress)       // 0x1dc813
                return true;
            if (prev->currentCwvf.is4Channel != cur->currentCwvf.is4Channel)   // 0x1dc81f
                return true;
            if (prev->currentCwvf.markerBits != cur->currentCwvf.markerBits)   // 0x1dc82d
                return true;
            if (prev->currentCwvf.trigger != cur->currentCwvf.trigger)         // 0x1dc839
                return true;
            if (prev->currentCwvf.precompFlags != cur->currentCwvf.precompFlags) // 0x1dc845
                return true;
            if (prev->currentCwvf.dummy != cur->currentCwvf.dummy)             // 0x1dc857
                return true;
            // Hold: only if rate > 0.                                          // 0x1dc86b
            if (prevRate > 0 &&
                prev->currentCwvf.hold != cur->currentCwvf.hold)               // 0x1dc870–0x1dc885
                return true;

            seenDifference = newSeenDifference;                                 // 0x1dc880
            goto advanceLoad;                                                   // -> 0x1dc88b

        } else if (prevType == NodeType::Play) {                               // 0x1dc715: cmp $0x2
            // --- Play node: check precomp conditions ---                     // 0x1dc71e
            {
            bool hasPrecomp = devConst_->hasPrecomp;
            if (prev->config.dummy || prev->config.hold) {                     // 0x1dc729/0x1dc72f
                int rate = prev->config.rate;                                  // 0x1dc739
                bool rateOk = (rate == 0) ||
                               (rate == -1 && prev->globalRate <= 0);          // 0x1dc740–0x1dc750
                if (rateOk && hasPrecomp) {                                    // 0x1dc756
                    // This Play node is also a precomp dummy/hold →
                    // skip to advanceLoad.                                    // -> 0x1dc88b
                    goto advanceLoad;
                }
            }
            } // scope for hasPrecomp
            // Fall through to checkSeenDifference.                            // 0x1dc75e -> 0x1dc955
        checkSeenDifference:
            if (seenDifference) {                                              // 0x1dc955: test r12b
                // Full inline PlayConfig comparison of prev->currentCwvf      // 0x1dc962
                // vs node->currentCwvf (all fields including conditional hold).
                {
                Node* cur = node.get();
                if (prev->currentCwvf.channelMask != cur->currentCwvf.channelMask) return true;  // 0x1dc968
                int r = prev->currentCwvf.rate;
                if (r != cur->currentCwvf.rate) return true;                   // 0x1dc974
                if (prev->currentCwvf.suppress != cur->currentCwvf.suppress) return true;
                if (prev->currentCwvf.is4Channel != cur->currentCwvf.is4Channel) return true;
                if (prev->currentCwvf.markerBits != cur->currentCwvf.markerBits) return true;
                if (prev->currentCwvf.trigger != cur->currentCwvf.trigger) return true;
                if (prev->currentCwvf.precompFlags != cur->currentCwvf.precompFlags) return true;
                if (prev->currentCwvf.dummy != cur->currentCwvf.dummy) return true;
                if (r > 0 && prev->currentCwvf.hold != cur->currentCwvf.hold) return true;  // 0x1dc9d5–0x1dc9e6
                } // end braces for goto-crosses-init
            }
            // Fall through to tree-walk section.                              // -> 0x1dc9ec
            goto treeWalk;
        }

    advanceLoad:                                                               // 0x1dc88b
        // Move curNode ← prevLoad, prevLoad ← prevLoad's parent.
        curNode = prevLoad;                                                    // 0x1dc899
        // Lock prev's parent weak_ptr.                                        // 0x1dc8bc–0x1dc8dd
        prevLoad = prev->parent.lock();
        continue;
    }

treeWalk:
    // --- Tree-walk section: find node in parent's links ---                   // 0x1dc9ec
    {
        Node* parentNode = parentSp.get();                                     // r12 = -0x30
        if (!parentNode)
            goto notFoundInTree;  // confirmed — shouldn't happen normally

        // Check if parentNode->next == node                                   // 0x1dc9f0–0x1dca03
        if (parentNode->next.get() != nullptr &&
            parentNode->next.get() == node.get()) {
            // Compare parentNode->currentCwvf vs node->currentCwvf (full).    // 0x1dcc9e
            Node* cur = node.get();
            if (parentNode->currentCwvf.channelMask != cur->currentCwvf.channelMask) return true;
            if (parentNode->currentCwvf.rate != cur->currentCwvf.rate) return true;
            if (parentNode->currentCwvf.suppress != cur->currentCwvf.suppress) return true;
            if (parentNode->currentCwvf.is4Channel != cur->currentCwvf.is4Channel) return true;
            if (parentNode->currentCwvf.markerBits != cur->currentCwvf.markerBits) return true;
            if (parentNode->currentCwvf.trigger != cur->currentCwvf.trigger) return true;
            if (parentNode->currentCwvf.precompFlags != cur->currentCwvf.precompFlags) return true;
            if (parentNode->currentCwvf.dummy != cur->currentCwvf.dummy) return true;
            int r = parentNode->currentCwvf.rate;
            return (r > 0) && (parentNode->currentCwvf.hold != cur->currentCwvf.hold);  // 0x1dcd1f–0x1dcd3a
        }

        // Check if parentNode->loop == node                                   // 0x1dca09–0x1dca1c
        if (parentNode->loop.get() != nullptr &&
            parentNode->loop.get() == node.get()) {
            // Compare parentNode->currentCwvf vs node->currentCwvf (full     // 0x1dcd3f
            // with hold conditional on rate > 0, but NOT using setg/and —
            // this variant returns true if rate>0 check fails, otherwise
            // falls through to lock parent's parent).
            Node* cur = node.get();
            if (parentNode->currentCwvf.channelMask != cur->currentCwvf.channelMask) return true;
            int r = parentNode->currentCwvf.rate;
            if (r != cur->currentCwvf.rate) return true;
            if (parentNode->currentCwvf.suppress != cur->currentCwvf.suppress) return true;
            if (parentNode->currentCwvf.is4Channel != cur->currentCwvf.is4Channel) return true;
            if (parentNode->currentCwvf.markerBits != cur->currentCwvf.markerBits) return true;
            if (parentNode->currentCwvf.trigger != cur->currentCwvf.trigger) return true;
            if (parentNode->currentCwvf.precompFlags != cur->currentCwvf.precompFlags) return true;
            if (parentNode->currentCwvf.dummy != cur->currentCwvf.dummy) return true;
            if (r > 0 && parentNode->currentCwvf.hold != cur->currentCwvf.hold)  // 0x1dcdc0–0x1dcdd3
                return true;

            // All currentCwvf fields match. Lock parentNode's parent and      // 0x1dcdd9
            // check if that parent's next == parentNode.
            auto grandParentSp = parentNode->parent.lock();                    // 0x1dcde0–0x1dcdf2
            Node* grandParent = grandParentSp.get();
            if (grandParent && grandParent->next.get() == parentNode) {        // 0x1dce0c
                // Compare grandParent->currentCwvf vs node->currentCwvf       // 0x1dd125
                // via PlayConfig::operator!=.
                return grandParent->currentCwvf != node.get()->currentCwvf;    // 0x1dd125–0x1dd135 -> 0x1dceec
            }

            // grandParent doesn't have next==parentNode.                      // 0x1dce19
            // Recursive call: needsNewCwvf(parentNode).                       // 0x1dce3f
            // Constructs shared_ptr<Node> aliasing parentSp's ctrl block
            // but pointing to parentNode raw ptr.
            return needsNewCwvf(std::shared_ptr<Node>(parentSp, parentNode));  // 0x1dce19–0x1dce3f
        }

        // Check if node is found in parentNode->branches                      // 0x1dca22–0x1dca4e
        {
            auto& branches = parentNode->branches;
            bool foundInBranches = false;
            for (auto it = branches.begin(); it != branches.end(); ++it) {
                if (it->get() == node.get()) {
                    foundInBranches = true;
                    break;
                }
            }

            if (!foundInBranches) {
                // Node not found in any of parent's links → return true.      // 0x1dcc96
                return true;
            }

            // Node found in branches. Lock parentNode's parent and            // 0x1dca53
            // compare PlayConfig.
            auto grandParentSp2 = parentNode->parent.lock();                   // 0x1dca63–0x1dca70
            Node* grandParent2 = grandParentSp2.get();                          // confirmed: raw ptr always from 0xf0(%r12)

            // Check if grandParent2->next == parentNode                       // 0x1dca80
            if (grandParent2 && grandParent2->next.get() == parentNode) {
                // Compare grandParent2->currentCwvf vs node->currentCwvf      // 0x1dca8d
                // (full inline comparison with setg/and for hold).
                Node* cur = node.get();
                if (grandParent2->currentCwvf.channelMask != cur->currentCwvf.channelMask) return true;
                if (grandParent2->currentCwvf.rate != cur->currentCwvf.rate) return true;
                if (grandParent2->currentCwvf.suppress != cur->currentCwvf.suppress) return true;
                if (grandParent2->currentCwvf.is4Channel != cur->currentCwvf.is4Channel) return true;
                if (grandParent2->currentCwvf.markerBits != cur->currentCwvf.markerBits) return true;
                if (grandParent2->currentCwvf.trigger != cur->currentCwvf.trigger) return true;
                if (grandParent2->currentCwvf.precompFlags != cur->currentCwvf.precompFlags) return true;
                if (grandParent2->currentCwvf.dummy != cur->currentCwvf.dummy) return true;
                int r = grandParent2->currentCwvf.rate;
                return (r > 0) && (grandParent2->currentCwvf.hold != cur->currentCwvf.hold);
            }

            // grandParent2 doesn't have next==parentNode.                     // 0x1dcb3d
            // Walk up the tree looking for an ancestor whose next or loop
            // links back to parentNode.
            Node* walk = parentNode;                                           // r12 = parentNode
            while (walk) {                                                     // 0x1dcb34
                // Lock walk's parent.                                         // 0x1dcb3d–0x1dcb54
                auto walkParentSp = walk->parent.lock();
                if (walkParentSp) {
                    Node* wp = walkParentSp.get();
                    if (wp->next.get() == walk) {                              // 0x1dcb5c–0x1dcb8c
                        // Found: ancestor's next == walk.                     // -> 0x1dce74
                        // Now lock walk's own parent for PlayConfig compare.
                        goto ancestorFoundNext;
                    }
                }
                // Null-parent fallback: checks if 0xb8 == r12 (next-field offset from nullptr).
                // In practice unreachable — a dead path / null-deref guard.       // confirmed
                // (At 0x1dcba0: cmp %r12, 0xb8 — compares walk against NULL->next)

                auto walk2ParentSp = parentSp;                                 // r15 = -0x30 (parentSp)  // 0x1dcbae
                if (walk2ParentSp) {
                    auto walk2GrandSp = walk2ParentSp->parent.lock();          // 0x1dcbb2–0x1dcbc3
                    if (walk2GrandSp) {
                        Node* w2g = walk2GrandSp.get();
                        if (w2g->loop.get() == walk2ParentSp.get()) {          // 0x1dcbcf–0x1dcbff
                            // Found: ancestor's loop == parentSp.             // -> 0x1dcef4
                            goto ancestorFoundLoop;
                        }
                    }
                }

                // Advance walk up via its parent.                             // 0x1dcc1e–0x1dcc55
                auto nextWalkSp = parentSp->parent.lock();                     // confirmed
                walk = nextWalkSp.get();
                parentSp = nextWalkSp;                                         // update parentSp
                continue;
            }

        ancestorFoundNext:                                                     // 0x1dce74
            {
                // Lock parentNode's parent for PlayConfig::operator!=.        // 0x1dce78–0x1dce95
                auto pSp = walk->parent.lock();                                // confirmed — r14 = [rbp-0x30] = walk = parentSp.get()
                Node* p = pSp.get();
                if (p) {
                    return p->currentCwvf != node.get()->currentCwvf;          // 0x1dcee0–0x1dceec: PlayConfig::operator!=
                }
                return false;                                                  // 0x1dcfde: r14=0
            }

        ancestorFoundLoop:                                                     // 0x1dcef4
            {
                // Compare parentSp's parent's currentCwvf vs node.            // 0x1dcef4–0x1dcfaa
                auto pSp = parentSp->parent.lock();                            // confirmed — r15
                Node* p = pSp.get();
                Node* cur = node.get();

                if (p->currentCwvf.channelMask != cur->currentCwvf.channelMask) return true;
                int r = p->currentCwvf.rate;
                if (r != cur->currentCwvf.rate) return true;
                if (p->currentCwvf.suppress != cur->currentCwvf.suppress) return true;
                if (p->currentCwvf.is4Channel != cur->currentCwvf.is4Channel) return true;
                if (p->currentCwvf.markerBits != cur->currentCwvf.markerBits) return true;
                if (p->currentCwvf.trigger != cur->currentCwvf.trigger) return true;
                if (p->currentCwvf.precompFlags != cur->currentCwvf.precompFlags) return true;
                if (p->currentCwvf.dummy != cur->currentCwvf.dummy) return true;
                if (r > 0 && p->currentCwvf.hold != cur->currentCwvf.hold) return true;

                // All match. Lock p's parent and p's parent's parent,         // 0x1dcfb0–0x1dd018
                // then compare via PlayConfig::operator!=.
                auto ppSp = p->parent.lock();                                  // confirmed — r14 = pSp.get() = p
                Node* pp = ppSp.get();
                if (pp) {
                    auto pppSp = pp->parent.lock();                            // confirmed
                    Node* ppp = pppSp.get();
                    if (ppp) {
                        return ppp->currentCwvf != cur->currentCwvf;           // 0x1dd008–0x1dd018: PlayConfig::operator!=
                    }
                }
                return false;
            }
        }
    }

notFoundInTree:
    return false;                                                              // 0x1dd112: mov r14d,%eax
}

// ============================================================================
// wvfImpl — emit waveform play with immediate offset
// 0x1d6ca0 (ends ~0x1d6f9e)
//
// Generates a wvf or wvfi instruction depending on the `indexed` flag.
// If offset < 0x100000 (fits in 20-bit immediate), emits a single wvf/wvfi
// with AsmRegister(0) as the address register and the offset as immediate.
// If offset >= 0x100000, allocates a temp register, loads the offset via
// ADDI (tempReg = R0 + offset), then emits wvf/wvfi with tempReg and offset=0.
// ============================================================================
AsmList Prefetch::wvfImpl(AsmRegister reg, int offset,
                          bool indexed) const {  // 0x1d6ca0
    // Select wvf or wvfi based on indexed flag                   // 0x1d6cc0
    auto wvfFn = indexed
        ? &AsmCommands::wvfi    // 0x1d6cc5: lea to 0x271820
        : &AsmCommands::wvf;    // 0x1d6cce: lea to 0x271730

    AsmList result;                                               // 0x1d6cd5

    if (offset >= 0x100000) {                                     // 0x1d6ce3
        // Offset too large for immediate — use a temp register
        AsmRegister tempReg(Resources::getRegisterNumber());      // 0x1d6cf4

        // Load offset into tempReg: tempReg = R0 + Immediate(offset)
        AsmRegister zeroReg(0);                                   // 0x1d6d1a
        auto addiResult = asmCommands_->addi(                     // 0x1d6d45
            tempReg, zeroReg, Immediate(offset));
        result.insert(result.end(),                               // 0x1d6d77
                      addiResult.begin(), addiResult.end());

        // Emit wvf/wvfi with tempReg as address, offset = 0
        auto entry = (asmCommands_.get()->*wvfFn)(reg, tempReg, 0);  // 0x1d6ed1
        result.push_back(entry);                                  // 0x1d6ed4
    } else {
        // Offset fits in immediate — emit directly
        AsmRegister zeroReg(0);                                   // 0x1d6e0d
        auto entry = (asmCommands_.get()->*wvfFn)(                // 0x1d6e26
            reg, zeroReg, offset);
        result.push_back(entry);                                  // 0x1d6e29
    }

    return result;
}

// ============================================================================
// wvfRegImpl — emit waveform play with register-based offset
// 0x1d7020
//
// Like wvfImpl but takes an AsmRegister for offset instead of int.
// Allocates a temp register, copies the offset register into it via addi(+0),
// emits ssl (set sample length) on the temp, then emits wvf/wvfi.
// ============================================================================
AsmList Prefetch::wvfRegImpl(AsmRegister reg, AsmRegister offset,
                             bool indexed) const {  // 0x1d7020
    AsmList result;

    // Allocate a temporary register for the address
    AsmRegister addrReg(Resources::getRegisterNumber());

    // Copy offset register into addrReg: addrReg = offset + 0
    auto addiResult = asmCommands_->addi(addrReg, offset, Immediate(0));  // 0x1d7093
    result.insert(result.end(), addiResult.begin(), addiResult.end());

    // Set sample length from the address register
    AsmList::Asm sslEntry = asmCommands_->ssl(addrReg);  // 0x1d71a2
    result.push_back(sslEntry);

    // Emit wvf or wvfi depending on indexed flag
    AsmList::Asm wvfEntry = indexed
        ? asmCommands_->wvfi(reg, addrReg, 0)   // 0x1d7269
        : asmCommands_->wvf(reg, addrReg, 0);   // 0x1d7272
    result.push_back(wvfEntry);  // 0x1d728d

    return result;
}

// ============================================================================
// Prefetch::insertPlay — 0x1def50
//
// Inserts play-related instructions into the output list:
//   1. A label entry (asmLabel) for the given name
//   2. Waveform implementation instructions (wvfImpl) using reg, addrA, flag
//   3. If NOT a Hirzel device: an xnori instruction setting reg = reg XNOR addrB
// ============================================================================
void Prefetch::insertPlay(AsmList& list, bool flag,
                          std::string const& name, AsmRegister reg,
                          detail::AddressImpl<uint32_t> addrA,
                          detail::AddressImpl<uint32_t> addrB) const  // 0x1def50
{
    // 1. Emit a label entry and append to list
    AsmList::Asm labelEntry = asmCommands_->asmLabel(name);  // 0x1def82
    list.push_back(labelEntry);                               // 0x1def91..0x1deffd

    // 2. Generate waveform instructions and append to list
    {
        AsmList wvfInstructions = wvfImpl(reg, addrA, flag);  // 0x1df056
        list.insert(list.end(),                                // 0x1df088
                    wvfInstructions.begin(),
                    wvfInstructions.end());
    }  // wvfInstructions destroyed here (0x1df09d..0x1df11a)

    // 3. If not a Hirzel device, emit xnori(reg, reg, Immediate(addrB))
    if (!config_->isHirzel) {                                 // 0x1df11f..0x1df126
        Immediate imm(addrB);                                 // 0x1df13d
        AsmList xnorInstructions = asmCommands_->xnori(reg, reg, imm);  // 0x1df156
        list.insert(list.end(),                               // 0x1df188
                    xnorInstructions.begin(),
                    xnorInstructions.end());
        // xnorInstructions destroyed (0x1df19d..0x1df21a)
        // imm destroyed (0x1df21f..0x1df241, inlined variant dtor)
    }
}

// ============================================================================
// wvfs — emit waveform-set instruction sequence
// 0x1d73e0 (ends ~0x1d7940)
//
// Dispatches waveform prefetch instructions based on offset size.
//
// If offset < 0x100000 (fits in 20-bit immediate):
//   Emits a single AsmCommands::wvfs(playDummyType, reg, offset).
//
// If offset >= 0x100000 (large address):
//   1. Allocates a temp register.
//   2. Emits addi(tempReg, reg, Immediate(offset - 0xFFFFF)) to load high bits.
//   3. If reg was originally invalid (R0) AND numChannelGroups >= 2 AND
//      the address space requires > 20 bits AND the addi produced an ADDI
//      instruction, emits an extra addiu(tempReg, tempReg, Immediate(0))
//      to extend the upper address bits.
//   4. Emits AsmCommands::wvfs(playDummyType, tempReg, 0xFFFFF) for the
//      low 20 bits.
// ============================================================================
AsmList Prefetch::wvfs(Assembler::PlayDummyType playDummyType,
                       AsmRegister reg, int offset) const {  // 0x1d73e0
    bool regIsValid = reg.isValid();                          // 0x1d7407
    if (!regIsValid) {
        reg = AsmRegister(0);                                 // 0x1d741c
    }

    AsmList result;                                           // 0x1d742c

    if (offset >= 0x100000) {                                 // 0x1d743c
        // Large offset — split into high-bits ADDI + low-bits wvfs
        AsmRegister tempReg(Resources::getRegisterNumber());  // 0x1d7454

        // tempReg = reg + (offset - 0xFFFFF)
        AsmList addiResult = asmCommands_->addi(              // 0x1d749e
            tempReg, reg, Immediate(offset - 0xFFFFF));
        result.insert(result.end(),                           // 0x1d74d4
                      addiResult.begin(), addiResult.end());

        // Check whether we need an extra addiu for upper address extension.
        // This applies when:
        //   - reg was not originally valid (defaulted to R0)
        //   - config has >= 2 channel groups (multi-channel mode)
        //   - the total address space computed from sequencerReg/auxReg
        //     exceeds 20 bits
        //   - the last emitted instruction is an ADDI
        if (!regIsValid &&                                    // 0x1d7610
            config_->numChannelGroups >= 2) {                 // 0x1d7619

            // Compute total address size in bytes from device constants
            uint32_t width  = devConst_->waveformGranularity;  // DC+0x40
            uint32_t depth  = devConst_->waveformPageSize;  // DC+0x44
            uint32_t auxW   = devConst_->bitsPerSample;        // DC+0x50

            // Round offset up to next multiple of depth         // 0x1d7632
            uint32_t rounded = ((offset + depth - 1) / depth) * depth;
            rounded = std::max(rounded, width);                  // 0x1d7644

            // Total bits, then convert to bytes (round up)      // 0x1d764d
            uint32_t totalBits  = rounded * auxW;
            uint32_t totalBytes = (totalBits + 7) / 8;

            // Check: last instruction is ADDI and upper bits are set
            auto& lastAsm = result.back();                       // 0x1d7661
            if (lastAsm.assembler.cmd == Assembler::Command::ADDI &&
                (totalBytes & 0xFFF00000) != 0) {

                // Emit addiu(tempReg, tempReg, Immediate(0))    // 0x1d76a9
                // to extend the address into upper 20+ bit range
                AsmRegister lastDst = lastAsm.assembler.regSrc;    // 0x1d7681
                auto addiuEntry = asmCommands_->addiu(
                    lastDst, lastDst, Immediate(0));
                result.push_back(addiuEntry);                    // 0x1d76b8
            }
        }

        // Emit wvfs with tempReg and the low 20-bit mask       // 0x1d77a9
        auto entry = asmCommands_->wvfs(
            playDummyType, tempReg, 0xFFFFF);
        result.push_back(entry);                                 // 0x1d77b8
    } else {
        // Offset fits in 20-bit immediate — emit directly       // 0x1d7560
        auto entry = asmCommands_->wvfs(
            playDummyType, reg, offset);
        result.push_back(entry);                                 // 0x1d7583
    }

    return result;
}

} // namespace zhinst
