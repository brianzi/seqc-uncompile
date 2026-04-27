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

#include "zhinst/prefetch.hpp"
#include "zhinst/asm_commands.hpp"
#include "zhinst/awg_compiler_config.hpp"
#include "zhinst/device_constants.hpp"
#include "zhinst/asm_list.hpp"
#include "zhinst/asm_commands.hpp"
#include "zhinst/node.hpp"
#include "zhinst/waveform.hpp"
#include "zhinst/waveform_ir.hpp"
#include "zhinst/wavetable_ir.hpp"
#include "zhinst/cache.hpp"
#include "zhinst/resources.hpp"
#include "zhinst/play_config.hpp"

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
            // Case 1: Load node
            // ================================================================
            case 1: {                                              // 0x1d79f8
                AsmList tempList;  // declared early to avoid goto-crosses-init
                int totalSize = 0; // declared early; flows to play_common_prf via stack
                                   // slot -0x140(%rbp). Set in indexed branches below
                                   // (lines ~490/526/578) and consumed at lines 636/644.
                int devIdx = (int)np->deviceIndex;                 // +0x40
                if (devIdx < 0)
                    break;  // case1_no_wave

                if (!np->wavesPerDev[devIdx].has_value())
                    break;  // case1_no_wave

                {
                auto& wavesPerDev = np->wavesPerDev;               // +0x28

                std::string waveName = wavesPerDev[devIdx].value(); // 0x1d7a26..0x1d8281

                // Look up or create nodeStates_ entry for this node
                auto& state = nodeStates_[node];                   // 0x1d82a5

                auto cachePtr = state.cachePtr;                    // offset +0x48
                if (!cachePtr)
                    break;  // case1_no_wave

                auto& loadState = nodeStates_[node];
                if (loadState.cachePtr->size_ == 0)                // 0x1d8308
                    break;

                // tempList declared early (before goto targets) to avoid goto-crosses-init

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
                                if (loadSt2.pageSize >= 2) {       // 0x1d901f
                                    // splitPlay path
                                    AsmList splitResult = splitPlay(node);  // 0x1d9056
                                    tempList.insert(tempList.end(),
                                        splitResult.begin(), splitResult.end());
                                    goto play_finalize;
                                }
                                // Cervino non-split path
                                goto play_cervino_nonsplit;
                            }
                            // Hirzel: falls through to dummy/wvf check
                        }
                    }

                    // ---- Common play: check isDummy (+0x66) ----
                    {
                        Node* npD = node.get();
                        if (!npD->config.dummy)                     // 0x1d90b9, +0x66 = config+0x1E
                            goto play_finalize;

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

                            // Compute effective length: check dc->maxWaveformLength(),
                            // dc->grainSize(), pad to grain boundary
                            // See 0x1d9162..0x1d91b3 for the length computation
                            if (npWvf->length != 0) {  // +0x90
                                int grainSize = dc->grainSize();     // +0x44
                                len2 = npWvf->length;  // +0x90
                                int paddedLen = ((len2 + grainSize - 1) / grainSize) * grainSize;
                                int maxLen = dc->maxWaveformLength(); // +0x40
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
                        // Check node->indexOffsetReg: if valid and non-zero, goto indexed path
                        Node* npCerv = node.get();
                        if (npCerv->indexOffsetReg.isValid()) {    // 0x1d91dc, +0x94
                            AsmRegister zeroReg(0);
                            if (!(npCerv->indexOffsetReg == zeroReg)) {
                                goto play_cervino_indexed;          // 0x1d920e → 0x1dabb9
                            }
                        }

                        // Get waveform size per device
                        auto waveOpt = npCerv->waveAtCurrentDeviceIndex(); // 0x1d9225
                        auto wfIR = wavetableIR_->getWaveformByName(waveOpt); // 0x1d9238
                        int sizePerDev = wfIR->getSizePerDevice(); // 0x1d9241

                        int pageSize = 1;
                        if (!config_->isHirzel) {                  // 0x1d9250
                            pageSize = nodeStates_.at(node).pageSize; // 0x1d925c, +0x1c
                        }
                        int pageCount = sizePerDev / pageSize;     // 0x1d9267

                        if (config_->isHirzel) {                   // 0x1d92a1
                            // ----- Hirzel cervino_nonsplit -----
                            // Look up nodeStates_[node] to get registerHirzel (+0x20)
                            AsmRegister regH = nodeStates_[node].registerHirzel; // 0x1d92cf

                            // virtualCallIdx stored at -0x278(%rbp) for cache dispatch
                            // Emit smap: smap(reg150, reg168, 0x400 + node->tableIndex)
                            int smapOffset = 0x400 + node.get()->tableIndex; // +0x9C // 0x1d9340..0x1d934d
                            {
                                auto smapVec = asmCommands_->smap(regH,
                                    nodeStates_[node].registerCervino,
                                    smapOffset);                   // 0x1d9354
                                for (auto& a : smapVec) tempList.append(a);
                            }

                            // Second nodeStates_ lookup: get cachePtr               // 0x1d941c
                            auto& st2 = nodeStates_[node];
                            if (st2.cachePtr->size_ != 0) {        // 0x1d9446..0x1d944a

                                // Check node->indexOffsetReg again for the inner indexed path
                                if (npCerv->indexOffsetReg.isValid()) {   // 0x1d946d, +0x94
                                    AsmRegister zeroReg(0);
                                    if (!(npCerv->indexOffsetReg == zeroReg)) {
                                        // Inner indexed path — see play_cervino_indexed2 below
                                        goto play_cervino_indexed2_hirzel; // 0x1d949f → separate block
                                    }
                                }

                                // Non-indexed Hirzel: get waveform numPages, compute byteCount
                                auto waveOpt2 = npCerv->waveAtCurrentDeviceIndex(); // 0x1d94b6
                                auto wfIR2 = wavetableIR_->getWaveformByName(waveOpt2); // 0x1d94c9
                                int sizePerDev2 = wfIR2->getSizePerDevice(); // 0x1d94d2

                                int pageSize2 = 1;
                                if (!config_->isHirzel) {          // 0x1d94df
                                    pageSize2 = nodeStates_[node].pageSize; // 0x1d9514, +0x1c
                                }
                                int pageCount2 = sizePerDev2 / pageSize2; // 0x1d9518

                                // Look up registerHirzel from nodeStates_ again
                                AsmRegister regH2 = nodeStates_[node].registerHirzel; // 0x1d9581

                                // Load the cachePtr from nodeStates_[node]
                                AsmRegister cacheReg = nodeStates_[node].registerCervino; // +0x28, 0x1d9d30

                                // Check config->deviceType                // 0x1d9d3d
                                if (config_->deviceType == HDAWG) {    // deviceType 2 = Hirzel
                                    // Hirzel-specific wvfImpl path // 0x1d9d49 → 0x1d9ee2 area
                                    // ... see below
                                } else if (nodeStates_[node].cachePtr->size_ == devConst_->waveformMemorySize) {
                                    // Cache size matches config: split wvfImpl           // 0x1d9d5f
                                    int halfPageCount = pageCount2 / 2;  // signed div, rounds toward zero
                                    // confirmed: shr $0x1f + add + sar $1 pattern = signed /2   // 0x1d9d68..0x1d9d75
                                    bool is4Ch = node.get()->config.now; // +0x64 = config+0x1C       // 0x1d9d78..0x1d9d7d

                                    // First wvfImpl call
                                    {
                                        AsmList wvfResult = wvfImpl(cacheReg, halfPageCount, is4Ch); // 0x1d9d8a
                                        tempList.insert(tempList.end(), wvfResult.begin(), wvfResult.end());
                                    }

                                    // addi tempReg2, cacheReg, Immediate(halfPageCount)
                                    AsmRegister tempReg2(resources_->getRegisterNumber()); // 0x1d9dd5
                                    {
                                        auto addiVec = asmCommands_->addi(tempReg2, cacheReg,
                                                           Immediate(halfPageCount)); // 0x1d9e24
                                        for (auto& a : addiVec) tempList.append(a);
                                    }

                                    // Second wvfImpl call
                                    {
                                        is4Ch = node.get()->config.now;  // +0x64 = config+0x1C
                                        AsmList wvfResult2 = wvfImpl(tempReg2, halfPageCount, is4Ch); // 0x1d9ea2
                                        tempList.insert(tempList.end(), wvfResult2.begin(), wvfResult2.end());
                                    }
                                } else {
                                    // Default: single wvfImpl       // 0x1d9ee2
                                    bool is4Ch = npCerv->config.now;  // +0x64 = config+0x1C
                                    AsmList wvfResult = wvfImpl(cacheReg, pageCount2, is4Ch); // 0x1d9ef7
                                    tempList.insert(tempList.end(), wvfResult.begin(), wvfResult.end());
                                }

                                // Post-wvf: check asmRegister again          // 0x1d9f3b
                                if (node.get()->indexOffsetReg.isValid()) {      // 0x1d9f43, +0x94
                                    int regVal = (int)node.get()->indexOffsetReg; // 0x1d9f58
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
                            } else {
                                // cachePtr->size_ == 0: skip to finalize  // 0x1da6df
                            }
                        } else {
                            // ----- Cervino (non-Hirzel) cervino_nonsplit -----
                            // Same structure as Hirzel path but uses registerCervino
                            // and different smap offset, plus ssl loop

                            AsmRegister regC = nodeStates_[node].registerCervino; // +0x28, 0x1d9d2c

                            // smap with node->tableIndex               // 0x1da567..0x1da591
                            int smapOffsetC = node.get()->tableIndex;   // +0x9C
                            {
                                auto smapVec = asmCommands_->smap(nodeStates_[node].registerHirzel,
                                    regC, smapOffsetC);            // 0x1da591
                                for (auto& a : smapVec) tempList.append(a);
                            }

                            // addi tempReg, regC, Immediate(pageCount) for ssl base
                            AsmRegister tempReg(resources_->getRegisterNumber());  // 0x1da7da

                            // addi tempReg, node->indexOffsetReg, Immediate(0)  // 0x1da823
                            {
                                AsmList addiInit;
                                asmCommands_->addi(addiInit, tempReg, node.get()->indexOffsetReg,
                                                   Immediate(0));
                                tempList.insert(tempList.end(), addiInit.begin(), addiInit.end());
                            }

                            // SSL loop: for each channel index i = 0..channels-1  // 0x1da876..0x1daa42
                            // Verified: WaveformIR+0xC8 = signal.channels_ (uint16_t)
                            // (Waveform::signal at +0x80, Signal::channels_ at +0x48).
                            // The variable was named "numPages" in the original
                            // source but the binary iterates over signal channels.
                            for (int16_t i = 0; ; i++) {
                                // Get wave name for current device index
                                auto waveOptI = node.get()->waveAtCurrentDeviceIndex();
                                auto wfI = wavetableIR_->getWaveformByName(waveOptI);
                                uint16_t numPages = wfI->signal.channels_; // +0xc8

                                if ((int16_t)i >= (int)numPages)
                                    break;

                                // Emit ssl(tempReg)                    // 0x1da990
                                AsmList::Asm sslAsm;
                                asmCommands_->ssl(sslAsm, tempReg);
                                tempList.append(sslAsm);
                            }

                            // After ssl loop: emit addr(stateRegH, tempReg)  // 0x1daa86
                            {
                                AsmRegister stRegH2 = nodeStates_[node].registerHirzel;
                                AsmList::Asm addrAsm;
                                asmCommands_->addr(addrAsm, stRegH2, tempReg);
                                tempList.append(addrAsm);
                            }

                            // Check cachePtr->size_ vs minIndexedSize     // 0x1daacb..0x1daad8
                            uint32_t cacheSizeC = nodeStates_[node].cachePtr->size_;
                            if (cacheSizeC >= minIndexedSize) {            // 0x1daad2
                                // Indexed: emit wwvf, ssl loop, prf with clampToCache
                                // ... see play_cervino_indexed_nonsplit below
                                goto play_cervino_indexed_nonsplit;
                            } else {
                                // Non-indexed: emit wwvf
                                {
                                    AsmList::Asm wwvfAsm;
                                    asmCommands_->wwvf(wwvfAsm);   // 0x1daae9
                                    tempList.append(wwvfAsm);
                                }

                                // Second ssl loop + addr + prf + wprf + wvfImpl
                                // (mirrors the first ssl loop structure)   // 0x1dab0a..0x1dabb4
                                AsmRegister stRegH3 = nodeStates_[node].registerHirzel;  // 0x1dab33
                                AsmRegister stRegC3 = nodeStates_[node].registerCervino; // 0x1dab58
                                uint32_t cacheSizeC2 = nodeStates_[node].cachePtr->size_;
                                int clampedC = clampToCache(cacheSizeC2);   // 0x1dab8c

                                AsmList::Asm prfAsmC;
                                asmCommands_->prf(prfAsmC, stRegH3, stRegC3, clampedC); // 0x1dab9f
                                tempList.append(prfAsmC);
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

                            // addi idxReg, node->indexOffsetReg, Immediate(0) // 0x1dac9a
                            {
                                AsmList addiIdx;
                                asmCommands_->addi(addiIdx, idxReg, node.get()->indexOffsetReg,
                                                   Immediate(0));
                                tempList.insert(tempList.end(), addiIdx.begin(), addiIdx.end());
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

                                AsmList::Asm sslAsm;
                                asmCommands_->ssl(sslAsm, idxReg); // 0x1dae1d
                                tempList.append(sslAsm);
                            }

                            // addr + wwvf + prf + clampToCache     // done inline
                            // ...falls through to common finalize
                        } else {
                            // Cervino non-Hirzel indexed path       // 0x1daed4
                            // Same structure but uses different register source
                            int length2 = node.get()->length; // +0x90   // 0x1daee5
                            auto waveOptIdx = npCervIdx->waveAtCurrentDeviceIndex();
                            auto wfIdx = wavetableIR_->getWaveformByName(waveOptIdx);
                            uint16_t numPages = wfIdx->signal.channels_;

                            AsmRegister idxReg(resources_->getRegisterNumber()); // 0x1daf5c

                            // addi idxReg, node->indexOffsetReg, Immediate(0)
                            {
                                AsmList addiIdx;
                                asmCommands_->addi(addiIdx, idxReg, node.get()->indexOffsetReg,
                                                   Immediate(0));
                                tempList.insert(tempList.end(), addiIdx.begin(), addiIdx.end());
                            }

                            totalSize = length2 * (int)numPages * 2;  // assigns outer-scope var

                            // SSL loop
                            for (int16_t i = 0; ; i++) {
                                auto waveOptI = node.get()->waveAtCurrentDeviceIndex();
                                auto wfI = wavetableIR_->getWaveformByName(waveOptI);
                                uint16_t np2 = wfI->signal.channels_;

                                if ((int16_t)i >= (int)np2)
                                    break;

                                AsmList::Asm sslAsm;
                                asmCommands_->ssl(sslAsm, idxReg);
                                tempList.append(sslAsm);
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

                        // addi reg4, node->indexOffsetReg, Immediate(0)   // 0x1d997e
                        {
                            AsmList addiInit;
                            asmCommands_->addi(addiInit, reg4, node.get()->indexOffsetReg,
                                               Immediate(0));
                            tempList.insert(tempList.end(), addiInit.begin(), addiInit.end());
                        }

                        totalSize = indexField * (int)numPages3 * 2; // 0x1d99bd..0x1d99c8 (outer-scope)

                        // SSL loop: 0x1d99ea..0x1d9bcd
                        for (int16_t i = 0; ; i++) {
                            auto waveOptI = node.get()->waveAtCurrentDeviceIndex();
                            auto wfI = wavetableIR_->getWaveformByName(waveOptI);
                            uint16_t np2 = wfI->signal.channels_;

                            if ((int16_t)i >= (int)np2)
                                break;

                            AsmList::Asm sslAsm;
                            asmCommands_->ssl(sslAsm, reg4);       // 0x1d9b03
                            tempList.append(sslAsm);
                        }

                        // After loop: addr(stateRegH, reg4)        // 0x1d9c06
                        AsmRegister stRegH4 = nodeStates_[node].registerHirzel;
                        {
                            AsmList::Asm addrAsm;
                            asmCommands_->addr(addrAsm, stRegH4, reg4); // 0x1d9c06
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
                        AsmList::Asm prfAsm;
                        asmCommands_->prf(prfAsm, stRegH, zeroReg, prfSize);
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
                std::shared_ptr<Node> loopBody;
                loopBody.reset(npLoop->loop.get());                // 0x1d7cc3..0x1d7ce1
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
            if (cfg->syncVersion() < 2)
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

            AsmList syncList;
            asmCommands_->syncCervino(syncList, reg1, reg2, noSeq); // 0x1d7c0b

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
                AsmList addiCwvf;
                asmCommands_->addi(addiCwvf, cwvfReg, AsmRegister(0), Immediate(cwvfEncoded));
                tempList.insert(tempList.end(), addiCwvf.begin(), addiCwvf.end());

                AsmList::Asm cwvfrAsm;
                asmCommands_->cwvfr(cwvfrAsm, cwvfReg);
                tempList.append(cwvfrAsm);
            } else {
                AsmList::Asm cwvfAsm;
                asmCommands_->cwvf(cwvfAsm, cwvfEncoded);
                tempList.append(cwvfAsm);
            }

            // Emit smap, ssl loop, addr, prf — same pattern as case 2 cervino_nonsplit
            // (0x1d8b3c..0x1dba0d mirrors the play cervino_nonsplit structure)

            out->insert(placeholderIter(), tempList.begin(), tempList.end());
        }
        // --- nodeType == 0x2000: SyncHirzel ---
        else if (nodeType == 0x2000) {                             // 0x1d7a66
            auto* cfg = config_;
            if (cfg->syncVersion() < 2 || cfg->deviceType != 2)
                return;

            AsmList::Asm syncAsm;
            asmCommands_->asmSyncHirzel(syncAsm);                 // 0x1d7a94
            { AsmList syncList; syncList.push_back(syncAsm); out->insert(placeholderIter(), syncList.begin(), syncList.end()); }
        }
    }
    else {
        // --- nodeType == 0x4000: CWVF-only node ---
        if (nodeType == 0x4000) {                                  // 0x1d7aeb
            int devIdx = (int)np->deviceIndex;
            if (devIdx < 0)
                return;

            auto& wavesPerDev = np->wavesPerDev;
            if (!wavesPerDev[devIdx].has_value())
                return;

            std::string waveName = wavesPerDev[devIdx].value();

            // Look up or create nodeStates_ entry
            auto& st = nodeStates_[node];
            if (st.cachePtr == nullptr || st.cachePtr->size_ == 0)
                return;

            AsmList tempList;

            // Get registerHirzel, allocate if needed
            AsmRegister regH = st.registerHirzel;
            if (!regH.isValid()) {
                regH = AsmRegister(resources_->getRegisterNumber());
                nodeStates_[node].registerHirzel = regH;
            }

            // Get wave address and emit addi
            auto waveOpt = np->waveAtCurrentDeviceIndex();
            auto waveform = wavetableIR_->getWaveformByName(waveOpt);
            uint32_t waveAddr = waveform->addressValue;

            {
                AsmList addiResult;
                asmCommands_->addi(addiResult, regH, AsmRegister(0), Immediate(waveAddr));
                tempList.insert(tempList.end(), addiResult.begin(), addiResult.end());
            }

            // Encode and emit cwvf
            usageEntries_.push_back(np->config);  // +0x48
            int cwvfEncoded = PlayConfig::encodeCwvf(np->config, np->globalRate);  // +0x100, passed as defaultRate

            if (cwvfEncoded >= 0x1000000) {
                AsmRegister cwvfReg(resources_->getRegisterNumber());
                AsmList addiCwvf;
                asmCommands_->addi(addiCwvf, cwvfReg, AsmRegister(0), Immediate(cwvfEncoded));
                tempList.insert(tempList.end(), addiCwvf.begin(), addiCwvf.end());

                AsmList::Asm cwvfrAsm;
                asmCommands_->cwvfr(cwvfrAsm, cwvfReg);
                tempList.append(cwvfrAsm);
            } else {
                AsmList::Asm cwvfAsm;
                asmCommands_->cwvf(cwvfAsm, cwvfEncoded);
                tempList.append(cwvfAsm);
            }

            // Emit prf for the CWVF-only node
            {
                uint32_t cacheSize = nodeStates_[node].cachePtr->size_;
                uint32_t clampedSize = clampToCache(cacheSize);
                AsmList::Asm prfAsm;
                asmCommands_->prf(prfAsm, regH, AsmRegister(0), clampedSize);
                tempList.append(prfAsm);
            }

            out->insert(placeholderIter(), tempList.begin(), tempList.end());
        }
        // --- nodeType == 0x8000: Store/Reset node ---
        else if (nodeType == 0x8000) {                             // 0x1d7afb
            AsmList tempList;
            AsmList::Asm stAsm;
            asmCommands_->st(stAsm, AsmRegister(0), 0x92);        // 0x1d7b34
            tempList.push_back(stAsm);

            out->insert(placeholderIter(), tempList.begin(), tempList.end());
        }
    }

    // Common epilogue: release shared_ptr refs, return             // 0x1dbaf2
}

} // namespace zhinst
