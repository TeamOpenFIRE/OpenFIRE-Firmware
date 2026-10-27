// A fake LittleFS volume: files in memory, listable by tests, with failures a test can switch on.
#pragma once

#include <cstring>
#include <map>
#include <memory>
#include <string>

#include "FS.h"

class LittleFS_ {
public:
    std::map<std::string, std::shared_ptr<fs::FileData>> files;
    bool mounts = true;         // whether begin() succeeds
    bool refuseWrites = false;  // whether open(..., "w") fails, as on a full or damaged volume
    int writesAllowed = -1;     // if not negative: how many more files may be opened for writing before the volume fills up
    int failWriteNumber = -1;   // if not negative: only the file opened for writing as the Nth from now (0 first) fails
    int formatCount = 0;

    bool begin() { return mounts; }
    void format() {
        files.clear();
        formatCount++;
    }
    fs::File open(const char* path, const char* mode) {
        if (strcmp(mode, "r") == 0) {
            auto found = files.find(path);
            return found == files.end() ? fs::File() : fs::File(found->second, false);
        }
        if (refuseWrites) return fs::File();
        if (failWriteNumber >= 0 && failWriteNumber-- == 0) return fs::File();
        if (writesAllowed == 0) return fs::File();
        if (writesAllowed > 0) writesAllowed--;
        auto data = std::make_shared<fs::FileData>();
        files[path] = data;     // "w" starts the file again
        return fs::File(data, true);
    }
    bool exists(const char* path) const { return files.count(path) != 0; }

    // For the tests: the bytes of a file, to read or to set.
    std::vector<uint8_t>& bytesOf(const char* path) {
        if (!files.count(path)) files[path] = std::make_shared<fs::FileData>();
        return files[path]->bytes;
    }
    void reset() {
        files.clear();
        mounts = true;
        refuseWrites = false;
        writesAllowed = -1;
        failWriteNumber = -1;
        formatCount = 0;
    }
};
inline LittleFS_ LittleFS;
