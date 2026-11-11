#pragma once

#include "core/models.hpp"
#include <string>
#include <vector>
#include <map>

namespace aiflow::core {

struct ParserSessionState {
    std::string sessionId;
    bool inThinkingBlock = false;
    std::string currentThinkingContent;
    std::string currentAssistantContent;
    
    // In-flight tool calls being assembled across streaming deltas
    struct PendingToolCall {
        std::string id;
        std::string name;
        std::string argumentsBuffer;
        bool isComplete = false;
    };
    std::map<int, PendingToolCall> toolCallIndices;
    std::map<std::string, PendingToolCall> namedToolCalls;
};

class StreamParser {
public:
    // Pure function: parse an incoming Server-Sent-Event line or JSON chunk
    static std::vector<FlowEvent> parseChunk(const std::string& rawChunk,
                                             ParserSessionState& state,
                                             int64_t currentTimestampMs);

    // Parses raw JSON completion (non-streaming full response)
    static std::vector<FlowEvent> parseFullResponse(const std::string& sessionId,
                                                    const std::string& jsonBody,
                                                    int64_t currentTimestampMs);

private:
    static std::vector<FlowEvent> parseOpenAiLine(const std::string& line,
                                                  ParserSessionState& state,
                                                  int64_t currentTimestampMs);

    static std::vector<FlowEvent> parseAnthropicLine(const std::string& line,
                                                     ParserSessionState& state,
                                                     int64_t currentTimestampMs);

    static std::vector<FlowEvent> processContentForThinkingTags(const std::string& text,
                                                                ParserSessionState& state,
                                                                int64_t currentTimestampMs);
};

} // namespace aiflow::core
