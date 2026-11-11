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

struct ToolResultReceived {
    std::string sessionId;
    std::string callId;
    std::string output;
    int64_t timestampMs;
};

struct SessionCompleted {
    std::string sessionId;
    int64_t promptTokens;
    int64_t completionTokens;
    int64_t timestampMs;
};

} // namespace events

using FlowEvent = std::variant<
    events::SessionStarted,
    events::ThinkingDelta,
    events::ThinkingFinished,
    events::ContentDelta,
    events::ToolCallDetected,
    events::ToolCallDecisionMade,
    events::ToolResultReceived,
    events::SessionCompleted
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

} // namespace actions

using DomainAction = std::variant<
    actions::ForwardPayloadToClient,
    actions::PauseResponseStream,
    actions::ResumeResponseStream,
    actions::EmitToolRejection,
    actions::NotifyUIRefresh
>;

struct AppState {
    ThemeType currentTheme = ThemeType::Vaporwave;
    InterceptionMode interceptionMode = InterceptionMode::InterceptDangerousOnly;
    std::vector<AgentSession> sessions;
    std::string activeSessionId;
    bool proxyRunning = false;
    uint16_t proxyPort = 8765;
    std::vector<std::string> dangerousToolPatterns = {
        "bash", "shell", "sh", "terminal", "exec", "system",
        "rm", "delete", "write_file", "edit_file", "drop_table",
        "curl", "wget", "chmod", "kill", "git_push"
    };
};

} // namespace aiflow::core
