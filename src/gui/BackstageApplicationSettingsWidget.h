#pragma once

#include <QWidget>

namespace Ui
{
class BackstageApplicationSettingsWidget;
}

class QCheckBox;
class QGroupBox;
class QHBoxLayout;

class BackstageApplicationSettingsWidget final : public QWidget
{
    Q_OBJECT

public:
    explicit BackstageApplicationSettingsWidget(QWidget* parent = nullptr);
    ~BackstageApplicationSettingsWidget() override;

    void retranslateUi();

    QGroupBox* themeGroup() const;
    QGroupBox* languageGroup() const;
    QGroupBox* workspaceGroup() const;
    QHBoxLayout* themeButtonLayout() const;
    QHBoxLayout* languageButtonLayout() const;
    QCheckBox* showLogCheckBox() const;

private:
    Ui::BackstageApplicationSettingsWidget* ui_ = nullptr;
};
