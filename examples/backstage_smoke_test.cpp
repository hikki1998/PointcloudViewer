#include <QApplication>
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QSurfaceFormat>
#include <QThread>
#include <QTranslator>

#include <iostream>

#include "gui/MainWindow.h"

#include "QtnRibbonBackstageView.h"
#include "QtnRibbonBar.h"
#include "QtnRibbonSystemPopupBar.h"

namespace
{
void pumpEvents(int durationMs)
{
    QElapsedTimer timer;
    timer.start();

    while (timer.elapsed() < durationMs) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
        QThread::msleep(25);
    }
}

bool verify(bool condition, const char* message)
{
    if (!condition) {
        std::cerr << "[FAIL] " << message << std::endl;
        return false;
    }
    return true;
}
}

int main(int argc, char* argv[])
{
    QCoreApplication::setAttribute(Qt::AA_UseDesktopOpenGL);

    QSurfaceFormat format;
    format.setRenderableType(QSurfaceFormat::OpenGL);
    format.setProfile(QSurfaceFormat::CompatibilityProfile);
    format.setVersion(2, 1);
    format.setDepthBufferSize(24);
    format.setStencilBufferSize(8);
    format.setSwapBehavior(QSurfaceFormat::DoubleBuffer);
    format.setSamples(4);
    QSurfaceFormat::setDefaultFormat(format);

    QApplication app(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("LASViewerBackstageSmokeTest"));
    QCoreApplication::setApplicationVersion(QStringLiteral("1.0"));

    QTranslator appTranslator;
    QTranslator qtTranslator;
    MainWindow window(&appTranslator, &qtTranslator);
    window.resize(1400, 900);
    window.show();
    pumpEvents(300);

    Qtitan::RibbonBar* ribbonBar = window.ribbonBar();
    if (!verify(ribbonBar != nullptr, "MainWindow should expose a RibbonBar")) {
        return 1;
    }

    Qtitan::RibbonSystemButton* systemButton = ribbonBar->getSystemButton();
    if (!verify(systemButton != nullptr, "Ribbon system button should exist")) {
        return 1;
    }

    Qtitan::RibbonBackstageView* backstageView =
        window.findChild<Qtitan::RibbonBackstageView*>(QStringLiteral("mainBackstageView"));
    if (!verify(backstageView != nullptr, "Backstage view should be created")) {
        return 1;
    }

    systemButton->click();
    pumpEvents(200);
    if (!verify(ribbonBar->isBackstageVisible(), "Backstage should become visible after clicking system button")) {
        return 1;
    }

    QWidget* applicationSettingsPage =
        window.findChild<QWidget*>(QStringLiteral("backstageApplicationSettingsPage"));
    QWidget* aboutPage = window.findChild<QWidget*>(QStringLiteral("backstageAboutPage"));
    if (!verify(applicationSettingsPage != nullptr, "Application Settings backstage page should exist")) {
        return 1;
    }
    if (!verify(aboutPage != nullptr, "About backstage page should exist")) {
        return 1;
    }

    backstageView->setActivePage(applicationSettingsPage);
    if (!verify(
            backstageView->getActivePage() == applicationSettingsPage,
            "Backstage should switch to Application Settings page")) {
        return 1;
    }

    backstageView->setActivePage(aboutPage);
    if (!verify(
            backstageView->getActivePage() == aboutPage,
            "Backstage should switch to About page")) {
        return 1;
    }

    backstageView->hide();
    pumpEvents(120);
    if (!verify(!ribbonBar->isBackstageVisible(), "Backstage should hide when requested")) {
        return 1;
    }

    std::cout << "[PASS] Main backstage smoke test completed." << std::endl;
    return 0;
}
