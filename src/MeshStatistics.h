#pragma once

#include <QVector3D>
#include <QString>

struct MeshStatistics
{
    QString fileName;
    quint64 triangleCount = 0;
    QVector3D minBounds;
    QVector3D maxBounds;
    QVector3D size;
    bool hasNormals = false;
};
