#include <cstddef>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>
#include <cassert>
#include <ranges>
#include <type_traits>

// TODO: How can I write a unit-test?
// TODO: How can I hook this up to valgrind or similar?
// TODO: How can I use C++20 modules?


template <typename U, typename V>
using smaller_type = std::conditional_t<sizeof(U) < sizeof(V), U, V>;

template <typename U, typename V>
using larger_type = std::conditional_t<sizeof(V) < sizeof(U), U, V>;

// std::min, except it automatically returns the smaller type.
// https://stackoverflow.com/a/31361217/11763503
template <typename U, typename V>
smaller_type<U, V> min_t(const U& u, const V& v) {
    // Result of std::min is guaranteed to fit in the smaller type.
    return static_cast<smaller_type<U,V>>(std::min<larger_type<U, V>>(u, v));
}

// This exposes the ability to read a range of bytes from
// a file starting from an offset. It may cache a larger range
// than requested, which might benefit the access pattern of
// reading the file from beginning to end.
// 
// This class does not make any guarantee that anything is cached.
class FileSession {
    // Insert private section up top due to auto type deduction requiring this method order.
private:
    // This will read CACHE_SIZE_BYTES bytes from the file or until the file ends,
    // place it into cache and return a range view into the cache.
    auto read(uint64_t byteOffset, uint16_t len) {

        if (byteOffset >= fileLength_) {
            throw new std::exception("Requested byteoffset is beyond file length");
        }

        stream_.seekg(byteOffset);

        // Limit read to the end of the file.
        uint16_t readLength = min_t(CACHE_SIZE_BYTES, fileLength_ - byteOffset);
        uint16_t viewLength = min_t(len, fileLength_ - byteOffset);

        // If we have the entire range, let's return our buffer.
        if (bufferValidBytes > 0 && bufferByteOffset_ <= byteOffset && byteOffset + viewLength <= bufferByteOffset_ + bufferValidBytes) {
            return buffer_ | std::ranges::views::take(viewLength);
        }

        // Casting byte* to char*. TODO: Does c++ guarantee this is okay at compile time?
        stream_.read((char*)buffer_.data(), readLength);

        // Mark the buffer as having usable data.
        bufferValidBytes = readLength;
        bufferByteOffset_ = byteOffset;

        return buffer_ | std::ranges::views::take(viewLength);
    }

public:
    // TODO: Where will file not found be handled?
    FileSession(std::string path) : stream_(path, std::ios::binary) {
        // Cache the file length
        stream_.seekg(0, std::ios::end);
        fileLength_ = stream_.tellg();
        stream_.seekg(0, std::ios::beg);

        // Configure stream to throw exceptions on read errors
        stream_.exceptions(std::ifstream::failbit | std::ifstream::badbit);

        // Set size once and we'll re-use this buffer forever.
        buffer_.resize(CACHE_SIZE_BYTES);
    }

    auto getRange(uint64_t byteOffset, uint16_t len) {
        assert(len > 0);
        return read(byteOffset, len);
    }

    std::byte getByte(uint64_t byteOffset) {
        return *getRange(byteOffset, 1).begin();
    }

private:
    // This must be at least as large as the len parameter on the read() function can be (uint16_t)
    // so that the read is always cacheable without reallocating.
    static const uint16_t CACHE_SIZE_BYTES = std::numeric_limits<uint16_t>::max();

    std::ifstream stream_;
    uint64_t fileLength_;

    std::vector<std::byte> buffer_;
    uint64_t bufferByteOffset_ = -1;
    uint16_t bufferValidBytes = 0;

    FileSession(const FileSession&) = delete;
    FileSession& operator=(FileSession) = delete;
};

// This class maintains a handle to a binary file and
// exposes information about the binary file.
class BinarySession {
public:
    BinarySession(std::string path) : path_(path), session_(path) {
        std::cout << std::hex << (int)session_.getByte(0x3C) << std::endl;
    }

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
