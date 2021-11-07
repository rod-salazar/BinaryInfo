module;

#include <cassert>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>
#include <cassert>
#include <ranges>
#include <type_traits>

export module FileSession;

namespace IO {
    template <typename U, typename V>
    using smaller_type = std::conditional_t < sizeof(U) < sizeof(V), U, V > ;

    template <typename U, typename V>
    using larger_type = std::conditional_t < sizeof(V) < sizeof(U), U, V > ;

    // std::min, except it automatically uses the smaller type as the return type.
    // https://stackoverflow.com/a/31361217/11763503
    template <typename U, typename V>
    smaller_type<U, V> min_t(const U& u, const V& v) {
        // Result of std::min is guaranteed to fit in the smaller type.
        return static_cast<smaller_type<U, V>>(std::min<larger_type<U, V>>(u, v));
    }

    // This class encapsulates the functionality of reading a range of bytes in a file
    // from a given byte offset. It assumes the file will likely be read sequentially from
    // beginning to end, so it may read ahead and buffer more data than requested.
    export class FileSession {
        using ReadLengthType = uint16_t;

        // Insert private section up top due to auto type deduction requiring this method order.
    private:
        /*
         * This will read CACHE_SIZE_BYTES bytes from the file or until the file ends,
         * place it into cache and return a range view into the cache buffer.
         *
         * If the requested range is already in the cache it will just return a range view.
         * If the requested len would go out of bounds of the file, the range view will be
         * smaller than len.
         */
        auto read(const uint64_t byteOffset, const ReadLengthType len) {
            if (byteOffset >= fileLength_) {
                throw new std::exception("byteoffset is beyond file length");
            }

            if (byteOffset < 0) {
                throw new std::exception("byteoffset must be >= 0");
            }

            if (len <= 0) {
                throw new std::exception("len must be > 0");
            }

            // Returned view length should be smaller than len if end of file is reached.
            uint16_t viewLength = min_t(len, fileLength_ - byteOffset);

            // If we have the entire range already buffered, let's return a range view into our buffer.
            // We want to return the range [byteOffset, viewEndOffset).
            uint64_t viewEndOffset = byteOffset + viewLength;
            uint64_t bufferEndOffset = bufferByteOffset_ + bufferValidBytes;
            if (bufferByteOffset_ <= byteOffset && viewEndOffset <= bufferEndOffset) {
                assert(byteOffset - bufferByteOffset_ <= std::numeric_limits<uint16_t>::max());
                uint16_t bytesBeforeView = static_cast<uint16_t>(byteOffset - bufferByteOffset_);
                return buffer_ | std::ranges::views::drop(static_cast<uint16_t>(bytesBeforeView)) | std::ranges::views::take(viewLength);
            }

            // Don't read beyond end of the file.
            uint16_t readLength = min_t(CACHE_SIZE_BYTES, fileLength_ - byteOffset);

            // Casting byte* to char*. TODO: Does c++ guarantee this is okay at compile time?
            stream_->seekg(byteOffset);
            stream_->read((char*)buffer_.data(), readLength);

            bufferValidBytes = readLength;
            bufferByteOffset_ = byteOffset;

            // We have to drop(0) here to have a consistent return type.
            return buffer_ | std::ranges::views::drop(0) | std::ranges::views::take(viewLength);
        }

    public:
        FileSession(const std::string& path) {
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

        /**
         * This function does not transfer ownership of the data.
         * The FileSession object must outlive the returned range.
         */
        auto getRangeOfBytes(const uint64_t byteOffset, const ReadLengthType len) {
            return read(byteOffset, len);
        }

        std::byte getByte(const uint64_t byteOffset) {
            return *getRangeOfBytes(byteOffset, 1).begin();
        }

    private:

        // Best guess based off Windows documented recommendation: https://bit.ly/3GXoYK9
        // Linux would have other approaches to choose this.
        static constexpr uint16_t CACHE_SIZE_BYTES = 65535;

        // CACHE_SIZE_BYTES should be at least as large as the len parameter on the 
        // FileSystem::read function so that the read is always cacheable without
        // needing to reallocate to larger size.
        static_assert(CACHE_SIZE_BYTES >= std::numeric_limits<ReadLengthType>::max());

        std::unique_ptr<std::ifstream> stream_;
        uint64_t fileLength_;

        std::vector<std::byte> buffer_;
        uint64_t bufferByteOffset_ = -1;
        uint16_t bufferValidBytes = 0;

        FileSession(const FileSession&) = delete;
        FileSession& operator=(FileSession) = delete;
    };
}