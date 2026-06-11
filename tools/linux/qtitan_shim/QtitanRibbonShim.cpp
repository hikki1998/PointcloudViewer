#include "QtitanRibbonShim.h"

#include <QAction>
#include <QApplication>
#include <QHBoxLayout>
#include <QPainter>
#include <QStyleOption>
#include <QTabBar>
#include <QToolTip>
#include <QVBoxLayout>

QTITAN_BEGIN_NAMESPACE

RibbonToolTip::RibbonToolTip(QWidget* parent)
    : QFrame(parent)
{
}

RibbonToolTip* RibbonToolTip::instance()
{
    static RibbonToolTip* tooltip = new RibbonToolTip();
    return tooltip;
}

void RibbonToolTip::hideToolTip()
{
    QToolTip::hideText();
    instance()->hide();
}

RibbonStyle::RibbonStyle(QStyle* style)
    : QProxyStyle(style)
{
}

void RibbonStyle::setTheme(Theme theme)
{
    theme_ = theme;
}

RibbonStyle::Theme RibbonStyle::getTheme() const
{
    return theme_;
}

void RibbonStyle::setActiveTabAccented(bool accented)
{
    activeTabAccented_ = accented;
}

bool RibbonStyle::isActiveTabAccented() const
{
    return activeTabAccented_;
}

void RibbonStyle::setAnimationEnabled(bool enabled)
{
    animationEnabled_ = enabled;
}

bool RibbonStyle::isAnimationEnabled() const
{
    return animationEnabled_;
}

void RibbonStyle::drawTabShape(const QStyleOption* option, QPainter* painter, const QWidget* widget) const
{
    if (option == nullptr || painter == nullptr) {
        return;
    }
    drawPrimitive(QStyle::PE_FrameTabWidget, option, painter, widget);
}

void RibbonStyle::drawTabShapeLabel(const QStyleOption* option, QPainter* painter, const QWidget* widget) const
{
    if (option == nullptr || painter == nullptr) {
        return;
    }
    drawControl(QStyle::CE_TabBarTabLabel, option, painter, widget);
}

bool RibbonStyle::showToolTip(const QPoint& pos, QWidget* widget)
{
    if (widget == nullptr || widget->toolTip().isEmpty()) {
        return false;
    }
    QToolTip::showText(pos, widget->toolTip(), widget);
    return true;
}

bool RibbonStyle::drawPanelTipLabel(const QStyleOption* option, QPainter* painter, const QWidget* widget) const
{
    Q_UNUSED(widget);
    if (option == nullptr || painter == nullptr) {
        return false;
    }
    painter->save();
    painter->fillRect(option->rect, option->palette.window());
    painter->drawRect(option->rect.adjusted(0, 0, -1, -1));
    painter->restore();
    return true;
}

RibbonBackstageButton::RibbonBackstageButton(QWidget* parent)
    : QToolButton(parent)
{
    setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    setAutoRaise(true);
}

void RibbonBackstageButton::setTabStyle(bool enabled)
{
    setCheckable(enabled);
}

void RibbonBackstageButton::setFlatStyle(bool enabled)
{
    setAutoRaise(enabled);
}

RibbonBackstagePage::RibbonBackstagePage(QWidget* parent)
    : QWidget(parent)
{
}

RibbonBackstageView::RibbonBackstageView(QWidget* parent)
    : QWidget(parent)
{
    setWindowFlags(Qt::Popup);
    auto* rootLayout = new QHBoxLayout(this);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->setSpacing(0);

    navigationBar_ = new QToolBar(this);
    navigationBar_->setOrientation(Qt::Vertical);
    navigationBar_->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    navigationBar_->setMovable(false);
    navigationBar_->setFloatable(false);
    navigationBar_->setIconSize(QSize(20, 20));
    rootLayout->addWidget(navigationBar_);

    stack_ = new QStackedWidget(this);
    rootLayout->addWidget(stack_, 1);
    resize(960, 620);
}

QAction* RibbonBackstageView::addPage(QWidget* page)
{
    if (page == nullptr) {
        return nullptr;
    }
    stack_->addWidget(page);
    QAction* action = new QAction(page->windowIcon(), page->windowTitle(), this);
    RibbonBackstageButton* button = addNavigationButton(action);
    connect(action, &QAction::triggered, this, [this, page, button]() {
        setActivePage(page);
        if (button != nullptr) {
            button->setChecked(true);
        }
    });
    return action;
}

QAction* RibbonBackstageView::addAction(const QIcon& icon, const QString& text)
{
    QAction* action = new QAction(icon, text, this);
    addNavigationButton(action);
    return action;
}

void RibbonBackstageView::addSeparator()
{
    navigationBar_->addSeparator();
}

void RibbonBackstageView::setActivePage(QWidget* page)
{
    if (page != nullptr && stack_->indexOf(page) >= 0) {
        stack_->setCurrentWidget(page);
    }
}

QWidget* RibbonBackstageView::getActivePage() const
{
    return stack_->currentWidget();
}

void RibbonBackstageView::open()
{
    emit aboutToShow();
    if (QWidget* parent = parentWidget()) {
        const QPoint topLeft = parent->mapToGlobal(QPoint(0, parent->height()));
        move(topLeft);
    }
    show();
    raise();
}

RibbonBackstageButton* RibbonBackstageView::addNavigationButton(QAction* action)
{
    if (action == nullptr) {
        return nullptr;
    }
    auto* button = new RibbonBackstageButton(this);
    button->setDefaultAction(action);
    navigationBar_->addWidget(button);
    return button;
}

RibbonQuickAccessBar::RibbonQuickAccessBar(QWidget* parent)
    : QToolBar(parent)
{
    setMovable(false);
    setFloatable(false);
    setIconSize(QSize(16, 16));
}

RibbonGroup::RibbonGroup(const QString& title, QWidget* parent)
    : QGroupBox(title, parent)
{
    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(8, 16, 8, 8);
    layout->setSpacing(4);
}

QAction* RibbonGroup::addAction(QAction* action, Qt::ToolButtonStyle style)
{
    if (action == nullptr) {
        return nullptr;
    }
    auto* button = new QToolButton(this);
    button->setDefaultAction(action);
    button->setToolButtonStyle(style);
    button->setAutoRaise(true);
    layout()->addWidget(button);
    return action;
}

RibbonPage::RibbonPage(const QString& title, QWidget* parent)
    : QWidget(parent)
    , title_(title)
{
    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(6, 4, 6, 4);
    layout->setSpacing(6);
    layout->addStretch(1);
}

QString RibbonPage::title() const
{
    return title_;
}

void RibbonPage::setTitle(const QString& title)
{
    title_ = title;
}

RibbonGroup* RibbonPage::addGroup(const QString& title)
{
    auto* group = new RibbonGroup(title, this);
    auto* boxLayout = qobject_cast<QHBoxLayout*>(layout());
    if (boxLayout != nullptr) {
        boxLayout->insertWidget(std::max(0, boxLayout->count() - 1), group);
    }
    return group;
}

RibbonSystemButton::RibbonSystemButton(QWidget* parent)
    : QToolButton(parent)
{
    setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    setAutoRaise(true);
}

void RibbonSystemButton::setBackstage(RibbonBackstageView* backstage)
{
    backstage_ = backstage;
    disconnect(this, &QToolButton::clicked, nullptr, nullptr);
    connect(this, &QToolButton::clicked, this, [this]() {
        if (backstage_ != nullptr) {
            backstage_->open();
        }
    });
}

RibbonBar::RibbonBar(QWidget* parent)
    : QWidget(parent)
{
    auto* rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->setSpacing(0);

    topRow_ = new QWidget(this);
    auto* topLayout = new QHBoxLayout(topRow_);
    topLayout->setContentsMargins(6, 2, 6, 2);
    topLayout->setSpacing(6);

    systemButton_ = new RibbonSystemButton(topRow_);
    topLayout->addWidget(systemButton_);

    quickAccessBar_ = new RibbonQuickAccessBar(topRow_);
    topLayout->addWidget(quickAccessBar_);
    topLayout->addStretch(1);
    rootLayout->addWidget(topRow_);

    pageTabs_ = new QToolBar(this);
    pageTabs_->setMovable(false);
    pageTabs_->setFloatable(false);
    pageTabs_->setToolButtonStyle(Qt::ToolButtonTextOnly);
    rootLayout->addWidget(pageTabs_);

    pageStack_ = new QStackedWidget(this);
    rootLayout->addWidget(pageStack_);
}

void RibbonBar::setFrameThemeEnabled(bool enabled)
{
    Q_UNUSED(enabled);
}

void RibbonBar::setTitleBarVisible(bool visible)
{
    titleBarVisible_ = visible;
}

bool RibbonBar::isTitleBarVisible() const
{
    return titleBarVisible_;
}

int RibbonBar::titleBarHeight() const
{
    return titleBarVisible_ ? topRow_->height() : 0;
}

void RibbonBar::showQuickAccess(bool visible)
{
    quickAccessBar_->setVisible(visible);
}

RibbonQuickAccessBar* RibbonBar::quickAccessBar() const
{
    return quickAccessBar_;
}

void RibbonBar::addSystemButton(const QIcon& icon, const QString& text)
{
    systemButton_->setIcon(icon);
    systemButton_->setText(text);
}

RibbonSystemButton* RibbonBar::getSystemButton() const
{
    return systemButton_;
}

bool RibbonBar::isBackstageVisible() const
{
    return systemButton_ != nullptr
        && systemButton_->backstage_ != nullptr
        && systemButton_->backstage_->isVisible();
}

RibbonPage* RibbonBar::addPage(const QString& title)
{
    auto* page = new RibbonPage(title, this);
    pages_.append(page);
    pageStack_->addWidget(page);

    QAction* tabAction = pageTabs_->addAction(title);
    tabAction->setCheckable(true);
    tabAction->setData(pages_.size() - 1);
    connect(tabAction, &QAction::triggered, this, [this, tabAction]() {
        const int index = tabAction->data().toInt();
        pageStack_->setCurrentIndex(index);
        for (QAction* action : pageTabs_->actions()) {
            action->setChecked(action == tabAction);
        }
    });
    if (pages_.size() == 1) {
        tabAction->setChecked(true);
        pageStack_->setCurrentIndex(0);
    }
    return page;
}

const QList<RibbonPage*>& RibbonBar::pages() const
{
    return pages_;
}

int RibbonBar::currentPageIndex() const
{
    return pageStack_->currentIndex();
}

void RibbonBar::setCornerWidget(QWidget* widget, Qt::Corner corner)
{
    if (widget == nullptr || corner != Qt::TopRightCorner) {
        return;
    }
    auto* topLayout = qobject_cast<QHBoxLayout*>(topRow_->layout());
    if (topLayout != nullptr) {
        topLayout->addWidget(widget);
    }
}

RibbonMainWindow::RibbonMainWindow(QWidget* parent)
    : QMainWindow(parent)
{
}

void RibbonMainWindow::setRibbonBar(RibbonBar* ribbonBar)
{
    ribbonBar_ = ribbonBar;
    setMenuWidget(ribbonBar_);
}

RibbonBar* RibbonMainWindow::ribbonBar() const
{
    return ribbonBar_;
}

QTITAN_END_NAMESPACE
