#include "gui/WebPageDock.h"

#include <QDesktopServices>
#include <QHBoxLayout>
#include <QLabel>
#include <QProgressBar>
#include <QShowEvent>
#include <QStyle>
#include <QToolButton>
#include <QVBoxLayout>

#ifdef LAS_VIEWER_HAS_WEBENGINE_DOCK
#include <QWebEngineView>
#endif

WebPageDock::WebPageDock(const QUrl& pageUrl, QWidget* parent)
    : QDockWidget(parent)
    , pageUrl_(pageUrl)
{
    setObjectName(QStringLiteral("webPageDock"));
    setFeatures(
        QDockWidget::DockWidgetClosable
        | QDockWidget::DockWidgetMovable
        | QDockWidget::DockWidgetFloatable);
    setAllowedAreas(Qt::AllDockWidgetAreas);
    createUi();
    retranslateUi();
}

QUrl WebPageDock::pageUrl() const
{
    return pageUrl_;
}

bool WebPageDock::webEngineAvailable() const
{
#ifdef LAS_VIEWER_HAS_WEBENGINE_DOCK
    return true;
#else
    return false;
#endif
}

void WebPageDock::setPageUrl(const QUrl& pageUrl)
{
    pageUrl_ = pageUrl;
    reloadPage();
}

void WebPageDock::reloadPage()
{
    loadedOnce_ = true;
#ifdef LAS_VIEWER_HAS_WEBENGINE_DOCK
    if (webView_ != nullptr && pageUrl_.isValid()) {
        progressBar_->setVisible(true);
        progressBar_->setValue(0);
        updateStatusText(tr("Loading %1").arg(pageUrl_.toString()), false);
        webView_->load(pageUrl_);
    }
#else
    updateStatusText(tr("Qt WebEngineWidgets is not available in this Qt build."), true);
#endif
}

void WebPageDock::showEvent(QShowEvent* event)
{
    QDockWidget::showEvent(event);
    if (!loadedOnce_) {
        reloadPage();
    }
}

void WebPageDock::retranslateUi()
{
    setWindowTitle(tr("Web Panel"));
    if (reloadButton_ != nullptr) {
        reloadButton_->setText(tr("Reload"));
        reloadButton_->setToolTip(tr("Reload the embedded web page"));
    }
    if (openExternalButton_ != nullptr) {
        openExternalButton_->setText(tr("Open"));
        openExternalButton_->setToolTip(tr("Open the web page in the system browser"));
    }
    if (!webEngineAvailable()) {
        updateStatusText(tr("Qt WebEngineWidgets is not available in this Qt build."), true);
    }
    if (unavailableLabel_ != nullptr) {
        unavailableLabel_->setText(tr(
            "Embedded browser support requires the Qt WebEngineWidgets module. "
            "Install a Qt build that includes Qt WebEngine, then rebuild this application."));
    }
}

void WebPageDock::createUi()
{
    auto* content = new QWidget(this);
    auto* layout = new QVBoxLayout(content);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(6);

    auto* toolRow = new QWidget(content);
    auto* toolLayout = new QHBoxLayout(toolRow);
    toolLayout->setContentsMargins(0, 0, 0, 0);
    toolLayout->setSpacing(6);

    reloadButton_ = new QToolButton(toolRow);
    reloadButton_->setIcon(style()->standardIcon(QStyle::SP_BrowserReload));
    reloadButton_->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    connect(reloadButton_, &QToolButton::clicked, this, &WebPageDock::reloadPage);

    openExternalButton_ = new QToolButton(toolRow);
    openExternalButton_->setIcon(style()->standardIcon(QStyle::SP_DialogOpenButton));
    openExternalButton_->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    connect(openExternalButton_, &QToolButton::clicked, this, [this]() {
        if (pageUrl_.isValid()) {
            QDesktopServices::openUrl(pageUrl_);
        }
    });

    statusLabel_ = new QLabel(toolRow);
    statusLabel_->setTextFormat(Qt::PlainText);
    statusLabel_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    statusLabel_->setStyleSheet(QStringLiteral(
        "QLabel {"
        "color: #334155;"
        "padding: 2px 6px;"
        "}"));

    progressBar_ = new QProgressBar(toolRow);
    progressBar_->setRange(0, 100);
    progressBar_->setTextVisible(false);
    progressBar_->setFixedWidth(96);
    progressBar_->setVisible(false);

    toolLayout->addWidget(reloadButton_);
    toolLayout->addWidget(openExternalButton_);
    toolLayout->addWidget(statusLabel_);
    toolLayout->addWidget(progressBar_);
    layout->addWidget(toolRow);

#ifdef LAS_VIEWER_HAS_WEBENGINE_DOCK
    webView_ = new QWebEngineView(content);
    connect(webView_, &QWebEngineView::loadStarted, this, [this]() {
        progressBar_->setVisible(true);
        progressBar_->setValue(0);
        updateStatusText(tr("Loading %1").arg(pageUrl_.toString()), false);
    });
    connect(webView_, &QWebEngineView::loadProgress, progressBar_, &QProgressBar::setValue);
    connect(webView_, &QWebEngineView::loadFinished, this, [this](bool ok) {
        progressBar_->setVisible(false);
        updateStatusText(
            ok ? tr("Loaded %1").arg(pageUrl_.toString()) : tr("Failed to load %1").arg(pageUrl_.toString()),
            !ok);
    });
    layout->addWidget(webView_, 1);
#else
    unavailableLabel_ = new QLabel(content);
    unavailableLabel_->setWordWrap(true);
    unavailableLabel_->setAlignment(Qt::AlignCenter);
    unavailableLabel_->setText(tr(
        "Embedded browser support requires the Qt WebEngineWidgets module. "
        "Install a Qt build that includes Qt WebEngine, then rebuild this application."));
    unavailableLabel_->setStyleSheet(QStringLiteral(
        "QLabel {"
        "background-color: #f8fafc;"
        "border: 1px solid #cbd5e1;"
        "border-radius: 8px;"
        "color: #0f172a;"
        "padding: 18px;"
        "}"));
    layout->addWidget(unavailableLabel_, 1);
#endif

    setWidget(content);
}

void WebPageDock::updateStatusText(const QString& text, bool isError)
{
    if (statusLabel_ == nullptr) {
        return;
    }

    statusLabel_->setText(text);
    statusLabel_->setStyleSheet(QStringLiteral(
        "QLabel {"
        "color: %1;"
        "padding: 2px 6px;"
        "}").arg(isError ? QStringLiteral("#b91c1c") : QStringLiteral("#334155")));
}
