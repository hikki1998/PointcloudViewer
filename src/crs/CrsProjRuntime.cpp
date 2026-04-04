#include "crs/CrsProjRuntime.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QVector>

#ifdef LASVIEWERCRS_HAS_PROJ
#include <proj.h>
#endif

namespace lasviewer::crs
{
#ifdef LASVIEWERCRS_HAS_PROJ
void configureProjSearchPaths(PJ_CONTEXT* context)
{
    if (context == nullptr) {
        return;
    }

    const QString explicitProjData = qEnvironmentVariable("PROJ_DATA");
    if (!explicitProjData.isEmpty() && QFileInfo::exists(explicitProjData)) {
        return;
    }

    const QString explicitProjLib = qEnvironmentVariable("PROJ_LIB");
    if (!explicitProjLib.isEmpty() && QFileInfo::exists(explicitProjLib)) {
        return;
    }

    const QString applicationDir = QCoreApplication::applicationDirPath();
    if (applicationDir.isEmpty()) {
        return;
    }

    QStringList candidatePaths;
    const QString bundledProjShare = QDir(applicationDir).filePath(QStringLiteral("proj9/share"));
    if (QFileInfo::exists(QDir(bundledProjShare).filePath(QStringLiteral("proj.db")))) {
        candidatePaths.append(QDir::toNativeSeparators(bundledProjShare));
    }

    const QString fallbackProjShare = QDir(applicationDir).filePath(QStringLiteral("share/proj"));
    if (QFileInfo::exists(QDir(fallbackProjShare).filePath(QStringLiteral("proj.db")))) {
        candidatePaths.append(QDir::toNativeSeparators(fallbackProjShare));
    }

    if (candidatePaths.isEmpty()) {
        return;
    }

    QVector<QByteArray> encodedPaths;
    encodedPaths.reserve(candidatePaths.size());
    QVector<const char*> rawPaths;
    rawPaths.reserve(candidatePaths.size());
    for (const QString& candidatePath : candidatePaths) {
        encodedPaths.append(QDir::toNativeSeparators(candidatePath).toUtf8());
    }
    for (const QByteArray& encodedPath : encodedPaths) {
        rawPaths.append(encodedPath.constData());
    }

    proj_context_set_search_paths(context, rawPaths.size(), rawPaths.constData());
}

QString projContextErrorMessage(PJ_CONTEXT* context, const QString& fallbackMessage)
{
    if (context == nullptr) {
        return fallbackMessage;
    }

    const int errorCode = proj_context_errno(context);
    const char* errorText = proj_context_errno_string(context, errorCode);
    if (errorText == nullptr || errorText[0] == '\0') {
        return fallbackMessage;
    }

    if (fallbackMessage.trimmed().isEmpty()) {
        return QString::fromUtf8(errorText);
    }

    return QCoreApplication::translate("CrsProjRuntime", "%1 (%2)")
        .arg(fallbackMessage, QString::fromUtf8(errorText));
}
#endif
}
