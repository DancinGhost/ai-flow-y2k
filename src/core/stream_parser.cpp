#include "core/stream_parser.hpp"
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonValue>
#include <sstream>
#include <array>
#include <string_view>

namespace aiflow::core {

namespace {

const std::array<std::string_view, 4> OPEN_THINKING_TAGS = {
    "<thinking>", "<thought>", "<reasoning>", "<antThinking>"
};

const std::array<std::string_view, 4> CLOSE_THINKING_TAGS = {
    "</thinking>", "</thought>", "</reasoning>", "</antThinking>"
};

} // anonymous namespace

std::vector<FlowEvent> StreamParser::processContentForThinkingTags(const std::string& text,
                                                                   ParserSessionState& state,
                                                                   int64_t currentTimestampMs) {
    std::vector<FlowEvent> events;
    std::string remaining = state.partialTagBuffer + text;
    state.partialTagBuffer.clear();

    // Check if remaining ends with an unclosed opening or closing tag prefix (e.g. "<think" or "</reas")
    size_t lastLt = remaining.rfind('<');
    if (lastLt != std::string::npos && remaining.find('>', lastLt) == std::string::npos) {
        std::string_view candidate = std::string_view(remaining).substr(lastLt);
        bool isCandidateTagPrefix = false;

        for (auto tag : OPEN_THINKING_TAGS) {
            if (tag.starts_with(candidate)) {
                isCandidateTagPrefix = true;
                break;
            }
        }
        if (!isCandidateTagPrefix) {
            for (auto tag : CLOSE_THINKING_TAGS) {
                if (tag.starts_with(candidate)) {
                    isCandidateTagPrefix = true;
                    break;
                }
            }
        }

        if (isCandidateTagPrefix) {
            state.partialTagBuffer = remaining.substr(lastLt);
            remaining = remaining.substr(0, lastLt);
        }
    }

    while (!remaining.empty()) {
        if (!state.inThinkingBlock) {
            size_t minTagPos = std::string::npos;
            size_t matchedTagLen = 0;

            for (auto tag : OPEN_THINKING_TAGS) {
                size_t pos = remaining.find(tag);
                if (pos != std::string::npos && (minTagPos == std::string::npos || pos < minTagPos)) {
                    minTagPos = pos;
                    matchedTagLen = tag.size();
                }
            }

            if (minTagPos != std::string::npos) {
                // Content preceding the tag is regular assistant content
                if (minTagPos > 0) {
                    std::string pre = remaining.substr(0, minTagPos);
                    state.currentAssistantContent += pre;
                    events.push_back(events::ContentDelta{state.sessionId, pre, currentTimestampMs});
                }
                state.inThinkingBlock = true;
                remaining = remaining.substr(minTagPos + matchedTagLen);
            } else {
                state.currentAssistantContent += remaining;
                events.push_back(events::ContentDelta{state.sessionId, remaining, currentTimestampMs});
                remaining.clear();
            }
        } else {
            size_t minEndPos = std::string::npos;
            size_t matchedEndLen = 0;

            for (auto tag : CLOSE_THINKING_TAGS) {
                size_t pos = remaining.find(tag);
                if (pos != std::string::npos && (minEndPos == std::string::npos || pos < minEndPos)) {
                    minEndPos = pos;
                    matchedEndLen = tag.size();
                }
            }

            if (minEndPos != std::string::npos) {
                std::string thinkDelta = remaining.substr(0, minEndPos);
                state.currentThinkingContent += thinkDelta;
                if (!thinkDelta.empty()) {
                    events.push_back(events::ThinkingDelta{state.sessionId, thinkDelta, currentTimestampMs});
                }
                events.push_back(events::ThinkingFinished{state.sessionId, currentTimestampMs});
                state.inThinkingBlock = false;
                remaining = remaining.substr(minEndPos + matchedEndLen);
            } else {
                state.currentThinkingContent += remaining;
                events.push_back(events::ThinkingDelta{state.sessionId, remaining, currentTimestampMs});
                remaining.clear();
            }
        }
    }

    return events;
}

std::vector<FlowEvent> StreamParser::parseOpenAiLine(const std::string& line,
                                                     ParserSessionState& state,
                                                     int64_t currentTimestampMs) {
    std::vector<FlowEvent> events;

    std::string payload = line;
    if (payload.rfind("data: ", 0) == 0) {
        payload = payload.substr(6);
    }

    if (payload == "[DONE]" || payload.empty()) {
        auto flushed = flush(state, currentTimestampMs);
        events.insert(events.end(), flushed.begin(), flushed.end());
        return events;
    }

    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(QByteArray::fromStdString(payload), &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject()) {
        return events;
    }

    QJsonObject root = doc.object();

    // Check for streaming token usage stats
    if (root.contains("usage") && root["usage"].isObject()) {
        QJsonObject usage = root["usage"].toObject();
        int promptTokens = usage["prompt_tokens"].toInt(0);
        int completionTokens = usage["completion_tokens"].toInt(0);
        events.push_back(events::TokenUsageUpdated{state.sessionId, promptTokens, completionTokens});
    }

    if (!root.contains("choices") || !root["choices"].isArray()) {
        return events;
    }

    QJsonArray choices = root["choices"].toArray();
    if (choices.isEmpty()) return events;

    QJsonObject firstChoice = choices.first().toObject();
    if (!firstChoice.contains("delta") || !firstChoice["delta"].isObject()) {
        return events;
    }

    QJsonObject delta = firstChoice["delta"].toObject();

    // 1. Check for dedicated reasoning_content (DeepSeek R1 / OpenAI o-series)
    if (delta.contains("reasoning_content") && delta["reasoning_content"].isString()) {
        std::string thinkChunk = delta["reasoning_content"].toString().toStdString();
        if (!thinkChunk.empty()) {
            state.inThinkingBlock = true;
            state.currentThinkingContent += thinkChunk;
            events.push_back(events::ThinkingDelta{state.sessionId, thinkChunk, currentTimestampMs});
        }
    }

    // 2. Check for standard content (processed for thinking tags)
    if (delta.contains("content") && delta["content"].isString()) {
        // If prior reasoning_content ended and normal content begins, close reasoning block
        if (state.inThinkingBlock && !delta.contains("reasoning_content") && state.partialTagBuffer.empty()) {
            events.push_back(events::ThinkingFinished{state.sessionId, currentTimestampMs});
            state.inThinkingBlock = false;
        }

        std::string contentChunk = delta["content"].toString().toStdString();
        if (!contentChunk.empty()) {
            auto subEvents = processContentForThinkingTags(contentChunk, state, currentTimestampMs);
            events.insert(events.end(), subEvents.begin(), subEvents.end());
        }
    }

    // 3. Check for streaming tool_calls
    if (delta.contains("tool_calls") && delta["tool_calls"].isArray()) {
        QJsonArray toolCalls = delta["tool_calls"].toArray();
        for (const auto& val : toolCalls) {
            if (!val.isObject()) continue;
            QJsonObject tc = val.toObject();
            int index = tc["index"].toInt(0);

            auto& pending = state.toolCallIndices[index];

            if (tc.contains("id") && tc["id"].isString()) {
                std::string callId = tc["id"].toString().toStdString();
                if (!callId.empty()) {
                    pending.id = callId;
                }
            }
            if (pending.id.empty()) {
                pending.id = "call_" + std::to_string(index) + "_" + std::to_string(currentTimestampMs);
            }

            if (tc.contains("function") && tc["function"].isObject()) {
                QJsonObject func = tc["function"].toObject();
                // Prevent duplicate concatenation if model emits identical name across deltas
                if (func.contains("name") && func["name"].isString()) {
                    std::string n = func["name"].toString().toStdString();
                    if (pending.name.empty()) {
                        pending.name = n;
                    }
                }
                if (func.contains("arguments") && func["arguments"].isString()) {
                    pending.argumentsBuffer += func["arguments"].toString().toStdString();
                }
            }
        }
    }

    // 4. Check for legacy function_call format
    if (delta.contains("function_call") && delta["function_call"].isObject()) {
        QJsonObject fc = delta["function_call"].toObject();
        auto& pending = state.toolCallIndices[0];
        if (pending.id.empty()) {
            pending.id = "func_" + std::to_string(currentTimestampMs);
        }
        if (fc.contains("name") && fc["name"].isString() && pending.name.empty()) {
            pending.name = fc["name"].toString().toStdString();
        }
        if (fc.contains("arguments") && fc["arguments"].isString()) {
            pending.argumentsBuffer += fc["arguments"].toString().toStdString();
        }
    }

    // If finish_reason indicates tool_calls or stop with pending calls, flush them
    if (firstChoice.contains("finish_reason") && firstChoice["finish_reason"].isString()) {
        std::string finish = firstChoice["finish_reason"].toString().toStdString();
        if (finish == "tool_calls" || (finish == "stop" && !state.toolCallIndices.empty())) {
            for (auto& [idx, tc] : state.toolCallIndices) {
                ToolCall call;
                call.id = tc.id;
                call.toolName = tc.name;
                call.rawArguments = tc.argumentsBuffer;
                call.timestampMs = currentTimestampMs;
                call.status = InterceptionStatus::PendingApproval;
                events.push_back(events::ToolCallDetected{state.sessionId, call});
            }
            state.toolCallIndices.clear();
        }
    }

    return events;
}

std::vector<FlowEvent> StreamParser::parseAnthropicLine(const std::string& line,
                                                        ParserSessionState& state,
                                                        int64_t currentTimestampMs) {
    std::vector<FlowEvent> events;

    std::string payload = line;
    if (payload.rfind("data: ", 0) == 0) {
        payload = payload.substr(6);
    }
    if (payload.empty()) return events;

    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(QByteArray::fromStdString(payload), &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject()) return events;

    QJsonObject root = doc.object();
    std::string type = root["type"].toString().toStdString();

    if (type == "message_start") {
        if (root.contains("message") && root["message"].isObject()) {
            QJsonObject msg = root["message"].toObject();
            if (msg.contains("usage") && msg["usage"].isObject()) {
                int inputTokens = msg["usage"].toObject()["input_tokens"].toInt(0);
                events.push_back(events::TokenUsageUpdated{state.sessionId, inputTokens, 0});
            }
        }
    } else if (type == "message_delta") {
        if (root.contains("usage") && root["usage"].isObject()) {
            int outputTokens = root["usage"].toObject()["output_tokens"].toInt(0);
            events.push_back(events::TokenUsageUpdated{state.sessionId, 0, outputTokens});
        }
    } else if (type == "content_block_start") {
        int index = root["index"].toInt(0);
        QJsonObject block = root["content_block"].toObject();
        std::string blockType = block["type"].toString().toStdString();

        if (blockType == "thinking") {
            state.inThinkingBlock = true;
        } else if (blockType == "tool_use") {
            std::string id = block["id"].toString().toStdString();
            std::string name = block["name"].toString().toStdString();
            state.anthropicBlockIndexToId[index] = id;
            state.activeAnthropicToolId = id;

            ParserSessionState::PendingToolCall tc;
            tc.id = id;
            tc.name = name;
            tc.blockIndex = index;
            state.namedToolCalls[id] = tc;
        }
    } else if (type == "content_block_delta") {
        int index = root["index"].toInt(0);
        QJsonObject delta = root["delta"].toObject();
        std::string deltaType = delta["type"].toString().toStdString();

        if (deltaType == "thinking_delta") {
            std::string thinkText = delta["thinking"].toString().toStdString();
            state.currentThinkingContent += thinkText;
            events.push_back(events::ThinkingDelta{state.sessionId, thinkText, currentTimestampMs});
        } else if (deltaType == "text_delta") {
            std::string text = delta["text"].toString().toStdString();
            auto sub = processContentForThinkingTags(text, state, currentTimestampMs);
            events.insert(events.end(), sub.begin(), sub.end());
        } else if (deltaType == "input_json_delta") {
            std::string partialJson = delta["partial_json"].toString().toStdString();

            // Correctly route delta to active tool call by block index
            std::string targetId;
            auto itIdx = state.anthropicBlockIndexToId.find(index);
            if (itIdx != state.anthropicBlockIndexToId.end()) {
                targetId = itIdx->second;
            } else if (!state.activeAnthropicToolId.empty()) {
                targetId = state.activeAnthropicToolId;
            }

            if (!targetId.empty()) {
                state.namedToolCalls[targetId].argumentsBuffer += partialJson;
            }
        }
    } else if (type == "content_block_stop") {
        int index = root["index"].toInt(0);

        if (state.inThinkingBlock) {
            events.push_back(events::ThinkingFinished{state.sessionId, currentTimestampMs});
            state.inThinkingBlock = false;
        }

        // Flush tool call associated with this block index
        auto itIdx = state.anthropicBlockIndexToId.find(index);
        if (itIdx != state.anthropicBlockIndexToId.end()) {
            std::string id = itIdx->second;
            auto itCall = state.namedToolCalls.find(id);
            if (itCall != state.namedToolCalls.end()) {
                ToolCall call;
                call.id = itCall->second.id;
                call.toolName = itCall->second.name;
                call.rawArguments = itCall->second.argumentsBuffer;
                call.timestampMs = currentTimestampMs;
                call.status = InterceptionStatus::PendingApproval;
                events.push_back(events::ToolCallDetected{state.sessionId, call});
                state.namedToolCalls.erase(itCall);
            }
            state.anthropicBlockIndexToId.erase(itIdx);
        }
    }

    return events;
}

std::vector<FlowEvent> StreamParser::parseGeminiLine(const std::string& line,
                                                     ParserSessionState& state,
                                                     int64_t currentTimestampMs) {
    std::vector<FlowEvent> events;

    std::string payload = line;
    if (payload.rfind("data: ", 0) == 0) {
        payload = payload.substr(6);
    }
    if (payload.empty()) return events;

    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(QByteArray::fromStdString(payload), &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject()) return events;

    QJsonObject root = doc.object();

    if (root.contains("usageMetadata") && root["usageMetadata"].isObject()) {
        QJsonObject u = root["usageMetadata"].toObject();
        int promptTokens = u["promptTokenCount"].toInt(0);
        int completionTokens = u["candidatesTokenCount"].toInt(0);
        events.push_back(events::TokenUsageUpdated{state.sessionId, promptTokens, completionTokens});
    }

    if (!root.contains("candidates") || !root["candidates"].isArray()) return events;
    QJsonArray candidates = root["candidates"].toArray();
    if (candidates.isEmpty()) return events;

    QJsonObject cand = candidates.first().toObject();
    if (!cand.contains("content") || !cand["content"].isObject()) return events;

    QJsonObject content = cand["content"].toObject();
    if (!content.contains("parts") || !content["parts"].isArray()) return events;

    for (const auto& pVal : content["parts"].toArray()) {
        if (!pVal.isObject()) continue;
        QJsonObject part = pVal.toObject();

        if (part.contains("thought") && part["thought"].toBool()) {
            std::string think = part["text"].toString().toStdString();
            state.inThinkingBlock = true;
            state.currentThinkingContent += think;
            events.push_back(events::ThinkingDelta{state.sessionId, think, currentTimestampMs});
            events.push_back(events::ThinkingFinished{state.sessionId, currentTimestampMs});
            state.inThinkingBlock = false;
        } else if (part.contains("text")) {
            if (state.inThinkingBlock) {
                events.push_back(events::ThinkingFinished{state.sessionId, currentTimestampMs});
                state.inThinkingBlock = false;
            }
            std::string text = part["text"].toString().toStdString();
            auto sub = processContentForThinkingTags(text, state, currentTimestampMs);
            events.insert(events.end(), sub.begin(), sub.end());
        } else if (part.contains("functionCall") && part["functionCall"].isObject()) {
            if (state.inThinkingBlock) {
                events.push_back(events::ThinkingFinished{state.sessionId, currentTimestampMs});
                state.inThinkingBlock = false;
            }
            QJsonObject fc = part["functionCall"].toObject();
            ToolCall call;
            call.id = "gemini_call_" + std::to_string(currentTimestampMs);
            call.toolName = fc["name"].toString().toStdString();
            call.rawArguments = QJsonDocument(fc["args"].toObject()).toJson(QJsonDocument::Compact).toStdString();
            call.timestampMs = currentTimestampMs;
            call.status = InterceptionStatus::PendingApproval;
            events.push_back(events::ToolCallDetected{state.sessionId, call});
        }
    }

    return events;
}

std::vector<FlowEvent> StreamParser::flush(ParserSessionState& state, int64_t currentTimestampMs) {
    std::vector<FlowEvent> events;

    if (state.inThinkingBlock) {
        events.push_back(events::ThinkingFinished{state.sessionId, currentTimestampMs});
        state.inThinkingBlock = false;
    }

    if (!state.partialTagBuffer.empty()) {
        state.currentAssistantContent += state.partialTagBuffer;
        events.push_back(events::ContentDelta{state.sessionId, state.partialTagBuffer, currentTimestampMs});
        state.partialTagBuffer.clear();
    }

    // Flush any pending OpenAI tool calls
    for (auto& [idx, tc] : state.toolCallIndices) {
        ToolCall call;
        call.id = tc.id;
        call.toolName = tc.name;
        call.rawArguments = tc.argumentsBuffer;
        call.timestampMs = currentTimestampMs;
        call.status = InterceptionStatus::PendingApproval;
        events.push_back(events::ToolCallDetected{state.sessionId, call});
    }
    state.toolCallIndices.clear();

    // Flush any pending Anthropic tool calls
    for (auto& [id, tc] : state.namedToolCalls) {
        ToolCall call;
        call.id = tc.id;
        call.toolName = tc.name;
        call.rawArguments = tc.argumentsBuffer;
        call.timestampMs = currentTimestampMs;
        call.status = InterceptionStatus::PendingApproval;
        events.push_back(events::ToolCallDetected{state.sessionId, call});
    }
    state.namedToolCalls.clear();
    state.anthropicBlockIndexToId.clear();
    state.activeAnthropicToolId.clear();

    return events;
}

std::vector<FlowEvent> StreamParser::parseChunk(const std::string& rawChunk,
                                                ParserSessionState& state,
                                                int64_t currentTimestampMs) {
    std::vector<FlowEvent> allEvents;
    std::istringstream stream(rawChunk);
    std::string line;

    while (std::getline(stream, line)) {
        if (line.empty() || line == "\r") continue;

        if (line.find("\"candidates\":") != std::string::npos) {
            auto evs = parseGeminiLine(line, state, currentTimestampMs);
            allEvents.insert(allEvents.end(), evs.begin(), evs.end());
        } else if (line.find("\"type\":") != std::string::npos &&
            (line.find("content_block_") != std::string::npos || 
             line.find("message_start") != std::string::npos ||
             line.find("message_delta") != std::string::npos)) {
            auto evs = parseAnthropicLine(line, state, currentTimestampMs);
            allEvents.insert(allEvents.end(), evs.begin(), evs.end());
        } else {
            auto evs = parseOpenAiLine(line, state, currentTimestampMs);
            allEvents.insert(allEvents.end(), evs.begin(), evs.end());
        }
    }

    return allEvents;
}

std::vector<FlowEvent> StreamParser::parseFullResponse(const std::string& sessionId,
                                                       const std::string& jsonBody,
                                                       int64_t currentTimestampMs) {
    std::vector<FlowEvent> events;
    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(QByteArray::fromStdString(jsonBody), &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject()) return events;

    QJsonObject root = doc.object();

    // 1. OpenAI Format
    if (root.contains("choices") && root["choices"].isArray()) {
        if (root.contains("usage") && root["usage"].isObject()) {
            QJsonObject u = root["usage"].toObject();
            events.push_back(events::TokenUsageUpdated{sessionId, u["prompt_tokens"].toInt(0), u["completion_tokens"].toInt(0)});
        }

        QJsonArray choices = root["choices"].toArray();
        if (!choices.isEmpty()) {
            QJsonObject first = choices.first().toObject();
            if (first.contains("message") && first["message"].isObject()) {
                QJsonObject msg = first["message"].toObject();
                if (msg.contains("reasoning_content") && msg["reasoning_content"].isString()) {
                    events.push_back(events::ThinkingDelta{sessionId, msg["reasoning_content"].toString().toStdString(), currentTimestampMs});
                    events.push_back(events::ThinkingFinished{sessionId, currentTimestampMs});
                }
                if (msg.contains("content") && msg["content"].isString()) {
                    events.push_back(events::ContentDelta{sessionId, msg["content"].toString().toStdString(), currentTimestampMs});
                }
                if (msg.contains("tool_calls") && msg["tool_calls"].isArray()) {
                    for (const auto& tcVal : msg["tool_calls"].toArray()) {
                        QJsonObject tc = tcVal.toObject();
                        ToolCall call;
                        call.id = tc["id"].toString().toStdString();
                        QJsonObject func = tc["function"].toObject();
                        call.toolName = func["name"].toString().toStdString();
                        call.rawArguments = func["arguments"].toString().toStdString();
                        call.timestampMs = currentTimestampMs;
                        events.push_back(events::ToolCallDetected{sessionId, call});
                    }
                }
            }
        }
        return events;
    }

    // 2. Anthropic Messages Format
    if (root.contains("content") && root["content"].isArray()) {
        if (root.contains("usage") && root["usage"].isObject()) {
            QJsonObject u = root["usage"].toObject();
            events.push_back(events::TokenUsageUpdated{sessionId, u["input_tokens"].toInt(0), u["output_tokens"].toInt(0)});
        }

        QJsonArray contentArray = root["content"].toArray();
        for (const auto& itemVal : contentArray) {
            if (!itemVal.isObject()) continue;
            QJsonObject item = itemVal.toObject();
            std::string type = item["type"].toString().toStdString();

            if (type == "thinking") {
                std::string think = item["thinking"].toString().toStdString();
                events.push_back(events::ThinkingDelta{sessionId, think, currentTimestampMs});
                events.push_back(events::ThinkingFinished{sessionId, currentTimestampMs});
            } else if (type == "text") {
                std::string txt = item["text"].toString().toStdString();
                events.push_back(events::ContentDelta{sessionId, txt, currentTimestampMs});
            } else if (type == "tool_use") {
                ToolCall call;
                call.id = item["id"].toString().toStdString();
                call.toolName = item["name"].toString().toStdString();
                if (item.contains("input") && item["input"].isObject()) {
                    call.rawArguments = QJsonDocument(item["input"].toObject()).toJson(QJsonDocument::Compact).toStdString();
                }
                call.timestampMs = currentTimestampMs;
                events.push_back(events::ToolCallDetected{sessionId, call});
            }
        }
        return events;
    }

    // 3. Gemini Format
    if (root.contains("candidates") && root["candidates"].isArray()) {
        if (root.contains("usageMetadata") && root["usageMetadata"].isObject()) {
            QJsonObject u = root["usageMetadata"].toObject();
            events.push_back(events::TokenUsageUpdated{sessionId, u["promptTokenCount"].toInt(0), u["candidatesTokenCount"].toInt(0)});
        }

        QJsonArray candidates = root["candidates"].toArray();
        if (!candidates.isEmpty()) {
            QJsonObject cand = candidates.first().toObject();
            if (cand.contains("content") && cand["content"].isObject()) {
                QJsonObject content = cand["content"].toObject();
                if (content.contains("parts") && content["parts"].isArray()) {
                    for (const auto& pVal : content["parts"].toArray()) {
                        if (!pVal.isObject()) continue;
                        QJsonObject part = pVal.toObject();
                        if (part.contains("thought") && part["thought"].toBool()) {
                            events.push_back(events::ThinkingDelta{sessionId, part["text"].toString().toStdString(), currentTimestampMs});
                            events.push_back(events::ThinkingFinished{sessionId, currentTimestampMs});
                        } else if (part.contains("text")) {
                            events.push_back(events::ContentDelta{sessionId, part["text"].toString().toStdString(), currentTimestampMs});
                        } else if (part.contains("functionCall") && part["functionCall"].isObject()) {
                            QJsonObject fc = part["functionCall"].toObject();
                            ToolCall call;
                            call.id = "gemini_call_" + std::to_string(currentTimestampMs);
                            call.toolName = fc["name"].toString().toStdString();
                            call.rawArguments = QJsonDocument(fc["args"].toObject()).toJson(QJsonDocument::Compact).toStdString();
                            call.timestampMs = currentTimestampMs;
                            events.push_back(events::ToolCallDetected{sessionId, call});
                        }
                    }
                }
            }
        }
        return events;
    }

    return events;
}

} // namespace aiflow::core
