#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <optional>
#include <stdexcept>

namespace matter {
namespace tlv {

// Matter TLV type tags (from the spec, section 7.1)
enum class ElementType : uint8_t {
    SignedInt8    = 0x00,
    SignedInt16   = 0x01,
    SignedInt32   = 0x02,
    SignedInt64   = 0x03,
    UnsignedInt8  = 0x04,
    UnsignedInt16 = 0x05,
    UnsignedInt32 = 0x06,
    UnsignedInt64 = 0x07,
    BoolFalse    = 0x08,
    BoolTrue     = 0x09,
    Float32      = 0x0A,
    Float64      = 0x0B,
    UTF8String1  = 0x0C,
    UTF8String2  = 0x0D,
    ByteString1  = 0x10,
    ByteString2  = 0x11,
    Null         = 0x14,
    Structure    = 0x15,
    Array        = 0x16,
    List         = 0x17,
    EndContainer = 0x18,
};

// Tag control field
enum class TagControl : uint8_t {
    Anonymous        = 0x00,
    ContextSpecific  = 0x20,
    CommonProfile2   = 0x40,
    CommonProfile4   = 0x60,
    ImplicitProfile2 = 0x80,
    ImplicitProfile4 = 0xA0,
    FullyQualified6  = 0xC0,
    FullyQualified8  = 0xE0,
};

// A decoded TLV element
struct Element {
    ElementType type;
    std::optional<uint8_t> contextTag;
    int64_t intVal = 0;
    uint64_t uintVal = 0;
    bool boolVal = false;
    float floatVal = 0.0f;
    double doubleVal = 0.0;
    std::string strVal;
    std::vector<uint8_t> bytesVal;
};

// TLV Writer — encodes data into Matter TLV format
class Writer {
public:
    Writer() = default;

    void startStructure(std::optional<uint8_t> tag = std::nullopt);
    void startArray(std::optional<uint8_t> tag = std::nullopt);
    void endContainer();

    void putBool(std::optional<uint8_t> tag, bool value);
    void putInt(std::optional<uint8_t> tag, int64_t value);
    void putUint(std::optional<uint8_t> tag, uint64_t value);
    void putFloat(std::optional<uint8_t> tag, float value);
    void putString(std::optional<uint8_t> tag, const std::string& value);
    void putBytes(std::optional<uint8_t> tag, const std::vector<uint8_t>& value);
    void putNull(std::optional<uint8_t> tag);

    const std::vector<uint8_t>& data() const { return buffer_; }
    size_t size() const { return buffer_.size(); }
    void clear() { buffer_.clear(); }

private:
    std::vector<uint8_t> buffer_;

    void writeControlAndTag(ElementType type, std::optional<uint8_t> tag);
    void writeUint8(uint8_t v);
    void writeUint16(uint16_t v);
    void writeUint32(uint32_t v);
    void writeUint64(uint64_t v);
};

// TLV Reader — decodes Matter TLV data
class Reader {
public:
    explicit Reader(const uint8_t* data, size_t length);
    explicit Reader(const std::vector<uint8_t>& data);

    bool next();
    const Element& current() const { return current_; }
    bool isContainerOpen() const { return depth_ > 0; }
    int depth() const { return depth_; }

    // Convenience: decode entire buffer into flat element list
    static std::vector<Element> decodeAll(const std::vector<uint8_t>& data);

private:
    const uint8_t* data_;
    size_t length_;
    size_t offset_ = 0;
    int depth_ = 0;
    Element current_;

    uint8_t readUint8();
    uint16_t readUint16();
    uint32_t readUint32();
    uint64_t readUint64();
};

} // namespace tlv
} // namespace matter
