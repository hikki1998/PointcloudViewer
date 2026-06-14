#include "gui/BackstageApplicationSettingsWidget.h"

#include <QCheckBox>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>

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
    if (ui_->captureGroup != nullptr) {
        ui_->captureGroup->setObjectName(QStringLiteral("backstageCaptureGroup"));
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

QGroupBox* BackstageApplicationSettingsWidget::captureGroup() const
{
    return ui_ != nullptr ? ui_->captureGroup : nullptr;
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

QLineEdit* BackstageApplicationSettingsWidget::webPanelUrlLineEdit() const
{
    return ui_ != nullptr ? ui_->webPanelUrlLineEdit : nullptr;
}

QLineEdit* BackstageApplicationSettingsWidget::captureSaveDirectoryLineEdit() const
{
    return ui_ != nullptr ? ui_->captureSaveDirectoryLineEdit : nullptr;
}

QPushButton* BackstageApplicationSettingsWidget::captureBrowseButton() const
{
    return ui_ != nullptr ? ui_->captureBrowseButton : nullptr;
}

QCheckBox* BackstageApplicationSettingsWidget::captureAutoSaveCheckBox() const
{
    return ui_ != nullptr ? ui_->captureAutoSaveCheckBox : nullptr;
}

QLabel* BackstageApplicationSettingsWidget::captureShortcutHintLabel() const
{
    return ui_ != nullptr ? ui_->captureShortcutHintLabel : nullptr;
}
