#include <cstddef>
#include <filesystem>
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

// std::min, except it automatically uses the smaller type as the return type.
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

        stream_->seekg(byteOffset);

        // Limit read to the end of the file.
        uint16_t readLength = min_t(CACHE_SIZE_BYTES, fileLength_ - byteOffset);
        uint16_t viewLength = min_t(len, fileLength_ - byteOffset);

        // If we have the entire range, let's return a view into our buffer.
        uint64_t viewEndOffset = byteOffset + viewLength;
        uint64_t bufferEndOffset = bufferByteOffset_ + bufferValidBytes;
        if (bufferValidBytes > 0 && bufferByteOffset_ <= byteOffset && viewEndOffset <= bufferEndOffset) {
            uint16_t bytesBeforeView = static_cast<uint16_t>(byteOffset - bufferByteOffset_);
            return buffer_ | std::ranges::views::drop(static_cast<uint16_t>(bytesBeforeView)) | std::ranges::views::take(viewLength);
        }

        // Casting byte* to char*. TODO: Does c++ guarantee this is okay at compile time?
        stream_->read((char*)buffer_.data(), readLength);

        bufferValidBytes = readLength;
        bufferByteOffset_ = byteOffset;

        return buffer_ | std::ranges::views::drop(0) | std::ranges::views::take(viewLength);
    }

public:
    FileSession(std::string path) {
        if (!std::filesystem::exists(path)) {
            throw new std::invalid_argument("File does not exist: " + path);
        }

        stream_ = std::make_unique<std::ifstream>(path, std::ios::binary);

        // Cache the file length
        stream_->seekg(0, std::ios::end);
        fileLength_ = stream_->tellg();
        stream_->seekg(0, std::ios::beg);

        // Configure stream to throw exceptions on read errors
        stream_->exceptions(std::ifstream::failbit | std::ifstream::badbit);

        // Set size once and we'll re-use this buffer forever.
        buffer_.resize(CACHE_SIZE_BYTES);
    }

    // How does ownership work? What prevents the range outliving the underlying storage?
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

    // I want to check if file exists before constructing the ifstream, for this reason
    // I can not use an initializer list for stream and just use a concrete type.
    std::unique_ptr<std::ifstream> stream_;
    uint64_t fileLength_;

    std::vector<std::byte> buffer_;
    uint64_t bufferByteOffset_ = -1;
    uint16_t bufferValidBytes = 0;

    FileSession(const FileSession&) = delete;
    FileSession& operator=(FileSession) = delete;
};

enum class BinaryType {
    WINDOWS_PE_32,
    WINDOWS_PE_64,
    UNKNOWN
};

const std::array<BinaryType, 2> BINARY_TYPES = { BinaryType::WINDOWS_PE_32, BinaryType::WINDOWS_PE_64 };

// This class maintains a handle to a binary file and
// exposes information about the binary file.
class BinarySession {
public:
    BinarySession(std::string path) : path_(path) {
        if (!std::filesystem::exists(path)) {
            throw new std::invalid_argument("File does not exist: " + path);
        }
        session_ = std::make_unique<FileSession>(path);
    }

    // TODO: Understand what const after the function name means.
    const std::string& path() const noexcept {
        return path_;
    }

    const BinaryType getBinaryType() {
        for (auto type : BINARY_TYPES) {
            switch (type) {
                case BinaryType::WINDOWS_PE_32:
                    if (isWindowsPE32()) {
                        return BinaryType::WINDOWS_PE_32;
                    }
                    break;
                case BinaryType::WINDOWS_PE_64:
                    if (isWindowsPE64()) {
                        return BinaryType::WINDOWS_PE_64;
                    }
            }
        }
        return BinaryType::UNKNOWN;
    }

    const bool isWindowsPE32() {
        // At location 0x3c, the stub has the file offset to the PE signature.
        // This information enables Windows to properly execute the image file,
        // even though it has an MS-DOS stub. This file offset is placed at
        // location 0x3c during linking.
        std::byte x3c = session_->getByte(0x3C);

        // TODO what if file is too small?
        auto peSig = session_->getRange((uint64_t)x3c, 4);

        // What's the best way to compare my range to an array?
        int i = 0;
        for (auto b : peSig) {
            if (b != PE_SIG[i++]) {
                return false;
            }
        }

        return true;
    }

    const bool isWindowsPE64() {
        return false;
    }

    const std::string describe() {
        BinaryType type = getBinaryType();
        switch (type) {
            case BinaryType::WINDOWS_PE_32:
                return "Windows PE 32";
            case BinaryType::WINDOWS_PE_64:
                return "Windows PE 64";
        }
        return "Unknown";
    }

private:
    BinarySession(const BinarySession&) = delete;
    BinarySession& operator=(BinarySession) = delete;

    std::string path_;
    std::unique_ptr<FileSession> session_;

    static constexpr const std::byte PE_SIG[] = { (std::byte)'P', (std::byte)'E', (std::byte)0, (std::byte)0 };
};


int main(int argc, char* argv[])
{
    // TODO: Use a command line parsing library.

    if (argc < 2) {
        std::cout << "Missing file argument" << std::endl;
        return 0;
    }

    // Invokes copy constructor
    std::string filePath = argv[1];
    if (!std::filesystem::exists(filePath)) {
        std::cout << "File does not exist: " << filePath << std::endl;
        return 1;
    }

    BinarySession session(filePath);
    std::cout << session.describe() << std::endl;
    return 0;
}
