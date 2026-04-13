#pragma once

#include "gui/MainWindow.h"

class QDockWidget;

namespace mainwindow_internal
{
inline constexpr int kWindowResizeBorder = 8;

MainWindow::UiLanguage defaultLanguageFromLocale();
bool isSupportedPointCloudFile(const QString& filePath);
void applyDefaultDockWidths(MainWindow* window, QDockWidget* projectDock, QDockWidget* inspectorDock);
}
