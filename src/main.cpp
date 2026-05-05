#include <QApplication>
#include <QAction>
#include <QCursor>
#include <QFont>
#include <QIcon>
#include <QPainter>
#include <QScreen>
#include <QSplashScreen>
#include <QStyleOption>
#include <QSurfaceFormat>
#include <QTimer>
#include <QToolButton>
#include <QToolTip>
#include <QTranslator>

#include "QtnRibbonToolTip.h"
#include "QtnRibbonStyle.h"

#include "gui/MainWindow.h"

namespace
{
class LasViewerRibbonStyle final : public Qtitan::RibbonStyle
{
public:
    using Qtitan::RibbonStyle::RibbonStyle;

protected:
    bool showToolTip(const QPoint& pos, QWidget* widget) override
    {
        const QString toolTipText = resolveToolTipText(widget);
        if (toolTipText.isEmpty()) {
            return Qtitan::RibbonStyle::showToolTip(pos, widget);
        }

        applyLightToolTipPalette();
        Qtitan::RibbonToolTip::hideToolTip();
        QToolTip::showText(pos, toolTipText, widget);
        return true;
    }

    bool drawPanelTipLabel(const QStyleOption* option, QPainter* painter, const QWidget* widget) const override
    {
        Q_UNUSED(widget);
        if (option == nullptr || painter == nullptr) {
            return Qtitan::RibbonStyle::drawPanelTipLabel(option, painter, widget);
        }

        painter->save();
        painter->fillRect(option->rect, QColor(248, 250, 252));
        painter->setPen(QPen(QColor(148, 163, 184)));
        painter->drawRect(option->rect.adjusted(0, 0, -1, -1));
        painter->restore();
        return true;
    }

private:
    static QString resolveToolTipText(QWidget* widget)
    {
        if (widget == nullptr) {
            return QString();
        }

        QString toolTipText = widget->toolTip().trimmed();
        if (!toolTipText.isEmpty()) {
            return toolTipText;
        }

        auto* button = qobject_cast<QToolButton*>(widget);
        if (button == nullptr) {
            return QString();
        }

        QAction* action = button->defaultAction();
        if (action == nullptr) {
            return QString();
        }

        toolTipText = action->toolTip().trimmed();
        if (!toolTipText.isEmpty()) {
            return toolTipText;
        }
        return action->text().trimmed();
    }

    static void applyLightToolTipPalette()
    {
        QPalette palette = QToolTip::palette();
        palette.setColor(QPalette::ToolTipBase, QColor(248, 250, 252));
        palette.setColor(QPalette::ToolTipText, QColor(15, 23, 42));
        QToolTip::setPalette(palette);
    }
};

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

    auto* ribbonStyle = new LasViewerRibbonStyle();
    ribbonStyle->setTheme(Qtitan::RibbonStyle::Office2016White);
    ribbonStyle->setActiveTabAccented(false);
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
