#pragma once

#include <deque>
#include <filesystem>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

#include "Buffer.h"
#include "BumpAllocator.h"
#include "SourceLocation.h"
#include "StringRef.h"

namespace slang {

struct SourceBuffer {
    Buffer<char> data;
    FileID id;

    SourceBuffer() : data(0) {}
    SourceBuffer(FileID id, Buffer<char>&& data) :
        id(id), data(std::move(data)) {
    }
};

class SourceManager {
public:
    SourceManager();

    std::string makeAbsolutePath(StringRef path) const;
    void addSystemDirectory(StringRef path);
    void addUserDirectory(StringRef path);

    // SourceLocation and FileID query methods
    uint32_t getLineNumber(SourceLocation location);
    uint32_t getColumnNumber(SourceLocation location);
    StringRef getFileName(FileID file);

    // get the buffer for the given file ID
    SourceBuffer* getBuffer(FileID id);

    // Give ownership of source code to the manager and refer to it by the given path.
    // This method will fail if the given path is already loaded.
    SourceBuffer* assignText(StringRef text);
    SourceBuffer* assignText(StringRef path, StringRef text);
    SourceBuffer* assignBuffer(StringRef path, Buffer<char>&& buffer);

    // get the source buffer for the file at the specified path
    SourceBuffer* readSource(StringRef path);
    SourceBuffer* readHeader(StringRef path, FileID includedFrom, bool isSystemPath);

private:
    using path_type = std::tr2::sys::path;

    BumpAllocator alloc;
    path_type workingDir;
    uint32_t nextFileID = 1;
    uint32_t unnamedBufferCount = 0;

    struct FileInfo {
        SourceBuffer* buffer = nullptr;
        const path_type* directory = nullptr;
        std::string name;
        std::vector<uint32_t> lineOffsets;
    };

    struct ExpansionInfo {
        SourceLocation originalLocation;
        SourceLocation expansionLocationStart;
        SourceLocation expansionLocationEnd;
    };

    struct BufferEntry {
        bool isFile;
        union {
            FileInfo file;
            ExpansionInfo expansion;
        };

        BufferEntry(FileInfo f) : isFile(true), file(f) {}
        BufferEntry(ExpansionInfo e) : isFile(false), expansion(e) {}

        BufferEntry(const BufferEntry& e) {
            isFile = e.isFile;
            if (isFile)
                file = e.file;
            else
                expansion = e.expansion;
        }

        ~BufferEntry() {
            if (isFile) file.~FileInfo();
            else expansion.~ExpansionInfo();
        }
    };

    // index from FileID to buffer metadata
    std::deque<BufferEntry> bufferEntries;

    // cache for file lookups; this holds on to the actual file data
    std::unordered_map<std::string, std::unique_ptr<SourceBuffer>> lookupCache;

    // directories for system and user includes
    std::vector<path_type> systemDirectories;
    std::vector<path_type> userDirectories;

    // uniquified backing memory for directories
    std::set<path_type> directories;

    SourceBuffer* openCached(path_type fullPath);
    SourceBuffer* cacheBuffer(std::string&& canonicalPath, path_type& path, Buffer<char>&& buffer);

    static void computeLineOffsets(const Buffer<char>& buffer, std::vector<uint32_t>& offsets);
    static bool readFile(const path_type& path, Buffer<char>& buffer);
};

}
