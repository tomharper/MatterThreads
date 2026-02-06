#include "matter/TLV.h"
#include <cstring>
#include <stdexcept>

namespace mt {

// --- TLVWriter ---

static void writeTagType(std::vector<uint8_t>& buf, uint8_t tag, TLVType type) {
    // Simplified: 1-byte context tag + type
    buf.push_back(static_cast<uint8_t>(type) | ((tag & 0x07) << 5));
}

void TLVWriter::putUint8(uint8_t tag, uint8_t value) {
    writeTagType(buffer_, tag, TLVType::UnsignedInt);
    buffer_.push_back(1); // length
    buffer_.push_back(value);
}

void TLVWriter::putUint16(uint8_t tag, uint16_t value) {
    writeTagType(buffer_, tag, TLVType::UnsignedInt);
    buffer_.push_back(2);
    buffer_.push_back(static_cast<uint8_t>(value & 0xFF));
    buffer_.push_back(static_cast<uint8_t>((value >> 8) & 0xFF));
}

void TLVWriter::putUint32(uint8_t tag, uint32_t value) {
    writeTagType(buffer_, tag, TLVType::UnsignedInt);
    buffer_.push_back(4);
    for (int i = 0; i < 4; ++i)
        buffer_.push_back(static_cast<uint8_t>((value >> (i * 8)) & 0xFF));
}

void TLVWriter::putUint64(uint8_t tag, uint64_t value) {
    writeTagType(buffer_, tag, TLVType::UnsignedInt);
    buffer_.push_back(8);
    for (int i = 0; i < 8; ++i)
        buffer_.push_back(static_cast<uint8_t>((value >> (i * 8)) & 0xFF));
}

void TLVWriter::putInt32(uint8_t tag, int32_t value) {
    writeTagType(buffer_, tag, TLVType::SignedInt);
    buffer_.push_back(4);
    uint32_t uval;
    std::memcpy(&uval, &value, 4);
    for (int i = 0; i < 4; ++i)
        buffer_.push_back(static_cast<uint8_t>((uval >> (i * 8)) & 0xFF));
}

void TLVWriter::putBool(uint8_t tag, bool value) {
    writeTagType(buffer_, tag, TLVType::Boolean);
    buffer_.push_back(1);
    buffer_.push_back(value ? 1 : 0);
}

void TLVWriter::putString(uint8_t tag, std::string_view value) {
    writeTagType(buffer_, tag, TLVType::UTF8String);
    auto len = static_cast<uint16_t>(value.size());
    buffer_.push_back(static_cast<uint8_t>(len & 0xFF));
    buffer_.push_back(static_cast<uint8_t>((len >> 8) & 0xFF));
    buffer_.insert(buffer_.end(), value.begin(), value.end());
}

void TLVWriter::putBytes(uint8_t tag, std::span<const uint8_t> value) {
    writeTagType(buffer_, tag, TLVType::ByteString);
    auto len = static_cast<uint16_t>(value.size());
    buffer_.push_back(static_cast<uint8_t>(len & 0xFF));
    buffer_.push_back(static_cast<uint8_t>((len >> 8) & 0xFF));
    buffer_.insert(buffer_.end(), value.begin(), value.end());
}

void TLVWriter::putNull(uint8_t tag) {
    writeTagType(buffer_, tag, TLVType::Null);
}

void TLVWriter::openStructure(uint8_t tag) {
    writeTagType(buffer_, tag, TLVType::Structure);
}

void TLVWriter::openArray(uint8_t tag) {
    writeTagType(buffer_, tag, TLVType::Array);
}

void TLVWriter::closeContainer() {
    buffer_.push_back(static_cast<uint8_t>(TLVType::EndOfContainer));
}

// --- TLVReader ---

bool TLVReader::next() {
    if (pos_ >= len_) return false;

    uint8_t control = data_[pos_++];
    current_tag_ = (control >> 5) & 0x07;
    current_type_ = static_cast<TLVType>(control & 0x1F);

    current_value_offset_ = pos_;
    current_value_len_ = 0;

    switch (current_type_) {
    case TLVType::EndOfContainer:
        break;

    case TLVType::Null:
        break;

    case TLVType::Boolean:
    case TLVType::SignedInt:
    case TLVType::UnsignedInt:
    case TLVType::Float:
        if (pos_ < len_) {
            current_value_len_ = data_[pos_++]; // length byte
            current_value_offset_ = pos_;
            pos_ += current_value_len_;
        }
        break;

    case TLVType::UTF8String:
    case TLVType::ByteString:
        if (pos_ + 1 < len_) {
            uint16_t slen = static_cast<uint16_t>(data_[pos_]) |
                            (static_cast<uint16_t>(data_[pos_ + 1]) << 8);
            pos_ += 2;
            current_value_offset_ = pos_;
            current_value_len_ = slen;
            pos_ += slen;
        }
        break;

    case TLVType::Structure:
    case TLVType::Array:
    case TLVType::List:
        // Container open — value is nested elements
        current_value_offset_ = pos_;
        break;
    }

    return true;
}

uint8_t TLVReader::getUint8() const {
    if (current_value_len_ < 1) return 0;
    return data_[current_value_offset_];
}

uint16_t TLVReader::getUint16() const {
    if (current_value_len_ < 2) return getUint8();
    return static_cast<uint16_t>(data_[current_value_offset_]) |
           (static_cast<uint16_t>(data_[current_value_offset_ + 1]) << 8);
}

uint32_t TLVReader::getUint32() const {
    if (current_value_len_ < 4) return getUint16();
    uint32_t val = 0;
    for (size_t i = 0; i < 4; ++i)
        val |= (static_cast<uint32_t>(data_[current_value_offset_ + i]) << (i * 8));
    return val;
}

uint64_t TLVReader::getUint64() const {
    if (current_value_len_ < 8) return getUint32();
    uint64_t val = 0;
    for (size_t i = 0; i < 8; ++i)
        val |= (static_cast<uint64_t>(data_[current_value_offset_ + i]) << (i * 8));
    return val;
}

int32_t TLVReader::getInt32() const {
    uint32_t uval = getUint32();
    int32_t val;
    std::memcpy(&val, &uval, 4);
    return val;
}

bool TLVReader::getBool() const {
    return getUint8() != 0;
}

std::string TLVReader::getString() const {
    return std::string(reinterpret_cast<const char*>(data_ + current_value_offset_),
                       current_value_len_);
}

std::vector<uint8_t> TLVReader::getBytes() const {
    return std::vector<uint8_t>(data_ + current_value_offset_,
                                 data_ + current_value_offset_ + current_value_len_);
}

void TLVReader::enterContainer() {
    // pos_ is already at the first element inside the container
}

void TLVReader::exitContainer() {
    // Skip until EndOfContainer
    int depth = 1;
    while (depth > 0 && pos_ < len_) {
        if (next()) {
            if (current_type_ == TLVType::Structure ||
                current_type_ == TLVType::Array ||
                current_type_ == TLVType::List) {
                ++depth;
            } else if (current_type_ == TLVType::EndOfContainer) {
                --depth;
            }
        } else {
            break;
        }
    }
}

} // namespace mt
