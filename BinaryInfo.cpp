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

import FileSession;


// TODO: How can I write a unit-test?
// TODO: How can I hook this up to valgrind or similar?

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
    BinarySession(const std::string& path) : path_(path) {
        if (!std::filesystem::exists(path)) {
            throw new std::invalid_argument("File does not exist: " + path);
        }
        session_ = std::make_unique<IO::FileSession>(path);

        // Test code
        std::ifstream f(path);
        f.seekg(0, std::ios::end);
        const int len = f.tellg();
        f.seekg(0, std::ios::beg);

        for (int i = 0; i < len; i += 1000) {
            auto r = session_->getRangeOfBytes(i, 10000);
            for (auto b : r) {
                if ((char)b == '?') std::cout << '?';
            }
        }
        // Test code
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
        auto peSig = session_->getRangeOfBytes((uint64_t)x3c, 4);

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
    std::unique_ptr<IO::FileSession> session_;

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
