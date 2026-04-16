#include <QApplication>
#include <QCursor>
#include <QFont>
#include <QIcon>
#include <QScreen>
#include <QSplashScreen>
#include <QSurfaceFormat>
#include <QTimer>
#include <QTranslator>

#include "QtnRibbonStyle.h"

#include "gui/MainWindow.h"

namespace
{
QScreen* resolveSplashTargetScreen()
{
    if (QScreen* cursorScreen = QGuiApplication::screenAt(QCursor::pos())) {
        return cursorScreen;
    }
    return QGuiApplication::primaryScreen();
}

QPixmap buildScaledSplashPixmap(const QPixmap& sourcePixmap, const QRect& availableGeometry)
{
    if (sourcePixmap.isNull() || !availableGeometry.isValid()) {
        return sourcePixmap;
    }

    const QSize maxSplashSize(
        std::max(640, static_cast<int>(std::lround(availableGeometry.width() * 0.72))),
        std::max(360, static_cast<int>(std::lround(availableGeometry.height() * 0.72))));
    if (sourcePixmap.size().width() <= maxSplashSize.width()
        && sourcePixmap.size().height() <= maxSplashSize.height()) {
        return sourcePixmap;
    }

    return sourcePixmap.scaled(maxSplashSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
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

    QApplication::setApplicationName("LAS Point Cloud Viewer");
    QApplication::setApplicationVersion("1.1.0");
    QApplication::setOrganizationName("VibeCodingProject");

    QFont appFont(QStringLiteral("Segoe UI"), 9);
    app.setFont(appFont);

    auto* ribbonStyle = new Qtitan::RibbonStyle();
    ribbonStyle->setTheme(Qtitan::RibbonStyle::Office2016White);
    ribbonStyle->setAnimationEnabled(false);
    app.setStyle(ribbonStyle);

    QTranslator appTranslator;
    QTranslator qtTranslator;

    const QIcon appIcon(QStringLiteral(":/assets/icon/software.png"));
    app.setWindowIcon(appIcon);

    const QPixmap splashPixmap(QStringLiteral(":/assets/icon/Splash.png"));
    QScreen* splashTargetScreen = resolveSplashTargetScreen();
    const QRect splashAvailableGeometry =
        splashTargetScreen != nullptr ? splashTargetScreen->availableGeometry() : QRect();
    QSplashScreen splashScreen(
        splashTargetScreen,
        buildScaledSplashPixmap(splashPixmap, splashAvailableGeometry));
    splashScreen.setWindowFlag(Qt::WindowStaysOnTopHint, true);
    if (splashAvailableGeometry.isValid()) {
        const QRect centeredGeometry(
            QPoint(
                splashAvailableGeometry.left() + (splashAvailableGeometry.width() - splashScreen.width()) / 2,
                splashAvailableGeometry.top() + (splashAvailableGeometry.height() - splashScreen.height()) / 2),
            splashScreen.size());
        splashScreen.setGeometry(centeredGeometry);
    }
    splashScreen.show();
    app.processEvents();

    MainWindow mainWindow(&appTranslator, &qtTranslator);
    mainWindow.setWindowIcon(appIcon);

    QTimer::singleShot(1500, [&]() {
        mainWindow.show();
        splashScreen.finish(&mainWindow);
    });

    return app.exec();
}
