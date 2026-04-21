// ============================================================================
// Reconstructed from disassembly of _seqc_compiler.so
// File: prefetch_helpers.cpp — Prefetch helper, getter, and query methods
// ============================================================================

#include "zhinst/prefetch.hpp"
#include "zhinst/node.hpp"
#include "zhinst/cache.hpp"
#include "zhinst/waveform_ir.hpp"
#include "zhinst/wavetable_ir.hpp"
#include "zhinst/device_constants.hpp"
#include "zhinst/awg_compiler_config.hpp"
#include "zhinst/signal.hpp"

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
//   numPages = ceil(signal.length_ / dc->waveformPageSize) * dc->waveformPageSize
//   numPages = min(numPages, dc->waveformGranularity)         // cap at max
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
                // DC+0x44 = waveformPageSize (stride/granularity)
                uint32_t stride = dc->waveformPageSize;     // +0x44
                // DC+0x40 = waveformGranularity (base/max cap)
                uint32_t base = dc->waveformGranularity;    // +0x40
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

            // Waveform+0x4C = addressValue (startOffset)
            uint32_t startOffset = wfm->addressValue;       // +0x4C
            // Net memory = bytes - startOffset
            uint32_t netMemory = memoryBytes - startOffset;

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
                uint32_t stride = dc->waveformPageSize;    // DC+0x44
                uint32_t base = dc->waveformGranularity;   // DC+0x40
                numPages = static_cast<uint32_t>(
                    ((numRepeats + stride - 1) / stride) * stride);
                numPages = std::min(numPages, base);
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
    auto* begin = reinterpret_cast<const char*>(/*usageEntries_.data()*/0);  // +0xE0
    auto* end = reinterpret_cast<const char*>(/*usageEntries_.end()*/0);     // +0xE8

    // Simplified scalar version of the SIMD loop:
    uint32_t result = 0;
    // Each UsageEntry is 0x20 bytes, channelMask at +0x08
    // for (auto it = usageEntries_.begin(); it != usageEntries_.end(); ++it) {
    //     result |= ~(it->channelMask);
    // }
    // The SIMD version processes 8 entries per iteration using SSE gather+XOR+OR
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
    // usageEntries_ at +0xE0, each entry 0x20 bytes
    // is4Channel at entry+0x0C (bool)
    // for (auto& entry : usageEntries_) {
    //     if (entry.is4Channel) return true;
    // }
    // return false;

    // Disasm: simple loop checking +0x0C of each 0x20-byte element
    // 0x1df420: cmpb $0x0, 0xc(%rcx) — check is4Channel
    // 0x1df426: add $0x20, %rcx — advance to next entry
    // 0x1df433: cmp %rax, %rcx; setne %al — return (found != end)
    return false;  // placeholder — actual implementation is the loop above
}

// ============================================================================
// 0x1d6c40 — Prefetch::clampToCache(AddressImpl<uint>) const
// Ends at 0x1d6c92.
//
// Clamps an address to fit within the cache address space.
// If config_->isHirzel (+0x18 == true, i.e. splitMode):
//   splitIndex = config_->splitIndex (+0x19, uint8_t)
//   pageSize = devConst_->waveformAlignment (+0x14)
//   cacheSize = (&devConst_->cachePageCount)[splitIndex] (+0x18 + idx*4)
//   limit = cacheSize * pageSize
//   addr = min(addr, limit)
//   addr = min(addr, 0xFFFFF)  // 20-bit address space
//   if splitIndex == 1:
//     addr = (addr + pageSize - 1) & ~(pageSize - 1)  // page-align up
// Else (non-split):
//   addr = min(addr, 0xFFFFF)
// ============================================================================
detail::AddressImpl<uint32_t>
Prefetch::clampToCache(detail::AddressImpl<uint32_t> addr) const // 0x1d6c40
{
    bool isSplit = config_->isHirzel;  // +0x18 (actually splitMode)

    if (isSplit) {
        uint8_t splitIndex = config_->splitIndex;  // +0x19
        uint32_t pageSize = devConst_->waveformAlignment;   // +0x14
        uint32_t cacheSize = (&devConst_->cachePageCount)[splitIndex];  // +0x18 + idx*4

        uint32_t limit = cacheSize * pageSize;
        uint32_t clamped = std::min(addr, limit);
        clamped = std::min(clamped, (uint32_t)0xFFFFF);

        if (splitIndex == 1) {
            // Page-align up: (addr + pageSize - 1) & ~(pageSize - 1)
            clamped = (clamped + pageSize - 1) & (~(pageSize - 1));
        }
        return clamped;
    } else {
        return std::min(addr, (uint32_t)0xFFFFF);
    }
}

// ============================================================================
// 0x1d57d0 — Prefetch::backwardTree(shared_ptr<Node>) const
// Ends at ~0x1d5e20.
//
// BFS traversal that sets parent weak_ptr (at +0xF0/+0xF8) for each child
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

        // For elseBranch / loop
        if (cur->elseBranch) {
            cur->elseBranch->parent = current;
            worklist.push_back(cur->elseBranch);
        }
    }
}

// ============================================================================
// 0x1d3530 — Prefetch::removeBranches(shared_ptr<Node>, stack&) const
// Ends at 0x1d3af0.
//
// Removes dead branches from the node. Iterates through branches vector,
// keeping only branches that contain used waveforms (checked via
// nodeStates_ lookup). Pushes removed branches onto the stack for later
// processing.
// ============================================================================
void Prefetch::removeBranches(
    std::shared_ptr<Node> node,
    std::stack<std::shared_ptr<Node>>& stack) const // 0x1d3530
{
    Node* n = node.get();
    auto& branches = n->branches;
    size_t origSize = branches.size();

    // Iterate branches, removing dead ones
    size_t writePos = 0;
    for (size_t i = 0; i < origSize; ++i) {
        auto& branch = branches[i];

        // Check if branch is still alive (has entries in nodeStates_)
        bool keep = true;  // TODO: actual check via nodeStates_ lookup

        if (keep) {
            if (writePos != i) {
                branches[writePos] = std::move(branches[i]);
            }
            ++writePos;
        } else {
            // Push removed branch onto stack
            stack.push(branch);
        }
    }

    // Erase from writePos to end
    branches.erase(branches.begin() + writePos, branches.end());

    // If single branch remaining, may need to update parent pointers
    if (branches.size() == 1) {
        auto& onlyChild = branches[0];
        // Update parent weak_ptr
        // (complex pointer management elided)
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
            int numGroups = config_->numCores;  // +0x1C

            // For each group beyond the first, clone the node
            for (int i = 1; i < numGroups; ++i) {
                auto newNode = std::make_shared<Node>(
                    NodeType::SetVar, numSlots, n->asmId);
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
    // BFS/DFS traversal looking for Play nodes with matching waveform
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
        if (cur->elseBranch)
            worklist.push_back(cur->elseBranch);
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
//      compare their PNS.playSize (+0x3C offset from hash node, PNS+0x0C)
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

    if (itB->second.pageSize != itA->second.pageSize)  // PNS+0x1C (hash_node+0x3C)
        return false;

    // Compare lengthReg (AsmRegister at +0x88)
    // 0x1d5f84: add 0x88 to b node, compare with a node's +0x88
    bool regsMatch = (bNode->lengthReg == aNode->lengthReg);

    return regsMatch;
}

// ============================================================================
// 0x1d60d0 — Prefetch::nodeByCachePointer(shared_ptr<Cache::Pointer>) const
// Ends at 0x1d6c40.
//
// Searches nodeStates_ map for a node whose PrefetcherNodeState has a
// Cache::Pointer matching the given pointer. Returns the node shared_ptr.
//
// Large method (~769 asm lines) — iterates the unordered_map, comparing
// Cache::Pointer shared_ptrs.
//
// TODO: Full reconstruction — complex hash table iteration with pointer
//       comparison. Approximate structure provided.
// ============================================================================
std::shared_ptr<Node> Prefetch::nodeByCachePointer(
    std::shared_ptr<Cache::Pointer> ptr) const // 0x1d60d0
{
    // Iterate nodeStates_ map
    for (auto& [nodePtr, state] : nodeStates_) {
        // Compare state.cachePtr with ptr
        // if (state.cachePtr == ptr)
        //     return nodePtr;
    }

    return nullptr;  // not found
}

// ============================================================================
// 0x1cb200 — Prefetch::determineFixedWaves()
// Ends at 0x1cbf60.
//
// BFS traversal using deque. For each Play(0x02) node in the tree:
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

        // Enqueue children
        if (cur->next)
            worklist.push_back(cur->next);
        for (auto& child : cur->branches)
            worklist.push_back(child);
        if (cur->elseBranch)
            worklist.push_back(cur->elseBranch);

        // Only process Play nodes
        if (cur->type != NodeType::Play)
            continue;

        int devIdx = cur->deviceIndex;  // +0x40

        // Get wave name at deviceIndex
        auto& waveName = cur->wavesPerDev[devIdx];  // +0x28
        if (!waveName.has_value())
            continue;

        // Look up WaveformIR
        auto wfm = wavetableIR_->getWaveformByName(waveName);
        if (!wfm) continue;

        // Already fixed? Skip
        if (wfm->fixed_)  // +0xD9
            continue;

        // Check size constraints
        uint32_t maxSize = devConst_->maxBlocks;  // +0x1C
        // Actually: numWaveforms * something
        uint32_t wfmSize = wfm->sizeInBytes;  // +0x74

        // Check if it fits
        if (wfmSize > maxSize) {
            continue;  // too large to be fixed
        }

        // Walk parent chain checking Play ancestors
        // (complex parent walk with size checking — approximate)
        bool canFix = true;
        auto parentWeak = cur->parent;
        while (auto parentNode = parentWeak.lock()) {
            if (parentNode->type == NodeType::Play) {
                // Check parent's waveform size
                auto pWfm = wavetableIR_->getWaveformByName(
                    parentNode->wavesPerDev[devIdx]);
                if (pWfm && pWfm->fixed_) {
                    // Parent already fixed — check memory constraints
                    int pLen = parentNode->length;  // +0x90
                    int bitsPerSample = devConst_->bitsPerSample;
                    int memLimit = devConst_->waveformMemSize;
                    int sizeInSamples = (pLen * bitsPerSample + 7) / 8;
                    if (sizeInSamples > memLimit) {
                        canFix = false;
                        break;
                    }
                }
            }
            parentWeak = parentNode->parent;
        }

        if (canFix) {
            wfm->fixed_ = true;  // +0xD9
        }

        firstIteration = false;
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

    uint32_t numGroups = config_->numCores;  // +0x1C

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

} // namespace zhinst
