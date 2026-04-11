#pragma once

#include <QGroupBox>

namespace Ui
{
class NavigationSettingsWidget;
}

class QCheckBox;
class QFormLayout;
class QLabel;
class QSlider;
class QWidget;

class NavigationSettingsWidget final : public QGroupBox
{
    Q_OBJECT

public:
    explicit NavigationSettingsWidget(QWidget* parent = nullptr);
    ~NavigationSettingsWidget() override;

    void retranslateUi();

    QLabel* tipsLabel() const;
    QFormLayout* toggleLayout() const;
    QCheckBox* invertOrbitCheckBox() const;
    QCheckBox* invertPanCheckBox() const;
    QCheckBox* invertWheelCheckBox() const;
    QWidget* wheelZoomSensitivityControl() const;
    QSlider* wheelZoomSensitivitySlider() const;
    QLabel* wheelZoomSensitivityValueLabel() const;

private:
    Ui::NavigationSettingsWidget* ui_ = nullptr;
};
