#pragma once

#include "QtitanDef.h"

#include <QFrame>
#include <QGroupBox>
#include <QList>
#include <QMainWindow>
#include <QMenuBar>
#include <QProxyStyle>
#include <QStackedWidget>
#include <QToolBar>
#include <QToolButton>
#include <QWidget>

QTITAN_BEGIN_NAMESPACE

class RibbonBackstageView;
class RibbonGroup;
class RibbonPage;
class RibbonSystemButton;

class QTITAN_EXPORT RibbonToolTip : public QFrame
{
    Q_OBJECT

public:
    explicit RibbonToolTip(QWidget* parent = nullptr);

    static RibbonToolTip* instance();
    static void hideToolTip();
};

class QTITAN_EXPORT RibbonStyle : public QProxyStyle
{
    Q_OBJECT

public:
    enum Theme
    {
        Office2016Colorful = 0,
        Office2016White = 1,
        Office2016DarkGray = 2
    };

    explicit RibbonStyle(QStyle* style = nullptr);

    void setTheme(Theme theme);
    Theme getTheme() const;
    void setActiveTabAccented(bool accented);
    bool isActiveTabAccented() const;
    void setAnimationEnabled(bool enabled);
    bool isAnimationEnabled() const;

protected:
    virtual void drawTabShape(const QStyleOption* option, QPainter* painter, const QWidget* widget) const;
    virtual void drawTabShapeLabel(const QStyleOption* option, QPainter* painter, const QWidget* widget) const;
    virtual bool showToolTip(const QPoint& pos, QWidget* widget);
    virtual bool drawPanelTipLabel(const QStyleOption* option, QPainter* painter, const QWidget* widget) const;

private:
    Theme theme_ = Office2016White;
    bool activeTabAccented_ = false;
    bool animationEnabled_ = false;
};

class QTITAN_EXPORT RibbonBackstageButton : public QToolButton
{
    Q_OBJECT

public:
    explicit RibbonBackstageButton(QWidget* parent = nullptr);

    void setTabStyle(bool enabled);
    void setFlatStyle(bool enabled);
};

class QTITAN_EXPORT RibbonBackstagePage : public QWidget
{
    Q_OBJECT

public:
    explicit RibbonBackstagePage(QWidget* parent = nullptr);
};

class QTITAN_EXPORT RibbonBackstageView : public QWidget
{
    Q_OBJECT

public:
    explicit RibbonBackstageView(QWidget* parent = nullptr);

    QAction* addPage(QWidget* page);
    QAction* addAction(const QIcon& icon, const QString& text);
    void addSeparator();
    void setActivePage(QWidget* page);
    QWidget* getActivePage() const;
    void open();

signals:
    void aboutToShow();

private:
    RibbonBackstageButton* addNavigationButton(QAction* action);

    QToolBar* navigationBar_ = nullptr;
    QStackedWidget* stack_ = nullptr;
};

class QTITAN_EXPORT RibbonQuickAccessBar : public QToolBar
{
    Q_OBJECT

public:
    explicit RibbonQuickAccessBar(QWidget* parent = nullptr);
};

class QTITAN_EXPORT RibbonGroup : public QGroupBox
{
    Q_OBJECT

public:
    explicit RibbonGroup(const QString& title = QString(), QWidget* parent = nullptr);

    QAction* addAction(QAction* action, Qt::ToolButtonStyle style);
};

class QTITAN_EXPORT RibbonPage : public QWidget
{
    Q_OBJECT

public:
    explicit RibbonPage(const QString& title = QString(), QWidget* parent = nullptr);

    QString title() const;
    void setTitle(const QString& title);
    RibbonGroup* addGroup(const QString& title);

private:
    QString title_;
};

class QTITAN_EXPORT RibbonSystemButton : public QToolButton
{
    Q_OBJECT

public:
    explicit RibbonSystemButton(QWidget* parent = nullptr);

    void setBackstage(RibbonBackstageView* backstage);

private:
    friend class RibbonBar;

    RibbonBackstageView* backstage_ = nullptr;
};

class QTITAN_EXPORT RibbonBar : public QWidget
{
    Q_OBJECT

public:
    explicit RibbonBar(QWidget* parent = nullptr);

    void setFrameThemeEnabled(bool enabled);
    void setTitleBarVisible(bool visible);
    bool isTitleBarVisible() const;
    int titleBarHeight() const;
    void showQuickAccess(bool visible);
    RibbonQuickAccessBar* quickAccessBar() const;
    void addSystemButton(const QIcon& icon, const QString& text);
    RibbonSystemButton* getSystemButton() const;
    bool isBackstageVisible() const;
    RibbonPage* addPage(const QString& title);
    const QList<RibbonPage*>& pages() const;
    int currentPageIndex() const;
    void setCornerWidget(QWidget* widget, Qt::Corner corner);

private:
    RibbonSystemButton* systemButton_ = nullptr;
    RibbonQuickAccessBar* quickAccessBar_ = nullptr;
    QWidget* topRow_ = nullptr;
    QStackedWidget* pageStack_ = nullptr;
    QToolBar* pageTabs_ = nullptr;
    QList<RibbonPage*> pages_;
    bool titleBarVisible_ = false;
};

class QTITAN_EXPORT RibbonMainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit RibbonMainWindow(QWidget* parent = nullptr);

    void setRibbonBar(RibbonBar* ribbonBar);
    RibbonBar* ribbonBar() const;

private:
    RibbonBar* ribbonBar_ = nullptr;
};

QTITAN_END_NAMESPACE

QTITAN_USE_NAMESPACE
