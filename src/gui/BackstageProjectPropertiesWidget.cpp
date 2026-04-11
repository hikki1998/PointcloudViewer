#include "gui/BackstageProjectPropertiesWidget.h"

#include <QLabel>
#include <QPushButton>

#include "ui_BackstageProjectPropertiesWidget.h"

BackstageProjectPropertiesWidget::BackstageProjectPropertiesWidget(QWidget* parent)
    : QWidget(parent)
    , ui_(new Ui::BackstageProjectPropertiesWidget())
{
    ui_->setupUi(this);

    if (ui_->projectFileLabel != nullptr) {
        ui_->projectFileLabel->setObjectName(QStringLiteral("backstageProjectFileLabel"));
    }
    if (ui_->datasetCountLabel != nullptr) {
        ui_->datasetCountLabel->setObjectName(QStringLiteral("backstageProjectDatasetsLabel"));
    }
    if (ui_->coordinateSystemsLabel != nullptr) {
        ui_->coordinateSystemsLabel->setObjectName(QStringLiteral("backstageProjectCoordinateSystemsLabel"));
    }
}

BackstageProjectPropertiesWidget::~BackstageProjectPropertiesWidget()
{
    delete ui_;
}

void BackstageProjectPropertiesWidget::retranslateUi()
{
    if (ui_ != nullptr) {
        ui_->retranslateUi(this);
    }
}

QLabel* BackstageProjectPropertiesWidget::projectFileLabel() const
{
    return ui_ != nullptr ? ui_->projectFileLabel : nullptr;
}

QLabel* BackstageProjectPropertiesWidget::datasetCountLabel() const
{
    return ui_ != nullptr ? ui_->datasetCountLabel : nullptr;
}

QLabel* BackstageProjectPropertiesWidget::coordinateSystemsLabel() const
{
    return ui_ != nullptr ? ui_->coordinateSystemsLabel : nullptr;
}

QLabel* BackstageProjectPropertiesWidget::projectFileValueLabel() const
{
    return ui_ != nullptr ? ui_->projectFileValueLabel : nullptr;
}

QLabel* BackstageProjectPropertiesWidget::datasetCountValueLabel() const
{
    return ui_ != nullptr ? ui_->datasetCountValueLabel : nullptr;
}

QLabel* BackstageProjectPropertiesWidget::coordinateSystemsValueLabel() const
{
    return ui_ != nullptr ? ui_->coordinateSystemsValueLabel : nullptr;
}

QPushButton* BackstageProjectPropertiesWidget::editCoordinateSystemsButton() const
{
    return ui_ != nullptr ? ui_->editCoordinateSystemsButton : nullptr;
}
