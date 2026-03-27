#include <QApplication>
#include <QFont>
#include <QSurfaceFormat>
#include <QTranslator>

#include "QtnRibbonStyle.h"

#include "gui/MainWindow.h"

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
    QApplication::setApplicationVersion("1.0.0");
    QApplication::setOrganizationName("VibeCodingProject");

    QFont appFont(QStringLiteral("Segoe UI"), 9);
    app.setFont(appFont);

    auto* ribbonStyle = new Qtitan::RibbonStyle();
    ribbonStyle->setTheme(Qtitan::RibbonStyle::Office2016White);
    ribbonStyle->setAnimationEnabled(false);
    app.setStyle(ribbonStyle);

    QTranslator appTranslator;
    QTranslator qtTranslator;

    MainWindow mainWindow(&appTranslator, &qtTranslator);
    mainWindow.show();

    return app.exec();
}
