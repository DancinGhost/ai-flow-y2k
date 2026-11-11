#pragma once

#include "core/models.hpp"
#include <vector>

namespace aiflow::core {

class FlowReducer {
public:
    // Pure function: takes current AppState and FlowEvent, returns updated AppState and actions to execute
    static AppState reduce(const AppState& state,
                           const FlowEvent& event,
                           std::vector<DomainAction>& outActions);
};

} // namespace aiflow::core
