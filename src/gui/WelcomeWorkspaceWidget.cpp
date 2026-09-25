#include "gui/WelcomeWorkspaceWidget.h"

#include <QAbstractItemView>
#include <QColor>
#include <QDir>
#include <QFileInfo>
#include <QFrame>
#include <QIcon>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QMenu>
#include <QListWidgetItem>
#include <QPushButton>
#include <QPixmap>
#include <QResizeEvent>
#include <QVBoxLayout>

namespace
{
QFrame* createSectionCard(QWidget* parent, QLabel*& titleLabel, QListWidget*& listWidget)
{
    auto* card = new QFrame(parent);
    card->setObjectName(QStringLiteral("welcomeSectionCard"));

    auto* layout = new QVBoxLayout(card);
    layout->setContentsMargins(18, 16, 18, 16);
    layout->setSpacing(10);

    titleLabel = new QLabel(card);
    titleLabel->setObjectName(QStringLiteral("welcomeSectionTitle"));
    layout->addWidget(titleLabel);

    listWidget = new QListWidget(card);
    listWidget->setObjectName(QStringLiteral("welcomeRecentList"));
    listWidget->setSelectionMode(QAbstractItemView::SingleSelection);
    listWidget->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    listWidget->setContextMenuPolicy(Qt::CustomContextMenu);
    listWidget->setIconSize(QSize(128, 72));
    listWidget->setSpacing(4);
    layout->addWidget(listWidget, 1);
    return card;
}
}

WelcomeWorkspaceWidget::WelcomeWorkspaceWidget(QWidget* parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("welcomeWorkspace"));
    setStyleSheet(QStringLiteral(
        "QWidget#welcomeWorkspace { background-color: #f4f7fb; color: #0f172a; }"
        "QLabel#welcomeTitle { color: #0f172a; font-size: 28px; font-weight: 700; }"
        "QLabel#welcomeSubtitle { color: #64748b; font-size: 13px; }"
        "QFrame#welcomeSectionCard { background: #ffffff; border: 1px solid #dbe4ef; border-radius: 10px; }"
        "QLabel#welcomeSectionTitle { color: #1e293b; font-size: 15px; font-weight: 700; }"
        "QPushButton { min-height: 38px; padding: 0 18px; border: 1px solid #cbd5e1; border-radius: 6px; background: #ffffff; color: #1e293b; font-weight: 600; }"
        "QPushButton:hover { background: #eff6ff; border-color: #60a5fa; }"
        "QPushButton#welcomePrimaryButton { background: #2563eb; border-color: #2563eb; color: #ffffff; }"
        "QPushButton#welcomePrimaryButton:hover { background: #1d4ed8; }"
        "QListWidget#welcomeRecentList { border: 0; background: transparent; outline: 0; color: #334155; }"
        "QListWidget#welcomeRecentList::item { background: #f8fafc; border: 1px solid #e2e8f0; border-radius: 6px; padding: 8px 10px; margin: 2px 0; }"
        "QListWidget#welcomeRecentList::item:hover { background: #eff6ff; border-color: #93c5fd; }"
        "QListWidget#welcomeRecentList::item:selected { background: #dbeafe; border-color: #60a5fa; color: #0f172a; }"));

    auto* rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(36, 28, 36, 28);
    rootLayout->setSpacing(18);

    titleLabel_ = new QLabel(this);
    titleLabel_->setObjectName(QStringLiteral("welcomeTitle"));
    subtitleLabel_ = new QLabel(this);
    subtitleLabel_->setObjectName(QStringLiteral("welcomeSubtitle"));
    rootLayout->addWidget(titleLabel_);
    rootLayout->addWidget(subtitleLabel_);

    auto* actionLayout = new QHBoxLayout();
    actionLayout->setSpacing(10);
    openProjectButton_ = new QPushButton(this);
    openProjectButton_->setObjectName(QStringLiteral("welcomePrimaryButton"));
    openDataButton_ = new QPushButton(this);
    addDataButton_ = new QPushButton(this);
    actionLayout->addWidget(openProjectButton_);
    actionLayout->addWidget(openDataButton_);
    actionLayout->addWidget(addDataButton_);
    actionLayout->addStretch(1);
    rootLayout->addLayout(actionLayout);

    sectionsLayout_ = new QGridLayout();
    sectionsLayout_->setHorizontalSpacing(16);
    sectionsLayout_->setVerticalSpacing(16);
    recentProjectsCard_ = createSectionCard(this, recentProjectsLabel_, recentProjectsList_);
    recentDataCard_ = createSectionCard(this, recentDataLabel_, recentDataList_);
    sectionsLayout_->addWidget(recentProjectsCard_, 0, 0);
    sectionsLayout_->addWidget(recentDataCard_, 0, 1);
    sectionsLayout_->setColumnStretch(0, 1);
    sectionsLayout_->setColumnStretch(1, 1);
    rootLayout->addLayout(sectionsLayout_, 1);

    connect(openProjectButton_, &QPushButton::clicked, this, &WelcomeWorkspaceWidget::openProjectRequested);
    connect(openDataButton_, &QPushButton::clicked, this, &WelcomeWorkspaceWidget::openDataRequested);
    connect(addDataButton_, &QPushButton::clicked, this, &WelcomeWorkspaceWidget::addDataRequested);
    connect(recentProjectsList_, &QListWidget::itemDoubleClicked, this, [this](QListWidgetItem* item) {
        if (item != nullptr && !item->data(Qt::UserRole).toString().isEmpty()) {
            emit recentProjectRequested(item->data(Qt::UserRole).toString());
        }
    });
    connect(recentDataList_, &QListWidget::itemDoubleClicked, this, [this](QListWidgetItem* item) {
        if (item != nullptr && !item->data(Qt::UserRole).toString().isEmpty()) {
            emit recentDataRequested(item->data(Qt::UserRole).toString());
        }
    });
    connect(recentProjectsList_, &QListWidget::customContextMenuRequested, this, [this](const QPoint& position) {
        showRecentItemMenu(recentProjectsList_, position, true);
    });
    connect(recentDataList_, &QListWidget::customContextMenuRequested, this, [this](const QPoint& position) {
        showRecentItemMenu(recentDataList_, position, false);
    });

    retranslateUi();
}

void WelcomeWorkspaceWidget::setRecentProjects(const QList<WelcomeWorkspaceItem>& items)
{
    recentProjects_ = items;
    populateList(recentProjectsList_, recentProjects_, tr("No recent projects"));
}

void WelcomeWorkspaceWidget::setRecentDataFiles(const QList<WelcomeWorkspaceItem>& items)
{
    recentDataFiles_ = items;
    populateList(recentDataList_, recentDataFiles_, tr("No recent data"));
}

void WelcomeWorkspaceWidget::retranslateUi()
{
    titleLabel_->setText(tr("Welcome to Power Point Cloud"));
    subtitleLabel_->setText(tr("Continue an inspection project or open point-cloud data to begin."));
    openProjectButton_->setText(tr("Open Project"));
    openDataButton_->setText(tr("Open Data"));
    addDataButton_->setText(tr("Add Data"));
    recentProjectsLabel_->setText(tr("Recent Projects"));
    recentDataLabel_->setText(tr("Recent Data"));
    populateList(recentProjectsList_, recentProjects_, tr("No recent projects"));
    populateList(recentDataList_, recentDataFiles_, tr("No recent data"));
}

void WelcomeWorkspaceWidget::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    if (sectionsLayout_ == nullptr || recentProjectsCard_ == nullptr || recentDataCard_ == nullptr) {
        return;
    }

    const bool stackCards = width() < 900;
    if (stackCards == cardsStacked_) {
        return;
    }
    cardsStacked_ = stackCards;
    sectionsLayout_->removeWidget(recentProjectsCard_);
    sectionsLayout_->removeWidget(recentDataCard_);
    sectionsLayout_->addWidget(recentProjectsCard_, 0, 0);
    sectionsLayout_->addWidget(recentDataCard_, stackCards ? 1 : 0, stackCards ? 0 : 1);
    sectionsLayout_->setColumnStretch(0, 1);
    sectionsLayout_->setColumnStretch(1, stackCards ? 0 : 1);
}

void WelcomeWorkspaceWidget::populateList(
    QListWidget* listWidget,
    const QList<WelcomeWorkspaceItem>& items,
    const QString& emptyText)
{
    listWidget->clear();
    if (items.isEmpty()) {
        auto* item = new QListWidgetItem(emptyText, listWidget);
        item->setFlags(item->flags() & ~Qt::ItemIsEnabled & ~Qt::ItemIsSelectable);
        return;
    }

    for (const WelcomeWorkspaceItem& entry : items) {
        QStringList lines { entry.title };
        if (!entry.details.trimmed().isEmpty()) {
            lines.append(entry.details);
        }
        if (!entry.location.trimmed().isEmpty()) {
            lines.append(entry.location);
        }
        auto* item = new QListWidgetItem(QIcon(QPixmap::fromImage(entry.thumbnail)), lines.join(QLatin1Char('\n')), listWidget);
        item->setData(Qt::UserRole, entry.filePath);
        item->setData(Qt::UserRole + 1, entry.missing);
        item->setToolTip(entry.filePath);
        item->setSizeHint(QSize(100, qMax(104, listWidget->fontMetrics().lineSpacing() * lines.size() + 28)));
        if (entry.missing) {
            item->setForeground(QColor(QStringLiteral("#94a3b8")));
        }
    }
}

void WelcomeWorkspaceWidget::showRecentItemMenu(QListWidget* listWidget, const QPoint& position, bool projectItem)
{
    QListWidgetItem* item = listWidget->itemAt(position);
    if (item == nullptr || item->data(Qt::UserRole).toString().isEmpty()) {
        return;
    }

    const QString filePath = item->data(Qt::UserRole).toString();
    const bool missing = item->data(Qt::UserRole + 1).toBool();
    QMenu menu(this);
    QAction* openAction = menu.addAction(tr("Open"));
    openAction->setEnabled(!missing);
    QAction* appendAction = nullptr;
    if (!projectItem) {
        appendAction = menu.addAction(tr("Add to Current Scene"));
        appendAction->setEnabled(!missing);
    }
    QAction* locateAction = menu.addAction(tr("Open Folder"));
    locateAction->setEnabled(QFileInfo(filePath).absoluteDir().exists());
    menu.addSeparator();
    QAction* removeAction = menu.addAction(tr("Remove from Recent"));

    QAction* chosenAction = menu.exec(listWidget->viewport()->mapToGlobal(position));
    if (chosenAction == openAction) {
        projectItem ? emit recentProjectRequested(filePath) : emit recentDataRequested(filePath);
    } else if (chosenAction == appendAction) {
        emit appendRecentDataRequested(filePath);
    } else if (chosenAction == locateAction) {
        emit locateRecentItemRequested(filePath);
    } else if (chosenAction == removeAction) {
        projectItem ? emit removeRecentProjectRequested(filePath) : emit removeRecentDataRequested(filePath);
    }
}
