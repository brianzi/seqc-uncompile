// ============================================================================
// Reconstructed from disassembly of _seqc_compiler.so
// Function: zhinst::Prefetch::print(shared_ptr<Node>, int) const
// Address:  0x1c5dd0 — 0x1c7799 (+ exception handlers to ~0x1c7cd1)
//
// This is a recursive debug printer that outputs the node tree to std::cout.
// It does NOT use logFunc_; all output goes directly to std::cout.
//
// String constants confirmed from .rodata.
// ============================================================================

#include <iostream>
#include <memory>
#include <optional>
#include <string>

#include "zhinst/prefetch.hpp"
#include "zhinst/node.hpp"

namespace zhinst {

// Helper: operator<< for AddressImpl<uint32_t> — prints hex via boost::format("%08x")
// Address: 0x1c7ce0
// (Defined separately, declared here for reference)
// std::ostream& operator<<(std::ostream& os, detail::AddressImpl<uint32_t> addr);

void Prefetch::print(std::shared_ptr<Node> node, int indent) const  // 0x1c5dd0
{
    if (!node) {
        // 0x1c6028: node is null — try to print from root_ (+0x60) instead
        if (!root_) {                                           // 0x1c6028
            return;
        }
        // Copy root_ into the node arg and restart (the binary does a loop back)
        node = root_;                                           // 0x1c6043
        // Falls through to restart the function from the top
        // (In the binary this is an actual loop back to 0x1c5df7)
    }

    // Repeat: check node again (handles the root_ copy case looping back)
    // The binary structure loops back if node was null but root_ was set.
    // In C++ we just process the (now non-null) node.

    // 0x1c5df7: Print "[" address "]"
    std::cout << "[";                                           // 0x1c5dfe
    // Set cout width to indent, fill=' '
    auto* rdbuf_vt = *reinterpret_cast<intptr_t**>(&std::cout);
    // The binary sets cout's width to indent and fill to ' '
    std::cout.width(indent);                                    // 0x1c5e19..0x1c5e60
    std::cout.fill(' ');                                        // 0x1c5e85..0x1c5ed0
    std::cout << "";                                            // prints indent spaces // 0x1c5eeb

    Node* n = node.get();

    // 0x1c5ef0: Big switch on n->type (+0x44)
    int typeCode = static_cast<int>(n->type);                   // 0x1c5ef4

    switch (static_cast<NodeType>(typeCode)) {

    // ---- Play (0x02) ----                                     // 0x1c5f1a
    case NodeType::Play: {
        // Gets waveAtCurrentDeviceIndex inline
        auto waveName = n->waveAtCurrentDeviceIndex();
        if (!waveName.has_value()) {
            // 0x1c6912: "load asmID " << asmId                  // APPROXIMATE — falls to common play path
            std::cout << "load asmID " << n->asmId;             // 0x1c6912
            // then falls to common play printing at 0x1c7570
        } else {
            // Has wave name:
            // 0x1c6ba4: destroy temp waveName, then print "load " + second waveName copy
            std::cout << "load " << waveName.value();           // 0x1c6bcb

            // Get second copy of waveAtCurrentDeviceIndex
            auto waveName2 = n->waveAtCurrentDeviceIndex();     // 0x1c6be6
            if (!waveName2.has_value()) {
                // empty wave — print empty string + " ("
            }
            // Print wave " (" pageSize ")"
            std::cout << waveName2.value_or("") << " (";       // APPROXIMATE

            // nodeStates_.find(node) at 0x1c6f4e
            auto it = nodeStates_.find(node);
            if (it != nodeStates_.end()) {
                // +0x3c from hash_node value = PNS.pageSize (+0x1C from PNS start)
                std::cout << it->second.pageSize;               // 0x1c6f5c
            }
            std::cout << ")";                                   // 0x1c6f67

            // Check if cachePtr is set
            auto it2 = nodeStates_.find(node);
            if (it2->second.cachePtr) {                         // 0x1c6fb8: +0x48 from hash_node
                std::cout << " @ ";                             // 0x1c6fbf
                auto& state = nodeStates_.at(node);
                // Print cache address: state.cachePtr->address  // 0x1c6fe5: +0x28 -> deref -> value
                std::cout << detail::AddressImpl<uint32_t>(      // APPROXIMATE
                    *reinterpret_cast<uint32_t*>(state.cachePtr.get()));
            }

            // 0x1c6ff3: Check load weak_ptr (node +0x18/+0x20)
            std::shared_ptr<Node> loadNode;
            if (n->loadCtrl) {                                  // 0x1c7004
                // weak_ptr::lock()
                loadNode = std::shared_ptr<Node>(n->load,       // APPROXIMATE
                    [](Node*){});  // simplified; binary does __shared_weak_count::lock()
            }

            if (loadNode) {
                auto& loadState = nodeStates_.at(loadNode);     // APPROXIMATE
                if (loadState.cachePtr) {                       // 0x1c704a
                    std::cout << " @ ";                         // 0x1c7051
                    auto& ls2 = nodeStates_.at(loadNode);
                    std::cout << detail::AddressImpl<uint32_t>(
                        *reinterpret_cast<uint32_t*>(ls2.cachePtr.get()));
                }
                // " (load-asmID "
                std::cout << " (load-asmID ";                   // 0x1c7090
                std::cout << loadNode->asmId;                   // 0x1c70a8
                std::cout << ")";                               // 0x1c70b3
            }

            // " with R"
            std::cout << " with R";                             // 0x1c70ce

            // Check config_->isHirzel (+0x18)
            bool isHirzel = *reinterpret_cast<const bool*>(
                reinterpret_cast<const char*>(config_) + 0x18); // 0x1c70e6
            if (isHirzel) {
                auto& state = nodeStates_.at(node);
                // Use registerHirzel (+0x00)                    // 0x1c70f2
                std::cout << static_cast<int>(state.registerHirzel);
            } else {
                auto& state = nodeStates_.at(node);
                // Use registerCervino (+0x08)                   // 0x1c70f9..0x1c7104
                std::cout << static_cast<int>(state.registerCervino);
            }

            // " asmID " << asmId
            std::cout << " asmID ";                             // 0x1c7121
            std::cout << n->asmId;                              // 0x1c7135

            // " rate " << length (at +0x4c, which is config.channelMask actually)
            std::cout << " rate ";                              // 0x1c7147
            std::cout << *reinterpret_cast<int*>(               // APPROXIMATE
                reinterpret_cast<char*>(n) + 0x4C);             // 0x1c715b — node+0x4C

            // " globalRate " << globalRate (+0x100)
            std::cout << " globalRate ";                        // 0x1c716d
            std::cout << n->globalRate;                         // 0x1c7181

            // " precompFlags " << precompFlags (+0x60 from config, node+0x60)
            std::cout << " precompFlags ";                      // 0x1c7196
            std::cout << detail::AddressImpl<uint32_t>(         // APPROXIMATE
                *reinterpret_cast<uint32_t*>(
                    reinterpret_cast<char*>(n) + 0x60));        // 0x1c71aa

            std::cout << "\n";                                  // 0x1c71bc
        }

        // Release temp loadNode if needed, then fall through to recurse on next
        // 0x1c71d7: release loadNode refcount

        // After all the play-specific printing, the function prints "\n"
        // and then recurses on node->next
        break;  // falls through to recursive next below
    }

    // ---- Load (0x01) ----                                     // 0x1c636f
    case NodeType::Load: {
        auto waveName = n->waveAtCurrentDeviceIndex();          // 0x1c636f inline
        if (!waveName.has_value()) {
            // 0x1c68aa: "play\n" then recurse on next
            std::cout << "play\n";                              // 0x1c68aa
            // recurse next
            if (auto nxt = n->next) {
                print(nxt, indent);
            }
            return;
        }
        // Has value: destroy temp, then print "play " + second copy
        std::cout << "play " << waveName.value_or("") << " ("; // 0x1c6b21..0x1c6f28  // APPROXIMATE

        auto it = nodeStates_.find(node);
        if (it != nodeStates_.end()) {
            std::cout << it->second.pageSize;
        }
        std::cout << ")";

        // Check cachePtr
        auto it2 = nodeStates_.find(node);
        if (it2 != nodeStates_.end() && it2->second.cachePtr) {
            std::cout << " @ ";
            auto& state = nodeStates_.at(node);
            std::cout << detail::AddressImpl<uint32_t>(
                *reinterpret_cast<uint32_t*>(state.cachePtr.get()));
        }

        // Check load weak_ptr
        std::shared_ptr<Node> loadNode;                         // APPROXIMATE
        if (n->loadCtrl) {
            // lock weak ptr
        }
        if (loadNode) {
            auto& loadState = nodeStates_.at(loadNode);
            if (loadState.cachePtr) {
                std::cout << " @ ";
                std::cout << detail::AddressImpl<uint32_t>(
                    *reinterpret_cast<uint32_t*>(
                        nodeStates_.at(loadNode).cachePtr.get()));
            }
            std::cout << " (load-asmID ";
            std::cout << loadNode->asmId;
            std::cout << ")";
        }

        // " with R" + register
        std::cout << " with R";
        bool isHirzel = *reinterpret_cast<const bool*>(
            reinterpret_cast<const char*>(config_) + 0x18);
        {
            auto& state = nodeStates_.at(node);
            if (isHirzel) {
                std::cout << static_cast<int>(state.registerHirzel);
            } else {
                std::cout << static_cast<int>(state.registerCervino);
            }
        }

        // " asmID " << asmId
        std::cout << " asmID " << n->asmId;

        // play vector iteration                                 // 0x1c73a3
        if (n->play.size() > 0) {                              // 0x1c73aa vs 0x1c73b1
            std::cout << " pointing to asmID ";                 // 0x1c73be
            for (auto& wp : n->play) {                          // 0x1c73ea
                auto sp = wp.lock();
                if (!sp) continue;
                if (sp) {
                    std::cout << sp->asmId << " ";              // 0x1c7443
                }
            }
        }

        // "(" << getUsedCache(node) << "," << nodeStates_[node].usedCache << ")"
        std::cout << "(";                                       // 0x1c748b
        {
            auto nodeCopy = node;
            std::cout << detail::AddressImpl<uint32_t>(
                getUsedCache(nodeCopy));                        // 0x1c74cf
        }
        std::cout << ",";                                       // 0x1c74ec

        auto itState = nodeStates_.find(node);
        if (itState != nodeStates_.end()) {
            std::cout << detail::AddressImpl<uint32_t>(
                itState->second.usedCache);                     // 0x1c7518: +0x40 from hash_node value = +0x20 PNS
        }
        std::cout << ")";                                       // 0x1c7523
        std::cout << "\n";                                      // 0x1c7570

        // recurse on next
        if (auto nxt = n->next) {
            print(nxt, indent);
        }
        return;
    }

    // ---- Branch (0x04) ----                                   // 0x1c63c3
    case NodeType::Branch: {
        // Iterate branches vector (+0xC8..+0xD0)
        for (auto it = n->branches.begin(); it != n->branches.end(); ++it) {
            if (!*it) continue;                                 // 0x1c6404

            // If not the first branch, print indent + newline
            if (it != n->branches.begin()) {                    // 0x1c640b
                // Print "          " (10 spaces, but uses indent)
                std::cout << "          ";                      // 0x1c641d  // APPROXIMATE
                std::cout.width(indent);                        // resets width
                std::cout.fill(' ');
                std::cout << "";
            }

            // Print "branch\n"
            std::cout << "branch\n";                            // 0x1c64ca

            // Recurse into branch with indent+2
            print(*it, indent + 2);                             // 0x1c650d
        }

        // After branches, recurse on next
        if (auto nxt = n->next) {                               // 0x1c6627
            print(nxt, indent);
        }
        return;
    }

    // ---- Loop (0x08) ----                                     // 0x1c6299
    case NodeType::Loop: {
        // If loop link exists, print "loop\n" and recurse on loop with indent+2
        if (n->loop) {                                          // 0x1c6299
            std::cout << "loop\n";                              // 0x1c62aa
            print(n->loop, indent + 2);                         // 0x1c62eb
        }
        // Then recurse on next
        if (auto nxt = n->next) {                               // 0x1c6322
            print(nxt, indent);
        }
        return;
    }

    // ---- SetVar (0x10) ----                                   // 0x1c6552
    case NodeType::SetVar: {
        std::cout << "setvar ";                                 // 0x1c6559
        std::cout << " R";                                      // 0x1c656d
        // AsmRegister::operator int() on node+0x88
        int regVal = static_cast<int>(n->lengthReg);           // 0x1c658c  // APPROXIMATE
        std::cout << regVal;                                    // 0x1c659a
        std::cout << " asmID " << n->asmId;                    // 0x1c65a6..0x1c65b7
        std::cout << "\n";                                      // 0x1c65c2
        // recurse on next
        if (auto nxt = n->next) {
            print(nxt, indent);
        }
        return;
    }

    // ---- Rate (0x20) ----                                     // 0x1c620c
    case NodeType::Rate: {
        std::cout << "rate ";                                   // 0x1c6213
        std::cout << n->globalRate;                             // 0x1c6227: +0x100  // APPROXIMATE
        std::cout << "\n";                                      // 0x1c6235
        // recurse on next
        if (auto nxt = n->next) {
            print(nxt, indent);
        }
        return;
    }

    // ---- Lock (0x40) ----
    case NodeType::Lock: {
        std::cout << "lock ";                                   // 0x1c619d
        // Gets waveAtCurrentDeviceIndex, prints name, then "\n", recurse next
        auto waveName = n->waveAtCurrentDeviceIndex();          // 0x1c61c5
        std::cout << waveName.value_or("") << "\n";            // 0x1c6a30..0x1c6a68
        // recurse on next
        if (auto nxt = n->next) {
            print(nxt, indent);
        }
        return;
    }

    // ---- Unlock (0x80) ----                                   // 0x1c6752
    case NodeType::Unlock: {
        std::cout << "unlock ";                                 // 0x1c6759
        auto waveName = n->waveAtCurrentDeviceIndex();          // 0x1c6771
        std::cout << waveName.value_or("") << "\n";            // 0x1c6d01..0x1c6d47
        // recurse on next
        if (auto nxt = n->next) {
            print(nxt, indent);
        }
        return;
    }

    // ---- SyncCervino (0x100) ----                             // 0x1c67c1
    case NodeType::SyncCervino: {
        std::cout << "sync_cervino\n";                          // 0x1c67c8
        // recurse on next
        if (auto nxt = n->next) {
            print(nxt, indent);
        }
        return;
    }

    // ---- Table (0x200) ----                                   // 0x1c60a7
    case NodeType::Table: {
        auto waveName = n->waveAtCurrentDeviceIndex();          // 0x1c60a7 inline
        if (!waveName.has_value()) {
            // 0x1c696e: "table\n" then recurse next
            std::cout << "table\n";                             // 0x1c6975
            if (auto nxt = n->next) {
                print(nxt, indent);
            }
            return;
        }
        // Has value: destroy temp, then "table " + second copy + " (" + pageSize + ")"
        std::cout << "table " << waveName.value_or("") << " (";// 0x1c6e00..0x1c77f7  // APPROXIMATE

        auto it = nodeStates_.find(node);
        if (it != nodeStates_.end()) {
            std::cout << it->second.pageSize;
        }
        std::cout << ")";

        // Check cachePtr
        auto it2 = nodeStates_.find(node);
        if (it2 != nodeStates_.end() && it2->second.cachePtr) {
            std::cout << " @ ";
            std::cout << detail::AddressImpl<uint32_t>(
                *reinterpret_cast<uint32_t*>(
                    nodeStates_.at(node).cachePtr.get()));
        }

        // Check load weak_ptr
        std::shared_ptr<Node> loadNode;
        if (n->loadCtrl) {
            // lock weak ptr                                    // APPROXIMATE
        }
        if (loadNode) {
            auto& loadState = nodeStates_.at(loadNode);
            if (loadState.cachePtr) {
                std::cout << " @ ";
                std::cout << detail::AddressImpl<uint32_t>(
                    *reinterpret_cast<uint32_t*>(
                        nodeStates_.at(loadNode).cachePtr.get()));
            }
            std::cout << " (load-asmID " << loadNode->asmId << ")";
        }

        // " with R" + register (using PNS +0x28 = registerCervino, always)
        std::cout << " with R";                                 // 0x1c79a4
        {
            auto& state = nodeStates_.at(node);
            // +0x28 from PNS = registerCervino (+0x08) — the binary adds 0x28 to hash_node result
            std::cout << static_cast<int>(state.registerCervino); // 0x1c79c5
        }

        // " asmID " << asmId
        std::cout << " asmID " << n->asmId;                    // 0x1c79e2..0x1c79f6

        // " rate " << node+0x4c
        std::cout << " rate ";
        std::cout << *reinterpret_cast<int*>(
            reinterpret_cast<char*>(n) + 0x4C);                 // APPROXIMATE

        // " globalRate " << globalRate
        std::cout << " globalRate " << n->globalRate;

        // " precompFlags " << precompFlags
        std::cout << " precompFlags ";
        std::cout << detail::AddressImpl<uint32_t>(
            *reinterpret_cast<uint32_t*>(
                reinterpret_cast<char*>(n) + 0x60));            // APPROXIMATE

        std::cout << "\n";                                      // 0x1c7a7d

        // Release loadNode, then fall through to recurse on next
        // 0x1c7a8e: release, then jumps to 0x1c6986 = "table\n" path's next recursion
        if (auto nxt = n->next) {
            print(nxt, indent);
        }
        return;
    }

    // ---- SetPrecomp (0x1000) ----                             // 0x1c6674
    case NodeType::SetPrecomp: {
        std::cout << "setPrecomp ";                             // 0x1c667b
        // Print node+0x60 as AddressImpl<uint32_t>
        std::cout << detail::AddressImpl<uint32_t>(
            *reinterpret_cast<uint32_t*>(
                reinterpret_cast<char*>(n) + 0x60));            // 0x1c668f  // APPROXIMATE
        std::cout << "\n";                                      // 0x1c669a
        // recurse on next
        if (auto nxt = n->next) {
            print(nxt, indent);
        }
        return;
    }

    // ---- SyncHirzel (0x2000) ----                             // 0x1c5f9e
    case NodeType::SyncHirzel: {
        std::cout << "sync_hirzel asmID ";                      // 0x1c5fa5
        std::cout << n->asmId;                                  // 0x1c5fb9
        std::cout << "\n";                                      // 0x1c5fc4
        // recurse on next
        if (auto nxt = n->next) {
            print(nxt, indent);
        }
        return;
    }

    // ---- PlainLoad (0x4000) ----                              // 0x1c66fe
    case NodeType::PlainLoad: {
        auto waveName = n->waveAtCurrentDeviceIndex();          // 0x1c66fe inline
        if (!waveName.has_value()) {
            // 0x1c6940: "plainload asmID " << asmId << "\n"
            std::cout << "plainload asmID " << n->asmId;       // 0x1c6947
            std::cout << "\n";                                  // 0x1c7716
            // recurse on next
            if (auto nxt = n->next) {
                print(nxt, indent);
            }
            return;
        }
        // Has value: "plainload " + waveName + " (" + pageSize + ")"
        std::cout << "plainload ";                              // 0x1c6c75

        auto waveName2 = n->waveAtCurrentDeviceIndex();         // 0x1c6c93
        std::cout << waveName2.value_or("") << " (";           // 0x1c75ee..0x1c7634

        auto it = nodeStates_.find(node);
        if (it != nodeStates_.end()) {
            std::cout << it->second.pageSize;
        }
        std::cout << ")";

        // Check cachePtr, print " @ " + address
        auto it2 = nodeStates_.find(node);
        if (it2 != nodeStates_.end() && it2->second.cachePtr) {
            std::cout << " @ ";
            std::cout << detail::AddressImpl<uint32_t>(
                *reinterpret_cast<uint32_t*>(
                    nodeStates_.at(node).cachePtr.get()));
        }

        // " with R" + register
        std::cout << " with R";                                 // 0x1c76a4
        {
            bool isHirzel2 = *reinterpret_cast<const bool*>(
                reinterpret_cast<const char*>(config_) + 0x18);
            auto& state = nodeStates_.at(node);
            if (isHirzel2) {
                std::cout << static_cast<int>(state.registerHirzel);
            } else {
                std::cout << static_cast<int>(state.registerCervino);
            }
        }

        // " asmID " << asmId
        std::cout << " asmID " << n->asmId;                    // 0x1c76f4..0x1c7704

        std::cout << "\n";                                      // 0x1c7716
        // recurse on next
        if (auto nxt = n->next) {
            print(nxt, indent);
        }
        return;
    }

    // ---- AwgReady (0x8000) ----                               // 0x1c6113
    case NodeType::AwgReady: {
        std::cout << "awg_ready asmID ";                        // 0x1c611a
        std::cout << n->asmId;                                  // 0x1c612e
        std::cout << "\n";                                      // 0x1c6139
        // recurse on next
        if (auto nxt = n->next) {
            print(nxt, indent);
        }
        return;
    }

    // ---- Unknown type ----                                    // 0x1c6829
    default: {
        // Log warning: "Unknown Node::NodeType with code" << typeCode << "."
        // Uses LogRecord with severity=1 (Warning)
        // logging::detail::LogRecord record(logging::Severity(1));   // 0x1c6835
        // record << "Unknown Node::NodeType with code" << typeCode << ".";
        // (record destructor flushes)                           // 0x1c68a0
        return;
    }

    } // end switch

    // Common tail for types that don't have their own return:
    // Most cases explicitly return after recursing on next.
    // The Play case (0x02) falls through here to recurse on next:

    // 0x1c7570/0x1c7584: Print "\n" then recurse on node->next if non-null
    std::cout << "\n";
    if (auto nxt = n->next) {                                   // 0x1c7587
        print(nxt, indent);
    }
}

} // namespace zhinst
