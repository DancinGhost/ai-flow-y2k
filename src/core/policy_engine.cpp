#include "core/policy_engine.hpp"
#include <algorithm>
#include <cctype>

namespace aiflow::core {

static std::string toLower(std::string_view str) {
    std::string result;
    result.reserve(str.size());
    for (char c : str) {
        result.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
    }
    return result;
}

bool PolicyEngine::containsHazardousPayload(const std::string& arguments) {
    std::string lower = toLower(arguments);
    const std::vector<std::string> hazards = {
        "rm -rf", "rm -r", "mkfs", ":(){ :|:& };:", "drop database",
        "drop table", "chmod 777", "> /dev/sda", "dd if=", "curl | bash",
        "curl | sh", "wget | bash", "wget | sh", "shutdown", "reboot"
    };

    for (const auto& hazard : hazards) {
        if (lower.find(hazard) != std::string::npos) {
            return true;
        }
    }
    return false;
}

bool PolicyEngine::isDangerousTool(const std::string& toolName,
                                   const std::string& rawArguments,
                                   const std::vector<std::string>& dangerousPatterns) {
    std::string lowerName = toLower(toolName);

    for (const auto& pattern : dangerousPatterns) {
        if (lowerName.find(toLower(pattern)) != std::string::npos) {
            return true;
        }
    }

    if (containsHazardousPayload(rawArguments)) {
        return true;
    }

    return false;
}

InterceptionStatus PolicyEngine::evaluateCall(const ToolCall& call,
                                              InterceptionMode mode,
                                              const std::vector<std::string>& dangerousPatterns) {
    switch (mode) {
        case InterceptionMode::MonitorOnly:
            return InterceptionStatus::AutoPassed;

        case InterceptionMode::InterceptAllTools:
            return InterceptionStatus::PendingApproval;

        case InterceptionMode::InterceptDangerousOnly:
            if (isDangerousTool(call.toolName, call.rawArguments, dangerousPatterns)) {
                return InterceptionStatus::PendingApproval;
            }
            return InterceptionStatus::AutoPassed;
    }
    return InterceptionStatus::AutoPassed;
}

} // namespace aiflow::core
