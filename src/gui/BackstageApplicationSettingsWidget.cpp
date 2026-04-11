#include "gui/BackstageApplicationSettingsWidget.h"

#include <QCheckBox>
#include <QGroupBox>
#include <QHBoxLayout>

#include "ui_BackstageApplicationSettingsWidget.h"

BackstageApplicationSettingsWidget::BackstageApplicationSettingsWidget(QWidget* parent)
    : QWidget(parent)
    , ui_(new Ui::BackstageApplicationSettingsWidget())
{
    ui_->setupUi(this);

    if (ui_->themeGroup != nullptr) {
        ui_->themeGroup->setObjectName(QStringLiteral("backstageThemeGroup"));
    }
    if (ui_->languageGroup != nullptr) {
        ui_->languageGroup->setObjectName(QStringLiteral("backstageLanguageGroup"));
    }
    if (ui_->workspaceGroup != nullptr) {
        ui_->workspaceGroup->setObjectName(QStringLiteral("backstageWorkspaceGroup"));
    }
}

BackstageApplicationSettingsWidget::~BackstageApplicationSettingsWidget()
{
    delete ui_;
}

void BackstageApplicationSettingsWidget::retranslateUi()
{
    if (ui_ != nullptr) {
        ui_->retranslateUi(this);
    }
}

QGroupBox* BackstageApplicationSettingsWidget::themeGroup() const
{
    return ui_ != nullptr ? ui_->themeGroup : nullptr;
}

QGroupBox* BackstageApplicationSettingsWidget::languageGroup() const
{
    return ui_ != nullptr ? ui_->languageGroup : nullptr;
}

QGroupBox* BackstageApplicationSettingsWidget::workspaceGroup() const
{
    return ui_ != nullptr ? ui_->workspaceGroup : nullptr;
}

QHBoxLayout* BackstageApplicationSettingsWidget::themeButtonLayout() const
{
    return ui_ != nullptr ? ui_->themeButtonLayout : nullptr;
}

QHBoxLayout* BackstageApplicationSettingsWidget::languageButtonLayout() const
{
    return ui_ != nullptr ? ui_->languageButtonLayout : nullptr;
}

QCheckBox* BackstageApplicationSettingsWidget::showLogCheckBox() const
{
    return ui_ != nullptr ? ui_->showLogCheckBox : nullptr;
}
