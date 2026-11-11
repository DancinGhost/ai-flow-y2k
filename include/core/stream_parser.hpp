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
    std::string partialTagBuffer; // For handling split tags across streaming deltas
    
    // In-flight tool calls being assembled across streaming deltas
    struct PendingToolCall {
        std::string id;
        std::string name;
        std::string argumentsBuffer;
        bool isComplete = false;
        int blockIndex = -1;
    };
    std::map<int, PendingToolCall> toolCallIndices;
    std::map<std::string, PendingToolCall> namedToolCalls;
    std::map<int, std::string> anthropicBlockIndexToId;
    std::string activeAnthropicToolId;
};

class StreamParser {
public:
    // Pure function: parse an incoming Server-Sent-Event line or JSON chunk
    static std::vector<FlowEvent> parseChunk(const std::string& rawChunk,
                                             ParserSessionState& state,
                                             int64_t currentTimestampMs);

    // Parses raw JSON completion (non-streaming full response) across OpenAI, Anthropic, and Gemini
    static std::vector<FlowEvent> parseFullResponse(const std::string& sessionId,
                                                    const std::string& jsonBody,
                                                    int64_t currentTimestampMs);

    // Flushes any in-flight pending thinking blocks or tool calls
    static std::vector<FlowEvent> flush(ParserSessionState& state, int64_t currentTimestampMs);

private:
    static std::vector<FlowEvent> parseOpenAiLine(const std::string& line,
                                                  ParserSessionState& state,
                                                  int64_t currentTimestampMs);

    static std::vector<FlowEvent> parseAnthropicLine(const std::string& line,
                                                     ParserSessionState& state,
                                                     int64_t currentTimestampMs);

    static std::vector<FlowEvent> parseGeminiLine(const std::string& line,
                                                  ParserSessionState& state,
                                                  int64_t currentTimestampMs);

    static std::vector<FlowEvent> processContentForThinkingTags(const std::string& text,
                                                                ParserSessionState& state,
                                                                int64_t currentTimestampMs);
};

} // namespace aiflow::core
