#include "gui/NavigationSettingsWidget.h"

#include <QCheckBox>
#include <QFormLayout>
#include <QLabel>
#include <QSlider>
#include <QWidget>

#include "ui_NavigationSettingsWidget.h"

NavigationSettingsWidget::NavigationSettingsWidget(QWidget* parent)
    : QGroupBox(parent)
    , ui_(new Ui::NavigationSettingsWidget())
{
    ui_->setupUi(this);

    if (ui_->tipsLabel != nullptr) {
        ui_->tipsLabel->setWordWrap(true);
    }

    if (ui_->toggleLayout != nullptr) {
        ui_->toggleLayout->setLabelAlignment(Qt::AlignLeft | Qt::AlignTop);
        ui_->toggleLayout->setFormAlignment(Qt::AlignTop);
    }

    if (ui_->wheelZoomSensitivitySlider != nullptr) {
        ui_->wheelZoomSensitivitySlider->setRange(50, 200);
        ui_->wheelZoomSensitivitySlider->setSingleStep(5);
        ui_->wheelZoomSensitivitySlider->setPageStep(10);
        ui_->wheelZoomSensitivitySlider->setTickInterval(10);
        ui_->wheelZoomSensitivitySlider->setTracking(false);
    }

    if (ui_->wheelZoomSensitivityValueLabel != nullptr) {
        ui_->wheelZoomSensitivityValueLabel->setMinimumWidth(56);
        ui_->wheelZoomSensitivityValueLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        ui_->wheelZoomSensitivityValueLabel->setStyleSheet(QStringLiteral(
            "QLabel {"
            "color: #475569;"
            "font-weight: 600;"
            "}"));
    }
}

NavigationSettingsWidget::~NavigationSettingsWidget()
{
    delete ui_;
}

void NavigationSettingsWidget::retranslateUi()
{
    if (ui_ != nullptr) {
        ui_->retranslateUi(this);
    }
}

QLabel* NavigationSettingsWidget::tipsLabel() const { return ui_ != nullptr ? ui_->tipsLabel : nullptr; }
QFormLayout* NavigationSettingsWidget::toggleLayout() const { return ui_ != nullptr ? ui_->toggleLayout : nullptr; }
QCheckBox* NavigationSettingsWidget::invertOrbitCheckBox() const { return ui_ != nullptr ? ui_->invertOrbitCheckBox : nullptr; }
QCheckBox* NavigationSettingsWidget::invertPanCheckBox() const { return ui_ != nullptr ? ui_->invertPanCheckBox : nullptr; }
QCheckBox* NavigationSettingsWidget::invertWheelCheckBox() const { return ui_ != nullptr ? ui_->invertWheelCheckBox : nullptr; }
QWidget* NavigationSettingsWidget::wheelZoomSensitivityControl() const { return ui_ != nullptr ? ui_->wheelZoomSensitivityControl : nullptr; }
QSlider* NavigationSettingsWidget::wheelZoomSensitivitySlider() const { return ui_ != nullptr ? ui_->wheelZoomSensitivitySlider : nullptr; }
QLabel* NavigationSettingsWidget::wheelZoomSensitivityValueLabel() const { return ui_ != nullptr ? ui_->wheelZoomSensitivityValueLabel : nullptr; }
