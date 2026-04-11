#pragma once

#include <QWidget>

namespace Ui
{
class IssueEditorWidget;
}

class QComboBox;
class QFormLayout;
class QGroupBox;
class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QTableWidget;
class QToolBar;
class QToolButton;

class IssueEditorWidget final : public QWidget
{
    Q_OBJECT

public:
    explicit IssueEditorWidget(QWidget* parent = nullptr);
    ~IssueEditorWidget() override;

    void retranslateUi();

    QToolBar* toolBar() const;
    QToolButton* menuButton() const;
    QLabel* issueCountLabel() const;
    QLabel* issueToolStatusLabel() const;
    QTableWidget* issueTable() const;
    QGroupBox* issueDetailsGroupBox() const;
    QFormLayout* issueDetailsLayout() const;
    QLineEdit* issueTitleEdit() const;
    QComboBox* issueCategoryComboBox() const;
    QComboBox* issueSeverityComboBox() const;
    QComboBox* issueStatusComboBox() const;
    QComboBox* issueRelatedTowerComboBox() const;
    QLineEdit* issueImagePathEdit() const;
    QLabel* issueLocationValueLabel() const;
    QLabel* issueCreatedAtValueLabel() const;
    QPlainTextEdit* issueDescriptionEdit() const;

private:
    Ui::IssueEditorWidget* ui_ = nullptr;
};
