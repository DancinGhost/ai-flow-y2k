#include <cassert>
#include <iostream>
#include "core/models.hpp"
#include "core/policy_engine.hpp"
#include "core/stream_parser.hpp"
#include "core/theme_engine.hpp"
#include "core/flow_reducer.hpp"

using namespace aiflow::core;

void testPolicyEngine() {
    std::cout << "[TEST] Running PolicyEngine tests..." << std::endl;
    std::vector<std::string> dangerous = {"bash", "shell", "rm", "write_file", "delete"};

    // 1. Safe tool should not be dangerous
    assert(!PolicyEngine::isDangerousTool("calculator", "{\"expr\": \"2+2\"}", dangerous));

    // 2. Shell tool matches dangerous pattern
    assert(PolicyEngine::isDangerousTool("bash", "{\"command\": \"ls\"}", dangerous));

    // 3. Hazardous payload check
    assert(PolicyEngine::containsHazardousPayload("{\"cmd\": \"rm -rf /\"}"));
    assert(!PolicyEngine::containsHazardousPayload("{\"cmd\": \"echo hello world\"}"));

    // 4. Interception modes
    ToolCall safeCall;
    safeCall.toolName = "read_file";
    safeCall.rawArguments = "{\"path\": \"readme.txt\"}";

    ToolCall dangerCall;
    dangerCall.toolName = "bash";
    dangerCall.rawArguments = "{\"command\": \"rm -rf /tmp/test\"}";

    // Under MonitorOnly: both auto-passed
    assert(PolicyEngine::evaluateCall(safeCall, InterceptionMode::MonitorOnly, dangerous) == InterceptionStatus::AutoPassed);
    assert(PolicyEngine::evaluateCall(dangerCall, InterceptionMode::MonitorOnly, dangerous) == InterceptionStatus::AutoPassed);

    // Under InterceptDangerousOnly: safe passes, danger requires approval
    assert(PolicyEngine::evaluateCall(safeCall, InterceptionMode::InterceptDangerousOnly, dangerous) == InterceptionStatus::AutoPassed);
    assert(PolicyEngine::evaluateCall(dangerCall, InterceptionMode::InterceptDangerousOnly, dangerous) == InterceptionStatus::PendingApproval);

    // Under InterceptAllTools: both require approval
    assert(PolicyEngine::evaluateCall(safeCall, InterceptionMode::InterceptAllTools, dangerous) == InterceptionStatus::PendingApproval);
    assert(PolicyEngine::evaluateCall(dangerCall, InterceptionMode::InterceptAllTools, dangerous) == InterceptionStatus::PendingApproval);

    std::cout << "  -> PolicyEngine tests passed!" << std::endl;
}

void testStreamParser() {
    std::cout << "[TEST] Running StreamParser tests..." << std::endl;

    ParserSessionState state;
    state.sessionId = "sess_test";

    // 1. OpenAI stream with reasoning_content
    std::string sseReasoning = "data: {\"choices\":[{\"delta\":{\"reasoning_content\":\"Let me ponder this question.\"}}]}\n\n";
    auto events1 = StreamParser::parseChunk(sseReasoning, state, 1000);
    assert(!events1.empty());
    assert(std::holds_alternative<events::ThinkingDelta>(events1[0]));
    auto& td = std::get<events::ThinkingDelta>(events1[0]);
    assert(td.deltaText == "Let me ponder this question.");

    // 2. Embedded thinking tags in standard content
    std::string textWithTags = "Hello world! <thinking>Investigating system logs...</thinking> Here is the answer.";
    auto tagEvents = StreamParser::parseChunk("data: {\"choices\":[{\"delta\":{\"content\":\"" + textWithTags + "\"}}]}\n\n", state, 2000);
    bool foundThinking = false;
    bool foundContent = false;
    for (const auto& ev : tagEvents) {
        if (std::holds_alternative<events::ThinkingDelta>(ev)) foundThinking = true;
        if (std::holds_alternative<events::ContentDelta>(ev)) foundContent = true;
    }
    assert(foundThinking);
    assert(foundContent);

    std::cout << "  -> StreamParser tests passed!" << std::endl;
}

void testThemeEngine() {
    std::cout << "[TEST] Running ThemeEngine tests..." << std::endl;

    // Verify all 3 themes generate valid non-empty tokens and QSS
    auto vaporTokens = ThemeEngine::getTokens(ThemeType::Vaporwave);
    assert(!vaporTokens.primaryColor.empty());
    std::string vaporQss = ThemeEngine::generateStyleSheet(ThemeType::Vaporwave);
    assert(vaporQss.find("QMainWindow") != std::string::npos);

    auto rainTokens = ThemeEngine::getTokens(ThemeType::DigitalRain);
    assert(!rainTokens.primaryColor.empty());
    std::string rainQss = ThemeEngine::generateStyleSheet(ThemeType::DigitalRain);
    assert(rainQss.find("QMainWindow") != std::string::npos);

    auto aeroTokens = ThemeEngine::getTokens(ThemeType::FrutigerAero);
    assert(!aeroTokens.primaryColor.empty());
    std::string aeroQss = ThemeEngine::generateStyleSheet(ThemeType::FrutigerAero);
    assert(aeroQss.find("QMainWindow") != std::string::npos);

    std::cout << "  -> ThemeEngine tests passed!" << std::endl;
}

void testFlowReducer() {
    std::cout << "[TEST] Running FlowReducer tests..." << std::endl;

    AppState state;
    state.interceptionMode = InterceptionMode::InterceptDangerousOnly;

    // 1. Session Start Event
    std::vector<DomainAction> actions;
    state = FlowReducer::reduce(state, events::SessionStarted{"s1", "gpt-4o", "TestAgent", 100}, actions);
    assert(state.sessions.size() == 1);
    assert(state.sessions[0].sessionId == "s1");

    // 2. Thinking Delta
    actions.clear();
    state = FlowReducer::reduce(state, events::ThinkingDelta{"s1", "Thinking deep thoughts...", 200}, actions);
    assert(state.sessions[0].thinkingHistory.size() == 1);
    assert(state.sessions[0].thinkingHistory[0].content == "Thinking deep thoughts...");

    // 3. Dangerous Tool Call Detected
    actions.clear();
    ToolCall tc;
    tc.id = "c1";
    tc.toolName = "bash";
    tc.rawArguments = "{\"command\": \"rm -rf /tmp/data\"}";
    state = FlowReducer::reduce(state, events::ToolCallDetected{"s1", tc}, actions);

    assert(state.sessions[0].interceptedCalls.size() == 1);
    assert(state.sessions[0].interceptedCalls[0].status == InterceptionStatus::PendingApproval);

    // Verify pause action was dispatched
    bool pauseDispatched = false;
    for (const auto& a : actions) {
        if (std::holds_alternative<actions::PauseResponseStream>(a)) {
            pauseDispatched = true;
        }
    }
    assert(pauseDispatched);

    // 4. Decision: Human Approves
    actions.clear();
    state = FlowReducer::reduce(state, events::ToolCallDecisionMade{"s1", "c1", InterceptionStatus::Approved, tc.rawArguments, ""}, actions);
    assert(state.sessions[0].interceptedCalls[0].status == InterceptionStatus::Approved);

    bool resumeDispatched = false;
    for (const auto& a : actions) {
        if (std::holds_alternative<actions::ResumeResponseStream>(a)) {
            resumeDispatched = true;
        }
    }
    assert(resumeDispatched);

    std::cout << "  -> FlowReducer tests passed!" << std::endl;
}

int main() {
    std::cout << "=================================================" << std::endl;
    std::cout << "  AI-FLOW-Y2K FUNCTIONAL CORE TEST SUITE         " << std::endl;
    std::cout << "=================================================" << std::endl;

    testPolicyEngine();
    testStreamParser();
    testThemeEngine();
    testFlowReducer();

    std::cout << "\n>>> ALL FUNCTIONAL CORE TESTS PASSED SUCCESSFULLY! <<<" << std::endl;
    return 0;
}
