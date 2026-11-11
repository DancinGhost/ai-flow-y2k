#include "core/stream_parser.hpp"
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonValue>
#include <sstream>

namespace aiflow::core {

std::vector<FlowEvent> StreamParser::processContentForThinkingTags(const std::string& text,
                                                                   ParserSessionState& state,
                                                                   int64_t currentTimestampMs) {
    std::vector<FlowEvent> events;
    std::string remaining = text;

    while (!remaining.empty()) {
        if (!state.inThinkingBlock) {
            size_t tagPos = remaining.find("<thinking>");
            if (tagPos == std::string::npos) {
                tagPos = remaining.find("<thought>");
            }

            if (tagPos != std::string::npos) {
                // Preceding content is regular text
                if (tagPos > 0) {
                    std::string pre = remaining.substr(0, tagPos);
                    state.currentAssistantContent += pre;
                    events.push_back(events::ContentDelta{state.sessionId, pre, currentTimestampMs});
                }
                state.inThinkingBlock = true;
                size_t tagLen = (remaining.substr(tagPos, 9) == "<thought>") ? 9 : 10;
                remaining = remaining.substr(tagPos + tagLen);
            } else {
                state.currentAssistantContent += remaining;
                events.push_back(events::ContentDelta{state.sessionId, remaining, currentTimestampMs});
                remaining.clear();
            }
        } else {
            size_t endTagPos = remaining.find("</thinking>");
            size_t tagLen = 11;
            if (endTagPos == std::string::npos) {
                endTagPos = remaining.find("</thought>");
                tagLen = 10;
            }

            if (endTagPos != std::string::npos) {
                std::string thinkDelta = remaining.substr(0, endTagPos);
                state.currentThinkingContent += thinkDelta;
                events.push_back(events::ThinkingDelta{state.sessionId, thinkDelta, currentTimestampMs});
                events.push_back(events::ThinkingFinished{state.sessionId, currentTimestampMs});
                state.inThinkingBlock = false;
                remaining = remaining.substr(endTagPos + tagLen);
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
        if (state.inThinkingBlock) {
            events.push_back(events::ThinkingFinished{state.sessionId, currentTimestampMs});
            state.inThinkingBlock = false;
        }
        return events;
    }

    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(QByteArray::fromStdString(payload), &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject()) {
        return events;
    }

    QJsonObject root = doc.object();
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

    // 2. Check for standard content
    if (delta.contains("content") && delta["content"].isString()) {
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
                pending.id = tc["id"].toString().toStdString();
            }
            if (pending.id.empty()) {
                pending.id = "call_" + std::to_string(index) + "_" + std::to_string(currentTimestampMs);
            }

            if (tc.contains("function") && tc["function"].isObject()) {
                QJsonObject func = tc["function"].toObject();
                if (func.contains("name") && func["name"].isString()) {
                    pending.name += func["name"].toString().toStdString();
                }
                if (func.contains("arguments") && func["arguments"].isString()) {
                    pending.argumentsBuffer += func["arguments"].toString().toStdString();
                }
            }
        }
    }

    // If finish_reason is "tool_calls", flush completed tool calls
    if (firstChoice.contains("finish_reason") && firstChoice["finish_reason"].isString()) {
        std::string finish = firstChoice["finish_reason"].toString().toStdString();
        if (finish == "tool_calls") {
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

    if (type == "content_block_start") {
        QJsonObject block = root["content_block"].toObject();
        std::string blockType = block["type"].toString().toStdString();
        if (blockType == "thinking") {
            state.inThinkingBlock = true;
        } else if (blockType == "tool_use") {
            ParserSessionState::PendingToolCall tc;
            tc.id = block["id"].toString().toStdString();
            tc.name = block["name"].toString().toStdString();
            state.namedToolCalls[tc.id] = tc;
        }
    } else if (type == "content_block_delta") {
        QJsonObject delta = root["delta"].toObject();
        std::string deltaType = delta["type"].toString().toStdString();
        if (deltaType == "thinking_delta") {
            std::string thinkText = delta["thinking"].toString().toStdString();
            state.currentThinkingContent += thinkText;
            events.push_back(events::ThinkingDelta{state.sessionId, thinkText, currentTimestampMs});
        } else if (deltaType == "text_delta") {
            std::string text = delta["text"].toString().toStdString();
            events.push_back(events::ContentDelta{state.sessionId, text, currentTimestampMs});
        } else if (deltaType == "input_json_delta") {
            std::string partialJson = delta["partial_json"].toString().toStdString();
            // Append to current tool call
            if (!state.namedToolCalls.empty()) {
                state.namedToolCalls.rbegin()->second.argumentsBuffer += partialJson;
            }
        }
    } else if (type == "content_block_stop") {
        if (state.inThinkingBlock) {
            events.push_back(events::ThinkingFinished{state.sessionId, currentTimestampMs});
            state.inThinkingBlock = false;
        }
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
    }

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

        if (line.find("\"type\":") != std::string::npos &&
            (line.find("content_block_") != std::string::npos || line.find("message_start") != std::string::npos)) {
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

    // Check OpenAI format
    if (root.contains("choices") && root["choices"].isArray()) {
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
    }

    return events;
}

} // namespace aiflow::core
