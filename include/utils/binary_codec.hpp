#pragma once
#include <cstdint>
#include <vector>
#include <string>
#include <stdexcept>

// =======================
// Zero-copy view for decode
// =======================

struct ByteView {
    const uint8_t* data;
    size_t size;

    const uint8_t* begin() const { return data; }
    const uint8_t* end()   const { return data + size; }
};

// =======================
// Writer（for encode）
// =======================

struct BinWriter {
    std::vector<uint8_t> buf;

    void write_u16(uint16_t v) {
        buf.push_back(v >> 8);
        buf.push_back(v & 0xFF);
    }

    void write_u32(uint32_t v) {
        buf.push_back((v >> 24) & 0xFF);
        buf.push_back((v >> 16) & 0xFF);
        buf.push_back((v >> 8) & 0xFF);
        buf.push_back(v & 0xFF);
    }

    void write_string(const std::string& s) {
        write_u16((uint16_t)s.size());
        buf.insert(buf.end(), s.begin(), s.end());
    }

    void write_fixed_string(const std::string& s, size_t len) {
        if (s.size() != len)
            throw std::runtime_error("fixed string length mismatch");
        buf.insert(buf.end(), s.begin(), s.end());
    }
};

// =======================
// Reader（for decode）
// Zero-copy
// =======================

struct BinReader {
    ByteView view;

    uint16_t read_u16() {
        if (view.size < 2) throw std::runtime_error("decode u16 OOB");
        uint16_t v = (view.data[0] << 8) | view.data[1];
        view.data += 2; view.size -= 2;
        return v;
    }

    uint32_t read_u32() {
        if (view.size < 4) throw std::runtime_error("decode u32 OOB");
        uint32_t v = (view.data[0] << 24) |
                     (view.data[1] << 16) |
                     (view.data[2] << 8)  |
                     view.data[3];
        view.data += 4; view.size -= 4;
        return v;
    }

    std::string read_string() {
        uint16_t len = read_u16();
        if (view.size < len) throw std::runtime_error("decode string OOB");
        std::string s((const char*)view.data, len);
        view.data += len; view.size -= len;
        return s;
    }

    std::string read_fixed_string(size_t len) {
        if (view.size < len) throw std::runtime_error("decode fixed string OOB");
        std::string s((const char*)view.data, len);
        view.data += len; view.size -= len;
        return s;
    }
};

std::string decode(ByteView payload) {
    constexpr size_t kMaxLen = 1024;

    if (!payload.data || payload.size == 0)
        return std::string();

    size_t real_len = 0;
    while (real_len < payload.size && real_len < kMaxLen && payload.data[real_len] != 0) {
        ++real_len;
    }

    return std::string(reinterpret_cast<const char*>(payload.data), real_len);
}
