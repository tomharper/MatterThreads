#pragma once

#include <cstdint>
#include <cstddef>
#include <span>
#include <vector>

namespace mt {

// Convenience aliases for byte spans
using ByteSpan      = std::span<const uint8_t>;
using MutableByteSpan = std::span<uint8_t>;

class ByteBuffer {
    std::vector<uint8_t> data_;
public:
    ByteBuffer() = default;
    explicit ByteBuffer(size_t capacity) { data_.reserve(capacity); }
    explicit ByteBuffer(std::vector<uint8_t> data) : data_(std::move(data)) {}
    ByteBuffer(const uint8_t* ptr, size_t len) : data_(ptr, ptr + len) {}

    void append(const uint8_t* ptr, size_t len) { data_.insert(data_.end(), ptr, ptr + len); }
    void append(ByteSpan span) { append(span.data(), span.size()); }

    template<typename T>
    void appendValue(T val) {
        auto* p = reinterpret_cast<const uint8_t*>(&val);
        append(p, sizeof(T));
    }

    void clear() { data_.clear(); }
    void resize(size_t n) { data_.resize(n); }

    uint8_t* data() { return data_.data(); }
    const uint8_t* data() const { return data_.data(); }
    size_t size() const { return data_.size(); }
    bool empty() const { return data_.empty(); }

    ByteSpan span() const { return {data_.data(), data_.size()}; }
    MutableByteSpan mutableSpan() { return {data_.data(), data_.size()}; }

    std::vector<uint8_t>& vec() { return data_; }
    const std::vector<uint8_t>& vec() const { return data_; }
};

} // namespace mt
