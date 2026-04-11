#include "gui/BackstageOpenActionsWidget.h"

#include <QVBoxLayout>

#include "ui_BackstageOpenActionsWidget.h"

BackstageOpenActionsWidget::BackstageOpenActionsWidget(QWidget* parent)
    : QWidget(parent)
    , ui_(new Ui::BackstageOpenActionsWidget())
{
    ui_->setupUi(this);
}

BackstageOpenActionsWidget::~BackstageOpenActionsWidget()
{
    delete ui_;
}

void BackstageOpenActionsWidget::retranslateUi()
{
    if (ui_ != nullptr) {
        ui_->retranslateUi(this);
    }
}

QVBoxLayout* BackstageOpenActionsWidget::actionsLayout() const
{
    return ui_ != nullptr ? ui_->actionsLayout : nullptr;
}
