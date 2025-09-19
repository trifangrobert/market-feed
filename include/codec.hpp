#pragma once
#include <vector>
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <type_traits>
#include <span>
#include "wire.hpp"


namespace codec {
    struct FrameView {
        Header hdr;
        std::span<const uint8_t> body;
    };

    template <typename T>
    std::vector<uint8_t> encode(const T& obj) {
        static_assert(std::is_trivially_copyable_v<T>, "T must be trivially copyable");
        std::vector<uint8_t> buf(sizeof(T));
        std::memcpy(buf.data(), &obj, sizeof(T));
        return buf;
    }

    template <typename T>
    T decode(const uint8_t* data, size_t len) {
        static_assert(std::is_trivially_copyable_v<T>, "T must be trivially copyable");
        if (len < sizeof(T)) {
            throw std::runtime_error("Buffer too small for decode");
        }
        T obj{};
        std::memcpy(&obj, data, sizeof(T));
        return obj;
    }

    template <typename BodyT>
    std::vector<uint8_t> pack(Header& hdr, const BodyT& body) {
        hdr.size = sizeof(Header) + sizeof(BodyT);

        auto hbytes = encode<Header>(hdr);
        auto bbytes = encode<BodyT>(body);

        std::vector<uint8_t> out;
        out.reserve(hbytes.size() + bbytes.size());
        out.insert(out.end(), hbytes.begin(), hbytes.end());
        out.insert(out.end(), bbytes.begin(), bbytes.end());

        return out;
    }

    inline FrameView unpack_frame(std::span<const uint8_t> frame) {
        if (frame.size() < sizeof(Header)) {
            throw std::runtime_error("unable to unpack frame: frame shorter than header size");
        }
        Header h = decode<Header>(frame.data(), sizeof(Header));
        if (h.size != frame.size()) {
            throw std::runtime_error("unable to unpack frame: frame size doesn't match header metadata");
        }
        return FrameView {h, frame.subspan(sizeof(Header))};
    }

    template <typename T>
    T decode_body(std::span<const uint8_t> body) {
        if (body.size() != sizeof(T)) {
            throw std::runtime_error("unable to decode body: body size mismatch");
        }
        return decode<T>(body.data(), sizeof(T));
    }

    template <typename T>
    T decode_expected(std::span<const uint8_t> frame, MsgType expected) {
        FrameView fv = unpack_frame(frame);
        if (fv.hdr.type != static_cast<uint8_t>(expected)) {
            throw std::runtime_error("different message expected");
        }
        return decode_body<T>(fv.body);
    }

}