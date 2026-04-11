#include "gui/IssueController.h"

#include <QAction>
#include <QComboBox>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QTableWidget>

IssueController::IssueController(
    QAction* startIssueMarkAction,
    QAction* cancelIssueToolAction,
    QAction* focusIssueAction,
    QAction* removeIssueAction,
    QAction* clearIssuesAction,
    QAction* exportIssuesCsvAction,
    QAction* exportInspectionReportAction,
    QTableWidget* issueTableWidget,
    QLineEdit* issueTitleEdit,
    QComboBox* issueCategoryComboBox,
    QComboBox* issueSeverityComboBox,
    QComboBox* issueStatusComboBox,
    QComboBox* issueRelatedTowerComboBox,
    QLineEdit* issueImagePathEdit,
    QPlainTextEdit* issueDescriptionEdit,
    VoidCallback beginIssueMarking,
    VoidCallback cancelIssueTool,
    VoidCallback focusSelectedIssue,
    VoidCallback removeSelectedIssue,
    VoidCallback clearAllIssues,
    VoidCallback exportIssuesCsv,
    VoidCallback exportInspectionReport,
    IntCallback issueSelectionChanged,
    VoidCallback commitIssueDetails,
    QObject* parent)
    : QObject(parent)
{
    auto connectAction = [this](QAction* action, const VoidCallback& callback) {
        if (action == nullptr) {
            return;
        }
        connect(action, &QAction::triggered, this, [callback]() {
            if (callback) {
                callback();
            }
        });
    };

    connectAction(startIssueMarkAction, beginIssueMarking);
    connectAction(cancelIssueToolAction, cancelIssueTool);
    connectAction(focusIssueAction, focusSelectedIssue);
    connectAction(removeIssueAction, removeSelectedIssue);
    connectAction(clearIssuesAction, clearAllIssues);
    connectAction(exportIssuesCsvAction, exportIssuesCsv);
    connectAction(exportInspectionReportAction, exportInspectionReport);

    if (issueTableWidget != nullptr) {
        connect(issueTableWidget, &QTableWidget::currentCellChanged, this, [issueSelectionChanged](int currentRow, int, int, int) {
            if (issueSelectionChanged) {
                issueSelectionChanged(currentRow);
            }
        });
    }

    if (issueTitleEdit != nullptr) {
        connect(issueTitleEdit, &QLineEdit::editingFinished, this, [commitIssueDetails]() {
            if (commitIssueDetails) {
                commitIssueDetails();
            }
        });
    }
    if (issueCategoryComboBox != nullptr) {
        connect(issueCategoryComboBox, &QComboBox::editTextChanged, this, [commitIssueDetails](const QString&) {
            if (commitIssueDetails) {
                commitIssueDetails();
            }
        });
    }
    if (issueSeverityComboBox != nullptr) {
        connect(issueSeverityComboBox, qOverload<int>(&QComboBox::currentIndexChanged), this, [commitIssueDetails](int) {
            if (commitIssueDetails) {
                commitIssueDetails();
            }
        });
    }
    if (issueStatusComboBox != nullptr) {
        connect(issueStatusComboBox, qOverload<int>(&QComboBox::currentIndexChanged), this, [commitIssueDetails](int) {
            if (commitIssueDetails) {
                commitIssueDetails();
            }
        });
    }
    if (issueRelatedTowerComboBox != nullptr) {
        connect(issueRelatedTowerComboBox, qOverload<int>(&QComboBox::currentIndexChanged), this, [commitIssueDetails](int) {
            if (commitIssueDetails) {
                commitIssueDetails();
            }
        });
    }
    if (issueImagePathEdit != nullptr) {
        connect(issueImagePathEdit, &QLineEdit::editingFinished, this, [commitIssueDetails]() {
            if (commitIssueDetails) {
                commitIssueDetails();
            }
        });
    }
    if (issueDescriptionEdit != nullptr) {
        connect(issueDescriptionEdit, &QPlainTextEdit::textChanged, this, [commitIssueDetails]() {
            if (commitIssueDetails) {
                commitIssueDetails();
            }
        });
    }
}