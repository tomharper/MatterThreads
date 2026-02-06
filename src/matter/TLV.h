#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>
#include <span>

namespace mt {

enum class TLVType : uint8_t {
    SignedInt       = 0x00,
    UnsignedInt    = 0x04,
    Boolean        = 0x08,
    Float          = 0x0A,
    UTF8String     = 0x0C,
    ByteString     = 0x10,
    Null           = 0x14,
    Structure      = 0x15,
    Array          = 0x16,
    List           = 0x17,
    EndOfContainer = 0x18
};

class TLVWriter {
    std::vector<uint8_t>& buffer_;

public:
    explicit TLVWriter(std::vector<uint8_t>& buffer) : buffer_(buffer) {}

    void putUint8(uint8_t tag, uint8_t value);
    void putUint16(uint8_t tag, uint16_t value);
    void putUint32(uint8_t tag, uint32_t value);
    void putUint64(uint8_t tag, uint64_t value);
    void putInt32(uint8_t tag, int32_t value);
    void putBool(uint8_t tag, bool value);
    void putString(uint8_t tag, std::string_view value);
    void putBytes(uint8_t tag, std::span<const uint8_t> value);
    void putNull(uint8_t tag);
    void openStructure(uint8_t tag);
    void openArray(uint8_t tag);
    void closeContainer();

    size_t size() const { return buffer_.size(); }
    const std::vector<uint8_t>& data() const { return buffer_; }
};

class TLVReader {
    const uint8_t* data_;
    size_t len_;
    size_t pos_ = 0;

    TLVType current_type_ = TLVType::Null;
    uint8_t current_tag_ = 0;
    size_t current_value_offset_ = 0;
    size_t current_value_len_ = 0;

public:
    TLVReader(const uint8_t* data, size_t len) : data_(data), len_(len) {}
    explicit TLVReader(std::span<const uint8_t> span) : data_(span.data()), len_(span.size()) {}

    bool next();
    TLVType type() const { return current_type_; }
    uint8_t tag() const { return current_tag_; }

    uint8_t getUint8() const;
    uint16_t getUint16() const;
    uint32_t getUint32() const;
    uint64_t getUint64() const;
    int32_t getInt32() const;
    bool getBool() const;
    std::string getString() const;
    std::vector<uint8_t> getBytes() const;

    void enterContainer();
    void exitContainer();

    bool atEnd() const { return pos_ >= len_; }
    size_t remaining() const { return pos_ < len_ ? len_ - pos_ : 0; }
};

} // namespace mt
