#include "gui/MainWindowInternal.h"

#include <QDockWidget>
#include <QFileInfo>
#include <QLocale>

namespace mainwindow_internal
{
MainWindow::UiLanguage defaultLanguageFromLocale()
{
    const QString localeName = QLocale::system().name().toLower();
    return localeName.startsWith(QStringLiteral("zh"))
        ? MainWindow::UiLanguage::Chinese
        : MainWindow::UiLanguage::English;
}

bool isSupportedPointCloudFile(const QString& filePath)
{
    const QFileInfo fileInfo(filePath);
    if (!fileInfo.exists() || !fileInfo.isFile()) {
        return false;
    }

    const QString suffix = fileInfo.suffix().toLower();
    return suffix == QStringLiteral("las") || suffix == QStringLiteral("laz");
}

void applyDefaultDockWidths(MainWindow* window, QDockWidget* projectDock, QDockWidget* inspectorDock)
{
    if (window != nullptr && projectDock != nullptr && inspectorDock != nullptr) {
        window->resizeDocks({ projectDock, inspectorDock }, { 320, 380 }, Qt::Horizontal);
    }
}
}
