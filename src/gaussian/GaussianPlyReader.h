#pragma once

#include <QString>

#include "gaussian/GaussianModel.h"

class GaussianPlyReader
{
public:
    bool read(const QString& filePath, GaussianModel* model, QString* errorMessage = nullptr) const;
};
