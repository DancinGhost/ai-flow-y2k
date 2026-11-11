#pragma once

#include <QMainWindow>
#include <QComboBox>
#include <QPushButton>
#include <QTableWidget>
#include <QTextEdit>
#include <QPlainTextEdit>
#include <QLabel>
#include <QSplitter>
#include <QListWidget>

#include "core/models.hpp"
#include "shell/proxy_server.hpp"
#include "shell/simulator.hpp"
#include "ui/visualizer_widget.hpp"

namespace aiflow::ui {

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override = default;

    void startProxy(quint16 port = 8765);
    void runSimulation();
    void applyTheme(aiflow::core::ThemeType theme);

private slots:
    void onThemeChanged(int index);
    void onModeChanged(int index);
    void onToggleProxyClicked();
    void onRunSimulationClicked();
    void onClearTracesClicked();

    void onToolCallSelected();
    void onReviewSelectedToolClicked();
    void onApproveSelectedToolClicked();
    void onRejectSelectedToolClicked();

    void onFlowEventReceived(const aiflow::core::FlowEvent& event);
    void onProxyStatusChanged(bool running, quint16 port);
    void onLogMessage(const QString& message);

private:
    void setupUi();
    void refreshUiFromState();
    void updateToolCallsTable();
    void updateThinkingView();

    // Functional Core State
    aiflow::core::AppState m_appState;

    // Imperative Shell Components
    aiflow::shell::ProxyServer* m_proxyServer = nullptr;
    aiflow::shell::AgentSimulator* m_simulator = nullptr;

    // UI Widgets
    VisualizerWidget* m_visualizer = nullptr;
    QComboBox* m_themeCombo = nullptr;
    QComboBox* m_modeCombo = nullptr;
    QPushButton* m_btnToggleProxy = nullptr;
    QPushButton* m_btnRunSim = nullptr;
    QPushButton* m_btnClear = nullptr;

    // Center UI
    QListWidget* m_sessionsList = nullptr;
    QPlainTextEdit* m_thinkingStreamEdit = nullptr;
    QTextEdit* m_agentResponseEdit = nullptr;
    QLabel* m_thinkingStatusLabel = nullptr;

    QTableWidget* m_toolsTable = nullptr;
    QPlainTextEdit* m_selectedToolDetails = nullptr;
    QPushButton* m_btnReview = nullptr;
    QPushButton* m_btnApprove = nullptr;
    QPushButton* m_btnReject = nullptr;

    QLabel* m_statusLabel = nullptr;
    QLabel* m_metricsLabel = nullptr;
};

} // namespace aiflow::ui
