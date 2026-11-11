#include "core/policy_engine.hpp"
#include <algorithm>
#include <cctype>
#include <regex>
#include <array>
#include <string_view>
#include <ranges>

namespace aiflow::core {

namespace {

std::string toLower(std::string_view str) {
    std::string result;
    result.reserve(str.size());
    for (char c : str) {
        result.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
    }
    return result;
}

bool wildcardMatchInternal(std::string_view text, std::string_view pattern) {
    size_t t = 0, p = 0;
    size_t textLen = text.size(), patLen = pattern.size();
    size_t starIdx = std::string_view::npos;
    size_t matchIdx = 0;

    while (t < textLen) {
        if (p < patLen && (std::tolower(static_cast<unsigned char>(pattern[p])) == 
                           std::tolower(static_cast<unsigned char>(text[t])) || pattern[p] == '?')) {
            ++t;
            ++p;
        } else if (p < patLen && pattern[p] == '*') {
            starIdx = p;
            matchIdx = t;
            ++p;
        } else if (starIdx != std::string_view::npos) {
            p = starIdx + 1;
            ++matchIdx;
            t = matchIdx;
        } else {
            return false;
        }
    }

    while (p < patLen && pattern[p] == '*') {
        ++p;
    }

    return p == patLen;
}

struct HazardDefinition {
    std::string_view pattern;
    std::string_view category;
    HazardSeverity severity;
    std::string_view description;
    bool isRegex = false;
};

// C++20 constexpr catalog of hazardous system, security, and infrastructure signatures
constexpr std::array<HazardDefinition, 46> HAZARD_CATALOG = {{
    // Filesystem destruction
    {"rm -rf", "filesystem_destruction", HazardSeverity::Critical, "Recursive forced file deletion", false},
    {"rm -r", "filesystem_destruction", HazardSeverity::High, "Recursive file deletion", false},
    {"rmdir /s /q", "filesystem_destruction", HazardSeverity::Critical, "Windows silent recursive directory wipe", false},
    {"del /f /s /q", "filesystem_destruction", HazardSeverity::Critical, "Windows silent recursive file wipe", false},
    {"shred ", "filesystem_destruction", HazardSeverity::High, "Secure file shredding / data obliteration", false},
    {"unlink ", "filesystem_destruction", HazardSeverity::Medium, "File unlinking operation", false},

    // Raw disk / Storage overwrite
    {"mkfs", "disk_wipe", HazardSeverity::Critical, "Filesystem formatting / partition destruction", false},
    {"format c:", "disk_wipe", HazardSeverity::Critical, "System drive format command", false},
    {"dd if=", "disk_wipe", HazardSeverity::Critical, "Direct device block overwrite", false},
    {"> /dev/sd", "disk_wipe", HazardSeverity::Critical, "Raw drive write operation", false},
    {"> /dev/nvme", "disk_wipe", HazardSeverity::Critical, "Raw NVMe drive write operation", false},
    {"wipefs", "disk_wipe", HazardSeverity::Critical, "Partition table signature wipe", false},

    // Denial of Service / Fork bombs
    {":(){ :|:& };:", "fork_bomb", HazardSeverity::Critical, "Bash fork bomb resource exhaustion", false},
    {"while :; do :; done", "fork_bomb", HazardSeverity::High, "Infinite CPU saturation loop", false},
    {"cat /dev/urandom >", "disk_wipe", HazardSeverity::Critical, "Storage exhaustion via random byte flooding", false},

    // Database mutations
    {"drop database", "database_mutation", HazardSeverity::Critical, "Database schema destruction", false},
    {"drop table", "database_mutation", HazardSeverity::Critical, "Database table drop", false},
    {"drop column", "database_mutation", HazardSeverity::High, "Database column drop", false},
    {"truncate table", "database_mutation", HazardSeverity::High, "Database table truncation", false},
    {"delete from", "database_mutation", HazardSeverity::Medium, "SQL bulk delete statement", false},
    {"db.dropdatabase", "database_mutation", HazardSeverity::Critical, "NoSQL database drop", false},

    // Privilege escalation & sensitive system access
    {"chmod 777", "privilege_escalation", HazardSeverity::High, "Insecure world-writable permission assignment", false},
    {"chmod -r 777", "privilege_escalation", HazardSeverity::Critical, "Recursive world-writable permission assignment", false},
    {"chown root", "privilege_escalation", HazardSeverity::High, "Root ownership transfer", false},
    {"chown -r root", "privilege_escalation", HazardSeverity::Critical, "Recursive root ownership transfer", false},
    {"sudo su", "privilege_escalation", HazardSeverity::High, "Unrestricted superuser shell switch", false},
    {"/etc/shadow", "credential_access", HazardSeverity::Critical, "Accessing Unix password shadow hashes", false},
    {"/etc/sudoers", "privilege_escalation", HazardSeverity::Critical, "Modifying system sudoers privileges", false},
    {"id_rsa", "credential_access", HazardSeverity::Critical, "Accessing private SSH identity keys", false},
    {".aws/credentials", "credential_access", HazardSeverity::Critical, "Accessing AWS cloud credentials", false},
    {".env", "credential_access", HazardSeverity::High, "Accessing environment secret configuration", false},

    // Remote script injection & Reverse shells
    {"curl | bash", "remote_code_execution", HazardSeverity::Critical, "Direct download and uninspected bash execution", false},
    {"curl | sh", "remote_code_execution", HazardSeverity::Critical, "Direct download and uninspected sh execution", false},
    {"wget | bash", "remote_code_execution", HazardSeverity::Critical, "Direct download and uninspected bash execution", false},
    {"wget | sh", "remote_code_execution", HazardSeverity::Critical, "Direct download and uninspected sh execution", false},
    {"bash -i >&", "reverse_shell", HazardSeverity::Critical, "Interactive TCP reverse shell socket", false},
    {"/dev/tcp/", "reverse_shell", HazardSeverity::Critical, "Direct bash network socket creation", false},
    {"nc -e", "reverse_shell", HazardSeverity::Critical, "Netcat executable reverse shell backdoor", false},
    {"powershell -enc", "obfuscated_execution", HazardSeverity::High, "Base64-encoded PowerShell script execution", false},
    {"iex (new-object", "remote_code_execution", HazardSeverity::Critical, "PowerShell remote script download and execute", false},

    // System lifecycle & service termination
    {"shutdown", "system_control", HazardSeverity::High, "Machine shutdown invocation", false},
    {"reboot", "system_control", HazardSeverity::High, "Machine reboot invocation", false},
    {"init 0", "system_control", HazardSeverity::High, "System halt invocation", false},

    // Cloud & orchestration destruction
    {"kubectl delete namespace", "cloud_infrastructure", HazardSeverity::Critical, "Kubernetes namespace wipe", false},
    {"kubectl delete all", "cloud_infrastructure", HazardSeverity::Critical, "Kubernetes resource wipe", false},
    {"terraform destroy", "cloud_infrastructure", HazardSeverity::Critical, "Infrastructure teardown via Terraform", false}
}};

} // anonymous namespace

std::string PolicyEngine::severityToString(HazardSeverity severity) {
    switch (severity) {
        case HazardSeverity::Critical: return "Critical";
        case HazardSeverity::High:     return "High";
        case HazardSeverity::Medium:   return "Medium";
        case HazardSeverity::Low:      return "Low";
        case HazardSeverity::None:     return "None";
    }
    return "None";
}

HazardSeverity PolicyEngine::severityFromString(const std::string& str) {
    std::string s = toLower(str);
    if (s == "critical") return HazardSeverity::Critical;
    if (s == "high")     return HazardSeverity::High;
    if (s == "medium")   return HazardSeverity::Medium;
    if (s == "low")      return HazardSeverity::Low;
    return HazardSeverity::None;
}

bool PolicyEngine::matchesPattern(std::string_view text, std::string_view pattern) {
    if (pattern.empty()) return true;

    // 1. Regex matching
    if (pattern.starts_with("regex:")) {
        try {
            std::string regexStr(pattern.substr(6));
            std::regex re(regexStr, std::regex::ECMAScript | std::regex::icase);
            std::string textStr(text);
            return std::regex_search(textStr, re);
        } catch (const std::regex_error&) {
            return false;
        }
    }

    // 2. Wildcard glob matching (* or ?)
    if (pattern.find('*') != std::string_view::npos || pattern.find('?') != std::string_view::npos) {
        if (wildcardMatchInternal(text, pattern)) {
            return true;
        }
        std::string wrapped = "*" + std::string(pattern) + "*";
        return wildcardMatchInternal(text, wrapped);
    }

    // 3. Fallback: case-insensitive substring search
    std::string lowerText = toLower(text);
    std::string lowerPattern = toLower(pattern);
    return lowerText.find(lowerPattern) != std::string::npos;
}

HazardMatch PolicyEngine::analyzeHazard(const std::string& arguments) {
    HazardMatch match;
    std::string lower = toLower(arguments);

    for (const auto& def : HAZARD_CATALOG) {
        if (lower.find(def.pattern) != std::string::npos) {
            match.matched = true;
            match.rulePattern = std::string(def.pattern);
            match.category = std::string(def.category);
            match.severity = def.severity;
            match.description = std::string(def.description);
            return match;
        }
    }

    struct RegexHazardEntry {
        std::string pattern;
        std::string_view category;
        std::string_view description;
        HazardSeverity severity;
    };

    static const std::array<RegexHazardEntry, 8> regexHazards = {{
        {R"(\brm\s+-(?:r[fF]|[fF]r|r)\b)", "filesystem_destruction", "Recursive forced deletion", HazardSeverity::Critical},
        {R"(\bdrop\s+(?:table|database)\b)", "database_mutation", "Database destruction", HazardSeverity::Critical},
        {R"(\btruncate\s+table\b)", "database_mutation", "Table truncation", HazardSeverity::High},
        {R"(\bchmod\s+(?:-R\s+)?777\b)", "privilege_escalation", "Permissive permission grant", HazardSeverity::Critical},
        {R"(\bbash\s+-i\s+>&)", "reverse_shell", "Interactive reverse shell", HazardSeverity::Critical},
        {R"(\b(?:curl|wget)\b.*\|\s*(?:bash|sh|zsh)\b)", "remote_code_execution", "Piped remote script execution", HazardSeverity::Critical},
        {R"(\bpowershell\b.*-(?:enc|encodedcommand)\b)", "obfuscated_execution", "Encoded PowerShell execution", HazardSeverity::High},
        {R"(\bterraform\s+destroy\b)", "cloud_infrastructure", "Terraform destruction", HazardSeverity::Critical}
    }};

    for (const auto& entry : regexHazards) {
        try {
            std::regex re(entry.pattern, std::regex::ECMAScript | std::regex::icase);
            if (std::regex_search(lower, re)) {
                match.matched = true;
                match.rulePattern = entry.pattern;
                match.category = std::string(entry.category);
                match.severity = entry.severity;
                match.description = std::string(entry.description);
                return match;
            }
        } catch (const std::regex_error&) {}
    }

    return match;
}

bool PolicyEngine::containsHazardousPayload(const std::string& arguments) {
    return analyzeHazard(arguments).matched;
}

HazardMatch PolicyEngine::analyzeTool(const std::string& toolName,
                                      const std::string& rawArguments,
                                      const std::vector<std::string>& dangerousPatterns) {
    // 1. Check argument payloads for hazardous exploits
    HazardMatch hazard = analyzeHazard(rawArguments);
    if (hazard.matched) {
        return hazard;
    }

    // 2. Check tool name against dangerous patterns using wildcards and regex
    for (const auto& pattern : dangerousPatterns) {
        if (matchesPattern(toolName, pattern)) {
            HazardMatch match;
            match.matched = true;
            match.rulePattern = pattern;
            match.category = "dangerous_tool";
            match.severity = HazardSeverity::High;
            match.description = "Tool name matches dangerous tool pattern: " + pattern;
            return match;
        }
    }

    return HazardMatch{};
}

bool PolicyEngine::isDangerousTool(const std::string& toolName,
                                   const std::string& rawArguments,
                                   const std::vector<std::string>& dangerousPatterns) {
    return analyzeTool(toolName, rawArguments, dangerousPatterns).matched;
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

InterceptionStatus PolicyEngine::evaluateCallWithRules(const ToolCall& call,
                                                      InterceptionMode mode,
                                                      const std::vector<std::string>& dangerousPatterns,
                                                      const std::vector<PolicyRule>& customRules) {
    // 1. Evaluate custom user rules first (rules take precedence when enabled)
    for (const auto& rule : customRules) {
        if (!rule.enabled) continue;

        bool toolMatches = rule.toolPattern.empty() || matchesPattern(call.toolName, rule.toolPattern);
        bool argMatches = rule.argumentPattern.empty() || matchesPattern(call.rawArguments, rule.argumentPattern);

        if (toolMatches && argMatches) {
            return rule.action;
        }
    }

    // 2. Fallback to default policy engine evaluation
    return evaluateCall(call, mode, dangerousPatterns);
}

} // namespace aiflow::core
