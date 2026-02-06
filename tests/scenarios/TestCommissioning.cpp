#include <gtest/gtest.h>
#include "ScenarioRunner.h"
#include "matter/PASE.h"

using namespace mt;

TEST(Scenarios, CommissioningHappyPath) {
    ScenarioRunner runner;

    ScenarioConfig config;
    config.topology = MeshTopology::fullyConnected();
    config.random_seed = 42;

    auto result = runner.run("commissioning-happy", config, [](ScenarioContext& ctx) {
        auto now = SteadyClock::now();

        // Initiator side
        PASESession initiator;
        initiator.setStartTime(now);
        auto start_result = initiator.startPairing(12345678, 2);
        EXPECT_TRUE(start_result.ok());
        EXPECT_EQ(initiator.state(), PASESession::State::PBKDFParamRequest);

        // Responder receives PBKDFParamRequest
        PASESession responder;
        auto r1 = responder.handlePBKDFParamRequest();
        EXPECT_TRUE(r1.ok());
        EXPECT_EQ(responder.state(), PASESession::State::PBKDFParamResponse);

        // Initiator receives PBKDFParamResponse
        auto r2 = initiator.handlePBKDFParamResponse();
        EXPECT_TRUE(r2.ok());
        EXPECT_EQ(initiator.state(), PASESession::State::PASE1);

        // Responder receives PASE1
        auto r3 = responder.handlePASE1();
        EXPECT_TRUE(r3.ok());

        // Initiator receives PASE2
        auto r4 = initiator.handlePASE2();
        EXPECT_TRUE(r4.ok());

        // Responder receives PASE3
        auto r5 = responder.handlePASE3();
        EXPECT_TRUE(r5.ok());
        EXPECT_TRUE(responder.isComplete());

        auto session = responder.getEstablishedSession();
        EXPECT_TRUE(session.has_value());
        EXPECT_EQ(session->type, SessionType::PASE);

        ctx.metrics.increment("commissioning_success");
        auto elapsed_ms = static_cast<double>(
            std::chrono::duration_cast<Duration>(SteadyClock::now() - now).count());
        ctx.metrics.record("commissioning_duration_ms", elapsed_ms);

        return true;
    });

    EXPECT_TRUE(result.passed);
}

TEST(Scenarios, CommissioningStateViolation) {
    PASESession session;

    // Try PASE2 before PASE1 — should fail
    auto result = session.handlePASE2();
    EXPECT_FALSE(result.ok());
}
