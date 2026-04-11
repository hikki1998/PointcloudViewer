#pragma once

#include <QDockWidget>

class ProfilePlotWidget;

class QLabel;
class QWidget;

class SpanProfileDock final : public QDockWidget
{
    Q_OBJECT

public:
    explicit SpanProfileDock(QWidget* parent = nullptr);

    void retranslateUi();
    ProfilePlotWidget* plotWidget() const;

private:
    QLabel* titleLabel_ = nullptr;
    QLabel* subtitleLabel_ = nullptr;
    ProfilePlotWidget* plotWidget_ = nullptr;
};
