#include <gtest/gtest.h>
#include "matter/InteractionModel.h"

using namespace mt;

class InteractionModelTest : public ::testing::Test {
protected:
    DataModel dm = DataModel::lightBulb();
    SessionManager sm;
    ExchangeManager em;
    SubscriptionManager sub;
    InteractionModel im{dm, sm, sub, em};
    SessionId session_id;

    void SetUp() override {
        session_id = sm.createSession(SessionType::CASE, 1, 1);
    }
};

TEST_F(InteractionModelTest, ReadAttribute) {
    std::vector<AttributePath> paths = {{1, Clusters::OnOff, Attributes::OnOff_OnOff}};

    auto result = im.sendRead(session_id, paths);
    ASSERT_TRUE(result.ok());

    auto& report = *result;
    ASSERT_EQ(report.attribute_reports.size(), 1);
    EXPECT_FALSE(report.attribute_reports[0].has_error);
    EXPECT_EQ(std::get<bool>(report.attribute_reports[0].value), false); // Light starts off
}

TEST_F(InteractionModelTest, WriteAndReadAttribute) {
    AttributePath path{1, Clusters::OnOff, Attributes::OnOff_OnOff};

    WriteRequestData write_req;
    write_req.writes.push_back({path, true});

    auto write_result = im.sendWrite(session_id, write_req);
    ASSERT_TRUE(write_result.ok());

    auto read_result = im.sendRead(session_id, {path});
    ASSERT_TRUE(read_result.ok());
    EXPECT_EQ(std::get<bool>(read_result->attribute_reports[0].value), true);
}

TEST_F(InteractionModelTest, ReadNonexistentAttribute) {
    std::vector<AttributePath> paths = {{99, 0xFFFF, 0xFFFF}};

    auto result = im.sendRead(session_id, paths);
    ASSERT_TRUE(result.ok());
    EXPECT_TRUE(result->attribute_reports[0].has_error);
}

TEST_F(InteractionModelTest, InvalidSession) {
    std::vector<AttributePath> paths = {{1, Clusters::OnOff, Attributes::OnOff_OnOff}};

    auto result = im.sendRead(999, paths);
    EXPECT_FALSE(result.ok());
}

TEST_F(InteractionModelTest, Subscribe) {
    std::vector<AttributePath> paths = {{1, Clusters::OnOff, Attributes::OnOff_OnOff}};

    auto result = im.sendSubscribe(session_id, paths, Duration(5000), Duration(60000));
    ASSERT_TRUE(result.ok());
    EXPECT_GT(*result, 0u);
    EXPECT_EQ(sub.activeCount(), 1);
}

TEST_F(InteractionModelTest, InvokeCommand) {
    InvokeRequestData req;
    req.command = {1, Clusters::OnOff, 0x01}; // Toggle
    req.timed = false;

    auto result = im.sendInvoke(session_id, req);
    ASSERT_TRUE(result.ok());
    EXPECT_EQ(result->status_code, 0);
}
