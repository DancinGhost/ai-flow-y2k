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
            outActions.push_back(actions::LogAuditEvent{
                ev.sessionId,
                "Session started: " + ev.agentName + " [" + ev.modelName + "]",
                "INFO",
                ev.timestampMs
            });
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

            // Deep analysis of hazard and severity
            HazardMatch hazard = PolicyEngine::analyzeHazard(call.rawArguments);
            HazardMatch toolHazard = PolicyEngine::analyzeTool(call.toolName, call.rawArguments, next.dangerousToolPatterns);

            call.isDangerous = toolHazard.matched || hazard.matched;
            if (hazard.matched) {
                call.hazardCategory = hazard.category;
                call.hazardSeverity = PolicyEngine::severityToString(hazard.severity);
            } else if (toolHazard.matched) {
                call.hazardCategory = toolHazard.category;
                call.hazardSeverity = PolicyEngine::severityToString(toolHazard.severity);
            } else {
                call.hazardCategory = "none";
                call.hazardSeverity = "None";
            }

            // Determine status based on custom rules, patterns, and mode
            call.status = PolicyEngine::evaluateCallWithRules(call, next.interceptionMode, next.dangerousToolPatterns, next.customPolicyRules);

            for (auto& s : next.sessions) {
                if (s.sessionId == ev.sessionId) {
                    s.interceptedCalls.push_back(call);
                    break;
                }
            }

            if (call.status == InterceptionStatus::PendingApproval) {
                outActions.push_back(actions::PauseResponseStream{ev.sessionId, call.id});
                outActions.push_back(actions::LogAuditEvent{
                    ev.sessionId,
                    "Tool call intercepted: " + call.toolName + " [Severity: " + call.hazardSeverity + "]",
                    "SECURITY",
                    call.timestampMs
                });
            } else {
                outActions.push_back(actions::ResumeResponseStream{ev.sessionId, call.id, call.rawArguments});
                outActions.push_back(actions::LogAuditEvent{
                    ev.sessionId,
                    "Tool call auto-passed: " + call.toolName,
                    "INFO",
                    call.timestampMs
                });
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
                                s.approvedCallsCount++;
                                outActions.push_back(actions::ResumeResponseStream{ev.sessionId, ev.callId, c.rawArguments});
                                outActions.push_back(actions::LogAuditEvent{
                                    ev.sessionId,
                                    "User APPROVED tool call: " + c.toolName + " (id: " + ev.callId + ")",
                                    "INFO",
                                    c.timestampMs
                                });
                            } else if (ev.status == InterceptionStatus::Modified) {
                                s.modifiedCallsCount++;
                                outActions.push_back(actions::ResumeResponseStream{ev.sessionId, ev.callId, ev.modifiedArgs});
                                outActions.push_back(actions::LogAuditEvent{
                                    ev.sessionId,
                                    "User MODIFIED tool call: " + c.toolName + " (id: " + ev.callId + ")",
                                    "WARN",
                                    c.timestampMs
                                });
                            } else if (ev.status == InterceptionStatus::Rejected) {
                                s.rejectedCallsCount++;
                                outActions.push_back(actions::EmitToolRejection{ev.sessionId, ev.callId, ev.reason});
                                outActions.push_back(actions::LogAuditEvent{
                                    ev.sessionId,
                                    "User REJECTED tool call: " + c.toolName + " (reason: " + ev.reason + ")",
                                    "SECURITY",
                                    c.timestampMs
                                });
                            }
                            break;
                        }
                    }
                    break;
                }
            }
            outActions.push_back(actions::NotifyUIRefresh{});
        }
        else if constexpr (std::is_same_v<T, events::ToolCallExecuting>) {
            for (auto& s : next.sessions) {
                if (s.sessionId == ev.sessionId) {
                    for (auto& c : s.interceptedCalls) {
                        if (c.id == ev.callId) {
                            c.status = InterceptionStatus::Executing;
                            outActions.push_back(actions::LogAuditEvent{
                                ev.sessionId,
                                "Tool executing: " + c.toolName + " (id: " + ev.callId + ")",
                                "INFO",
                                ev.timestampMs
                            });
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
                            outActions.push_back(actions::LogAuditEvent{
                                ev.sessionId,
                                "Tool execution completed: " + c.toolName + " (id: " + ev.callId + ")",
                                "INFO",
                                ev.timestampMs
                            });
                            break;
                        }
                    }
                    break;
                }
            }
            outActions.push_back(actions::NotifyUIRefresh{});
        }
        else if constexpr (std::is_same_v<T, events::TokenUsageUpdated>) {
            for (auto& s : next.sessions) {
                if (s.sessionId == ev.sessionId) {
                    if (ev.promptTokens > 0) s.promptTokens = ev.promptTokens;
                    if (ev.completionTokens > 0) s.completionTokens = ev.completionTokens;
                    next.totalTokensProcessed += (ev.promptTokens + ev.completionTokens);
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
                    next.totalTokensProcessed += (ev.promptTokens + ev.completionTokens);
                    outActions.push_back(actions::LogAuditEvent{
                        ev.sessionId,
                        "Session completed. Total tokens: " + std::to_string(ev.promptTokens + ev.completionTokens),
                        "INFO",
                        ev.timestampMs
                    });
                    break;
                }
            }
            outActions.push_back(actions::NotifyUIRefresh{});
        }
        else if constexpr (std::is_same_v<T, events::InterceptionModeChanged>) {
            next.interceptionMode = ev.newMode;
            outActions.push_back(actions::LogAuditEvent{
                next.activeSessionId,
                "Interception mode updated",
                "INFO",
                0
            });
            outActions.push_back(actions::NotifyUIRefresh{});
        }
        else if constexpr (std::is_same_v<T, events::ThemeChanged>) {
            next.currentTheme = ev.newTheme;
            outActions.push_back(actions::NotifyUIRefresh{});
        }
        else if constexpr (std::is_same_v<T, events::PolicyRuleAdded>) {
            next.customPolicyRules.push_back(ev.rule);
            outActions.push_back(actions::LogAuditEvent{
                next.activeSessionId,
                "Custom policy rule added: " + ev.rule.name,
                "INFO",
                0
            });
            outActions.push_back(actions::NotifyUIRefresh{});
        }
        else if constexpr (std::is_same_v<T, events::SessionReset>) {
            for (auto& s : next.sessions) {
                if (s.sessionId == ev.sessionId) {
                    s.messages.clear();
                    s.thinkingHistory.clear();
                    s.interceptedCalls.clear();
                    s.approvedCallsCount = 0;
                    s.rejectedCallsCount = 0;
                    s.modifiedCallsCount = 0;
                    outActions.push_back(actions::LogAuditEvent{
                        ev.sessionId,
                        "Session data reset",
                        "INFO",
                        0
                    });
                    break;
                }
            }
            outActions.push_back(actions::NotifyUIRefresh{});
        }
    }, event);

    return next;
}

} // namespace aiflow::core
