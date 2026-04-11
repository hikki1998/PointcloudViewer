#include "gui/BackstagePageHeaderWidget.h"

#include <QLabel>

#include "ui_BackstagePageHeaderWidget.h"

BackstagePageHeaderWidget::BackstagePageHeaderWidget(QWidget* parent)
    : QWidget(parent)
    , ui_(new Ui::BackstagePageHeaderWidget())
{
    ui_->setupUi(this);

    if (ui_->titleLabel != nullptr) {
        ui_->titleLabel->setObjectName(QStringLiteral("backstageTitleLabel"));
    }
    if (ui_->subtitleLabel != nullptr) {
        ui_->subtitleLabel->setObjectName(QStringLiteral("backstageSubtitleLabel"));
    }
}

BackstagePageHeaderWidget::~BackstagePageHeaderWidget()
{
    delete ui_;
}

void BackstagePageHeaderWidget::retranslateUi()
{
    if (ui_ != nullptr) {
        ui_->retranslateUi(this);
    }
}

void BackstagePageHeaderWidget::setTitleText(const QString& text)
{
    if (ui_ != nullptr && ui_->titleLabel != nullptr) {
        ui_->titleLabel->setText(text);
    }
}

void BackstagePageHeaderWidget::setSubtitleText(const QString& text)
{
    if (ui_ != nullptr && ui_->subtitleLabel != nullptr) {
        ui_->subtitleLabel->setText(text);
    }
}

QLabel* BackstagePageHeaderWidget::titleLabel() const
{
    return ui_ != nullptr ? ui_->titleLabel : nullptr;
}

QLabel* BackstagePageHeaderWidget::subtitleLabel() const
{
    return ui_ != nullptr ? ui_->subtitleLabel : nullptr;
}
