#include "gui/WorkspaceThumbnailCache.h"

#include <QBuffer>
#include <QCryptographicHash>
#include <QDateTime>
#include <QFile>
#include <QDir>
#include <QFileInfo>
#include <QLinearGradient>
#include <QPainter>
#include <QSaveFile>
#include <QStandardPaths>

#include <algorithm>

namespace
{
constexpr int kThumbnailWidth = 320;
constexpr int kThumbnailHeight = 180;
constexpr int kThumbnailVersion = 1;
constexpr int kMaximumCacheFiles = 200;
constexpr qint64 kMaximumCacheBytes = 100LL * 1024LL * 1024LL;

QString cacheRoot()
{
    const QString overridePath = qEnvironmentVariable("LAS_VIEWER_THUMBNAIL_CACHE_DIR").trimmed();
    if (!overridePath.isEmpty()) {
        return QDir::fromNativeSeparators(overridePath);
    }
    const QString appDataPath = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    return QDir(appDataPath.isEmpty() ? QDir::tempPath() : appDataPath).filePath(QStringLiteral("thumbnails"));
}

void trimCache()
{
    QDir root(cacheRoot());
    QFileInfoList files;
    for (const QString& subdirectory : { QStringLiteral("projects"), QStringLiteral("datasets") }) {
        files.append(QDir(root.filePath(subdirectory)).entryInfoList(
            { QStringLiteral("*.jpg") }, QDir::Files, QDir::Time | QDir::Reversed));
    }
    std::sort(files.begin(), files.end(), [](const QFileInfo& left, const QFileInfo& right) {
        return left.lastModified() < right.lastModified();
    });

    qint64 totalBytes = 0;
    for (const QFileInfo& fileInfo : files) {
        totalBytes += fileInfo.size();
    }
    while (!files.isEmpty() && (files.size() > kMaximumCacheFiles || totalBytes > kMaximumCacheBytes)) {
        const QFileInfo oldest = files.takeFirst();
        totalBytes -= oldest.size();
        QFile::remove(oldest.absoluteFilePath());
    }
}
}

QImage WorkspaceThumbnailCache::imageFor(const QString& filePath, Kind kind)
{
    const QImage cachedImage(cacheFilePath(filePath, kind));
    return cachedImage.isNull() ? placeholder(filePath, kind) : cachedImage;
}

bool WorkspaceThumbnailCache::save(const QString& filePath, Kind kind, const QImage& source)
{
    if (source.isNull() || filePath.trimmed().isEmpty()) {
        return false;
    }

    const QString outputPath = cacheFilePath(filePath, kind);
    if (!QDir(QFileInfo(outputPath).absolutePath()).mkpath(QStringLiteral("."))) {
        return false;
    }

    QImage thumbnail(kThumbnailWidth, kThumbnailHeight, QImage::Format_RGB32);
    thumbnail.fill(Qt::black);
    QPainter painter(&thumbnail);
    const QImage scaled = source.scaled(thumbnail.size(), Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
    painter.drawImage((thumbnail.width() - scaled.width()) / 2, (thumbnail.height() - scaled.height()) / 2, scaled);
    painter.end();

    QByteArray encoded;
    QBuffer buffer(&encoded);
    if (!buffer.open(QIODevice::WriteOnly) || !thumbnail.save(&buffer, "JPG", 82)) {
        return false;
    }
    QSaveFile outputFile(outputPath);
    if (!outputFile.open(QIODevice::WriteOnly) || outputFile.write(encoded) != encoded.size() || !outputFile.commit()) {
        return false;
    }
    trimCache();
    return true;
}

QImage WorkspaceThumbnailCache::placeholder(const QString& filePath, Kind kind, const QSize& size)
{
    QImage image(size, QImage::Format_RGB32);
    QPainter painter(&image);
    QLinearGradient gradient(0, 0, image.width(), image.height());
    const bool project = kind == Kind::Project;
    gradient.setColorAt(0.0, project ? QColor(QStringLiteral("#1d4ed8")) : QColor(QStringLiteral("#0f766e")));
    gradient.setColorAt(1.0, project ? QColor(QStringLiteral("#60a5fa")) : QColor(QStringLiteral("#5eead4")));
    painter.fillRect(image.rect(), gradient);

    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setPen(QPen(QColor(255, 255, 255, 70), 1.0));
    const int step = qMax(18, image.width() / 12);
    for (int x = -image.height(); x < image.width(); x += step) {
        painter.drawLine(x, 0, x + image.height(), image.height());
    }

    QFont font = painter.font();
    font.setBold(true);
    font.setPixelSize(qMax(22, image.height() / 4));
    painter.setFont(font);
    painter.setPen(Qt::white);
    const QString label = project
        ? QStringLiteral("PROJECT")
        : QFileInfo(filePath).suffix().trimmed().toUpper();
    painter.drawText(image.rect().adjusted(16, 12, -16, -12), Qt::AlignCenter, label.isEmpty() ? QStringLiteral("DATA") : label);
    return image;
}

QString WorkspaceThumbnailCache::cacheFilePath(const QString& filePath, Kind kind)
{
    const QFileInfo fileInfo(filePath);
    const QString identity = QStringLiteral("%1|%2|%3|%4|%5")
        .arg(kThumbnailVersion)
        .arg(kind == Kind::Project ? QStringLiteral("project") : QStringLiteral("data"))
        .arg(QDir::fromNativeSeparators(fileInfo.absoluteFilePath()).toLower())
        .arg(fileInfo.exists() ? fileInfo.size() : -1)
        .arg(fileInfo.exists() ? fileInfo.lastModified().toMSecsSinceEpoch() : -1);
    const QString hash = QString::fromLatin1(QCryptographicHash::hash(identity.toUtf8(), QCryptographicHash::Sha256).toHex());
    const QString subdirectory = kind == Kind::Project ? QStringLiteral("projects") : QStringLiteral("datasets");
    return QDir(cacheRoot()).filePath(subdirectory + QLatin1Char('/') + hash + QStringLiteral(".jpg"));
}
