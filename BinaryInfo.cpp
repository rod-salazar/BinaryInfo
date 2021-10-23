#include <cstddef>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

// TODO: How can I write a unit-test?
// TODO: How can I hook this up to valgrind or similar?
// TODO: How can I use C++20 modules?

// This exposes the ability to read a range of bytes from
// a file starting from an offset. It may cache a larger range
// than requested, which might benefit the access pattern of
// reading the file from beginning to end.
// 
// This class does not make any guarantee that anything is cached.
class FileSession {
public:
    // TODO: Where will file not found be handled?
    FileSession(std::string path) : stream_(path, std::ios::binary) {
        // Cache the file length
        stream_.seekg(0, std::ios::end);
        fileLength_ = stream_.tellg();
        stream_.seekg(0, std::ios::beg);

        // Configure stream to throw exceptions on read errors
        stream_.exceptions(std::ifstream::failbit | std::ifstream::badbit);

        buffer_.reserve(CACHE_SIZE_BYTES);
    }

    std::byte getByte(uint64_t byteOffset) {
        return getRange(byteOffset, 1)[0];
    }

    std::vector<std::byte> getRange(uint64_t byteOffset, uint64_t len) {
        return read(byteOffset, len);
    }

private:
    static const uint64_t CACHE_SIZE_BYTES = 1024 * 1024;

    std::ifstream stream_;
    uint64_t fileLength_;

    std::vector<std::byte> buffer_;
    uint64_t bufferByteOffset_ = -1;
    bool bufferValid_ = false;

    FileSession(const FileSession&) = delete;
    FileSession& operator=(FileSession) = delete;

    std::vector<std::byte> read(uint64_t byteOffset, uint64_t len) {
        stream_.seekg(byteOffset);

        // TODO: Determine if read if out of file length bounds here. Throw?

        if (len > CACHE_SIZE_BYTES) {
            // If the read is more than we'd like to cache, let's just read it
            // and return it.
            std::vector<std::byte> largeReadBuffer;
            largeReadBuffer.reserve(len);
            stream_.read((char*)&largeReadBuffer[0], len);
            return largeReadBuffer;
        }

        // If we have the entire range, let's return our buffer.
        if (bufferValid_ && bufferByteOffset_ <= byteOffset && byteOffset + len <= bufferByteOffset_ + CACHE_SIZE_BYTES) {
            return buffer_;
        }

        // Casting byte* to char*. TODO: Does c++ guarantee this is okay at compile time?
        stream_.read((char*)&buffer_[0], CACHE_SIZE_BYTES);

        // Marking the buffer as having usable data.
        bufferValid_ = true;
        bufferByteOffset_ = byteOffset;

        // TODO:  Does this guarantee copy elison on return?
        return buffer_;
    }
};

// This class maintains a handle to a binary file and
// exposes information about the binary file.
class BinarySession {
public:
    BinarySession(std::string path) : path_(path), session_(path) {}

    // TODO: Understand what const after the function name means.
    const std::string& path() const noexcept {
        return path_;
    }

private:
    BinarySession(const BinarySession&) = delete;
    BinarySession& operator=(BinarySession) = delete;

    std::string path_;
    FileSession session_;
};


int main(int argc, char* argv[])
{
    // TODO: Use a command line parsing library.

    if (argc < 2) {
        std::cout << "Missing file argument" << std::endl;
        return 0;
    }

    // Invokes copy constructor
    std::string fileName = argv[1];
    BinarySession session(fileName);
    std::cout << session.path() << std::endl;
}
