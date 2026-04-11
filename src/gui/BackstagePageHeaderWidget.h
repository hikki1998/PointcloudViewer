#pragma once

#include <QWidget>

namespace Ui
{
class BackstagePageHeaderWidget;
}

class QLabel;

class BackstagePageHeaderWidget final : public QWidget
{
    Q_OBJECT

public:
    explicit BackstagePageHeaderWidget(QWidget* parent = nullptr);
    ~BackstagePageHeaderWidget() override;

    void retranslateUi();
    void setTitleText(const QString& text);
    void setSubtitleText(const QString& text);

    QLabel* titleLabel() const;
    QLabel* subtitleLabel() const;

private:
    Ui::BackstagePageHeaderWidget* ui_ = nullptr;
};
