#include <gtest/gtest.h>
#include "matter/TLV.h"

using namespace mt;

TEST(TLV, WriteAndReadUint) {
    std::vector<uint8_t> buf;
    TLVWriter writer(buf);

    writer.putUint8(1, 42);
    writer.putUint16(2, 1234);
    writer.putUint32(3, 0xDEADBEEF);

    TLVReader reader(buf.data(), buf.size());

    ASSERT_TRUE(reader.next());
    EXPECT_EQ(reader.tag(), 1);
    EXPECT_EQ(reader.getUint8(), 42);

    ASSERT_TRUE(reader.next());
    EXPECT_EQ(reader.tag(), 2);
    EXPECT_EQ(reader.getUint16(), 1234);

    ASSERT_TRUE(reader.next());
    EXPECT_EQ(reader.tag(), 3);
    EXPECT_EQ(reader.getUint32(), 0xDEADBEEF);
}

TEST(TLV, WriteAndReadBool) {
    std::vector<uint8_t> buf;
    TLVWriter writer(buf);

    writer.putBool(0, true);
    writer.putBool(1, false);

    TLVReader reader(buf.data(), buf.size());

    ASSERT_TRUE(reader.next());
    EXPECT_TRUE(reader.getBool());

    ASSERT_TRUE(reader.next());
    EXPECT_FALSE(reader.getBool());
}

TEST(TLV, WriteAndReadString) {
    std::vector<uint8_t> buf;
    TLVWriter writer(buf);

    writer.putString(0, "Hello, Matter!");

    TLVReader reader(buf.data(), buf.size());

    ASSERT_TRUE(reader.next());
    EXPECT_EQ(reader.getString(), "Hello, Matter!");
}

TEST(TLV, WriteAndReadBytes) {
    std::vector<uint8_t> buf;
    TLVWriter writer(buf);

    std::vector<uint8_t> data = {0xDE, 0xAD, 0xBE, 0xEF};
    writer.putBytes(0, data);

    TLVReader reader(buf.data(), buf.size());

    ASSERT_TRUE(reader.next());
    auto result = reader.getBytes();
    EXPECT_EQ(result, data);
}

TEST(TLV, Structure) {
    std::vector<uint8_t> buf;
    TLVWriter writer(buf);

    writer.openStructure(0);
    writer.putUint16(1, 100);
    writer.putString(2, "test");
    writer.closeContainer();

    TLVReader reader(buf.data(), buf.size());

    ASSERT_TRUE(reader.next());
    EXPECT_EQ(reader.type(), TLVType::Structure);

    reader.enterContainer();

    ASSERT_TRUE(reader.next());
    EXPECT_EQ(reader.getUint16(), 100);

    ASSERT_TRUE(reader.next());
    EXPECT_EQ(reader.getString(), "test");
}

TEST(TLV, EmptyBuffer) {
    TLVReader reader(nullptr, 0);
    EXPECT_FALSE(reader.next());
    EXPECT_TRUE(reader.atEnd());
}
