#pragma once

#include <QWidget>

namespace Ui
{
class BackstageApplicationSettingsWidget;
}

class QCheckBox;
class QGroupBox;
class QHBoxLayout;
class QLabel;
class QLineEdit;
class QPushButton;

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
    QGroupBox* captureGroup() const;
    QHBoxLayout* themeButtonLayout() const;
    QHBoxLayout* languageButtonLayout() const;
    QCheckBox* showLogCheckBox() const;
    QLineEdit* webPanelUrlLineEdit() const;
    QLineEdit* captureSaveDirectoryLineEdit() const;
    QPushButton* captureBrowseButton() const;
    QCheckBox* captureAutoSaveCheckBox() const;
    QLabel* captureShortcutHintLabel() const;

private:
    Ui::BackstageApplicationSettingsWidget* ui_ = nullptr;
};
