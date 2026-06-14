#pragma once

#include <QDockWidget>
#include <QUrl>

class QLabel;
class QProgressBar;
class QShowEvent;
class QToolButton;

#ifdef LAS_VIEWER_HAS_WEBENGINE_DOCK
class QWebEngineView;
#endif

class WebPageDock final : public QDockWidget
{
    Q_OBJECT

public:
    explicit WebPageDock(const QUrl& pageUrl, QWidget* parent = nullptr);

    QUrl pageUrl() const;
    bool webEngineAvailable() const;
    void setPageUrl(const QUrl& pageUrl);
    void reloadPage();
    void retranslateUi();

protected:
    void showEvent(QShowEvent* event) override;

private:
    void createUi();
    void updateStatusText(const QString& text, bool isError);

    QUrl pageUrl_;
    QLabel* statusLabel_ = nullptr;
    QProgressBar* progressBar_ = nullptr;
    QToolButton* reloadButton_ = nullptr;
    QToolButton* openExternalButton_ = nullptr;
    QLabel* unavailableLabel_ = nullptr;
    bool loadedOnce_ = false;

#ifdef LAS_VIEWER_HAS_WEBENGINE_DOCK
    QWebEngineView* webView_ = nullptr;
#endif
};
