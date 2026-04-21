// ============================================================================
// Reconstructed from disassembly of _seqc_compiler.so
// Class: zhinst::Prefetch — tree preparation methods
//
// Methods:
//   preparePlays()            0x1c8740 .. 0x1c8870
//   prepareTree(node)         0x1c8870 .. 0x1c9b30
//   countBranches(node)       0x1c9b30 .. 0x1ca370
//   definePlaySize(node)      0x1ca370 .. 0x1cb200
// ============================================================================

#include "zhinst/prefetch.hpp"
#include "zhinst/node.hpp"
#include "zhinst/wavetable_ir.hpp"
#include "zhinst/waveform_ir.hpp"
#include "zhinst/signal.hpp"
#include "zhinst/asm_register.hpp"

#include <stack>
#include <deque>
#include <optional>
#include <algorithm>
#include <cmath>

namespace zhinst {

// ============================================================================
// preparePlays() — 0x1c8740
// Orchestrator: calls the three tree-preparation passes in sequence.
// 1. prepareTree(root_)     — classify nodes, insert Load nodes, handle Wait
// 2. countBranches(root_)   — count branching paths, update nodeStates_
// 3. definePlaySize(root_)  — compute waveform play sizes, assign registers
// ============================================================================
void Prefetch::preparePlays() // 0x1c8740
{
    prepareTree(root_);       // 0x1c8770
    countBranches(root_);     // 0x1c87bd
    definePlaySize(root_);    // 0x1c880a
}

// ============================================================================
// prepareTree(node) — 0x1c8870
//
// BFS traversal over the node tree using a deque as a stack.
// For each node, dispatches on node->type (+0x44) to perform tree
// transformations:
//
// - Play (0x01): Extract wave name at node->deviceIndex from wavesPerDev.
//                Also iterates node->play (+0xA0..+0xA8 range) to check
//                for valid waves; if any found, calls collectUsedWaves
//                and pushes next to stack.
//
// - Load (0x02): Calls linkLoad(node), then pushes next to stack.
//
// - Branch (0x04): Calls removeBranches(node, stack). This flattens
//                  branches into the traversal stack.
//
// - Loop (0x08):  Pushes both next (+0xB8) and loop (+0xE0) to stack.
//
// - SetVar (0x10): Calls expandSetVar(node), then pushes next.
//
// - Rate (0x20):  (falls through to default — just pushes next)
//
// - Wait (0x40): If config_->field_0x18 is false (not append mode):
//                  For each branch child (iterating node->next chain):
//                    Get wave name, look up WaveformIR via wavetableIR_,
//                    call findLockedPlay to check if wave is already locked.
//                    If locked play found: remove the wait node,
//                    and if there's a next load, merge them via mergeLoads.
//                    Otherwise: create a new load via createLoad,
//                    and if next load exists, insert it before via insertBefore.
//                  At end, for all valid waves in play range, mark them
//                  as used in the WaveformIR (set waveform->used_ = true
//                  at +0xD8).
//                  Then calls collectUsedWaves on this node.
//
// - Table (0x200): Same as Load — calls linkLoad, pushes next.
//
// - PlainLoad (0x4000): Calls collectUsedWaves, pushes next.
//
// - Default: Just pushes next to stack.
//
// After processing each node, the loop pops the next from the deque
// and continues until the deque is empty.
// ============================================================================
void Prefetch::prepareTree(std::shared_ptr<Node> node) // 0x1c8870
{
    // Local deque used as traversal stack
    std::deque<std::shared_ptr<Node>> stack;  // -0x60(%rbp)
    stack.push_back(node);                    // 0x1c889e

    // Cache wavetableIR_ shared_ptr into a local  // 0x1c88e4
    auto wavetableIR = wavetableIR_;              // -0x108(%rbp) / -0x68(%rbp)

    while (!stack.empty()) {                      // 0x1c890b / 0x1c8920
        // Pop from back (LIFO traversal)
        auto current = stack.back();              // -0xa0(%rbp)
        stack.pop_back();                         // 0x1c89b7..0x1c8a05

        if (!current)                             // 0x1c8a0c
            goto cleanup_validwaves;

        // Collect valid waves for this node (copy_if on wavesPerDev)
        // into a local vector<optional<string>>
        std::vector<std::optional<std::string>> validWaves; // -0xe0(%rbp)
        // calls Node::allValidWaves() pattern: copy_if on wavesPerDev  // 0x1c8a2a
        // validWaves = current->allValidWaves();

        NodeType type = current->type;            // 0x1c8a4c: mov 0x44(%rbx),%eax

        switch (type) {
        // ---- Play (0x01) ---- // jump table case 0 → 0x1c8a6f
        case NodeType::Play: {
            // Get wave name at deviceIndex from wavesPerDev
            int devIdx = current->deviceIndex;    // 0x1c8a6f: movslq 0x40(%rbx)
            if (devIdx >= 0) {
                auto& wave = current->wavesPerDev[devIdx]; // 0x1c8a7c..0x1c8a94
                if (wave.has_value()) {
                    // Wave name is valid — will be processed
                    // (stores to local optional)
                }
            }
            // Then iterate current->play range (+0xA0..+0xA8)
            auto playBegin = /* current->play.begin() */ current->play.data();  // 0x1c9636
            auto playEnd = /* current->play.end() */   current->play.data() + current->play.size(); // 0x1c963d
            if (playBegin == playEnd) {
                // No plays — fall through to push next
                goto push_next_and_loop;          // 0x1c9664 → 0x1c966a
            }
            // Plays exist: for each valid wave, mark waveform as used (+0xD8)
            // 0x1c97b9..0x1c98c5 loop
            for (auto it = playBegin; it != playEnd; ++it) {
                auto waveform = wavetableIR->getWaveformByName(*it); // 0x1c97fe
                if (waveform) {
                    // Get waveform again and set used flag
                    auto wf2 = wavetableIR->getWaveformByName(*it); // 0x1c9844
                    wf2->used_ = true;  // 0x1c9850: movb $0x1, 0xd8(%rax)
                }
            }
            // Then call collectUsedWaves on this node
            collectUsedWaves(current);            // 0x1c991c
            goto push_next_and_loop;
        }

        // ---- Load (0x02) ---- // jump table case 1 → 0x1c8d13
        case NodeType::Load: {
            linkLoad(current);                    // 0x1c8d3e
            // Push next (+0xB8) to stack
            if (current->next)                    // 0x1c8d66: mov 0xb8(%rbx)
                stack.push_back(current->next);
            goto cleanup_validwaves;
        }

        // ---- Branch (0x04) ---- // jump table case 3 → 0x1c8cac
        case NodeType::Branch: {
            removeBranches(current, stack);        // 0x1c8cd7
            goto cleanup_validwaves;
        }

        // ---- Loop (0x08) ---- // jump table case 7 → 0x1c8b85
        case NodeType::Loop: {
            // Push next to stack if present
            if (current->next)                    // 0x1c8b85: mov 0xb8(%rbx)
                stack.push_back(current->next);
            // Push loop child to stack if present
            if (current->loop)                    // 0x1c8c1c: mov 0xe0(%rbx)
                stack.push_back(current->loop);
            goto cleanup_validwaves;
        }

        // ---- SetVar (0x10) ---- // jump table case 15 → 0x1c8db9
        case NodeType::SetVar: {
            expandSetVar(current);                // 0x1c8de3
            // Push next to stack
            if (current->next)
                stack.push_back(current->next);
            goto cleanup_validwaves;
        }

        // ---- Wait (0x40) ---- // explicit cmp 0x40 → 0x1c8e64
        case NodeType::Lock: {
            // Check if config_->isHirzel (+0x18) is set
            // 0x1c8e64: mov -0x70(%rbp),%rax; mov (%rax),%rax; cmpb $0x0,0x18(%rax)
            if (config_->isHirzel) {
                // If append mode, just push next to stack
                goto push_next_and_loop;          // 0x1c9514
            }

            // Not append mode: process Wait node for prefetch insertion
            // Loop over the next chain starting from current
            auto waitNode = current;              // -0xb0(%rbp)
            std::shared_ptr<Node> lastLoad;       // -0xc0(%rbp), initially null

            while (true) {
                // Get next node in chain via waitNode->next
                auto nextNode = waitNode->next;   // 0x1c8eae: movups 0xb8(%rbx)

                // Get wave name for this node
                int devIdx2 = current->deviceIndex; // 0x1c8ed8
                std::optional<std::string> waveName;
                if (devIdx2 >= 0) {
                    waveName = current->wavesPerDev[devIdx2];
                }

                // Look up WaveformIR
                auto waveform = wavetableIR->getWaveformByName(waveName);  // 0x1c8f5a

                // Try to find a locked play for this waveform
                auto lockedPlay = findLockedPlay(nextNode, waveform);      // 0x1c8f70

                // Move lockedPlay to waitNode context
                waitNode = lockedPlay;  // -0xb0(%rbp) ← result  // 0x1c8f9b

                if (lockedPlay) {
                    // Found locked play: remove the wait node
                    // 0x1c9095: Lock into parent via weak_ptr
                    auto parent = waitNode->parent.lock();  // 0x1c90af
                    Node::remove(waitNode);                 // 0x1c90d2

                    // If lastLoad exists, merge
                    if (lastLoad) {
                        // Lock parent again
                        auto parent2 = waitNode->parent.lock();
                        mergeLoads(parent2, lastLoad);     // 0x1c9170
                    }
                } else {
                    // No locked play: create a new Load node
                    auto loadNode = createLoad(waitNode);  // 0x1c9202
                    // createLoad returns into lastLoad (-0xc0)
                    lastLoad = loadNode;
                }

                // If lastLoad exists, set its asmId from current node's asmId
                if (lastLoad) {
                    // 0x1c9312: mov 0x14(%rdi),%ecx; mov %ecx,0x14(%rax)
                    lastLoad->asmId = current->asmId;
                    // Insert lastLoad before waitNode
                    current->insertBefore(lastLoad);       // 0x1c9345
                }

                // Move to next iteration (waitNode = waitNode->next chain)
                // Loop continues while waitNode->next exists // 0x1c8ea7..0x1c9302
                if (!lastLoad)
                    break;
                // (loop restarts at 0x1c8ea0)
            }
            goto push_next_and_loop;
        }

        // ---- Table (0x200) ---- // explicit cmp 0x200 → 0x1c93b6
        case NodeType::Table: {
            linkLoad(current);                    // 0x1c93e1
            // Push next to stack
            if (current->next)
                stack.push_back(current->next);
            goto cleanup_validwaves;
        }

        // ---- PlainLoad (0x4000) ---- // explicit cmp 0x4000 → 0x1c8adf
        case NodeType::PlainLoad: {
            collectUsedWaves(current);            // 0x1c8b0a
            // Push next to stack
            if (current->next)
                stack.push_back(current->next);
            goto cleanup_validwaves;
        }

        // ---- Default ---- // 0x1c945c
        default: {
push_next_and_loop:
            // Push next to stack if present
            if (current->next)
                stack.push_back(current->next);
            goto cleanup_validwaves;
        }
        } // end switch

cleanup_validwaves:
        // Destroy the validWaves vector (free heap strings)
        // 0x1c9708..0x1c977d
        validWaves.clear();
        // validWaves dtor runs here

        // Continue loop (pop next from deque)
    } // end while

    // Cleanup: release wavetableIR local shared_ptr // 0x1c9958
    // deque destructor runs // 0x1c9984..0x1c9988
}

// ============================================================================
// countBranches(node) — 0x1c9b30
//
// BFS traversal that counts maximum branch depth for each node.
// For each node, inserts it into nodeStates_ map (emplace with default
// PrefetcherNodeState), then processes based on type:
//
// - Branch (0x04):
//     Gets branchCount from nodeStates_[current] (+0x34 in PNS).
//     Iterates branches vector (+0xC8..+0xD0), counts them.
//     branchCount += branches.size() - 1
//     Updates max in Prefetch.maxBranches_ (+0xB8 in Prefetch — wait,
//       that's pageSize_! Actually: uses this->field at +0xB8 of Prefetch
//       as maxBranches storage — 0x1c9da6: mov -0x80(%rbp),%rsi;
//       mov 0xb8(%rsi),%ecx — this is Prefetch +0xB8 = pageSize_)
//     Actually reconsidering: pageSize_ is at +0xB8 of Prefetch but this
//     code reads it as maxBranches_. The constructor sets it to 1, and
//     countBranches takes max(current, branchCount). This IS maxBranches
//     stored in what was labeled pageSize_.
//     >>> DISCOVERY: +0xB8 is maxBranches_ (int), NOT pageSize_.
//         The constructor init of 1 makes sense as initial max-branches.
//
//     Then pushes next (+0xB8 of node) to stack.
//     For each branch child, looks up its nodeStates_ entry and
//     compares branchCount. If current branchCount > child's, updates
//     child's branchCount and pushes child to stack.
//
// - Play (0x02):
//     Gets branchCount from nodeStates_. If zero, sets to 1 (new discovery).
//     Propagates to next (+0xB8) node's nodeStates_.
//
// - Table (0x200):
//     Same structure as Play — gets branchCount, if zero sets to 1,
//     propagates to next.
//
// - Default (all other types):
//     Gets branchCount from nodeStates_.
//     Propagates to next (+0xB8) if present.
//     If node has loop (+0xE0), propagates branchCount to loop child.
//
// PrefetcherNodeState field at +0x34 (relative to value start in hash node)
// is the branchCount.
//
// DISCOVERY: PrefetcherNodeState.branchCount at offset +0x14 within the
// struct (the emplace returns hash_node*, value is at +0x20 from node start,
// the shared_ptr<Node> key occupies +0x20..+0x2F, so PNS starts at +0x30,
// and +0x34 from the returned pointer = PNS+0x04). Wait — the emplace call
// returns a pointer where +0x34 is accessed. Given the hash_node layout:
//   +0x00: __next_ pointer
//   +0x08: __hash_ 
//   +0x10: __value_ (pair<shared_ptr<Node>, PrefetcherNodeState>)
//   +0x10: key (shared_ptr<Node>) = 16 bytes
//   +0x20: value (PrefetcherNodeState) starts here
//   +0x34 from node = PNS + 0x14
// So branchCount is at PrefetcherNodeState offset +0x14.
//
// Actually the return from __emplace_unique_key_args returns a
// __hash_iterator which points to the hash_node. The +0x34 offset
// from the returned pointer targets: node+0x34. The hash_node has:
//   +0x00: __next_
//   +0x08: __hash_cached  
//   +0x10: pair.first (shared_ptr key) = 16 bytes  
//   +0x20: pair.second (PrefetcherNodeState) starts
// So +0x34 = PNS + 0x14. branchCount = PNS[0x14].
//
// But also +0x3c is accessed in definePlaySize (for playSize).
// And +0x40 was accessed in constructor (cacheSize).
// So PrefetcherNodeState layout includes at least:
//   +0x14: int branchCount
//   +0x1c: int playSize  (from definePlaySize +0x3c offset)
//   +0x20: AsmRegister lengthReg (from definePlaySize AsmRegister write)
// ============================================================================
void Prefetch::countBranches(std::shared_ptr<Node> node) // 0x1c9b30
{
    std::deque<std::shared_ptr<Node>> stack;      // -0x70(%rbp)
    stack.push_back(node);                        // 0x1c9b5b

    while (!stack.empty()) {                      // 0x1c9bd0
        auto current = stack.back();              // -0x40(%rbp)
        stack.pop_back();                         // pop + refcount decrement

        if (!current)
            continue;

        NodeType type = current->type;            // 0x1c9cbe: mov 0x44(%rax),%ecx

        switch (type) {
        // ---- Branch (0x04) ---- // 0x1c9d5f
        case NodeType::Branch: {
            // Emplace current into nodeStates_, get branchCount
            auto [it1, _1] = nodeStates_.emplace(
                std::piecewise_construct,
                std::forward_as_tuple(current),
                std::forward_as_tuple());         // 0x1c9d77
            int branchCount = it1->second.branchCount; // +0x34 → PNS.branchCount

            // Count branches: branches.size()
            auto& branches = current->branches;   // +0xC8..+0xD0
            int numBranches = (int)(branches.size());
            int newCount = branchCount + numBranches - 1; // 0x1c9d9b

            // Update max in Prefetch  // 0x1c9da6..0x1c9db3
            // Note: this->maxBranches_ at +0xB8 (formerly labeled pageSize_)
            if (newCount > this->maxBranches_)
                this->maxBranches_ = newCount;

            // Push next if present
            if (current->next)                    // 0x1c9db9
                stack.push_back(current->next);

            // For each branch child, propagate branchCount and push
            auto branchIt = branches.begin();
            auto branchEnd = branches.end();      // +0xD0

            // First: look up current's branchCount (re-emplace to read)
            auto [it2, _2] = nodeStates_.emplace(
                std::piecewise_construct,
                std::forward_as_tuple(current),
                std::forward_as_tuple());         // 0x1c9ddf
            int parentBranch = it2->second.branchCount;

            // Propagate to next node's state
            auto [it3, _3] = nodeStates_.emplace(
                std::piecewise_construct,
                std::forward_as_tuple(current->next),
                std::forward_as_tuple());         // 0x1c9d55
            it3->second.branchCount = parentBranch;

            // For each branch child:
            for (auto bit = branches.begin(); bit != branches.end(); ++bit) {
                // Emplace branch child
                auto [bitIt, _b] = nodeStates_.emplace(
                    std::piecewise_construct,
                    std::forward_as_tuple(*bit),
                    std::forward_as_tuple());     // 0x1c9ef3

                // If parent's branchCount > child's, update and push
                if (newCount > bitIt->second.branchCount) { // 0x1c9ef8
                    auto [bitIt2, _b2] = nodeStates_.emplace(
                        std::piecewise_construct,
                        std::forward_as_tuple(*bit),
                        std::forward_as_tuple()); // 0x1c9f15
                    bitIt2->second.branchCount = newCount;
                    stack.push_back(*bit);        // 0x1c9f67..0x1c9f80
                }
            }
            break;
        }

        // ---- Play (0x02) ---- // 0x1c9cdf (cmp $0x2)
        case NodeType::Play: {
            if (!current->next)                   // 0x1c9cdf: cmpq $0, 0xb8(%rax)
                break;

            // Emplace, check branchCount
            auto [it, _] = nodeStates_.emplace(
                std::piecewise_construct,
                std::forward_as_tuple(current),
                std::forward_as_tuple());         // 0x1c9cff
            int bc = it->second.branchCount;

            if (bc == 0) {
                // Never branched: set to 1        // 0x1c9d0a → 0x1ca247
                auto [it2, _2] = nodeStates_.emplace(
                    std::piecewise_construct,
                    std::forward_as_tuple(current->next),
                    std::forward_as_tuple());
                it2->second.branchCount = 1;      // 0x1ca272: mov %r15d, 0x34(%rax) with r15=1
            } else {
                // Read branchCount, propagate to next
                auto [it2a, _2a] = nodeStates_.emplace(
                    std::piecewise_construct,
                    std::forward_as_tuple(current),
                    std::forward_as_tuple());
                int parentBC = it2a->second.branchCount;

                auto [it3, _3] = nodeStates_.emplace(
                    std::piecewise_construct,
                    std::forward_as_tuple(current->next),
                    std::forward_as_tuple());
                it3->second.branchCount = parentBC;
            }

            // Push next to stack
            stack.push_back(current->next);       // 0x1ca2c3..0x1ca2e3
            break;
        }

        // ---- Table (0x200) ---- // 0x1c9f9b
        case NodeType::Table: {
            if (!current->next)                   // 0x1c9f9b
                break;

            auto [it, _] = nodeStates_.emplace(
                std::piecewise_construct,
                std::forward_as_tuple(current),
                std::forward_as_tuple());
            int bc = it->second.branchCount;

            if (bc == 0) {
                // Set next's branchCount to 1
                auto [it2, _2] = nodeStates_.emplace(
                    std::piecewise_construct,
                    std::forward_as_tuple(current->next),
                    std::forward_as_tuple());
                it2->second.branchCount = 1;
            } else {
                auto [it2a, _2a] = nodeStates_.emplace(
                    std::piecewise_construct,
                    std::forward_as_tuple(current),
                    std::forward_as_tuple());
                int parentBC = it2a->second.branchCount;

                auto [it3, _3] = nodeStates_.emplace(
                    std::piecewise_construct,
                    std::forward_as_tuple(current->next),
                    std::forward_as_tuple());
                it3->second.branchCount = parentBC;
            }

            stack.push_back(current->next);
            break;
        }

        // ---- Default (all other types) ---- // 0x1ca01b
        default: {
            // If no next, done
            if (!current->next)                   // 0x1ca01b
                goto check_loop;

            // Get branchCount from nodeStates_
            auto [it, _] = nodeStates_.emplace(
                std::piecewise_construct,
                std::forward_as_tuple(current),
                std::forward_as_tuple());
            int bc = it->second.branchCount;

            // Propagate to next
            auto [it2, _2] = nodeStates_.emplace(
                std::piecewise_construct,
                std::forward_as_tuple(current->next),
                std::forward_as_tuple());
            it2->second.branchCount = bc;         // 0x1ca06f

            // Push next to stack
            stack.push_back(current->next);       // 0x1ca0c0

check_loop:
            // If loop child exists, propagate branchCount to it too
            if (current->loop) {                  // 0x1ca0fd: cmpq $0, 0xe0(%rax)
                auto [itL, _L] = nodeStates_.emplace(
                    std::piecewise_construct,
                    std::forward_as_tuple(current),
                    std::forward_as_tuple());
                int bcL = itL->second.branchCount;

                auto [itL2, _L2] = nodeStates_.emplace(
                    std::piecewise_construct,
                    std::forward_as_tuple(current->loop),
                    std::forward_as_tuple());
                itL2->second.branchCount = bcL;   // 0x1ca151

                stack.push_back(current->loop);   // 0x1ca1a2
            }
            break;
        }
        } // end switch
    } // end while
}

// ============================================================================
// definePlaySize(node) — 0x1ca370
//
// BFS traversal that determines the play size (number of waveform table
// pages) for each Play node, and stores results in nodeStates_.
//
// For each node:
//   1. Push children (next, branches, loop) onto the stack for traversal
//   2. If type == Play (0x02) and config.field_0x1E == false (+0x66 in node,
//      which is PlayConfig +0x1E, likely a "skip" or "dynamic" flag):
//
//      a. Look up WaveformIR via wavetableIR_->getWaveformByName(waveName)
//         Get signal length: waveform->signal.length_ (at waveform+0xD0)
//         This is the "waveLength" in samples.
//
//      b. Look up again to get the granularity from Signal (+0x70..+0x78)
//         waveform->signal.sampleRate? Actually from signal's linked data.
//
//      c. Compute aligned play size:
//         playSize = ceil(waveLength / granularity) * granularity
//         playSize = min(playSize, maxPlayLength)
//         where maxPlayLength comes from the Signal's linked structure
//
//      d. If playSize != waveLength, resize the waveform:
//         waveform->signal.resizeSamples(playSize) // 0x1caa40
//
//      e. Look up waveform again, compute memory footprint:
//         channelCount = waveform->channelCount (+0xC8 as uint16)
//         playCount = waveform->playCount (+0xD0 as int)
//         sampleBits = signal->sampleBits (+0x50 as int, via signal at +0x78)
//         
//         If playCount != 0:
//           playLength = ceil(playCount / sampleRate) * sampleRate
//           playLength = min(playLength, maxPlay)
//         else:
//           playLength = 0
//
//         memoryBytes = channelCount * playLength * sampleBits
//         memoryBits = memoryBytes (approx)
//         memoryWords = ceil(memoryBits / 8)  // bits to bytes
//
//         memPerPage = devConst_->waveformMemSize / pageSize_
//         // 0x1cab6c: mov 0x8(%r12),%rax; mov 0xc(%rax),%eax; div 0xb8(%r12)
//
//      f. If memoryWords >= memPerPage:
//         Node's length (+0x90) needs splitting
//         pagesNeeded = 1 (initially)
//         ... additional page computation ...
//
//      g. Final: if pagesNeeded >= 2:
//         Store pagesNeeded in nodeStates_[node].playSize (+0x3c)
//         Allocate a register: Resources::getRegisterNumber()
//         Store register in nodeStates_[node].lengthReg (+0x20)
//         
//         Also propagate to parent node if parent exists:
//           nodeStates_[parent].lengthReg = nodeStates_[node].lengthReg
//           nodeStates_[parent].playSize = pagesNeeded
// ============================================================================
void Prefetch::definePlaySize(std::shared_ptr<Node> node) // 0x1ca370
{
    std::deque<std::shared_ptr<Node>> stack;       // -0xc0(%rbp)
    stack.push_back(node);                         // 0x1ca3ad

    while (!stack.empty()) {                       // 0x1ca440
        auto current = stack.back();               // -0x80(%rbp)
        stack.pop_back();

        if (!current)                              // 0x1ca54e: test %r14,%r14
            continue;

        // Push next to stack if present
        if (current->next)                         // 0x1ca557: mov 0xb8(%r14)
            stack.push_back(current->next);

        // Push all branch children
        for (auto& branch : current->branches)     // 0x1ca605..0x1ca6e0
            stack.push_back(branch);

        // Push loop child if present
        if (current->loop)                         // 0x1ca6e0: mov 0xe0(%r14)
            stack.push_back(current->loop);

        // Only process Play nodes (type == 0x02)
        if (current->type != NodeType::Play)       // 0x1ca795: cmpl $0x2, 0x44(%r14)
            continue;

        // Check if config.field_0x1E is false (skip dynamic plays)
        if (current->config./*field_0x1E*/unknown_1e) // 0x1ca7a0: cmpb $0, 0x66(%r14)
            continue;                              // 0x66 = 0x48 (config offset) + 0x1E

        // ---- Compute play size ----

        // Get wave name at deviceIndex
        int devIdx = current->deviceIndex;         // 0x1ca7b9: movslq 0x40(%r14)
        std::optional<std::string> waveName;
        if (devIdx >= 0) {
            waveName = current->wavesPerDev[devIdx];
        }

        // First lookup: get waveLength
        auto waveform1 = wavetableIR_->getWaveformByName(waveName); // 0x1ca82d
        int waveLength = waveform1->playCount;     // 0x1ca83a: mov 0xd0(%rax) — actually signal length
        // (waveform1 released)

        // Second lookup: compute aligned size
        // Re-get wave name
        auto waveform2 = wavetableIR_->getWaveformByName(waveName); // 0x1ca90e

        int playSize;
        if (waveLength != 0) {
            // Compute aligned play size
            // 0x1ca918..0x1ca945
            // sampleRate = devConst_->field at some offset
            int granularity = waveform2->signal.granularity; // from signal chain
            int maxLength = waveform2->signal.maxLength;
            playSize = ((waveLength + granularity - 1) / granularity) * granularity;
            if ((unsigned)maxLength > (unsigned)playSize)
                playSize = maxLength;
            // Actually: playSize = min(ceil_aligned, maxLength) where max is "cmova"
        } else {
            playSize = 0;
        }

        // If playSize changed from waveLength, resize
        if (playSize != waveLength) {              // 0x1ca9aa: cmp %r14d,%r15d
            // Re-lookup waveform and resize its signal
            auto waveform3 = wavetableIR_->getWaveformByName(waveName); // 0x1caa30
            waveform3->signal.resizeSamples(playSize); // 0x1caa40
        }

        // ---- Compute memory footprint ----
        // Look up waveform again
        auto waveform4 = wavetableIR_->getWaveformByName(waveName); // 0x1cab13
        uint16_t channelCount = waveform4->channelCount; // +0xC8 as uint16
        int playCount = waveform4->playCount;      // +0xD0
        auto* sig = waveform4->signal.data;        // +0x78 pointer
        
        int memSize;
        if (playCount != 0) {
            int maxPlay = sig->maxLength;          // +0x40
            int sampleRate = sig->sampleRate;      // +0x44
            int aligned = ((playCount + sampleRate - 1) / sampleRate) * sampleRate;
            aligned = std::min((unsigned)aligned, (unsigned)maxPlay);
            memSize = aligned;
        } else {
            memSize = 0;
        }

        // memoryBits = channelCount * memSize * sampleBits
        int sampleBits = sig->sampleBits;          // +0x50
        int64_t memBits = (int64_t)channelCount * memSize * sampleBits;
        // Convert bits to bytes, rounding up
        int memBytes = (int)memBits;
        int remainder = memBytes & 0x7;
        int memWords = (memBytes >> 3) + (remainder >= 1 ? 1 : 0);

        // memPerPage = devConst_->waveformMemSize / this->maxBranches_
        int wfMemSize = devConst_->/*waveformMemSize at +0xC*/field_0c;
        int memPerPage = (unsigned)wfMemSize / (unsigned)this->maxBranches_;

        if ((unsigned)memPerPage < (unsigned)memWords) {
            // Waveform doesn't fit in one page — need splitting
            int nodeLength = current->length;      // +0x90

            int pagesNeeded = 1;  // initial

            if (nodeLength == 0) {
                // Compute from waveform: pages = ceil(memTotal * maxBranches_ / (memPerHalf))
                auto waveform5 = wavetableIR_->getWaveformByName(waveName);
                uint16_t cc5 = waveform5->channelCount;
                int memTotal = nodeLength * cc5 * (int)this->maxBranches_;
                int halfMem = wfMemSize >> 1;
                pagesNeeded = (unsigned)memTotal / (unsigned)halfMem;
                pagesNeeded++;
            }

            if (pagesNeeded >= 2) {
                // Store playSize in nodeStates_
                auto [nsIt, _ns] = nodeStates_.emplace(
                    std::piecewise_construct,
                    std::forward_as_tuple(current),
                    std::forward_as_tuple());      // 0x1cb006
                nsIt->second.playSize = pagesNeeded; // +0x3c: 0x1cb00b

                // Allocate a register for length tracking
                int regNum = Resources::getRegisterNumber(); // 0x1cb00f (static call)
                AsmRegister lengthReg(regNum);     // 0x1cb01a

                // Store register in nodeStates_
                auto [nsIt2, _ns2] = nodeStates_.emplace(
                    std::piecewise_construct,
                    std::forward_as_tuple(current),
                    std::forward_as_tuple());      // 0x1cb034
                nsIt2->second.lengthReg = lengthReg; // +0x20: 0x1cb039..0x1cb041

                // Propagate to parent if exists
                auto parentPtr = current->parent.lock(); // 0x1cb05a
                if (parentPtr) {
                    // Copy lengthReg to parent's nodeStates_ entry
                    auto [pIt, _p] = nodeStates_.emplace(
                        std::piecewise_construct,
                        std::forward_as_tuple(current),
                        std::forward_as_tuple());
                    auto [pIt2, _p2] = nodeStates_.emplace(
                        std::piecewise_construct,
                        std::forward_as_tuple(current),
                        std::forward_as_tuple());
                    pIt2->second.lengthReg = pIt->second.lengthReg; // 0x1cb0c8..0x1cb0cc

                    // Also copy to parent's entry using parent shared_ptr
                    auto [ppIt, _pp] = nodeStates_.emplace(
                        std::piecewise_construct,
                        std::forward_as_tuple(parentPtr),
                        std::forward_as_tuple());
                    ppIt->second.playSize = pagesNeeded; // 0x1cb0ea
                }
            }
        }
        // else: fits in one page, no special handling needed
    } // end while
}

} // namespace zhinst
