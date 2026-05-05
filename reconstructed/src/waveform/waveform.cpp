// ============================================================================
// Reconstructed from disassembly of _seqc_compiler.so
// Waveform base class — ALL method implementations
//
// Source path (from debug strings):
//   /builds/labone/labone/ziAWG/ziAWGUtils/src/main/include/Waveform
// ============================================================================

#include "zhinst/waveform/waveform_front.hpp"
#include "zhinst/device/device_constants.hpp"

#include <boost/json.hpp>
#include <cstring>
#include <unordered_map>

namespace zhinst {

// ============================================================================
// 1. Waveform::toJson() const
// Binary address: 0x2a33c0 – 0x2a3a06
// Mangled: _ZNK6zhinst8Waveform6toJsonEv
// Calling convention: sret (rdi=return json::value, rsi=this)
//
// Constructs a boost::json::value from an initializer_list of 12 key-value
// pairs. The initializer list is built on the stack using boost::json::value_ref
// pairs, then passed to json::value(initializer_list, storage_ptr).
//
// Field accesses on this (rsi):
//   [this+0x00] name            — string, key "name" (len 4, at 0x904d94)
//   [this+0x18] waveformType    — int, converted via File::typeToStr, key "type" (len 4, at 0x906186)
//   [this+0x20] functionArgs   — string, key "functionArgs" (len 12, at 0x90a35a)
//   [this+0x38] file            — shared_ptr<File>, file->name or "", key "fileName" (len 8, at 0x90a367)
//   [this+0x48] used            — bool, key "load" (len 4, at 0x9060f4)
//   [this+0x4C] addressValue    — AddressImpl<uint>, key "globalAddress" (len 13, at 0x90a370)
//   [this+0x50] funDescrName     — string, key "genFunc" (len 7, at 0x90a37e)
//   [this+0x68] playConfig        — uint32_t→int, key "playConfig" (len 10, at 0x90a3ad)
//   [this+0x6C] waveIndex       — int32_t, key "waveIndex" (len 9, at 0x90a350)
//   [this+0x70] minLengthSamples     — int, key "minLengthSamples" (len 16, at 0x90a386)
//   [this+0x74] allocationByteSize         — int, key "allocationSize" (len 14, at 0x90a397)
//   [this+0x80] signal          — Signal, key "signal" (len 6, at 0x90a3a6)
//
// Serialization order (from initializer_list construction, 12 pairs = edx=0xc):
//   {"name", name}
//   {"type", typeToStr(waveformType)}
//   {"functionArgs", functionArgs}
//   {"fileName", file ? file->name : ""}
//   {"load", used}
//   {"globalAddress", addressValue}
//   {"genFunc", funDescrName}
//   {"waveIndex", waveIndex}
//   {"minLengthSamples", minLengthSamples}
//   {"allocationSize", allocationByteSize}
//   {"signal", signal.toJson()}
//   {"playConfig", playConfig}
// ============================================================================
boost::json::value Waveform::toJson() const  // 0x2a33c0
{
    // 0x2a3470: mov esi, [rsi+0x18]  — load waveformType
    // 0x2a3477: call 2a3a90           — File::typeToStr(waveformType)
    std::string typeStr = File::typeToStr(waveformType);

    // 0x2a3569: mov rax, [r14+0x38]  — load file.get()
    // 0x2a356d: test rax, rax         — null check
    // 0x2a3570: je (empty string)
    // 0x2a3572: test BYTE [rax], 0x1  — SSO check on file->name
    std::string fileName;
    if (file) {
        // File's name is at file+0x00 (the object pointed to by shared_ptr)
        // file.get() returns the managed File*, whose first field is a string
        fileName = std::string(file->name);  // deep copy of file->name
    }
    // else: fileName remains empty (0x2a3588: mov WORD [rbp-0x40], 0x0)

    // 0x2a38b6: lea rsi, [r14+0x80]  — &this->signal
    // 0x2a38c4: call 2a3e40           — Signal::toJson()
    boost::json::value signalJson = signal.toJson();

    // 0x2a3977: mov QWORD [rbp-0x48], 0  — null storage_ptr
    // 0x2a398a: mov edx, 0xc              — 12 pairs
    // 0x2a3992: call 349550               — json::value(initializer_list, storage_ptr)
    return boost::json::value{
        {"name",             name},              // +0x00, key at 0x904d94
        {"type",             typeStr},           // +0x18 (converted), key at 0x906186
        {"functionArgs",     functionArgs},     // +0x20, key at 0x90a35a
        {"fileName",         fileName},          // +0x38, key at 0x90a367
        {"load",             used},              // +0x48, key at 0x9060f4
        {"globalAddress",    addressValue},      // +0x4C, key at 0x90a370
        {"genFunc",          funDescrName},       // +0x50, key at 0x90a37e
        {"waveIndex",        waveIndex},         // +0x6C, key at 0x90a350
        {"minLengthSamples", minLengthSamples},       // +0x70, key at 0x90a386
        {"allocationSize",   allocationByteSize},           // +0x74, key at 0x90a397
        {"signal",           std::move(signalJson)}, // +0x80, key at 0x90a3a6
        {"playConfig",       static_cast<int>(playConfig)}  // +0x68, key at 0x90a3ad
    };

    // Cleanup: destroys signalJson, fileName string, typeStr string
    // 0x2a39c1: call 348f10  — json::value::~value() for signalJson
    // 0x2a39d8: call 8c4e80  — dealloc fileName if heap
    // 0x2a39ef: call 8c4e80  — dealloc typeStr if heap
}

// ============================================================================
// 2. Waveform::fromJson(boost::json::value const& json, DeviceConstants const& dc)
// Binary address: 0x2a54b0 – 0x2a61d3
// Mangled: _ZN6zhinst8Waveform8fromJsonERKN5boost4json5valueERKNS_15DeviceConstantsE
// Calling convention: sret (rdi=return Waveform, rsi=json, rdx=dc)
//
// Static method. Deserializes a Waveform from JSON.
// Uses source_location for error reporting (file at 0x90a493, func at 0x90a4d8).
//
// Read order (each key is looked up via as_object().at(key)):
//   1. "name"             → as_string → construct std::string (name)
//   2. "type"             → as_string → construct std::string → typeFromStr (fileType)
//   3. "functionArgs"     → as_string → construct std::string (functionArgs)
//   4. "fileName"         → as_string → if non-empty, make_shared<File>(c_str)
//   5. "load"             → as_bool (used)
//   6. "globalAddress"    → to_number<uint64_t> (addressValue)
//   7. "genFunc"          → as_string → construct std::string (funDescrName)
//   8. "playConfig"       → as_int64 (playConfig)
//   9. "waveIndex"        → as_int64 (waveIndex)
//  10. "minLengthSamples" → as_int64 (minLengthSamples)
//  11. "allocationSize"   → as_int64 (allocationByteSize)
//  12. "signal"           → Signal::fromJson (signal)
//
// Then calls the full Waveform constructor at 0x2a71e0 with all 13 parameters
// (name, fileType, functionArgs, filePtr, used, addressValue, genFunc,
//  playConfig, waveIndex, minLengthSamples, allocationByteSize, dc, signal)
// on the stack (8 pushes for params beyond register capacity).
// ============================================================================
Waveform Waveform::fromJson(boost::json::value const& json,
                             DeviceConstants const& dc)  // 0x2a54b0
{
    // r15 = &json (preserved across calls)
    // Store dc pointer at [rbp-0x100] for later

    // --- 1. Read "name" ---
    // 0x2a5651: call 34b170 (as_object)
    // 0x2a556f: call 33fa40 (object.at("name", 4))  — key "name" at 0x904d94
    // 0x2a5574: call 34b290 (as_string)
    // Result: json string → extract c_str via tag check (0x85 = inline)
    // Then construct std::string at [rbp-0xa0] from c_str + strlen
    auto const& obj = json.as_object();
    auto const& nameJsonStr = obj.at("name").as_string();

    // Binary reads raw SSO internals at 0x2a5686, but semantically just
    // extracts the C string from the JSON string value.
    std::string nameStr(nameJsonStr.data(), nameJsonStr.size());  // 0x2a5691

    // --- 2. Read "type" ---
    // 0x2a5754: call 34b170 (as_object with source_location)
    // 0x2a576f: call 33fa40 (object.at("functionArgs", 12)) — WAIT, this is "functionArgs"
    // Actually re-reading: first is "name", then:
    // 0x2a5759: lea rsi, [rip+0x664bfa] → 0x90a35a = "functionArgs"
    // 0x2a5767: mov edx, 0xc (len=12)
    // ... then 0x2a579d: call 34b290 (as_string)
    // This reads "functionArgs" → functionArgs
    //
    // Let me re-trace. After "name", the code reads:
    //   at 0x2a5712: call 2a63c0 (typeFromStr) on the string just built
    //     → That was the "type" field read BEFORE the name!
    //
    // Actually from the start of fromJson:
    //   0x2a54b0-0x2a5650: prologue, store dc, get json as_object
    //   First at() call uses key at 0x904d94 = "name", len 4
    //   → as_string → extract c_str → build string [rbp-0x60..] (the "name" local)
    //
    // Hmm, let me re-read more carefully. The key string pointers tell the story:
    //   0x904d94 = "name"
    //   0x906186 = "type"
    //   0x90a35a = "functionArgs"
    //   0x90a367 = "fileName"
    //   0x9060f4 = "load"
    //   0x90a370 = "globalAddress"
    //   0x90a37e = "genFunc"
    //   0x90a350 = "waveIndex"
    //   0x90a386 = "minLengthSamples"
    //   0x90a397 = "allocationSize"
    //   0x90a3a6 = "signal"
    //   0x90a3ad = "playConfig"
    //
    // From the disassembly at 0x2a5600ish, the first .at() call is key 0x904d94="name"
    // The result string is stored, then at 0x2a5712 typeFromStr is called on it? No—
    // wait, typeFromStr is called on [rbp-0x88] which was built from a DIFFERENT string.
    //
    // Let me just follow the logical flow based on the key references in order:

    // Read "name" key (len 4) → as_string → build string → store as nameStr
    // Read "type" key (not visible in truncated output but implied by typeFromStr call)
    //   Actually at 0x2a5712 we see call to typeFromStr on [rbp-0x88]
    //   That string was built at 0x2a5696+ from an as_string result
    //   The key for that must be from the first .at() which loaded "name" len 4...
    //
    // I'll trust the semantic reconstruction. The function reads these keys:

    // Read "name" → string for Waveform::name
    // (Implementation details: extracts c_str from json::string, builds std::string)

    // Read "type" → string → typeFromStr → File::Type
    auto const& typeJsonStr = obj.at("type").as_string();
    const char* typeCStr = typeJsonStr.data();
    std::string typeLocalStr(typeCStr, strlen(typeCStr));
    File::Type fileType = File::typeFromStr(std::move(typeLocalStr));
    // 0x2a571e: store result at [rbp-0xa4] (mov DWORD)

    // --- 3. Read "functionArgs" ---
    // 0x2a5759: key at 0x90a35a "functionArgs", len 12
    // 0x2a579d: as_string
    auto const& funcArgsJsonStr = obj.at("functionArgs").as_string();
    const char* funcArgsCStr = funcArgsJsonStr.data();
    std::string functionArgsStr(funcArgsCStr, strlen(funcArgsCStr));
    // stored at [rbp-0x70]

    // --- 4. Read "fileName" ---
    // 0x2a5870: key at 0x90a367 "fileName", len 8
    // 0x2a58b4: as_string
    auto const& fileNameJsonStr = obj.at("fileName").as_string();

    // 0x2a58b9: cmp BYTE [rax+0x8], 0x5  — check if string is short-inline (len stored in tag)
    // Determine length: if inline, len = 14 - tag_byte; else len = [rax+0x10]->size field
    // 0x2a58c7-0x2a58d3: extract length, store at [rbp-0xd0]
    // 0x2a58d3: je 2a59cb (if length == 0, skip file creation)
    // Binary reads SSO internals at 0x2a588f to compute length, but
    // semantically just checks if the filename string is non-empty.
    size_t fileNameLen = fileNameJsonStr.size();

    std::shared_ptr<File> filePtr;
    if (fileNameLen != 0) {
        // 0x2a58d9-0x2a5902: setup source_location, call as_object again
        // 0x2a5907: key "fileName" again, len 8
        // 0x2a594b: as_string again → get c_str
        // 0x2a5967: store filename c_str pointer at [rbp-0x110]
        const char* fileNameCStr = fileNameJsonStr.data();

        // 0x2a596e: mov edi, 0x58 — allocate 0x58 bytes (shared_ptr_emplace<File>)
        // 0x2a5973: call 8c6b30 (operator new)
        // 0x2a5984: set vtable for __shared_ptr_emplace<File>
        // 0x2a599f: call 2a7ff0 — construct_at<File>(ptr, fileNameCStr)
        // 0x2a59a4-0x2a59a8: store as shared_ptr (obj at +0x18 from alloc, ctrl at alloc)
        filePtr = std::make_shared<File>(fileNameCStr);
    }
    // else: filePtr remains null (0x2a59cb: xorpd, movapd → zero pair)
    // stored at [rbp-0x40] (ptr, ctrl_block)

    // --- 5. Read "load" ---
    // 0x2a59f3-0x2a5a02: as_object with source_location
    // 0x2a5a02: key at 0x9060f4 "load", len 4
    // 0x2a5a18: object.at("load")
    // 0x2a5a46: call 34b710 (as_bool)
    bool usedVal = obj.at("load").as_bool();
    // 0x2a5a4b: mov r14d, eax — store bool result
    // Later stored at [rbp-0x29]

    // --- 6. Read "globalAddress" ---
    // 0x2a5a6d-0x2a5a77: as_object
    // 0x2a5a7c: key at 0x90a370 "globalAddress", len 13
    // 0x2a5a92: object.at("globalAddress")
    // 0x2a5a97-0x2a5b6c: to_number<uint64_t> inline expansion
    //   Checks value type (0x04=double, 0x03=int64, 0x02=uint64)
    //   For double: range-check [0, UINT64_MAX], convert via cvttsd2si with overflow handling
    //   For int64: if negative → error 0x1b; else cast
    //   For uint64: direct use
    //   On error: constructs error_code with json category, throws
    // 0x2a5b6c: store result at [rbp-0xf8]
    auto const& addrJson = obj.at("globalAddress");
    boost::system::error_code ec;
    uint64_t addrVal = addrJson.to_number<uint64_t>(ec);
    if (ec) {
        // 0x2a5b79-0x2a5be0: construct source_location and throw
        boost::system::throw_exception_from_error(ec, BOOST_CURRENT_LOCATION);
    }
    // stored at [rbp-0xf8] as uint64_t, later truncated to uint32_t

    // --- 7. Read "genFunc" ---
    // 0x2a5bf5-0x2a5c1e: as_object
    // 0x2a5c23: key at 0x90a37e "genFunc", len 7
    // 0x2a5c39: object.at("genFunc")
    // 0x2a5c67: as_string → extract c_str → build std::string at [rbp-0x58]
    auto const& genFuncJsonStr = obj.at("genFunc").as_string();
    const char* genFuncCStr = genFuncJsonStr.data();
    std::string genFuncStr(genFuncCStr, strlen(genFuncCStr));

    // --- 8. Read "playConfig" ---
    // 0x2a5d0c-0x2a5d35: as_object
    // 0x2a5d3a: key at 0x90a3ad "playConfig", len 10
    // 0x2a5d50: object.at("playConfig")
    // 0x2a5d7e: call 34b3b0 (as_int64)
    int64_t playConfigVal = obj.at("playConfig").as_int64();
    // 0x2a5d83: mov r12, rax — save result

    // --- 9. Read "waveIndex" ---
    // 0x2a5d86-0x2a5daf: as_object
    // 0x2a5db4: key at 0x90a350 "waveIndex", len 9
    // 0x2a5dca: object.at("waveIndex")
    // 0x2a5df8: call 34b3b0 (as_int64)
    int64_t waveIndexVal = obj.at("waveIndex").as_int64();
    // 0x2a5dfd: mov rbx, rax

    // --- 10. Read "minLengthSamples" ---
    // 0x2a5e00-0x2a5e29: as_object
    // 0x2a5e2e: key at 0x90a386 "minLengthSamples", len 16
    // 0x2a5e44: object.at("minLengthSamples")
    // 0x2a5e72: call 34b3b0 (as_int64)
    int64_t minLengthVal = obj.at("minLengthSamples").as_int64();
    // 0x2a5e77-0x2a5e7a: store, swap into r13

    // --- 11. Read "allocationSize" ---
    // 0x2a5e7d-0x2a5ea6: as_object
    // 0x2a5eab: key at 0x90a397 "allocationSize", len 14
    // 0x2a5ec1: object.at("allocationSize")
    // 0x2a5ec6: store minLengthVal at [rbp-0xf0]
    // Then: to_number<int> inline expansion (similar pattern to globalAddress)
    //   Handles double→int conversion with range checking
    auto const& allocJson = obj.at("allocationSize");
    int64_t allocSizeVal;
    {
        boost::system::error_code ec2;
        allocSizeVal = allocJson.to_number<int64_t>(ec2);
        if (ec2) {
            boost::system::throw_exception_from_error(ec2, BOOST_CURRENT_LOCATION);
        }
    }

    // --- 12. Read "signal" ---
    // 0x2a603c-0x2a6073: as_object
    // 0x2a6078: key at 0x90a3a6 "signal", len 6
    // 0x2a608e: object.at("signal")
    // 0x2a60a0: call 2a65d0 — Signal::fromJson(json_value)
    // Result stored at [rbp-0x378] (Signal object, contains 3 vectors)
    Signal sig = Signal::fromJson(obj.at("signal"));  // 0x2a60a0

    // --- Construct Waveform ---
    // 0x2a60a5-0x2a60e7: Push parameters onto stack for the full constructor
    // Stack layout for call at 0x2a60e7:
    //   rdi = return ptr (sret, stored at [rbp-0x108])
    //   rsi = &nameStr (at [rbp-0xa0])     — name (string, moved)
    //   edx = fileType                      — [rbp-0xa4]
    //   rcx = &functionArgsStr (at [rbp-0x70])  — functionArgs (string, moved)
    //   r8  = &filePtr (at [rbp-0x40])      — shared_ptr<File> (moved)
    //   r9d = usedVal                       — [rbp-0x29]
    //   [rsp+0x00] = addrVal               — [rbp-0xf8] (uint64 → uint32)
    //   [rsp+0x08] = &genFuncStr           — [rbp-0x58] (string, moved)
    //   [rsp+0x10] = playConfigVal (r12)   — playConfig (int)
    //   [rsp+0x18] = waveIndexVal (r13)    — waveIndex (int)
    //   [rsp+0x20] = minLengthVal          — [rbp-0xf0] (minLengthSamples, int)
    //   [rsp+0x28] = allocSizeVal (r12_saved) — allocationByteSize (int)
    //   [rsp+0x30] = &dc                   — [rbp-0x100] (DeviceConstants const&)
    //   [rsp+0x38] = &sig                  — [rbp-0x378] (Signal, moved)
    //
    // 0x2a60e7: call 2a71e0 — Waveform full constructor
    Waveform result(
        std::move(nameStr),                         // rsi → name
        fileType,                                    // edx → waveformType (+0x18)
        std::move(functionArgsStr),                // rcx → functionArgs (+0x20)
        std::move(filePtr),                         // r8  → file (+0x38)
        usedVal,                                    // r9  → used (+0x48)
        static_cast<uint32_t>(addrVal),             // stack → addressValue (+0x4C)
        std::move(genFuncStr),                      // stack → funDescrName (+0x50)
        static_cast<int>(playConfigVal),            // stack → playConfig (+0x68)
        static_cast<int>(waveIndexVal),             // stack → waveIndex (+0x6C)
        static_cast<int>(minLengthVal),             // stack → minLengthSamples (+0x70)
        static_cast<int>(allocSizeVal),             // stack → allocationByteSize (+0x74)
        dc,                                          // stack → deviceConstants (+0x78)
        std::move(sig)                              // stack → signal (+0x80)
    );

    // 0x2a60ec-0x2a6156: Cleanup of local temporaries
    //   - Signal::fromJson result vectors (samples, markers, markerBits)
    //   - genFuncStr (if heap-allocated)
    //   - shared_ptr<File> release
    //   - functionArgsStr (if heap-allocated)
    //   - nameStr (if heap-allocated)

    return result;  // 0x2a61bf-0x2a61d3: return and epilogue
}

// ============================================================================
// 3. Waveform::getSizePerDevice() const
// Binary address: 0x1d5c30 – 0x1d5c87
// Mangled: _ZNK6zhinst8Waveform16getSizePerDeviceEv
// Calling convention: rdi=this, returns uint32_t in eax
//
// Field accesses:
//   0x1d5c34: movzx ecx, WORD [rdi+0xC8]   — signal.channels_ (uint16_t)
//   0x1d5c3b: mov eax, DWORD [rdi+0xD0]    — signal.length_ (low 32 bits)
//   0x1d5c41: mov rsi, QWORD [rdi+0x78]    — deviceConstants ptr
//   0x1d5c49: mov edi, DWORD [rsi+0x40]    — dc->minWaveformLength
//   0x1d5c4c: mov r8d, DWORD [rsi+0x44]    — dc->maxWaveformLength
//   0x1d5c6c: movsxd rcx, DWORD [rsi+0x50] — dc->bitsPerSample (sign-extended)
//
// Algorithm:
//   if (length == 0) return 0;
//   rounded = ceil(length / granularity) * granularity;
//   rounded = max(rounded, minWaveformLength);
//   totalBits = (uint64_t)rounded * channels * bitsPerSample;
//   return ceil(totalBits / 8);
// ============================================================================
uint32_t Waveform::getSizePerDevice() const  // 0x1d5c30
{
    uint16_t channels = signal.channels_;           // 0x1d5c34: [rdi+0xC8]
    uint32_t length = static_cast<uint32_t>(signal.length_);     // 0x1d5c3b: [rdi+0xD0]
    const DeviceConstants* dc = deviceConstants;      // 0x1d5c41: [rdi+0x78]

    // 0x1d5c45: test eax, eax
    // 0x1d5c47: je → return 0
    if (length == 0)
        return 0;                                   // 0x1d5c66: xor eax,eax; jmp epilog

    uint32_t minLen = dc->maxWaveformLength;       // 0x1d5c49: [rsi+0x40]
    uint32_t granularity = dc->grainSize;     // 0x1d5c4c: [rsi+0x44]

    // 0x1d5c50: xor edx, edx
    // 0x1d5c52: div r8d          — length / granularity
    // 0x1d5c55: cmp edx, 1       — check remainder
    // 0x1d5c58: sbb eax, -1      — if remainder>0, quotient+1 (i.e. ceil division)
    // 0x1d5c5b: imul eax, r8d    — * granularity
    uint32_t quotient = length / granularity;
    uint32_t remainder = length % granularity;
    uint32_t rounded = (quotient + (remainder > 0 ? 1 : 0)) * granularity;

    // 0x1d5c5f: cmp edi, eax     — compare minLen to rounded
    // 0x1d5c61: cmova eax, edi   — if minLen > rounded, use minLen
    if (minLen > rounded)
        rounded = minLen;

    // 0x1d5c68: imul rax, rcx    — rounded * channels (zero-extended)
    uint64_t sizeTimesChannels = static_cast<uint64_t>(rounded) * static_cast<uint64_t>(channels);

    // 0x1d5c6c: movsxd rcx, DWORD [rsi+0x50] — bitsPerSample (sign-extended to 64-bit)
    int32_t bitsPerSample = dc->bitsPerSample;

    // 0x1d5c70: imul rcx, rax    — totalBits = sizeTimesChannels * bitsPerSample
    uint64_t totalBits = sizeTimesChannels * static_cast<int64_t>(bitsPerSample);

    // 0x1d5c74: mov eax, ecx     — low 32 bits of totalBits
    // 0x1d5c76: and eax, 0x7     — remainder mod 8
    // 0x1d5c79: shr rcx, 3       — totalBits / 8
    // 0x1d5c7d: cmp rax, 1       — if remainder >= 1
    // 0x1d5c81: sbb ecx, -1      — bytes + 1 (ceiling)
    uint32_t bitRemainder = static_cast<uint32_t>(totalBits) & 0x7;
    uint32_t bytes = static_cast<uint32_t>(totalBits >> 3);
    if (bitRemainder >= 1)
        bytes += 1;

    // 0x1d5c84: mov eax, ecx
    // 0x1d5c86: pop rbp; ret
    return bytes;
}

// ============================================================================
// 4. Waveform::operator==(Waveform const& other) const
// Binary address: 0x2a9510 – 0x2a967e
// Mangled: _ZNK6zhinst8WaveformeqERKS0_
// Calling convention: rdi=this, rsi=other, returns bool in al
//
// Comparison order (each mismatch returns false immediately):
//   0x2a951d: [rdi+0x00] name == [rsi+0x00] other.name           (string ==)
//   0x2a958d: [rdi+0x18] waveformType == [rsi+0x18]              (DWORD ==)
//   0x2a9596: [rdi+0x20] functionArgs == [rsi+0x20]             (string ==)
//   0x2a95fd: [rdi+0x38] file vs [rsi+0x38]                      (ptr null/non-null then File::==)
//   0x2a961f: [rdi+0x48] used == [rsi+0x48]                      (BYTE ==)
//   0x2a9629: [rdi+0x4C] addressValue == [rsi+0x4C]              (DWORD ==)
//   0x2a9632: [rdi+0x50] funDescrName == [rsi+0x50]               (string ==)
//   0x2a9643: [rdi+0x68] playConfig == [rsi+0x68]                  (DWORD ==)
//   0x2a964c: [rdi+0x6C] waveIndex == [rsi+0x6C]                 (DWORD ==)
//   0x2a9655: [rdi+0x70] minLengthSamples == [rsi+0x70]               (DWORD ==)
//   0x2a965e: [rdi+0x74] allocationByteSize == [rsi+0x74]                   (DWORD ==)
//   0x2a9667: [rdi+0x80] signal == [rsi+0x80]                    (tail-call Signal::operator==)
// ============================================================================
bool Waveform::operator==(Waveform const& other) const  // 0x2a9510
{
    // r14 = this, rbx = &other

    // --- Compare name (string at +0x00) ---
    // 0x2a951d-0x2a958b: standard libc++ SSO string comparison
    //   Check lengths first (from SSO tag or heap size field)
    //   If equal length, bcmp the contents
    if (name != other.name)                         // 0x2a956d/0x2a958b: test/jne → 2a95f6
        return false;

    // --- Compare waveformType (int at +0x18) ---
    // 0x2a958d: mov eax, [r14+0x18]; cmp eax, [rbx+0x18]
    if (waveformType != other.waveformType)         // 0x2a9594: jne → 2a95f6
        return false;

    // --- Compare functionArgs (string at +0x20) ---
    // 0x2a9596-0x2a95f4: same pattern as name comparison
    if (functionArgs != other.functionArgs)
        return false;

    // --- Compare file (shared_ptr<File> at +0x38) ---
    // 0x2a95fd: mov rdi, [r14+0x38]   — this->file.get()
    // 0x2a9601: mov rsi, [rbx+0x38]   — other.file.get()
    // 0x2a9605: test rdi, rdi          — this->file null?
    // 0x2a9608: je → check if other also null
    // 0x2a960a: test rsi, rsi          — other null? if so return false
    // 0x2a960f: call 2a9680            — File::operator==(*file, *other.file)
    // 0x2a9614: test al, al            — if not equal, return false
    // 0x2a961a: (else path) test rsi, rsi — if this null but other not, return false
    if (file) {
        if (!other.file)
            return false;                           // 0x2a960d: je → 2a95f6
        if (!(*file == *other.file))                // 0x2a960f: call 2a9680
            return false;                           // 0x2a9616: je → 2a95f6
    } else {
        if (other.file)
            return false;                           // 0x2a961d: jne → 2a95f6
    }

    // --- Compare used (bool at +0x48) ---
    // 0x2a961f: movzx eax, BYTE [r14+0x48]; cmp al, BYTE [rbx+0x48]
    if (used != other.used)                         // 0x2a9627: jne → 2a95f6
        return false;

    // --- Compare addressValue (uint32_t at +0x4C) ---
    // 0x2a9629: mov eax, [r14+0x4C]; cmp eax, [rbx+0x4C]
    if (addressValue != other.addressValue)         // 0x2a9630: jne → 2a95f6
        return false;

    // --- Compare funDescrName (string at +0x50) ---
    // 0x2a9632: lea rdi, [r14+0x50]; lea rsi, [rbx+0x50]
    // 0x2a963a: call 2a0680 (string operator==)
    if (funDescrName != other.funDescrName)           // 0x2a9641: je → 2a95f6
        return false;

    // --- Compare playConfig (uint32_t at +0x68) ---
    // 0x2a9643: mov eax, [r14+0x68]; cmp eax, [rbx+0x68]
    if (playConfig != other.playConfig)                 // 0x2a964a: jne → 2a95f6
        return false;

    // --- Compare waveIndex (int32_t at +0x6C) ---
    // 0x2a964c: mov eax, [r14+0x6C]; cmp eax, [rbx+0x6C]
    if (waveIndex != other.waveIndex)               // 0x2a9653: jne → 2a95f6
        return false;

    // --- Compare minLengthSamples (int at +0x70) ---
    // 0x2a9655: mov eax, [r14+0x70]; cmp eax, [rbx+0x70]
    if (minLengthSamples != other.minLengthSamples)           // 0x2a965c: jne → 2a95f6
        return false;

    // --- Compare allocationByteSize (int at +0x74) ---
    // 0x2a965e: mov eax, [r14+0x74]; cmp eax, [rbx+0x74]
    if (allocationByteSize != other.allocationByteSize)                   // 0x2a9665: jne → 2a95f6
        return false;

    // --- Compare signal (Signal at +0x80, 0x58 bytes) ---
    // 0x2a9667: sub r14, -0x80   — r14 = &this->signal (this+0x80)
    // 0x2a966b: sub rbx, -0x80   — rbx = &other.signal (other+0x80)
    // 0x2a966f: mov rdi, r14
    // 0x2a9672: mov rsi, rbx
    // 0x2a9679: jmp 2a9750        — tail-call Signal::operator==
    return signal == other.signal;                  // tail-call to 0x2a9750
}

// ============================================================================
// 5. Waveform::Waveform(shared_ptr<Waveform> source, string newName)
// Binary address: 0x114f10 – 0x115006
// Mangled: _ZN6zhinst8WaveformC2ENSt3__110shared_ptrIS0_EENS1_12basic_stringIcNS1_11char_traitsIcEENS1_9allocatorIcEEEE
// Calling convention: rdi=this, rsi=shared_ptr<Waveform>*, rdx=string*
//
// "Copy-rename" constructor. Takes ownership of newName (moved), copies
// all other fields from *source.
//
// Instruction-level field assignments:
//   0x114f21-0x114f3c: move newName (rdx) into this->name (+0x00)
//                      (SSO: movups 16 bytes + mov qword; then zero source)
//   0x114f3d: mov rax, [rsi]       — source.get() (dereference shared_ptr)
//   0x114f40: mov ecx, [rax+0x18]  — src->waveformType
//   0x114f43: mov [rdi+0x18], ecx  — this->waveformType = src->waveformType
//   0x114f46-0x114f73: copy src->functionArgs to this->functionArgs (+0x20)
//                      (SSO path vs heap path via __init_copy_ctor_external)
//   0x114f75: mov rax, [r15]       — re-dereference source.get()
//   0x114f78-0x114f89: copy src->file (shared_ptr at +0x38)
//                      movups 16 bytes (ptr + ctrl), lock inc refcount
//   0x114f91: movzx ecx, BYTE [rax+0x48] — src->used
//   0x114f95: mov [rbx+0x48], cl         — this->used
//   0x114f98: mov ecx, [rax+0x4C]        — src->addressValue
//   0x114f9b: mov [rbx+0x4C], ecx        — this->addressValue
//   0x114f9e-0x114fcd: copy src->funDescrName to this->funDescrName (+0x50)
//   0x114fcf: mov rsi, [r15]       — re-dereference source.get()
//   0x114fd2: mov rax, [rsi+0x68]  — src->playConfig+waveIndex as qword
//   0x114fd6: mov [rbx+0x68], rax  — this->playConfig+waveIndex
//   0x114fda: mov eax, [rsi+0x70]  — src->minLengthSamples
//   0x114fdd: mov [rbx+0x70], eax  — this->minLengthSamples
//   0x114fe0: mov eax, [rsi+0x74]  — src->allocationByteSize
//   0x114fe3: mov [rbx+0x74], eax  — this->allocationByteSize
//   0x114fe6: mov rax, [rsi+0x78]  — src->deviceConstants
//   0x114fea: mov [rbx+0x78], rax  — this->deviceConstants
//   0x114fee: lea rdi, [rbx+0x80]  — &this->signal
//   0x114ff5: sub rsi, -0x80       — &src->signal (rsi was src, +0x80)
//   0x114ff9: call 1150e0          — Signal::Signal(Signal const&) copy ctor
// ============================================================================
Waveform::Waveform(std::shared_ptr<Waveform> source, std::string newName)  // 0x114f10
{
    // r15 = &source (shared_ptr on stack), rbx = this

    // Move newName into this->name
    // 0x114f21-0x114f3c: memcpy 24 bytes from rdx to rdi, zero rdx
    name = std::move(newName);                      // +0x00

    Waveform* src = source.get();                   // 0x114f3d: mov rax, [rsi]

    // 0x114f40-0x114f43
    waveformType = src->waveformType;               // +0x18 ← [src+0x18]

    // 0x114f46-0x114f73: deep copy string
    functionArgs = src->functionArgs;             // +0x20 ← [src+0x20]

    // 0x114f75-0x114f89: copy shared_ptr (16 bytes) + atomic refcount inc
    file = src->file;                               // +0x38 ← [src+0x38]

    // Re-dereference after potential realloc
    src = source.get();                             // 0x114f8e: mov rax, [r15]

    // 0x114f91-0x114f9b
    used = src->used;                               // +0x48 ← [src+0x48]
    addressValue = src->addressValue;               // +0x4C ← [src+0x4C]

    // 0x114f9e-0x114fcd: deep copy string
    funDescrName = src->funDescrName;                 // +0x50 ← [src+0x50]

    // Re-dereference
    src = source.get();                             // 0x114fcf: mov rsi, [r15]

    // 0x114fd2-0x114fd6: copy 8 bytes (playConfig + waveIndex together)
    playConfig = src->playConfig;                       // +0x68 ← [src+0x68]
    waveIndex = src->waveIndex;

    // 0x114fda-0x114fea
    minLengthSamples = src->minLengthSamples;
    allocationByteSize = src->allocationByteSize;
    deviceConstants = src->deviceConstants;

    // 0x114fee-0x114ff9: copy-construct Signal via placement new +
    // explicit Signal::Signal(const&) call (not a bitwise copy — vtable safe)
    // lea rdi, [rbx+0x80]; sub rsi, -0x80; call Signal::Signal(const&)
    new (&signal) Signal(src->signal);              // +0x80
}

// ============================================================================
// 6. Waveform::File::typeToStr(File::Type type)
// Binary address: 0x2a3a90 – 0x2a3c3a
// Mangled: _ZN6zhinst8Waveform4File9typeToStrENS1_4TypeE
// Calling convention: sret (rdi=return string, esi=Type enum value)
//
// Static method with lazy-initialized static local:
//   static unordered_map<Type, string> typeToStrMap = {
//       {0, "csv"}, {1, "raw"}, {2, "gen"}
//   };
//
// Guard variable at 0xb84c78 (_ZGVZ...typeToStrMap)
// Map data at 0xb84c50 (_ZZ...typeToStrMap)
//
// On first call:
//   0x2a3c64: lea rdi, [guard]; call __cxa_guard_acquire
//   0x2a3c7e-0x2a3cee: construct 3 pairs on stack, call unordered_map ctor
//     pair 1: {0, "csv"} — key at [rbp-0x2c]=0, value at 0x90a3b8
//     pair 2: {1, "raw"} — key at [rbp-0x28]=1, value at 0x90a3bc
//     pair 3: {2, "gen"} — key at [rbp-0x24]=2, value at 0x90a3c0
//   0x2a3d1c-0x2a3d3d: register atexit destructor, __cxa_guard_release
//
// On subsequent calls:
//   0x2a3ab1: load map size [rip+0x8e11a0] → r8
//   0x2a3ac1-0x2a3c19: hash-table lookup (open addressing with linked chains)
//     Hash = type value, bucket = hash % capacity
//     Walk chain comparing: [node+0x08]==hash && [node+0x10]==type
//   0x2a3bb7-0x2a3bd1: found → copy string from [node+0x18] to return value
//   0x2a3c58: not found → throw out_of_range
// ============================================================================
std::string Waveform::File::typeToStr(Waveform::File::Type type)  // 0x2a3a90
{
    // 0x2a3aa2: movzx eax, BYTE [guard] — check if initialized
    // 0x2a3aa9: test al, al; je → first-time init path
    static std::unordered_map<Waveform::File::Type, std::string> typeToStrMap = {
        {Waveform::File::Type::CSV, "csv"},    // 0x2a3c7e: DWORD [rbp-0x2c] = 0; rdx → 0x90a3b8 "csv"
        {Waveform::File::Type::RAW, "raw"},    // 0x2a3ca3: DWORD [rbp-0x28] = 1; rdx → 0x90a3bc "raw"
        {Waveform::File::Type::GEN, "gen"},    // 0x2a3cc1: DWORD [rbp-0x24] = 2; rdx → 0x90a3c0 "gen"
    };

    // 0x2a3ac1-0x2a3c19: hash table lookup
    // 0x2a3bb7: found → copy string (SSO or heap)
    // 0x2a3c58: not found → lea rdi, [0x8ff664 "unordered_map::at..."]; call throw_out_of_range
    return typeToStrMap.at(type);
}

// ============================================================================
// 7. Waveform::File::typeFromStr(std::string str)
// Binary address: 0x2a63c0 – 0x2a6400
// Mangled: _ZN6zhinst8Waveform4File11typeFromStrENSt3__112basic_stringIcNS2_11char_traitsIcEENS2_9allocatorIcEEEE
// Calling convention: rdi=string (by value), returns int (Type enum) in eax
//
// Static method with lazy-initialized static local:
//   static unordered_map<string, Type> typeToStrMap = {
//       {"csv", 0}, {"raw", 1}, {"gen", 2}
//   };
//
// Guard variable at 0xb84ca8
// Map data at 0xb84c80
//
// On first call:
//   0x2a6401: __cxa_guard_acquire
//   0x2a6411-0x2a6481: construct 3 pairs:
//     pair 1: key from 0x90a3b8 "csv", value DWORD [rbp-0x24] = 0
//     pair 2: key from 0x90a3bc "raw", value DWORD [rbp-0x20] = 1
//     pair 3: key from 0x90a3c0 "gen", value DWORD [rbp-0x1c] = 2
//   0x2a64af-0x2a64d5: register atexit, __cxa_guard_release
//
// On subsequent calls:
//   0x2a63db: lea rdi, [map]; mov rsi, rbx (str ptr); call hash_table::find
//   0x2a63ea: test rax, rax; je → throw out_of_range
//   0x2a63f3: mov eax, [rax+0x28]  — return the Type value from found node
//             (node layout: +0x00 next, +0x08 hash, +0x10 key(string,24B), +0x28 value(int))
// ============================================================================
Waveform::File::Type Waveform::File::typeFromStr(std::string str)  // 0x2a63c0
{
    // 0x2a63d0: check guard byte
    static std::unordered_map<std::string, Waveform::File::Type> typeToStrMap = {
        {"csv", Waveform::File::Type::CSV},    // 0x2a6411: value=0, key from 0x90a3b8
        {"raw", Waveform::File::Type::RAW},    // 0x2a6436: value=1, key from 0x90a3bc
        {"gen", Waveform::File::Type::GEN},    // 0x2a6454: value=2, key from 0x90a3c0
    };

    // 0x2a63e5: call hash_table::find(str)
    // 0x2a63ea: test rax, rax — null means not found
    // 0x2a63ed: je → 0x2a64da: throw out_of_range
    // 0x2a63f3: mov eax, [rax+0x28] — extract Type from node
    return typeToStrMap.at(str);
}

// ============================================================================
// 8. Waveform::File::operator==(File const& other) const
// Binary address: 0x2a9680 – 0x2a9742
// Mangled: _ZNK6zhinst8Waveform4FileeqERKS1_
// Calling convention: rdi=this (File*), rsi=other (File const*), returns bool in al
//
// File struct layout (inferred):
//   +0x00  std::string name      (24 bytes, SSO)
//   +0x18  int32_t     formatType   (4 bytes)
//   +0x1C  int32_t     columnMode   (4 bytes)
//   +0x20  int32_t     isIntegerFormat   (4 bytes)
//   +0x24  (4 padding)
//   +0x28  void*       data_begin (vector<uint8_t>::begin)
//   +0x30  void*       data_end   (vector<uint8_t>::end)
//   +0x38  void*       data_cap   (vector<uint8_t>::capacity)
//   Total: 0x40 bytes (the managed object inside shared_ptr is at ctrl+0x18)
//
// Comparison sequence:
//   0x2a9680-0x2a96b4: compare name strings (length then content via bcmp)
//   0x2a96f7: cmp [r14+0x18], [rbx+0x18]  — formatType
//   0x2a9700: cmp [r14+0x1C], [rbx+0x1C]  — columnMode
//   0x2a9709: cmp [r14+0x20], [rbx+0x20]  — isIntegerFormat
//   0x2a9712-0x2a972d: compare vector data
//     mov rdi, [r14+0x28]; rdx = [r14+0x30] - rdi   — this data range
//     mov rsi, [rbx+0x28]; rax = [rbx+0x30] - rsi   — other data range
//     cmp rdx, rax                                    — size must match
//     0x2a972d: call bcmp                             — compare contents
//   0x2a9734: sete al                                 — return bcmp==0
// ============================================================================
bool Waveform::File::operator==(Waveform::File const& other) const  // 0x2a9680
{
    // r14 = this, rbx = &other

    // --- Compare name (string at +0x00) ---
    // 0x2a9680-0x2a96b4: standard SSO string length check
    //   If lengths differ → return false (0x2a969e: xor eax,eax; ret)
    // 0x2a96b6-0x2a96f5: bcmp contents
    //   0x2a96dd/0x2a96ee: call bcmp with length=rdx
    //   If bcmp != 0 → return false (0x2a96e4/0x2a96f5: jne → 0x2a973c)
    if (name != other.name)
        return false;

    // --- Compare formatType (int at +0x18) ---
    // 0x2a96f7: mov eax, [r14+0x18]; cmp eax, [rbx+0x18]
    // 0x2a96fe: jne → 0x2a973c
    if (formatType != other.formatType)
        return false;

    // --- Compare columnMode (int at +0x1C) ---
    // 0x2a9700: mov eax, [r14+0x1C]; cmp eax, [rbx+0x1C]
    // 0x2a9707: jne → 0x2a973c
    if (columnMode != other.columnMode)
        return false;

    // --- Compare isIntegerFormat (int at +0x20) ---
    // 0x2a9709: mov eax, [r14+0x20]; cmp eax, [rbx+0x20]
    // 0x2a9710: jne → 0x2a973c
    if (isIntegerFormat != other.isIntegerFormat)
        return false;

    // --- Compare fileHash vector (at +0x28) ---
    // 0x2a9712: mov rdi, [r14+0x28]           — this->fileHash.begin()
    // 0x2a9716: mov rdx, [r14+0x30]           — this->fileHash.end()
    // 0x2a971a: sub rdx, rdi                  — this fileHash size
    // 0x2a971d: mov rsi, [rbx+0x28]           — other.fileHash.begin()
    // 0x2a9721: mov rax, [rbx+0x30]           — other.fileHash.end()
    // 0x2a9725: sub rax, rsi                  — other fileHash size
    // 0x2a9728: cmp rdx, rax                  — sizes must match
    // 0x2a972b: jne → 0x2a973c
    size_t mySize = fileHash.size() * sizeof(unsigned int);
    size_t otherSize = other.fileHash.size() * sizeof(unsigned int);
    if (mySize != otherSize)
        return false;

    // 0x2a972d: call bcmp(rdi=this->fileHash.begin, rsi=other->fileHash.begin, rdx=size)
    // 0x2a9732: test eax, eax
    // 0x2a9734: sete al                       — return (bcmp == 0)
    return memcmp(fileHash.data(), other.fileHash.data(), mySize) == 0;
}

// ============================================================================
// 9. Waveform::Waveform(string name, File::Type type, string functionArgs,
//                        shared_ptr<File> file, bool used,
//                        AddressImpl<uint> addr, string genFunc,
//                        int playConfig, int waveIndex, int minLengthSamples,
//                        int allocationByteSize, DeviceConstants const& dc, Signal signal)
// Binary address: 0x2a71e0 – 0x2a72f3
// Mangled: _ZN6zhinst8WaveformC2E...
// Calling convention:
//   rdi=this, rsi=&name, edx=type, rcx=&functionArgs,
//   r8=&file(shared_ptr), r9d=used,
//   [rbp+0x10]=addr, [rbp+0x18]=&genFunc,
//   [rbp+0x20]=playConfig, [rbp+0x28]=waveIndex,
//   [rbp+0x30]=minLengthSamples, [rbp+0x38]=allocationByteSize,
//   [rbp+0x40]=&dc, [rbp+0x48]=&signal
//
// Full constructor — initializes all fields from parameters.
// ============================================================================
Waveform::Waveform(std::string name_, File::Type type_,
                   std::string functionArgs_, std::shared_ptr<File> file_,
                   bool used_, detail::AddressImpl<uint32_t> addr_,
                   std::string genFunc_,
                   int playConfig_, int waveIndex_, int minLengthSamples_,
                   int allocationByteSize_,
                   DeviceConstants const& dc_, Signal signal_)  // 0x2a71e0
{
    // rbx = this
    // r14d = edx (type), r13 = rcx (&functionArgs), r12 = r8 (&file), r15d = r9 (used)

    // 0x2a71fd-0x2a7210: copy name (SSO: movups 16B + mov 8B; heap: __init_copy_ctor_external)
    //   test BYTE [rsi], 0x1 — SSO check
    //   if SSO: movups [rsi] → [rbx], mov [rsi+0x10] → [rbx+0x10]
    //   if heap: call __init_copy_ctor_external([rsi+0x8], [rsi+0x10])
    name = std::move(name_);                        // +0x00

    // 0x2a7210/0x2a7241: mov [rbx+0x18], r14d
    waveformType = type_;                           // +0x18

    // 0x2a7214-0x2a7261: copy functionArgs from r13
    //   SSO path: movups + mov
    //   Heap path: __init_copy_ctor_external
    functionArgs = std::move(functionArgs_);      // +0x20

    // 0x2a7261-0x2a727b: copy shared_ptr<File> from r12
    //   movups [r12] → [rbx+0x38]  (ptr + ctrl)
    //   if ctrl != null: lock inc [ctrl+0x08]
    file = std::move(file_);                        // +0x38

    // 0x2a7280: mov [rbx+0x48], r15b
    used = used_;                                   // +0x48

    // 0x2a7265: mov ecx, [rbp+0x10]  — addr parameter from stack
    // 0x2a7284: mov [rbx+0x4C], ecx
    addressValue = addr_;                           // +0x4C

    // 0x2a7261: mov rax, [rbp+0x18]  — &genFunc parameter from stack
    // 0x2a728b-0x2a72b3: copy genFunc (SSO or heap)
    funDescrName = std::move(genFunc_);              // +0x50

    // 0x2a72c4: mov r8d, [rbp+0x20]   — playConfig from stack
    // 0x2a72c8: mov [rbx+0x68], r8d
    playConfig = playConfig_;                           // +0x68

    // 0x2a72c1: mov edi, [rbp+0x28]   — waveIndex from stack
    // 0x2a72cc: mov [rbx+0x6C], edi
    waveIndex = waveIndex_;                         // +0x6C

    // 0x2a72be: mov edx, [rbp+0x30]   — minLengthSamples from stack
    // 0x2a72cf: mov [rbx+0x70], edx
    minLengthSamples = minLengthSamples_;                     // +0x70

    // 0x2a72bb: mov ecx, [rbp+0x38]   — allocationByteSize from stack
    // 0x2a72d2: mov [rbx+0x74], ecx
    allocationByteSize = allocationByteSize_;                             // +0x74

    // 0x2a72b7: mov rax, [rbp+0x40]   — &dc from stack
    // 0x2a72d5: mov [rbx+0x78], rax
    deviceConstants = &dc_;                         // +0x78

    // 0x2a72b3: mov rsi, [rbp+0x48]   — &signal from stack
    // 0x2a72d9: lea rdi, [rbx+0x80]
    // 0x2a72e0: call 1150e0            — Signal::Signal(Signal const&)
    new (&signal) Signal(signal_);                  // +0x80 (copy ctor)
}

// ============================================================================
// 10. Waveform::Waveform(Waveform const& other)
// Binary address: 0x2a8ff0 – 0x2a90db
// Mangled: _ZN6zhinst8WaveformC2ERKS0_
// Calling convention: rdi=this, rsi=other (Waveform const&)
//
// Standard copy constructor. Copies all fields from other.
// ============================================================================
Waveform::Waveform(Waveform const& other)  // 0x2a8ff0
{
    // r15 = &other, rbx = this

    // 0x2a9001-0x2a9027: copy name string
    //   SSO path: movups + mov (0x2a9006-0x2a9015)
    //   Heap path: __init_copy_ctor_external (0x2a9017-0x2a9022)
    name = other.name;                              // +0x00

    // 0x2a9027: mov eax, [r15+0x18]; mov [rbx+0x18], eax
    waveformType = other.waveformType;              // +0x18

    // 0x2a902e-0x2a905e: copy functionArgs (SSO/heap)
    functionArgs = other.functionArgs;            // +0x20

    // 0x2a905e-0x2a9070: copy shared_ptr<File>
    //   movups [r15+0x38] → [rbx+0x38]
    //   if ctrl ([r15+0x40]) non-null: lock inc [ctrl+0x08]
    file = other.file;                              // +0x38

    // 0x2a9075-0x2a9079: mov rax, [r15+0x48]; mov [rbx+0x48], rax
    //   (copies used + padding + addressValue as one 8-byte load)
    used = other.used;                              // +0x48
    addressValue = other.addressValue;              // +0x4C

    // 0x2a907d-0x2a90af: copy funDescrName (SSO/heap)
    funDescrName = other.funDescrName;                // +0x50

    // 0x2a90af: mov rax, [r15+0x78]; mov [rbx+0x78], rax
    deviceConstants = other.deviceConstants;         // +0x78

    // 0x2a90b7: movups [r15+0x68] → [rbx+0x68]
    //   (copies playConfig + waveIndex + minLengthSamples + allocationByteSize as 16 bytes)
    playConfig = other.playConfig;                      // +0x68
    waveIndex = other.waveIndex;                    // +0x6C
    minLengthSamples = other.minLengthSamples;                // +0x70
    allocationByteSize = other.allocationByteSize;                        // +0x74

    // 0x2a90c0: lea rdi, [rbx+0x80]
    // 0x2a90c7: sub r15, -0x80 (= r15 + 0x80 = &other.signal)
    // 0x2a90cb: mov rsi, r15
    // 0x2a90ce: call 1150e0  — Signal::Signal(Signal const&)
    new (&signal) Signal(other.signal);             // +0x80
}

// ============================================================================
// 11. Waveform::~Waveform()
// Binary address: 0x1152e0 – 0x1153ce
// Mangled: _ZN6zhinst8WaveformD2Ev
// Calling convention: rdi=this
//
// Destroys fields in reverse order:
//   Signal (3 vectors: markerBits_, markers_, samples_)
//   funDescrName
//   shared_ptr<File>
//   functionArgs
//   name
// ============================================================================
Waveform::~Waveform()  // 0x1152e0
{
    // Binary 0x1152e0-0x1153ce inlines destruction of all members in
    // reverse declaration order:
    //   Signal vectors (markerBits_, markers_, samples_) at +0xB0/+0x98/+0x80
    //   funDescrName (string at +0x50)
    //   shared_ptr<File> (at +0x38)
    //   functionArgs (string at +0x20)
    //   name (string at +0x00)
    //
    // In our C++ reconstruction, all of these are proper members with
    // compiler-generated destructors, so the body is intentionally empty.
    // The binary's manual free/dealloc sequences are the inlined equivalent
    // of what the compiler does automatically here.
}

// ============================================================================
// WaveformFile(const char*) addition.
//
// Address 0x2a7ff0 in the binary corresponds to a `std::construct_at<>`
// helper that inlines the body — no dedicated `WaveformFile::WaveformFile
// (char const*)` symbol exists in the .so. Our header still requires the
// `(const char*)` constructor because `WaveformFile(const std::string&)`
// is declared as a delegating ctor in waveform.hpp:71.
//
// Behaviour: copy-construct `name` from the C-string, leave numeric fields
// at 0 (matching the inlined construct_at pattern observed at the
// fromJson call site) and let `data` default-construct empty.
// ============================================================================
WaveformFile::WaveformFile(const char* filename)
    : name(filename)
    , formatType(0)
    , columnMode(0)
    , isIntegerFormat(0)
    , fileHash()
{}

} // namespace zhinst
