#pragma once

#include <QObject>

#include <functional>

class QAction;
class QComboBox;
class QLineEdit;
class QPlainTextEdit;
class QTableWidget;

class IssueController final : public QObject
{
    Q_OBJECT

public:
    using VoidCallback = std::function<void()>;
    using IntCallback = std::function<void(int)>;

    IssueController(
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
        QObject* parent = nullptr);
};