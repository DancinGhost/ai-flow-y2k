#include "core/flow_reducer.hpp"
#include "core/policy_engine.hpp"

namespace aiflow::core {

AppState FlowReducer::reduce(const AppState& state,
                             const FlowEvent& event,
                             std::vector<DomainAction>& outActions) {
    AppState next = state;

    std::visit([&](const auto& ev) {
        using T = std::decay_t<decltype(ev)>;

        if constexpr (std::is_same_v<T, events::SessionStarted>) {
            AgentSession session;
            session.sessionId = ev.sessionId;
            session.modelName = ev.modelName;
            session.agentName = ev.agentName;
            session.isLive = true;
            next.sessions.push_back(session);
            next.activeSessionId = ev.sessionId;
            outActions.push_back(actions::NotifyUIRefresh{});
        }
        else if constexpr (std::is_same_v<T, events::ThinkingDelta>) {
            for (auto& s : next.sessions) {
                if (s.sessionId == ev.sessionId) {
                    if (s.thinkingHistory.empty() || !s.thinkingHistory.back().isStreaming) {
                        ThinkingBlock block;
                        block.id = "think_" + std::to_string(ev.timestampMs);
                        block.startTimestampMs = ev.timestampMs;
                        block.isStreaming = true;
                        block.content = ev.deltaText;
                        s.thinkingHistory.push_back(block);
                    } else {
                        s.thinkingHistory.back().content += ev.deltaText;
                    }
                    break;
                }
            }
            outActions.push_back(actions::NotifyUIRefresh{});
        }
        else if constexpr (std::is_same_v<T, events::ThinkingFinished>) {
            for (auto& s : next.sessions) {
                if (s.sessionId == ev.sessionId) {
                    if (!s.thinkingHistory.empty() && s.thinkingHistory.back().isStreaming) {
                        s.thinkingHistory.back().isStreaming = false;
                        s.thinkingHistory.back().endTimestampMs = ev.timestampMs;
                    }
                    break;
                }
            }
            outActions.push_back(actions::NotifyUIRefresh{});
        }
        else if constexpr (std::is_same_v<T, events::ContentDelta>) {
            for (auto& s : next.sessions) {
                if (s.sessionId == ev.sessionId) {
                    if (s.messages.empty() || s.messages.back().role != "assistant") {
                        AgentMessage msg;
                        msg.role = "assistant";
                        msg.content = ev.deltaText;
                        msg.timestampMs = ev.timestampMs;
                        s.messages.push_back(msg);
                    } else {
                        s.messages.back().content += ev.deltaText;
                    }
                    break;
                }
            }
            outActions.push_back(actions::NotifyUIRefresh{});
        }
        else if constexpr (std::is_same_v<T, events::ToolCallDetected>) {
            ToolCall call = ev.call;
            call.isDangerous = PolicyEngine::isDangerousTool(call.toolName, call.rawArguments, next.dangerousToolPatterns);
            call.status = PolicyEngine::evaluateCall(call, next.interceptionMode, next.dangerousToolPatterns);

            for (auto& s : next.sessions) {
                if (s.sessionId == ev.sessionId) {
                    s.interceptedCalls.push_back(call);
                    break;
                }
            }

            if (call.status == InterceptionStatus::PendingApproval) {
                outActions.push_back(actions::PauseResponseStream{ev.sessionId, call.id});
            } else {
                outActions.push_back(actions::ResumeResponseStream{ev.sessionId, call.id, call.rawArguments});
            }
            outActions.push_back(actions::NotifyUIRefresh{});
        }
        else if constexpr (std::is_same_v<T, events::ToolCallDecisionMade>) {
            for (auto& s : next.sessions) {
                if (s.sessionId == ev.sessionId) {
                    for (auto& c : s.interceptedCalls) {
                        if (c.id == ev.callId) {
                            c.status = ev.status;
                            c.modifiedArguments = ev.modifiedArgs;
                            c.rejectionReason = ev.reason;

                            if (ev.status == InterceptionStatus::Approved) {
                                outActions.push_back(actions::ResumeResponseStream{ev.sessionId, ev.callId, c.rawArguments});
                            } else if (ev.status == InterceptionStatus::Modified) {
                                outActions.push_back(actions::ResumeResponseStream{ev.sessionId, ev.callId, ev.modifiedArgs});
                            } else if (ev.status == InterceptionStatus::Rejected) {
                                outActions.push_back(actions::EmitToolRejection{ev.sessionId, ev.callId, ev.reason});
                            }
                            break;
                        }
                    }
                    break;
                }
            }
            outActions.push_back(actions::NotifyUIRefresh{});
        }
        else if constexpr (std::is_same_v<T, events::ToolResultReceived>) {
            for (auto& s : next.sessions) {
                if (s.sessionId == ev.sessionId) {
                    for (auto& c : s.interceptedCalls) {
                        if (c.id == ev.callId) {
                            c.executionResult = ev.output;
                            c.status = InterceptionStatus::Completed;
                            break;
                        }
                    }
                    break;
                }
            }
            outActions.push_back(actions::NotifyUIRefresh{});
        }
        else if constexpr (std::is_same_v<T, events::SessionCompleted>) {
            for (auto& s : next.sessions) {
                if (s.sessionId == ev.sessionId) {
                    s.isLive = false;
                    s.promptTokens = ev.promptTokens;
                    s.completionTokens = ev.completionTokens;
                    break;
                }
            }
            outActions.push_back(actions::NotifyUIRefresh{});
        }
    }, event);

    return next;
}

} // namespace aiflow::core
