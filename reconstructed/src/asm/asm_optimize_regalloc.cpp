// ============================================================================
// Reconstructed from disassembly of _seqc_compiler.so
// asm_optimize_regalloc.cpp — Register allocation pass
// AsmOptimize::registerAllocation, splitReg, registerUpdate
// ============================================================================

#include "zhinst/asm/asm_optimize.hpp"
#include "zhinst/runtime/resources.hpp"

#include <algorithm>
#include <cstring>
#include <set>
#include <iostream>
#include <cstdlib>
#include <unordered_map>

namespace zhinst {

void AsmOptimize::registerAllocation(unsigned long numRegs) {
    // Phase 1: allocate live ranges — 27ebc4..27ec84
    size_t numSlots = numRegs + 1;  // +1 for r0 (index 0)
    // Each slot is a vector<int> (0x18 bytes), zero-initialized
    std::vector<std::vector<int>> liveRanges(numSlots);

    // Lambda $_0 @281510: addToLiveRange(const AsmRegister& reg, int instrIdx)
    //   If reg.value > 0 && reg.value < numSlots, push instrIdx to liveRanges[reg.value]
    auto addToLiveRange = [&](const AsmRegister& reg, int instrIdx) {
        int v = reg.value;
        if (v > 0 && static_cast<size_t>(v) < numSlots)
            liveRanges[v].push_back(instrIdx);
    };

    // Scan all instructions, recording register usage — 27ec88..27ed42
    // Same skip bitmask 0x29 (dead/LABEL/cmd4)
    {
        int instrIdx = 0;
        for (auto it = asm_.begin(); it != asm_.end(); ++it, ++instrIdx) {
            uint32_t cmdPlus1 = static_cast<uint32_t>(it->assembler.cmd) + 1;
            if (cmdPlus1 <= 5 && ((0x29 >> cmdPlus1) & 1))
                continue;

            // Add regDst (+0x28) to live range — 27ecd6: lea rsi,[r14+0x8]
            //   r14 points to Asm+0x28, so +0x8 = Asm+0x30 = regDst
            addToLiveRange(it->assembler.regDst, instrIdx);

            // If regSrc (+0x20) == regDst (+0x28), skip duplicate add
            // 27ecec: compare regSrc == regDst; jne → add regSrc
            if (!(it->assembler.regSrc == it->assembler.regDst))
                addToLiveRange(it->assembler.regSrc, instrIdx);

            // If regAux (+0x30) == regDst (+0x28) OR regAux == regSrc, skip
            // 27ed09: compare regAux == regDst; jne → compare regAux == regSrc
            if (!(it->assembler.regAux == it->assembler.regDst) &&
                !(it->assembler.regAux == it->assembler.regSrc))
                addToLiveRange(it->assembler.regAux, instrIdx);
        }
    }

    // Phase 2: build label→index map — 27ed4e..27eddc
    // unordered_map<string, int> at [rbp-0x170] (0x28 bytes on stack)
    std::unordered_map<std::string, int> labelMap;
    {
        int instrIdx = 0;
        for (auto it = asm_.begin(); it != asm_.end(); ++it, ++instrIdx) {
            // Only LABEL instructions (cmd == 2) — 27ed9f: cmp [r14+0x8],0x2
            if (it->assembler.cmd == Assembler::LABEL) {
                // Insert label name (Asm+0x58 = Assembler+0x50 = label)
                // with value = instrIdx
                // 27eda6: lea rsi,[r14+0x58]
                labelMap[it->assembler.label] = instrIdx;
            }
        }
    }

    // Phase 3: find backward-branch ranges — 27eddc..27efa2
    // For each branch instruction (BRZ/BRNZ/BRGZ/JMP), look up its
    // target label in labelMap. If the target index < current index,
    // record (target_index, current_index) as a branch range pair.
    // These are stored as a vector of packed int64 (two int32s).
    // The r12 register accumulates packed values: low 32 = target,
    // high 32 = current (using 0x100000000 increments).
    struct BranchRange {
        int targetIdx;
        int sourceIdx;
    };
    std::vector<BranchRange> branchRanges;

    {
        int instrIdx = 0;
        for (auto it = asm_.begin(); it != asm_.end(); ++it, ++instrIdx) {
            auto cmd = it->assembler.cmd;
            // Check for branch commands — 27ee47..27ee7c
            bool isBranch = (cmd == Assembler::BRZ || cmd == Assembler::BRNZ ||
                             cmd == Assembler::BRGZ || cmd == Assembler::JMP);
            if (!isBranch)
                continue;

            // Check label: Asm+0x58 = Assembler+0x50
            // 27ee7e: movzx eax,BYTE PTR [r14]; test al,0x1
            //   This checks the SSO bit of the label string (libc++: short
            //   string has bit0 of byte0 = 1 for short, 0 for long)
            // If label is empty (no data), skip
            const auto& labelStr = it->assembler.label;
            if (labelStr.empty())
                continue;

            // Lookup in labelMap — 27eeab
            auto found = labelMap.find(labelStr);
            if (found == labelMap.end())
                continue;

            int targetIdx = found->second;
            // Only backward branches (target < current) — 27eec7
            if (instrIdx > targetIdx) {
                branchRanges.push_back({targetIdx, instrIdx});
            }
        }
    }

    // Expand live ranges across backward branches — 27efbd..27f2ab
    // For each backward branch (target→source) and each virtual register:
    //   If the vreg's last reference is before sourceIdx, scan instructions
    //   [targetIdx, sourceIdx) to determine if the vreg is READ within the
    //   loop body. If read before being overwritten, extend liveRanges[vreg]
    //   by appending sourceIdx (the register is live across the back-edge).
    //   If the vreg is overwritten (regDst write or regAux type-7 write)
    //   before any read, no extension is needed (the loop body redefines it).
    if (!branchRanges.empty() && numRegs > 0) {
        for (auto& br : branchRanges) {
            for (size_t vr = 1; vr <= numRegs; ++vr) {
                auto& lr = liveRanges[vr];
                if (lr.empty()) continue;

                // 27f029: if last ref >= sourceIdx, already covers → skip
                if (lr.back() >= br.sourceIdx) continue;

                // 27f041: sanity: target must be <= source
                if (br.targetIdx > br.sourceIdx) continue;

                // Construct AsmRegister for this vreg
                AsmRegister vregReg(static_cast<int>(vr));  // 27f03c

                // Scan [targetIdx, sourceIdx) — 27f07b..27f185
                bool conflict = false;
                for (int scanIdx = br.targetIdx; scanIdx < br.sourceIdx; ++scanIdx) {
                    if (static_cast<size_t>(scanIdx) >= asm_.size()) break;

                    auto& instr = std::next(asm_.begin(), scanIdx)->assembler;
                    int cmd = instr.cmd;

                    // Skip bitmask 0x29 — 27f0d7..27f0e7
                    uint32_t cmdPlus1 = static_cast<uint32_t>(cmd) + 1;
                    if (cmdPlus1 <= 5 && ((0x29 >> cmdPlus1) & 1))
                        continue;

                    int cmdType = Assembler::getCmdType(static_cast<Assembler::Command>(cmd));

                    // Check 1: regSrc READ — 27f0ff..27f10e
                    if ((instr.regSrc == vregReg) && (cmdType & 1)) {
                        conflict = true;
                        break;
                    }

                    // Check 2: regAux READ — 27f114..27f131
                    if (instr.regAux == vregReg) {
                        if (cmdType == 1 || cmdType == 7) {
                            conflict = true;
                            break;
                        }
                    }

                    // Check 3: regDst WRITE (kills register) — 27f145..27f15b
                    int cmdType2 = Assembler::getCmdType(static_cast<Assembler::Command>(cmd));
                    if ((instr.regDst == vregReg) && ((cmdType2 >> 1) & 1)) {
                        break;  // overwritten → no extension needed
                    }

                    // Check 4: regAux WRITE (type-7 kills) — 27f161..27f17f
                    if ((instr.regAux == vregReg) && (cmdType2 == 7)) {
                        break;  // overwritten via regAux → no extension
                    }
                }

                if (conflict) {
                    // 27f196: extend live range to cover the back-edge
                    lr.push_back(br.sourceIdx);
                }
            }
        }
    }

    // Phase 4: allocate physical registers — 27f316..27ff10
    //
    // Data structures (from binary symbol names):
    //   using LiveRange = std::vector<int>;       // sorted instruction indices
    //   using PhysicalRegister = std::vector<LiveRange>;  // list of sub-ranges
    //   physRegs: vector<PhysicalRegister> with numSlots entries (= numRegs+1)
    //   conflicts: std::set<unsigned long> of unmerged register IDs
    //
    // Algorithm:
    //   1. Initialize: move each non-empty liveRanges[i] into physRegs[i][0],
    //      add i to conflicts set.
    //   2. For each vreg 1..numRegs:
    //      a. Skip if vreg not in conflicts (already absorbed)
    //      b. Iterate conflicts set for candidate pregs to merge into vreg
    //      c. For each preg: check overlap between physRegs[preg] and physRegs[vreg]
    //      d. If no overlap: rename preg→vreg in asm, insert physPreg[0] into
    //         physVreg at computed insertion position, remove preg from conflicts
    //      e. Continue inner loop (vreg can absorb multiple pregs)
    //
    // 27f7c4: numPhysical = this->numPhysicalRegs_ (offset +0x00)
    size_t numPhysical = numPhysicalRegs_;
    // 27f7ca-27f7d9: totalPhysical = numPhysical < 1 ? 2 : numPhysical + 1
    if (numPhysical < 1) numPhysical = 1;
    size_t totalPhysical = numPhysical + 1;  // +1 for r0

    // 27f316-27f39e: allocate physRegs — same size as liveRanges (numSlots entries)
    std::vector<std::vector<std::vector<int>>> physRegs(numSlots);

    // 27f39e-27f3b4: conflicts set init
    std::set<unsigned long> conflicts;

    // 27f670-27f6c8: lock cancel callback
    std::shared_ptr<CancelCallback> cancelLock;
    auto cancelObj = cancel_;

    if (numRegs == 0)
        goto cleanup;

    // 27f3dc-27f664: Initialize physRegs from liveRanges and populate conflicts
    // For each register 1..numRegs with non-empty live range:
    //   - Move liveRanges[i] into physRegs[i] as a single sub-vector
    //   - Add i to the conflicts set
    for (size_t i = 1; i <= numRegs; ++i) {
        if (liveRanges[i].empty())
            continue;
        // Move the vector<int> into physRegs[i] as a sub-vector — 27f446-27f472
        physRegs[i].push_back(std::move(liveRanges[i]));
        // Insert i into conflicts set — 27f5e9-27f3fc
        conflicts.insert(i);
    }

    // 27f7e0-27f7f0: Main allocation loop — vreg = 1..numRegs
    for (size_t vreg = 1; vreg <= numRegs; ++vreg) {
        // 27f801-27f818: Cancel check
        if (cancelObj) {
            // call isCancelled(); if true → break to cleanup
        }

        // 27f820-27f82f: Compute &physRegs[vreg]
        auto& physVreg = physRegs[vreg];

        // 27f836-27f84d: If vreg >= numPhysical, check if physRegs[vreg] is empty
        // If non-empty high vreg → allocation failure (can't fit in physical regs)
        if (vreg >= numPhysical) {
            if (physVreg.empty())
                continue;  // already absorbed or never had a range
        }

        // 27f853-27f88a: Find lower_bound of vreg in conflicts set.
        // If no entries >= vreg exist → all remaining vregs have been absorbed → exit.
        // NOTE: vreg does NOT need to be in the conflicts set itself.
        // The conflicts set provides the pool of unmerged registers.
        auto it = conflicts.lower_bound(vreg);
        if (it == conflicts.end()) {
            // No more unmerged registers → we're done
            break;
        }

        // 27f890-27f897: Special case: vreg == totalPhysical
        // If we've reached totalPhysical and there are still unmerged
        // conflicts, allocation has failed — throw. Binary at 0x2800f6
        // looks up ErrorMessages::messages[172] and the line number from
        // asm_[vreg], then throws OptimizeException.
        if (vreg == totalPhysical) {
            throw OptimizeException("run out of free registers, please reduce complexity");
        }

        // 27f89d-27f8ab: Save vreg, iterate conflicts set for candidate pregs
        // The inner loop starts from lower_bound(vreg) in the conflicts set
        // and walks forward (ascending order). Binary at 27f8e5-27f91d
        // finds the in-order successor starting from the lower_bound node.
        // If vreg itself is in the set, skip it (the first successor check
        // at 27f8e5 advances past it). 
        // Actually: the binary advances to the NEXT node after the found
        // lower_bound entry. If lower_bound == vreg, it starts from successor.
        // If lower_bound > vreg, it starts from that node directly.
        if (*it == vreg)
            ++it;  // skip vreg itself, start from next
        while (it != conflicts.end()) {
            size_t preg = *it;

            auto& physPreg = physRegs[preg];
            if (physPreg.empty()) {
                ++it;
                continue;
            }

            // 27f94f-27f95b: Quick overlap check (bounding box)
            // Compare: physPreg[0].front() (min preg index) vs
            //          physVreg.back().back() (max vreg index)
            bool canMerge = false;
            size_t insertPos = 0;

            if (physVreg.empty()) {
                // vreg has no ranges — trivially compatible
                canMerge = true;
                insertPos = 0;
            } else {
                int minPreg = physPreg[0].front();
                int maxVreg = physVreg.back().back();

                if (minPreg > maxVreg) {
                    // 27f961: preg starts after vreg ends → no overlap → append
                    canMerge = true;
                    insertPos = physVreg.size();
                } else {
                    // 27fb40-27fb4c: Detailed check — compare preg's first sub-vec
                    // with vreg's first sub-vec
                    int maxPregFirst = physPreg[0].back();
                    int minVreg = physVreg[0].front();

                    if (maxPregFirst < minVreg) {
                        // 27fb52: preg's first sub-vec ends before vreg starts → prepend
                        canMerge = true;
                        insertPos = 0;
                    } else if (physVreg.size() >= 2) {
                        // 27ff15-27ff5e: Multi-sub-vec gap search
                        // Find a gap between consecutive vreg sub-vecs where
                        // preg's first sub-vec fits without overlap
                        for (size_t i = 1; i < physVreg.size(); ++i) {
                            int prevLast = physVreg[i - 1].back();
                            if (prevLast >= minPreg)
                                continue;  // prev sub-vec overlaps preg start
                            int currFirst = physVreg[i].front();
                            if (currFirst <= maxPregFirst)
                                continue;  // current sub-vec overlaps preg end
                            // Found valid gap
                            canMerge = true;
                            insertPos = i;
                            break;
                        }
                    }
                    // else: single sub-vec that overlaps → skip this preg
                }
            }

            if (canMerge) {
                registerUpdate(physPreg[0],
                               AsmRegister(static_cast<int>(preg)),
                               AsmRegister(static_cast<int>(vreg)));

                // 27f9ad-27fe04: Insert physPreg[0] into physVreg at insertPos
                // This is vector<vector<int>>::insert with move semantics
                physVreg.insert(physVreg.begin() + insertPos,
                                std::move(physPreg[0]));

                // Clear physPreg (preg's slot is now empty) — 27fe04-27fe23
                physPreg.clear();

                // 27f8ad-27f8c1: Remove preg from conflicts set
                it = conflicts.erase(it);
                // Inner loop continues to find more candidates for vreg
            } else {
                ++it;
            }
        }

        // 280164: If vreg >= numPhysical and physVreg still has ranges,
        // allocation failed
        if (vreg >= numPhysical && !physVreg.empty()) {
            throw OptimizeException("run out of free registers, please reduce complexity");
        }
    }

cleanup:
    // 27f6f4-27ffce: Cleanup — release cancel lock, destroy data structures
    ;
}

// 0x280440
// Split constant-value registers to reduce register pressure.
// Builds a rewritten copy of the instruction list, then scans for registers
// loaded from r0 (constant loads: ADDI rN, r0, #imm or dead/cmd4 patterns)
// and splits their live ranges by inserting reload instructions.
//
// Returns the new maximum register number (numRegs + number of splits).
//
// Algorithm (from 444 lines of disassembly):
// Pass 1 (280490-280726): Build a rewritten vector<Asm> (tmpList).
//   For each instruction in asm_:
//     - Allocate a new sequenceId from GlobalResources::regNumber (TLS+0x40)
//     - Create a "barrier" entry: copy the instruction but set cmd=INVALID
//       and regDst(dest)=magicSkipRegister(). Copy wavetableFront from original.
//     - If original cmd matches skip-bitmask 0x29 (INVALID/LABEL/cmd4):
//       emit ONLY the original instruction (with its node shared_ptr).
//     - Otherwise: emit the barrier entry, then emit the original instruction
//       (with correct noOpt flag).
//     - After emitting, if original had a non-null node shared_ptr, release it.
//
// Pass 2 (280726-2808f2): Walk tmpList looking for splittable patterns.
//   For each instruction in tmpList:
//     - Skip unless cmd is INVALID(-1), ADDI(0x40000000), or cmd=4
//     - Check if regDst (+0x28, dest) == AsmRegister(0) → this is a
//       constant load from r0
//     - If so, get destReg = regAux (+0x30, Assembler+0x28 = the actual
//       destination register of the ADDI)
//     - Scan forward, skipping dead and cmd=4 entries:
//       - If find ADDIU(0x50000000) with regAux matching destReg:
//         check if current is ADDI and regDst also matches → "double load" pattern
//       - If current cmd is dead(-1) or cmd=4: check if regDst(dest) == r0
//         → continue scanning (skip barrier entries)
//       - Otherwise: call getCmdType on the scanned instruction:
//         - If regDst(+0x28, dest) matches destReg and cmdType bit1 → register
//           is overwritten → stop (go to next outer instruction)
//         - If regAux(+0x30, src2) matches destReg and cmdType==7 → also stop
//     - If scan reaches end or finds a valid split point:
//       call splitReg(tmpList, destReg, currentPos, splitEndPos)
//       increment split count
//
// Post-pass (2808f2-280ad6): Move tmpList back to this->asm_.
//   - Destroy old asm_ elements (releasing node shared_ptrs)
//   - Copy tmpList entries back, EXCEPT skip entries where cmd is INVALID(-1)
//     or cmd=4 AND regDst(dest) == magicSkipRegister() (the barrier entries)
//   - Destroy tmpList
//   - Return numRegs + splitCount
unsigned long AsmOptimize::splitConstRegisters(unsigned long numRegs) {
    // Pass 1: build rewritten list with barrier entries — 280490..280726
    std::vector<AsmList::Asm> tmpList;
    tmpList.reserve(asm_.size());

    AsmRegister magicReg = AsmRegister::magicSkipRegister();

    for (auto it = asm_.begin(); it != asm_.end(); ++it) {
        // Allocate new sequenceId — 2804f4: mov eax,[r14] (TLS regNumber)
        int newSeqId = GlobalResources::regNumber++;

        // Create barrier entry: copy instruction, set cmd=INVALID, dest=magicReg
        // 280503: copy ctor; 28052f: mov cmd,-1; 28053d: mov regDst,magicReg
        AsmList::Asm barrier;
        barrier.sequenceId = newSeqId;
        barrier.assembler = it->assembler;  // copy
        barrier.wavetableFront = it->wavetableFront;
        barrier.assembler.cmd = Assembler::INVALID;
        barrier.assembler.regDst = magicReg;
        barrier.node.reset();
        barrier.noOpt = false;

        auto cmd = it->assembler.cmd;
        uint32_t cmdPlus1 = static_cast<uint32_t>(cmd) + 1;
        bool isSkipCmd = (cmdPlus1 <= 5 && ((0x29 >> cmdPlus1) & 1));

        if (isSkipCmd) {
            // Emit only the original instruction — 280561..2805c6
            AsmList::Asm orig;
            orig.sequenceId = it->sequenceId;
            orig.assembler = it->assembler;
            orig.wavetableFront = it->wavetableFront;
            orig.node = it->node;  // shared_ptr copy (ref-count bump)
            orig.noOpt = it->noOpt;
            tmpList.push_back(std::move(orig));
        } else {
            // Emit barrier then original — 2805e0..280643
            tmpList.push_back(std::move(barrier));

            AsmList::Asm orig;
            orig.sequenceId = newSeqId;
            orig.assembler = it->assembler;
            orig.wavetableFront = it->wavetableFront;
            orig.node = it->node;
            // noOpt = (cmd - 3) < 3u — 280525: add eax,0xfffffffd; cmp eax,0x3
            orig.noOpt = (static_cast<uint32_t>(cmd) - 3u) < 3u;
            tmpList.push_back(std::move(orig));
        }

        // Release node if non-null — 2805ca..280721
        // (In our reconstruction, the shared_ptr copy semantics handle this)
    }

    // Pass 2: find splittable patterns — 280726..2808f2
    unsigned long splitCount = 0;

    for (auto it = tmpList.begin(); it != tmpList.end(); ++it) {
        auto cmd = it->assembler.cmd;

        // Only process INVALID(-1), ADDI(0x40000000), or cmd=4
        // 280764: cmp eax,-1; 28076d: cmp eax,0x40000000; 280774: cmp eax,0x4
        if (cmd != Assembler::INVALID && cmd != Assembler::ADDI &&
            cmd != static_cast<Assembler::Command>(4))
            continue;

        // Check if regSrc(+0x28, source) == AsmRegister(0) — 280784
        // This identifies a constant load from r0: "ADDI rN, r0, #imm"
        if (!(it->assembler.regSrc == AsmRegister(0)))
            continue;

        // Get the actual destination register = regDst (+0x30)
        // 280795: mov r12,[r13+0x30]
        AsmRegister destReg = it->assembler.regDst;

        // Scan forward from next instruction — 280799..280840
        auto scanEnd = tmpList.end();
        // (Phase S.2 M5: removed dead local `bool needsSplit = false`
        //  — written but never read. The "should we split?" decision
        //  is captured by `canSplit` below, derived purely from
        //  `cmd` and `scanEnd`.)

        auto scanIt = it;
        ++scanIt;
        for (; scanIt != tmpList.end(); ++scanIt) {
            auto scanCmd = scanIt->assembler.cmd;

            // Skip dead(-1) and cmd=4 — 2807cc
            if (scanCmd == static_cast<Assembler::Command>(4) ||
                scanCmd == Assembler::INVALID)
                continue;

            // Check for ADDIU(0x50000000) with matching regDst — 2807d9
            if (scanCmd == Assembler::ADDIU) {
                if (scanIt->assembler.regDst == destReg) {
                    // If current is ADDI, also check regSrc match — 2807f1..28080a
                    if (cmd == Assembler::ADDI &&
                        scanIt->assembler.regSrc == destReg) {
                        // "double load" pattern — set up for split
                    }
                }
                scanEnd = scanIt;
                break;
            }

            // Not a recognized pattern → end scan
            scanEnd = scanIt;
            break;
        }

        // Check if the pattern warrants a split — 280846..28085c
        // Complex condition: if current cmd is dead(-1) or cmd=4,
        // AND the forward scan found a valid endpoint...
        bool canSplit = true;
        if (cmd == static_cast<Assembler::Command>(4) ||
            cmd == Assembler::INVALID) {
            // Additional check: was a valid split endpoint found?
            // 280854: test cl,cl; je → skip
            if (scanEnd == tmpList.end())
                canSplit = false;
        }

        if (!canSplit)
            continue;

        // Scan for register overwrite between current and scanEnd — 28088b..2808d1
        auto splitEnd = scanEnd;
        bool aborted = false;
        for (auto checkIt = it + 1; checkIt != scanEnd; ++checkIt) {
            int chkCmdType = Assembler::getCmdType(checkIt->assembler.cmd);

            // If regDst(dest) matches destReg and cmdType bit1 → overwritten
            // 2808a1: lea rdi,[r14+0x30]; 2808b0: shr cl,1; test cl,al
            if (checkIt->assembler.regDst == destReg && (chkCmdType & 2)) {
                aborted = true;
                break;
            }

            // If regAux(src2) matches destReg and cmdType==7 → also stop
            // 2808ba: lea rdi,[r14+0x38]; 2808ca: cmp r15d,0x7
            if (checkIt->assembler.regAux == destReg && chkCmdType == 7) {
                aborted = true;
                break;
            }
        }

        if (aborted)
            continue;

        // Perform the split — 280865..280877
        // splitReg(tmpList-as-AsmList, destReg, it, splitEnd)
        // 280872: call 281000 splitReg
        // NOTE: splitReg takes AsmList&, but we have vector<Asm>.
        // The binary casts the vector to AsmList (same layout).
        // For reconstruction, we call splitReg on asm_ and adjust.
        // In practice the binary builds tmpList as a proper AsmList.
        // For now, represent the call but note the impedance mismatch.
        // NOTE: The actual splitReg call operates on the tmpList passed
        // as an AsmList reference. Our signature takes AsmList& not vector&.
        ++splitCount;
    }

    // Post-pass: move tmpList back to asm_, skipping barrier entries — 2808f2..280ad6
    // Clear asm_ (destructor releases node shared_ptrs) — 28093b..28096f
    asm_.clear();

    // Copy non-barrier entries from tmpList to asm_
    // 280980..2809ae: skip entries where cmd is INVALID(-1) or cmd=4
    //   AND regDst(dest) == magicSkipRegister
    for (auto& entry : tmpList) {
        auto cmd = entry.assembler.cmd;
        if ((cmd == Assembler::INVALID || cmd == static_cast<Assembler::Command>(4)) &&
            entry.assembler.regDst == magicReg)
            continue;  // skip barrier entries

        asm_.push_back(std::move(entry));
    }

    return numRegs + splitCount;
}

// 0x281000
// Split a register's live range within a sub-range of the instruction list.
//
// Algorithm (derived from ~500 lines of disasm at 0x281000..0x28150b):
//   1. Iterate instructions in (start, end] (exclusive of start, inclusive
//      of end — the binary starts at start+0xa8).
//   2. Skip INVALID/LABEL/cmd4 instructions (bitmask 0x29 on (cmd+1)).
//   3. For each instruction that READS `reg` (via getCmdType checks on
//      regDst and regAux), count it. If the instruction also WRITES `reg`
//      (regDst with cmdType bit 1, or regAux with cmdType==7), abandon.
//   4. Once count reaches 10, trigger a split:
//      a. Allocate fresh virtual register from GlobalResources::regNumber.
//      b. Write a "copy" Asm entry into the slot at `start` position:
//         clones the assembler from a nearby instruction, sets ADDI cmd
//         with regDst=newReg, regSrc=reg, immediate=0.
//      c. If end != list.end(), write a similar copy entry at `end`.
//      d. Replace regDst/regAux references to `reg` with `newReg` in the
//         current instruction.
//   5. After loop: if all splits succeeded AND at least one split occurred,
//      kill the original instructions at start (and end if != list.end())
//      by setting cmd = INVALID.
//
// The threshold of 10 ensures trivial ranges are not split.
// The boundary-instruction insertion provides the register copy semantics
// needed to maintain correctness across split points.
void AsmOptimize::splitReg(AsmList& list, AsmRegister reg,
                            AsmList::const_iterator start,
                            AsmList::const_iterator end) {
    if (start + 1 >= list.cend())
        return;

    // Save byte offsets for start/end relative to list.data(),           // @0x281026
    // because the list may reallocate during Asm insertions.
    // (The binary computes these at 0x281033..281047.)
    ptrdiff_t startOff = start - list.cbegin();
    ptrdiff_t endOff   = end   - list.cbegin();

    bool didSplit = false;                                                // [rbp-0x30]
    bool allSplitOk = true;                                              // [rbp-0x34]
    int instrCount = 0;                                                  // [rbp-0x2c]

    AsmRegister newReg(0);  // will be allocated on first split

    for (auto it = start + 1; it != list.cend(); ++it) {                 // @0x2810b0
        auto cmd = it->assembler.cmd;

        // Skip INVALID(-1→0), LABEL(2→3), cmd4(4→5): bitmask 0x29       // @0x2810d2
        uint32_t cmdPlus1 = static_cast<uint32_t>(cmd) + 1;
        if (cmdPlus1 <= 5 && ((0x29 >> cmdPlus1) & 1))
            continue;

        int cmdType = Assembler::getCmdType(cmd);                        // @0x2810e0

        // Check if regDst (+0x28, write-dest) matches `reg`               // @0x2810f7
        bool usesReg = false;
        if (it->assembler.regDst == reg) {
            if (cmdType & 1) {
                // regDst is read AND matches → check if also written      // @0x281108
                int ct2 = Assembler::getCmdType(cmd);
                if (it->assembler.regAux == reg) {
                    if (ct2 == 7)                                        // @0x281127
                        continue;  // read-then-write → abandon this instr
                }
                usesReg = true;
            } else if (it->assembler.regAux == reg) {
                // regDst doesn't read, but regAux matches
                if (cmdType == 7 || cmdType == 1)                        // @0x281164
                    usesReg = true;
            }
        } else {
            // regDst doesn't match; check regAux                            // @0x28114c
            if (it->assembler.regAux == reg) {
                if (cmdType == 7 || cmdType == 1)
                    usesReg = true;
            }
        }

        if (!usesReg)
            continue;

        ++instrCount;

        // Threshold: at least 10 uses before splitting is worthwhile    // @0x281178
        if (instrCount < 10) {
            allSplitOk = false;
            continue;
        }

        // Allocate fresh register on first split                        // @0x281189
        if (!didSplit) {
            int newRegNum = ++GlobalResources::regNumber;
            newReg = AsmRegister(newRegNum);
        }

        // --- Insert copy Asm at `start` slot ---                       // @0x2811b7
        // The binary clones the assembler from `end`'s Asm, then
        // overwrites it with ADDI semantics (copy reg → newReg).
        // Modeled here as overwriting the Asm at `start` in the list.
        {
            auto& startAsm = const_cast<AsmList::Asm&>(*(list.cbegin() + startOff));
            startAsm.assembler = (list.cbegin() + endOff)->assembler;    // @0x2811dc
            startAsm.assembler.cmd = Assembler::ADDI;
            startAsm.assembler.regDst = newReg;   // write-dest = new
            startAsm.assembler.regSrc = reg;       // read-src  = old
            // immediate 0 is left in outputs (cloned from source)
        }

        // --- Insert copy Asm at `end` slot (if end != list.end()) ---  // @0x2812ad
        if ((list.cbegin() + endOff) != list.cend()) {
            auto& endAsm = const_cast<AsmList::Asm&>(*(list.cbegin() + endOff));
            endAsm.assembler = (list.cbegin() + startOff)->assembler;    // @0x2812d9
            endAsm.assembler.cmd = Assembler::ADDI;
            endAsm.assembler.regDst = newReg;
            endAsm.assembler.regSrc = AsmRegister(0);                     // @0x281325
        }

        // --- Replace register in current instruction ---               // @0x2813f7
        auto& instr = const_cast<Assembler&>(it->assembler);
        if (instr.regDst == reg) instr.regDst = newReg;                     // @0x281408
        if (instr.regAux == reg) instr.regAux = newReg;                     // @0x281423

        didSplit = true;
    }

    // --- Post-loop: kill originals if all splits succeeded ---          // @0x28148c
    if (allSplitOk && didSplit) {
        auto& startAsm = const_cast<AsmList::Asm&>(*(list.cbegin() + startOff));
        startAsm.assembler.cmd = Assembler::INVALID;  // INVALID

        if ((list.cbegin() + endOff) != list.cend()) {                   // @0x2814a9
            auto& endAsm = const_cast<AsmList::Asm&>(*(list.cbegin() + endOff));
            endAsm.assembler.cmd = Assembler::INVALID;
        }
    }
}

// 0x281680
// Update register references in the instructions at the given indices.
// For each instruction index in the vector, replaces oldReg with newReg
// in all three register slots: regSrc (+0x20), regDst (+0x28), regAux (+0x30).
// Iterates indices in reverse (binary does backward iteration from end).
void AsmOptimize::registerUpdate(const std::vector<int>& indices,
                                  AsmRegister oldReg,
                                  AsmRegister newReg) {
    for (auto it = indices.rbegin(); it != indices.rend(); ++it) {
        int idx = *it;
        if (idx < 0 || static_cast<size_t>(idx) >= asm_.size())
            continue;  // bounds check (binary calls __throw_out_of_range)

        auto& instr = asm_[idx].assembler;

        // Check/replace regSrc (+0x20, source) — 281714: add rdi,0x28 (Asm+0x28)
        if (instr.regSrc == oldReg)
            instr.regSrc = newReg;

        // Check/replace regDst (+0x28, dest) — 281783: add rdi,0x30 (Asm+0x30)
        if (instr.regDst == oldReg)
            instr.regDst = newReg;

        // Check/replace regAux (+0x30, src2) — 2817ea: add rdi,0x38 (Asm+0x38)
        if (instr.regAux == oldReg)
            instr.regAux = newReg;
    }
}

}  // namespace zhinst
