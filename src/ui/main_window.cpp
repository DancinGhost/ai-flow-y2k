#include "ui/main_window.hpp"
#include "core/flow_reducer.hpp"
#include "core/theme_engine.hpp"
#include "ui/tool_approval_dialog.hpp"

#include <QApplication>
#include <QScrollBar>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QHeaderView>
#include <QDateTime>
#include <QMessageBox>
#include <QStatusBar>

namespace aiflow::ui {

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent),
      m_proxyServer(new aiflow::shell::ProxyServer(this)),
      m_simulator(new aiflow::shell::AgentSimulator(this)) {
    setupUi();

    // Connect Proxy Server
    connect(m_proxyServer, &aiflow::shell::ProxyServer::eventGenerated,
            this, &MainWindow::onFlowEventReceived);
    connect(m_proxyServer, &aiflow::shell::ProxyServer::statusChanged,
            this, &MainWindow::onProxyStatusChanged);
    connect(m_proxyServer, &aiflow::shell::ProxyServer::logMessage,
            this, &MainWindow::onLogMessage);

    // Connect Simulator
    connect(m_simulator, &aiflow::shell::AgentSimulator::eventGenerated,
            this, &MainWindow::onFlowEventReceived);
    connect(m_simulator, &aiflow::shell::AgentSimulator::logMessage,
            this, &MainWindow::onLogMessage);

    // Default Theme: Vaporwave
    applyTheme(aiflow::core::ThemeType::Vaporwave);
}

void MainWindow::startProxy(quint16 port) {
    if (m_proxyServer) {
        m_proxyServer->startServer(port);
    }
}

void MainWindow::runSimulation() {
    if (m_simulator) {
        m_simulator->startSimulation();
    }
}

void MainWindow::setupUi() {
    setWindowTitle("ai-flow-y2k // AI Agent Tool Interceptor & Thinking Monitor");
    resize(1200, 850);

    auto* centralWidget = new QWidget(this);
    centralWidget->setObjectName("CentralWidget");
    setCentralWidget(centralWidget);

    auto* mainLayout = new QVBoxLayout(centralWidget);
    mainLayout->setContentsMargins(10, 10, 10, 10);
    mainLayout->setSpacing(8);

    // 1. Procedural Theme Header Visualizer
    m_visualizer = new VisualizerWidget(this);
    mainLayout->addWidget(m_visualizer);

    // 2. Control Toolbar
    auto* toolbarLayout = new QHBoxLayout();
    toolbarLayout->setSpacing(10);

    // Theme selector
    auto* themeLabel = new QLabel("🎨 Style:", this);
    m_themeCombo = new QComboBox(this);
    m_themeCombo->addItem("🌸 Vaporwave Neon '95", static_cast<int>(aiflow::core::ThemeType::Vaporwave));
    m_themeCombo->addItem("🟢 Digital Rain '99", static_cast<int>(aiflow::core::ThemeType::DigitalRain));
    m_themeCombo->addItem("🫧 Frutiger Aero '04", static_cast<int>(aiflow::core::ThemeType::FrutigerAero));
    connect(m_themeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &MainWindow::onThemeChanged);
    toolbarLayout->addWidget(themeLabel);
    toolbarLayout->addWidget(m_themeCombo);

    // Interception Mode selector
    auto* modeLabel = new QLabel("🛡️ Gatekeeper:", this);
    m_modeCombo = new QComboBox(this);
    m_modeCombo->addItem("Intercept Dangerous Only", static_cast<int>(aiflow::core::InterceptionMode::InterceptDangerousOnly));
    m_modeCombo->addItem("Intercept All Tools", static_cast<int>(aiflow::core::InterceptionMode::InterceptAllTools));
    m_modeCombo->addItem("Monitor Only (Pass-through)", static_cast<int>(aiflow::core::InterceptionMode::MonitorOnly));
    connect(m_modeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &MainWindow::onModeChanged);
    toolbarLayout->addWidget(modeLabel);
    toolbarLayout->addWidget(m_modeCombo);

    toolbarLayout->addSpacing(15);

    // Proxy Toggle Button
    m_btnToggleProxy = new QPushButton("🚀 Start Local Proxy (Port 8765)", this);
    connect(m_btnToggleProxy, &QPushButton::clicked, this, &MainWindow::onToggleProxyClicked);
    toolbarLayout->addWidget(m_btnToggleProxy);

    // Simulator Run Button
    m_btnRunSim = new QPushButton("▶ Simulate Agent Workflow", this);
    connect(m_btnRunSim, &QPushButton::clicked, this, &MainWindow::onRunSimulationClicked);
    toolbarLayout->addWidget(m_btnRunSim);

    // Clear Button
    m_btnClear = new QPushButton("🗑 Clear Traces", this);
    connect(m_btnClear, &QPushButton::clicked, this, &MainWindow::onClearTracesClicked);
    toolbarLayout->addWidget(m_btnClear);

    toolbarLayout->addStretch();
    mainLayout->addLayout(toolbarLayout);

    // 3. Main Workspace Splitter
    auto* hSplitter = new QSplitter(Qt::Horizontal, this);

    // Left Pane: Active Sessions & Metrics
    auto* leftContainer = new QWidget(this);
    auto* leftLayout = new QVBoxLayout(leftContainer);
    leftLayout->setContentsMargins(0, 0, 0, 0);

    auto* sessionsGroup = new QGroupBox("Sessions & Wire Interceptions", leftContainer);
    auto* sessionsGroupLayout = new QVBoxLayout(sessionsGroup);

    m_sessionsList = new QListWidget(sessionsGroup);
    sessionsGroupLayout->addWidget(m_sessionsList);

    m_metricsLabel = new QLabel(sessionsGroup);
    m_metricsLabel->setTextFormat(Qt::RichText);
    m_metricsLabel->setText("<b>Sessions:</b> 0<br><b>Intercepted Calls:</b> 0<br><b>Pending Approval:</b> 0");
    sessionsGroupLayout->addWidget(m_metricsLabel);

    leftLayout->addWidget(sessionsGroup);
    leftContainer->setMinimumWidth(260);
    hSplitter->addWidget(leftContainer);

    // Right Center Pane: Thinking Monitor + Tool Interception Gatekeeper
    auto* rightContainer = new QWidget(this);
    auto* rightLayout = new QVBoxLayout(rightContainer);
    rightLayout->setContentsMargins(0, 0, 0, 0);

    auto* vSplitter = new QSplitter(Qt::Vertical, rightContainer);

    // Center-Top: Agent Thinking / Reasoning Stream
    auto* thinkingGroup = new QGroupBox("Agent Reasoning Stream (<thinking> & Chain-of-Thought)", vSplitter);
    auto* thinkingLayout = new QVBoxLayout(thinkingGroup);

    auto* thinkingHeaderLayout = new QHBoxLayout();
    m_thinkingStatusLabel = new QLabel("Idle (No stream active)", thinkingGroup);
    m_thinkingStatusLabel->setStyleSheet("font-weight: bold; color: #01CDFE;");
    thinkingHeaderLayout->addWidget(m_thinkingStatusLabel);
    thinkingHeaderLayout->addStretch();
    thinkingLayout->addLayout(thinkingHeaderLayout);

    m_thinkingStreamEdit = new QPlainTextEdit(thinkingGroup);
    m_thinkingStreamEdit->setReadOnly(true);
    QFont monoFont("Monospace", 10);
    m_thinkingStreamEdit->setFont(monoFont);
    thinkingLayout->addWidget(m_thinkingStreamEdit);

    vSplitter->addWidget(thinkingGroup);

    // Center-Bottom: Tool Call Interceptor Gatekeeper
    auto* toolsGroup = new QGroupBox("Tool Call Interceptor & Human-In-The-Loop Gatekeeper", vSplitter);
    auto* toolsLayout = new QVBoxLayout(toolsGroup);

    m_toolsTable = new QTableWidget(toolsGroup);
    m_toolsTable->setColumnCount(5);
    m_toolsTable->setHorizontalHeaderLabels({"Timestamp", "Tool Name", "Status", "Risk Level", "Arguments Preview"});
    m_toolsTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_toolsTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_toolsTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    m_toolsTable->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    m_toolsTable->horizontalHeader()->setSectionResizeMode(4, QHeaderView::Stretch);
    m_toolsTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_toolsTable->setSelectionMode(QAbstractItemView::SingleSelection);
    connect(m_toolsTable, &QTableWidget::itemSelectionChanged, this, &MainWindow::onToolCallSelected);
    toolsLayout->addWidget(m_toolsTable);

    // Selected Call Inspector & Actions
    auto* inspectLayout = new QHBoxLayout();

    m_selectedToolDetails = new QPlainTextEdit(toolsGroup);
    m_selectedToolDetails->setReadOnly(true);
    m_selectedToolDetails->setFixedHeight(90);
    m_selectedToolDetails->setFont(monoFont);
    m_selectedToolDetails->setPlaceholderText("Select a tool call from the table above to view arguments, edit, or approve.");
    inspectLayout->addWidget(m_selectedToolDetails);

    auto* actionBtnLayout = new QVBoxLayout();
    m_btnReview = new QPushButton("🔍 Inspect & Edit", toolsGroup);
    connect(m_btnReview, &QPushButton::clicked, this, &MainWindow::onReviewSelectedToolClicked);
    actionBtnLayout->addWidget(m_btnReview);

    m_btnApprove = new QPushButton("✅ Approve Call", toolsGroup);
    m_btnApprove->setObjectName("ApproveBtn");
    connect(m_btnApprove, &QPushButton::clicked, this, &MainWindow::onApproveSelectedToolClicked);
    actionBtnLayout->addWidget(m_btnApprove);

    m_btnReject = new QPushButton("⛔ Reject Call", toolsGroup);
    m_btnReject->setObjectName("DangerBtn");
    connect(m_btnReject, &QPushButton::clicked, this, &MainWindow::onRejectSelectedToolClicked);
    actionBtnLayout->addWidget(m_btnReject);

    inspectLayout->addLayout(actionBtnLayout);
    toolsLayout->addLayout(inspectLayout);

    vSplitter->addWidget(toolsGroup);
    rightLayout->addWidget(vSplitter);
    hSplitter->addWidget(rightContainer);

    hSplitter->setStretchFactor(0, 1);
    hSplitter->setStretchFactor(1, 3);
    mainLayout->addWidget(hSplitter);

    // 4. Status Bar
    m_statusLabel = new QLabel("Ready // ai-flow-y2k [FCIS Architecture: Functional Core + Imperative Shell]", this);
    statusBar()->addWidget(m_statusLabel);
}

void MainWindow::applyTheme(aiflow::core::ThemeType theme) {
    m_appState.currentTheme = theme;
    m_visualizer->setTheme(theme);
    std::string qss = aiflow::core::ThemeEngine::generateStyleSheet(theme);
    if (auto* app = qobject_cast<QApplication*>(qApp)) {
        app->setStyleSheet(QString::fromStdString(qss));
    }
    m_statusLabel->setText(QString("Aesthetic: %1 // Functional Core Active").arg(QString::fromStdString(aiflow::core::ThemeEngine::getThemeTitle(theme))));
}

void MainWindow::onThemeChanged(int index) {
    auto theme = static_cast<aiflow::core::ThemeType>(m_themeCombo->itemData(index).toInt());
    applyTheme(theme);
}

void MainWindow::onModeChanged(int index) {
    m_appState.interceptionMode = static_cast<aiflow::core::InterceptionMode>(m_modeCombo->itemData(index).toInt());
    onLogMessage(QString("Gatekeeper mode switched to: %1").arg(m_modeCombo->currentText()));
}

void MainWindow::onToggleProxyClicked() {
    if (m_proxyServer->isListening()) {
        m_proxyServer->stopServer();
    } else {
        m_proxyServer->startServer(8765);
    }
}

void MainWindow::onProxyStatusChanged(bool running, quint16 port) {
    m_appState.proxyRunning = running;
    m_appState.proxyPort = port;
    if (running) {
        m_btnToggleProxy->setText(QString("🛑 Stop Proxy (Port %1)").arg(port));
        m_btnToggleProxy->setObjectName("DangerBtn");
    } else {
        m_btnToggleProxy->setText("🚀 Start Local Proxy (Port 8765)");
        m_btnToggleProxy->setObjectName("");
    }
    // Re-apply style to update button styling
    applyTheme(m_appState.currentTheme);
}

void MainWindow::onRunSimulationClicked() {
    m_simulator->startSimulation();
}

void MainWindow::onClearTracesClicked() {
    m_appState.sessions.clear();
    m_appState.activeSessionId.clear();
    m_sessionsList->clear();
    m_thinkingStreamEdit->clear();
    m_toolsTable->setRowCount(0);
    m_selectedToolDetails->clear();
    m_thinkingStatusLabel->setText("Idle (Traces cleared)");
    m_metricsLabel->setText("<b>Sessions:</b> 0<br><b>Intercepted Calls:</b> 0<br><b>Pending Approval:</b> 0");
}

void MainWindow::onFlowEventReceived(const aiflow::core::FlowEvent& event) {
    // 1. Execute Functional Core state reduction
    std::vector<aiflow::core::DomainAction> actions;
    m_appState = aiflow::core::FlowReducer::reduce(m_appState, event, actions);

    // 2. Execute Shell side-effects and actions
    for (const auto& action : actions) {
        m_proxyServer->handleAction(action);

        std::visit([&](const auto& act) {
            using T = std::decay_t<decltype(act)>;
            if constexpr (std::is_same_v<T, aiflow::core::actions::PauseResponseStream>) {
                // Breakpoint hit! Locate the pending tool call and prompt user
                for (const auto& sess : m_appState.sessions) {
                    if (sess.sessionId == act.sessionId) {
                        for (const auto& call : sess.interceptedCalls) {
                            if (call.id == act.callId && call.status == aiflow::core::InterceptionStatus::PendingApproval) {
                                ToolApprovalDialog dlg(call, this);
                                if (dlg.exec() == QDialog::Accepted) {
                                    auto decision = dlg.decision();
                                    std::string modifiedArgs = dlg.modifiedArguments();
                                    std::string reason = dlg.rejectionReason();

                                    // Emit decision event back into the loop
                                    onFlowEventReceived(aiflow::core::events::ToolCallDecisionMade{
                                        act.sessionId,
                                        act.callId,
                                        decision,
                                        modifiedArgs,
                                        reason
                                    });

                                    // Notify simulator if running
                                    m_simulator->notifyToolDecision(act.callId, decision);
                                }
                                break;
                            }
                        }
                        break;
                    }
                }
            }
        }, action);
    }

    // 3. Update UI Presentation
    refreshUiFromState();
}

void MainWindow::refreshUiFromState() {
    // Update Sessions List
    m_sessionsList->clear();
    int pendingCount = 0;
    int totalCalls = 0;

    for (const auto& sess : m_appState.sessions) {
        QString itemText = QString("%1 [%2] - %3 calls")
                               .arg(QString::fromStdString(sess.agentName))
                               .arg(QString::fromStdString(sess.modelName))
                               .arg(sess.interceptedCalls.size());
        if (sess.isLive) itemText += " (STREAMING)";
        m_sessionsList->addItem(itemText);

        totalCalls += sess.interceptedCalls.size();
        for (const auto& c : sess.interceptedCalls) {
            if (c.status == aiflow::core::InterceptionStatus::PendingApproval) {
                pendingCount++;
            }
        }
    }

    m_metricsLabel->setText(QString("<b>Sessions:</b> %1<br><b>Intercepted Calls:</b> %2<br><b>Pending Approval:</b> %3")
                               .arg(m_appState.sessions.size())
                               .arg(totalCalls)
                               .arg(pendingCount));

    updateThinkingView();
    updateToolCallsTable();
}

void MainWindow::updateThinkingView() {
    if (m_appState.sessions.empty()) return;

    const auto& activeSess = m_appState.sessions.back();
    QString fullThinking;

    for (const auto& block : activeSess.thinkingHistory) {
        fullThinking += QString::fromStdString(block.content);
    }

    m_thinkingStreamEdit->setPlainText(fullThinking);
    m_thinkingStreamEdit->verticalScrollBar()->setValue(m_thinkingStreamEdit->verticalScrollBar()->maximum());

    if (!activeSess.thinkingHistory.empty() && activeSess.thinkingHistory.back().isStreaming) {
        m_thinkingStatusLabel->setText("🧠 LIVE STREAMING AGENT REASONING...");
        m_thinkingStatusLabel->setStyleSheet("font-weight: bold; color: #FF007F;");
    } else {
        m_thinkingStatusLabel->setText("✅ REASONING FINISHED / EXECUTION PHASE");
        m_thinkingStatusLabel->setStyleSheet("font-weight: bold; color: #05FFA1;");
    }
}

void MainWindow::updateToolCallsTable() {
    if (m_appState.sessions.empty()) return;

    const auto& activeSess = m_appState.sessions.back();
    m_toolsTable->setRowCount(activeSess.interceptedCalls.size());

    for (int i = 0; i < (int)activeSess.interceptedCalls.size(); ++i) {
        const auto& call = activeSess.interceptedCalls[i];

        QString timeStr = QDateTime::fromMSecsSinceEpoch(call.timestampMs).toString("hh:mm:ss.zzz");
        auto* itemTime = new QTableWidgetItem(timeStr);
        auto* itemName = new QTableWidgetItem(QString::fromStdString(call.toolName));

        QString statusStr;
        switch (call.status) {
            case aiflow::core::InterceptionStatus::PendingApproval:
                statusStr = "🔴 PENDING APPROVAL";
                break;
            case aiflow::core::InterceptionStatus::Approved:
                statusStr = "🟢 APPROVED";
                break;
            case aiflow::core::InterceptionStatus::Modified:
                statusStr = "🟡 MODIFIED & RUN";
                break;
            case aiflow::core::InterceptionStatus::Rejected:
                statusStr = "⛔ REJECTED / BLOCKED";
                break;
            case aiflow::core::InterceptionStatus::AutoPassed:
                statusStr = "⚪ AUTO-PASSED";
                break;
            case aiflow::core::InterceptionStatus::Executing:
                statusStr = "⏳ EXECUTING";
                break;
            case aiflow::core::InterceptionStatus::Completed:
                statusStr = "✅ COMPLETED";
                break;
        }
        auto* itemStatus = new QTableWidgetItem(statusStr);

        auto* itemRisk = new QTableWidgetItem(call.isDangerous ? "⚠️ HIGH RISK" : "🛡️ LOW RISK");
        if (call.isDangerous) {
            itemRisk->setForeground(QBrush(QColor("#FF2A6D")));
        }

        auto* itemArgs = new QTableWidgetItem(QString::fromStdString(call.rawArguments));

        m_toolsTable->setItem(i, 0, itemTime);
        m_toolsTable->setItem(i, 1, itemName);
        m_toolsTable->setItem(i, 2, itemStatus);
        m_toolsTable->setItem(i, 3, itemRisk);
        m_toolsTable->setItem(i, 4, itemArgs);
    }
}

void MainWindow::onToolCallSelected() {
    int row = m_toolsTable->currentRow();
    if (m_appState.sessions.empty() || row < 0) return;

    const auto& activeSess = m_appState.sessions.back();
    if (row < (int)activeSess.interceptedCalls.size()) {
        const auto& call = activeSess.interceptedCalls[row];
        QString details = QString("Tool: %1\nID: %2\nRaw Arguments:\n%3")
                              .arg(QString::fromStdString(call.toolName))
                              .arg(QString::fromStdString(call.id))
                              .arg(QString::fromStdString(call.rawArguments));
        if (!call.modifiedArguments.empty()) {
            details += QString("\nModified Arguments:\n%1").arg(QString::fromStdString(call.modifiedArguments));
        }
        if (!call.executionResult.empty()) {
            details += QString("\nResult:\n%1").arg(QString::fromStdString(call.executionResult));
        }
        m_selectedToolDetails->setPlainText(details);
    }
}

void MainWindow::onReviewSelectedToolClicked() {
    int row = m_toolsTable->currentRow();
    if (m_appState.sessions.empty() || row < 0) return;

    const auto& activeSess = m_appState.sessions.back();
    if (row < (int)activeSess.interceptedCalls.size()) {
        const auto& call = activeSess.interceptedCalls[row];
        ToolApprovalDialog dlg(call, this);
        if (dlg.exec() == QDialog::Accepted) {
            auto decision = dlg.decision();
            std::string modifiedArgs = dlg.modifiedArguments();
            std::string reason = dlg.rejectionReason();

            onFlowEventReceived(aiflow::core::events::ToolCallDecisionMade{
                activeSess.sessionId,
                call.id,
                decision,
                modifiedArgs,
                reason
            });
            m_simulator->notifyToolDecision(call.id, decision);
        }
    }
}

void MainWindow::onApproveSelectedToolClicked() {
    int row = m_toolsTable->currentRow();
    if (m_appState.sessions.empty() || row < 0) return;

    const auto& activeSess = m_appState.sessions.back();
    if (row < (int)activeSess.interceptedCalls.size()) {
        const auto& call = activeSess.interceptedCalls[row];
        onFlowEventReceived(aiflow::core::events::ToolCallDecisionMade{
            activeSess.sessionId,
            call.id,
            aiflow::core::InterceptionStatus::Approved,
            call.rawArguments,
            ""
        });
        m_simulator->notifyToolDecision(call.id, aiflow::core::InterceptionStatus::Approved);
    }
}

void MainWindow::onRejectSelectedToolClicked() {
    int row = m_toolsTable->currentRow();
    if (m_appState.sessions.empty() || row < 0) return;

    const auto& activeSess = m_appState.sessions.back();
    if (row < (int)activeSess.interceptedCalls.size()) {
        const auto& call = activeSess.interceptedCalls[row];
        onFlowEventReceived(aiflow::core::events::ToolCallDecisionMade{
            activeSess.sessionId,
            call.id,
            aiflow::core::InterceptionStatus::Rejected,
            "",
            "Operation blocked by user"
        });
        m_simulator->notifyToolDecision(call.id, aiflow::core::InterceptionStatus::Rejected);
    }
}

void MainWindow::onLogMessage(const QString& message) {
    m_statusLabel->setText(QString("[%1] %2")
                               .arg(QDateTime::currentDateTime().toString("hh:mm:ss"))
                               .arg(message));
}

} // namespace aiflow::ui
