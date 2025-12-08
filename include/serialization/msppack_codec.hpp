#pragma once
#include <msgpack.hpp>
#include "serialization/binary_codec.hpp"

namespace zlc {
    template <typename T>
    void encode(const T& obj, ByteBuffer& out) {
        out.size = 0;
        msgpack::packer<ByteBuffer> pk(out);
        pk.pack(obj);
    }

    template <typename T>
    void decode(const ByteView& bv, T& out) {
        msgpack::object_handle oh = msgpack::unpack(
            reinterpret_cast<const char*>(bv.data),
            bv.size
        );
        oh.get().convert(out);
    }
}
