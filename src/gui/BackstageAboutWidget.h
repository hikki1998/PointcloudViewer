#pragma once

#include <QWidget>

namespace Ui
{
class BackstageAboutWidget;
}

class QLabel;

class BackstageAboutWidget final : public QWidget
{
    Q_OBJECT

public:
    explicit BackstageAboutWidget(QWidget* parent = nullptr);
    ~BackstageAboutWidget() override;

    void retranslateUi();
    QLabel* bodyLabel() const;

private:
    Ui::BackstageAboutWidget* ui_ = nullptr;
};
