#include "gui/BackstageAboutWidget.h"

#include <QLabel>

#include "ui_BackstageAboutWidget.h"

BackstageAboutWidget::BackstageAboutWidget(QWidget* parent)
    : QWidget(parent)
    , ui_(new Ui::BackstageAboutWidget())
{
    ui_->setupUi(this);
}

BackstageAboutWidget::~BackstageAboutWidget()
{
    delete ui_;
}

void BackstageAboutWidget::retranslateUi()
{
    if (ui_ != nullptr) {
        ui_->retranslateUi(this);
    }
}

QLabel* BackstageAboutWidget::bodyLabel() const
{
    return ui_ != nullptr ? ui_->bodyLabel : nullptr;
}
