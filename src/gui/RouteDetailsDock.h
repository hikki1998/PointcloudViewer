#pragma once

#include <QDockWidget>

class QString;
class QTabWidget;
class QVBoxLayout;
class QWidget;

class RouteDetailsDock final : public QDockWidget
{
    Q_OBJECT

public:
    explicit RouteDetailsDock(QWidget* parent = nullptr);
    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

    void retranslateUi();
    QTabWidget* tabWidget() const;
    QVBoxLayout* waypointsLayout() const;
    QVBoxLayout* partPointsLayout() const;
    QVBoxLayout* routeQaLayout() const;

private:
    struct TabPage
    {
        QWidget* page = nullptr;
        QVBoxLayout* layout = nullptr;
    };

    TabPage createTabPage(const QString& objectName);

    QTabWidget* tabWidget_ = nullptr;
    TabPage waypointsTab_;
    TabPage partPointsTab_;
    TabPage routeQaTab_;
};
