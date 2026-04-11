#include "gui/BackstageOpenProjectWidget.h"

#include <QAbstractItemView>
#include <QGroupBox>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>

#include "ui_BackstageOpenProjectWidget.h"

BackstageOpenProjectWidget::BackstageOpenProjectWidget(QWidget* parent)
    : QWidget(parent)
    , ui_(new Ui::BackstageOpenProjectWidget())
{
    ui_->setupUi(this);

    if (ui_->recentProjectsGroup != nullptr) {
        ui_->recentProjectsGroup->setObjectName(QStringLiteral("backstageRecentProjectsGroup"));
    }
    if (ui_->projectFileGroup != nullptr) {
        ui_->projectFileGroup->setObjectName(QStringLiteral("backstageProjectFileGroup"));
    }

    if (ui_->recentProjectsListWidget != nullptr) {
        ui_->recentProjectsListWidget->setSelectionMode(QAbstractItemView::SingleSelection);
        ui_->recentProjectsListWidget->setMinimumHeight(220);
    }
    if (ui_->projectPathLineEdit != nullptr) {
        ui_->projectPathLineEdit->setClearButtonEnabled(true);
    }
    if (ui_->openButton != nullptr) {
        ui_->openButton->setEnabled(false);
    }
}

BackstageOpenProjectWidget::~BackstageOpenProjectWidget()
{
    delete ui_;
}

void BackstageOpenProjectWidget::retranslateUi()
{
    if (ui_ != nullptr) {
        ui_->retranslateUi(this);
    }
    if (ui_ != nullptr && ui_->projectPathLineEdit != nullptr) {
        ui_->projectPathLineEdit->setPlaceholderText(tr("Project file path"));
    }
}

QGroupBox* BackstageOpenProjectWidget::recentProjectsGroup() const
{
    return ui_ != nullptr ? ui_->recentProjectsGroup : nullptr;
}

QGroupBox* BackstageOpenProjectWidget::projectFileGroup() const
{
    return ui_ != nullptr ? ui_->projectFileGroup : nullptr;
}

QListWidget* BackstageOpenProjectWidget::recentProjectsListWidget() const
{
    return ui_ != nullptr ? ui_->recentProjectsListWidget : nullptr;
}

QLineEdit* BackstageOpenProjectWidget::projectPathLineEdit() const
{
    return ui_ != nullptr ? ui_->projectPathLineEdit : nullptr;
}

QPushButton* BackstageOpenProjectWidget::browseButton() const
{
    return ui_ != nullptr ? ui_->browseButton : nullptr;
}

QPushButton* BackstageOpenProjectWidget::openButton() const
{
    return ui_ != nullptr ? ui_->openButton : nullptr;
}
