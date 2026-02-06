#pragma once

#include "core/Types.h"
#include "net/FaultInjector.h"
#include <vector>
#include <string>

namespace mt {

class FaultPlan {
public:
    struct Step {
        Duration delay_from_start;
        FaultRule rule;
        std::string description;
    };

    std::string name;
    std::string description;
    std::vector<Step> steps;

    static FaultPlan fromJson(const std::string& json);
    std::string toJson() const;

    // Built-in plans
    static FaultPlan meshHealingTest();
    static FaultPlan subscriptionStress();
    static FaultPlan commissioningFlaky();
    static FaultPlan progressiveDegradation();
};

} // namespace mt
