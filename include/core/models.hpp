#pragma once

#include <string>
#include <vector>
#include <variant>
#include <optional>
#include <cstdint>

namespace aiflow::core {

enum class ThemeType {
    Vaporwave,
    DigitalRain,
    FrutigerAero
};

enum class InterceptionMode {
    MonitorOnly,            // Pass through all tools without blocking, log and observe
    InterceptDangerousOnly, // Pause and require approval for risky tools (bash, shell, rm, write_file)
    InterceptAllTools       // Pause and require approval for every tool invocation
};

enum class InterceptionStatus {
    PendingApproval,
    Approved,
    Modified,
    Rejected,
    AutoPassed,
    Executing,
    Completed
};

enum class HazardSeverity {
    None,
    Low,
    Medium,
    High,
    Critical
};

struct HazardMatch {
    bool matched = false;
    std::string rulePattern;
    std::string category; // e.g. "filesystem_destruction", "privilege_escalation", "data_exfiltration", "reverse_shell", "database_mutation", "system_control"
    HazardSeverity severity = HazardSeverity::None;
    std::string description;
};

struct PolicyRule {
    std::string id;
    std::string name;
    std::string toolPattern;     // Supports wildcard glob (*, ?) or regex: prefix
    std::string argumentPattern; // Supports wildcard glob (*, ?) or regex: prefix
    InterceptionStatus action = InterceptionStatus::PendingApproval;
    HazardSeverity severity = HazardSeverity::High;
    std::string reason;
    bool enabled = true;
};

struct ToolCall {
    std::string id;
    std::string toolName;
    std::string rawArguments;
    std::string modifiedArguments;
    InterceptionStatus status = InterceptionStatus::PendingApproval;
    std::string rejectionReason;
    std::string executionResult;
    int64_t timestampMs = 0;
    bool isDangerous = false;
    std::string hazardCategory; // Categorized hazard type
    std::string hazardSeverity; // "Critical", "High", "Medium", "Low", "None"
};

struct ThinkingBlock {
    std::string id;
    std::string content;
    bool isStreaming = false;
    int64_t startTimestampMs = 0;
    int64_t endTimestampMs = 0;
};

struct AgentMessage {
    std::string role;      // "system", "user", "assistant", "tool"
    std::string content;
    std::string thinking;
    std::vector<ToolCall> toolCalls;
    int64_t timestampMs = 0;
};

struct AgentSession {
    std::string sessionId;
    std::string modelName;
    std::string agentName;
    std::vector<AgentMessage> messages;
    std::vector<ThinkingBlock> thinkingHistory;
    std::vector<ToolCall> interceptedCalls;
    int64_t promptTokens = 0;
    int64_t completionTokens = 0;
    bool isLive = false;
    int approvedCallsCount = 0;
    int rejectedCallsCount = 0;
    int modifiedCallsCount = 0;
};

// Pure Domain Events
namespace events {

struct SessionStarted {
    std::string sessionId;
    std::string modelName;
    std::string agentName;
    int64_t timestampMs;
};

struct ThinkingDelta {
    std::string sessionId;
    std::string deltaText;
    int64_t timestampMs;
};

struct ThinkingFinished {
    std::string sessionId;
    int64_t timestampMs;
};

struct ContentDelta {
    std::string sessionId;
    std::string deltaText;
    int64_t timestampMs;
};

struct ToolCallDetected {
    std::string sessionId;
    ToolCall call;
};

struct ToolCallDecisionMade {
    std::string sessionId;
    std::string callId;
    InterceptionStatus status;
    std::string modifiedArgs;
    std::string reason;
};

struct ToolCallExecuting {
    std::string sessionId;
    std::string callId;
    int64_t timestampMs = 0;
};

struct ToolResultReceived {
    std::string sessionId;
    std::string callId;
    std::string output;
    int64_t timestampMs;
};

struct TokenUsageUpdated {
    std::string sessionId;
    int64_t promptTokens = 0;
    int64_t completionTokens = 0;
};

struct SessionCompleted {
    std::string sessionId;
    int64_t promptTokens;
    int64_t completionTokens;
    int64_t timestampMs;
};

struct InterceptionModeChanged {
    InterceptionMode newMode;
};

struct ThemeChanged {
    ThemeType newTheme;
};

struct PolicyRuleAdded {
    PolicyRule rule;
};

struct SessionReset {
    std::string sessionId;
};

} // namespace events

using FlowEvent = std::variant<
    events::SessionStarted,
    events::ThinkingDelta,
    events::ThinkingFinished,
    events::ContentDelta,
    events::ToolCallDetected,
    events::ToolCallDecisionMade,
    events::ToolCallExecuting,
    events::ToolResultReceived,
    events::TokenUsageUpdated,
    events::SessionCompleted,
    events::InterceptionModeChanged,
    events::ThemeChanged,
    events::PolicyRuleAdded,
    events::SessionReset
>;

// Domain Actions output by the functional reducer
namespace actions {

struct ForwardPayloadToClient {
    std::string sessionId;
    std::string chunk;
};

struct PauseResponseStream {
    std::string sessionId;
    std::string callId;
};

struct ResumeResponseStream {
    std::string sessionId;
    std::string callId;
    std::string transformedPayload;
};

struct EmitToolRejection {
    std::string sessionId;
    std::string callId;
    std::string errorMessage;
};

struct NotifyUIRefresh {};

struct LogAuditEvent {
    std::string sessionId;
    std::string message;
    std::string level; // "INFO", "WARN", "SECURITY", "ERROR"
    int64_t timestampMs = 0;
};

} // namespace actions

using DomainAction = std::variant<
    actions::ForwardPayloadToClient,
    actions::PauseResponseStream,
    actions::ResumeResponseStream,
    actions::EmitToolRejection,
    actions::NotifyUIRefresh,
    actions::LogAuditEvent
>;

struct AppState {
    ThemeType currentTheme = ThemeType::Vaporwave;
    InterceptionMode interceptionMode = InterceptionMode::MonitorOnly;
    std::vector<AgentSession> sessions;
    std::string activeSessionId;
    bool proxyRunning = false;
    uint16_t proxyPort = 8765;
    std::vector<std::string> dangerousToolPatterns = {
        "bash", "shell", "sh", "terminal", "exec", "system",
        "rm", "delete", "write_file", "edit_file", "drop_table",
        "curl", "wget", "chmod", "kill", "git_push"
    };
    std::vector<PolicyRule> customPolicyRules;
    int64_t totalTokensProcessed = 0;
};

} // namespace aiflow::core
