#include "shell/simulator.hpp"
#include <QDateTime>

namespace aiflow::shell {

AgentSimulator::AgentSimulator(QObject* parent)
    : QObject(parent), m_timer(new QTimer(this)) {
    m_timer->setSingleShot(true);
    connect(m_timer, &QTimer::timeout, this, &AgentSimulator::onTick);
}

AgentSimulator::~AgentSimulator() {
    stopSimulation();
}

bool AgentSimulator::isRunning() const {
    return m_timer->isActive() || m_waitingForDecision;
}

void AgentSimulator::buildScenario() {
    m_scenario.clear();
    qint64 now = QDateTime::currentMSecsSinceEpoch();
    m_activeSessionId = "sim_" + std::to_string(now);

    // Step 0: Session Start
    SimStep s0;
    s0.type = SimStep::Type::StartSession;
    s0.delayMs = 200;
    m_scenario.push_back(s0);

    // Step 1-4: Agent Thinking Stream
    SimStep t1;
    t1.type = SimStep::Type::ThinkingChunk;
    t1.text = "Initial objective: Resolve database performance bottleneck.\nExamining recent query plans...";
    t1.delayMs = 500;
    m_scenario.push_back(t1);

    SimStep t2;
    t2.type = SimStep::Type::ThinkingChunk;
    t2.text = "\nFound 3 unindexed queries scanning large tables. Before modifying schema, I must check storage and existing tables.";
    t2.delayMs = 600;
    m_scenario.push_back(t2);

    SimStep t3;
    t3.type = SimStep::Type::EndThinking;
    t3.delayMs = 300;
    m_scenario.push_back(t3);

    // Step 5: Safe Tool Call (Reads data)
    SimStep call1;
    call1.type = SimStep::Type::EmitToolCall;
    call1.toolCall.id = "call_sim_read_" + std::to_string(now);
    call1.toolCall.toolName = "sql_query";
    call1.toolCall.rawArguments = "{\"query\": \"SELECT relname, n_dead_tup FROM pg_stat_user_tables ORDER BY n_dead_tup DESC LIMIT 5;\"}";
    call1.delayMs = 400;
    m_scenario.push_back(call1);

    // Step 6: Tool Output
    SimStep out1;
    out1.type = SimStep::Type::ToolOutput;
    out1.text = "[{\"relname\": \"events_cache\", \"n_dead_tup\": 42109}, {\"relname\": \"user_sessions\", \"n_dead_tup\": 8901}]";
    out1.delayMs = 500;
    m_scenario.push_back(out1);

    // Step 7: Second Thinking Block (Dangerous tool contemplated)
    SimStep t4;
    t4.type = SimStep::Type::ThinkingChunk;
    t4.text = "The events_cache table has excessive dead tuples and bloated indexes.\nI should purge stale cache tables and restart the background daemon.";
    t4.delayMs = 600;
    m_scenario.push_back(t4);

    SimStep t5;
    t5.type = SimStep::Type::EndThinking;
    t5.delayMs = 200;
    m_scenario.push_back(t5);

    // Step 8: Dangerous Tool Call that hits Interception Breakpoint!
    SimStep call2;
    call2.type = SimStep::Type::EmitToolCall;
    call2.toolCall.id = "call_sim_danger_" + std::to_string(now);
    call2.toolCall.toolName = "bash";
    call2.toolCall.rawArguments = "{\"command\": \"sudo systemctl stop pg-worker && rm -rf /var/cache/pg_temp/* && vacuumdb -f -z db_prod\"}";
    call2.delayMs = 400;
    m_scenario.push_back(call2);

    // Step 9: Wait for human decision
    SimStep wait;
    wait.type = SimStep::Type::WaitForDecision;
    wait.delayMs = 100;
    m_scenario.push_back(wait);

    // Step 10: Tool Output after approval/modification
    SimStep out2;
    out2.type = SimStep::Type::ToolOutput;
    out2.text = "Successfully vacuumed and freed 4.2 GB disk space. Daemon restarted cleanly.";
    out2.delayMs = 600;
    m_scenario.push_back(out2);

    // Step 11: Final Assistant Response
    SimStep resp;
    resp.type = SimStep::Type::AssistantChunk;
    resp.text = "Completed optimization. Database maintenance concluded safely with human verification.";
    resp.delayMs = 500;
    m_scenario.push_back(resp);

    // Step 12: Finish Session
    SimStep fin;
    fin.type = SimStep::Type::FinishSession;
    fin.delayMs = 300;
    m_scenario.push_back(fin);
}

void AgentSimulator::startSimulation() {
    stopSimulation();
    buildScenario();
    m_currentStepIndex = 0;
    m_waitingForDecision = false;
    m_decisionHandled = false;
    emit logMessage("Starting simulated agent workflow...");
    onTick();
}

void AgentSimulator::stopSimulation() {
    m_timer->stop();
    m_waitingForDecision = false;
    m_decisionHandled = false;
    m_scenario.clear();
}

void AgentSimulator::notifyToolDecision(const std::string& callId, aiflow::core::InterceptionStatus status) {
    if (m_pendingCallId == callId) {
        m_decisionHandled = true;
        if (m_waitingForDecision) {
            m_waitingForDecision = false;
            emit logMessage(QString("Tool decision acknowledged for %1. Resuming simulation.").arg(QString::fromStdString(callId)));

            if (status == aiflow::core::InterceptionStatus::Rejected) {
                // If rejected, agent explains gracefully
                qint64 now = QDateTime::currentMSecsSinceEpoch();
                emit eventGenerated(aiflow::core::events::ContentDelta{
                    m_activeSessionId,
                    "\n[Agent]: The command was rejected by the supervisor. Aborting risky purge operation.",
                    now
                });
                emit simulationFinished();
                return;
            }

            m_currentStepIndex++;
            if (m_currentStepIndex < m_scenario.size()) {
                m_timer->start(m_scenario[m_currentStepIndex].delayMs);
            } else {
                emit simulationFinished();
            }
        }
    }
}

void AgentSimulator::onTick() {
    if (m_currentStepIndex >= m_scenario.size()) {
        emit simulationFinished();
        return;
    }

    const auto& step = m_scenario[m_currentStepIndex];
    qint64 now = QDateTime::currentMSecsSinceEpoch();

    switch (step.type) {
        case SimStep::Type::StartSession:
            emit eventGenerated(aiflow::core::events::SessionStarted{
                m_activeSessionId,
                "claude-3-7-sonnet-synthetic",
                "SystemMaintenanceAgent",
                now
            });
            break;

        case SimStep::Type::ThinkingChunk:
            emit eventGenerated(aiflow::core::events::ThinkingDelta{
                m_activeSessionId,
                step.text.toStdString(),
                now
            });
            break;

        case SimStep::Type::EndThinking:
            emit eventGenerated(aiflow::core::events::ThinkingFinished{
                m_activeSessionId,
                now
            });
            break;

        case SimStep::Type::AssistantChunk:
            emit eventGenerated(aiflow::core::events::ContentDelta{
                m_activeSessionId,
                step.text.toStdString(),
                now
            });
            break;

        case SimStep::Type::EmitToolCall: {
            aiflow::core::ToolCall call = step.toolCall;
            call.timestampMs = now;
            m_pendingCallId = call.id;
            m_decisionHandled = false;
            emit eventGenerated(aiflow::core::events::ToolCallDetected{
                m_activeSessionId,
                call
            });
            break;
        }

        case SimStep::Type::WaitForDecision:
            if (m_decisionHandled) {
                emit logMessage(QString("Tool call %1 auto-passed (Monitor Only) - proceeding without pause.").arg(QString::fromStdString(m_pendingCallId)));
                m_currentStepIndex++;
                if (m_currentStepIndex < m_scenario.size()) {
                    m_timer->start(m_scenario[m_currentStepIndex].delayMs);
                } else {
                    emit simulationFinished();
                }
                return;
            }
            m_waitingForDecision = true;
            emit logMessage(QString("Breakpoint hit! Waiting for user action on %1...").arg(QString::fromStdString(m_pendingCallId)));
            return; // Wait for notifyToolDecision

        case SimStep::Type::ToolOutput:
            emit eventGenerated(aiflow::core::events::ToolResultReceived{
                m_activeSessionId,
                m_pendingCallId,
                step.text.toStdString(),
                now
            });
            break;

        case SimStep::Type::FinalResponse:
            emit eventGenerated(aiflow::core::events::ContentDelta{
                m_activeSessionId,
                step.text.toStdString(),
                now
            });
            break;

        case SimStep::Type::FinishSession:
            emit eventGenerated(aiflow::core::events::SessionCompleted{
                m_activeSessionId,
                1420,
                388,
                now
            });
            emit logMessage("Simulated session completed.");
            emit simulationFinished();
            return;
    }

    m_currentStepIndex++;
    if (m_currentStepIndex < m_scenario.size()) {
        m_timer->start(m_scenario[m_currentStepIndex].delayMs);
    } else {
        emit simulationFinished();
    }
}

} // namespace aiflow::shell
