#include "TLVCodec.h"
#include <cstring>
#include <stdexcept>

namespace matter {
namespace tlv {

// --- Writer ---

void Writer::writeUint8(uint8_t v) {
    buffer_.push_back(v);
}

void Writer::writeUint16(uint16_t v) {
    buffer_.push_back(static_cast<uint8_t>(v & 0xFF));
    buffer_.push_back(static_cast<uint8_t>((v >> 8) & 0xFF));
}

void Writer::writeUint32(uint32_t v) {
    for (int i = 0; i < 4; i++) {
        buffer_.push_back(static_cast<uint8_t>((v >> (i * 8)) & 0xFF));
    }
}

void Writer::writeUint64(uint64_t v) {
    for (int i = 0; i < 8; i++) {
        buffer_.push_back(static_cast<uint8_t>((v >> (i * 8)) & 0xFF));
    }
}

void Writer::writeControlAndTag(ElementType type, std::optional<uint8_t> tag) {
    uint8_t control = static_cast<uint8_t>(type);
    if (tag.has_value()) {
        control |= static_cast<uint8_t>(TagControl::ContextSpecific);
        writeUint8(control);
        writeUint8(tag.value());
    } else {
        writeUint8(control);
    }
}

void Writer::startStructure(std::optional<uint8_t> tag) {
    writeControlAndTag(ElementType::Structure, tag);
}

void Writer::startArray(std::optional<uint8_t> tag) {
    writeControlAndTag(ElementType::Array, tag);
}

void Writer::endContainer() {
    writeUint8(static_cast<uint8_t>(ElementType::EndContainer));
}

void Writer::putBool(std::optional<uint8_t> tag, bool value) {
    writeControlAndTag(value ? ElementType::BoolTrue : ElementType::BoolFalse, tag);
}

void Writer::putInt(std::optional<uint8_t> tag, int64_t value) {
    if (value >= -128 && value <= 127) {
        writeControlAndTag(ElementType::SignedInt8, tag);
        writeUint8(static_cast<uint8_t>(value));
    } else if (value >= -32768 && value <= 32767) {
        writeControlAndTag(ElementType::SignedInt16, tag);
        writeUint16(static_cast<uint16_t>(value));
    } else if (value >= INT32_MIN && value <= INT32_MAX) {
        writeControlAndTag(ElementType::SignedInt32, tag);
        writeUint32(static_cast<uint32_t>(value));
    } else {
        writeControlAndTag(ElementType::SignedInt64, tag);
        writeUint64(static_cast<uint64_t>(value));
    }
}

void Writer::putUint(std::optional<uint8_t> tag, uint64_t value) {
    if (value <= 0xFF) {
        writeControlAndTag(ElementType::UnsignedInt8, tag);
        writeUint8(static_cast<uint8_t>(value));
    } else if (value <= 0xFFFF) {
        writeControlAndTag(ElementType::UnsignedInt16, tag);
        writeUint16(static_cast<uint16_t>(value));
    } else if (value <= 0xFFFFFFFF) {
        writeControlAndTag(ElementType::UnsignedInt32, tag);
        writeUint32(static_cast<uint32_t>(value));
    } else {
        writeControlAndTag(ElementType::UnsignedInt64, tag);
        writeUint64(value);
    }
}

void Writer::putFloat(std::optional<uint8_t> tag, float value) {
    writeControlAndTag(ElementType::Float32, tag);
    uint32_t bits;
    std::memcpy(&bits, &value, sizeof(bits));
    writeUint32(bits);
}

void Writer::putString(std::optional<uint8_t> tag, const std::string& value) {
    if (value.size() <= 0xFF) {
        writeControlAndTag(ElementType::UTF8String1, tag);
        writeUint8(static_cast<uint8_t>(value.size()));
    } else {
        writeControlAndTag(ElementType::UTF8String2, tag);
        writeUint16(static_cast<uint16_t>(value.size()));
    }
    buffer_.insert(buffer_.end(), value.begin(), value.end());
}

void Writer::putBytes(std::optional<uint8_t> tag, const std::vector<uint8_t>& value) {
    if (value.size() <= 0xFF) {
        writeControlAndTag(ElementType::ByteString1, tag);
        writeUint8(static_cast<uint8_t>(value.size()));
    } else {
        writeControlAndTag(ElementType::ByteString2, tag);
        writeUint16(static_cast<uint16_t>(value.size()));
    }
    buffer_.insert(buffer_.end(), value.begin(), value.end());
}

void Writer::putNull(std::optional<uint8_t> tag) {
    writeControlAndTag(ElementType::Null, tag);
}

// --- Reader ---

Reader::Reader(const uint8_t* data, size_t length)
    : data_(data), length_(length) {}

Reader::Reader(const std::vector<uint8_t>& data)
    : data_(data.data()), length_(data.size()) {}

uint8_t Reader::readUint8() {
    if (offset_ >= length_) throw std::runtime_error("TLV read past end");
    return data_[offset_++];
}

uint16_t Reader::readUint16() {
    uint16_t v = readUint8();
    v |= static_cast<uint16_t>(readUint8()) << 8;
    return v;
}

uint32_t Reader::readUint32() {
    uint32_t v = 0;
    for (int i = 0; i < 4; i++) {
        v |= static_cast<uint32_t>(readUint8()) << (i * 8);
    }
    return v;
}

uint64_t Reader::readUint64() {
    uint64_t v = 0;
    for (int i = 0; i < 8; i++) {
        v |= static_cast<uint64_t>(readUint8()) << (i * 8);
    }
    return v;
}

bool Reader::next() {
    if (offset_ >= length_) return false;

    uint8_t control = readUint8();
    auto tagControl = static_cast<TagControl>(control & 0xE0);
    auto elemType = static_cast<ElementType>(control & 0x1F);

    current_ = Element{};
    current_.type = elemType;

    // Read tag
    if (tagControl == TagControl::ContextSpecific) {
        current_.contextTag = readUint8();
    }

    // Read value
    switch (elemType) {
        case ElementType::SignedInt8:
            current_.intVal = static_cast<int8_t>(readUint8());
            break;
        case ElementType::SignedInt16:
            current_.intVal = static_cast<int16_t>(readUint16());
            break;
        case ElementType::SignedInt32:
            current_.intVal = static_cast<int32_t>(readUint32());
            break;
        case ElementType::SignedInt64:
            current_.intVal = static_cast<int64_t>(readUint64());
            break;
        case ElementType::UnsignedInt8:
            current_.uintVal = readUint8();
            break;
        case ElementType::UnsignedInt16:
            current_.uintVal = readUint16();
            break;
        case ElementType::UnsignedInt32:
            current_.uintVal = readUint32();
            break;
        case ElementType::UnsignedInt64:
            current_.uintVal = readUint64();
            break;
        case ElementType::BoolFalse:
            current_.boolVal = false;
            break;
        case ElementType::BoolTrue:
            current_.boolVal = true;
            break;
        case ElementType::Float32: {
            uint32_t bits = readUint32();
            std::memcpy(&current_.floatVal, &bits, sizeof(float));
            break;
        }
        case ElementType::Float64: {
            uint64_t bits = readUint64();
            std::memcpy(&current_.doubleVal, &bits, sizeof(double));
            break;
        }
        case ElementType::UTF8String1: {
            uint8_t len = readUint8();
            current_.strVal = std::string(reinterpret_cast<const char*>(data_ + offset_), len);
            offset_ += len;
            break;
        }
        case ElementType::UTF8String2: {
            uint16_t len = readUint16();
            current_.strVal = std::string(reinterpret_cast<const char*>(data_ + offset_), len);
            offset_ += len;
            break;
        }
        case ElementType::ByteString1: {
            uint8_t len = readUint8();
            current_.bytesVal.assign(data_ + offset_, data_ + offset_ + len);
            offset_ += len;
            break;
        }
        case ElementType::ByteString2: {
            uint16_t len = readUint16();
            current_.bytesVal.assign(data_ + offset_, data_ + offset_ + len);
            offset_ += len;
            break;
        }
        case ElementType::Null:
            break;
        case ElementType::Structure:
        case ElementType::Array:
        case ElementType::List:
            depth_++;
            break;
        case ElementType::EndContainer:
            depth_--;
            break;
    }

    return true;
}

std::vector<Element> Reader::decodeAll(const std::vector<uint8_t>& data) {
    std::vector<Element> elements;
    Reader reader(data);
    while (reader.next()) {
        elements.push_back(reader.current());
    }
    return elements;
}

} // namespace tlv
} // namespace matter
