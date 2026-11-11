#include "ui/tool_approval_dialog.hpp"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QDateTime>

namespace aiflow::ui {

ToolApprovalDialog::ToolApprovalDialog(const aiflow::core::ToolCall& call, QWidget* parent)
    : QDialog(parent), m_call(call) {
    setWindowTitle(QString("Tool Interception: %1").arg(QString::fromStdString(call.toolName)));
    resize(640, 500);

    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(12);

    // Call Details Box
    auto* infoGroup = new QGroupBox("Intercepted Call Signature", this);
    auto* infoLayout = new QVBoxLayout(infoGroup);

    m_infoLabel = new QLabel(this);
    m_infoLabel->setTextFormat(Qt::RichText);
    QString infoText = QString("<b>Tool:</b> <code style='font-size:12pt;color:#01CDFE;'>%1</code><br>"
                               "<b>Call ID:</b> %2<br>"
                               "<b>Detected At:</b> %3")
                           .arg(QString::fromStdString(call.toolName))
                           .arg(QString::fromStdString(call.id))
                           .arg(QDateTime::fromMSecsSinceEpoch(call.timestampMs).toString("yyyy-MM-dd hh:mm:ss.zzz"));
    m_infoLabel->setText(infoText);
    infoLayout->addWidget(m_infoLabel);

    if (call.isDangerous) {
        m_dangerWarning = new QLabel(this);
        m_dangerWarning->setStyleSheet("color: #FF2A6D; font-weight: bold; background-color: rgba(255, 42, 109, 0.15); padding: 8px; border-radius: 4px;");
        m_dangerWarning->setText("⚠️ DANGEROUS TOOL DETECTED: This invocation may alter files, execute shell commands, or perform destructive actions!");
        infoLayout->addWidget(m_dangerWarning);
    }

    mainLayout->addWidget(infoGroup);

    // Editable Arguments
    auto* argsGroup = new QGroupBox("JSON Arguments (Inspect or Modify Before Execution)", this);
    auto* argsLayout = new QVBoxLayout(argsGroup);

    m_argumentsEditor = new QPlainTextEdit(this);
    m_argumentsEditor->setPlainText(QString::fromStdString(call.rawArguments));
    QFont monoFont("Monospace", 10);
    m_argumentsEditor->setFont(monoFont);
    argsLayout->addWidget(m_argumentsEditor);
    mainLayout->addWidget(argsGroup);

    // Rejection Reason Input
    auto* rejectGroup = new QGroupBox("Rejection Feedback (Delivered to Agent on Block)", this);
    auto* rejectLayout = new QVBoxLayout(rejectGroup);
    m_reasonInput = new QLineEdit(this);
    m_reasonInput->setPlaceholderText("e.g. Blocked: Command execution disallowed by human administrator.");
    rejectLayout->addWidget(m_reasonInput);
    mainLayout->addWidget(rejectGroup);

    // Action Buttons
    auto* btnLayout = new QHBoxLayout();

    m_btnReject = new QPushButton("⛔ Reject & Block Call", this);
    m_btnReject->setObjectName("DangerBtn");
    connect(m_btnReject, &QPushButton::clicked, this, &ToolApprovalDialog::onRejectClicked);
    btnLayout->addWidget(m_btnReject);

    btnLayout->addStretch();

    m_btnModify = new QPushButton("✏️ Modify & Execute", this);
    connect(m_btnModify, &QPushButton::clicked, this, &ToolApprovalDialog::onModifyClicked);
    btnLayout->addWidget(m_btnModify);

    m_btnApprove = new QPushButton("✅ Approve Call", this);
    m_btnApprove->setObjectName("ApproveBtn");
    connect(m_btnApprove, &QPushButton::clicked, this, &ToolApprovalDialog::onApproveClicked);
    btnLayout->addWidget(m_btnApprove);

    mainLayout->addLayout(btnLayout);
}

void ToolApprovalDialog::onApproveClicked() {
    m_decision = aiflow::core::InterceptionStatus::Approved;
    accept();
}

void ToolApprovalDialog::onModifyClicked() {
    m_decision = aiflow::core::InterceptionStatus::Modified;
    accept();
}

void ToolApprovalDialog::onRejectClicked() {
    m_decision = aiflow::core::InterceptionStatus::Rejected;
    accept();
}

std::string ToolApprovalDialog::modifiedArguments() const {
    return m_argumentsEditor->toPlainText().toStdString();
}

std::string ToolApprovalDialog::rejectionReason() const {
    QString reason = m_reasonInput->text().trimmed();
    if (reason.isEmpty()) {
        reason = "Operation denied by human supervisor.";
    }
    return reason.toStdString();
}

} // namespace aiflow::ui
