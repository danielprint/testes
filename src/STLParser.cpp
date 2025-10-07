#include "STLParser.h"

#include <QByteArray>
#include <QDataStream>
#include <QFile>
#include <QIODevice>
#include <QRegularExpression>
#include <QString>
#include <QStringList>
#include <QTextStream>
#include <QObject>
#include <QVector>

MeshBuffer STLParser::parse(const QString &path, QString *errorMessage) const
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        if (errorMessage)
            *errorMessage = QObject::tr("Unable to open file %1").arg(path);
        return {};
    }

    QByteArray header = file.peek(512);
    const bool headerLooksAscii = header.trimmed().startsWith("solid");
    const bool containsNull = header.contains('\0');

    file.seek(0);
    if (headerLooksAscii && !containsNull) {
        return parseAscii(file, errorMessage);
    }

    file.seek(0);
    return parseBinary(file, errorMessage);
}

MeshBuffer STLParser::parseAscii(QIODevice &device, QString *errorMessage) const
{
    MeshBuffer buffer;
    QTextStream stream(&device);
    stream.setCodec("UTF-8");

    QVector<QVector3D> facetVertices;
    facetVertices.reserve(3);
    QVector3D facetNormal(0, 1, 0);

    while (!stream.atEnd()) {
        QString line = stream.readLine().trimmed();
        if (line.startsWith(QLatin1String("facet"), Qt::CaseInsensitive)) {
            QStringList parts = line.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);
            if (parts.size() >= 5) {
                facetNormal = QVector3D(parts.at(2).toFloat(), parts.at(3).toFloat(), parts.at(4).toFloat());
                if (!facetNormal.isNull())
                    facetNormal.normalize();
            }
        } else if (line.startsWith(QLatin1String("vertex"), Qt::CaseInsensitive)) {
            QStringList parts = line.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);
            if (parts.size() >= 4) {
                QVector3D v(parts.at(1).toFloat(), parts.at(2).toFloat(), parts.at(3).toFloat());
                facetVertices.append(v);
            }
        } else if (line.startsWith(QLatin1String("endfacet"), Qt::CaseInsensitive)) {
            if (facetVertices.size() == 3) {
                const unsigned int baseIndex = buffer.positions.size();
                for (int i = 0; i < 3; ++i) {
                    buffer.positions.append(facetVertices.at(i));
                    buffer.normals.append(facetNormal);
                    buffer.indices.append(baseIndex + i);
                }
            }
            facetVertices.clear();
        }
    }

    buffer.hasNormals = !buffer.normals.isEmpty();

    if (buffer.positions.isEmpty() && errorMessage)
        *errorMessage = QObject::tr("No geometry found in STL file.");

    return buffer;
}

MeshBuffer STLParser::parseBinary(QIODevice &device, QString *errorMessage) const
{
    MeshBuffer buffer;
    if (!device.isOpen()) {
        if (!device.open(QIODevice::ReadOnly)) {
            if (errorMessage)
                *errorMessage = QObject::tr("Unable to read STL stream.");
            return buffer;
        }
    }

    if (!device.seek(80)) {
        if (errorMessage)
            *errorMessage = QObject::tr("Invalid STL header.");
        return buffer;
    }

    QDataStream stream(&device);
    stream.setByteOrder(QDataStream::LittleEndian);

    quint32 triangleCount = 0;
    stream >> triangleCount;
    if (stream.status() != QDataStream::Ok) {
        if (errorMessage)
            *errorMessage = QObject::tr("Unable to read triangle count.");
        return buffer;
    }

    buffer.positions.reserve(triangleCount * 3);
    buffer.normals.reserve(triangleCount * 3);
    buffer.indices.reserve(triangleCount * 3);

    for (quint32 i = 0; i < triangleCount; ++i) {
        float nx, ny, nz;
        float ax, ay, az;
        float bx, by, bz;
        float cx, cy, cz;
        quint16 attribute = 0;

        stream >> nx >> ny >> nz;
        stream >> ax >> ay >> az;
        stream >> bx >> by >> bz;
        stream >> cx >> cy >> cz;
        stream >> attribute;

        if (stream.status() != QDataStream::Ok) {
            if (errorMessage)
                *errorMessage = QObject::tr("Unexpected end of STL file.");
            buffer.positions.clear();
            buffer.normals.clear();
            buffer.indices.clear();
            return buffer;
        }

        QVector3D normal(nx, ny, nz);
        if (!normal.isNull())
            normal.normalize();
        const unsigned int baseIndex = buffer.positions.size();
        const QVector3D vertices[3] = {
            QVector3D(ax, ay, az),
            QVector3D(bx, by, bz),
            QVector3D(cx, cy, cz)};
        for (int v = 0; v < 3; ++v) {
            buffer.positions.append(vertices[v]);
            buffer.normals.append(normal);
            buffer.indices.append(baseIndex + v);
        }
        Q_UNUSED(attribute);
    }

    buffer.hasNormals = !buffer.normals.isEmpty();
    return buffer;
}
