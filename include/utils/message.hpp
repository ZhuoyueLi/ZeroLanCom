#pragma once
#include <string>
#include <random>
#include <sstream>
#include <iomanip>
#include <array>

namespace zlc {


inline std::string generateUUID() {
    static thread_local std::mt19937_64 rng{std::random_device{}()};
    static thread_local std::uniform_int_distribution<uint32_t> dist32(0, 0xFFFFFFFF);

    std::array<uint8_t, 16> bytes;
    for (int i = 0; i < 4; ++i) {
        uint32_t r = dist32(rng);
        bytes[i * 4 + 0] = static_cast<uint8_t>((r >> 24) & 0xFF);
        bytes[i * 4 + 1] = static_cast<uint8_t>((r >> 16) & 0xFF);
        bytes[i * 4 + 2] = static_cast<uint8_t>((r >> 8) & 0xFF);
        bytes[i * 4 + 3] = static_cast<uint8_t>((r) & 0xFF);
    }

    bytes[6] = (bytes[6] & 0x0F) | 0x40;
    bytes[8] = (bytes[8] & 0x3F) | 0x80;

    // format as 8-4-4-4-12
    std::ostringstream oss;
    oss << std::hex << std::nouppercase << std::setfill('0');

    for (int i = 0; i < 16; ++i) {
        oss << std::setw(2) << static_cast<int>(bytes[i]);
        if (i == 3 || i == 5 || i == 7 || i == 9) {
            oss << '-';
        }
    }

    return oss.str(); // length 36
}

}
