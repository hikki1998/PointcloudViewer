#include "gui/TowerEditorWidget.h"

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

#include <algorithm>
#include <cmath>

#include "ui_TowerEditorWidget.h"

TowerEditorWidget::TowerEditorWidget(QWidget* parent)
    : QWidget(parent)
    , ui_(new Ui::TowerEditorWidget())
{
    ui_->setupUi(this);

    if (ui_->towerCountValueLabel != nullptr) {
        ui_->towerCountValueLabel->setWordWrap(true);
    }
    if (ui_->towerToolStatusLabel != nullptr) {
        ui_->towerToolStatusLabel->setWordWrap(true);
    }

    if (ui_->towerToolBar != nullptr) {
        const double towerUiScale = std::clamp(static_cast<double>(logicalDpiX()) / 96.0, 1.0, 2.0);
        const int towerIconSize = static_cast<int>(std::lround(26.0 * towerUiScale));
        const int towerButtonSize = static_cast<int>(std::lround(44.0 * towerUiScale));
        const int towerButtonPadding = static_cast<int>(std::lround(7.0 * towerUiScale));
        const int towerButtonRadius = static_cast<int>(std::lround(9.0 * towerUiScale));
        const int towerToolSpacing = static_cast<int>(std::lround(8.0 * towerUiScale));

        ui_->towerToolBar->setIconSize(QSize(towerIconSize, towerIconSize));
        ui_->towerToolBar->setToolButtonStyle(Qt::ToolButtonIconOnly);
        ui_->towerToolBar->setMovable(false);
        ui_->towerToolBar->setFloatable(false);
        ui_->towerToolBar->setStyleSheet(QStringLiteral(
            "QToolBar { spacing: %1px; }"
            "QToolButton {"
            "min-width: %2px;"
            "min-height: %2px;"
            "padding: %3px;"
            "border: 1px solid #cbd5e1;"
            "border-radius: %4px;"
            "background-color: #ffffff;"
            "}"
            "QToolButton:hover {"
            "border-color: #94a3b8;"
            "background-color: #f8fafc;"
            "}"
            "QToolButton:pressed {"
            "background-color: #e2e8f0;"
            "}"
            "QToolButton:disabled {"
            "border-color: #e2e8f0;"
            "background-color: #f8fafc;"
            "color: #94a3b8;"
            "}")
                .arg(towerToolSpacing)
                .arg(towerButtonSize)
                .arg(towerButtonPadding)
                .arg(towerButtonRadius));
    }

    if (ui_->towerToolbarHostLayout != nullptr && ui_->towerToolBar != nullptr) {
        const double towerUiScale = std::clamp(static_cast<double>(logicalDpiX()) / 96.0, 1.0, 2.0);
        const int towerToolSpacing = static_cast<int>(std::lround(8.0 * towerUiScale));
        ui_->towerToolbarHostLayout->setSpacing(towerToolSpacing + 2);
    }

    if (ui_->towerTableWidget != nullptr) {
        ui_->towerTableWidget->setColumnCount(5);
        ui_->towerTableWidget->setSelectionMode(QAbstractItemView::SingleSelection);
        ui_->towerTableWidget->setSelectionBehavior(QAbstractItemView::SelectRows);
        ui_->towerTableWidget->setAlternatingRowColors(true);
        ui_->towerTableWidget->setEditTriggers(QAbstractItemView::DoubleClicked | QAbstractItemView::EditKeyPressed);
        ui_->towerTableWidget->verticalHeader()->setVisible(false);
        ui_->towerTableWidget->horizontalHeader()->setStretchLastSection(false);
        ui_->towerTableWidget->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
        ui_->towerTableWidget->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
        ui_->towerTableWidget->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
        ui_->towerTableWidget->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
        ui_->towerTableWidget->horizontalHeader()->setSectionResizeMode(4, QHeaderView::ResizeToContents);
        ui_->towerTableWidget->setColumnHidden(2, true);
        ui_->towerTableWidget->setColumnHidden(3, true);
        ui_->towerTableWidget->setColumnHidden(4, true);
        ui_->towerTableWidget->setStyleSheet(QStringLiteral(
            "QTableWidget {"
            "background-color: #ffffff;"
            "alternate-background-color: #f8fafc;"
            "gridline-color: #e2e8f0;"
            "color: #0f172a;"
            "}"
            "QHeaderView::section {"
            "background-color: #e2e8f0;"
            "color: #0f172a;"
            "border: 1px solid #cbd5e1;"
            "padding: 4px 8px;"
            "font-weight: 600;"
            "}"
            "QHeaderView::section:hover {"
            "background-color: #dbeafe;"
            "}"
            "QHeaderView::section:pressed {"
            "background-color: #bfdbfe;"
            "}"
            "QTableCornerButton::section {"
            "background-color: #e2e8f0;"
            "border: 1px solid #cbd5e1;"
            "}"));
    }

    if (ui_->towerDetailsLayout != nullptr) {
        ui_->towerDetailsLayout->setLabelAlignment(Qt::AlignLeft | Qt::AlignTop);
        ui_->towerDetailsLayout->setFormAlignment(Qt::AlignTop);
    }
    if (ui_->towerNotesEdit != nullptr) {
        ui_->towerNotesEdit->setMaximumHeight(96);
    }
}

TowerEditorWidget::~TowerEditorWidget()
{
    delete ui_;
}

void TowerEditorWidget::retranslateUi()
{
    if (ui_ != nullptr) {
        ui_->retranslateUi(this);
    }
}

QToolBar* TowerEditorWidget::toolBar() const { return ui_ != nullptr ? ui_->towerToolBar : nullptr; }
QLabel* TowerEditorWidget::towerCountLabel() const { return ui_ != nullptr ? ui_->towerCountValueLabel : nullptr; }
QLabel* TowerEditorWidget::towerToolStatusLabel() const { return ui_ != nullptr ? ui_->towerToolStatusLabel : nullptr; }
QTableWidget* TowerEditorWidget::towerTable() const { return ui_ != nullptr ? ui_->towerTableWidget : nullptr; }
QGroupBox* TowerEditorWidget::towerDetailsGroupBox() const { return ui_ != nullptr ? ui_->towerDetailsGroupBox : nullptr; }
QFormLayout* TowerEditorWidget::towerDetailsLayout() const { return ui_ != nullptr ? ui_->towerDetailsLayout : nullptr; }
QLineEdit* TowerEditorWidget::towerCodeEdit() const { return ui_ != nullptr ? ui_->towerCodeEdit : nullptr; }
QLineEdit* TowerEditorWidget::towerLineNameEdit() const { return ui_ != nullptr ? ui_->towerLineNameEdit : nullptr; }
QLineEdit* TowerEditorWidget::towerVoltageLevelEdit() const { return ui_ != nullptr ? ui_->towerVoltageLevelEdit : nullptr; }
QComboBox* TowerEditorWidget::towerTypeComboBox() const { return ui_ != nullptr ? ui_->towerTypeComboBox : nullptr; }
QLineEdit* TowerEditorWidget::towerStructureTypeEdit() const { return ui_ != nullptr ? ui_->towerStructureTypeEdit : nullptr; }
QLineEdit* TowerEditorWidget::towerInspectionDateEdit() const { return ui_ != nullptr ? ui_->towerInspectionDateEdit : nullptr; }
QLineEdit* TowerEditorWidget::towerStatusEdit() const { return ui_ != nullptr ? ui_->towerStatusEdit : nullptr; }
QPlainTextEdit* TowerEditorWidget::towerNotesEdit() const { return ui_ != nullptr ? ui_->towerNotesEdit : nullptr; }
