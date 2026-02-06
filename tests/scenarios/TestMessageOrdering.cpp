#include <gtest/gtest.h>
#include "ScenarioRunner.h"
#include "metrics/Collector.h"
#include "metrics/Histogram.h"
#include "core/Random.h"
#include <set>
#include <algorithm>

using namespace mt;

TEST(Scenarios, MessageOrderingWithJitter) {
    ScenarioRunner runner;

    ScenarioConfig config;
    config.topology = MeshTopology::fullyConnected();
    config.random_seed = 42;

    auto result = runner.run("message-ordering", config, [](ScenarioContext& ctx) {
        // Simulate 100 messages with varying latencies
        // Track which arrive in order

        Random rng(42);
        int total_messages = 100;
        int ordering_violations = 0;
        int duplicates = 0;

        struct Message {
            int seq;
            int64_t arrival_time_ms; // relative to send time
        };

        // Simulate: each message gets a random delay (jitter)
        std::vector<Message> received;
        for (int i = 0; i < total_messages; ++i) {
            // Base latency 10ms + jitter up to 200ms
            auto delay = static_cast<int64_t>(rng.nextGaussian(10.0, 50.0));
            if (delay < 0) delay = 0;
            int64_t arrival = static_cast<int64_t>(i) * 100 + delay; // Sent 10/sec = every 100ms
            received.push_back({i, arrival});

            ctx.metrics.record("rtt_ms", static_cast<double>(delay));
        }

        // Sort by arrival time (simulating actual network behavior)
        std::sort(received.begin(), received.end(),
                  [](const Message& a, const Message& b) {
                      return a.arrival_time_ms < b.arrival_time_ms;
                  });

        // Check ordering
        int last_seq = -1;
        std::set<int> seen;
        for (const auto& msg : received) {
            if (msg.seq <= last_seq) {
                ++ordering_violations;
            }
            last_seq = std::max(last_seq, msg.seq);

            if (seen.count(msg.seq) > 0) {
                ++duplicates;
            }
            seen.insert(msg.seq);
        }

        ctx.metrics.increment("message_ordering_violations", static_cast<uint64_t>(ordering_violations));
        ctx.metrics.increment("duplicate_message_count", static_cast<uint64_t>(duplicates));

        auto rtt_stats = ctx.metrics.getHistogram("rtt_ms");
        ctx.metrics.event(SteadyClock::now(), 0, "test", "message_ordering_complete",
                           "violations=" + std::to_string(ordering_violations) +
                           " p50=" + std::to_string(rtt_stats.p50) +
                           " p99=" + std::to_string(rtt_stats.p99));

        // No duplicates expected (no fault injection for duplication)
        EXPECT_EQ(duplicates, 0);

        // Some ordering violations are expected with jitter
        // The test passes as long as we correctly measure them
        return true;
    });

    EXPECT_TRUE(result.passed);
}
