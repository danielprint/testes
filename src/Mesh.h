#pragma once

#include <QOpenGLBuffer>
#include <QOpenGLFunctions_4_1_Core>
#include <QOpenGLVertexArrayObject>
#include <QVector>
#include <QVector3D>

class Mesh
{
public:
    Mesh();
    ~Mesh();

    void clear();
    bool isValid() const;

    void setData(const QVector<QVector3D> &positions,
                 const QVector<QVector3D> &normals,
                 const QVector<unsigned int> &indices,
                 bool hasNormals);

    void upload(QOpenGLFunctions_4_1_Core *gl);
    void draw(QOpenGLFunctions_4_1_Core *gl) const;

    quint64 triangleCount() const;
    const QVector3D &minBounds() const { return m_minBounds; }
    const QVector3D &maxBounds() const { return m_maxBounds; }
    QVector3D size() const { return m_maxBounds - m_minBounds; }

    bool hasSourceNormals() const { return m_hasSourceNormals; }
    void computeSmoothNormals();
    void restoreOriginalNormals();

    const QVector<QVector3D> &positions() const { return m_positions; }
    const QVector<unsigned int> &indices() const { return m_indices; }
    const QVector<QVector3D> &normals() const { return m_normals; }

private:
    void updateBounds();

    QVector<QVector3D> m_positions;
    QVector<unsigned int> m_indices;
    QVector<QVector3D> m_normals;
    QVector<QVector3D> m_originalNormals;

    bool m_hasSourceNormals = false;
    bool m_uploaded = false;

    QVector3D m_minBounds;
    QVector3D m_maxBounds;

    QOpenGLBuffer m_vbo;
    QOpenGLBuffer m_ebo;
    QOpenGLVertexArrayObject m_vao;
};
