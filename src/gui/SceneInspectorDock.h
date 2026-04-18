#pragma once

#include <QDockWidget>

class QScrollArea;
class QTabWidget;
class QVBoxLayout;

class SceneInspectorDock final : public QDockWidget
{
    Q_OBJECT

public:
    explicit SceneInspectorDock(QWidget* parent = nullptr);
    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

    void retranslateUi();
    QTabWidget* tabWidget() const;

    QScrollArea* overviewScrollArea() const;
    QVBoxLayout* overviewLayout() const;
    QScrollArea* towerScrollArea() const;
    QVBoxLayout* towerLayout() const;
    QScrollArea* issueScrollArea() const;
    QVBoxLayout* issueLayout() const;
    QScrollArea* renderingScrollArea() const;
    QVBoxLayout* renderingLayout() const;
    QScrollArea* measurementScrollArea() const;
    QVBoxLayout* measurementLayout() const;
    QScrollArea* analysisScrollArea() const;
    QVBoxLayout* analysisLayout() const;
    QScrollArea* navigationScrollArea() const;
    QVBoxLayout* navigationLayout() const;

private:
    struct TabPage
    {
        QScrollArea* scrollArea = nullptr;
        QVBoxLayout* layout = nullptr;
    };

    TabPage createTabPage(const QString& objectName);

    QTabWidget* tabWidget_ = nullptr;
    TabPage overviewTab_;
    TabPage towerTab_;
    TabPage issueTab_;
    TabPage renderingTab_;
    TabPage measurementTab_;
    TabPage analysisTab_;
    TabPage navigationTab_;
};
