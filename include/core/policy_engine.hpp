#pragma once

#include "core/models.hpp"
#include <string>
#include <vector>

namespace aiflow::core {

class PolicyEngine {
public:
    // Pure function: checks if a tool call matches dangerous patterns
    static bool isDangerousTool(const std::string& toolName,
                                const std::string& rawArguments,
                                const std::vector<std::string>& dangerousPatterns);

    // Pure function: determines the interception action required
    static InterceptionStatus evaluateCall(const ToolCall& call,
                                           InterceptionMode mode,
                                           const std::vector<std::string>& dangerousPatterns);

    // Checks arguments for hazardous shell commands (e.g. rm -rf, drop table, mkfs, curl | sh)
    static bool containsHazardousPayload(const std::string& arguments);
};

} // namespace aiflow::core
