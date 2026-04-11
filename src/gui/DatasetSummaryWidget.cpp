#include "gui/DatasetSummaryWidget.h"

#include <QFormLayout>
#include <QLabel>
#include <QList>

#include "ui_DatasetSummaryWidget.h"

DatasetSummaryWidget::DatasetSummaryWidget(QWidget* parent)
    : QGroupBox(parent)
    , ui_(new Ui::DatasetSummaryWidget())
{
    ui_->setupUi(this);

    if (ui_->datasetLayout != nullptr) {
        ui_->datasetLayout->setLabelAlignment(Qt::AlignLeft | Qt::AlignTop);
        ui_->datasetLayout->setFormAlignment(Qt::AlignTop);
    }

    const QList<QLabel*> datasetLabels = {
        ui_->datasetNameValueLabel,
        ui_->datasetPathValueLabel,
        ui_->datasetPointsValueLabel,
        ui_->datasetBoundsValueLabel,
        ui_->datasetExtentValueLabel,
        ui_->datasetColorValueLabel
    };
    for (QLabel* label : datasetLabels) {
        if (label != nullptr) {
            label->setWordWrap(true);
            label->setTextInteractionFlags(Qt::TextSelectableByMouse);
        }
    }
}

DatasetSummaryWidget::~DatasetSummaryWidget()
{
    delete ui_;
}

void DatasetSummaryWidget::retranslateUi()
{
    if (ui_ != nullptr) {
        ui_->retranslateUi(this);
    }
}

QFormLayout* DatasetSummaryWidget::datasetLayout() const { return ui_ != nullptr ? ui_->datasetLayout : nullptr; }
QLabel* DatasetSummaryWidget::datasetNameValueLabel() const { return ui_ != nullptr ? ui_->datasetNameValueLabel : nullptr; }
QLabel* DatasetSummaryWidget::datasetPathValueLabel() const { return ui_ != nullptr ? ui_->datasetPathValueLabel : nullptr; }
QLabel* DatasetSummaryWidget::datasetPointsValueLabel() const { return ui_ != nullptr ? ui_->datasetPointsValueLabel : nullptr; }
QLabel* DatasetSummaryWidget::datasetBoundsValueLabel() const { return ui_ != nullptr ? ui_->datasetBoundsValueLabel : nullptr; }
QLabel* DatasetSummaryWidget::datasetExtentValueLabel() const { return ui_ != nullptr ? ui_->datasetExtentValueLabel : nullptr; }
QLabel* DatasetSummaryWidget::datasetColorValueLabel() const { return ui_ != nullptr ? ui_->datasetColorValueLabel : nullptr; }
