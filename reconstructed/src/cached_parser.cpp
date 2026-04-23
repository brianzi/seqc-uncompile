// ============================================================================
// Reconstructed from disassembly of _seqc_compiler.so
// CachedParser — on-disk LRU cache of parsed waveform data
// ============================================================================

#include "zhinst/cached_parser.hpp"
#include "zhinst/elf_reader.hpp"

#include <algorithm>
#include <cstring>
#include <ctime>
#include <fstream>
#include <boost/filesystem.hpp>

// Forward-declare util::wave::hash used by getHash — defined elsewhere.
namespace zhinst { namespace util { namespace wave {
    std::vector<unsigned int> hash(const std::string& filePath);
}}}

namespace zhinst {

// ---- CacheEntry ----

// 0x2b10b0
CachedParser::CacheEntry::CacheEntry(
    const std::string& name,
    const std::string& filePath,
    std::size_t fileSize,
    std::vector<unsigned int> hash,
    bool valid)
    : name_(name)
    , filePath_(filePath)
    , fileSize_(fileSize)
    , timestamp_(std::time(nullptr))
    , hash_(std::move(hash))
    , valid_(valid)
{
}

// 0x2b1320
CachedParser::CacheEntry::~CacheEntry() = default;

// 0x2b1210
CachedParser::CacheEntry&
CachedParser::CacheEntry::operator=(const CacheEntry& other)
{
    if (this != &other) {
        name_      = other.name_;
        filePath_  = other.filePath_;
        fileSize_  = other.fileSize_;
        timestamp_ = other.timestamp_;
        hash_      = other.hash_;
        valid_     = other.valid_;
    }
    return *this;
}

// ---- CachedFile ----

// 0x2b1f70
CachedParser::CachedFile::~CachedFile() = default;

// ---- CachedParser ----

// 0x2afa70
CachedParser::CachedParser(std::size_t cacheSize,
                           const boost::filesystem::path& cachePath)
    : index_()
    , enabled_(cacheSize != 0)
    , cacheSize_(cacheSize)
    , currentSize_(0)
    , cachePath_(cachePath)
    , indexFilePath_(cachePath / "index")
{
    if (enabled_) {
        boost::filesystem::create_directories(cachePath_);
        loadCacheIndex();
    }
}

// 0x29aac0
CachedParser::~CachedParser() = default;

// 0x2afec0
//
// Loads the persisted index_ map from indexFilePath_ via boost::archive::
// text_iarchive. After a successful load, walks the freshly-loaded __tree
// in-order to recompute currentSize_ as the sum of all entries' fileSize_
// (binary inlines the tree walk and reads [node+0x68] = entry+0x30 = fileSize_).
// If the recomputed size exceeds cacheSize_, calls removeOldFiles() to
// prune.
//
// Error handling has two catch handlers in the binary, both of which call
// cleanCache() to drop the on-disk state and continue with an empty index:
//   - r15==1: thrown during ifstream construction (file missing/unreadable)
//   - r15==2: thrown during text_iarchive setup or load_object (corrupt file)
//
// If the file simply doesn't exist (open succeeded but streambuf is null),
// the function returns silently with index_ unchanged (still empty).
void CachedParser::loadCacheIndex()
{
    if (!enabled_) return;

    try {
        std::ifstream ifs(indexFilePath_.string(), std::ios::in);
        if (!ifs) {
            // No index file yet — first run with this cache directory.
            return;
        }

        try {
            // Real implementation:
            //   boost::archive::text_iarchive ia(ifs);
            //   ia >> index_;
            // The boost serializer singleton for
            //   std::map<vector<uint>, CacheEntry>
            // is lazy-initialized via __cxa_guard_acquire on first call.
            // TODO: when build links real boost archive lib, restore the
            //       2-line iarchive construct + operator>> pattern above.

            // Recompute currentSize_ from loaded entries (sum of fileSize_).
            std::size_t total = 0;
            for (const auto& kv : index_) {
                total += kv.second.fileSize_;
            }
            currentSize_ = total;
        } catch (...) {
            // Corrupt archive — wipe and start fresh.
            cleanCache();
            return;
        }

        if (currentSize_ > cacheSize_) {
            removeOldFiles();
        }
    } catch (...) {
        // ifstream construction itself threw — wipe and start fresh.
        cleanCache();
    }
}

// 0x2b03c0
//
// Serialization layout:
//   - Skips entirely if enabled_ is false OR if index_ is empty
//     (binary checks [rdi+0x10] which is the __tree size field).
//   - Opens an ofstream to indexFilePath_ with mode flags 0x10
//     (this is std::ios::binary in libc++ — value differs from libstdc++).
//   - Bails silently if the ofstream's streambuf is null (open failed).
//   - Constructs a boost::archive::text_oarchive over the stream and
//     does `oa << index_` (the boost machinery resolves to save_object
//     on a oserializer<text_oarchive, std::map<vector<uint>, CacheEntry>>
//     singleton, lazily constructed via __cxa_guard_acquire).
//   - On any exception (text_oarchive ctor, save_object, ofstream errors,
//     etc.) the catch handler at 0x2b056b sets enabled_ = false. This is
//     the cache's "give up after I/O failure" sticky-disable mechanism.
void CachedParser::saveCacheIndex()
{
    if (!enabled_) return;
    if (index_.empty()) return;

    try {
        std::ofstream ofs(indexFilePath_.string(), std::ios::binary);
        if (!ofs) {
            // Stream couldn't open — silently return (binary checks
            // streambuf pointer at rbp-0x160 and skips archive setup).
            return;
        }

        // boost::archive::text_oarchive uses RAII; archive dtor flushes.
        // We don't include boost/archive headers here because the
        // reconstructed sources avoid pulling in heavy dependencies;
        // the actual serialize() template is not reconstructed.
        // TODO: when the build links real boost archive lib, replace
        //       the placeholder below with the genuine 2-line oarchive
        //       construct + operator<< pattern:
        //
        //   boost::archive::text_oarchive oa(ofs);
        //   oa << index_;
    } catch (...) {
        // Sticky-disable the cache after any serialization failure.
        enabled_ = false;
    }
}

// 0x2b0140
void CachedParser::cleanCache()
{
    // 1. index_.clear() — destroys the __tree, then resets head fields.
    // 2. currentSize_ = 0  (cacheSize_ is NOT touched).
    // 3. boost::filesystem::remove_all(cachePath_)  (no error_code → may throw).
    // 4. boost::filesystem::create_directory(cachePath_)  (singular, not _directories).
    // The whole body sits inside a try/catch that calls __cxa_begin_catch /
    // __cxa_end_catch — i.e. exceptions from filesystem ops are swallowed.
    try {
        index_.clear();
        currentSize_ = 0;
        boost::filesystem::remove_all(cachePath_);
        boost::filesystem::create_directory(cachePath_);
    } catch (...) {
        // intentionally swallowed (matches binary catch handler at 0x2b0188)
    }
}

// 0x2b01a0
void CachedParser::removeOldFiles()
{
    // Eviction algorithm:
    //   1. Walk index_ in-order, copy every CacheEntry into a local vector.
    //   2. Sort the vector by some comparator (lambda $_0 in binary, almost
    //      certainly `a.timestamp_ < b.timestamp_` — oldest first).
    //   3. Iterate the sorted vector. For each entry while
    //      currentSize_ > cacheSize_ AND entry.valid_ == false (i.e. evictable):
    //        - boost::filesystem::remove(entry.filePath_)
    //        - currentSize_ -= entry.fileSize_
    //        - index_.erase(entry.hash_)
    //      If an entry has valid_ == true, set the "kept-pinned" flag (r15b=1)
    //      and stop the eviction loop.
    //   4. Call saveCacheIndex() unconditionally afterwards.
    //   5. Destroy the local vector. Returns r15b in the binary, but the C++
    //      signature is `void` so the caller ignores it. We mirror that.
    //
    // currentSize_/cacheSize_ are tracked in BYTES (sub of fileSize_ confirms).
    //
    // valid_ == true means "do not evict" (pinned). The flag was named
    // `valid_` in the header; semantically it acts as a pin/lock bit.

    std::vector<CacheEntry> entries;
    entries.reserve(index_.size());
    for (const auto& kv : index_) {
        entries.push_back(kv.second);
    }

    std::sort(entries.begin(), entries.end(),
              [](const CacheEntry& a, const CacheEntry& b) {
                  return a.timestamp_ < b.timestamp_;
              });

    for (const auto& entry : entries) {
        if (currentSize_ <= cacheSize_) break;
        if (entry.valid_) {
            // Pinned entry — stop evicting (binary sets a "stopped early" flag here).
            break;
        }
        boost::filesystem::remove(entry.filePath_);
        currentSize_ -= entry.fileSize_;
        index_.erase(entry.hash_);
    }

    saveCacheIndex();
}

// 0x2b05b0
//
// Builds a cached ELF artifact at <cachePath_>/<hash-as-hex>.<ext>, populates
// it with the supplied waveform data as named ELF sections, and inserts the
// corresponding CacheEntry into index_.
//
// Sections written (4-byte little-endian section names observed in disasm):
//   .format        — single byte '3'  (kCacheFormat, see #64)
//   .file_name     — original source path (from `name` argument)
//   .channel       — channel/oscillator index info
//   .data          — sample bytes (from `samples`)
//   .marker        — marker bytes  (from `markers`)
//   .marker_bits   — marker-bit configuration (from `markerBitsVec`)
//   .config        — sampleFormat + markerBits packed
//
// Algorithm:
//   1. If !enabled_ → return.
//   2. currentSize_ += rough byte budget for this entry (binary uses
//      `(samples.byteSize() / 4)` as a 32-bit-word count for budgeting).
//   3. If currentSize_ > cacheSize_ → call removeOldFiles(); if it returns
//      false (couldn't free enough) → bail out (binary jumps to 0x2b0e8b).
//   4. Build the cache filename:
//        oss << "<cachePath_>/" << util::wave::hash2str(hash) << ".elf"
//        (or similar suffix — exact suffix string lives at 0x8ff1b6).
//   5. Construct ElfWriter elfw(channel); call elfw.addSection(name, data)
//      for each of the 7 sections above.
//   6. Save the ELF to disk via elfw.save(filename).
//   7. Construct CacheEntry{name, filename, fileSize, hash, valid=true},
//      insert into index_ (or update via operator= if key exists).
//   8. Call saveCacheIndex().
//
// Heavy boost / ELFIO interaction (~880 instructions) — exact ELFIO calls
// not reconstructed; the operational outline above is sufficient for a
// behavioural model.
void CachedParser::cacheFile(
    const std::string& name,
    std::vector<unsigned int> hash,
    int sampleFormat,
    const std::vector<std::uint8_t>& markers,
    int markerBits,
    const std::vector<double>& samples,
    const std::vector<std::uint8_t>& markerBitsVec)
{
    if (!enabled_) return;

    // Budget update — binary computes (samples.bytesize / 4); we approximate.
    currentSize_ += samples.size() * sizeof(double) / 4;
    if (currentSize_ > cacheSize_) {
        removeOldFiles();
        // The binary checks removeOldFiles' bool return (r15b "kept-pinned"
        // flag) and bails if true. The C++ signature is void so we can't
        // mirror that exactly; in practice removeOldFiles makes a best-effort
        // sweep and we proceed regardless.
    }

    // TODO: reconstruct full ELF-building body from disassembly at 0x2b05b0.
    // The skeleton:
    //
    //   std::string filename = (cachePath_ / (util::wave::hash2str(hash) + ".elf")).string();
    //   ElfWriter elfw(/*channel=*/markerBits >> 8);
    //   elfw.addSection(".format",      std::string(1, '3'));
    //   elfw.addSection(".file_name",   name);
    //   elfw.addSection(".channel",     ...);
    //   elfw.addSection(".data",        samples);
    //   elfw.addSection(".marker",      markers);
    //   elfw.addSection(".marker_bits", markerBitsVec);
    //   elfw.addSection(".config",      pack(sampleFormat, markerBits));
    //   elfw.save(filename);
    //
    //   std::size_t fileSize = boost::filesystem::file_size(filename);
    //   CacheEntry entry(name, filename, fileSize, std::move(hash), true);
    //   auto [it, inserted] = index_.try_emplace(entry.hash_, entry);
    //   if (!inserted) it->second = entry;   // operator= at 0x2b1210
    //   saveCacheIndex();
    (void)sampleFormat; (void)markers; (void)markerBitsVec;
    (void)hash; (void)name;
}

// 0x2b14d0
//
// NOTE: despite the parameter being named `name`, this function does NOT search
// index_. It opens `name` as a path to a cached ELF artifact and inspects two
// sections within it:
//
//   .format     — must contain the single byte '3' (kCacheFormat, see #64).
//                 If absent or different, the cached file is outdated.
//   .file_name  — contains the path to the *original source* that produced
//                 this cached ELF. We compare last_write_time(source) against
//                 last_write_time(cachedElf=name). If source is newer, the
//                 cached file is outdated.
//
// Any exception (file not openable, sections missing, ELFIO failure, etc.)
// causes the function to return true (treat as outdated). This is the
// `__cxa_begin_catch` handler at 0x2b1893 setting r14b=1.
bool CachedParser::cacheFileOutdated(const std::string& cachedElfPath) const
{
    bool outdated = true;  // default if anything goes wrong
    try {
        ElfReader reader(cachedElfPath);

        // Section ".format" must equal "3"
        std::string fmt = sectionAsString(reader.getSection(".format"));
        if (fmt.empty() || fmt[0] != '3') {
            return true;
        }

        // Section ".file_name" gives the original source path
        std::string sourcePath =
            sectionAsString(reader.getSection(".file_name"));

        std::time_t srcMtime =
            boost::filesystem::last_write_time(sourcePath);
        std::time_t elfMtime =
            boost::filesystem::last_write_time(cachedElfPath);

        // Outdated iff source is newer than the cached ELF
        outdated = (srcMtime > elfMtime);
    } catch (...) {
        outdated = true;
    }
    return outdated;
}

// 0x2b1900
//
// Lookup-and-load:
//   1. Zero-init result (channel_ + 3 vectors).
//   2. If !enabled_ or index_ is empty → return result (miss).
//   3. Lower-bound search of index_ keyed by `hash` (lexicographic compare
//      on vector<unsigned int>). Miss → return result.
//   4. On hit, load filePath_ from the entry, call cacheFileOutdated.
//      If outdated:
//        - boost::filesystem::remove(filePath_)
//        - erase entry from index_ (__tree_remove + delete 0x98-byte node)
//        - return result (still default — cache miss after eviction).
//   5. On valid hit:
//        - entry.timestamp_ = time(nullptr)   (LRU touch, [r15+0x70])
//        - entry.valid_ = true                (mark accessed, [r15+0x90])
//        - Construct ElfReader(filePath_).
//        - Read .channels  → result.channel_ = first int32 cast to uint16
//        - Read .marker_bits → result.markerBits_
//        - Read .data        → result.samples_
//        - Read .marker      → result.markers_
//   6. Any exception during ELF reading triggers cleanCache() and an empty
//      result is returned (catch handler at 0x2b1f27).
//
// NOTE: there is no boolean "found" flag in CachedFile. Callers detect a
// miss by inspecting samples_.empty().
CachedParser::CachedFile
CachedParser::getCachedFile(const std::vector<unsigned int>& hash)
{
    CachedFile result{};
    result.channel_ = 0;

    if (!enabled_) return result;
    if (index_.empty()) return result;

    auto it = index_.find(hash);
    if (it == index_.end()) return result;

    CacheEntry& entry = it->second;

    if (cacheFileOutdated(entry.filePath_)) {
        boost::filesystem::remove(entry.filePath_);
        index_.erase(it);
        return result;
    }

    // LRU touch + access flag.
    entry.timestamp_ = std::time(nullptr);
    entry.valid_     = true;

    try {
        ElfReader reader(entry.filePath_);

        // .channels — first int32 of the section gives the channel/oscillator
        // index. Only the low 16 bits are kept.
        {
            std::string s = sectionAsString(reader.getSection(".channels"));
            if (s.size() >= 4) {
                std::uint32_t ch = 0;
                std::memcpy(&ch, s.data(), 4);
                result.channel_ = static_cast<std::uint16_t>(ch);
            }
        }

        auto loadBytes = [&](const char* name) {
            std::string s = sectionAsString(reader.getSection(name));
            return std::vector<std::uint8_t>(s.begin(), s.end());
        };
        auto loadDoubles = [&](const char* name) {
            std::string s = sectionAsString(reader.getSection(name));
            const auto n = s.size() / sizeof(double);
            std::vector<double> v(n);
            if (n) std::memcpy(v.data(), s.data(), n * sizeof(double));
            return v;
        };

        result.markerBits_ = loadBytes(".marker_bits");
        result.samples_    = loadDoubles(".data");
        result.markers_    = loadBytes(".marker");
    } catch (...) {
        // Corrupt cache file — wipe everything and return empty result.
        cleanCache();
        result = CachedFile{};
    }

    return result;
}

// 0x2b1fe0
std::vector<unsigned int>
CachedParser::getHash(const std::string& filePath) const
{
    // Returns empty vector if disabled; otherwise delegates to
    // zhinst::util::wave::hash(filePath).
    if (!enabled_) {
        return {};
    }
    return util::wave::hash(filePath);
}

} // namespace zhinst
