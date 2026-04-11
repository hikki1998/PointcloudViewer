#include "gui/IssueEditorWidget.h"

#include <QAbstractItemView>
#include <QComboBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QSize>
#include <QTableWidget>
#include <QToolBar>
#include <QToolButton>

#include "ui_IssueEditorWidget.h"

IssueEditorWidget::IssueEditorWidget(QWidget* parent)
    : QWidget(parent)
    , ui_(new Ui::IssueEditorWidget())
{
    ui_->setupUi(this);

    if (ui_->issueToolBar != nullptr) {
        ui_->issueToolBar->setIconSize(QSize(16, 16));
        ui_->issueToolBar->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
        ui_->issueToolBar->setMovable(false);
        ui_->issueToolBar->setFloatable(false);
    }
    if (ui_->issueMenuButton != nullptr) {
        ui_->issueMenuButton->setPopupMode(QToolButton::InstantPopup);
        ui_->issueMenuButton->setToolButtonStyle(Qt::ToolButtonTextOnly);
    }

    if (ui_->issueCountValueLabel != nullptr) {
        ui_->issueCountValueLabel->setWordWrap(true);
    }
    if (ui_->issueToolStatusLabel != nullptr) {
        ui_->issueToolStatusLabel->setWordWrap(true);
    }

    if (ui_->issueTableWidget != nullptr) {
        ui_->issueTableWidget->setColumnCount(6);
        ui_->issueTableWidget->setSelectionMode(QAbstractItemView::SingleSelection);
        ui_->issueTableWidget->setSelectionBehavior(QAbstractItemView::SelectRows);
        ui_->issueTableWidget->setAlternatingRowColors(true);
        ui_->issueTableWidget->setEditTriggers(QAbstractItemView::NoEditTriggers);
        ui_->issueTableWidget->verticalHeader()->setVisible(false);
        ui_->issueTableWidget->horizontalHeader()->setStretchLastSection(false);
        ui_->issueTableWidget->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
        ui_->issueTableWidget->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
        ui_->issueTableWidget->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
        ui_->issueTableWidget->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
        ui_->issueTableWidget->horizontalHeader()->setSectionResizeMode(4, QHeaderView::ResizeToContents);
        ui_->issueTableWidget->horizontalHeader()->setSectionResizeMode(5, QHeaderView::ResizeToContents);
    }

    if (ui_->issueDetailsLayout != nullptr) {
        ui_->issueDetailsLayout->setLabelAlignment(Qt::AlignLeft | Qt::AlignTop);
        ui_->issueDetailsLayout->setFormAlignment(Qt::AlignTop);
    }
    if (ui_->issueCategoryComboBox != nullptr) {
        ui_->issueCategoryComboBox->setEditable(true);
    }
    if (ui_->issueDescriptionEdit != nullptr) {
        ui_->issueDescriptionEdit->setMaximumHeight(110);
    }
    if (ui_->issueLocationValueLabel != nullptr) {
        ui_->issueLocationValueLabel->setWordWrap(true);
    }
    if (ui_->issueCreatedAtValueLabel != nullptr) {
        ui_->issueCreatedAtValueLabel->setWordWrap(true);
    }
}

IssueEditorWidget::~IssueEditorWidget()
{
    delete ui_;
}

void IssueEditorWidget::retranslateUi()
{
    if (ui_ != nullptr) {
        ui_->retranslateUi(this);
    }
}

QToolBar* IssueEditorWidget::toolBar() const { return ui_ != nullptr ? ui_->issueToolBar : nullptr; }
QToolButton* IssueEditorWidget::menuButton() const { return ui_ != nullptr ? ui_->issueMenuButton : nullptr; }
QLabel* IssueEditorWidget::issueCountLabel() const { return ui_ != nullptr ? ui_->issueCountValueLabel : nullptr; }
QLabel* IssueEditorWidget::issueToolStatusLabel() const { return ui_ != nullptr ? ui_->issueToolStatusLabel : nullptr; }
QTableWidget* IssueEditorWidget::issueTable() const { return ui_ != nullptr ? ui_->issueTableWidget : nullptr; }
QGroupBox* IssueEditorWidget::issueDetailsGroupBox() const { return ui_ != nullptr ? ui_->issueDetailsGroupBox : nullptr; }
QFormLayout* IssueEditorWidget::issueDetailsLayout() const { return ui_ != nullptr ? ui_->issueDetailsLayout : nullptr; }
QLineEdit* IssueEditorWidget::issueTitleEdit() const { return ui_ != nullptr ? ui_->issueTitleEdit : nullptr; }
QComboBox* IssueEditorWidget::issueCategoryComboBox() const { return ui_ != nullptr ? ui_->issueCategoryComboBox : nullptr; }
QComboBox* IssueEditorWidget::issueSeverityComboBox() const { return ui_ != nullptr ? ui_->issueSeverityComboBox : nullptr; }
QComboBox* IssueEditorWidget::issueStatusComboBox() const { return ui_ != nullptr ? ui_->issueStatusComboBox : nullptr; }
QComboBox* IssueEditorWidget::issueRelatedTowerComboBox() const { return ui_ != nullptr ? ui_->issueRelatedTowerComboBox : nullptr; }
QLineEdit* IssueEditorWidget::issueImagePathEdit() const { return ui_ != nullptr ? ui_->issueImagePathEdit : nullptr; }
QLabel* IssueEditorWidget::issueLocationValueLabel() const { return ui_ != nullptr ? ui_->issueLocationValueLabel : nullptr; }
QLabel* IssueEditorWidget::issueCreatedAtValueLabel() const { return ui_ != nullptr ? ui_->issueCreatedAtValueLabel : nullptr; }
QPlainTextEdit* IssueEditorWidget::issueDescriptionEdit() const { return ui_ != nullptr ? ui_->issueDescriptionEdit : nullptr; }
