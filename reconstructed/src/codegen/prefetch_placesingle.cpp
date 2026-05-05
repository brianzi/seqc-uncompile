// ============================================================================
// Reconstructed from disassembly of _seqc_compiler.so
// Phase 7e: Prefetch::placeSingleCommand — the largest single method
//
// Methods reconstructed:
//   placeSingleCommand   0x1d7940 – 0x1dbb03 (with exception handlers to 0x1dd1a0)
//
// This is the main dispatch function that generates assembly instructions
// for a single node based on its nodeType2 (+0x44) field. It dispatches
// to different code paths for Load(1), Play(2), Branch(4), Loop(8),
// Sync(0x100/0x200), CWVF-store(0x8000), and Sync-Hirzel(0x2000) nodes.
//
// The cervino non-split path (0x1d91b8..0x1dba0d) is the most complex,
// involving nested isHirzel checks, ssl loops, smap/addr/wwvf/wprf/prf
// instruction emission, and indexed play with minIndexedSize threshold.
//
// CASE LABEL ATTRIBUTION (Phase 10.8b investigation):
// The switch dispatches via jump table at 0x95af74 (indexed by nodeType-1,
// range 0..7). Verified targets:
//   table[0] (type=1 Load)  → 0x1d79f8 — body extends to 0x1d7a4b, then
//                              jmps to 0x1d8281 for the name-lookup tail
//                              (Cache lookup, register alloc, addi emission)
//   table[1] (type=2 Play)  → 0x1d7d49 — body covers the cervino emission,
//                              cwvf, ssl loops, prf, etc. Reads loadRef via
//                              `__shared_weak_count::lock()` on Node+0x18/+0x20.
//   table[3] (type=4 Loop)  → 0x1d7e26 (case 4 in source)
//   table[7] (type=8 Branch)→ 0x1d7cc3 (case 8 in source)
// table[2,4,5,6] all jump to 0x1dbaf2 (default exit).
//
// The source's monolithic `case 1:` body merges Load (0x1d79f8..0x1d7a4b +
// 0x1d8281+) AND Play (0x1d7d49..0x1d8281-region) code under a single label.
// A future refactor should split this into proper `case NodeType::Load:` and
// `case NodeType::Play:` blocks. The Phase 10.8b session resolved the
// individual marker comments in-place rather than restructuring; the code is
// semantically correct but the case label is misleading.
// ============================================================================

#include "zhinst/codegen/prefetch.hpp"
#include "zhinst/asm/asm_commands.hpp"
#include "zhinst/codegen/awg_compiler_config.hpp"
#include "zhinst/device/device_constants.hpp"
#include "zhinst/asm/asm_list.hpp"
#include "zhinst/asm/asm_commands.hpp"
#include "zhinst/ast/node.hpp"
#include "zhinst/waveform/waveform.hpp"
#include "zhinst/waveform/waveform_ir.hpp"
#include "zhinst/waveform/wavetable_ir.hpp"
#include "zhinst/runtime/cache.hpp"
#include "zhinst/runtime/resources.hpp"
#include "zhinst/waveform/play_config.hpp"

namespace zhinst {

// Helper: emit the common "cervino non-split" instruction sequence.
// This appears three times in the disassembly (for Hirzel-indexed at 0x1dabb9,
// Cervino non-Hirzel-indexed at 0x1daed4, and non-indexed paths).
// The pattern is:
//   1. addi tempReg, node->indexOffsetReg, Immediate(0)  — load base register
//   2. ssl loop: for each page, emit ssl(tempReg) then increment
//   3. addr(stateReg, tempReg2)
//   4. optionally wwvf()
//   5. prf(stateReg, stateCervino, clampToCache(cacheSize))
//   6. wvfImpl or wvfRegImpl
// These are inlined below rather than factored, to match the binary layout.

// 0x1d7940
void Prefetch::placeSingleCommand(AsmList* out, std::shared_ptr<Node> node) {
    // Find placeholder entry in the AsmList for this node. Returns an
    // AsmList::Asm* iterator into the underlying vector (refactored
    // Phase 10.8d; see prefetch.hpp:213).
    AsmList::Asm* placeholder = findPlaceholder(out, node);        // 0x1d7987

    // Copy the placeholder's wavetable-front context to the assembler.
    // 0x1d79bf-0x1d79c7:
    //   mov r14, [r15+0x70]    ; r14 = asmCommands_.get()
    //   mov eax, [r12+0x88]    ; eax = placeholderAsm->wavetableFront
    //   mov [r14+0x50], eax    ; asmCommands_->wavetableFrontIndex_ = eax
    asmCommands_->setWavetableFrontIndex(placeholder->wavetableFront);

    Node* np = node.get();
    int nodeType = static_cast<int>(np->type);                     // 0x1d79d2, offset +0x44



    // Convert the placeholder Asm* into a vector iterator usable by
    // AsmList::insert. Recomputed on each use because the underlying
    // vector may reallocate between insertions; placeholder is updated
    // by re-finding via index when needed.
    const auto placeholderIndex = static_cast<size_t>(
        placeholder - &(*out->begin()));
    auto placeholderIter = [&]() {
        return out->begin() + placeholderIndex;
    };

    // ========================================================================
    // Main dispatch on nodeType2
    // ========================================================================

    // --- Switch table for values 1..8 (jump table at 0x95af74) ---
    if (nodeType <= 0x1ff) {
        if (nodeType >= 1 && nodeType <= 8) {
            switch (nodeType) {
            // ================================================================
            // Case 2 (Play): 0x1d7d49 — enters Play path directly
            // ================================================================
            case 2:
                goto play_entry;

            // ================================================================
            // Case 1 (Load): 0x1d79f8 — Load-specific setup then falls into Play
            // ================================================================
            case 1: {                                              // 0x1d79f8

                {
                    int devIdx = (int)np->deviceIndex;             // +0x40
                    if (devIdx < 0)
                        goto load_fallback;  // 0x1d7fc0

                    if (!np->wavesPerDev[devIdx].has_value())
                        goto load_fallback;  // 0x1d7fc0

                    auto& wavesPerDev = np->wavesPerDev;           // +0x28
                    std::string waveName = wavesPerDev[devIdx].value(); // 0x1d7a26..0x1d8281

                    // Look up or create nodeStates_ entry for this node
                    auto& state = nodeStates_[node];               // 0x1d82a5

                    auto cachePtr = state.cachePtr;                // offset +0x48
                    if (!cachePtr) {
                        goto load_fallback;  // 0x1d82da → 0x1d7fc4
                    }

                    auto& loadState = nodeStates_[node];
                    if (loadState.cachePtr->size_ == 0) {           // 0x1d8308
                        break;  // 0x1d830c → default_exit
                    }
                }

                // ============================================================
                // Load case-1: own instruction generation path (0x1d8312-0x1d86cb)
                // This does NOT fall through to play_entry. The Load node
                // generates addi + prf independently of the Play node.
                // ============================================================
                {
                    AsmList tempList;                               // 0x1d8312

                    // Step 1: Ensure registerHirzel is allocated    // 0x1d8348-0x1d839d
                    auto& stateH = nodeStates_[node];
                    if (!stateH.registerHirzel.isValid()) {        // 0x1d8354
                        int regNum = resources_->getRegisterNumber(); // 0x1d8361
                        AsmRegister newReg(regNum);
                        nodeStates_[node].registerHirzel = newReg; // 0x1d8399
                    }

                    // Step 2: Load registerHirzel and emit addi    // 0x1d83a4-0x1d8467
                    AsmRegister regHirzel = nodeStates_[node].registerHirzel; // 0x1d83cc
                    AsmRegister zeroReg(0);                         // 0x1d83d9

                    auto waveOpt = np->waveAtCurrentDeviceIndex(); // 0x1d83f3
                    auto wfIR = wavetableIR_->getWaveformByName(waveOpt); // 0x1d8406
                    uint32_t addrValue = wfIR->addressValue;       // 0x1d840f: +0x4c

                    {
                        auto addiVec = asmCommands_->addi(regHirzel, zeroReg,
                                           Immediate(addrValue));  // 0x1d8435
                        for (auto& a : addiVec) tempList.append(a);
                    }

                    // Step 3: Check isHirzel for cervino next-node path // 0x1d84bf
                    if (!config_->isHirzel) {                      // 0x1d84c3
                        Node* npNode = node.get();

                        if (npNode->next != nullptr) {             // 0x1d84c9: +0xB8
                            // Cervino: emit addi for this load node's registerCervino
                            auto& loadState = nodeStates_[node];   // 0x1d84fb
                            AsmRegister regCervino = loadState.registerCervino; // 0x1d8500: +0x28

                            AsmRegister zeroReg2(0);               // 0x1d850d

                            // Look up cachePtr->position_ for the load node
                            auto cachePtrL = nodeStates_[node].cachePtr; // 0x1d8531: +0x48
                            uint32_t cachePos = cachePtrL->position_; // 0x1d8535: +0x0

                            auto addiVec2 = asmCommands_->addi(regCervino, zeroReg2,
                                                Immediate(cachePos)); // 0x1d855d
                            for (auto& a : addiVec2) tempList.append(a);
                        }
                    }

                    // Step 4: Check lengthReg (+0x88, NOT indexOffsetReg per IF-102)
                    // Only take load_indexed_play when split_=false (large waveform, stream path).
                    // When split_=true (small waveform fits in cache), the load uses standard prf.
                    // IF-143: binary gates this on split_=false (0x1d1a84 checks 0xbc(%r14)).
                    if (!split_) {
                        Node* npIdx = node.get();
                        if (npIdx->lengthReg.isValid()) {              // 0x1d85bb
                            AsmRegister zrCheck(0);
                            if (!(npIdx->lengthReg == zrCheck)) {
                                // Indexed-Load path → 0x1da77f
                                goto load_indexed_play;
                            }
                        }
                    }

                    // Step 5: Hirzel prf emission                  // 0x1d85f6-0x1d86cb
                    if (config_->isHirzel) {                       // 0x1d85f6
                        auto& stateDA = nodeStates_[node];
                        if (stateDA.useDA) {                       // 0x1d8625: +0x58 in hash_node = +0x38 in PNS
                            AsmRegister regH2 = nodeStates_[node].registerHirzel; // 0x1d8652
                            AsmRegister zr2(0);                    // 0x1d865f

                            auto cachePtrH = nodeStates_[node].cachePtr; // 0x1d8684
                            int cacheSize = cachePtrH->size_;      // 0x1d8688: +0x04

                            int clampedSize = clampToCache(cacheSize); // 0x1d868e
                            AsmList::Asm prfAsm = asmCommands_->prf(regH2, zr2, clampedSize); // 0x1d86aa
                            tempList.append(prfAsm);
                        }
                    } else {
                        // Non-Hirzel (Cervino): jump to 0x1d9c33 — different prf path
                        // TODO: implement cervino Load prf path
                        goto load_cervino_prf;
                    }

                load_finalize:                                     // 0x1db92e (shared finalize)
                    {
                        // 0x1db931: if (!config_->isHirzel) emit wprf
                        if (!config_->isHirzel) {                  // 0x1db931
                            AsmList::Asm wprfAsm = asmCommands_->wprf(); // 0x1db942
                            tempList.append(wprfAsm);
                        }
                        // Insert tempList into output at placeholder
                        out->insert(placeholderIter(), tempList.begin(), tempList.end());
                        break;
                    }

                load_indexed_play:
                    {
                        // Indexed-Load path (binary 0x1da77f)
                        //
                        // Reached when the Load node has lengthReg != 0.  The Load
                        // emits an extended sequence in place of the simple
                        // load_cervino_prf: it includes an SSL loop over the
                        // waveform's channel-page count, an addr() to fold the
                        // shifted index into the Hirzel base register, an
                        // optional wwvf, and a half-size prf.  GDB-traced via
                        // the uhf_doc_tv_mode case (see IF-105 in
                        // reconstructed/notes/incidental_findings.md).

                        auto cachePtrI = nodeStates_[node].cachePtr;
                        uint32_t numRepeatsI = cachePtrI->numRepeats_;     // +0x0C

                        // 0x1da7a8: if numRepeats >= 2, emit a leading wwvf
                        if (numRepeatsI >= 2) {                            // 0x1da7ac
                            AsmList::Asm wwvfAsm = asmCommands_->wwvf();   // 0x1da7b9
                            tempList.append(wwvfAsm);
                        }

                        // 0x1da7da: allocate a temp register for the indexed
                        // offset computation
                        AsmRegister tempIdxReg(resources_->getRegisterNumber());

                        // 0x1da823: addi(tempIdxReg, lengthReg, Imm(0))
                        // — copy lengthReg value into the temp register
                        Node* npI = node.get();
                        {
                            auto addiVec = asmCommands_->addi(tempIdxReg, npI->lengthReg,
                                               Immediate(0));
                            for (auto& a : addiVec) tempList.append(a);
                        }

                        // 0x1da876..0x1daa42: SSL loop over wave's channel pages.
                        // Each iteration emits one ssl(tempIdxReg).
                        // The loop count is the waveform's signal.channels_
                        // (16-bit field at WaveformIR +0xc8).
                        for (int16_t i = 0; ; i++) {
                            auto waveOptI = npI->waveAtCurrentDeviceIndex();
                            auto wfI = wavetableIR_->getWaveformByName(waveOptI);
                            uint16_t numCh = wfI->signal.channels_;
                            if ((int16_t)i >= (int)numCh)
                                break;
                            AsmList::Asm sslAsm = asmCommands_->ssl(tempIdxReg);  // 0x1da990
                            tempList.append(sslAsm);
                        }

                        // 0x1daa86: addr(stRegHirzel, tempIdxReg)
                        // — fold the shifted index into the Hirzel base reg
                        AsmRegister stRegHI = nodeStates_[node].registerHirzel;  // +0x20
                        {
                            AsmList::Asm addrAsm = asmCommands_->addr(stRegHI, tempIdxReg);
                            tempList.append(addrAsm);
                        }

                        // 0x1daad2: cacheSize >= minIndexedSize?
                        uint32_t cacheSizeI = cachePtrI->size_;             // +0x04
                        if (cacheSizeI >= minIndexedSize) {                 // 0x1daad8
                            // Alternative path 0x1db562: more elaborate emission
                            // (multiple addi, wprf, prf) — not yet implemented.
                            // Fall through to the simpler path for now.  See
                            // IF-105 for the full sequence.
                        }

                        // 0x1daae9: wwvf()
                        {
                            AsmList::Asm wwvfAsm = asmCommands_->wwvf();
                            tempList.append(wwvfAsm);
                        }

                        // 0x1dab9f: prf(stRegH, stRegC, clampToCache(cacheSize))
                        // Binary at 0x1dab7d-0x1dab87 reads cachePtr->size_ directly
                        // (NOT cacheSize/2) and passes to clampToCache.
                        AsmRegister stRegCI = nodeStates_[node].registerCervino;  // +0x28
                        {
                            uint32_t clamped = clampToCache(cacheSizeI);
                            AsmList::Asm prfAsm = asmCommands_->prf(stRegHI, stRegCI,
                                                                    (int)clamped);
                            tempList.append(prfAsm);
                        }

                        // Falls through to load_finalize, which emits wprf (when
                        // !isHirzel) and inserts tempList at the placeholder.
                        goto load_finalize;
                    }

                load_cervino_prf:                              // 0x1d9c33
                    {
                        auto cachePtrC = nodeStates_[node].cachePtr; // 0x1d9c33: +0x48
                        int numRepeats = cachePtrC->numRepeats_;     // 0x1d9c5a: +0x0C

                        if (numRepeats >= 2) {
                            // Path A (0x1d9c60): numRepeats >= 2
                            // Use Load node's own registers, half cache size
                            AsmRegister regH = nodeStates_[node].registerHirzel;   // +0x20
                            AsmRegister regC = nodeStates_[node].registerCervino;  // +0x28
                            int halfSize = cachePtrC->size_ / 2;                   // +0x04
                            AsmList::Asm prfAsm = asmCommands_->prf(regH, regC, halfSize);
                            tempList.append(prfAsm);
                        } else {
                            // Path B (0x1db1f3): numRepeats < 2
                            int cacheSize = cachePtrC->size_;                      // +0x04
                            int wfMemSize = devConst_->waveformMemorySize;         // +0x0C

                            if (cacheSize == wfMemSize) {
                                // Path B1 (0x1db22c): equal — uses Load node state, size/2, + extra
                                // TODO: implement B1 fully (complex path with extra reg alloc)
                                AsmRegister regH = nodeStates_[node].registerHirzel;
                                AsmRegister regC = nodeStates_[node].registerCervino;
                                int halfSize = cacheSize / 2;
                                AsmList::Asm prfAsm = asmCommands_->prf(regH, regC, halfSize);
                                tempList.append(prfAsm);
                            } else {
                                // Path B2 (0x1db876): not-equal — uses Load node's own state, full size
                                AsmRegister regH = nodeStates_[node].registerHirzel;   // 0x1db8a0: +0x20
                                AsmRegister regC = nodeStates_[node].registerCervino;  // 0x1db8c5: +0x28
                                AsmList::Asm prfAsm = asmCommands_->prf(regH, regC, cacheSize); // 0x1db8fc
                                tempList.append(prfAsm);
                            }
                        }
                        goto load_finalize;
                    }
                }

                // =============================================================
                // Load fallback path: 0x1d7fc0
                // Reached when Load node has no wave (devIdx<0 or no has_value)
                // or has wave but no cache (cachePtr==null).
                // Re-checks the node's wave; if still no wave, checks
                // globalCwvfValid_ and emits cwvf from lastCwvfNode_->config.
                // If wave IS found on re-check (cache-less node), enters
                // register allocation + cwvf path at 0x1d8780.
                // =============================================================
                load_fallback:
                    {
                    // Re-read node's deviceIndex and wave (0x1d7fc0)
                    Node* npFb = node.get();
                    int devIdxFb = (int)npFb->deviceIndex;
                    bool hasWaveFb = false;
                    if (devIdxFb >= 0 && npFb->wavesPerDev[devIdxFb].has_value()) {
                        hasWaveFb = true;
                    }

                    if (hasWaveFb) {
                        // Load node has wave but no cache: 0x1d8780 path
                        // Allocate registerHirzel if needed
                        auto& stFb = nodeStates_[node];
                        if (!stFb.registerHirzel.isValid()) {
                            int regNum = resources_->getRegisterNumber();
                            AsmRegister newReg(regNum);
                            nodeStates_[node].registerHirzel = newReg;
                        }
                        AsmRegister regHFb = nodeStates_[node].registerHirzel;
                        AsmRegister zrFb(0);

                        // Wave lookup + cwvf emission (same pattern as below)
                        auto waveOptFb = npFb->waveAtCurrentDeviceIndex();
                        auto wfIRFb = wavetableIR_->getWaveformByName(waveOptFb);
                        uint32_t addrFb = wfIRFb->addressValue;

                        // TODO: this path also emits addi + cwvf
                        // For now emit addi only (cwvf handled below in no-wave path)
                        AsmList tempFb;
                        auto addiVecFb = asmCommands_->addi(regHFb, zrFb, Immediate(addrFb));
                        for (auto& a : addiVecFb) tempFb.append(a);
                        out->insert(placeholderIter(), tempFb.begin(), tempFb.end());
                        break;
                    }

                    // No wave at all: 0x1d8013
                    // Check globalCwvfValid_ — if true, emit cwvf from lastCwvfNode_
                    if (!globalCwvfValid_)
                        break;  // default_exit

                    // Emit cwvf: copy lastCwvfNode_->config into cwvfConfig_,
                    // push to usageEntries_, encode and emit               // 0x1d8021
                    AsmList tempCwvf;

                    Node* lastCwvf = lastCwvfNode_.get();
                    if (lastCwvf) {
                        cwvfConfig_ = lastCwvf->config;                    // 0x1d803e-0x1d804e
                    }
                    usageEntries_.push_back(cwvfConfig_);                  // 0x1d8060

                    int cwvfEncoded = PlayConfig::encodeCwvf(cwvfConfig_,
                        lastCwvf ? lastCwvf->globalRate : 0);              // 0x1d8075

                    if (cwvfEncoded >= 0x1000000) {
                        AsmRegister cwvfReg(resources_->getRegisterNumber());
                        auto addiVec = asmCommands_->addi(cwvfReg, AsmRegister(0),
                                           Immediate(cwvfEncoded));
                        for (auto& a : addiVec) tempCwvf.append(a);
                        AsmList::Asm cwvfrAsm = asmCommands_->cwvfr(cwvfReg);
                        tempCwvf.append(cwvfrAsm);
                    } else {
                        AsmList::Asm cwvfAsm = asmCommands_->cwvf(cwvfEncoded);
                        tempCwvf.append(cwvfAsm);
                    }

                    // Insert cwvf instructions at placeholder position
                    out->insert(placeholderIter(), tempCwvf.begin(), tempCwvf.end());
                    break;
                }
                break;  // End of case 1

            play_entry:                                            // 0x1d7d49
                {
                    AsmList tempList;
                    int totalSize = 0;
                    // Check globalCwvfValid_
                    if (!globalCwvfValid_) {                        // 0x1d7dc1
                        bool needsCwvf = needsNewCwvf(node);       // 0x1d7df9
                        if (needsCwvf) {
                            // Emit cwvf configuration              // 0x1d8c3b
                            Node* npCwvf = node.get();
                            PlayConfig* pc = &npCwvf->config;
                            usageEntries_.push_back(*pc);

                            int cwvfEncoded = PlayConfig::encodeCwvf(*pc, npCwvf->globalRate);  // +0x100, passed as defaultRate

                            if (cwvfEncoded >= 0x1000000) {        // 0x1d8e7c
                                AsmRegister cwvfReg(resources_->getRegisterNumber());
                                auto addiVec = asmCommands_->addi(cwvfReg, AsmRegister(0),
                                                   Immediate(cwvfEncoded));
                                for (auto& a : addiVec) tempList.append(a);
                                AsmList::Asm cwvfrAsm = asmCommands_->cwvfr(cwvfReg); // 0x1d8f30
                                tempList.append(cwvfrAsm);
                            } else {
                                AsmList::Asm cwvfAsm = asmCommands_->cwvf(cwvfEncoded); // 0x1d8f55
                                tempList.append(cwvfAsm);
                            }
                        }
                    }

                    // play_emit_cwvf_then_play:                    // 0x1d8f76
                    // Check loadNode has cache with non-zero size.
                    // The loadNode is obtained at 0x1d7d49..0x1d7d6f by locking
                    // node->loadRef (Node+0x18/+0x20 weak_ptr) — the same back-
                    // reference pattern used in case NodeType::Sync (line 730).
                    // The address ranges 0x1d7d49..0x1d8f76 belong to the Play
                    // case (NodeType=2) per jump table at 0x95af74; see file
                    // header note about the case-1/case-2 merger.
                    std::shared_ptr<Node> loadNode = np->loadRef.lock();
                    if (loadNode.get() != nullptr) {
                        auto& loadSt = nodeStates_[loadNode];
                        if (loadSt.cachePtr != nullptr && loadSt.cachePtr->size_ != 0) {
                            if (!config_->isHirzel) {              // 0x1d8fed
                                auto& loadSt2 = nodeStates_[loadNode];
                                if (loadSt2.pagesNeeded >= 2) {       // 0x1d901f
                                    // splitPlay path
                                    AsmList splitResult = splitPlay(node);  // 0x1d9056
                                    tempList.insert(tempList.end(),
                                        splitResult.begin(), splitResult.end());
                                    goto play_finalize;
                                }
                                // Cervino non-split path
                                goto play_cervino_nonsplit;
                            }
                            // Hirzel: go to cervino_nonsplit path (which
                            // handles both Hirzel and Cervino despite its name).
                            // Binary: 0x1d8ff1 jne 0x1d91b8 (isHirzel → hirzel play path)
                            goto play_cervino_nonsplit;
                        }
                    }

                    // ---- Common play: check isDummy (+0x66) ----
                    {
                        Node* npD = node.get();
                        if (!npD->config.dummy)                     // 0x1d90b9, +0x66 = config+0x1E
                            goto play_finalize;

                        // ---- Hirzel dummy shortcut (0x1d90ca) ----
                        // For type==2 (Play) nodes on Hirzel devices with default rate,
                        // emit wvfs directly without going through wvfImpl.
                        if (nodeType == 2) {                        // 0x1d90ca: cmpl $0x2, 0x44(%rdi)
                            auto* dc = devConst_;
                            int rate = npD->config.rate;            // +0x4C = config+0x04
                            bool rateOk = (rate == 0) ||
                                          (rate == -1 && npD->globalRate <= 0);
                            if (rateOk && dc->hasPrecomp) {         // 0x1d90f0: hasPrecomp = isHirzel indicator
                                // Compute byte count for wvfs       // 0x1d90f9..0x1d9122
                                Assembler::PlayDummyType dummyType =
                                    static_cast<Assembler::PlayDummyType>(npD->config.hold); // +0x65
                                int len = npD->length;              // +0x90
                                if (len != 0) {
                                    int grainSize = dc->grainSize;
                                    int paddedLen = ((len + grainSize - 1) / grainSize) * grainSize;
                                    int maxLen = dc->maxWaveformLength;
                                    if ((unsigned)maxLen > (unsigned)paddedLen)
                                        len = maxLen;
                                    else
                                        len = paddedLen;
                                }
                                // Byte count computation            // 0x1db998
                                int bitsPerSample = dc->bitsPerSample;
                                long byteLen = (long)len * bitsPerSample;
                                int bitRemainder = (int)(byteLen & 0x7);
                                int byteCount = (int)(byteLen >> 3);
                                if (bitRemainder >= 1) byteCount++;
                                byteCount /= 2;                     // 0x1db9b9: shr $1,%r8d

                                // reg from node->indexOffsetReg     // 0x1db9b2
                                AsmRegister reg = npD->indexOffsetReg;

                                AsmList wvfResult = wvfs(dummyType, reg, byteCount); // 0x1db9c9
                                tempList.insert(tempList.end(), wvfResult.begin(), wvfResult.end());
                                goto play_finalize;
                            }
                        }

                        // Check asmRegister validity for wvfRegImpl
                        if (npD->indexOffsetReg.isValid()) {       // 0x1d912e, +0x94
                            AsmRegister zeroReg(0);
                            if (!(npD->indexOffsetReg == zeroReg)) {
                                // wvfRegImpl path                  // 0x1d95f7
                                Node* npW = node.get();
                                AsmRegister nodeReg = npW->indexOffsetReg;  // +0x94
                                int length = npW->length;          // +0x90
                                bool is4Ch = npW->config.now;      // +0x64 = config+0x1C
                                AsmList wvfResult = wvfRegImpl(AsmRegister(0), nodeReg, is4Ch);
                                tempList.insert(tempList.end(), wvfResult.begin(), wvfResult.end());
                                goto play_finalize;
                            }
                        }

                        // Compute waveform length from node data   // 0x1d958a..0x1d95ba
                        {
                            Node* npWvf = node.get();
                            auto* dc = devConst_;
                            int len2 = 0;

                            // Compute effective length: check dc->maxWaveformLength,
                            // dc->grainSize, pad to grain boundary
                            // See 0x1d9162..0x1d91b3 for the length computation
                            if (npWvf->length != 0) {  // +0x90
                                int grainSize = dc->grainSize;     // +0x44
                                len2 = npWvf->length;  // +0x90
                                int paddedLen = ((len2 + grainSize - 1) / grainSize) * grainSize;
                                int maxLen = dc->maxWaveformLength; // +0x40
                                if ((unsigned)maxLen > (unsigned)paddedLen)
                                    len2 = maxLen;
                                else
                                    len2 = paddedLen;
                            }

                            // Compute byte size                    // 0x1d958c
                            int bitsPerSample = dc->bitsPerSample; // +0x50
                            long byteLen = (long)len2 * bitsPerSample;
                            int bitRemainder = (int)(byteLen & 0x7);
                            int byteCount = (int)(byteLen >> 3);
                            if (bitRemainder >= 1) byteCount++;

                            bool is4Ch = npWvf->config.now;        // +0x64 = config+0x1C

                            AsmList wvfResult = wvfImpl(AsmRegister(0), byteCount, is4Ch); // 0x1d95ba
                            tempList.insert(tempList.end(), wvfResult.begin(), wvfResult.end());
                        }
                    }

                    goto play_finalize;

                play_cervino_nonsplit:                              // 0x1d91b8
                    {
                        // Check node->lengthReg: if valid and non-zero, goto indexed path
                        // Binary 0x1d91d4: `add $0x88, %rdi` → field offset +0x88
                        // = lengthReg (NOT indexOffsetReg @ +0x94 as previously
                        // documented). See IF-102.
                        Node* npCerv = node.get();
                        if (npCerv->lengthReg.isValid()) {         // 0x1d91dc, +0x88
                            AsmRegister zeroReg(0);
                            if (!(npCerv->lengthReg == zeroReg)) {
                                goto play_cervino_indexed;          // 0x1d920e → 0x1dabb9
                            }
                        }

                        // Get waveform size per device
                        auto waveOpt = npCerv->waveAtCurrentDeviceIndex(); // 0x1d9225
                        auto wfIR = wavetableIR_->getWaveformByName(waveOpt); // 0x1d9238
                        int sizePerDev = wfIR->getSizePerDevice(); // 0x1d9241

                        int pagesNeeded = 1;
                        if (!config_->isHirzel) {                  // 0x1d9250
                            pagesNeeded = nodeStates_.at(node).pagesNeeded; // 0x1d925c, +0x1c
                        }
                        int pageCount = sizePerDev / pagesNeeded;  // 0x1d9267

                        // ----- Shared Hirzel/Cervino path -----
                        // Binary: isHirzel branch at 0x1d92a1 selects the register:
                        //   Hirzel:  0x1d92cf adds +0x20 (registerHirzel), jmp 0x1d9d30
                        //   Cervino: 0x1d9d2c adds +0x28 (registerCervino), falls through to 0x1d9d30
                        // From 0x1d9d30 onward, the logic is shared.
                        AsmRegister stateReg = config_->isHirzel                 // 0x1d92a1
                            ? nodeStates_[node].registerHirzel                   // 0x1d92cf: +0x20
                            : nodeStates_[node].registerCervino;                 // 0x1d9d2c: +0x28

                        // Shared logic at 0x1d9d30: check deviceType
                        // r15 = config_ here; 0x8(%r15) points to secondary config
                        if (config_->deviceType == HDAWG) {                    // 0x1d9d3d
                            // HDAWG: jump to single wvfImpl at 0x1d9ee2
                            bool is4Ch = npCerv->config.now;                   // 0x1d9ee5: +0x64
                            AsmList wvfResult = wvfImpl(stateReg, pageCount, is4Ch);
                            tempList.insert(tempList.end(), wvfResult.begin(), wvfResult.end());
                        } else {
                            // Non-HDAWG: check pageCount vs config field at +0xC // 0x1d9d4e
                            // If mismatch: single wvfImpl at 0x1d9ee2
                            // If match: double wvfImpl with addi between them  // 0x1d9d65
                            int configPageField = nodeStates_[node].pagesNeeded;  // 0x1d9d5c: 0xc(%rax)
                            if (pageCount != configPageField) {                // 0x1d9d5f: jne 0x1d9ee2
                                bool is4Ch = npCerv->config.now;
                                AsmList wvfResult = wvfImpl(stateReg, pageCount, is4Ch);
                                tempList.insert(tempList.end(), wvfResult.begin(), wvfResult.end());
                            } else {
                                // pageCount matches: first wvfImpl, then addi+second wvfImpl
                                // 0x1d9d65..0x1d9ea2
                                {
                                    int halfPage = pageCount / 2;              // 0x1d9d6b..0x1d9d72: sar $1
                                    bool is4Ch = npCerv->config.now;           // 0x1d9d78: +0x64
                                    AsmList wvfResult = wvfImpl(stateReg, halfPage, is4Ch); // 0x1d9d8a
                                    tempList.insert(tempList.end(), wvfResult.begin(), wvfResult.end());
                                }

                                AsmRegister tempReg(resources_->getRegisterNumber());  // 0x1d9dd5
                                {
                                    auto addiVec = asmCommands_->addi(tempReg, stateReg,
                                                       Immediate(pageCount));          // 0x1d9e24
                                    for (auto& a : addiVec) tempList.append(a);
                                }
                                {
                                    bool is4Ch = node.get()->config.now;               // 0x1d9e90: +0x64
                                    AsmList wvfResult = wvfImpl(tempReg, pageCount, is4Ch); // 0x1d9ea2
                                    tempList.insert(tempList.end(), wvfResult.begin(), wvfResult.end());
                                }
                            }
                        }

                        // Post-wvf: check lengthReg again            // 0x1d9f3b
                        // Binary 0x1d9f3b/0x1d9f50: `mov $0x88, %edi` →
                        // checks lengthReg (+0x88), not indexOffsetReg. IF-102.
                        if (node.get()->lengthReg.isValid()) {        // 0x1d9f43, +0x88
                            int regVal = (int)node.get()->lengthReg;  // 0x1d9f58
                            if (regVal != 0 && !split_) {          // 0x1d9f5f, 0x1d9f65
                                // Indexed play: emit addi + prf + wprf + wvfImpl
                                uint32_t cacheSize = nodeStates_[node].cachePtr->size_;
                                if (cacheSize >= minIndexedSize) {     // 0x1d9f9c..0x1d9fa5
                                    AsmRegister idxReg(resources_->getRegisterNumber()); // 0x1d9fab
                                    AsmRegister stRegH = nodeStates_[node].registerHirzel; // 0x1d9ff2

                                    // addi idxReg, stRegH, Immediate(pageCount)
                                    {
                                        auto addiVec = asmCommands_->addi(idxReg, stRegH,
                                                           Immediate(pageCount)); // 0x1da01b
                                        for (auto& a : addiVec) tempList.append(a);
                                    }

                                    // If !isHirzel: addi reg3, stRegCervino, Immediate(pageCount)
                                    AsmRegister reg3(resources_->getRegisterNumber()); // 0x1da06b
                                    if (!config_->isHirzel) {  // 0x1da096
                                        AsmRegister stRegC = nodeStates_[node].registerCervino; // 0x1da0d4
                                        auto addiVec2 = asmCommands_->addi(reg3, stRegC,
                                                           Immediate(pageCount)); // 0x1da101
                                        for (auto& a : addiVec2) tempList.append(a);
                                    }

                                    // prf(idxReg, reg3, pageCount)  // 0x1da178
                                    {
                                        AsmList::Asm prfAsm = asmCommands_->prf(idxReg, reg3, pageCount);
                                        tempList.append(prfAsm);
                                    }

                                    // wprf()                        // 0x1da1a4
                                    {
                                        AsmList::Asm wprfAsm = asmCommands_->wprf();
                                        tempList.append(wprfAsm);
                                    }

                                    // wvfImpl with the appropriate register
                                    AsmRegister wvfReg = config_->isHirzel ? idxReg : reg3;
                                    // 0x1da1cc..0x1da1d7: cmovne selects between idxReg/reg3
                                    {
                                        bool is4Ch = node.get()->config.now;  // +0x64 = config+0x1C
                                        AsmList wvfResult = wvfImpl(wvfReg, pageCount, is4Ch); // 0x1da1f3
                                        tempList.insert(tempList.end(), wvfResult.begin(), wvfResult.end());
                                    }
                                }
                            }
                        }
                    }

                    goto play_finalize;

                play_cervino_indexed:                              // 0x1dabb9
                    {
                        // Cervino indexed play: node has non-zero asmRegister
                        // Branches on split_ (+0xBC on Prefetch)
                        Node* npCervIdx = node.get();

                        if (split_) {                           // 0x1dabb9..0x1dabc1
                            // Hirzel indexed path
                            int length = node.get()->length; // +0x90  // 0x1dabca
                            auto waveOptIdx = npCervIdx->waveAtCurrentDeviceIndex(); // 0x1dabe1
                            auto wfIdx = wavetableIR_->getWaveformByName(waveOptIdx);
                            uint16_t numPages = wfIdx->signal.channels_;   // +0xc8   // 0x1dabfd

                            // Allocate register                    // 0x1dac43
                            AsmRegister idxReg(resources_->getRegisterNumber());

                            // addi idxReg, node->lengthReg, Immediate(0) // 0x1dac9a
                            // Binary 0x1dac6f loads `0x88(%rax)` into %r13 →
                            // 3rd arg to addi: that's lengthReg. IF-102.
                            {
                                auto addiVec = asmCommands_->addi(idxReg, node.get()->lengthReg,
                                                   Immediate(0));
                                for (auto& a : addiVec) tempList.append(a);
                            }

                            // Compute totalSize = length * numPages * 2  // 0x1dacd2..0x1dace1
                            totalSize = length * (int)numPages * 2;  // assigns outer-scope var

                            // SSL loop                             // 0x1dad00..0x1dae02
                            for (int16_t i = 0; ; i++) {
                                auto waveOptI = node.get()->waveAtCurrentDeviceIndex();
                                auto wfI = wavetableIR_->getWaveformByName(waveOptI);
                                uint16_t np2 = wfI->signal.channels_;

                                if ((int16_t)i >= (int)np2)
                                    break;

                                AsmList::Asm sslAsm = asmCommands_->ssl(idxReg); // 0x1dae1d
                                tempList.append(sslAsm);
                            }

                            // After ssl loop: addr(idxReg, stRegCervino) + wvf
                            // Binary 0x1dae86: addr(idxReg, stateRegC) — fold cervino base
                            // into the ssl'd offset register.
                            // Binary 0x1daecb: wvf(idxReg, R0, totalSize) — play slice.
                            // NOTE: no wwvf here — the compiler appends wwvf+nop+end globally.
                            {
                                AsmRegister stRegC = nodeStates_[node].registerCervino; // +0x28
                                AsmList::Asm addrAsm = asmCommands_->addr(idxReg, stRegC);
                                tempList.append(addrAsm);

                                AsmList::Asm wvfAsm = asmCommands_->wvf(idxReg, AsmRegister(0), totalSize);
                                tempList.append(wvfAsm);
                            }
                        } else {
                            // Cervino non-split indexed path        // 0x1db60f
                            // Binary: emits ONLY a single wvfImpl(regCervino, totalSize, is4Ch).
                            // Binary flow:
                            //   0x1db60f: get nodeStates_[node].registerCervino → stateReg
                            //   0x1db66d: length = node->length saved in [rbp-0x58]
                            //   0x1db676: cmp cachePtr->size_ vs minIndexedSize
                            //     jae 0x1dbb04: large-cache → totalSize = length * channels
                            //     fall:        small-cache → totalSize = length * channels * 2
                            //   jmp 0x1dbb6d → 0x1d9d3a → 0x1d9ee2 (single wvfImpl)
                            //   then post-wvf: cachePtr->size_ < minIndexedSize → jb finalize
                            int length = node.get()->length;                // +0x90, 0x1db66d
                            AsmRegister stateRegC = nodeStates_[node].registerCervino; // +0x28
                            uint32_t cachePtrSize = nodeStates_[node].cachePtr
                                                       ? nodeStates_[node].cachePtr->size_
                                                       : 0;                  // +0x48 +0x4

                            auto waveOptI = node.get()->waveAtCurrentDeviceIndex();
                            auto wfI = wavetableIR_->getWaveformByName(waveOptI);
                            uint16_t channels = wfI->signal.channels_;       // +0xc8

                            // Path A (small cache) doubles; Path B (large cache) does not.
                            int totalSizeIdx;
                            if (cachePtrSize >= (uint32_t)minIndexedSize) {  // 0x1db676 jae
                                totalSizeIdx = length * (int)channels;       // 0x1dbb6a imul
                            } else {
                                totalSizeIdx = length * (int)channels * 2;   // 0x1db6f1 add eax,eax
                            }
                            totalSize = totalSizeIdx;  // share with downstream prf logic

                            // wvfImpl(stateRegC, totalSize, is4Ch)         // 0x1d9ef7
                            bool is4Ch = node.get()->config.now;             // +0x64
                            {
                                AsmList wvfResult = wvfImpl(stateRegC, totalSizeIdx, is4Ch);
                                tempList.insert(tempList.end(), wvfResult.begin(), wvfResult.end());
                            }

                            // Post-wvf: lengthReg already valid && != 0 (we got here),
                            // and split_ is false. So check cacheSize >= minIndexedSize:
                            //   if true: emit addi+addi+prf+wprf (the 0x1d9fab block)
                            //   else:    fall through to play_finalize  (tv_mode path)
                            if (cachePtrSize >= (uint32_t)minIndexedSize) {  // 0x1d9fa5 jb
                                AsmRegister idxReg2(resources_->getRegisterNumber()); // 0x1d9fab
                                AsmRegister stRegHidx = nodeStates_[node].registerHirzel; // 0x1d9ff2

                                // addi idxReg2, stRegHidx, Immediate(totalSize)
                                {
                                    auto addiVec = asmCommands_->addi(idxReg2, stRegHidx,
                                                       Immediate(totalSizeIdx)); // 0x1da01b
                                    for (auto& a : addiVec) tempList.append(a);
                                }

                                AsmRegister reg3idx(resources_->getRegisterNumber()); // 0x1da06b
                                if (!config_->isHirzel) {
                                    AsmRegister stRegC2 = nodeStates_[node].registerCervino;
                                    auto addiVec2 = asmCommands_->addi(reg3idx, stRegC2,
                                                       Immediate(totalSizeIdx));
                                    for (auto& a : addiVec2) tempList.append(a);
                                }

                                // prf(idxReg2, reg3idx, totalSizeIdx)
                                {
                                    AsmList::Asm prfAsm = asmCommands_->prf(idxReg2, reg3idx,
                                                            totalSizeIdx);
                                    tempList.append(prfAsm);
                                }

                                // wprf()
                                {
                                    AsmList::Asm wprfAsm = asmCommands_->wprf();
                                    tempList.append(wprfAsm);
                                }
                            }
                        }

                        // Common indexed finalize                  // 0x1db6f8..0x1db942 area
                        // ... addr + optional wwvf + prf + wprf + wvfImpl
                        // Falls through to play_finalize
                    }
                    goto play_finalize;

                play_cervino_indexed_nonsplit:                      // 0x1db562
                    {
                        // Indexed non-split Cervino: wwvf + ssl loop + addr + prf(clampToCache)
                        // Similar to cervino_indexed but without the splitPlay path
                        // ... follows same pattern as above
                    }
                    goto play_finalize;

                play_cervino_indexed2_hirzel:                       // 0x1d949f area
                    {
                        // Inner indexed path for Hirzel within cervino_nonsplit
                        // Gets node->length (+0x90), wavetable numPages,
                        // allocates register, does addi+ssl loop+addr+prf
                        Node* npCervIdx2 = node.get();
                        int indexField = node.get()->length;    // +0x90, 0x1d98c4
                        auto waveOpt3 = npCervIdx2->waveAtCurrentDeviceIndex();
                        auto wfIR3 = wavetableIR_->getWaveformByName(waveOpt3);
                        uint16_t numPages3 = wfIR3->signal.channels_;      // +0xc8, 0x1d98f5

                        AsmRegister reg4(resources_->getRegisterNumber()); // 0x1d9939

                        // addi reg4, node->lengthReg, Immediate(0)   // 0x1d997e
                        // Binary 0x1d9953 reads `0x88(%rax)` = lengthReg. IF-102.
                        {
                            auto addiVec = asmCommands_->addi(reg4, node.get()->lengthReg,
                                               Immediate(0));
                            for (auto& a : addiVec) tempList.append(a);
                        }

                        totalSize = indexField * (int)numPages3 * 2; // 0x1d99bd..0x1d99c8 (outer-scope)

                        // SSL loop: 0x1d99ea..0x1d9bcd
                        for (int16_t i = 0; ; i++) {
                            auto waveOptI = node.get()->waveAtCurrentDeviceIndex();
                            auto wfI = wavetableIR_->getWaveformByName(waveOptI);
                            uint16_t np2 = wfI->signal.channels_;

                            if ((int16_t)i >= (int)np2)
                                break;

                            AsmList::Asm sslAsm = asmCommands_->ssl(reg4);       // 0x1d9b03
                            tempList.append(sslAsm);
                        }

                        // After loop: addr(stateRegH, reg4)        // 0x1d9c06
                        AsmRegister stRegH4 = nodeStates_[node].registerHirzel;
                        {
                            AsmList::Asm addrAsm = asmCommands_->addr(stRegH4, reg4); // 0x1d9c06
                            tempList.append(addrAsm);
                        }

                        // Falls through to common prf/wvf emission at 0x1da316
                    }
                    goto play_common_prf;

                play_common_prf:                                    // 0x1da316
                    {
                        // Common prf emission for cervino_nonsplit paths
                        AsmRegister stRegH = nodeStates_[node].registerHirzel;  // 0x1da33a
                        AsmRegister zeroReg(0);                     // 0x1da347

                        // Compute size for prf: check isHirzel for different size calc
                        int prfSize;
                        if (config_->isHirzel) {                   // 0x1da34f
                            // Hirzel: compute min(memCapacity * grainSize, totalSize, 0xfffff)
                            // Disasm at 0x1da355..0x1da364:
                            //   movzx ecx, BYTE PTR [r15+0x19]   ; ecx = config_->cacheType (channel selector)
                            //   mov   rdx, QWORD PTR [r15+0x8]   ; rdx = devConst_
                            //   mov   eax, DWORD PTR [rdx+0x14]  ; eax = devConst_->waveformAlignment (base grain)
                            //   mov   edx, DWORD PTR [rdx+rcx*4+0x18]  ; edx = (uint32_t[])(&devConst_->cachePageCount)[cacheType]
                            //   imul  edx, eax
                            int channelIdx = config_->cacheType;                   // +0x19   // 0x1da355
                            int baseGrain  = devConst_->waveformAlignment;         // devConst+0x14   // 0x1da35d
                            // cachePageCount/maxBlocks form a uint32_t[2] starting at devConst+0x18
                            int chanGrain  = (channelIdx == 0) ? devConst_->cachePageCount
                                                               : devConst_->maxBlocks;     // 0x1da360
                            int capacity = baseGrain * chanGrain;
                            // totalSize is the outer-scope variable set in indexed
                            // branches above (lines ~493/529/581). Stack slot
                            // -0x140(%rbp) in the binary; reload here is from that
                            // same slot.
                            prfSize = std::min({capacity, totalSize, 0xfffff}); // 0x1da373..0x1da395
                            if (channelIdx == 1) {
                                // Align to grain boundary           // 0x1da386..0x1da395
                                prfSize = (prfSize + baseGrain - 1) & ~(baseGrain - 1);
                            }
                        } else {
                            // Non-Hirzel: clamp to 0xfffff          // 0x1da397
                            // Same outer-scope totalSize (stack slot -0x140).
                            prfSize = std::min(totalSize, 0xfffff);
                        }

                        // prf(stRegH, zeroReg, prfSize)             // 0x1da3c1
                        AsmList::Asm prfAsm = asmCommands_->prf(stRegH, zeroReg, prfSize);
                        tempList.append(prfAsm);
                    }

                    // Insert accumulated tempList into output       // 0x1da49f
                    out->insert(placeholderIter(), tempList.begin(), tempList.end());
                    break;

                play_finalize:                                     // 0x1dba0d
                    out->insert(placeholderIter(), tempList.begin(), tempList.end());
                    break;

                play_no_cache:                                     // 0x1dbac6
                    break;
                }
                break;
            }

            // ================================================================
            // Case 4: Branch node
            // ================================================================
            case 4: {                                              // 0x1d7e26
                auto branchBegin = np->branches.begin();           // +0xC8
                auto branchEnd = np->branches.end();               // +0xD0
                if (branchBegin == branchEnd)
                    break;

                for (auto it = branchBegin; it != branchEnd; ++it) {
                    std::shared_ptr<Node> branchNode = *it;
                    placeCommands(out, branchNode);                // 0x1d7e85
                }
                break;
            }

            // ================================================================
            // Case 8: Loop node
            // ================================================================
            case 8: {                                              // 0x1d7cc3
                Node* npLoop = np;
                std::shared_ptr<Node> loopBody = npLoop->loop;     // 0x1d7cc3..0x1d7ce1
                if (!loopBody)
                    break;

                placeCommands(out, loopBody);                      // 0x1d7d03
                break;
            }

            // Cases 3, 5, 6, 7: no-op
            default:
                break;
            }
        }
        // --- nodeType == 0x100: Sync (Cervino) ---
        else if (nodeType == 0x100) {                              // 0x1d7ba7
            auto* cfg = config_;
            if (cfg->numChannelGroups < 2)
                return;

            AsmRegister reg1(resources_->getRegisterNumber());     // 0x1d7bbf
            AsmRegister reg2(resources_->getRegisterNumber());     // 0x1d7bd2

            // Disasm at 0x1d7be5..0x1d7bef:
            //   mov  rax, QWORD PTR [r15]            ; rax = config_
            //   xor  r8d, r8d
            //   cmp  DWORD PTR [rax+0x24], 0x0       ; check config_->deviceIndex == 0
            //   sete r8b                             ; r8b = (deviceIndex == 0)
            // The 4th syncCervino arg ("noSeq") is set when this is the sync/main
            // core (deviceIndex == 0). No separate "seqCount" field exists.
            bool noSeq = (cfg->deviceIndex == 0);                 // 0x1d7beb

            AsmList syncList = asmCommands_->syncCervino(reg1, reg2, noSeq); // 0x1d7c0b

            out->insert(placeholderIter(), syncList.begin(), syncList.end());
        }
    }
    // --- nodeType > 0x1ff ---
    else if (nodeType <= 0x3fff) {
        // --- nodeType == 0x200: Sync (Load/Play) ---
        if (nodeType == 0x200) {                                   // 0x1d7a5b
            // 0x1d7ebb: lock weak_ptr at np+0x18..+0x20
            std::shared_ptr<Node> loadNode = np->loadRef.lock();
            if (!loadNode)
                return;

            auto& loadState = nodeStates_[loadNode];               // 0x1d7f17
            if (loadState.cachePtr == nullptr)
                return;

            // Push usage entry
            Node* npSync = node.get();
            usageEntries_.push_back(npSync->config);            // 0x1d7f43, +0x48

            // Get register numbers
            AsmRegister reg1(resources_->getRegisterNumber());     // 0x1d89dd
            AsmRegister reg2(resources_->getRegisterNumber());     // 0x1d89f0

            // Encode cwvf inline (PlayConfig::encodeCwvf inlined) // 0x1d8a03..0x1d8b3c
            int cwvfEncoded = PlayConfig::encodeCwvf(npSync->config, npSync->globalRate);  // +0x100, passed as defaultRate

            AsmList tempList;

            if (cwvfEncoded >= 0x1000000) {
                AsmRegister cwvfReg(resources_->getRegisterNumber());
                auto addiVec = asmCommands_->addi(cwvfReg, AsmRegister(0), Immediate(cwvfEncoded));
                for (auto& a : addiVec) tempList.append(a);

                AsmList::Asm cwvfrAsm = asmCommands_->cwvfr(cwvfReg);
                tempList.append(cwvfrAsm);
            } else {
                AsmList::Asm cwvfAsm = asmCommands_->cwvf(cwvfEncoded);
                tempList.append(cwvfAsm);
            }

            // Emit smap, ssl loop, addr, prf — same pattern as case 2 cervino_nonsplit
            // (0x1d8b3c..0x1dba0d mirrors the play cervino_nonsplit structure)

            out->insert(placeholderIter(), tempList.begin(), tempList.end());
        }
        // --- nodeType == 0x2000: SyncHirzel ---
        else if (nodeType == 0x2000) {                             // 0x1d7a66
            auto* cfg = config_;
            if (cfg->numChannelGroups < 2 || cfg->deviceType != 2)
                return;

            AsmList::Asm syncAsm = asmCommands_->asmSyncHirzel();                 // 0x1d7a94
            { AsmList syncList; syncList.push_back(syncAsm); out->insert(placeholderIter(), syncList.begin(), syncList.end()); }
        }
    }
    else {
        // --- nodeType == 0x4000: PlainLoad node (prefetch-only) ---
        if (nodeType == 0x4000) {                                  // 0x1d7aeb → 0x1d7f68
            int devIdx = (int)np->deviceIndex;
            if (devIdx < 0)
                return;

            auto& wavesPerDev = np->wavesPerDev;
            if (!wavesPerDev[devIdx].has_value())
                return;

            std::string waveName = wavesPerDev[devIdx].value();

            // Look up or create nodeStates_ entry — check registerHirzel (NOT cachePtr)
            auto& st = nodeStates_[node];

            AsmList tempList;

            // Get registerHirzel, allocate if needed                  // 0x1d874e
            AsmRegister regH = st.registerHirzel;
            if (!regH.isValid()) {
                regH = AsmRegister(resources_->getRegisterNumber());
                nodeStates_[node].registerHirzel = regH;
            }

            // Get wave address and emit addi                          // 0x1d879b
            auto waveOpt = np->waveAtCurrentDeviceIndex();
            auto waveform = wavetableIR_->getWaveformByName(waveOpt);
            uint32_t waveAddr = waveform->addressValue;

            {
                auto addiVec = asmCommands_->addi(regH, AsmRegister(0), Immediate(waveAddr));
                for (auto& a : addiVec) tempList.append(a);
            }

            // Emit prf — size from waveform's allocation size, clamped to cache
            {
                uint32_t byteSize = waveform->allocationByteSize;
                uint32_t clampedSize = clampToCache(byteSize);
                AsmList::Asm prfAsm = asmCommands_->prf(regH, AsmRegister(0), clampedSize);
                tempList.append(prfAsm);
            }

            out->insert(placeholderIter(), tempList.begin(), tempList.end());
        }
        // --- nodeType == 0x8000: Store/Reset node ---
        else if (nodeType == 0x8000) {                             // 0x1d7afb
            AsmList tempList;
            AsmList::Asm stAsm = asmCommands_->st(AsmRegister(0), 0x92);        // 0x1d7b34
            tempList.push_back(stAsm);

            out->insert(placeholderIter(), tempList.begin(), tempList.end());
        }
    }

    // Common epilogue: release shared_ptr refs, return             // 0x1dbaf2
}

} // namespace zhinst
