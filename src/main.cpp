#include <QApplication>
#include <QAction>
#include <QCursor>
#include <QFont>
#include <QFontMetrics>
#include <QIcon>
#include <QPainter>
#include <QPainterPath>
#include <QScreen>
#include <QSplashScreen>
#include <QStyleOption>
#include <QStyleOptionTab>
#include <QSurfaceFormat>
#include <QTimer>
#include <QToolButton>
#include <QToolTip>
#include <QTranslator>

#include "QtnRibbonBar.h"
#include "QtnRibbonPage.h"
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
    void drawTabShape(const QStyleOption* option, QPainter* painter, const QWidget* widget) const override
    {
        if (!shouldUseReadableRibbonTabPaint(option, painter, widget)) {
            Qtitan::RibbonStyle::drawTabShape(option, painter, widget);
            return;
        }

        const RibbonTabVisual visual = resolveRibbonTabVisual(option, widget);
        const QRect tabRect = option->rect.adjusted(1, 3, -1, 0);
        if (!tabRect.isValid()) {
            return;
        }

        painter->save();
        painter->setRenderHint(QPainter::Antialiasing, true);

        const QPainterPath tabPath = buildRibbonTabPath(tabRect, 7.0);
        if (visual.background.alpha() > 0) {
            painter->fillPath(tabPath, visual.background);
        }
        if (visual.border.alpha() > 0) {
            painter->strokePath(tabPath, QPen(visual.border, 1.0));
        }

        painter->restore();
    }

    void drawTabShapeLabel(const QStyleOption* option, QPainter* painter, const QWidget* widget) const override
    {
        if (!shouldUseReadableRibbonTabPaint(option, painter, widget)) {
            Qtitan::RibbonStyle::drawTabShapeLabel(option, painter, widget);
            return;
        }

        const QString tabText = resolveRibbonTabText(option, widget);
        if (tabText.trimmed().isEmpty()) {
            return;
        }

        const RibbonTabVisual visual = resolveRibbonTabVisual(option, widget);
        const bool selected = isRibbonTabSelected(option, widget);
        const int horizontalPadding = option->rect.width() >= 72 ? 10 : 4;
        const QRect textRect = option->rect.adjusted(horizontalPadding, 2, -horizontalPadding, -1);

        QFont font = widget != nullptr ? widget->font() : QApplication::font();
        font.setBold(selected);

        painter->save();
        painter->setFont(font);
        painter->setPen(visual.text);

        const QFontMetrics metrics(font);
        const QString elidedText = metrics.elidedText(
            tabText,
            Qt::ElideRight,
            std::max(0, textRect.width()));
        painter->drawText(
            textRect,
            Qt::AlignCenter | Qt::TextShowMnemonic | Qt::TextSingleLine,
            elidedText);
        painter->restore();
    }

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
    struct RibbonTabVisual
    {
        QColor background;
        QColor border;
        QColor text;
    };

    bool shouldUseReadableRibbonTabPaint(
        const QStyleOption* option,
        const QPainter* painter,
        const QWidget* widget) const
    {
        if (option == nullptr || painter == nullptr || widget == nullptr) {
            return false;
        }

        if (!isRibbonTabWidget(widget)) {
            return false;
        }

        const Qtitan::RibbonStyle::Theme theme = getTheme();
        return theme == Qtitan::RibbonStyle::Office2016Colorful
            || theme == Qtitan::RibbonStyle::Office2016White;
    }

    static bool isRibbonTabSelected(const QStyleOption* option, const QWidget* widget)
    {
        if (widget != nullptr) {
            if (const Qtitan::RibbonBar* ribbonBar = findRibbonBar(widget)) {
                const int tabIndex = resolveRibbonTabIndex(widget);
                if (tabIndex >= 0) {
                    return ribbonBar->currentPageIndex() == tabIndex;
                }
            }
        }
        return option != nullptr && option->state.testFlag(QStyle::State_Selected);
    }

    RibbonTabVisual resolveRibbonTabVisual(const QStyleOption* option, const QWidget* widget) const
    {
        const Qtitan::RibbonStyle::Theme theme = getTheme();
        const bool colorfulTheme = theme == Qtitan::RibbonStyle::Office2016Colorful;
        const bool enabled = option->state.testFlag(QStyle::State_Enabled);
        const bool selected = isRibbonTabSelected(option, widget);
        const bool pressed = option->state.testFlag(QStyle::State_Sunken)
            || option->state.testFlag(QStyle::State_On);
        const bool hovered = option->state.testFlag(QStyle::State_MouseOver);

        if (!enabled) {
            return colorfulTheme
                ? RibbonTabVisual{ QColor(0, 0, 0, 0), QColor(0, 0, 0, 0), QColor(226, 232, 240, 160) }
                : RibbonTabVisual{ QColor(0, 0, 0, 0), QColor(0, 0, 0, 0), QColor(148, 163, 184) };
        }

        if (pressed) {
            return RibbonTabVisual{ QColor(30, 64, 175), QColor(30, 58, 138), QColor(255, 255, 255) };
        }

        if (selected) {
            return RibbonTabVisual{ QColor(29, 78, 216), QColor(30, 64, 175), QColor(255, 255, 255) };
        }

        if (hovered) {
            return colorfulTheme
                ? RibbonTabVisual{ QColor(255, 255, 255, 44), QColor(191, 219, 254, 96), QColor(255, 255, 255) }
                : RibbonTabVisual{ QColor(219, 234, 254), QColor(147, 197, 253), QColor(15, 23, 42) };
        }

        return colorfulTheme
            ? RibbonTabVisual{ QColor(0, 0, 0, 0), QColor(0, 0, 0, 0), QColor(239, 246, 255) }
            : RibbonTabVisual{ QColor(0, 0, 0, 0), QColor(0, 0, 0, 0), QColor(15, 23, 42) };
    }

    static QString resolveRibbonTabText(const QStyleOption* option, const QWidget* widget)
    {
        if (widget != nullptr) {
            if (const Qtitan::RibbonBar* ribbonBar = findRibbonBar(widget)) {
                const int tabIndex = resolveRibbonTabIndex(widget);
                const QList<Qtitan::RibbonPage*>& pages = ribbonBar->pages();
                if (tabIndex >= 0 && tabIndex < pages.size() && pages.at(tabIndex) != nullptr) {
                    const QString pageTitle = pages.at(tabIndex)->title().trimmed();
                    if (!pageTitle.isEmpty()) {
                        return pageTitle;
                    }
                }
            }
        }

        if (option != nullptr && option->styleObject != nullptr) {
            const QString titleText = option->styleObject->property("title").toString().trimmed();
            if (!titleText.isEmpty()) {
                return titleText;
            }

            const QString objectText = option->styleObject->property("text").toString().trimmed();
            if (!objectText.isEmpty()) {
                return objectText;
            }

            if (const auto* action = qobject_cast<const QAction*>(option->styleObject)) {
                const QString actionText = action->text().trimmed();
                if (!actionText.isEmpty()) {
                    return actionText;
                }
            }
        }

        if (const auto* tabOption = qstyleoption_cast<const QStyleOptionTab*>(option)) {
            if (!tabOption->text.trimmed().isEmpty()) {
                return tabOption->text;
            }
        }

        if (widget != nullptr) {
            const QString propertyText = widget->property("text").toString().trimmed();
            if (!propertyText.isEmpty()) {
                return propertyText;
            }
        }

        return widget != nullptr ? widget->windowTitle() : QString();
    }

    static bool isRibbonTabWidget(const QWidget* widget)
    {
        return widget != nullptr
            && QString::fromLatin1(widget->metaObject()->className()).contains(
                QStringLiteral("RibbonTab"),
                Qt::CaseSensitive);
    }

    static const Qtitan::RibbonBar* findRibbonBar(const QWidget* widget)
    {
        const QWidget* current = widget;
        while (current != nullptr) {
            if (const auto* ribbonBar = qobject_cast<const Qtitan::RibbonBar*>(current)) {
                return ribbonBar;
            }
            current = current->parentWidget();
        }
        return nullptr;
    }

    static int resolveRibbonTabIndex(const QWidget* widget)
    {
        if (widget == nullptr || widget->parentWidget() == nullptr) {
            return -1;
        }

        QList<const QWidget*> ribbonTabs;
        const QObjectList siblings = widget->parentWidget()->children();
        for (QObject* sibling : siblings) {
            const QWidget* siblingWidget = qobject_cast<const QWidget*>(sibling);
            if (isRibbonTabWidget(siblingWidget)) {
                ribbonTabs.append(siblingWidget);
            }
        }

        std::sort(
            ribbonTabs.begin(),
            ribbonTabs.end(),
            [](const QWidget* left, const QWidget* right) {
                if (left == nullptr || right == nullptr) {
                    return left < right;
                }
                if (left->geometry().left() != right->geometry().left()) {
                    return left->geometry().left() < right->geometry().left();
                }
                return left->geometry().top() < right->geometry().top();
            });

        for (int index = 0; index < ribbonTabs.size(); ++index) {
            if (ribbonTabs.at(index) == widget) {
                return index;
            }
        }
        return -1;
    }

    static QPainterPath buildRibbonTabPath(const QRect& rect, qreal radius)
    {
        const qreal left = rect.left();
        const qreal top = rect.top();
        const qreal right = rect.right();
        const qreal bottom = rect.bottom();
        const qreal clampedRadius = std::min<qreal>(radius, std::max<qreal>(0.0, rect.width() / 2.0));

        QPainterPath path;
        path.moveTo(left, bottom);
        path.lineTo(left, top + clampedRadius);
        path.quadTo(left, top, left + clampedRadius, top);
        path.lineTo(right - clampedRadius, top);
        path.quadTo(right, top, right, top + clampedRadius);
        path.lineTo(right, bottom);
        path.closeSubpath();
        return path;
    }

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
    QApplication::setApplicationVersion("1.2.0");
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
