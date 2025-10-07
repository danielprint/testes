#pragma once

#include <QString>
#include <QVector>
#include <QVector3D>

struct MeshBuffer
{
    QVector<QVector3D> positions;
    QVector<QVector3D> normals;
    QVector<unsigned int> indices;
    bool hasNormals = false;
};

class MeshLoader
{
public:
    MeshLoader();
    MeshBuffer load(const QString &path, QString *errorMessage) const;
};
