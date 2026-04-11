#pragma once

#include <QDockWidget>

class QWidget;

class ProfileClassificationDock final : public QDockWidget
{
    Q_OBJECT

public:
    explicit ProfileClassificationDock(QWidget* parent = nullptr);

    void retranslateUi();
    void setContentWidget(QWidget* widget);

private:
    QWidget* surface_ = nullptr;
    QWidget* contentWidget_ = nullptr;
};
