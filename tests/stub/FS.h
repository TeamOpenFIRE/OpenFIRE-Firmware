// A fake of the Arduino file system classes (File, and the LittleFS volume) that keeps its files in memory.
#pragma once

#include <cstddef>
#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace fs {

enum SeekMode { SeekSet = 0, SeekCur = 1, SeekEnd = 2 };

// One file's bytes. Files are shared between the volume and the File objects that have them open.
struct FileData {
    std::vector<uint8_t> bytes;
};

class File {
public:
    File() = default;
    File(std::shared_ptr<FileData> data, bool writable) : data(std::move(data)), writable(writable) {}

    explicit operator bool() const { return data != nullptr; }

    int available() const { return data ? (int)(data->bytes.size() - position) : 0; }
    int read() {
        if (!data || position >= data->bytes.size()) return -1;
        return data->bytes[position++];
    }
    size_t readBytes(char* buffer, size_t length) {
        size_t count = 0;
        while (count < length && data && position < data->bytes.size()) buffer[count++] = (char)data->bytes[position++];
        return count;
    }
    // Reads up to the terminator, which is taken from the file but not stored, or up to the length.
    size_t readBytesUntil(char terminator, char* buffer, size_t length) {
        size_t count = 0;
        while (count < length && data && position < data->bytes.size()) {
            const char next = (char)data->bytes[position++];
            if (next == terminator) break;
            buffer[count++] = next;
        }
        return count;
    }
    size_t write(uint8_t byte) {
        if (!data || !writable) return 0;
        data->bytes.push_back(byte);
        return 1;
    }
    size_t write(const uint8_t* buffer, size_t length) {
        if (!data || !writable) return 0;
        data->bytes.insert(data->bytes.end(), buffer, buffer + length);
        return length;
    }
    bool seek(uint32_t offset, SeekMode mode = SeekSet) {
        if (!data) return false;
        int64_t target = mode == SeekSet ? (int64_t)offset : mode == SeekCur ? (int64_t)position + offset : (int64_t)data->bytes.size() + offset;
        if (target < 0 || target > (int64_t)data->bytes.size()) return false;
        position = (size_t)target;
        return true;
    }
    size_t size() const { return data ? data->bytes.size() : 0; }
    void close() { data.reset(); }

private:
    std::shared_ptr<FileData> data;
    bool writable = false;
    size_t position = 0;
};

} // namespace fs

using fs::File;
