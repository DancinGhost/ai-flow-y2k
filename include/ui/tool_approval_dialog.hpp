#pragma once

#include <QDialog>
#include <QPlainTextEdit>
#include <QLineEdit>
#include <QLabel>
#include <QPushButton>
#include "core/models.hpp"

namespace aiflow::ui {

class ToolApprovalDialog : public QDialog {
    Q_OBJECT

public:
    explicit ToolApprovalDialog(const aiflow::core::ToolCall& call, QWidget* parent = nullptr);
    ~ToolApprovalDialog() override = default;

    aiflow::core::InterceptionStatus decision() const { return m_decision; }
    std::string modifiedArguments() const;
    std::string rejectionReason() const;

private slots:
    void onApproveClicked();
    void onModifyClicked();
    void onRejectClicked();

private:
    aiflow::core::ToolCall m_call;
    aiflow::core::InterceptionStatus m_decision = aiflow::core::InterceptionStatus::PendingApproval;

    QLabel* m_infoLabel = nullptr;
    QLabel* m_dangerWarning = nullptr;
    QPlainTextEdit* m_argumentsEditor = nullptr;
    QLineEdit* m_reasonInput = nullptr;

    QPushButton* m_btnApprove = nullptr;
    QPushButton* m_btnModify = nullptr;
    QPushButton* m_btnReject = nullptr;
};

} // namespace aiflow::ui
