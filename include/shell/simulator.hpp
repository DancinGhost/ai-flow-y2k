#pragma once

#include <QObject>
#include <QTimer>
#include <vector>
#include "core/models.hpp"

namespace aiflow::shell {

class AgentSimulator : public QObject {
    Q_OBJECT

public:
    explicit AgentSimulator(QObject* parent = nullptr);
    ~AgentSimulator() override;

    void startSimulation();
    void stopSimulation();
    bool isRunning() const;

    // Resumes simulation when a tool call decision was made
    void notifyToolDecision(const std::string& callId, aiflow::core::InterceptionStatus status);

signals:
    void eventGenerated(const aiflow::core::FlowEvent& event);
    void simulationFinished();
    void logMessage(const QString& message);

private slots:
    void onTick();

private:
    struct SimStep {
        enum class Type {
            StartSession,
            ThinkingChunk,
            EndThinking,
            AssistantChunk,
            EmitToolCall,
            WaitForDecision,
            ToolOutput,
            FinalResponse,
            FinishSession
        } type;
        QString text;
        aiflow::core::ToolCall toolCall;
        int delayMs = 300;
    };

    void buildScenario();

    QTimer* m_timer = nullptr;
    std::vector<SimStep> m_scenario;
    size_t m_currentStepIndex = 0;
    std::string m_activeSessionId;
    std::string m_pendingCallId;
    bool m_waitingForDecision = false;
};

} // namespace aiflow::shell
