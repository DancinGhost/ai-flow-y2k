#pragma once

#include "core/models.hpp"
#include <string>
#include <vector>
#include <string_view>

namespace aiflow::core {

class PolicyEngine {
public:
    // Pure function: converts HazardSeverity to string
    static std::string severityToString(HazardSeverity severity);

    // Pure function: parse string to HazardSeverity
    static HazardSeverity severityFromString(const std::string& str);

    // Pattern matching supporting wildcards (*, ?) and regex (prefixed with "regex:")
    static bool matchesPattern(std::string_view text, std::string_view pattern);

    // Checks arguments for hazardous shell commands, dangerous scripts, or destructive ops
    static bool containsHazardousPayload(const std::string& arguments);

    // Deep analysis of argument payload against hazard catalog
    static HazardMatch analyzeHazard(const std::string& arguments);

    // Pure function: checks if a tool call matches dangerous patterns or contains hazardous payload
    static bool isDangerousTool(const std::string& toolName,
                                const std::string& rawArguments,
                                const std::vector<std::string>& dangerousPatterns);

    // Deep analysis of tool call (name and arguments) against dangerous patterns
    static HazardMatch analyzeTool(const std::string& toolName,
                                   const std::string& rawArguments,
                                   const std::vector<std::string>& dangerousPatterns);

    // Pure function: determines the interception action required based on mode and patterns
    static InterceptionStatus evaluateCall(const ToolCall& call,
                                           InterceptionMode mode,
                                           const std::vector<std::string>& dangerousPatterns);

    // Pure function: evaluates call with both global mode and granular custom PolicyRules
    static InterceptionStatus evaluateCallWithRules(const ToolCall& call,
                                                    InterceptionMode mode,
                                                    const std::vector<std::string>& dangerousPatterns,
                                                    const std::vector<PolicyRule>& customRules);
};

} // namespace aiflow::core
