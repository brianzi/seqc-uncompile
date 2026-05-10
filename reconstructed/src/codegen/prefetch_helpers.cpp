// ============================================================================
// Reconstructed from disassembly of _seqc_compiler.so
// File: prefetch_helpers.cpp — Prefetch helper, getter, and query methods
// ============================================================================

#include "zhinst/codegen/prefetch.hpp"
#include "zhinst/ast/node.hpp"
#include "zhinst/runtime/cache.hpp"
#include "zhinst/waveform/waveform_ir.hpp"
#include "zhinst/waveform/wavetable_ir.hpp"
#include "zhinst/device/device_constants.hpp"
#include "zhinst/codegen/awg_compiler_config.hpp"
#include "zhinst/waveform/signal.hpp"

#include <deque>
#include <stack>

namespace zhinst {

// ============================================================================
// 0x1cc650 — Prefetch::getMemoryHighWatermark() const
// Ends at 0x1cc802.
//
// Computes the maximum waveform memory high-water mark across all device indices.
// For each device index, calls getUsedWavesForDevice(idx), then for each
// WaveformIR computes:
//   numPages = ceil(signal.length_ / dc->grainSize) * dc->grainSize
//   numPages = min(numPages, dc->maxWaveformLength)         // cap at max
//   memoryBits = numPages * signal.channels_ * dc->bitsPerSample
//   memoryBytes = (memoryBits + 7) / 8                        // bits → bytes
//   netMemory = memoryBytes - addressValue                    // subtract startOffset
// Tracks max(netMemory) across waveforms within a device, then max across devices.
//
// Device index range:
//   If config_->numChannelGroups >= 2 AND config_->deviceType == HDAWG(2):
//     iterate deviceIdx = 0 .. numChannelGroups-1
//   Else:
//     single iteration with deviceIdx = config_->deviceIndex
// ============================================================================
size_t Prefetch::getMemoryHighWatermark() const // 0x1cc650
{
    int numChannelGroups = config_->numChannelGroups; // +0x1C
    int devType = static_cast<int>(config_->deviceType); // +0x00

    size_t startIdx, endIdx;
    if (numChannelGroups >= 2 && devType == 2 /*HDAWG*/) {
        startIdx = 0;                                     // 0x1cc67e: xor r14,r14
        endIdx = static_cast<size_t>(numChannelGroups);   // 0x1cc66b: stored as loop limit
    } else {
        startIdx = static_cast<size_t>(config_->deviceIndex); // 0x1cc686: mov 0x24(%rax),%r15d
        endIdx = startIdx + 1;                                // 0x1cc68a: lea 0x1(%r15),%rax
    }

    uint32_t maxWatermark = 0; // r14d

    for (size_t devIdx = startIdx; devIdx < endIdx; ++devIdx) {
        // 0x1cc6cf: call getUsedWavesForDevice(devIdx)
        auto waves = getUsedWavesForDevice(devIdx);
        if (waves.empty())
            continue;

        uint32_t deviceMax = 0;

        for (auto& wfm : waves) {
            // Waveform+0xC8 = signal.channels_ (uint16_t)
            uint16_t channels = wfm->signal.channels_;
            // Waveform+0xD0 = signal.length_ — used as numRepeats/playLength
            uint64_t numRepeats = wfm->signal.length_;
            // Waveform+0x78 = deviceConstants pointer
            DeviceConstants const* dc = wfm->deviceConstants;

            uint32_t numPages;
            if (numRepeats != 0) {
                // DC+0x44 = grainSize (stride/granularity)
                uint32_t stride = dc->grainSize;     // +0x44
                // DC+0x40 = maxWaveformLength (base/max cap)
                uint32_t base = dc->maxWaveformLength;    // +0x40
                // Round up to multiple of stride
                numPages = static_cast<uint32_t>(
                    ((numRepeats + stride - 1) / stride) * stride);
                // Cap at base
                numPages = std::min(numPages, base);
            } else {
                numPages = 0;
            }

            // DC+0x50 = bitsPerSample — bits per sample
            uint32_t bitsPerSample = dc->bitsPerSample;      // +0x50
            // memoryBits = numPages * channels * bitsPerSample
            uint64_t memoryBits = static_cast<uint64_t>(numPages) * channels * bitsPerSample;
            // Convert bits → bytes, rounding up
            uint32_t memoryBytes = static_cast<uint32_t>((memoryBits + 7) / 8);

            // Waveform+0x4C = addressValue (waveform address)
            uint32_t addrValue = wfm->addressValue;         // +0x4C
            // Binary @0x1cc73a: sub %esi,%r8d; add %eax,%r8d
            // esi = config_->addressImpl (loaded once before loop at +0x153)
            // Net memory = (addressValue - addressImpl) + memoryBytes
            uint32_t addressImpl = config_->addressImpl;     // config+0x10
            uint32_t netMemory = (addrValue - addressImpl) + memoryBytes;

            deviceMax = std::max(deviceMax, netMemory);
        }

        maxWatermark = std::max(maxWatermark, deviceMax);
    }

    return maxWatermark;
}

// ============================================================================
// 0x1cc930 — Prefetch::getRequiredMemory() const
// Ends at 0x1ccac2.
//
// Computes the total required waveform memory per device, returns the max
// across devices. Same structure as getMemoryHighWatermark but:
//   - Sums waveform sizes within each device (instead of taking max)
//   - Does NOT subtract addressValue (startOffset)
// ============================================================================
size_t Prefetch::getRequiredMemory() const // 0x1cc930
{
    int numChannelGroups = config_->numChannelGroups; // +0x1C
    int devType = static_cast<int>(config_->deviceType); // +0x00

    size_t startIdx, endIdx;
    if (numChannelGroups >= 2 && devType == 2 /*HDAWG*/) {
        startIdx = 0;
        endIdx = static_cast<size_t>(numChannelGroups);
    } else {
        startIdx = static_cast<size_t>(config_->deviceIndex); // +0x24
        endIdx = startIdx + 1;
    }

    uint32_t maxRequired = 0;

    for (size_t devIdx = startIdx; devIdx < endIdx; ++devIdx) {
        auto waves = getUsedWavesForDevice(devIdx);
        if (waves.empty())
            continue;

        uint32_t totalBytes = 0;

        for (auto& wfm : waves) {
            uint16_t channels = wfm->signal.channels_;       // Waveform+0xC8
            uint64_t numRepeats = wfm->signal.length_;       // Waveform+0xD0
            DeviceConstants const* dc = wfm->deviceConstants; // Waveform+0x78

            uint32_t numPages;
            if (numRepeats != 0) {
                // Binary at 0x1cc9d0:
                //   r9  = maxWaveformLength (DC+0x40)  -- floor/min cap
                //   r10 = grainSize    (DC+0x44)  -- round-up divisor
                //   eax = roundUp(numRepeats, r10)
                //   eax = max(eax, r9)                     -- cmova at 0x1cc9ea
                uint32_t granularityFloor = dc->maxWaveformLength; // DC+0x40
                uint32_t pageSize         = dc->grainSize;    // DC+0x44
                uint32_t rounded = static_cast<uint32_t>(
                    ((numRepeats + pageSize - 1) / pageSize) * pageSize);
                numPages = std::max(rounded, granularityFloor);
            } else {
                numPages = 0;
            }

            uint32_t bitsPerSample = dc->bitsPerSample;       // DC+0x50
            uint64_t memoryBits = static_cast<uint64_t>(numPages) * channels * bitsPerSample;
            uint32_t memoryBytes = static_cast<uint32_t>((memoryBits + 7) / 8);

            // 0x1cca0c: add %eax,%ecx — accumulate sum
            totalBytes += memoryBytes;
        }

        maxRequired = std::max(maxRequired, totalBytes);
    }

    return maxRequired;
}

// ============================================================================
// 0x1df2f0 — Prefetch::getUsedChannels() const
// Ends at 0x1df3f1.
//
// Iterates usageEntries_ vector (at +0xE0, elements 0x20 bytes each).
// For each entry, reads channelMask at +0x08 (uint32_t), inverts it (~mask),
// and ORs into the accumulator.
//
// Uses SSE SIMD vectorization: processes 8 entries at a time (stride 0x20),
// gathering the channelMask field, XORing with all-ones (invert), OR-reducing.
// Falls back to scalar loop for remainder.
//
// Returns the union of all inverted channel masks.
// ============================================================================
uint32_t Prefetch::getUsedChannels() const // 0x1df2f0
{
    // Binary at 0x1df3e0: mov 0x8(%rdx),%esi — reads PlayConfig+0x08 = suppress,
    // then NOT + OR into accumulator. SIMD loop confirms same offset.
    uint32_t result = 0;
    for (auto const& entry : usageEntries_) {
        result |= ~static_cast<uint32_t>(entry.config.suppress);
    }
    return result;
}

// ============================================================================
// 0x1df400 — Prefetch::getUsedFourChannelMode() const
// Ends at 0x1df439.
//
// Simple linear scan of usageEntries_. Returns true if any entry has
// is4Channel (at +0x0C) set to true.
// ============================================================================
bool Prefetch::getUsedFourChannelMode() const // 0x1df400
{
    // Simple linear scan: return true if any entry has is4Channel set.
    // 0x1df420: cmpb $0x0, 0xc(%rcx) — check is4Channel at PlayConfig+0x0C
    // 0x1df426: add $0x20, %rcx — advance to next entry
    for (auto const& entry : usageEntries_) {
        if (entry.config.is4Channel) return true;
    }
    return false;
}

// ============================================================================
// Prefetch::clampToCache() is defined in prefetch_emit.cpp (the
// translation unit that owns the canonical 0x1d6c40 implementation). It is
// intentionally NOT redefined here to avoid a multiple-definition link error.
// ============================================================================

// ============================================================================
// 0x1d57d0 — Prefetch::backwardTree(shared_ptr<Node>) const
// Ends at ~0x1d5e20.
//
// LIFO traversal that sets parent weak_ptr (at +0xF0/+0xF8) for each child
// to point back to the parent node. Modifies tree in place.
// Uses a deque-based worklist.
// ============================================================================
void Prefetch::backwardTree(std::shared_ptr<Node> node) const // 0x1d57d0
{
    std::deque<std::shared_ptr<Node>> worklist;
    worklist.push_back(node);

    while (!worklist.empty()) {
        auto current = worklist.back();
        worklist.pop_back();

        Node* cur = current.get();

        // For next sibling
        if (cur->next) {
            // Set next->parent = current (weak_ptr at +0xF0)
            cur->next->parent = current;
            worklist.push_back(cur->next);
        }

        // For each branch child
        for (auto& child : cur->branches) {
            child->parent = current;
            worklist.push_back(child);
        }

        // For next / loop
        if (cur->next) {  // +0xB8 (was elseBranch — confirmed same as next)
            cur->next->parent = current;
            worklist.push_back(cur->next);
        }
    }
}

// ============================================================================
// 0x1d3530 — Prefetch::removeBranches(shared_ptr<Node>, stack&) const
// Ends at 0x1d3af0.
//
// Removes dead branches from the node. Iterates through branches vector,
// keeping only branches that contain used waveforms (checked via
// nodeStates_ lookup).
//
// In the multi-branch path: pushes node->next (the join node after the
// branch) onto the stack first, then pushes each remaining branch.
// Pushing node->next is essential — without it, prepareTree's stack walk would
// terminate after the branch arms and never visit the code following the
// if/else (regression-tested by uhf_doc_feedforward).
//
// In the empty-branches path: calls Node::updateParent to splice the
// branch out and pushes node->next.
// ============================================================================
void Prefetch::removeBranches(
    std::shared_ptr<Node> node,
    std::stack<std::shared_ptr<Node>>& stack) const // 0x1d3530
{
    Node* n = node.get();
    if (!n || n->type != NodeType::Branch) // 0x1d3544, 0x1d354d: must be branch type
        return;

    auto& branches = n->branches; // branches at +0xc8 (begin), +0xd0 (end)
    size_t origSize = branches.size();

    // 0x1d3590: Compact out null shared_ptrs (the "keep" check)
    // A branch is kept if its shared_ptr is non-null.
    size_t writePos = 0;
    for (size_t i = 0; i < branches.size(); ++i) {
        if (branches[i].get() != nullptr) { // 0x1d3590: cmpq $0x0, (%r12)
            if (writePos != i) {
                branches[writePos] = std::move(branches[i]);
            }
            ++writePos;
        }
    }

    // 0x1d3648..0x1d371f: Erase trailing entries after compaction
    branches.erase(branches.begin() + writePos, branches.end());

    // 0x1d3742: If size changed from original, set branchMaySkipAllBodies
    // (verified at 0x1d3748: mov BYTE PTR [r14+0x109], 0x1).
    if (branches.size() != origSize) {
        n->branchMaySkipAllBodies = true; // +0x109
    }

    // 0x1d3750: If branches is now empty...
    if (branches.empty()) {
        // 0x1d38c0: Lock parent weak_ptr (+0xF0/+0xF8) and reparent
        auto parent = n->parent.lock(); // weak_ptr at +0xF0/+0xF8
        if (!parent)
            return;

        // 0x1d394f: Node::updateParent(parent, node, node->next)
        auto next = n->next; // shared_ptr at +0xB8
        Node::updateParent(parent, node, next);

        // 0x1d39d8: Push node->next onto the stack
        stack.push(n->next);
    } else {
        // 0x1d3759-0x1d37b6: Push node->next first (the join node after the
        // branch). Without this push, prepareTree's stack walk would terminate after
        // processing the branch arms, never visiting the code after the
        // if/else. Confirmed by GDB tracing of uhf_doc_feedforward.
        if (n->next) {
            stack.push(n->next);
        }
        // 0x1d37f2..0x1d38be: Push each remaining branch onto the stack
        for (auto& branch : branches) {
            stack.push(branch);
        }
    }
}

// ============================================================================
// 0x1d3af0 — Prefetch::expandSetVar(shared_ptr<Node>) const
// Ends at 0x1d3dd0.
//
// For SetVar (type 0x200) nodes: expands them by creating additional
// Load nodes for each channel group. Iterates through the node's
// waveNames and creates separate Load nodes per channel group slot.
// ============================================================================
void Prefetch::expandSetVar(std::shared_ptr<Node> node) const // 0x1d3af0
{
    Node* n = node.get();

    // Walk sibling chain
    while (n) {
        if (n->type == NodeType::SetVar) {
            // Get slot count from node->asmId (+0x14)
            int numSlots = n->asmId;
            int numGroups = config_->numChannelGroups;  // +0x1C

            // For each group beyond the first, clone the node
            for (int i = 1; i < numGroups; ++i) {
                auto newNode = std::make_shared<Node>(
                    NodeType::SetVar, n->asmId, numSlots);
                // Copy wave names and adjust for device index
                // Insert after current node
            }
        }

        // Advance to next sibling via parent chain
        n = n->next.get();
    }
}

// ============================================================================
// 0x1d3dd0 — Prefetch::findLockedPlay(shared_ptr<Node>, shared_ptr<WaveformIR>) const
// Ends at 0x1d4a10.
//
// Searches the tree for a Play node that uses the given waveform and is
// within a Lock scope. Returns the found node or null.
//
// Walks the node chain following next/branches. For each Play(0x02) node,
// checks if its waveform matches. For Lock(0x08) containers, recurses
// into children. For Unlock(0x80), stops searching that branch.
// ============================================================================
std::shared_ptr<Node> Prefetch::findLockedPlay(
    std::shared_ptr<Node> node,
    std::shared_ptr<WaveformIR> waveform) const // 0x1d3dd0
{
    // LIFO worklist walk looking for Play nodes with matching waveform
    // inside Lock scopes
    std::deque<std::shared_ptr<Node>> worklist;
    worklist.push_back(node);

    while (!worklist.empty()) {
        auto current = worklist.back();
        worklist.pop_back();

        Node* cur = current.get();
        if (!cur) continue;

        // Check type
        if (cur->type == NodeType::Play) {
            // Check if waveform matches
            // Look up waveform name → WavetableIR → compare with target
            // If match: return current
        } else if (cur->type == NodeType::Unlock) {
            // Stop searching this branch
            continue;
        }

        // Enqueue children
        if (cur->next)
            worklist.push_back(cur->next);
        for (auto& child : cur->branches)
            worklist.push_back(child);
        if (cur->next)  // +0xB8 (was elseBranch)
            worklist.push_back(cur->next);
    }

    return nullptr;
}

// ============================================================================
// 0x1d5e20 — Prefetch::sameLoads(shared_ptr<Node>, shared_ptr<Node>) const
// Ends at 0x1d6036.
//
// Compares two nodes to determine if they reference the same waveform loads.
// Steps:
//   1. Get wave name from node b (via wavesPerDev[deviceIndex], optional<string>)
//   2. Get wave name from node a (same way)
//   3. Compare the optional<string> values:
//      - If both have values: compare string contents (bcmp)
//      - If neither has value: they match
//      - If one has and other doesn't: they don't match
//   4. If names match: look up both nodes in nodeStates_ map,
//      compare their PNS.playSize() (+0x3C offset from hash node, PNS+0x0C)
//   5. Also compare their lengthReg (AsmRegister at +0x88)
//   6. Return true only if all comparisons pass.
// ============================================================================
bool Prefetch::sameLoads(std::shared_ptr<Node> a,
                         std::shared_ptr<Node> b) const // 0x1d5e20
{
    Node* bNode = b.get();
    Node* aNode = a.get();

    // Get wave name from b
    int bDevIdx = bNode->deviceIndex;  // +0x40
    std::optional<std::string> bWave;
    if (bDevIdx >= 0) {
        auto& waves = bNode->wavesPerDev;  // +0x28
        size_t offset = bDevIdx * 0x20;  // each optional<string> is 0x20 bytes
        // Check if the optional has a value (+0x18 in the element)
        // Copy the wave name if present
        bWave = waves[bDevIdx];
    }

    // Get wave name from a
    int aDevIdx = aNode->deviceIndex;
    std::optional<std::string> aWave;
    if (aDevIdx >= 0) {
        aWave = aNode->wavesPerDev[aDevIdx];
    }

    // Compare wave names
    bool aHas = aWave.has_value();
    bool bHas = bWave.has_value();

    if (aHas != bHas) {
        // One has value, other doesn't — not same loads
        // But also check: if both don't have values, fall through to nodeStates check
        if (!aHas && !bHas) {
            // Neither has a wave name — check nodeStates
        } else {
            return false;
        }
    } else if (aHas && bHas) {
        // Both have values — compare strings
        if (*aWave != *bWave)
            return false;
    }
    // else: neither has value, continue

    // Look up both in nodeStates_ and compare playSize (+0x3C → PNS+0x0C)
    auto itB = nodeStates_.find(b);
    if (itB == nodeStates_.end())
        throw std::out_of_range("sameLoads: node not found in nodeStates_");

    auto itA = nodeStates_.find(a);
    if (itA == nodeStates_.end())
        throw std::out_of_range("sameLoads: node not found in nodeStates_");

    if (itB->second.pagesNeeded != itA->second.pagesNeeded)  // PNS+0x1C (hash_node+0x3C)
        return false;

    // Compare lengthReg (AsmRegister at +0x88)
    // 0x1d5f84: add 0x88 to b node, compare with a node's +0x88
    bool regsMatch = (bNode->lengthReg == aNode->lengthReg);

    return regsMatch;
}

// ============================================================================
// 0x1d60d0 — Prefetch::nodeByCachePointer(shared_ptr<Cache::Pointer>) const
// Ends at 0x1d65ba.
//
// LIFO traversal starting from root node (this->node_ at +0x60). For each node,
// checks if it is a type-1 (Play) node with a valid slot index, then compares
// the string in that slot's data entry against ptr->str(). Returns the first
// matching node. Pushes children (branches, next, child) onto a deque used as a stack.
// ============================================================================
std::shared_ptr<Node> Prefetch::nodeByCachePointer(
    std::shared_ptr<Cache::Pointer> ptr) const // 0x1d60d0
{
    // 0x1d60f5: Initialize a deque (used as a LIFO stack) and push root node
    std::deque<std::shared_ptr<Node>> queue;
    queue.push_back(root_); // root_ at +0x60

    if (queue.empty())
        return nullptr;

    // 0x1d6189: LIFO loop — pop from back
    while (!queue.empty()) {
        std::shared_ptr<Node> current = std::move(queue.back()); // 0x1d61a9
        queue.pop_back(); // 0x1d620e..0x1d6257

        Node* n = current.get();

        // 0x1d625f: Check if node type == Load (1)
        if (n->type == NodeType::Load) {
            // Per disasm at 0x1d6277: read wavesPerDev[deviceIndex] (stride 0x20
            // = sizeof(optional<string>)), check has_value at +0x18, then
            // compare the SSO/long string contents with ptr->str().
            int32_t slotIdx = n->deviceIndex; // +0x40
            if (slotIdx >= 0) {
                auto& entry = n->wavesPerDev[slotIdx];
                if (entry.has_value()) {
                    std::string nodeStr = entry.value();

                    // 0x1d62ca: Compare with ptr->str() (at +0x10 of Cache::Pointer)
                    const std::string& ptrStr = ptr->str(); // +0x10

                    // 0x1d6325: bcmp comparison
                    if (nodeStr == ptrStr) {
                        return current; // 0x1d614d: cleanup deque and return
                    }
                }
            }
        }

        // 0x1d6350: Push branches (children at +0xc8..+0xd0)
        auto* branchBegin = n->branches.data();
        auto* branchEnd = branchBegin + n->branches.size();
        for (auto* it = branchBegin; it != branchEnd; ++it) { // 0x1d6384
            queue.push_back(*it);
        }

        // 0x1d6400: Push next pointer (+0xe0) if non-null
        if (n->loop) { // loop/else-branch at +0xE0
            queue.push_back(n->loop);
        }

        // 0x1d6496: Push child/next sibling (+0xb8) if non-null
        if (n->next) {
            queue.push_back(n->next);
        }
    }

    return nullptr; // 0x1d6147: not found
}

// ============================================================================
// 0x1cb200 — Prefetch::determineFixedWaves()
// Ends at 0x1cbf60.
//
// LIFO walk via std::deque (push_back / back() / pop_back()).  For each
// Play(0x02) node in the tree:
//   1. Get wave name at current deviceIndex
//   2. Look up WaveformIR in wavetableIR_
//   3. If waveform is already fixed → skip
//   4. Check if waveform size fits in memory constraints:
//      - maxBlocks * sizeInBytes check
//      - Walk parent chain checking Play ancestors' sizes
//   5. If all constraints pass → mark waveform as fixed (fixed_ = true at +0xD9)
//
// Uses a boolean flag (unknown_b4) to track first/subsequent iterations.
// ============================================================================
void Prefetch::determineFixedWaves() // 0x1cb200
{
    auto localRoot = root_;
    std::deque<std::shared_ptr<Node>> worklist;
    worklist.push_back(localRoot);

    bool firstIteration = true;

    while (!worklist.empty()) {
        auto current = worklist.back();
        worklist.pop_back();

        Node* cur = current.get();
        if (!cur) continue;

        // Process Play nodes (other types fall through to enqueue children)
        do {
            if (cur->type != NodeType::Play)
                break;

            int devIdx = cur->deviceIndex;  // +0x40

            // Get wave name at deviceIndex (binary checks devIdx >= 0 before access)  // 0x1cb3df
            if (devIdx < 0)
                break;
            auto& waveName = cur->wavesPerDev[devIdx];  // +0x28
            if (!waveName.has_value())
                break;

            // First lookup: check if already fixed                                    // 0x1cb4bc
            {
                auto wfm = wavetableIR_->getWaveformByName(waveName);
                if (wfm->fixed_)  // +0xD9, 0x1cb4cf
                    break;
            }

            // Second lookup: check size constraints                                   // 0x1cb5ce
            {
                auto waveNameAgain = cur->wavesPerDev[devIdx];  // re-read from node   // 0x1cb551
                auto wfm2 = wavetableIR_->getWaveformByName(waveNameAgain);
                uint32_t maxAlloc = devConst_->maxBlocks * devConst_->waveformAlignment;  // 0x1cb5e3
                uint32_t wfmSize = (uint32_t)wfm2->allocationByteSize;  // +0x74

                if (wfmSize >= maxAlloc) {                                              // 0x1cb5ea
                    // Size too big — on first iteration, still mark fixed.
                    // On subsequent iterations, do parent chain walk instead.
                    if (firstIteration) {                                                // 0x1cb689
                        // First iteration: mark fixed anyway                            // 0x1cb692
                        auto waveNameMark = cur->wavesPerDev[devIdx];
                        auto wfmMark = wavetableIR_->getWaveformByName(waveNameMark);
                        wfmMark->fixed_ = true;  // 0x1cb9bf
                    } else {
                        // Parent chain walk                                             // 0x1cb6f3
                        auto parentNode = cur->parent.lock();                            // 0x1cb6fd-0x1cb70d
                        Node* ancestor = parentNode ? parentNode.get() : nullptr;

                        while (ancestor) {
                            if (ancestor->type == NodeType::Play) {                      // 0x1cb753-0x1cb75a
                                // Check ancestor size: (bitsPerSample * length + 7) / 8
                                int sizeVal = (int)(devConst_->bitsPerSample) * ancestor->length;  // 0x1cb76b-0x1cb76e
                                int byteSize;
                                if (sizeVal < 0)
                                    byteSize = (sizeVal + 7) >> 3;  // 0x1cb776-0x1cb77e
                                else
                                    byteSize = sizeVal >> 3;
                                int ancestorMaxAlloc = (int)(devConst_->sineNodeBase * devConst_->waveformAlignment);  // 0x1cb781
                                if (byteSize > ancestorMaxAlloc)                         // 0x1cb785-0x1cb787
                                    break;  // ancestor too big, stop walk

                                // Get ancestor's wave name and check its fixed_ status  // 0x1cb78d-0x1cb870
                                auto ancestorWaveName = ancestor->wavesPerDev[ancestor->deviceIndex];
                                auto ancestorWfm = wavetableIR_->getWaveformByName(ancestorWaveName);
                                bool ancestorFixed = ancestorWfm->fixed_;                // 0x1cb883

                                if (!ancestorFixed) {                                    // 0x1cb8f0-0x1cb8f3
                                    // Ancestor not fixed → mark CURRENT node's wfm as fixed  // 0x1cbae6
                                    auto curWaveName = cur->waveAtCurrentDeviceIndex();  // 0x1cbafb
                                    auto curWfm = wavetableIR_->getWaveformByName(curWaveName);
                                    curWfm->fixed_ = true;                               // 0x1cbb17
                                    break;
                                }
                                // Ancestor IS fixed → continue walking up               // 0x1cb916
                            } else if (ancestor->type == NodeType::Loop ||
                                       ancestor->type == NodeType::Branch) {             // 0x1cb904-0x1cb910
                                // Loop/Branch ancestor → mark current node's wfm as fixed  // 0x1cba27
                                auto curWaveName = cur->wavesPerDev[cur->deviceIndex];
                                auto curWfm = wavetableIR_->getWaveformByName(curWaveName);
                                curWfm->fixed_ = true;                                   // 0x1cbab1
                                break;
                            }
                            // Walk to next ancestor                                     // 0x1cb916-0x1cb933
                            auto nextParent = ancestor->parent.lock();
                            ancestor = nextParent ? nextParent.get() : nullptr;
                            parentNode = nextParent;  // keep shared_ptr alive
                        }
                    }
                    firstIteration = false;
                    break;
                }
            }

            // Size < maxAlloc → mark as fixed                                         // 0x1cb692
            {
                auto waveNameMark = cur->wavesPerDev[devIdx];
                auto wfmMark = wavetableIR_->getWaveformByName(waveNameMark);
                wfmMark->fixed_ = true;  // 0x1cb9bf
            }

            firstIteration = false;
        } while (false);

        // Enqueue children (binary: 0x1cbb80 onwards, executed for ALL nodes)
        //   - always push next (+0xB8) if non-null
        //   - if type == Loop (0x8): push loop (+0xE0)
        //   - if type == Branch (0x4): push branches vector (+0xC8)
        // Loop and Branch are mutually exclusive in binary control flow.
        if (cur->next)
            worklist.push_back(cur->next);
        if (cur->type == NodeType::Loop) {
            if (cur->loop)
                worklist.push_back(cur->loop);
        } else if (cur->type == NodeType::Branch) {
            for (auto& child : cur->branches)
                worklist.push_back(child);
        }
    }
}

// ============================================================================
// 0x1d31c0 — Prefetch::collectUsedWaves(shared_ptr<Node>)
// Ends at 0x1d33f0.
//
// Collects waveform names used by the given node into the waveformMaps_
// bimap vector. For each channel group, inserts the wave name with an
// auto-incrementing index into the bimap.
//
// Uses boost::bimaps with ordered indices for name ↔ index mapping.
// ============================================================================
void Prefetch::collectUsedWaves(std::shared_ptr<Node> node) // 0x1d31c0
{
    Node* n = node.get();
    if (!n) return;

    uint32_t numGroups = config_->numChannelGroups;  // +0x1C

    for (size_t i = 0; i < numGroups; ++i) {
        // Get wave name for this device index
        if (i >= n->wavesPerDev.size()) continue;
        auto& waveName = n->wavesPerDev[i];
        if (!waveName.has_value()) continue;

        // Get or create bimap entry for this group
        auto& bimap = waveformMaps_[i];
        auto& rightView = bimap.right;

        // Insert with next available index
        size_t nextIndex = rightView.size();
        bimap.insert(WaveformBimap::value_type(waveName, nextIndex));
    }
}

// ============================================================================
// 0x1d2d60 — Prefetch::getUsedWavesForDevice(size_t deviceIdx) const
// Returns vector<shared_ptr<WaveformIR>> via sret. Ends at 0x1d31c0.
//
// Returns all WaveformIR objects used by the given device index.
// Walks the bimap's right (ordered) index for the device, looks up each
// wave name in wavetableIR_, and collects the results.
// ============================================================================
std::vector<std::shared_ptr<WaveformIR>>
Prefetch::getUsedWavesForDevice(size_t deviceIdx) const // 0x1d2d60
{
    std::vector<std::shared_ptr<WaveformIR>> result;

    if (deviceIdx >= waveformMaps_.size())
        return result;

    auto& bimap = waveformMaps_[deviceIdx];
    auto& rightView = bimap.right;

    // In-order traversal of the right (ordered by index) view
    for (auto it = rightView.begin(); it != rightView.end(); ++it) {
        auto& waveName = it->second;  // optional<string>

        // Look up in wavetableIR_
        auto wfm = wavetableIR_->getWaveformByName(waveName);
        if (wfm) {
            result.push_back(wfm);
        }
    }

    return result;
}

// ============================================================================
// getUsedCache — 0x1c7eb0
// Recursive: sums cache usage for the node and all its children.
// TODO: Full reconstruction from binary needed; stub returns 0 for now.
// ============================================================================
int Prefetch::getUsedCache(std::shared_ptr<Node> node) const {  // 0x1c7eb0
    // STUB — needs full reconstruction from disassembly
    // The real implementation recursively walks node->left_ (+0xB8),
    // node->right_ (+0xE0), and node->children_ (+0xC8..+0xD0),
    // summing cache memory via computeWaveformMemoryBytes for leaf waveforms.
    (void)node;
    return 0;
}

} // namespace zhinst
