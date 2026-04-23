// ============================================================================
// Reconstructed from disassembly of _seqc_compiler.so
// File: prefetch_splitplay.cpp — Prefetch::splitPlay
//
// Method reconstructed:
//   splitPlay    0x1dd1a0 .. ~0x1deb66
// ============================================================================

#include "zhinst/prefetch.hpp"
#include "zhinst/asm_commands.hpp"
#include "zhinst/resources.hpp"
#include "zhinst/node.hpp"
#include "zhinst/cache.hpp"
#include "zhinst/waveform_ir.hpp"
#include "zhinst/wavetable_ir.hpp"
#include "zhinst/device_constants.hpp"
#include "zhinst/awg_compiler_config.hpp"

#include <cstdint>
#include <string>

namespace zhinst {

// ============================================================================
// splitPlay — generate assembly for a waveform play that must be split across
// multiple cache pages. Computes total waveform length, determines how many
// full pages fit, emits a loop of insertPlay calls with page-sized chunks,
// and handles the remainder.
//
// 0x1dd1a0 — 0x1deb66
//
// Signature: AsmList splitPlay(std::shared_ptr<Node> node) const
// Returns AsmList (vector<AsmList::Asm>) by sret pointer.
//
// node layout used:
//   +0x28  wavesPerDev (vector<optional<string>>, each 0x20 bytes)
//   +0x40  deviceIndex (int)
//   +0x4C  length (int)     — used via WaveformIR
//   +0x64  config.now (bool) — confirmed (was "indexed")
//   +0x68  PlayConfig
//   +0x88  lengthReg (AsmRegister)
//   +0x90  length (int)     — if nonzero, triggers simple path
// ============================================================================
AsmList Prefetch::splitPlay(std::shared_ptr<Node> node) const  // 0x1dd1a0
{
    AsmList result;
    Node* raw = node.get();

    // ---- Step 1: Compute totalLength (r12d) ----
    // 0x1dd1d3..0x1dd458
    uint32_t totalLength;

    if (raw->length != 0) {  // 0x1dd1da: test r12d,r12d, +0x90
        // length2 != 0 path (Hirzel-like simple path)
        // 0x1dd1e3..0x1dd36b
        auto wfIR = wavetableIR_;  // this+0x110
        int devIdx = raw->deviceIndex;  // +0x40

        // Get the optional<string> at wavesPerDev[devIdx]
        // wavesPerDev is at raw+0x28, each element 0x20 bytes
        // 0x1dd1f7: mov 0x28(%rax), %rax; shl $0x5, %rcx
        std::optional<std::string> waveName;
        if (devIdx >= 0) {
            const auto& opt = raw->wavesPerDev[devIdx];
            if (opt.has_value()) {
                waveName = opt;
            }
        }

        // 0x1dd2e3: call WavetableIR::getWaveformByName
        auto wfm = wfIR->getWaveformByName(waveName);

        // 0x1dd2fd: movzwl 0xc8(%rax) — channels (uint16_t)
        uint16_t channels = wfm->signal.channels_;  // WaveformIR+0xC8

        // totalLength = length2 * channels * 2
        totalLength = raw->length * channels;  // 0x1dd31b: imul %ebx,%r12d, +0x90
        totalLength *= 2;                        // 0x1dd368: add %r12d,%r12d
    } else {
        // length2 == 0 path — compute from signal data
        // 0x1dd23e..0x1dd458
        auto wfIR = wavetableIR_;
        int devIdx = raw->deviceIndex;

        std::optional<std::string> waveName;
        if (devIdx >= 0) {
            const auto& opt = raw->wavesPerDev[devIdx];
            if (opt.has_value()) {
                waveName = opt;
            }
        }

        auto wfm = wfIR->getWaveformByName(waveName);

        uint16_t channels = wfm->signal.channels_;   // +0xC8
        uint32_t numRepeats = wfm->signal.length_;    // +0xD0  confirmed
        DeviceConstants const* dc = wfm->deviceConstants;  // +0x78

        // 0x1dd3bf..0x1dd3e2: compute adjusted length
        uint32_t adjLen;
        if (numRepeats != 0) {
            uint32_t base = dc->waveformGranularity;    // +0x40
            uint32_t stride = dc->waveformPageSize;    // +0x44
            // ceil division * stride, capped at base
            uint32_t rounded = ((numRepeats + stride - 1) / stride) * stride;
            adjLen = std::min(rounded, base);
        } else {
            adjLen = 0;
        }

        // 0x1dd3e2..0x1dd400: totalLength = ceil((adjLen * channels * bitWidth) / 8)
        // bitWidth from dc+0x50
        uint64_t totalBits = static_cast<uint64_t>(adjLen) * channels;
        int bitWidth = dc->bitsPerSample;  // +0x50  confirmed
        totalBits *= bitWidth;
        totalLength = static_cast<uint32_t>(totalBits / 8);
        if ((totalBits & 7) != 0)
            totalLength++;  // 0x1dd3f8: sbb $0xffffffff, %r12d (round up)
    }

    // ---- Step 2: Set up shared_ptr<Node> copy for map lookups ----
    // 0x1dd458..0x1dd46c
    // Copy the node's parent/ref shared_ptr from raw+0x20 into local
    // for nodeStates_ lookups
    // 0x1dd462..0x1dd49c: lock weak_ptr at raw+0x18..+0x20
    std::shared_ptr<Node> lookupNode = raw->loadRef.lock();

    // ---- Step 3: Compute remainder if waveform exceeds cache page ----
    // 0x1dd4be..0x1dd555
    uint32_t remainder = 0;  // stored at -0x34(%rbp)

    if (lookupNode) {
        auto it = nodeStates_.find(lookupNode);
        if (it != nodeStates_.end()) {
            auto& pns = it->second;
            // 0x1dd4da: mov rax, [rax+0x48]   ; rax = pns.cachePtr.get()
            // 0x1dd4de: mov ebx, [rax+0xc]    ; ebx = cachePtr->numRepeats_
            uint32_t numRepeats = pns.cachePtr->numRepeats_;

            auto it2 = nodeStates_.find(lookupNode);
            if (it2 != nodeStates_.end()) {
                auto& pns2 = it2->second;
                // cachePtr->size / 2
                auto* cachePtr = reinterpret_cast<Cache::Pointer*>(
                    pns2.cachePtr.get());  // PNS+0x28 → hash_node+0x48
                uint32_t halfSize = cachePtr->size_ / 2;  // 0x1dd501: shr $1,%ebx

                if (totalLength < static_cast<uint32_t>(numRepeats * halfSize)) {
                    // totalLength fits — no split needed beyond one full page
                    // falls through to no-remainder case
                } else {
                    // 0x1dd508..0x1dd54e: compute how many full pages
                    auto it3 = nodeStates_.find(lookupNode);
                    auto& pns3 = it3->second;
                    // 0x1dd520/0x1dd524: same numRepeats_ load as above
                    uint32_t numRepeats2 = pns3.cachePtr->numRepeats_;

                    auto it4 = nodeStates_.find(lookupNode);
                    auto& pns4 = it4->second;
                    auto* cachePtr2 = reinterpret_cast<Cache::Pointer*>(
                        pns4.cachePtr.get());
                    uint32_t halfSize2 = cachePtr2->size_ / 2;

                    // remainder = totalLength - (numRepeats - 1) * halfSize
                    uint32_t fullPages = numRepeats2 - 1;  // 0x1dd53f: dec %ebx
                    uint32_t coveredByFull = fullPages * halfSize2;
                    remainder = totalLength - coveredByFull;  // 0x1dd54b
                    // 0x1dd552: mov %ebx,%r12d — totalLength now becomes coveredByFull
                    // (the full-page-aligned portion); the leftover is in `remainder`
                    totalLength = coveredByFull;
                }
            }
        }
    }

    // ---- Step 4: Get a register for the address computation ----
    // 0x1dd559..0x1dd567
    AsmRegister addrReg(resources_->getRegisterNumber());  // -0x168(%rbp)

    // ---- Step 5: Emit initial addi — load waveform offset into addrReg ----
    // 0x1dd56c..0x1dd72f
    {
        auto wfIR = wavetableIR_;
        int devIdx = raw->deviceIndex;
        std::optional<std::string> waveName;
        if (devIdx >= 0) {
            const auto& opt = raw->wavesPerDev[devIdx];
            if (opt.has_value()) {
                waveName = opt;
            }
        }

        // 0x1dd60f: call WavetableIR::getWaveformByName
        auto wfm = wfIR->getWaveformByName(waveName);

        // 0x1dd629: add 0x4c(%rax),%r12d — add wfm->waveformOffset to totalLength
        // WaveformIR+0x4C = some offset value
        uint32_t wfmOffset = wfm->addressValue;  // Waveform::addressValue +0x4C (formerly waveformOffset)
        uint32_t initialAddr = totalLength + wfmOffset;

        // 0x1dd637..0x1dd656: addi(asmCommands_, AsmRegister(0), addrReg, Immediate(initialAddr))
        AsmRegister zeroReg(0);  // -0x198(%rbp) was constructed with 0
        AsmList addiResult = asmCommands_->addi(addrReg, zeroReg, Immediate(initialAddr));
        // 0x1dd65b..0x1dd68c: insert into result at end
        result.insert(result.end(), addiResult.begin(), addiResult.end());
    }

    // ---- Step 6: Handle node's PlayConfig register (0x88) ----
    // 0x1dd7c8..0x1dd967
    AsmRegister copyReg;  // declared here — used in later steps
    if (raw->lengthReg.isValid() /* +0x88 */ ) {  // 0x1dd7d7: call isValid
        AsmRegister invalidReg(0);
        if (!(raw->lengthReg == invalidReg)) {  // 0x1dd80d: call operator==
            // Get a new register, emit addi(newReg, lengthReg, Immediate(0))
            copyReg = AsmRegister(resources_->getRegisterNumber());  // -0x150(%rbp)

            AsmList addiCopy = asmCommands_->addi(
                copyReg,
                raw->lengthReg,  // +0x88
                Immediate(0));
            result.insert(result.end(), addiCopy.begin(), addiCopy.end());
        }
    }

    // ---- Step 7: Set up Hirzel vs Cervino register ----
    // 0x1ddc7e..0x1dde05
    AsmRegister cervinoReg(0);  // -0x158(%rbp), init to invalid/0

    bool isHirzel = config_->isHirzel;  // config_+0x18
    if (!isHirzel) {
        // Cervino path: get register from nodeState
        // 0x1ddc99..0x1dddfb
        AsmRegister newReg(resources_->getRegisterNumber());
        cervinoReg = newReg;  // -0x158(%rbp) = -0xe0(%rbp)

        // Look up registerCervino from nodeStates_
        auto it = nodeStates_.find(node);
        if (it != nodeStates_.end()) {
            AsmRegister stateCervinoReg = it->second.registerCervino;  // PNS+0x08 → +0x28 from node

            // Emit addi(cervinoReg, stateCervinoReg, Immediate(0))
            AsmList addiCervino = asmCommands_->addi(cervinoReg, stateCervinoReg, Immediate(0));
            result.insert(result.end(), addiCervino.begin(), addiCervino.end());
        }
    }

    // ---- Step 8: Get Hirzel register and emit addi ----
    // 0x1dde05..0x1ddf7b
    AsmRegister hirzelReg(resources_->getRegisterNumber());  // -0x130(%rbp)

    {
        // Look up registerHirzel from nodeStates_
        auto it = nodeStates_.find(node);  // 0x1dde27..0x1dde31
        if (it != nodeStates_.end()) {
            AsmRegister stateHirzelReg = it->second.registerHirzel;  // PNS+0x00 → +0x20 from node

            // Emit addi(hirzelReg, stateHirzelReg, Immediate(0))
            AsmList addiHirzel = asmCommands_->addi(hirzelReg, stateHirzelReg, Immediate(0));
            result.insert(result.end(), addiHirzel.begin(), addiHirzel.end());
        }
    }

    // ---- Step 9: SSL loop — emit ssl instructions for each waveform channel ----
    // 0x1dd971..0x1ddb9b
    {
        int channelIdx = 0;
        for (;;) {
            auto wfIR = wavetableIR_;
            int devIdx = raw->deviceIndex;
            std::optional<std::string> waveName;
            if (devIdx >= 0) {
                const auto& opt = raw->wavesPerDev[devIdx];
                if (opt.has_value()) {
                    waveName = opt;
                }
            }
            auto wfm = wfIR->getWaveformByName(waveName);
            uint16_t channels = wfm->signal.channels_;  // +0xC8

            if (static_cast<int16_t>(channelIdx) >= static_cast<int>(channels)) {
                break;  // 0x1ddab6: jge to after loop
            }

            // 0x1ddac0..0x1ddad5: emit ssl(copyReg) — copyReg from step 6 at -0x150(%rbp)
            AsmList sslAsm = asmCommands_->ssl(copyReg);  // confirmed: -0x150(%rbp)
            result.push_back(sslAsm.front());

            channelIdx++;
        }
    }

    // ---- Step 10: Emit addr instruction ----
    // 0x1ddb9b..0x1ddc7a
    {
        // 0x1ddbbf: call AsmCommands::addr(addrReg, copyReg)
        AsmList addrAsm = asmCommands_->addr(addrReg, copyReg);  // confirmed: -0x150(%rbp)
        result.push_back(addrAsm.front());
    }

    // ---- Step 11: Generate labels ----
    // 0x1ddf85..0x1de066
    std::string playLabel = resources_->newLabel("play");  // 0x1ddf8c..0x1ddfab
    std::string lastLabel = resources_->newLabel("last");  // 0x1ddfd0..0x1ddff6
    std::string doneLabel = resources_->newLabel("done");  // 0x1de01b..0x1de041

    // ---- Step 12: Determine which register to use for insertPlay ----
    // 0x1de066..0x1de090
    bool indexed = raw->config.now;  // +0x64 = config+0x1C  confirmed (movzbl 0x64(%rax))

    // Choose register: if isHirzel, use hirzelReg (-0x130), else cervinoReg (-0x158)
    AsmRegister playReg = isHirzel ? hirzelReg : cervinoReg;

    // ---- Step 13: First insertPlay call — emit the main page play ----
    // 0x1de090..0x1de0f1
    {
        // Look up cachePtr->size / 2 from nodeStates_
        auto it = nodeStates_.find(node);
        auto* cachePtr = reinterpret_cast<Cache::Pointer*>(it->second.cachePtr.get());
        uint32_t halfSize = cachePtr->size_ / 2;  // 0x1de0ac: mov 0x4(%rax),%r15d; shr $1

        // Look up cachePtr->hash_ from second find (used as the start-address
        // hash key for insertPlay). 0x1de0cf reads +0x8 = hash_, not position_.
        auto it2 = nodeStates_.find(node);
        auto* cachePtr2 = reinterpret_cast<Cache::Pointer*>(it2->second.cachePtr.get());
        uint32_t cacheHash = cachePtr2->hash_;  // 0x1de0cf: mov 0x8(%rax),%eax

        // 0x1de0ec: call insertPlay(result, indexed, playLabel, playReg, halfSize, cacheHash)
        insertPlay(result, indexed, playLabel, playReg, halfSize, cacheHash);
    }

    // ---- Step 14: Emit addi to advance the register by halfSize ----
    // 0x1de0f1..0x1de20f
    {
        auto it = nodeStates_.find(node);
        auto* cachePtr = reinterpret_cast<Cache::Pointer*>(it->second.cachePtr.get());
        uint32_t halfSize = cachePtr->size_ / 2;  // 0x1de11f: shr $1,%esi

        AsmList addiAdv = asmCommands_->addi(hirzelReg, hirzelReg, Immediate(halfSize));
        result.insert(result.end(), addiAdv.begin(), addiAdv.end());
    }

    // ---- Step 15: Get a loop counter register, emit addi(counterReg, hirzelReg, Immediate(1)) ----
    // 0x1de241..0x1de35f. Verified Immediate(1) at 0x1de274 (mov esi,0x1).
    AsmRegister counterReg(resources_->getRegisterNumber());

    {
        AsmList addiLoop = asmCommands_->addi(counterReg, hirzelReg, Immediate(1));  // confirmed
        result.insert(result.end(), addiLoop.begin(), addiLoop.end());
    }

    // ---- Step 16: Emit subr(counterReg, addrReg) ----
    // 0x1de38b..0x1de458
    {
        // 0x1de3ae: call AsmCommands::subr(counterReg, addrReg)  confirmed
        AsmList subrAsm = asmCommands_->subr(counterReg, addrReg);
        result.push_back(subrAsm.front());
    }

    // ---- Step 17: Loop structure — conditional branches ----
    // 0x1de458..0x1de81f

    if (remainder == 0) {
        // 0x1de49d: emit brgz(counterReg, doneLabel, false)
        AsmList brgzDone = asmCommands_->brgz(counterReg, doneLabel, false);
        result.push_back(brgzDone.front());
    }

    // If not isHirzel, emit prf + wprf sequence
    if (!isHirzel) {
        // 0x1de581..0x1de739
        // Look up cachePtr->size / 2 for prf
        auto it = nodeStates_.find(node);
        auto* cachePtr = reinterpret_cast<Cache::Pointer*>(it->second.cachePtr.get());
        uint32_t halfSize = cachePtr->size_ / 2;

        AsmList prfAsm = asmCommands_->prf(hirzelReg, cervinoReg, static_cast<int>(halfSize));
        result.push_back(prfAsm.front());

        AsmList wprfAsm = asmCommands_->wprf();
        result.push_back(wprfAsm.front());
    }

    if (remainder != 0) {
        // 0x1de748: emit brgz(counterReg, lastLabel, false)  — branch back for more pages
        AsmList brgzLast = asmCommands_->brgz(counterReg, lastLabel, false);
        result.push_back(brgzLast.front());
    }

    // ---- Step 18: Emit brz(AsmRegister(0), playLabel, false) — skip if zero ----
    // 0x1de81f..0x1de8ee
    {
        AsmRegister zeroReg(0);  // -0x188(%rbp)
        AsmList brzAsm = asmCommands_->brz(zeroReg, playLabel, false);
        result.push_back(brzAsm.front());
    }

    // ---- Step 19: Handle remainder — second insertPlay call ----
    // 0x1de8fa..0x1de96d
    if (remainder != 0) {
        // 0x1de900..0x1de96d
        bool indexed2 = raw->config.now;  // +0x64 = config+0x1C  confirmed
        bool isHirzel2 = config_->isHirzel;

        AsmRegister playReg2 = isHirzel2 ? hirzelReg : cervinoReg;

        auto it = nodeStates_.find(node);
        auto* cachePtr = reinterpret_cast<Cache::Pointer*>(it->second.cachePtr.get());
        uint32_t position = cachePtr->position_;  // confirmed Cache::Pointer+0x08

        // 0x1de96d: call insertPlay(result, indexed2, lastLabel, playReg2, remainder, position)  confirmed
        insertPlay(result, indexed2, lastLabel, playReg2, remainder, position);
    }

    // ---- Step 20: Emit "done" label ----
    // 0x1de972..0x1dea26
    {
        AsmList labelAsm = asmCommands_->asmLabel(doneLabel);
        result.push_back(labelAsm.front());
    }

    // ---- Cleanup and return ----
    // 0x1dea32..0x1deb66: destroy labels, shared_ptrs, return result
    return result;
}

} // namespace zhinst
