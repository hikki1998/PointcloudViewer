#pragma once

#include <QWidget>

namespace Ui
{
class BackstageOpenActionsWidget;
}

class QVBoxLayout;

class BackstageOpenActionsWidget final : public QWidget
{
    Q_OBJECT

public:
    explicit BackstageOpenActionsWidget(QWidget* parent = nullptr);
    ~BackstageOpenActionsWidget() override;

    void retranslateUi();
    QVBoxLayout* actionsLayout() const;

private:
    Ui::BackstageOpenActionsWidget* ui_ = nullptr;
};
