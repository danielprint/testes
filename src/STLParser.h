#pragma once

#include "MeshLoader.h"

class STLParser
{
public:
    STLParser() = default;
    MeshBuffer parse(const QString &path, QString *errorMessage) const;

private:
    MeshBuffer parseAscii(QIODevice &device, QString *errorMessage) const;
    MeshBuffer parseBinary(QIODevice &device, QString *errorMessage) const;
};
