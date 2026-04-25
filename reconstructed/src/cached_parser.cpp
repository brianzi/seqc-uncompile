// ============================================================================
// Reconstructed from disassembly of _seqc_compiler.so
// CachedParser — on-disk LRU cache of parsed waveform data
// ============================================================================

#include "zhinst/cached_parser.hpp"
#include "zhinst/elf_reader.hpp"
#include "zhinst/elf_writer.hpp"

#include <algorithm>
#include <cstring>
#include <ctime>
#include <fstream>
#include <sstream>
#include <boost/filesystem.hpp>

// Forward-declare util::wave::hash and hash2str used by getHash/cacheFile.
namespace zhinst { namespace util { namespace wave {
    std::vector<unsigned int> hash(const std::string& filePath);
    std::string hash2str(const std::vector<unsigned int>& hash);
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
bool CachedParser::removeOldFiles()
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
    //   5. Return the "kept-pinned" flag (r15b).
    //
    // currentSize_/cacheSize_ are tracked in BYTES (sub of fileSize_ confirms).
    //
    // valid_ == true means "do not evict" (pinned). The flag was named
    // `valid_` in the header; semantically it acts as a pin/lock bit.

    bool keptPinned = false;

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
            // Pinned entry — stop evicting (binary sets r15b=1 here).
            keptPinned = true;
            break;
        }
        boost::filesystem::remove(entry.filePath_);
        currentSize_ -= entry.fileSize_;
        index_.erase(entry.hash_);
    }

    saveCacheIndex();
    return keptPinned;
}

// 0x2b05b0
//
// Builds a cached .wave artifact at <cachePath_>/csv<hash2str>.wave, populates
// it with the supplied waveform data as named ELF sections (using ElfWriter
// with machineType=3), and inserts the corresponding CacheEntry into index_.
//
// Sections written:
//   ".format"      — single byte '3'  (kCacheFormat, see #64)             // 0x2b0923
//   ".file_name"   — original source path (from `name` argument)          // 0x2b0999
//   ".channels"    — sampleFormat as 4-byte int                           // 0x2b0a08
//   ".marker_bits" — markers bytes (NOTE: `markers` param, not markerBitsVec) // 0x2b0a7f
//   ".data"        — raw sample bytes (samples.data(), samples.size()*8)  // 0x2b0ade
//   ".marker"      — marker bits bytes (from `markerBitsVec` param)       // 0x2b0b3e
//   ".config"      — std::to_string(markerBits) (decimal text)            // 0x2b0bc0
//
// Algorithm:
//   1. If !enabled_ → return.                                             // 0x2b05c4
//   2. budget = samples.byte_size() >> 2 (= samples.size() * 2)          // 0x2b05e4
//      currentSize_ += budget                                             // 0x2b05fa
//   3. If currentSize_ > cacheSize_ → call removeOldFiles();             // 0x2b05fe
//      if it returns true (pinned entry blocked eviction):
//        currentSize_ -= budget; return                                   // 0x2b0e8b
//   4. filename = cachePath_ / ("csv" + hash2str(hash) + ".wave")        // 0x2b0628–0x2b0858
//   5. ElfWriter elfw(3); add 7 sections                                  // 0x2b08e2–0x2b0bfd
//   6. elfw.writeFile(filename)                                           // 0x2b0c10
//   7. CacheEntry entry(name, filename, budget, hash, /*valid=*/true)     // 0x2b0c97
//   8. index_.try_emplace(hash, entry); it->second = entry                // 0x2b0ce0–0x2b0cf3
//   9. saveCacheIndex()                                                   // 0x2b0cfb
void CachedParser::cacheFile(
    const std::string& name,
    std::vector<unsigned int> hash,
    int sampleFormat,
    const std::vector<std::uint8_t>& markers,
    int markerBits,
    const std::vector<double>& samples,
    const std::vector<std::uint8_t>& markerBitsVec)
{
    if (!enabled_) return;                                                     // 0x2b05c4

    // Budget = samples.byte_size() >> 2 = samples.size() * 2
    std::size_t budget = samples.size() * sizeof(double) / 4;                  // 0x2b05e4
    currentSize_ += budget;                                                    // 0x2b05fa

    if (currentSize_ > cacheSize_) {                                           // 0x2b05fe
        bool keptPinned = removeOldFiles();                                    // 0x2b060a
        if (keptPinned) {                                                      // 0x2b0614
            currentSize_ -= budget;                                            // 0x2b0e8b
            return;
        }
    }

    // Build filename: cachePath_ / ("csv" + hash2str(hash) + ".wave")         // 0x2b0628
    std::ostringstream oss;
    oss << "csv";                                                              // 0x2b0637
    oss << util::wave::hash2str(hash);                                         // 0x2b0666–0x2b0695
    oss << ".wave";                                                            // 0x2b069a
    boost::filesystem::path filePath = cachePath_ / oss.str();                 // 0x2b06ce–0x2b0858
    std::string filePathStr = filePath.string();

    // Construct ElfWriter with machineType=3 and add sections                 // 0x2b08e2
    ElfWriter elfw(3);

    // ".format" — single byte '3' (kCacheFormat)                              // 0x2b0923
    elfw.addData("3", 1, std::string(".format"));

    // ".file_name" — original source name                                     // 0x2b0999
    elfw.addData(name.data(), name.size(), std::string(".file_name"));

    // ".channels" — sampleFormat as a 4-byte int                              // 0x2b0a08
    elfw.addData(reinterpret_cast<const char*>(&sampleFormat),
                 sizeof(sampleFormat), std::string(".channels"));

    // ".marker_bits" — from `markers` parameter (not markerBitsVec!)          // 0x2b0a7f
    elfw.addData(reinterpret_cast<const char*>(markers.data()),
                 markers.size(), std::string(".marker_bits"));

    // ".data" — raw sample bytes                                              // 0x2b0ade
    elfw.addData(reinterpret_cast<const char*>(samples.data()),
                 samples.size() * sizeof(double), std::string(".data"));

    // ".marker" — from `markerBitsVec` parameter (not markers!)               // 0x2b0b3e
    elfw.addData(reinterpret_cast<const char*>(markerBitsVec.data()),
                 markerBitsVec.size(), std::string(".marker"));

    // ".config" — markerBits as decimal string                                // 0x2b0b6a–0x2b0bc0
    std::string configStr = std::to_string(markerBits);
    elfw.addData(configStr.data(), configStr.size(), std::string(".config"));

    // Write the ELF to disk                                                   // 0x2b0c10
    elfw.writeFile(filePathStr);

    // Construct CacheEntry and insert into index_                             // 0x2b0c15–0x2b0cf8
    // Budget is used as fileSize (not boost::filesystem::file_size)
    CacheEntry entry(name, filePathStr, budget, hash, /*valid=*/true);         // 0x2b0c97
    auto [it, inserted] = index_.try_emplace(hash, entry);                     // 0x2b0ce0
    if (!inserted) {
        it->second = entry;                                                    // 0x2b0cf3 — operator= at 0x2b1210
    }
    saveCacheIndex();                                                          // 0x2b0cfb
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
