#include "domain/TowerFileInterop.h"

#include <QFile>
#include <QObject>
#include <QTextStream>

namespace
{
QStringList parseCsvLine(const QString& line)
{
    QStringList fields;
    QString currentField;
    bool insideQuotes = false;

    for (int index = 0; index < line.size(); ++index) {
        const QChar currentChar = line.at(index);
        if (currentChar == QLatin1Char('"')) {
            if (insideQuotes && index + 1 < line.size() && line.at(index + 1) == QLatin1Char('"')) {
                currentField.append(QLatin1Char('"'));
                ++index;
                continue;
            }
            insideQuotes = !insideQuotes;
            continue;
        }
        if (currentChar == QLatin1Char(',') && !insideQuotes) {
            fields.append(currentField.trimmed());
            currentField.clear();
            continue;
        }
        currentField.append(currentChar);
    }
    fields.append(currentField.trimmed());
    return fields;
}

QString escapeCsvField(const QString& value)
{
    QString escaped = value;
    escaped.replace(QLatin1Char('"'), QStringLiteral("\"\""));
    return QStringLiteral("\"%1\"").arg(escaped);
}
}

bool importTowerLiTowerFile(
    const QString& filePath,
    QList<TowerRecord>* towers,
    QString* errorMessage)
{
    if (towers == nullptr) {
        if (errorMessage != nullptr) {
            *errorMessage = QObject::tr("Tower output buffer is null.");
        }
        return false;
    }

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        if (errorMessage != nullptr) {
            *errorMessage = QObject::tr("Failed to open tower file.");
        }
        return false;
    }

    QTextStream stream(&file);
    stream.setCodec("UTF-8");

    if (stream.atEnd()) {
        if (errorMessage != nullptr) {
            *errorMessage = QObject::tr("Tower file is empty.");
        }
        return false;
    }

    const QString header = stream.readLine().trimmed();
    if (header.compare(QStringLiteral("Index,X,Y,Z,Type,Name"), Qt::CaseSensitive) != 0) {
        if (errorMessage != nullptr) {
            *errorMessage = QObject::tr("Tower file header is invalid. Expected: Index,X,Y,Z,Type,Name");
        }
        return false;
    }

    QList<TowerRecord> importedTowers;
    int lineNumber = 1;
    while (!stream.atEnd()) {
        ++lineNumber;
        const QString line = stream.readLine();
        if (line.trimmed().isEmpty()) {
            continue;
        }

        const QStringList fields = parseCsvLine(line);
        if (fields.size() != 6) {
            if (errorMessage != nullptr) {
                *errorMessage = QObject::tr("Invalid tower row at line %1.").arg(lineNumber);
            }
            return false;
        }

        bool ok = false;
        const int indexValue = fields.at(0).toInt(&ok);
        if (!ok) {
            if (errorMessage != nullptr) {
                *errorMessage = QObject::tr("Invalid index at line %1.").arg(lineNumber);
            }
            return false;
        }

        TowerRecord towerRecord;
        towerRecord.index = indexValue;
        towerRecord.point.x = fields.at(1).toDouble(&ok);
        if (!ok) {
            if (errorMessage != nullptr) {
                *errorMessage = QObject::tr("Invalid X coordinate at line %1.").arg(lineNumber);
            }
            return false;
        }
        towerRecord.point.y = fields.at(2).toDouble(&ok);
        if (!ok) {
            if (errorMessage != nullptr) {
                *errorMessage = QObject::tr("Invalid Y coordinate at line %1.").arg(lineNumber);
            }
            return false;
        }
        towerRecord.point.z = fields.at(3).toDouble(&ok);
        if (!ok) {
            if (errorMessage != nullptr) {
                *errorMessage = QObject::tr("Invalid Z coordinate at line %1.").arg(lineNumber);
            }
            return false;
        }

        towerRecord.towerType = towerTypeFromLiTowerString(fields.at(4));
        towerRecord.name = fields.at(5).trimmed();
        if (towerRecord.name.isEmpty()) {
            if (errorMessage != nullptr) {
                *errorMessage = QObject::tr("Tower name is empty at line %1.").arg(lineNumber);
            }
            return false;
        }
        importedTowers.append(towerRecord);
    }

    *towers = importedTowers;
    return true;
}

bool exportTowerLiTowerFile(
    const QString& filePath,
    const QList<TowerRecord>& towers,
    QString* errorMessage)
{
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        if (errorMessage != nullptr) {
            *errorMessage = QObject::tr("Failed to create tower file.");
        }
        return false;
    }

    QTextStream stream(&file);
    stream.setCodec("UTF-8");
    stream << "Index,X,Y,Z,Type,Name\n";

    for (const TowerRecord& towerRecord : towers) {
        stream
            << towerRecord.index << ','
            << QString::number(towerRecord.point.x, 'f', 6) << ','
            << QString::number(towerRecord.point.y, 'f', 6) << ','
            << QString::number(towerRecord.point.z, 'f', 6) << ','
            << escapeCsvField(towerTypeToLiTowerString(towerRecord.towerType)) << ','
            << escapeCsvField(towerRecord.name)
            << '\n';
    }

    return true;
}
