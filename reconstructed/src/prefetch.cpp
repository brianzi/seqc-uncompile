// ============================================================================
// Reconstructed from disassembly of _seqc_compiler.so
// Class: zhinst::Prefetch — constructor, destructor, and 11 load/optimize methods
// ============================================================================

#include "zhinst/prefetch.hpp"
#include "zhinst/awg_compiler_config.hpp"
#include "zhinst/device_constants.hpp"
#include "zhinst/cache.hpp"
#include "zhinst/node.hpp"
#include "zhinst/error_messages.hpp"
#include "zhinst/wavetable_ir.hpp"
#include "zhinst/waveform_ir.hpp"
#include "zhinst/play_config.hpp"
// #include "zhinst/resources.hpp"  // for Resources::getRegisterNumber()

#include <deque>
#include <stack>

namespace zhinst {

// 0x1c5850 — Prefetch::Prefetch(...)
// Ends at 0x1c5a53 (ret), exception cleanup through 0x1c5b59.
Prefetch::Prefetch(
    AWGCompilerConfig const& config,
    DeviceConstants const& devConst,
    std::shared_ptr<AsmCommands> asmCommands,
    std::shared_ptr<Node> root,
    std::shared_ptr<WavetableIR> wavetableIR,
    std::function<void(std::string const&)> const& logFunc,
    std::weak_ptr<CancelCallback> cancelCb)
    : config_(&config)
    , devConst_(&devConst)
    , nodeStates_()
    , nameMap_()
    , root_(std::move(root))
    , asmCommands_(std::move(asmCommands))
    , resources_()
    , cache_()
    , waveformMaps_()
    , pageSize_(1)
    , split_(false)
    , cwvfConfig_()
    , usageEntries_()
    , lastCwvfNode_()
    , globalCwvfValid_(false)
    , wavetableIR_(std::move(wavetableIR))
    , logFunc_(logFunc)
    , cancelCb_(std::move(cancelCb))
{
    // cwvfConfig_ init: channelMask = -1 (0xFFFFFFFF), rest zeroed
    cwvfConfig_.channelMask = static_cast<uint32_t>(-1);

    cache_ = std::make_shared<Cache>(
        static_cast<detail::AddressImpl<uint32_t>>(devConst.waveformMemorySize),
        devConst.sampleLength,
        config.isHirzel);

    waveformMaps_.resize(config.numChannelGroups);

    auto cacheSize = cache_->getSize();
    nodeStates_[root_] = PrefetcherNodeState{};
    // nodeStates_[root_].usedCache = cacheSize;  // at +0x40 in hash node value
}

// 0x11eed0 — Prefetch::~Prefetch()
Prefetch::~Prefetch()
{
    // All implicit via member destructors.
}

// ============================================================================
// 0x1d5620 — Prefetch::globalCwvf(shared_ptr<Node>)
// Ends at 0x1d5765.
//
// For SetVar (type==0x02) nodes: checks if the node's PlayConfig (+0x48)
// matches the saved cwvfConfig_ (+0xC0). If cwvfConfig_.channelMask == -1
// (first call), copies the node's config into cwvfConfig_ and saves the
// node as lastCwvfNode_. Otherwise compares all PlayConfig fields;
// if they differ, sets globalCwvfValid_ = false.
//
// Additional checks for nodes with specific cwvf conditions:
//   - if node->dummy (+0x66) is set OR node->hold (+0x65) is set:
//     - if node->rate (+0x4C) == 0 or (rate == -1 && globalRate (+0x100) <= 0):
//   - if devConst_->hasPrecomp (+0x88, bit 0): skip (return)
//     - else: proceed to comparison
// ============================================================================
void Prefetch::globalCwvf(std::shared_ptr<Node> node)  // 0x1d5620
{
    Node* n = node.get();
    if (!n)
        return;

    // Only process Play-type nodes (type == 0x02 i.e. Play)
    if (n->type != NodeType::Play)
        return;

    bool useDA = devConst_->/*+0x88*/ hasPrecomp;

    // Check if node has dummy/hold flags that might skip cwvf tracking
    bool hasDummyOrHold = n->config.dummy || n->config.hold;
    if (hasDummyOrHold) {
        int32_t rate = n->config.rate;
        if (rate == 0 || (rate == -1 && n->globalRate <= 0)) {
            if (useDA & 0x1)
                return;  // skip DA nodes with no rate
        }
    }

    // First node: initialize cwvfConfig_ from node's config
    if (cwvfConfig_.channelMask == static_cast<uint32_t>(-1)) {
        // Copy node->config (+0x48..+0x67) → cwvfConfig_ (+0xC0..+0xDF)
        // 0x1f bytes = PlayConfig size, done via two movups (16 + 15 bytes)
        cwvfConfig_ = n->config;

        // Save node as lastCwvfNode_ (shared_ptr copy)
        lastCwvfNode_ = node;
        globalCwvfValid_ = true;
        return;
    }

    // Compare all PlayConfig fields
    bool match = true;
    if (cwvfConfig_.channelMask != n->config.channelMask) match = false;
    else if (cwvfConfig_.rate != n->config.rate) match = false;
    else if (cwvfConfig_.suppress != n->config.suppress) match = false;
    else if (cwvfConfig_.is4Channel != n->config.is4Channel) match = false;
    else if (cwvfConfig_.markerBits != n->config.markerBits) match = false;
    else if (cwvfConfig_.trigger != n->config.trigger) match = false;
    else if (cwvfConfig_.precompFlags != n->config.precompFlags) match = false;
    else if (cwvfConfig_.dummy != n->config.dummy) match = false;
    else {
        // All basic fields match. Check hold only if rate > 0
        if (cwvfConfig_.rate > 0) {
            if (cwvfConfig_.hold != n->config.hold)
                match = false;
        }
    }

    if (!match)
        globalCwvfValid_ = false;
}

// ============================================================================
// 0x1cf7b0 — Prefetch::optimizeSync(shared_ptr<Node>)
// Ends at 0x1cfc31.
//
// BFS traversal using a deque. For each node (popped from back):
//   - If node has next (+0xB8): enqueue next
//   - For each child in branches (+0xC8): enqueue child
//   - If node has loop/else (+0xE0): enqueue loop
//   - Check node type:
//     - Load (0x01): if lastPlaceholder is set, call Node::swap(lastPlaceholder, currentNode)
//       to move the placeholder before this load
//     - Placeholder (0x100): save as lastPlaceholder
//     - Other types: clear lastPlaceholder
// ============================================================================
void Prefetch::optimizeSync(std::shared_ptr<Node> node)  // 0x1cf7b0
{
    std::deque<std::shared_ptr<Node>> queue;
    queue.push_back(node);

    std::shared_ptr<Node> lastPlaceholder;  // -0x70(%rbp)

    while (!queue.empty()) {
        auto current = queue.back();
        queue.pop_back();

        Node* cur = current.get();

        // Enqueue next sibling
        if (cur->next) {
            queue.push_back(cur->next);
        }

        // Enqueue child branches
        auto& branches = cur->branches;
        for (auto& child : branches) {
            queue.push_back(child);
        }

        // Enqueue loop/else
        if (cur->loop) {
            queue.push_back(cur->loop);
        }

        // Type-specific handling
        NodeType t = cur->type;
        if (t == NodeType::Load) {
            // If we have a pending placeholder, swap it before this load
            if (lastPlaceholder) {
                Node::swap(lastPlaceholder, current);
            }
        } else if (t == NodeType::Placeholder) {
            // Save as last placeholder
            lastPlaceholder = current;
        } else {
            // Reset placeholder tracking
            lastPlaceholder.reset();
        }
    }
}

// ============================================================================
// 0x1cfc70 — Prefetch::optimizeCwvf(shared_ptr<Node>)
// Ends at 0x1d0fb0.
//
// Recursive tree traversal that propagates the current CWVF (Channel
// WaveForm) configuration down through the node chain. The CWVF config
// describes how the AWG output channels are configured (channel mask,
// rate, suppress, markers, trigger, precomp, hold/dummy flags). This
// pass ensures every node in the tree carries the correct "currentCwvf"
// so that later code-generation can detect when a CWVF instruction needs
// to be emitted (i.e., when the config changes between consecutive Play
// nodes).
//
// Algorithm:
//   1. Start with the parent node's currentCwvf as the "running" config.
//   2. Walk the linked list (node→next) iteratively.
//   3. For each node, based on its type:
//      - Branch: set globalRate/defaultPrecompFlags, then for each branch
//        child, propagate config (write to child) and recurse. After all
//        branches, check if they all agree on the CWVF state; if so,
//        adopt the common state going forward. If not, reset to default.
//      - Table (0x200): set config on node, check if node qualifies for
//        CWVF optimization (dummy/hold + rate conditions + useDA), and
//        if so write the running cwvf into node->currentCwvf. Then call
//        globalCwvf(node) to track global consistency.
//      - Loop (0x8): write config+currentCwvf to node, propagate into
//        node->loop child, then walk the next-chain from the loop child
//        to find the tail node and adopt its config.
//      - Rate (0x20): write config, adopt node's defaultPrecompFlags.
//      - SetPrecomp (0x1000): write config, adopt node's globalRate.
//      - Default (all others): just write current config to node.
//   4. After type-specific handling, write the running currentCwvf state
//      to the Prefetch object's tracked node fields, then advance to
//      node->next.
//   5. On reaching the end of the chain (null next), return.
//
// The "CWVF optimization" for Play/Table nodes checks:
//   - node->config.dummy is set OR node->config.hold is set
//   - node->config.rate == 0 OR (rate == -1 AND globalRate <= 0)
//   - devConst_->hasPrecomp bit 0 is set
//   If all conditions are met, the running cwvf state is copied into
//   node->currentCwvf (overriding the default config copy), enabling
//   the node to inherit the parent's CWVF settings and avoid emitting
//   a redundant CWVF instruction.
//
// For branches, a convergence check determines if all branches ended
// with the same cwvf state. The check is done both for PlayConfig
// equality (field-by-field, with special hold/rate logic) and for
// defaultPrecompFlags agreement. If branches diverge, the cwvf state
// is reset to "invalid" (channelMask = -1, all zeros) so that the
// next Play node will force a fresh CWVF instruction.
//
// Two error paths throw ZIAWGCompilerException:
//   - Error 0xC3: if the next-chain scan after a Branch finds no
//     qualifying terminator (Play/Branch/Loop type).
//   - Error 0xC4: if the next-chain scan after a SetPrecomp (0x1000)
//     update similarly fails.
// ============================================================================
void Prefetch::optimizeCwvf(std::shared_ptr<Node> node)  // 0x1cfc70
{
    // 0x1cfc8b: rax = node.get()
    Node* cur = node.get();
    if (!cur)                                              // 0x1cfc91: je → return
        return;

    // Load the initial "running" cwvf state from the input node's currentCwvf (+0x68).
    // These are carried in registers/stack across the iteration.
    PlayConfig cwvf = cur->currentCwvf;                    // 0x1cfc97-0x1cfcd4: load +0x68..+0x86
    int globalRate = cur->globalRate;                       // 0x1cfcdc: +0x100 → stack -0x40
    unsigned int defaultPrecompFlags = cur->defaultPrecompFlags; // 0x1cfce5: +0x104 → stack -0x30

    // curNode tracks the raw Node* we're processing; curCtrl is the shared_ptr ctrl block.
    Node* curNode = cur;                                   // 0x1cfcee: → stack -0xb0
    // Make a local copy of the shared_ptr (increment refcount).       // 0x1cfcf5-0x1cfd05
    std::shared_ptr<Node> curShared = node;

    // ---- Main iteration: walk the next-chain ----
    // Entry at 0x1cfd3e; advance at 0x1d0869 → 0x1d08cf..0x1d0934 → back to 0x1cfd10.
    while (curNode) {                                      // 0x1cfd17/0x1cfd38: test+je
        NodeType type = curNode->type;                     // 0x1cfd3e: mov 0x44(%rax),%ecx

        // Switch on type via jump table for types 2..0x20 (type-2 ≤ 0x1E).
        // Types outside this range fall through to default handler at 0x1d0290.
        switch (type) {

        // ==== Branch (type 0x4) ==== target 0x1cfd74
        case NodeType::Branch: {
            curNode->globalRate = globalRate;               // 0x1cfd7e: store -0x40 → +0x100
            // Save/restore markers for branch iteration
            // 0x1cfd99-0x1cfda7: load branches vector (+0xC8 begin, +0xD0 end)
            auto& branches = curNode->branches;
            bool allSameConfig = true;                     // 0x1cfdba: -0x42(%rbp)
            bool allSamePrecomp = true;                    // 0x1cfdc9: -0x48(%rbp) = 1

            // Track the "expected" cwvf from first branch for convergence checking.
            PlayConfig expectedCwvf = cwvf;                // saved at various stack slots
            unsigned int expectedPrecomp = defaultPrecompFlags;

            // If branches vector is empty, skip to after-branch logic at 0x1d063b.
            for (auto it = branches.begin(); it != branches.end(); ++it) { // 0x1cfe37
                Node* branchNode = it->get();              // 0x1cfe37: mov (%rbx),%rax
                if (!branchNode)
                    continue;                              // 0x1cfe3d: je → next iter

                // For Play (type 2) children in the branch vector:
                if (branchNode->type == NodeType::Play) {  // 0x1cfe3f: cmpl $0x2
                    branchNode->globalRate = globalRate;    // 0x1cfe4c
                    branchNode->defaultPrecompFlags = defaultPrecompFlags; // 0x1cfe55

                    // Check if this Play node qualifies for CWVF optimization.
                    // Condition: (dummy || hold) && (rate==0 || (rate==-1 && globalRate<=0)) && useDA
                    bool useDA = devConst_->hasPrecomp & 0x1;   // 0x1cfe5b-0x1cfe66: this→devConst_→+0x88
                    bool hasDummyOrHold = branchNode->config.dummy || branchNode->config.hold;
                    if (hasDummyOrHold) {                   // 0x1cfe6d-0x1cfe77
                        int rate = branchNode->config.rate; // +0x4C
                        if (rate == 0 || (rate == -1 && globalRate <= 0)) { // 0x1cfe80-0x1cfe91
                            if (useDA) {                   // 0x1cfe97
                                // Write running cwvf → node->currentCwvf (+0x68)
                                branchNode->currentCwvf = cwvf;  // 0x1cfea3-0x1cfee6
                                goto branchChildRecurse;    // skip default copy
                            }
                        }
                    }
                    // Default: copy config → currentCwvf (memcpy +0x48→+0x68)
                    branchNode->currentCwvf = branchNode->config; // 0x1cff70-0x1cff7c
                } else {
                    // Non-Play branch child: write running config into node's config (+0x48)
                    branchNode->config = cwvf;              // 0x1cfef0-0x1cff26

                    // Additional type-specific handling for the branch child:
                    if (branchNode->type == NodeType::Rate) {       // 0x1cff30: cmp $0x20
                        branchNode->defaultPrecompFlags = defaultPrecompFlags; // 0x1cff38
                    } else {
                        branchNode->globalRate = globalRate;  // 0x1cff43
                        if (branchNode->type != NodeType::SetPrecomp) { // 0x1cff49: cmp $0x1000
                            branchNode->defaultPrecompFlags = defaultPrecompFlags; // 0x1cff54
                            if (branchNode->type == NodeType::Play) {   // 0x1cff5a: cmp $0x2
                                // Re-check CWVF optimization (for nested Play)
                                goto recheckCwvf;           // 0x1cff5d → 0x1cfe5b
                            }
                        }
                    }
                    // Copy config → currentCwvf
                    branchNode->currentCwvf = branchNode->config; // 0x1cff70-0x1cff7c
                }

            branchChildRecurse:
                // Recurse into this branch child.
                {
                    std::shared_ptr<Node> branchShared = *it;  // 0x1cff80-0x1cff93: copy shared_ptr
                    optimizeCwvf(branchShared);                 // 0x1cffa6: recursive call
                }

                // After recursion, walk to the tail of the branch child's chain
                // to read its final cwvf state.
                Node* tail = it->get();                    // 0x1cffcd
                // Follow next-chain to find the last node:
                {
                    std::shared_ptr<Node> tailShared = *it;
                    while (tail->next) {                   // 0x1cfff7: test+je
                        tail = tail->next.get();           // 0x1d0006
                        // (shared_ptr bookkeeping for tail traversal)
                    }
                }

                // Check if this branch's final state matches the first branch's.
                // First branch? → use globalCwvfValid_ flag
                if (allSameConfig) {                       // 0x1d005b-0x1d006c
                    if (!this->branchMaySkipAllBodies) {   // confirmed: 0x1d0073: cmpb $0x0,0x109(%rax)
                        // Update expected cwvf from this branch's tail node
                        expectedCwvf = tail->currentCwvf;  // 0x1d0084-0x1d00fe
                        expectedPrecomp = tail->defaultPrecompFlags; // 0x1d00f7
                    }
                }

                // Field-by-field comparison of tail->currentCwvf vs expectedCwvf:
                // (mirrors PlayConfig::operator!=, with special hold/rate logic)
                bool matches = true;
                if (expectedCwvf.channelMask != tail->currentCwvf.channelMask)  // 0x1d010a
                    matches = false;
                else if (expectedCwvf.rate != tail->currentCwvf.rate)           // 0x1d0171
                    matches = false;
                else if (expectedCwvf.suppress != tail->currentCwvf.suppress)   // 0x1d017d
                    matches = false;
                else if (expectedCwvf.is4Channel != tail->currentCwvf.is4Channel) // 0x1d0187
                    matches = false;
                else if (expectedCwvf.markerBits != tail->currentCwvf.markerBits) // 0x1d0190
                    matches = false;
                else if (expectedCwvf.trigger != tail->currentCwvf.trigger)     // 0x1d0199
                    matches = false;
                else if (expectedCwvf.precompFlags != tail->currentCwvf.precompFlags) // 0x1d01a2
                    matches = false;
                else if (expectedCwvf.dummy != tail->currentCwvf.dummy)         // 0x1d01af
                    matches = false;
                else {
                    // Special: hold only compared when rate > 0
                    if (expectedCwvf.rate > 0) {           // 0x1d01b8
                        if (expectedCwvf.hold != tail->currentCwvf.hold) // 0x1d01c2
                            matches = false;
                    }
                }

                if (!matches) {
                    allSameConfig = false;                 // 0x1d01d0
                    // Also check if defaultPrecompFlags matches:
                    if (expectedPrecomp == tail->defaultPrecompFlags) // 0x1d01d7-0x1d01e7
                        ; // keep allSamePrecomp
                    else
                        allSamePrecomp = false;
                } else {
                    // Config matches. Check if cwvf is non-trivial (not all-zero).
                    // 0x1d023c-0x1d0282: OR together ~channelMask|rate|suppress,
                    // is4Channel, markerBits, trigger, precompFlags, dummy —
                    // if all zero (default/invalid), clear allSameConfig.
                    bool isNonTrivial =
                        ((~expectedCwvf.channelMask) | expectedCwvf.rate | expectedCwvf.suppress) != 0
                        || expectedCwvf.is4Channel
                        || expectedCwvf.markerBits != 0
                        || expectedCwvf.trigger != 0
                        || expectedCwvf.precompFlags != 0
                        || expectedCwvf.dummy;
                    allSameConfig = allSameConfig && isNonTrivial; // 0x1d0280-0x1d0282
                    // Check precomp
                    if (expectedPrecomp != tail->defaultPrecompFlags)
                        allSamePrecomp = false;
                }
            } // end branch iteration

            // After all branches: decide whether to adopt the common state.
            // 0x1d05fa: testb $0x1,-0x48(%rbp) — allSameConfig?
            if (allSameConfig) {
                // All branches converged. Adopt the expected cwvf.
                defaultPrecompFlags = expectedPrecomp;     // 0x1d0604-0x1d060a
                cwvf = expectedCwvf;                       // 0x1d060d-0x1d0639
            } else {
                // Branches diverged. Reset to invalid/default cwvf state.
                cwvf.channelMask = static_cast<uint32_t>(-1); // 0x1d06b3: $0xffffffff
                cwvf.rate = 0;                             // 0x1d06d9
                cwvf.suppress = 0;
                cwvf.is4Channel = false;
                cwvf.markerBits = 0;
                cwvf.trigger = 0;
                cwvf.precompFlags = 0;
                cwvf.now = false;
                cwvf.hold = false;
                cwvf.dummy = false;
                defaultPrecompFlags = expectedPrecomp;     // 0x1d06bc
            }

            // Write cwvf to the Prefetch-tracked node.
            // 0x1d06ea-0x1d0722: store cwvf fields to curNode->config (+0x48)
            curNode->config = cwvf;

            // Check allSamePrecomp flag (-0x42(%rbp)):
            if (!allSamePrecomp) {                         // 0x1d0726: testb -0x42
                // Need to scan the next-chain for a terminator to verify consistency.
                // 0x1d074e-0x1d07f8: walk curNode->next chain, looking for
                // Play/Branch/Loop nodes (bitmask 0x114 = bits 2,4,8 = Play|Branch|Loop).
                // Also check for SetPrecomp (0x1000) and mark if found.
                Node* scanNode = curNode->next.get();
                bool foundSetPrecomp = false;
                while (scanNode) {
                    NodeType scanType = scanNode->type;
                    if (scanType == NodeType::SetPrecomp) {
                        foundSetPrecomp = true;            // 0x1d07b0
                    }
                    // Check bitmask: types Play(2), Branch(4), Loop(8) → 0x114
                    if (static_cast<int>(scanType) <= 8) {
                        int mask = 0x114;  // bits for Play(2), Branch(4), Loop(8) -- APPROXIMATE
                        if ((mask >> static_cast<int>(scanType)) & 1) {
                            break;  // found terminator
                        }
                    }
                    scanNode = scanNode->next.get();
                }
                // 0x1d0835: test foundSetPrecomp (bl)
                if (!foundSetPrecomp) {
                    // 0x1d0e38: throw ZIAWGCompilerException(ErrorMessages::format(0xC3))
                    throw ZIAWGCompilerException(
                        ErrorMessages::format(ErrorMessageT(0xC3)));
                }
                // If found, continue with current state.
            }
            // 0x1d0860: store defaultPrecompFlags to curNode
            curNode->defaultPrecompFlags = defaultPrecompFlags;
            break;
        }

        // ==== Table / SetVar (type 0x200) ==== target 0x1d02a8
        // NOTE: Despite the enum name, the jump table maps type 0x2
        // (which is Play in the enum) to this case. The actual NodeType::Play
        // value per the jump table entry at idx 0 (type-2=0, i.e. type=2).
        // APPROXIMATE: The enum names may be swapped vs what the jump table shows.
        case NodeType::Play: {                             // 0x1d02a8 (jump table idx 0, type=2)
            curNode->globalRate = globalRate;               // 0x1d02b1
            int nodeRate = curNode->config.rate;            // 0x1d02b7: +0x4C
            if (nodeRate == -1) {
                curNode->config.rate = globalRate;          // 0x1d02c5
            }
            curNode->defaultPrecompFlags = defaultPrecompFlags; // 0x1d02cc
            curNode->config.precompFlags = defaultPrecompFlags; // 0x1d02d2 — +0x60

            // Check CWVF optimization eligibility (same logic as branch children).
            bool isDummy = curNode->config.dummy;           // 0x1d02d5: +0x66
            if (curNode->type == NodeType::Play) {          // 0x1d02d9: cmp $0x2 — APPROXIMATE
                bool useDA = devConst_->hasPrecomp & 0x1;
                bool hasDummyOrHold = isDummy || curNode->config.hold;
                if (hasDummyOrHold) {
                    if (nodeRate == 0 || (nodeRate == -1 && globalRate <= 0)) {
                        if (useDA) {
                            // Eligible: write running cwvf → currentCwvf, skip globalCwvf
                            curNode->currentCwvf = cwvf;   // 0x1d031d → 0x1d0869
                            goto advanceToNext;
                        }
                    }
                }
            }

            // Not eligible: read the node's own config as the new cwvf.
            cwvf = curNode->config;                         // 0x1d0330-0x1d0368

            // Call globalCwvf(node) to track global CWVF consistency.
            {
                std::shared_ptr<Node> nodeShared(curNode, curShared.get_deleter()); // APPROXIMATE
                globalCwvf(nodeShared);                    // 0x1d0395
            }
            // Falls through to advanceToNext via 0x1d03e4 → 0x1d0869
            break;
        }

        // ==== Loop (type 0x8) ==== target 0x1d046f
        case NodeType::Loop: {
            // Write current config AND currentCwvf to the loop node.
            curNode->config = cwvf;                         // 0x1d046f-0x1d04a0
            curNode->currentCwvf = cwvf;                    // 0x1d04a4-0x1d04de
            curNode->globalRate = globalRate;                // 0x1d04e8
            curNode->defaultPrecompFlags = defaultPrecompFlags; // 0x1d04f1

            // If node has a loop child (+0xE0), propagate into it.
            Node* loopChild = curNode->loop.get();          // 0x1d04f7
            if (loopChild) {
                // Check loop child type: if Play → same CWVF opt check
                if (loopChild->type == NodeType::Play) {    // 0x1d0507: cmpl $0x2
                    loopChild->globalRate = globalRate;      // 0x1d0514
                    loopChild->defaultPrecompFlags = defaultPrecompFlags; // 0x1d051d

                    bool useDA = devConst_->hasPrecomp & 0x1;
                    bool hasDummyOrHold = loopChild->config.dummy || loopChild->config.hold;
                    if (hasDummyOrHold) {
                        int rate = loopChild->config.rate;
                        if (rate == 0 || (rate == -1 && globalRate <= 0)) {
                            if (useDA) {
                                loopChild->currentCwvf = cwvf; // 0x1d0568-0x1d05a2
                                goto loopChildRecurseDone;
                            }
                        }
                    }
                    // Default: copy config → currentCwvf
                    loopChild->currentCwvf = loopChild->config; // 0x1d095c-0x1d0968
                } else {
                    // Non-Play loop child: write config, handle Rate/SetPrecomp
                    loopChild->config = cwvf;               // 0x1d065d-0x1d068e
                    Node* loopNext = curNode->loop.get();   // re-read
                    NodeType loopNextType = loopNext->type;
                    if (loopNextType == NodeType::Rate) {
                        loopChild->defaultPrecompFlags = defaultPrecompFlags;
                    } else {
                        loopChild->globalRate = globalRate;
                        if (loopNextType != NodeType::SetPrecomp) {
                            loopChild->defaultPrecompFlags = defaultPrecompFlags;
                            if (loopNextType == NodeType::Play) {
                                // Re-check CWVF opt (similar to branch)
                                goto recheckLoopCwvf;       // → 0x1d0523
                            }
                        }
                    }
                    loopChild->currentCwvf = loopChild->config;
                }

            loopChildRecurseDone:
                // Recurse into loop child.
                {
                    std::shared_ptr<Node> loopShared = curNode->loop; // 0x1d096c-0x1d0986
                    optimizeCwvf(loopShared);                         // 0x1d0999
                }

                // After recursion, walk from the loop child to the end of its chain
                // to find the tail node and potentially adopt its config/defaultPrecompFlags.
                // 0x1d09c6-0x1d0a81: follow loop->next chain to find tail
                Node* loopTail = curNode->loop.get();
                while (loopTail->next) {
                    loopTail = loopTail->next.get();
                }

                // Read tail node's config if non-null.
                if (loopTail) {                            // 0x1d0ab2
                    cwvf = loopTail->config;               // 0x1d0ac3-0x1d0af6
                    // Write config to curNode (+0x48)
                    curNode->config = cwvf;                 // 0x1d0b02-0x1d0b48
                    globalRate = loopTail->globalRate;      // 0x1d0b4c → -0x40
                    if (defaultPrecompFlags != loopTail->defaultPrecompFlags) { // 0x1d0b58
                        // Need to scan next-chain for terminator (same as branch)
                        // 0x1d0b7c-0x1d0c38: scan loop->next for Play/Branch/Loop
                        bool foundSetPrecomp = true;       // 0x1d0b94: init 1
                        Node* scan = curNode->loop.get();
                        // ... (same scan logic as branch case)

                        // 0x1d0c81-0x1d0d38: scan curNode->next chain too
                        bool foundSetPrecomp2 = true;
                        Node* scan2 = curNode->next.get();
                        // ... (same scan logic)

                        // 0x1d0d7c: check foundSetPrecomp && foundSetPrecomp2
                        if (!(foundSetPrecomp && foundSetPrecomp2)) {
                            // 0x1d0e83: throw ZIAWGCompilerException(ErrorMessages::format(0xC4))
                            throw ZIAWGCompilerException(
                                ErrorMessages::format(ErrorMessageT(0xC4)));
                        }
                        // Adopt the tail's defaultPrecompFlags.
                        defaultPrecompFlags = loopTail->defaultPrecompFlags; // 0x1d0d95
                    }
                }
            }
            break;
        }

        // ==== Rate (type 0x20) ==== target 0x1d042c
        case NodeType::Rate: {
            curNode->config = cwvf;                         // 0x1d042c-0x1d045d
            // Read the node's existing globalRate → adopt as our running globalRate.
            // (Don't overwrite it — read it.)
            globalRate = curNode->globalRate;                // 0x1d0461-0x1d0467: mov 0x100→-0x40
            // Store defaultPrecompFlags → node.
            curNode->defaultPrecompFlags = defaultPrecompFlags; // (via 0x1d0860)
            break;
        }

        // ==== SetPrecomp (type 0x1000) ==== target 0x1d05ae
        case NodeType::SetPrecomp: {
            curNode->config = cwvf;                         // 0x1d05ae-0x1d05df
            curNode->globalRate = globalRate;                // 0x1d05e6
            // Read the node's existing defaultPrecompFlags → adopt.
            defaultPrecompFlags = curNode->defaultPrecompFlags; // 0x1d05ec-0x1d05f2
            break;
        }

        // ==== Default (all other types) ==== target 0x1d03e9
        default: {
            // Write current running config into node's config (+0x48).
            curNode->config = cwvf;                         // 0x1d03e9-0x1d041a
            curNode->globalRate = globalRate;                // 0x1d0421
            // Store defaultPrecompFlags.
            curNode->defaultPrecompFlags = defaultPrecompFlags; // (via 0x1d0860)
            break;
        }

        } // end switch

    advanceToNext:
        // 0x1d0869-0x1d08ce: Write current running cwvf state into curNode's
        // currentCwvf (+0x68..+0x86) fields.
        curNode->currentCwvf = cwvf;                        // 0x1d0874-0x1d08c8
        curNode->defaultPrecompFlags = defaultPrecompFlags; // (through the loop)

        // Advance to next node in the chain.
        // 0x1d08cf-0x1d0934: curNode = curNode->next.get(); curShared = curNode->next
        Node* nextNode = curNode->next.get();               // 0x1d08cf: +0xB8
        std::shared_ptr<Node> nextShared(curNode->next);    // copy shared_ptr + incref
        // Release old curShared, adopt nextShared.
        curShared = std::move(nextShared);
        curNode = nextNode;                                 // 0x1d08e7 → -0xb0
        // Loop back to 0x1cfd10 → 0x1cfd3e
    }

    // 0x1d0df7-0x1d0e26: release curShared, return.
}

// ============================================================================
// 0x1ccad0 — Prefetch::moveLoadsToFront(shared_ptr<Node>)
// Ends at 0x1cd77b (ret); exception cleanup 0x1cd77c–0x1cd853.
//
// Algorithm: For each WaveformIR used on the current device that needs loading
// (irBool1 == true at +0xDA), creates a new Load node and inserts it before
// the input node. Then does a BFS over the tree: for each Play node whose
// waveform name matches the current WaveformIR and the device string matches,
// it sets the Play's load pointer (+0x18/+0x20) to the new Load node, copies
// the Play's play-reference list (+0xA0) into the Load node, and calls
// Node::updateParent to splice the Play out if it's a child whose parent
// matches the Load's parent.
//
// In short: for every waveform that requires loading, a single Load node is
// created at the front of the node list, and all Play nodes referencing that
// waveform are linked to it. This hoists memory loads ahead of playback.
//
// Parameters (sret convention):
//   rdi = sret shared_ptr<Node> return
//   rsi = this (Prefetch*)
//   rdx = shared_ptr<Node> param (pointer to shared_ptr on stack)
//
// Returns: the (possibly updated) head of the node chain — if loads were
// inserted, the first inserted Load becomes the new head.
// ============================================================================
std::shared_ptr<Node> Prefetch::moveLoadsToFront(std::shared_ptr<Node> node)  // 0x1ccad0
{
    // 0x1ccaee-0x1ccb02: Early return if node is null or has no next sibling
    if (!node || !node->next) {                                        // 0x1ccaf1, 0x1ccafa
        return node;                                                   // 0x1cd6b3
    }

    // 0x1ccb08-0x1ccb18: Get device index from config, fetch used waveforms
    int deviceIdx = config_->deviceIndex;  // config_+0x24              // 0x1ccb0e
    std::vector<std::shared_ptr<WaveformIR>> usedWaves =
        getUsedWavesForDevice(deviceIdx);                              // 0x1ccb18

    // 0x1ccb1d-0x1ccb2e: If no waveforms, jump to cleanup and return node
    if (usedWaves.empty()) {                                           // 0x1ccb2b
        return node;                                                   // 0x1cd6d0-0x1cd6ee
    }

    // 0x1ccb34-0x1ccb58: Setup for main loop over usedWaves
    // r15 = this, rbx = &deque (stack local), r12 = iterator into usedWaves

    // Iterate over each used waveform
    for (auto& waveformIR : usedWaves) {                               // 0x1ccb7b, 0x1ccb6e

        // 0x1ccb7f-0x1ccb86: Skip waveforms that don't need loading
        // Checks WaveformIR+0xD8 (irBool1 / needsLoad flag)
        if (!waveformIR->irBool1) {                                    // 0x1ccb7f  cmpb $0x1,0xd8(%rax)
            continue;                                                  // 0x1ccb86 → 0x1ccb6e
        }

        // 0x1ccb88-0x1ccbbe: Create a new Load node
        // NodeType::Load = 1, asmId from node->asmId (+0x14),
        // numWaveSlots from &config_->numChannelGroups (+0x1C) passed as const int&
        int asmId = node->asmId;                                       // 0x1ccb99: node+0x14
        auto loadNode = std::make_shared<Node>(
            NodeType::Load, asmId, config_->numChannelGroups);         // 0x1ccbbe

        // 0x1ccbc3-0x1ccc53: Copy waveform name from WaveformIR into
        // loadNode->wavesPerDev[config_->deviceIndex]
        // The WaveformIR is accessed via (*r12) — its name field (an optional<string>)
        // is at the base of the object. The name is copied into the slot at
        // wavesPerDev[deviceIdx], which is at +0x28 of the new node, indexed
        // by deviceIdx * 0x20 (sizeof(optional<string>) = 32 bytes).
        Node* ln = loadNode.get();                                     // 0x1ccbc3: r14 = -0x70(%rbp)
        std::optional<std::string> waveName = *waveformIR;  // APPROXIMATE: confirmed: copy name from WaveformIR  // 0x1ccbcb-0x1ccbff

        // 0x1ccc04-0x1ccc8b: Assign the wave name into loadNode->wavesPerDev[deviceIdx]
        int devIdx = config_->deviceIndex;                             // 0x1ccc07: config_+0x24
        ln->wavesPerDev[devIdx] = std::move(waveName);                 // 0x1ccc13-0x1ccc8b

        // 0x1cccb0-0x1cccba: Set loadNode->deviceIndex = config_->deviceIndex
        ln->deviceIndex = config_->deviceIndex;                        // 0x1cccba: mov %edx,0x40(%rax)

        // 0x1cccbd-0x1ccd3b: Get a register number and emplace into nodeStates_
        // Branch on config_->isHirzel (+0x18)
        bool isHirzel = config_->isHirzel;                             // 0x1cccbd: cmpb $0x1,0x18(%rcx)
        int regNum = Resources::getRegisterNumber();                   // 0x1cccc3 / 0x1ccd40
        AsmRegister reg(regNum);                                       // 0x1cccd1 / 0x1ccd4e

        if (isHirzel) {                                                // 0x1cccc1
            // 0x1cccd6-0x1cccfb: emplace loadNode into nodeStates_, set registerHirzel
            auto [it, _] = nodeStates_.emplace(loadNode, PrefetcherNodeState{});
            it->second.registerHirzel = reg;                           // 0x1ccd02: mov %rcx,0x20(%rax)
            // 0x1ccd06-0x1ccd3b: Also emplace and copy devConst_->hasPrecomp (+0x88) flag
            // into the nodeStates_ entry
            bool useDA = devConst_->hasPrecomp;                        // 0x1ccd0a: movzbl 0xda(%rax)
            auto [it2, _2] = nodeStates_.emplace(loadNode, PrefetcherNodeState{});
            it2->second.useDA = useDA;                                 // 0x1ccd37: mov %r14b,0x58(%rax)
        } else {                                                       // 0x1ccd40
            // 0x1ccd40-0x1ccd7f: emplace loadNode into nodeStates_, set registerCervino
            auto [it, _] = nodeStates_.emplace(loadNode, PrefetcherNodeState{});
            it->second.registerCervino = reg;                          // 0x1ccd7f: mov %rcx,0x28(%rax)
        }

        // 0x1ccd83-0x1ccdb4: Insert loadNode before the current head node
        // node->next->insertBefore(loadNode)
        // confirmed: the exact insertion point — inserts before whatever
        // node->next points to, using the shared_ptr stored at -0x70(%rbp)
        Node* curHead = node.get();                                    // 0x1ccd83-0x1ccd8d
        curHead->next->insertBefore(loadNode);                         // 0x1ccdb4

        // 0x1ccdb9-0x1cce24: Update node param — set node = loadNode
        // (so the returned value reflects the new head)
        node = loadNode;                                               // 0x1ccded-0x1ccdf8

        // 0x1cce24-0x1cce86: BFS traversal setup — create a deque, push node->next
        std::deque<std::shared_ptr<Node>> worklist;                    // 0x1cce24-0x1cce2f
        Node* startNode = node.get();                                  // 0x1cce33-0x1cce3a: r14 = *(-0x98)
        worklist.push_back(startNode->next);                           // 0x1cce40-0x1cce86: push next (+0xB8)

        // 0x1ccee9-0x1cd2d6: BFS loop — process nodes from back of deque
        while (!worklist.empty()) {                                    // 0x1cced4-0x1cced7
            // Pop from back
            auto current = worklist.back();                            // 0x1ccee7-0x1ccf0a
            worklist.pop_back();                                       // 0x1ccf62-0x1ccfb0

            Node* cur = current.get();  // r15                         // 0x1ccfb5-0x1ccfbc
            if (!cur) {                                                // 0x1ccfbf
                continue;  // skip nulls                               // 0x1cd2d6
            }

            // 0x1ccfc5-0x1cd0db: Check if current is a Play node with
            // matching waveform name on the correct device
            if (cur->type == NodeType::Load) {                         // 0x1ccfc5: cmpl $0x1,0x44(%r15) — confirmed: compares to 1 (Load)
                int curDevIdx = cur->deviceIndex;                      // 0x1ccfd0: movslq 0x40(%r15)
                if (curDevIdx >= 0) {                                  // 0x1ccfd7
                    // 0x1ccfdd-0x1cd02b: Get wavesPerDev[curDevIdx] from current node
                    auto& curWaveName = cur->wavesPerDev[curDevIdx];   // 0x1ccfdd-0x1cd02b

                    if (curWaveName.has_value()) {                     // 0x1ccff0-0x1ccff5
                        // 0x1cd02f-0x1cd0db: Compare wave name against the
                        // Compare against the WaveformIR's name (loaded from *waveformIR via r12)
                        // Uses bcmp for string comparison
                        const std::string& wfName = waveformIR->name;  // 0x1cd02f: mov (%r12),%rsi — confirmed: loads from WaveformIR
                        bool namesMatch = (*curWaveName == wfName);    // 0x1cd041-0x1cd0be: bcmp

                        if (namesMatch) {                              // 0x1cd0db-0x1cd0de
                            // 0x1cd313: Match found — link this Play to the Load

                            // 0x1cd31e-0x1cd349: Append current node's play refs
                            // to loadNode's play vector
                            // loadNode->play.insert(loadNode->play.end(),
                            //     cur->play.begin(), cur->play.end())
                            auto& loadPlays = loadNode->play;          // 0x1cd31e: -0x70(%rbp)+0xA0
                            loadPlays.insert(loadPlays.end(),
                                cur->play.begin(), cur->play.end());   // 0x1cd344

                            // 0x1cd349-0x1cd552: For each weak_ptr in cur->play,
                            // lock it and set its load pointer (+0x18/+0x20) to
                            // point at loadNode; also emplace into nodeStates_
                            for (auto it = cur->play.begin();
                                 it != cur->play.end(); ++it) {        // 0x1cd361-0x1cd370
                                auto playTarget = it->lock();          // 0x1cd38b-0x1cd3a6
                                if (!playTarget) continue;             // 0x1cd3b5-0x1cd3b8

                                // 0x1cd3be-0x1cd3f8: Set playTarget->load = loadNode
                                // (raw ptr at +0x18, weak_ptr ctrl block at +0x20)
                                Node* target = playTarget.get();
                                // Increment refcounts, store into target+0x18/+0x20
                                target->load = loadNode.get();         // 0x1cd3e7
                                // (also updates weak_ptr control block)

                                // 0x1cd414-0x1cd511: Emplace into nodeStates_ for both
                                // loadNode and playTarget; copy register assignment
                                if (isHirzel) {                        // 0x1cd418
                                    auto [itL, _1] = nodeStates_.emplace(
                                        loadNode, PrefetcherNodeState{});  // 0x1cd44d
                                    auto [itP, _2] = nodeStates_.emplace(
                                        playTarget, PrefetcherNodeState{});  // 0x1cd473
                                    itP->second.registerHirzel =
                                        itL->second.registerHirzel;    // 0x1cd47f-0x1cd483
                                } else {                               // 0x1cd4ab
                                    auto [itL, _1] = nodeStates_.emplace(
                                        loadNode, PrefetcherNodeState{});  // 0x1cd4d7
                                    auto [itP, _2] = nodeStates_.emplace(
                                        playTarget, PrefetcherNodeState{});  // 0x1cd4fd
                                    itP->second.registerCervino =
                                        itL->second.registerCervino;   // 0x1cd509-0x1cd50d
                                }
                            }

                            // 0x1cd552-0x1cd5e9: Call Node::updateParent to splice
                            // the current Play node out of its parent's tree,
                            // replacing it with its own next sibling
                            // parent = cur->parent.lock()
                            // Node::updateParent(parent, current, current->next)
                            auto parentNode = cur->parent.lock();      // 0x1cd563-0x1cd587  confirmed: locks weak at +0xF0
                            if (parentNode) {                          // confirmed
                                Node::updateParent(parentNode,
                                    current, current->next);           // 0x1cd5e9
                            }

                            // After updateParent, fall through to enqueue children
                            // (goto 0x1cd0f0)
                            continue;  // confirmed: continues to next deque item after updateParent
                        }
                    }
                }
            }

            // 0x1cd0f0-0x1cd2d6: Not a matching Play — enqueue children for BFS
            // Enqueue next sibling
            if (cur->next) {                                           // 0x1cd0f7-0x1cd101
                worklist.push_back(cur->next);                         // 0x1cd138-0x1cd186
            }

            // Enqueue each branch child
            for (auto& branch : cur->branches) {                      // 0x1cd18d-0x1cd19e
                worklist.push_back(branch);                            // 0x1cd1c4-0x1cd233
            }

            // Enqueue loop body
            if (cur->loop) {                                           // 0x1cd240-0x1cd24a
                worklist.push_back(cur->loop);                         // 0x1cd250-0x1cd2cf
            }
        }
        // 0x1cce8c-0x1ccec9: Deque destroyed, release loadNode shared_ptr, continue outer loop
    }

    // 0x1cd6d0-0x1cd76a: Cleanup usedWaves vector, return node
    return node;                                                       // 0x1cd767
}

// ============================================================================
// 0x1cdae0 — Prefetch::optimize(shared_ptr<Node>)
// Ends at 0x1cf7b0 (~7.3KB).
//
// Algorithm: BFS traversal of the node tree using a deque as worklist.
// For each node, resolves its parent (via weak_ptr at +0xF0) and looks
// up the parent's PrefetcherNodeState in nodeStates_.
//
// The optimization has three main cases based on node type:
//
// 1. **Play nodes (type == 0x02)**: The core optimization target.
//    If the parent is a Loop (type == 0x08) and the parent's lengthReg
//    (AsmRegister at +0x88) matches the play node's lengthReg, then
//    the parent's load pointer (+0x18/+0x20 weak_ptr) is resolved
//    and if the load is also type Play (type == 0x02), then the
//    play node's next chain is enqueued and we continue.
//
//    Otherwise, the parent is type Play (type == 0x01, i.e. Load):
//    - Walk up the parent chain (parent→parent→...) while each is a
//      Play/Load node, calling sameLoads(parent, currentNode) to see
//      if they reference the same waveform. If sameLoads returns true,
//      call mergeLoads(currentNode, parent) to combine them.
//    - If sameLoads is false, for each parent in the chain, get the
//      waveform name from wavesPerDev[deviceIndex], look it up in
//      wavetableIR_ to get the WaveformIR, compute the waveform byte
//      size as: (channels * ceil(sampleCount / alignment) * alignment *
//      bitsPerSample) / 8, round up. Divide by the node's pageSize
//      from nodeStates_. If the result (pages needed) <= the parent's
//      usedCache, the waveform fits and we can swap the nodes.
//
//    When the parent type != Play and parent has a load pointer
//    (+0x18 non-null), the parent is type 0x02 (a non-load parent with
//    an associated load). In this case, the refTrack in nodeStates_ is
//    adjusted (incremented/decremented) to track how many references
//    share the same load. If the parent is the node's loop target
//    (+0xE0) or next (+0xB8), specific merge/split logic applies.
//
// 2. **Branch nodes (type == 0x04)**:
//    Enqueue the next pointer (+0xB8) if present, then iterate all
//    branch children (+0xC8 vector) and enqueue each.
//
// 3. **Loop nodes (type == 0x08)**:
//    Enqueue next (+0xB8) if present, then enqueue the loop target
//    (+0xE0) if present.
//
// 4. **All other node types**:
//    Just enqueue next (+0xB8) if present.
//
// After processing the node itself, if a Play node's waveform doesn't
// fit in the parent's cache pages (usedCache < pagesNeeded), or if the
// parent references a different load that can't be merged, the node is
// swapped with its parent via Node::swap(), the parent's asmId is
// copied to the node, and the root_ is re-enqueued.
//
// Special case: when a Play node's parent has zero or negative refTrack
// and the parent has a load pointer, the function:
//   a. Calls Node::last(currentNode) to find the tail of the chain
//   b. Clones the current node via Node::clone()
//   c. Calls Node::updateParent to splice the clone after the last node
//   d. Calls Node::updateParent again to detach the current node
//   e. Nulls out the current node's next pointer
//   f. Copies asmId from the load to the clone
//   g. Calls assignLoad(clone, currentNode, config_->isHirzel) to
//      create a new load assignment
//   h. Re-enqueues root_
// ============================================================================
void Prefetch::optimize(std::shared_ptr<Node> node)  // 0x1cdae0
{
    // 0x1cdafd-0x1cdb09: Initialize deque<shared_ptr<Node>> worklist (zeroed, 48 bytes)
    std::deque<std::shared_ptr<Node>> worklist;

    // 0x1cdb09-0x1cdb4c: Push initial node onto worklist
    worklist.push_back(node);                                          // 0x1cdb09

    // 0x1cdb50: if worklist size == 0, return immediately
    if (worklist.empty())                                              // 0x1cdb50
        return;                                                        // 0x1cdb52

    // 0x1cdb6d: precompute &nodeStates_ for repeated use
    auto* nodeStatesPtr = &nodeStates_;                                // 0x1cdb6d

    // --- Main BFS loop ---
    while (!worklist.empty()) {                                        // 0x1cdb90
        // 0x1cdb99-0x1cdc71: Pop back from worklist into `current`
        auto current = worklist.back();                                // 0x1cdb99
        worklist.pop_back();                                           // 0x1cdc1e

        Node* curNode = current.get();                                 // 0x1cdc71
        if (!curNode)                                                  // 0x1cdc75
            continue;                                                  // → 0x1ce201

        // 0x1cdc7e-0x1cdcb7: Resolve parent via weak_ptr at +0xF0
        std::shared_ptr<Node> parent;                                  // 0x1cdc81
        {
            auto* weakCtrl = curNode->/*+0xF8*/ parent_ctrl;
            if (weakCtrl) {                                            // 0x1cdc8f
                Node* locked = weakCtrl->lock();                       // 0x1cdc98
                if (locked) {                                          // 0x1cdca4
                    parent.reset(curNode->/*+0xF0*/ parent_ptr);       // 0x1cdca9
                }
            }
        }

        // 0x1cdcd1-0x1cddab: If parent exists, look up/create nodeStates_ entry
        // for `current`, get its usedCache (+0x40 in hash node = +0x20 in PNS)
        if (parent.get()) {                                            // 0x1cdcc1
            // 0x1cdcd1: emplace current into nodeStates_ (get-or-create)
            auto& state = nodeStates_[current];                        // 0x1cdcf8
            int usedCache = state.usedCache;                           // 0x1cdcfd: +0x40

            // 0x1cdd01-0x1cdd29: Copy current, call getUsedCache(current)
            int actualUsed = getUsedCache(current);                    // 0x1cdd24

            // 0x1cdd4b: if usedCache < actualUsed → skip to type dispatch
            if (usedCache >= actualUsed) {                             // 0x1cdd4b
                // 0x1cdd54-0x1cddab: Subtract actualUsed from nodeStates_[current].usedCache
                int used2 = getUsedCache(current);                     // 0x1cdd77
                nodeStates_[current].usedCache -= used2;               // 0x1cddab
            }
        }

        // =================================================================
        // 0x1cde00: Type dispatch on current node
        // =================================================================
        int nodeType = curNode->type;                                  // 0x1cde04

        if (nodeType == static_cast<int>(NodeType::Loop)) {            // 0x1cde08: cmp $0x8
            // --- LOOP node (type == 0x08) ---
            // 0x1ce024-0x1ce0ba: Enqueue next if present
            if (curNode->next) {                                       // 0x1ce024
                worklist.push_back(curNode->next);                     // 0x1ce065
            }
            // 0x1ce0be-0x1ce148: Enqueue loop target (+0xE0) if present
            if (curNode->loop) {                                       // 0x1ce0be
                worklist.push_back(curNode->loop);                     // 0x1ce0ff
            }
        }
        else if (nodeType == static_cast<int>(NodeType::Branch)) {     // 0x1cde11: cmp $0x4
            // --- BRANCH node (type == 0x04) ---
            // 0x1cdecf-0x1cdf65: Enqueue next if present
            if (curNode->next) {                                       // 0x1cdecf
                worklist.push_back(curNode->next);                     // 0x1cdf10
            }
            // 0x1cdf69-0x1ce01f: Iterate branches vector, enqueue each child
            auto* branchIt = curNode->branches.begin();                // 0x1cdf69
            auto* branchEnd = curNode->branches.end();                 // 0x1cdf70
            while (branchIt != branchEnd) {                            // 0x1cdf77
                worklist.push_back(*branchIt);                         // 0x1cdfdc
                ++branchIt;                                            // 0x1cdf97
            }
        }
        else if (nodeType == static_cast<int>(NodeType::Load)) {       // 0x1cde1a: cmp $0x1
            // --- LOAD node (type == 0x01) — this is the Play optimization case ---

            // 0x1cde23: parent must exist (checked above for Play nodes to optimize)
            if (!parent.get())                                         // 0x1cde23
                goto enqueue_next;                                     // → 0x1cee02

            int parentType = parent->type;                             // 0x1cde33

            // 0x1cde37: Check if parent is a Loop (type == 0x10 i.e. SetVar)
            if (parentType == static_cast<int>(NodeType::SetVar)) {    // 0x1cde37: cmp $0x10
                // 0x1cde40-0x1cde56: Compare parent's lengthReg (+0x88) with current's
                // using AsmRegister::operator==
                bool regsMatch = (parent->lengthReg == curNode->lengthReg); // 0x1cde51

                if (regsMatch) {                                       // 0x1cde5d
                    // 0x1cde65-0x1cdea5: Resolve parent's load pointer
                    // (weak_ptr at +0xF0..+0xF8 of parent)
                    std::shared_ptr<Node> parentLoad;                  // 0x1cde65
                    {
                        auto* wkCtrl = parent->/*+0xF8*/ parent_ctrl;
                        if (wkCtrl) {                                  // 0x1cde76
                            Node* lk = wkCtrl->lock();                 // 0x1cde7f
                            if (lk) {                                  // 0x1cde8b
                                parentLoad.reset(parent->/*+0xF0*/ parent_ptr);
                            }
                        }
                    }

                    // 0x1cdeae: If parentLoad exists and its type is Loop (0x08)
                    if (parentLoad.get() && parentLoad->type == static_cast<int>(NodeType::Loop)) {
                        // 0x1cdec6: Copy parent's asmId to current node
                        curNode->asmId = parent->asmId;                // 0x1cdec6
                    }

                    // 0x1ce685: Enqueue current's next chain + cleanup
                    // Push next of current onto worklist
                    if (curNode->next) {                               // 0x1ce685
                        worklist.push_back(curNode->next);
                    }
                    goto cleanup_parent;
                }
            }

            // 0x1ce259: Parent type != SetVar with matching regs
            // Check parent type for Load-merge cases
            if (parentType == static_cast<int>(NodeType::Load)) {      // 0x1ce259: cmp $0x1
                // --- Parent is also a Load node ---
                // 0x1ce386-0x1ce3ab: Copy parent's asmId to current
                curNode->asmId = parent->asmId;                        // 0x1ce38a

                // 0x1ce391-0x1ce3a9: Save parent as `loadNode` (shared_ptr copy)
                // loadNode = parent
                std::shared_ptr<Node> loadNode = parent;               // 0x1ce391

                // confirmed: Walk up parent chain to find matching loads
                // 0x1ce3e2-0x1ce529: Loop: for each loadNode in the ancestor chain
                bool merged = false;
                while (true) {                                         // 0x1ce3e2
                    // 0x1ce3e9: Check loadNode->deviceIndex
                    if (loadNode->deviceIndex < 0)                     // 0x1ce3e9
                        break;  // → enqueue/no-merge path

                    // 0x1ce3ff-0x1ce454: Get waveform name from
                    // loadNode->wavesPerDev[loadNode->deviceIndex]
                    // (reads optional<string> from vector)
                    // This is done but the string is immediately freed (0x1ce461-0x1ce470)

                    // 0x1ce475-0x1ce4be: Call sameLoads(loadNode, current)
                    bool same = sameLoads(loadNode, current);          // 0x1ce4be

                    if (same) {                                        // 0x1ce526
                        // 0x1ceb19-0x1ceb62: Call mergeLoads(current, loadNode)
                        mergeLoads(current, loadNode);                 // 0x1ceb62
                        // Push root_ onto worklist
                        worklist.push_back(root_);                     // 0x1cec0d-0x1cec38
                        merged = true;
                        break;
                    }

                    // 0x1ce52f-0x1ce55b: Walk to loadNode's parent
                    // Resolve loadNode->parent weak_ptr
                    {
                        auto* wk = loadNode->/*+0xF8*/parent_ctrl;
                        if (wk) {
                            Node* parentOfLoad = wk->lock();           // 0x1ce542
                            if (parentOfLoad && parentOfLoad->type == static_cast<int>(NodeType::Load)) {
                                // 0x1ce583: Update loadNode to the new parent
                                loadNode = std::shared_ptr<Node>(/*parent of load*/);
                                continue;                              // 0x1ce5b5
                            }
                        }
                    }
                    break;  // Parent not a Load → exit loop
                }

                if (merged)
                    goto cleanup_parent;

                // --- Not merged: fall through to waveform-size check ---
                // (goes to the general "other parent type" path below)
            }
            else if (parentType == static_cast<int>(NodeType::Play)) { // 0x1ce266: cmp $0x2
                // --- Parent is a Play node (type == 0x02) ---
                // 0x1ce26f-0x1ce2b8: Call sameLoads(current, parent)
                bool same = sameLoads(current, parent);                // 0x1ce2b8

                if (same) {                                            // 0x1ce304
                    // 0x1ce30d-0x1ce326: Look up current in nodeStates_
                    auto it = nodeStates_.find(current);               // 0x1ce318
                    if (it == nodeStates_.end())                       // 0x1ce320
                        throw std::out_of_range("");

                    // 0x1ce326: Check pageSize >= 2
                    if (it->second.pageSize >= 2) {                    // 0x1ce326
                        // Waveform spans ≥2 pages — enqueue next and continue
                        goto enqueue_next;                             // 0x1ce330 → 0x1cee02
                    }

                    // 0x1ce74b-0x1ce7e1: pageSize < 2 → set counter to 0,
                    // resolve parent's load pointer, call mergeLoads
                    nodeStates_[parent].counter = 0;                   // 0x1ce77a

                    // Resolve parent's load ptr (+0x18/+0x20 as weak_ptr)
                    std::shared_ptr<Node> parentLoadPtr;
                    {
                        auto* wk = parent->/*+0x20*/ load_ctrl;
                        if (wk) {                                      // 0x1ce7af
                            Node* lk = wk->lock();                     // 0x1ce7b4
                            if (lk) {
                                parentLoadPtr.reset(parent->/*+0x18*/ load_ptr);
                            }
                        }
                    }

                    // 0x1ce7d0: mergeLoads(current, parentLoadPtr)
                    mergeLoads(current, parentLoadPtr);                 // 0x1ce7e1

                    // Push root_ onto worklist
                    worklist.push_back(root_);                         // 0x1ce844-0x1ce873
                    goto cleanup_parent;
                }
                else {
                    // 0x1ce8c8: sameLoads returned false
                    // Resolve parent's load pointer (+0xF0/+0xF8 weak_ptr)
                    std::shared_ptr<Node> parentOfParent;              // 0x1ce8cf
                    {
                        auto* wk = parent->/*+0xF8*/ parent_ctrl;
                        if (wk) {                                      // 0x1ce8e0
                            Node* lk = wk->lock();                     // 0x1ce8e5
                            if (lk) {
                                parentOfParent.reset(parent->/*+0xF0*/ parent_ptr);
                            }
                        }
                    }

                    // Fall through to waveform-size-based optimization
                    // (same path as "other parent type")
                }
            }
            else {
                // --- Other parent type (not Load, not Play, not matching SetVar) ---
                // 0x1ce602-0x1ce61e: Look up parent in nodeStates_ → get usedCache
                auto it = nodeStates_.find(parent);                    // 0x1ce610
                if (it == nodeStates_.end())                           // 0x1ce615
                    throw std::out_of_range("");

                int parentUsedCache = it->second.usedCache;            // 0x1ce61e
            }

            // =============================================================
            // confirmed: Waveform size computation and page comparison
            // 0x1ce622-0x1cea98 / 0x1ce904-0x1cea98
            // =============================================================
            // Common path for all parent types that didn't merge above.
            //
            // 1. Get wavetableIR_ pointer (this->+0x110)
            // 2. Get current node's waveform name:
            //    wavesPerDev[deviceIndex] → optional<string>
            // 3. If deviceIndex < 0 or no waveform name → skip to enqueue
            // 4. Call wavetableIR_->getWaveformByName(waveName) to get WaveformIR*
            // 5. From WaveformIR:
            //    - channels = wfm->channelCount (+0xC8, uint16_t)
            //    - sampleCount = wfm->sampleCount (+0xD0, int)
            //    - playConfig = wfm->playConfig (+0x78)
            //    - alignment = playConfig->alignment (+0x44)
            //    - maxSamples = playConfig->maxSamples (+0x40)
            //    - bitsPerSample = playConfig->bitsPerSample (+0x50, signed)
            // 6. Compute aligned sample count:
            //    if (sampleCount != 0) {
            //        int pages = ceil(sampleCount / alignment) * alignment;
            //        pages = min(pages, maxSamples);
            //    }
            // 7. Byte size = channels * alignedSamples * bitsPerSample
            //    Bit-round: byteSize = (byteSize + 7) / 8   (i.e. ceil to bytes)
            // 8. Divide by nodeStates_[current].pageSize to get pagesNeeded
            //
            // (The above is done TWICE in slightly different contexts —
            //  once for the "other parent" path at 0x1ce622 and once for
            //  the "parent==Play, sameLoads==false" path at 0x1ce8c8)
            // =============================================================

            {
                // Look up current's nodeState for usedCache comparison
                auto stateIt = nodeStates_.find(current);              // 0x1cea26
                if (stateIt == nodeStates_.end())
                    throw std::out_of_range("");

                // 0x1cea3c: pagesNeeded = byteSize / state.pageSize
                int pagesNeeded = /* computed above */0;               // 0x1cea37
                int parentUsedCache = /* from parent nodeState */0;

                // 0x1cea95: if parentUsedCache < pagesNeeded → cannot fit
                if (parentUsedCache < pagesNeeded) {                   // 0x1cea95
                    goto enqueue_next;                                 // → 0x1cee02
                }
            }

            // --- Waveform fits: check parent relationships for swap/merge ---

            // 0x1ceaa5: Check if current node type is Loop (0x08)
            if (curNode->type == static_cast<int>(NodeType::Loop)) {   // 0x1ceaac
                // 0x1ceab6: Check if parent's loop (+0xE0) == current
                if (parent->loop.get() == current.get()) {             // 0x1ceab6
                    // --- Current is in parent's loop slot ---
                    // 0x1cec7b-0x1cecbc: emplace parent in nodeStates_,
                    // check refTrack (at +0x38 in hash node = +0x18 PNS)
                    auto& pState = nodeStates_[parent];                // 0x1ceca2
                    if (pState.refTrack > 0) {                         // 0x1cecab
                        // Additional check: if parent type is Loop
                        if (parent->type == static_cast<int>(NodeType::Loop)) {
                            // 0x1ceac7: Also check parent's next == current
                            goto check_next_slot;
                        }
                        goto do_swap_and_enqueue;                      // 0x1cf4b3
                    }

                    // refTrack <= 0: need to split/clone
                    goto clone_and_reassign;                           // → 0x1cef76
                }

check_next_slot:
                // 0x1ceac7: Check if parent's next (+0xB8) == current
                if (parent->next.get() == current.get()) {             // 0x1ceacb
                    // --- Current is parent's next ---
                    // 0x1cef13: emplace/get nodeStates_[parent], increment refTrack
                    nodeStates_[parent].refTrack++;                    // 0x1cef3f

                    // 0x1cef42-0x1cf06d: Get parent's loop (+0xE0), walk it
                    // to the end of its next chain
                    std::shared_ptr<Node> loopEnd = parent->loop;      // 0x1cef49
                    // Walk loopEnd->next until null
                    while (loopEnd && loopEnd->next) {                 // 0x1cefd7
                        loopEnd = loopEnd->next;                       // 0x1cefe3
                    }

                    // Copy asmId from last node in chain to current
                    curNode->asmId = loopEnd->asmId;                   // 0x1cf06a

                    // 0x1cf07b-0x1cf0dc: Call Node::updateParent(parent, current, next_of_current)
                    // This detaches current from parent's next slot
                    Node::updateParent(parent, current, curNode->next); // 0x1cf0dc

                    // 0x1cf17c-0x1cf1c5: Call Node::updateParent(loopEnd, nullptr, current)
                    // This attaches current after loopEnd
                    Node::updateParent(loopEnd, nullptr, current);     // 0x1cf1c5

                    // 0x1cf257-0x1cf277: Null out current's next pointer
                    curNode->next.reset();                             // 0x1cf257

                    // Push root_ onto worklist
                    worklist.push_back(root_);                         // 0x1cf2d4
                    goto cleanup_parent;
                }

                // 0x1cead8: Check if parent's loop == current
                if (parent->loop.get() == current.get()) {             // 0x1cead8
                    // 0x1ceae5-0x1ceb14: emplace nodeStates_[current], decrement refTrack
                    nodeStates_[current].refTrack--;                   // 0x1ceb11
                    goto do_swap_and_enqueue;                          // 0x1cf4b3
                }

                goto do_swap_and_enqueue;                              // 0x1ceadf
            }

clone_and_reassign:
            // 0x1cef76-0x1cf4ae: refTrack <= 0 and parent has a load pointer
            {
                // Resolve current's load pointer (weak_ptr at +0x18/+0x20)
                std::shared_ptr<Node> loadPtr;                         // 0x1cef7d
                {
                    auto* wk = curNode->/*+0x20*/ load_ctrl;
                    if (wk) {                                          // 0x1cef88
                        Node* lk = wk->lock();                         // 0x1cef91
                        if (lk) {
                            loadPtr.reset(curNode->/*+0x18*/ load_ptr);
                        }
                    }
                }

                if (!loadPtr.get()) {                                  // 0x1cf377
                    // No load pointer — need to create one
                    // 0x1cf380-0x1cf4ae: last() + clone() + updateParent + assignLoad
                    auto lastNode = Node::last(current);               // 0x1cf3aa
                    auto cloned = curNode->clone();                    // 0x1cf3c2

                    // Splice: updateParent(lastNode, nullptr, cloned)
                    Node::updateParent(lastNode, nullptr, cloned);     // 0x1cf426

                    // Copy asmId from loadPtr chain end to cloned
                    cloned->asmId = loadPtr ? loadPtr->asmId : curNode->asmId; // 0x1cf454

                    // 0x1cf476-0x1cf48e: assignLoad(cloned, current, config_->isHirzel)
                    bool isHirzel = *reinterpret_cast<const uint8_t*>(
                        reinterpret_cast<const char*>(config_) + 0x18);
                    assignLoad(cloned, current, isHirzel);             // 0x1cf48e
                }
            }

do_swap_and_enqueue:
            // 0x1cf4b3: Node::swap(parent, current)
            Node::swap(parent, current);                               // 0x1cf4be

            // Push root_ onto worklist and continue
            worklist.push_back(root_);                                 // 0x1cf4f7
            goto cleanup_parent;

enqueue_next:
            // 0x1cee02-0x1cee90: Enqueue current->next if non-null
            {
                Node* cn = current.get();                              // 0x1cee02
                if (cn->next) {                                        // 0x1cee06
                    worklist.push_back(cn->next);                      // 0x1cee47
                }
            }
        }
        else {
            // --- Default case: non-Play, non-Branch, non-Loop ---
            // 0x1ce14d-0x1ce1dc: Enqueue next if present
            if (curNode->next) {                                       // 0x1ce14d
                worklist.push_back(curNode->next);                     // 0x1ce18e
            }
        }

cleanup_parent:
        ;
        // 0x1ce1e3-0x1ce254: Release parent shared_ptr, loop back
    }

    // 0x1cdb52: Destroy worklist, return
}

// ============================================================================
// 0x1d0fb0 — Prefetch::allocate(shared_ptr<Node>, shared_ptr<Cache>)
// Ends at 0x1d20b0 (ret). Exception handlers through 0x1d271d.
//
// Iterates the node linked list (via node->next). For each node, switches
// on node->type and performs cache allocation / bookkeeping:
//
// Type dispatch (jump table at 0x95af54, indexed by type-1 for type ≤ 8):
//   type=1 (Play):        0x1d1045 — get waveform name, mark in nameMap_
//   type=2 (Load/SetVar): 0x1d130d — lock load ptr, Cache::play() on load's cachePtr
//   type=4 (Branch):      0x1d117a — recurse into each branch child with scoped cache
//   type=8 (Loop):        0x1d1273 — recurse into loop body with scoped cache
//   type=0x40 (Wait):     0x1d140d — get waveform name, mark in nameMap_ (variant A)
//   type=0x80 (Rate?):    0x1d145a — get waveform name, mark in nameMap_ (variant B)
//   type=0x200 (Table):   0x10af   — lock load ptr, Cache::play() on current node's cachePtr
//
// For Play/Wait/Rate-like nodes (types 1, 0x40, 0x80):
//   1. Read wavesPerDev[deviceIndex] to get optional<string> waveform name
//   2. Insert waveform name into nameMap_ (with bool = true for type 1, false for 0x80)
//   3. If node has load ptr (weak_ptr at +0x18/+0x20): lock it, look up
//      nodeStates_ for the load node, get its cachePtr, then
//      look up nodeStates_ for current node and call
//      Cache::play(cachePtr, nodeState.counter) — replay the cached pointer
//      with the current counter/state.
//   4. If no load ptr: check node play vector (+0xA0). If empty and
//      nodeStates_[node].useDA flag (+0x66 in hash node) is not set,
//      throw ZIAWGCompilerException (error 0xA2).
//
// For Load/SetVar (type 2) and Table (type 0x200):
//   Same load-ptr locking and Cache::play() logic, but operating on the
//   node's own load pointer.
//
// For Play nodes that reach the allocation path (have play refs but
// no existing cache pointer to reuse):
//   1. Look up the load node via nodeStates_[loadNode].cachePtr
//   2. If cachePtr exists and counter >= 2:
//      - Copy cachePtr from loadNode's state → current node's state
//      (transfer the Cache::Pointer shared_ptr)
//   3. Otherwise, if cachePtr is null or not stillInMemory():
//      - Get WaveformIR from WavetableIR::getWaveformByName(waveName)
//      - Compute numSamples from WaveformIR:
//        signal (+0x78) → length (+0x50) → sampleCount (+0x40/+0x44)
//        For node->length > 0: numSamples = ceil(length / sampleCount) * sampleCount
//        Then: byteSize = channels_per_signal * numSamples * bits_per_sample / 8
//        (rounded up to byte boundary)
//      - Call Cache::allocate(waveformIR, byteSize, nameMap_, maxBranches_, split_)
//      - Store returned Cache::Pointer into nodeStates_[node].cachePtr
//   4. If cachePtr is stillInMemory():
//      - Call Cache::reuse(cachePtr)
//      - Look up the original node that owns this cachePtr via nodeByCachePointer()
//      - Call mergeLoads(originalNode, currentNode) to merge them
//
// For Branch (type 4):
//   - Iterate branches vector (node+0xC8..node+0xD0)
//   - For each branch child: create a scoped Cache via cache->getScope(),
//     then recursively call allocate(branchChild, scopedCache)
//
// For Loop (type 8):
//   - If node->loop (+0xE0) is non-null: create scoped Cache via
//     cache->getScope(), recursively call allocate(loopNode, scopedCache)
//
// Exception handling:
//   All std::exception catches are re-thrown as ZIAWGCompilerException
//   with ErrorMessages::format(0xA2, exceptionMessage).
//
// Registers:
//   r14 = this (Prefetch*)
//   rbx = cache (shared_ptr<Cache> — points to the shared_ptr struct)
//   -0xa0(%rbp) = current node ptr (raw Node*)
//   -0x98(%rbp) = current node ctrl block
//   -0x88(%rbp) = &this->nodeStates_ (this+0x10)
//   -0xc8(%rbp) = &this->nameMap_ (this+0x38)
// ============================================================================
void Prefetch::allocate(std::shared_ptr<Node> node,
                        std::shared_ptr<Cache> cache)  // 0x1d0fb0
{
    // 0x1d0fc4-0x1d0fe9: Prologue. Copy node shared_ptr into local.
    auto curNode = node;                                               // -0xa0(%rbp)

    if (!curNode)                                                      // 0x1d0fec: je → 0x1d2077
        return;                                                        // 0x1d2070-0x1d20b0

    // 0x1d0ff2-0x1d1008: Cache pointers to nodeStates_ and nameMap_
    // auto* nodeStatesPtr = &nodeStates_;  // this+0x10, stored at -0x88(%rbp)
    // auto* nameMapPtr = &nameMap_;        // this+0x38, stored at -0xc8(%rbp)

    // ---- Main loop: iterate via node->next chain ----
    while (curNode) {                                                  // 0x1d1017/0x1d101a → 0x1d2070
        Node* cur = curNode.get();                                     // 0x1d1020
        NodeType type = cur->type;                                     // 0x1d1020: mov 0x44(%r12),%eax

        // 0x1d1025-0x1d10a9: Type dispatch
        if (static_cast<int>(type) > 0x3F) {                           // 0x1d1025: cmp $0x3f; jg
            // High type values: 0x40, 0x80, 0x200
            if (type == NodeType::Wait) {                              // 0x1d1090: cmp $0x40 → 0x1d140d
                goto handleWait;
            } else if (static_cast<int>(type) == 0x80) {               // 0x1d1099: cmp $0x80 → 0x1d145a
                goto handleRate80;
            } else if (static_cast<int>(type) == 0x200) {              // 0x1d10a4: cmp $0x200 → 0x1d10af
                goto handleTable200;
            }
            goto advance;  // unknown type → skip                      // 0x1d10a9: jne → 0x1d1660
        }

        // Low type values: jump table for (type-1) in [0..7]
        // type=1 → Play, type=2 → Load, type=4 → Branch, type=8 → Loop
        switch (type) {

        // ==================================================================
        // type=1 (Play): 0x1d1045 — waveform name → nameMap, set true
        // ==================================================================
        case NodeType::Play: {                                         // 0x1d1045
            // 0x1d1045-0x1d108a: Read wavesPerDev[deviceIndex]
            int devIdx = cur->deviceIndex;                             // 0x1d1045: movslq 0x40(%r12)
            if (devIdx < 0)                                            // 0x1d104a: js → advance
                goto advance;

            // Get optional<string> at wavesPerDev + devIdx * 0x20
            auto& wavesVec = cur->wavesPerDev;                         // +0x28
            auto& optName = wavesVec[devIdx];                          // 0x1d1053-0x1d1064: +0x18 check (has_value)
            if (!optName.has_value())                                   // 0x1d1064: cmpb → advance
                goto advance;

            // 0x1d1532-0x1d155e: Copy the waveform name string into local
            std::string waveName = *optName;                           // 0x1d106f-0x1d108a or 0x1d1532-0x1d155e

            // 0x1d155e-0x1d15be: Check if node has a load pointer (weak_ptr at +0x18/+0x20)
            // 0x1d1565-0x1d159f: Lock the node's parent/referenced node from play vector
            auto& curNodeRef = curNode;                                // -0xa0(%rbp)
            Node* refNode = curNodeRef.get();

            // 0x1d156f-0x1d15a5: Lock weak_ptr at node+0x20 → shared_ptr loadNode
            std::shared_ptr<Node> loadNode;                            // -0xb0(%rbp)
            {
                auto* weakCtrl = reinterpret_cast<std::__1::__shared_weak_count*>(
                    refNode->/*+0x20*/ loadCtrl);
                if (weakCtrl) {                                        // 0x1d1573: test+je
                    auto* locked = weakCtrl->lock();                   // 0x1d157c: call lock()
                    if (locked) {
                        loadNode = /* from refNode+0x18 */;            // 0x1d1591-0x1d159c
                    }
                }
            }

            // 0x1d15be-0x1d15e8: Insert waveName into nameMap_[waveName] = true
            nameMap_.emplace(std::move(waveName), true);               // 0x1d15ca-0x1d15e3: call emplace_unique
            // 0x1d15e8: result+0x28 = true (movb $0x1,0x28(%rax))

            goto advance;                                              // 0x1d15f2 → 0x1d1660
        }

    handleWait: {
            // ==================================================================
            // type=0x40 (Wait): 0x1d140d — same waveform-name lookup, nameMap[name]=true
            // ==================================================================
            // 0x1d140d-0x1d14a3: Same wavesPerDev[deviceIndex] lookup
            int devIdx = cur->deviceIndex;                             // 0x1d140d: movslq 0x40(%r12)
            if (devIdx < 0)                                            // 0x1d1415: js → 0x1d14a3
                goto advance;

            auto& optName = cur->wavesPerDev[devIdx];
            if (!optName.has_value())                                  // 0x1d142c: cmpb → 0x1d15c2
                goto waitInsertNameMap;

            std::string waveName = *optName;                           // 0x1d1437-0x1d1455

        waitInsertNameMap:
            // 0x1d15c2-0x1d15e8: nameMap_[waveName] = true
            nameMap_.emplace(std::move(waveName), true);               // 0x1d15ca-0x1d15e3
            // 0x1d15e8: movb $0x1,0x28(%rax)
            goto advance;
        }

    handleRate80: {
            // ==================================================================
            // type=0x80: 0x1d145a — waveform-name lookup, nameMap[name]=false
            // ==================================================================
            int devIdx = cur->deviceIndex;                             // 0x1d145a: movslq 0x40(%r12)
            if (devIdx < 0)
                goto advance;

            auto& optName = cur->wavesPerDev[devIdx];
            if (!optName.has_value())
                goto rate80InsertNameMap;

            std::string waveName = *optName;

        rate80InsertNameMap:
            // 0x1d160c-0x1d1632: nameMap_[waveName] = false
            nameMap_.emplace(std::move(waveName), false);              // 0x1d1614-0x1d162d
            // 0x1d1632: movb $0x0,0x28(%rax)
            goto advance;
        }

    handleTable200: {
            // ==================================================================
            // type=0x200 (Table/SetVar with load): 0x1d10af
            // Lock load ptr → Cache::play(loadNode's cachePtr, curNode's counter)
            // ==================================================================
            // 0x1d10af-0x1d10e2: Lock weak_ptr at node+0x20
            std::shared_ptr<Node> loadNode;                            // -0x50(%rbp)
            {
                auto* weakCtrl = reinterpret_cast<std::__1::__shared_weak_count*>(
                    cur->/*+0x20*/ loadCtrl);
                if (weakCtrl) {                                        // 0x1d10bb: test+je
                    auto* locked = weakCtrl->lock();                   // 0x1d10c4: call lock()
                    if (locked) {
                        loadNode.reset(cur->/*+0x18*/ loadPtr, ...);   // 0x1d10d6-0x1d10e2
                    }
                }
            }

            // 0x1d10e2-0x1d10e8: If loadNode is null, check fallback
            if (!loadNode) {                                           // 0x1d14da: test → 0x1d14e7
                // 0x1d14e7-0x1d14f2: Check nodeStates_[curNode].useDA flag
                // hash_node+0x66 = PNS struct internal offset
                auto& state = nodeStates_[curNode];
                if (!curNode->config.dummy) {                           // 0x1d14ee: cmpb $0x0,0x66(%rax) — confirmed: Node+0x66 = config.dummy
                    // 0x1d2106: throw ZIAWGCompilerException(format(0xA2, "..."))
                    throw ZIAWGCompilerException(
                        ErrorMessages::format(ErrorMessageT(0xA2),
                            "missing load for table node"));
                }
                goto advance;
            }

            // 0x1d10e8-0x1d1160: Look up nodeStates_ for loadNode and curNode,
            // get load's cachePtr, call Cache::play()
            {
                Cache* cachePtr = cache.get();                         // 0x1d10e8: mov (%rbx),%r13
                // Emplace loadNode into nodeStates_ (get-or-create)
                auto& loadState = nodeStates_[loadNode];               // 0x1d10ef-0x1d110c: call emplace
                // Copy load's cachePtr (shared_ptr at PNS+0x28)
                auto loadCachePtr = loadState.cachePtr;                // 0x1d1111-0x1d1125: movups 0x48(%rax)

                // Emplace curNode into nodeStates_
                auto& curState = nodeStates_[curNode];                 // 0x1d112a-0x1d114e: call emplace
                // Read curState.counter as PointerState
                auto pointerState = static_cast<Cache::PointerState>(
                    curState.counter);                                 // 0x1d1153: mov 0x30(%rax),%edx

                // Call Cache::play(loadCachePtr, pointerState)
                cachePtr->play(loadCachePtr, pointerState);            // 0x1d1160: call Cache::play
            }

            // 0x1d1165-0x1d1175: Release loadCachePtr, then release loadNode
            goto cleanupLoadAndAdvance;                                // → 0x1d14f8
        }

        // ==================================================================
        // type=2 (Load): 0x1d130d — same load-ptr lock + Cache::play pattern
        // ==================================================================
        case static_cast<NodeType>(2): {                               // 0x1d130d
            // 0x1d130d-0x1d1340: Lock weak_ptr at node+0x20
            std::shared_ptr<Node> loadNode;
            {
                auto* weakCtrl = reinterpret_cast<std::__1::__shared_weak_count*>(
                    cur->loadCtrl);
                if (weakCtrl) {
                    auto* locked = weakCtrl->lock();
                    if (locked) {
                        loadNode.reset(cur->loadPtr, ...);
                    }
                }
            }

            if (!loadNode) {                                           // 0x1d14bb-0x1d14c8
                auto& state = nodeStates_[curNode];
                if (!curNode->config.dummy) {                           // 0x1d14cf: cmpb $0x0,0x66(%rax) — confirmed: Node+0x66
                    // 0x1d20b1: throw ZIAWGCompilerException(format(0xA2, "..."))
                    throw ZIAWGCompilerException(
                        ErrorMessages::format(ErrorMessageT(0xA2),
                            "missing load for node"));
                }
                goto advance;
            }

            // 0x1d1346-0x1d13be: Same pattern — emplace load & cur, Cache::play
            {
                Cache* cachePtr = cache.get();                         // 0x1d1346: mov (%rbx),%r13
                auto& loadState = nodeStates_[loadNode];               // 0x1d1349-0x1d136a
                auto loadCachePtr = loadState.cachePtr;                // 0x1d136f-0x1d1383

                auto& curState = nodeStates_[curNode];                 // 0x1d1388-0x1d13ac
                auto pointerState = static_cast<Cache::PointerState>(
                    curState.counter);                                 // 0x1d13b1: mov 0x30(%rax),%edx

                cachePtr->play(loadCachePtr, pointerState);            // 0x1d13be: call Cache::play
            }

            goto cleanupLoadAndAdvance;
        }

        // ==================================================================
        // type=4 (Branch): 0x1d117a — recurse into each branch child
        // ==================================================================
        case NodeType::Branch: {                                       // 0x1d117a
            // 0x1d117a-0x1d118a: Load branches vector begin/end
            auto* branchBegin = cur->branches.data();                  // +0xC8
            auto* branchEnd = /* +0xD0 */;

            if (branchBegin == branchEnd)                              // 0x1d118a: je → advance
                goto advance;

            // 0x1d11a0-0x1d1262: For each branch child:
            for (auto* it = branchBegin; it != branchEnd; ++it) {      // 0x1d11a0: add $0x10
                // Copy the branch shared_ptr
                auto branchChild = *it;                                // 0x1d11b8-0x1d11cc

                // Create a scoped cache: cache->getScope()
                auto scopedCache = cache.get()->getScope();            // 0x1d11de: call Cache::getScope

                // Recurse: allocate(branchChild, scopedCache)
                allocate(branchChild, scopedCache);                    // 0x1d11f0: call allocate (self-recursive)

                // Release scopedCache and branchChild shared_ptrs
                // 0x1d11f5-0x1d1262: shared_ptr cleanup
            }
            goto advance;                                              // 0x1d126e → 0x1d11a0 (loop) or → 0x1d1660
        }

        // ==================================================================
        // type=8 (Loop): 0x1d1273 — recurse into loop body
        // ==================================================================
        case static_cast<NodeType>(8): {                               // 0x1d1273
            // 0x1d1273-0x1d127e: Read loop ptr (node+0xE0)
            Node* loopBody = cur->loop.get();                          // 0x1d1273: mov 0xe0(%r12)
            if (!loopBody)                                             // 0x1d127e: je → advance
                goto advance;

            // 0x1d1284-0x1d129f: Copy loop shared_ptr, increment refcount
            auto loopNode = cur->loop;                                 // -0x110(%rbp)

            // Create scoped cache
            auto scopedCache = cache.get()->getScope();                // 0x1d12b1: call Cache::getScope

            // Recurse: allocate(loopNode, scopedCache)
            allocate(loopNode, scopedCache);                           // 0x1d12c3: call allocate

            // 0x1d12c8-0x1d1308: Release scopedCache and loopNode
            goto advance;  // or cleanupLoadAndAdvance
        }

        default:
            goto advance;                                              // 0x1d1660
        } // end switch

    cleanupLoadAndAdvance:
        // 0x1d14f8-0x1d152d: Release loadNode shared_ptr (if held), fall through to advance
        ;

    advance:
        // ==================================================================
        // 0x1d1660-0x1d16c5: Advance to next node in the chain
        // ==================================================================
        // curNode = curNode->next
        {
            Node* curRaw = curNode.get();                              // 0x1d1660: mov -0xa0(%rbp),%rax
            Node* nextRaw = curRaw->next.get();                        // 0x1d1667: mov 0xb8(%rax),%r12
            auto nextCtrl = /* next's ctrl block at +0xC0 */;          // 0x1d166e: mov 0xc0(%rax)

            // Swap: release old curNode, adopt nextRaw
            curNode = curRaw->next;                                    // 0x1d167f-0x1d16c5
        }
        continue;                                                      // → 0x1d1017 (while test)
    } // end while

    // ========================================================================
    // 0x1d1532-0x1d15a5: PLAY NODE ALLOCATION PATH (entered from type=1 when
    // node has a load pointer)
    //
    // This is the complex path for Play nodes that have a load reference.
    // It attempts to reuse existing cache allocations or performs new ones.
    // ========================================================================

    // confirmed: The following block is reached via the Play case when
    // the node has a valid load pointer. The logic is interleaved with
    // the main loop above but conceptually forms a separate allocation path.

    // --- Phase 1: Check load node's existing cachePtr ---
    // 0x1d16ca-0x1d17b3: If loadNode has a cachePtr in nodeStates_:
    //   Look up nodeStates_[loadNode].cachePtr
    //   If cachePtr exists (non-null at hash_node+0x48):
    //     Check nodeStates_[loadNode].counter >= 2
    //     If so: transfer cachePtr to nodeStates_[curNode]
    //            (move shared_ptr from load's state → current's state)

    // --- Phase 2: Check if cachePtr is still in memory ---
    // 0x1d17b3-0x1d1a19: If cachePtr is null OR counter < 2:
    //   Call Cache::stillInMemory(cachePtr) → bool
    //   If stillInMemory:
    //     Call Cache::reuse(cachePtr)
    //     Find original owner: nodeByCachePointer(cachePtr)
    //     Call mergeLoads(originalOwner, currentNode)
    //   If NOT stillInMemory:
    //     Fall through to Phase 3 (new allocation)

    // --- Phase 3: New cache allocation ---
    // 0x1d1a19-0x1d2016: No reusable pointer available.
    //   Iterate play vector (node+0xA0..+0xA8)
    //   Lock first weak_ptr → get play target node
    //   If split_ (0xBC) is set: use split allocation path
    //   Otherwise: standard allocation path
    //
    //   Standard path (0x1d1a92-0x1d1cbc):
    //     Get waveform name from wavesPerDev[deviceIndex]
    //     waveformIR = WavetableIR::getWaveformByName(waveName)  // 0x1d1ba7
    //     Also get a second waveformIR for the other channel:
    //       waveformIR2 = WavetableIR::getWaveformByName(waveName2)  // 0x1d1c31
    //     Compute numSamples:
    //       signal = waveformIR2->signal  // +0x78
    //       channels = waveformIR2->channels  // +0xC8 (uint16)
    //       sampleCount = signal->sampleCount  // signal+0x40/+0x44
    //       length = cur->length  // node+0x90
    //       if (length > 0):
    //         numSamples = ceil(length / sampleCount) * sampleCount  // 0x1d1c52-0x1d1c68
    //       else:
    //         numSamples = 0
    //       byteSize = channels * numSamples * bitsPerSample / 8     // 0x1d1c71-0x1d1c8d
    //       byteSize = ceil_to_byte(byteSize)
    //
    //     Call Cache::allocate(waveformIR, byteSize, nameMap_,
    //                          maxBranches_, split_)                  // 0x1d1cb7
    //     Store result → nodeStates_[curNode].cachePtr               // 0x1d1ce8-0x1d1d01
    //
    //   Split path (0x1d1b1b-0x1d1efc):  confirmed
    //     Same WavetableIR lookups but uses a different size calculation.
    //     Gets node->length (0x90) and computes:
    //       numSamples = length * channels_per_signal * 2            // 0x1d1ec5-0x1d1ecb
    //     Then calls Cache::allocate with this size.
    //     Store result → nodeStates_[curNode].cachePtr               // 0x1d1f25-0x1d1f43

    // 0x1d2070-0x1d20b0: Epilogue — release curNode, return
    return;

    // ========================================================================
    // Exception handlers: 0x1d20b1-0x1d271d
    // All catch std::exception, wrap message in ZIAWGCompilerException(0xA2)
    // ========================================================================
}

// ============================================================================
// 0x1d33f0 — Prefetch::linkLoad(shared_ptr<Node>)
//
// Creates a Load node for the given play node and inserts it before the
// play node in the tree.  Calls createLoad() to build the Load node,
// then calls Node::insertBefore() to link it.
// ============================================================================
void Prefetch::linkLoad(std::shared_ptr<Node> node) // 0x1d33f0
{
    // 0x1d3405: this → rsi (Prefetch*)
    // 0x1d3409: copy node shared_ptr into local
    auto localNode = node;  // 0x1d340d-0x1d3416: movups + incq refcount

    // 0x1d3429: call createLoad(localNode)
    auto loadNode = createLoad(localNode);  // sret return

    // 0x1d342e-0x1d3445: release localNode shared_ptr

    // 0x1d3449: test if createLoad returned non-null
    if (loadNode) {
        // 0x1d346b: insertBefore the play node
        node->insertBefore(loadNode);
    }

    // 0x1d34b8-0x1d34e4: release loadNode shared_ptr
}

// ============================================================================
// 0x1d53a0 — Prefetch::assignLoad(shared_ptr<Node>, shared_ptr<Node> const&, bool)
//
// Assigns a load node to a play node. Sets the play node's loadNode
// weak_ptr (at +0x18/+0x20) to point to the load. Optionally emplaces
// both into nodeStates_ map.
//
// node = the play node (from parameter rsi)
// load = the load node (from parameter rdx)
// flag = whether to use isHirzel path (from parameter ecx)
//
// If load is null (rdx->ptr == nullptr), returns immediately.
// ============================================================================
void Prefetch::assignLoad(std::shared_ptr<Node> node,
                          std::shared_ptr<Node> const& load,
                          bool flag) // 0x1d53a0
{
    // 0x1d53a0-0x1d53a6: if (*load == null) return
    if (!load) return;

    Node* playNode = node.get();  // 0x1d53c9: mov (%rsi), %rcx

    // 0x1d53cc-0x1d53da: copy load's ctrl block ptr, incq strong + weak
    // 0x1d53df: store load.get() → playNode->loadNode (+0x18)
    // 0x1d53e7: store load's ctrl block → playNode->loadCtrl (+0x20) as weak_ptr
    // This sets the play node's loadNode_ weak_ptr to point to the load
    playNode->loadNode = load.get();   // +0x18: raw pointer
    // (also sets weak_ptr ctrl block at +0x20, releasing old one)

    // 0x1d540c: add 0x10 to this → &nodeStates_
    // 0x1d5410: test flag
    if (flag) {
        // isHirzel path
        // 0x1d5431: emplace load into nodeStates_
        auto [it1, _1] = nodeStates_.emplace(load, PrefetcherNodeState{});
        // 0x1d544e: emplace node into nodeStates_
        auto [it2, _2] = nodeStates_.emplace(node, PrefetcherNodeState{});
        // 0x1d5453-0x1d545b: copy it1->second.registerHirzel (+0x20) to it2->second
        it2->second.registerHirzel = it1->second.registerHirzel;  // +0x20 offset
    } else {
        // Cervino path
        // 0x1d5494: emplace load into nodeStates_
        auto [it1, _1] = nodeStates_.emplace(load, PrefetcherNodeState{});
        // 0x1d54b1: emplace node into nodeStates_
        auto [it2, _2] = nodeStates_.emplace(node, PrefetcherNodeState{});
        // 0x1d54b6-0x1d54ba: copy registerCervino (+0x28) from load's state to node's state
        it2->second.registerCervino = it1->second.registerCervino;  // +0x28 offset
    }
}

// ============================================================================
// 0x1d4a10 — Prefetch::createLoad(shared_ptr<Node>)
//
// Creates a Load node for a given Play/SetPlay node. Returns null if
// the node already has a load, or if the node's isPlaceholder flag is set.
//
// Steps:
//   1. Check node type is Play (0x2) or SetPlay (0x200)
//   2. Check if load already exists (via parent weak_ptr at +0x20)
//      - If parent exists AND loadNode (+0x18) is set → already loaded, skip
//      - Special case: if parent exists but loadNode is null, check isPlaceholder
//   3. Check isPlaceholder (+0x66) — if true, return null
//   4. Get a new register number from Resources::getRegisterNumber()
//   5. Create a new Node with type=Load (0x1), same asmId as source
//   6. Copy deviceIndex from config_->numChannelGroups
//   7. Copy waveNames_ (optional<string> vector at +0x28) from source
//   8. Copy playLength (+0x88) from source node
//   9. Branch on config_->isHirzel:
//      - If isHirzel: set AsmRegister in PNS+0x00 (registerHirzel)
//      - Else: set AsmRegister in PNS+0x08 (registerCervino)
//  10. Call assignLoad(this, node, loadNode, isHirzel)
//  11. Add source node as weak_ptr to loadNode's loadTargets_ (+0xA0)
//  12. For each valid wave name, look up WaveformIR and mark it not fixed (+0xD8 = false)
//  13. Call collectUsedWaves(node)
// ============================================================================
std::shared_ptr<Node> Prefetch::createLoad(std::shared_ptr<Node> node) // 0x1d4a10
{
    std::shared_ptr<Node> result;  // sret, init null

    Node* n = node.get();
    if (!n) return result;

    // 0x1d4a41-0x1d4a4e: check type is Play(0x2) or SetPlay(0x200)
    int nodeType = n->type;  // +0x44
    if (nodeType != 0x200 && nodeType != 0x2)
        return result;

    // 0x1d4a54-0x1d4a6b: check parent weak_ptr at +0x20
    // Lock weak_ptr → if parent exists:
    //   0x1d4a6b: check loadNode (+0x18) — if non-null, already loaded → return
    //   0x1d4f51: if loadNode is null, check isPlaceholder and re-enter
    // If no parent (lock returns null): fall through

    // 0x1d4aac: check isPlaceholder (+0x66)
    if (n->isPlaceholder)  // +0x66
        return result;

    // 0x1d4ab6: get fresh register number
    int regNum = Resources::getRegisterNumber();

    // 0x1d4abe-0x1d4ae9: create Node(NodeType::Load, asmId, seqRef)
    NodeType loadType = NodeType::Load;  // 0x1 → -0x68(%rbp)
    int asmId = n->asmId;               // +0x14
    result = std::make_shared<Node>(loadType, asmId, /*seqRef*/ asmId);

    Node* loadNode = result.get();

    // 0x1d4b5b-0x1d4b9c: copy waveNames from source to load node
    // source->waveNames_ is at +0x28, vector<optional<string>>
    loadNode->waveNames = n->waveNames;  // copy vector

    // 0x1d4b96-0x1d4b9c: set deviceIndex from config
    loadNode->deviceIndex = config_->numChannelGroups;  // +0x24 of config → +0x40 of node

    // 0x1d4c45-0x1d4c53: copy playLength
    loadNode->playLength = n->playLength;  // +0x88

    // 0x1d4c53-0x1d4cc4: branch on isHirzel
    bool isHirzel = config_->isHirzel;  // +0x18
    if (isHirzel) {
        // Create AsmRegister with regNum, emplace into nodeStates_
        AsmRegister reg(regNum);
        auto [it, _] = nodeStates_.emplace(result, PrefetcherNodeState{});
        it->second.registerHirzel = reg;  // +0x20 offset
    } else {
        AsmRegister reg(regNum);
        auto [it, _] = nodeStates_.emplace(result, PrefetcherNodeState{});
        it->second.registerCervino = reg;  // +0x28 offset
    }

    // 0x1d4cc4-0x1d4cf4: call assignLoad(this, node, result, isHirzel)
    assignLoad(node, result, isHirzel);

    // 0x1d4d28-0x1d4d52: push source node as weak_ptr into loadNode->loadTargets_ (+0xA0)
    // (loadNode+0xA0 is vector<weak_ptr<Node>>)

    // 0x1d4d64-0x1d4d93: allValidWaves() — copy_if on source waveNames (+0x28)
    // that have values, into a temp vector
    // 0x1d4d9b-0x1d4e6b: for each valid wave name, look up in wavetableIR_
    // and mark waveform as not fixed (+0xD8 = false)
    auto wavetable = wavetableIR_.get();
    for (auto const& waveName : n->waveNames) {
        if (!waveName.has_value()) continue;
        auto wfm = wavetable->getWaveformByName(waveName);
        if (!wfm) continue;
        wfm->fixed_ = false;  // +0xD8: mark as needing load
    }

    // 0x1d4eb3-0x1d4ebe: call collectUsedWaves(node)
    collectUsedWaves(node);

    return result;
}

// ============================================================================
// 0x1d5040 — Prefetch::mergeLoads(shared_ptr<Node>, shared_ptr<Node>)
//
// Merges load targets from one Load node into another. Iterates through
// the source Load's loadTargets_ (weak_ptr vector at +0xA0), and for each
// target that still has a valid parent chain, calls assignLoad to redirect
// it to the destination Load node.
//
// After processing all targets, if all were merged, removes the source
// Load from the tree via Node::remove().
// ============================================================================
void Prefetch::mergeLoads(std::shared_ptr<Node> dst,
                          std::shared_ptr<Node> src) // 0x1d5040
{
    // 0x1d5055-0x1d507e: null checks + both must be Load type (0x1)
    if (!dst || !src) return;
    if (dst->type != NodeType::Load || src->type != NodeType::Load)
        return;

    // 0x1d5087-0x1d509f: iterate src->loadTargets_ (+0xA0)
    auto& srcTargets = src->loadTargets;  // vector<weak_ptr<Node>> at +0xA0
    auto it = srcTargets.begin();

    while (it != srcTargets.end()) {
        // 0x1d50cc-0x1d5120: for each weak_ptr in dst->loadTargets_,
        // check if the target still has a valid parent chain
        // (lock weak_ptr at +0x20, follow loadNode +0x18)
        // If the parent chain leads back to src → this target should be merged

        // Lock the weak_ptr to get target node
        auto targetNode = it->lock();  // 0x1d512b-0x1d5134: lock weak at *it

        if (!targetNode) {
            ++it;
            continue;
        }

        // 0x1d5167-0x1d5179: call assignLoad(this, targetNode, dst, isHirzel)
        assignLoad(targetNode, dst, config_->isHirzel);

        // 0x1d51ab-0x1d5267: iterate dst->loadTargets_ to check if the
        // weak_ptr target already matches (avoiding duplicates)
        // If not found among existing, emplace the weak_ptr into dst->loadTargets_
        auto& dstTargets = dst->loadTargets;
        bool found = false;
        for (auto& wp : dstTargets) {
            auto locked = wp.lock();
            if (locked.get() == targetNode.get()) {
                found = true;
                break;
            }
        }
        if (!found) {
            dstTargets.emplace_back(*it);
        }

        ++it;
    }

    // 0x1d52b9-0x1d52d3: after loop, remove src from tree
    // Copy src into local shared_ptr, call Node::remove()
    Node::remove(src);
}

// ============================================================================
// 0x1cbf60 — Prefetch::placeLoads()
//
// Top-level load placement orchestrator. Steps:
//   1. Check if getMemoryHighWatermark() fits in cache (for SHFQA/SHFSG)
//      - If not, throw ZIAWGCompilerException with memory usage message
//   2. Check if getRequiredMemory() <= cacheSize and config is non-append mode
//      - If so, proceed to single-root load placement
//      - Otherwise, set split_ = true and:
//        a. Call moveLoadsToFront(root_) → returns new root
//        b. For HDAWG-like devices (>=2 cores): insert SyncHirzel/AwgReady
//           markers before root's first child
//        c. Look up appropriate error message (0x31 or 0x32) for logging
//        d. Call logFunc_ with the message
//        e. Call optimize(root_)
//   3. Call optimizeSync(root_)
//   4. Copy root's playConfig (+0x48..+0x67) to root's cwvfConfig (+0x68..+0x87)
//   5. Call optimizeCwvf(root_)
//   6. Call allocate(root_, cache_)
// ============================================================================
void Prefetch::placeLoads() // 0x1cbf60
{
    // 0x1cbf73: call getMemoryHighWatermark()
    size_t watermark = getMemoryHighWatermark();

    // 0x1cbf78-0x1cbf8c: check device type — *(config_) == 0x20 (SHFSG) or 0x10 (SHFQA)
    int devType = *(int*)config_;         // config_->deviceType at +0x00
    int cacheSize = devConst_->waveformMemorySize; // devConst_ +0x0C

    if (devType == 0x20 /*SHFSG*/ || devType == 0x10 /*SHFQA*/) {
        if (watermark > (size_t)cacheSize) {
            // 0x1cc4db-0x1cc578: throw ZIAWGCompilerException
            // Compute memory in samples: watermark * 8 / bitsPerSample / 1024.0
            // Format error message 0x33 with two doubles
            double memUsed = (double)watermark / (devConst_->bitsPerSample / 8) / 1024.0;
            double memAvail = (double)cacheSize / (devConst_->bitsPerSample / 8) / 1024.0;
            throw ZIAWGCompilerException(
                ErrorMessages::format<double, double>(ErrorMessageT(0x33), memUsed, memAvail));
        }
    }

    // 0x1cbf96-0x1cbfa5: call getRequiredMemory()
    size_t required = getRequiredMemory();

    // 0x1cbfa5-0x1cbfb5: save root_ to local
    auto localRoot = root_;

    // 0x1cbfbf-0x1cbfda: check if required <= cacheSize AND !config_->appendMode
    if (required <= (size_t)cacheSize || config_->appendMode) {
        // 0x1cbfda: split_ = true
        split_ = true;

        // 0x1cbffe-0x1cc008: call moveLoadsToFront(root_)
        auto newRoot = moveLoadsToFront(root_);
        localRoot = newRoot;
    }

    // 0x1cc085-0x1cc250: if localRoot is non-null, check device type for
    // SyncHirzel/AwgReady insertion
    if (localRoot) {
        int devTypeVal = *(int*)config_;
        int numCores = config_->numCores;  // +0x1C

        if (devTypeVal == 0x2 /*HDAWG*/ && numCores >= 2) {
            // 0x1cc0af-0x1cc0d2: create SyncHirzel node (type 0x2000)
            auto syncNode = std::make_shared<Node>(NodeType::SyncHirzel,
                                                    localRoot->asmId);
            // Insert before first child or set as first child
            if (localRoot->firstChild) {
                localRoot->firstChild->insertBefore(syncNode);
            } else {
                localRoot->firstChild = syncNode.get();
                // set control block
            }
        } else if (devTypeVal == 0x1 /*UHFLI*/ || devTypeVal == 0x2 /*HDAWG*/ ||
                   devTypeVal == 0x4 /*UHFQA*/ || devTypeVal == 0x8 /*SHFQA-like*/) {
            // 0x1cc117-0x1cc193: bitmask check (0x100010104 covers bits 0,2,8,32)
            // Create AwgReady node (type 0x8000)
            auto readyNode = std::make_shared<Node>(NodeType::AwgReady,
                                                     localRoot->asmId);
            if (localRoot->firstChild) {
                localRoot->firstChild->insertBefore(readyNode);
            } else {
                localRoot->firstChild = readyNode.get();
            }
        }
    }

    // 0x1cc250-0x1cc2ea: if !split_, look up error message and call logFunc_
    if (!split_) {
        // Check device type for which error message to use
        int devTypeVal = *(int*)config_;
        int numCores = config_->numCores;
        ErrorMessageT msgId;
        if (numCores >= 2 && devTypeVal == 0x2) {
            msgId = ErrorMessageT(0x31);  // HDAWG multi-core message
        } else {
            msgId = ErrorMessageT(0x32);  // single-core message
        }

        // 0x1cc2ea-0x1cc304: look up message and call logFunc_
        auto& msg = ErrorMessages::messages[msgId];
        logFunc_(msg);

        // 0x1cc327: call optimize(root_)
        optimize(root_);
    }

    // 0x1cc354-0x1cc374: call optimizeSync(root_)
    optimizeSync(root_);

    // 0x1cc3a1-0x1cc3b5: copy root's playConfig to cwvfConfig
    // movups from +0x48, +0x57 → movups to +0x68, +0x77
    Node* rootNode = root_.get();
    // memcpy(&rootNode->cwvfConfig, &rootNode->playConfig, sizeof(PlayConfig));
    // (copies 32 bytes from +0x48 to +0x68)

    // 0x1cc3d8: call optimizeCwvf(root_)
    optimizeCwvf(root_);

    // 0x1cc43a-0x1cc448: call allocate(root_, cache_)
    allocate(root_, cache_);
}

// ============================================================================
// 0x1c7eb0 — Prefetch::getUsedCache(shared_ptr<Node>) const
// Ends at 0x1c8738.
//
// Recursively computes total cache usage (in pages) for a node subtree.
// For Load (type==1) or Play (type==2) nodes with a waveform name assigned,
// computes the waveform size in pages. Otherwise recurses into children.
//
// Field references (raw binary offsets for cross-check):
//   Node+0x28 = wavesPerDev, Node+0x40 = deviceIndex, Node+0x44 = type
//   Node+0x90 = length, Node+0xB8 = next, Node+0xC8 = branches, Node+0xE0 = loop
//   WaveformIR+0xC8 = signal.channels_ (uint16_t)
//   WaveformIR+0xD0 = signal.length_ (uint64_t, "numRepeats")
//   DeviceConstants+0x40 = waveformGranularity, +0x44 = waveformPageSize
//   DeviceConstants+0x50 = bitsPerSample
//   PrefetcherNodeState+0x10 = counter, +0x1C = pageSize
// ============================================================================
int Prefetch::getUsedCache(std::shared_ptr<Node> node) const  // 0x1c7eb0
{
    // Get waveform name for the node's current device index
    int devIdx = node->deviceIndex;
    std::optional<std::string> waveName;
    if (devIdx >= 0 && static_cast<size_t>(devIdx) < node->wavesPerDev.size()) {
        waveName = node->wavesPerDev[devIdx];
    }

    // --- Case 1: Load node (type == 0x01) with a waveform name ---
    if (waveName.has_value() &&
        static_cast<int>(node->type) == static_cast<int>(NodeType::Load))
    {
        auto waveform = wavetableIR_->getWaveformByName(waveName);
        uint16_t channels = waveform->signal.channels_;

        // Look up page size from nodeStates_
        auto it = nodeStates_.find(node);
        int pageSize = it->second.pageSize;

        if (node->length != 0) {
            // length is specified directly: pages = (length * channels) / pageSize
            return (node->length * channels) / pageSize;               // 0x1c8180 (approx)
        } else {
            // Compute full waveform size in bytes
            uint64_t numRepeats = waveform->signal.length_;
            uint32_t stride = devConst_->waveformPageSize;             // DC+0x44
            uint32_t base = devConst_->waveformGranularity;            // DC+0x40

            uint64_t numPages;
            if (numRepeats != 0) {
                // ceil_div(numRepeats, stride) * stride, capped at base
                numPages = ((numRepeats + stride - 1) / stride) * stride;
                if (numPages > base) {
                    numPages = base;
                }
            } else {
                numPages = 0;
            }

            // Total size in bits, then convert to bytes (round up)
            uint32_t bitsPerSample = devConst_->bitsPerSample;         // DC+0x50
            uint64_t totalBits = numPages * channels * bitsPerSample;
            int byteSize = static_cast<int>((totalBits + 7) / 8);

            return byteSize / pageSize;                                // 0x1c8480 (approx)
        }
    }

    // --- Case 2: Play node (type == 0x02) with a waveform name ---
    if (waveName.has_value() &&
        static_cast<int>(node->type) == static_cast<int>(NodeType::Play))
    {
        // Check counter in nodeStates_; if 0, skip to recursive case
        auto stateIt = nodeStates_.find(node);
        if (stateIt != nodeStates_.end() && stateIt->second.counter == 0) {
            goto recursive;  // counter == 0 means this Play doesn't own cache
        }

        auto waveform = wavetableIR_->getWaveformByName(waveName);
        uint16_t channels = waveform->signal.channels_;
        int pageSize = stateIt->second.pageSize;

        if (node->length != 0) {
            return (node->length * channels) / pageSize;
        } else {
            uint64_t numRepeats = waveform->signal.length_;
            uint32_t stride = devConst_->waveformPageSize;             // DC+0x44
            uint32_t base = devConst_->waveformGranularity;            // DC+0x40

            uint64_t numPages;
            if (numRepeats != 0) {
                numPages = ((numRepeats + stride - 1) / stride) * stride;
                if (numPages > base) {
                    numPages = base;
                }
            } else {
                numPages = 0;
            }

            uint32_t bitsPerSample = devConst_->bitsPerSample;         // DC+0x50
            uint64_t totalBits = numPages * channels * bitsPerSample;
            int byteSize = static_cast<int>((totalBits + 7) / 8);

            return byteSize / pageSize;
        }
    }

recursive:
    // --- Case 3: No waveform or not Load/Play — recurse into children ---
    int result = 0;

    if (node->next) {
        result += getUsedCache(node->next);
    }

    if (node->loop) {
        result += getUsedCache(node->loop);
    }

    for (auto const& child : node->branches) {
        result += getUsedCache(child);
    }

    return result;
}

} // namespace zhinst
