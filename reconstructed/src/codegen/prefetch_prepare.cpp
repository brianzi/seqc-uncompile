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

#include "zhinst/codegen/prefetch.hpp"
#include "zhinst/runtime/resources.hpp"
#include "zhinst/ast/node.hpp"
#include "zhinst/waveform/wavetable_ir.hpp"
#include "zhinst/waveform/waveform_ir.hpp"
#include "zhinst/waveform/signal.hpp"
#include "zhinst/asm/asm_register.hpp"
#include "zhinst/codegen/awg_compiler_config.hpp"
#include "zhinst/device/device_constants.hpp"

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
// LIFO traversal over the node tree using std::stack<shared_ptr<Node>,
// std::deque<shared_ptr<Node>>>.  For each node, dispatches on
// node->type (+0x44, NodeType enum from ast/node.hpp) to perform tree
// transformations:
//
// - Load (NodeType::Load == 0x0001):
//                Resolves the wave name at node->deviceIndex from
//                wavesPerDev (when deviceIndex >= 0), iterates the
//                node->play range (+0xA0..+0xA8) locking each
//                weak_ptr<Node> and marks each resolved waveform's
//                markedForLoad flag (+0xD8).  Then iterates the local
//                validWaves vector and marks those waveforms too.
//                Finally calls collectUsedWaves(node) and pushes
//                node->next.
//
// - Play (NodeType::Play == 0x0002):
//                Calls linkLoad(node), then pushes node->next.
//
// - Branch (NodeType::Branch == 0x0004):
//                Calls removeBranches(node, stack).  This flattens
//                branches into the traversal stack itself.
//
// - Loop (NodeType::Loop == 0x0008):
//                Pushes both node->next (+0xB8) and node->loop (+0xE0)
//                to the stack.
//
// - SetVar (NodeType::SetVar == 0x0010):
//                Calls expandSetVar(node), then pushes node->next.
//
// - Lock (NodeType::Lock == 0x0040):
//                If config_->isHirzel (+0x18) is true, just pushes
//                node->next (Hirzel devices skip the merge logic).
//                Otherwise iterates the next chain: for each step,
//                resolves the wave name, looks up its WaveformIR,
//                calls findLockedPlay to look for an already-locked
//                play of the same wave.  On a hit, removes the
//                current node and merges with any pending lastLoad
//                via mergeLoads.  On a miss, synthesises a fresh
//                load via createLoad and stores it as lastLoad.
//                When lastLoad is non-null, copies node->asmId onto
//                it and splices it in via insertBefore.  Loop ends
//                when lastLoad is null after a step.
//
// - Table (NodeType::Table == 0x0200):
//                Calls linkLoad(node), then pushes node->next.
//
// - PlainLoad (NodeType::PlainLoad == 0x4000):
//                Calls collectUsedWaves(node), then pushes node->next.
//
// - All other types: pushes node->next only.
//
// After processing, the local validWaves vector is destroyed (frees
// any heap-allocated optional<string> contents) and the loop pops
// the next node from the stack until empty.
// ============================================================================
void Prefetch::prepareTree(std::shared_ptr<Node> node) // 0x1c8870
{
    // Local std::stack used as traversal stack — must match removeBranches'
    // parameter type (verified at 0x1c8cd7: callee symbol is
    // removeBranches(shared_ptr<Node>, std::stack<shared_ptr<Node>,
    // std::deque<shared_ptr<Node>>>&) const).
    std::stack<std::shared_ptr<Node>, std::deque<std::shared_ptr<Node>>> stack;  // -0x60(%rbp)
    stack.push(node);                         // 0x1c889e

    // Cache wavetableIR_ shared_ptr into a local  // 0x1c88e4
    auto wavetableIR = wavetableIR_;              // -0x108(%rbp) / -0x68(%rbp)

    while (!stack.empty()) {                      // 0x1c890b / 0x1c8920
        // Pop from top (LIFO traversal)
        auto current = stack.top();               // -0xa0(%rbp)
        stack.pop();                              // 0x1c89b7..0x1c8a05

        if (!current)                             // 0x1c8a0c
            continue;  // skip to next iteration

        // Collect valid waves for this node (copy_if on wavesPerDev)
        // into a local vector<optional<string>>
        std::vector<std::optional<std::string>> validWaves; // -0xe0(%rbp)
        // calls Node::allValidWaves() pattern: copy_if on wavesPerDev  // 0x1c8a2a
        for (auto& w : current->wavesPerDev) {
            if (w.has_value())
                validWaves.push_back(w);
        }

        NodeType type = current->type;            // 0x1c8a4c: mov 0x44(%rbx),%eax

        switch (type) {
        // ---- Load (NodeType=0x01) ---- // jump table[0] → 0x1c8a6f (verified at 0x95ae98)
        case NodeType::Load: {
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
            // Plays exist: for each valid wave, mark waveform as used (+0xD8)
            // 0x1c97b9..0x1c98c5 loop
            for (auto it = playBegin; it != playEnd; ++it) {
                // *it is weak_ptr<Node>; lock to access the node
                // (verified at 0x1c97b9..0x1c98c5)
                auto lockedNode = it->lock();
                if (!lockedNode) continue;
                auto waveName = lockedNode->waveAtCurrentDeviceIndex();
                auto waveform = wavetableIR->getWaveformByName(waveName); // 0x1c97fe
                if (waveform) {
                    waveform->markedForLoad = true;  // 0x1c9850: movb $0x1, 0xd8(%rax)
                }
            }
            // 0x1c97b9-0x1c98c5: Iterate validWaves, mark each waveform's markedForLoad
            for (auto& vw : validWaves) {
                auto waveform = wavetableIR->getWaveformByName(vw); // 0x1c97fe
                if (waveform) {
                    waveform->markedForLoad = true;  // 0x1c9850: movb $0x1, 0xd8(%rax)
                }
            }
            // Then call collectUsedWaves on this node
            collectUsedWaves(current);            // 0x1c991c
            goto push_next_and_loop;
        }

        // ---- Play (NodeType=0x02) ---- // jump table[1] → 0x1c8d13 (verified at 0x95ae98)
        case NodeType::Play: {
            linkLoad(current);                    // 0x1c8d3e
            // Push next (+0xB8) to stack
            if (current->next)                    // 0x1c8d66: mov 0xb8(%rbx)
                stack.push(current->next);
            goto cleanup_validwaves;
        }

        // ---- Branch (0x04) ---- // jump table case 3 → 0x1c8cac
        case NodeType::Branch: {
            // 0x1c8cd7: removeBranches(current, stack) — passes the std::stack
            // by reference; callee inspects branches and pushes any orphaned
            // children back onto the traversal stack itself.
            removeBranches(current, stack);
            goto cleanup_validwaves;
        }

        // ---- Loop (0x08) ---- // jump table case 7 → 0x1c8b85
        case NodeType::Loop: {
            // Push next to stack if present
            if (current->next)                    // 0x1c8b85: mov 0xb8(%rbx)
                stack.push(current->next);
            // Push loop child to stack if present
            if (current->loop)                    // 0x1c8c1c: mov 0xe0(%rbx)
                stack.push(current->loop);
            goto cleanup_validwaves;
        }

        // ---- SetVar (0x10) ---- // jump table case 15 → 0x1c8db9
        case NodeType::SetVar: {
            expandSetVar(current);                // 0x1c8de3
            // Push next to stack
            if (current->next)
                stack.push(current->next);
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
                stack.push(current->next);
            goto cleanup_validwaves;
        }

        // ---- PlainLoad (0x4000) ---- // explicit cmp 0x4000 → 0x1c8adf
        case NodeType::PlainLoad: {
            collectUsedWaves(current);            // 0x1c8b0a
            // Push next to stack
            if (current->next)
                stack.push(current->next);
            goto cleanup_validwaves;
        }

        // ---- Default ---- // 0x1c945c
        default: {
push_next_and_loop:
            // Push next to stack if present
            if (current->next)
                stack.push(current->next);
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
// Stack traversal (std::deque used LIFO via back()/pop_back()) that
// propagates per-node branchCount values through nodeStates_ and
// records the program-wide maximum in this->maxBranches_ (+0xB8).
// For each visited node, dispatches on node->type:
//
// - Branch (NodeType::Branch == 0x0004):
//     Reads parent's branchCount from nodeStates_[current],
//     computes newCount = branchCount + (branches.size() - 1).
//     If newCount > maxBranches_, updates maxBranches_.
//     Pushes node->next, copies parent's branchCount onto next's
//     entry, then for each branch child whose stored branchCount is
//     smaller than newCount, overwrites it with newCount and pushes
//     the child to the stack.
//
// - Play (NodeType::Play == 0x0002):
//     If !next, breaks.  Otherwise reads node's branchCount.
//     If zero (never visited from a Branch), writes 1 onto next's
//     entry; otherwise propagates the parent's branchCount onto
//     next's entry.  Then pushes next.
//
// - Table (NodeType::Table == 0x0200):
//     Same shape as Play.
//
// - All other types:
//     If node->next exists, copies branchCount onto its entry and
//     pushes it.  If node->loop exists, also copies branchCount onto
//     the loop child's entry and pushes the loop child.
//
// On entry every visited node receives a default-constructed
// PrefetcherNodeState entry in nodeStates_ (branchCount initialised
// to 1 by the struct default), so reads after emplace are safe even
// for never-before-seen nodes.
// ============================================================================
void Prefetch::countBranches(std::shared_ptr<Node> node) // 0x1c9b30
{
    std::deque<std::shared_ptr<Node>> stack;      // -0x70(%rbp)
    stack.push_back(node);                        // 0x1c9b5b

    while (!stack.empty()) {                      // 0x1c9bd0
        auto current = stack.back();              // -0x40(%rbp)
        stack.pop_back();

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
            int numBranches = static_cast<int>(branches.size());
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

            {  // braces to avoid goto-crosses-init
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
            }  // end braces

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
// Stack traversal that determines the per-page footprint of each Play
// node and reserves a length register when the wave memory required
// for the play exceeds a single page.
//
// For every visited node:
//   1. Push node->next, every entry in node->branches, and node->loop
//      (when present) onto the traversal stack.
//   2. Continue if node->type != NodeType::Play (== 0x0002), or if
//      node->config.dummy is set (skip dummy plays).
//
// For each non-dummy Play node:
//
//   a. Resolve the wave name at node->deviceIndex from wavesPerDev
//      (when deviceIndex >= 0).
//   b. Look up the WaveformIR via wavetableIR_->getWaveformByName.
//      Read waveLength = waveform->signal.length_ (Signal +0x68).
//   c. Compute aligned playSize:
//        grainSize = devConst_->grainSize (+0x44)
//        minLenSamples = waveform->minLengthSamples (Waveform +0x70)
//        playSize = ceil(waveLength / grainSize) * grainSize
//        if (minLenSamples > playSize) playSize = minLenSamples
//        (verified disasm 0x1ca918..0x1ca945)
//   d. If playSize != waveLength, look up the waveform again and call
//      waveform->signal.resizeSamples(playSize).
//
//   e. Compute memory footprint:
//        channelCount = waveform->signal.channels()  (+0xC8 uint16)
//        playCount    = waveform->signal.length_     (+0xD0)
//        wfDc         = waveform->deviceConstants    (+0x78)
//        Aligned cap (verified 0x1cab18..0x1cab4c):
//          aligned = ceil(playCount / wfDc->grainSize) * wfDc->grainSize
//          if (wfDc->maxWaveformLength > aligned)
//              aligned = wfDc->maxWaveformLength
//        sampleBits = wfDc->bitsPerSample (+0x50, always 16)
//        memBits  = channelCount * aligned * sampleBits
//        memWords = ceil(memBits / 8)
//
//   f. Compute per-page budget:
//        memPerPage = devConst_->waveformMemorySize / this->maxBranches_
//        (verified 0x1cab6c: mov 0x8(%r12),%rax; mov 0xc(%rax),%eax;
//                            div 0xb8(%r12))
//
//   g. If memPerPage < memWords, the wave does not fit in one page:
//        nodeLength = current->length (+0x90)
//        pagesNeeded = 1
//        If nodeLength == 0, recompute from waveform with
//          pagesNeeded = (nodeLength * channelCount * maxBranches_)
//                        / (waveformMemorySize / 2) + 1
//
//      If pagesNeeded >= 2:
//        nodeStates_[current].pagesNeeded = pagesNeeded   (+0x3C)
//        regNum     = Resources::getRegisterNumber()
//        lengthReg  = AsmRegister(regNum)
//        nodeStates_[current].registerHirzel = lengthReg  (+0x20)
//        If current->parent.lock() yields a parent ptr, mirror both
//        registerHirzel and pagesNeeded onto the parent's entry so
//        the surrounding control flow can reference them.
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

        // Check if config.dummy is set (skip dummy plays).
        // 0x1ca7a0: cmpb $0, 0x66(%r14)
        //   0x66 = 0x48 (Node::config offset) + 0x1E (PlayConfig::dummy)
        if (current->config.dummy)
            continue;

        // ---- Compute play size ----

        // Get wave name at deviceIndex
        int devIdx = current->deviceIndex;         // 0x1ca7b9: movslq 0x40(%r14)
        std::optional<std::string> waveName;
        if (devIdx >= 0) {
            waveName = current->wavesPerDev[devIdx];
        }

        // First lookup: get waveLength
        auto waveform1 = wavetableIR_->getWaveformByName(waveName); // 0x1ca82d
        int waveLength = static_cast<int>(waveform1->signal.length_);     // 0x1ca83a: mov 0xd0(%rax) = Signal::length_
        // (waveform1 released)

        // Second lookup: compute aligned size
        // Re-get wave name
        auto waveform2 = wavetableIR_->getWaveformByName(waveName); // 0x1ca90e

        int playSize;
        if (waveLength != 0) {
            // Compute aligned play size (verified disasm 0x1ca918..0x1ca945)
            //   mov eax,[devConst_+0x44]   ; grainSize (= "grainSize")
            //   mov ecx,[waveform2+0x70]   ; Waveform::minLengthSamples ("minLengthSamples")
            //   r15 = ceil_div(waveLength, grainSize) * grainSize
            //   if (minLengthSamples > r15) r15 = minLengthSamples   ; cmova → max
            int grainSize = static_cast<int>(devConst_->grainSize);
            int minLenSamples = waveform2->minLengthSamples;
            playSize = ((waveLength + grainSize - 1) / grainSize) * grainSize;
            if (static_cast<unsigned>(minLenSamples) > static_cast<unsigned>(playSize))
                playSize = minLenSamples;
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
        uint16_t channelCount = waveform4->signal.channels(); // +0xC8 as uint16
        int playCount = static_cast<int>(waveform4->signal.length_);      // +0xD0 = Signal::length_
        // Verified disasm 0x1cab18..0x1cab4c:
        //   rsi = [waveform4+0x78]              ; waveform's DeviceConstants*
        //   edi = [rsi+0x40] = maxWaveformLength  ; "max" cap
        //   r8d = [rsi+0x44] = grainSize     ; alignment grain
        //   eax = ceil_div(playCount, r8d) * r8d
        //   if (edi > eax) eax = edi             ; cmova → max(aligned, maxWaveformLength)
        const DeviceConstants* wfDc = waveform4->deviceConstants;

        int memSize;
        if (playCount != 0) {
            int wfMaxCap   = static_cast<int>(wfDc->maxWaveformLength); // +0x40
            int wfGrain    = static_cast<int>(wfDc->grainSize);    // +0x44
            int aligned = ((playCount + wfGrain - 1) / wfGrain) * wfGrain;
            if (static_cast<unsigned>(wfMaxCap) > static_cast<unsigned>(aligned))
                aligned = wfMaxCap;
            memSize = aligned;
        } else {
            memSize = 0;
        }

        // memoryBits = channelCount * memSize * sampleBits
        // Verified: [rsi+0x50] = bitsPerSample (always 16)
        int sampleBits = static_cast<int>(wfDc->bitsPerSample);  // +0x50
        int64_t memBits = static_cast<int64_t>(channelCount) * memSize * sampleBits;
        // Convert bits to bytes, rounding up
        int memBytes = static_cast<int>(memBits);
        int remainder = memBytes & 0x7;
        int memWords = (memBytes >> 3) + (remainder >= 1 ? 1 : 0);

        // memPerPage = devConst_->waveformMemorySize / this->maxBranches_
        int wfMemSize = devConst_->waveformMemorySize;
        int memPerPage = static_cast<unsigned>(wfMemSize) / static_cast<unsigned>(this->maxBranches_);

        if (static_cast<unsigned>(memPerPage) < static_cast<unsigned>(memWords)) {
            // Waveform doesn't fit in one page — need splitting
            int nodeLength = current->length;      // +0x90

            int pagesNeeded = 1;  // initial

            if (nodeLength == 0) {
                // Compute from waveform: pages = ceil(memTotal * maxBranches_ / (memPerHalf))
                auto waveform5 = wavetableIR_->getWaveformByName(waveName);
                uint16_t cc5 = waveform5->signal.channels_;
                int memTotal = nodeLength * cc5 * static_cast<int>(this->maxBranches_);
                int halfMem = wfMemSize >> 1;
                pagesNeeded = static_cast<unsigned>(memTotal) / static_cast<unsigned>(halfMem);
                pagesNeeded++;
            }

            if (pagesNeeded >= 2) {
                // Store playSize in nodeStates_
                auto [nsIt, _ns] = nodeStates_.emplace(
                    std::piecewise_construct,
                    std::forward_as_tuple(current),
                    std::forward_as_tuple());      // 0x1cb006
                nsIt->second.pagesNeeded = pagesNeeded; // +0x3c: 0x1cb00b

                // Allocate a register for length tracking
                int regNum = Resources::getRegisterNumber(); // 0x1cb00f (static call)
                AsmRegister lengthReg(regNum);     // 0x1cb01a

                // Store register in nodeStates_
                auto [nsIt2, _ns2] = nodeStates_.emplace(
                    std::piecewise_construct,
                    std::forward_as_tuple(current),
                    std::forward_as_tuple());      // 0x1cb034
                nsIt2->second.registerHirzel = lengthReg; // +0x20: 0x1cb039..0x1cb041

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
                    pIt2->second.registerHirzel = pIt->second.registerHirzel; // 0x1cb0c8..0x1cb0cc

                    // Also copy to parent's entry using parent shared_ptr
                    auto [ppIt, _pp] = nodeStates_.emplace(
                        std::piecewise_construct,
                        std::forward_as_tuple(parentPtr),
                        std::forward_as_tuple());
                    ppIt->second.pagesNeeded = pagesNeeded; // 0x1cb0ea
                }
            }
        }
        // else: fits in one page, no special handling needed
    } // end while
}

} // namespace zhinst
