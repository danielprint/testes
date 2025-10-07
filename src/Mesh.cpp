#include "Mesh.h"

#include <QtMath>
#include <algorithm>

Mesh::Mesh()
    : m_vbo(QOpenGLBuffer::VertexBuffer)
    , m_ebo(QOpenGLBuffer::IndexBuffer)
{
}

Mesh::~Mesh()
{
    clear();
}

void Mesh::clear()
{
    m_positions.clear();
    m_indices.clear();
    m_normals.clear();
    m_originalNormals.clear();
    m_hasSourceNormals = false;
    m_uploaded = false;

    if (m_vao.isCreated())
        m_vao.destroy();
    if (m_vbo.isCreated())
        m_vbo.destroy();
    if (m_ebo.isCreated())
        m_ebo.destroy();
}

bool Mesh::isValid() const
{
    return !m_positions.isEmpty() && !m_indices.isEmpty();
}

void Mesh::setData(const QVector<QVector3D> &positions,
                   const QVector<QVector3D> &normals,
                   const QVector<unsigned int> &indices,
                   bool hasNormals)
{
    m_positions = positions;
    m_indices = indices;
    m_originalNormals = normals;
    m_normals = normals;
    m_hasSourceNormals = hasNormals && normals.size() == positions.size();

    if (!m_hasSourceNormals) {
        computeSmoothNormals();
    } else {
        if (m_normals.size() != m_positions.size())
            m_normals = QVector<QVector3D>(m_positions.size(), QVector3D(0.0f, 1.0f, 0.0f));
    }

    updateBounds();
    m_uploaded = false;
}

void Mesh::updateBounds()
{
    if (m_positions.isEmpty()) {
        m_minBounds = QVector3D();
        m_maxBounds = QVector3D();
        return;
    }

    m_minBounds = m_positions.first();
    m_maxBounds = m_positions.first();
    for (const QVector3D &p : m_positions) {
        m_minBounds.setX(qMin(m_minBounds.x(), p.x()));
        m_minBounds.setY(qMin(m_minBounds.y(), p.y()));
        m_minBounds.setZ(qMin(m_minBounds.z(), p.z()));
        m_maxBounds.setX(qMax(m_maxBounds.x(), p.x()));
        m_maxBounds.setY(qMax(m_maxBounds.y(), p.y()));
        m_maxBounds.setZ(qMax(m_maxBounds.z(), p.z()));
    }
}

void Mesh::computeSmoothNormals()
{
    m_normals.resize(m_positions.size());
    std::fill(m_normals.begin(), m_normals.end(), QVector3D());

    for (int i = 0; i + 2 < m_indices.size(); i += 3) {
        const unsigned int ia = m_indices.at(i);
        const unsigned int ib = m_indices.at(i + 1);
        const unsigned int ic = m_indices.at(i + 2);
        if (ia >= static_cast<unsigned int>(m_positions.size()) ||
            ib >= static_cast<unsigned int>(m_positions.size()) ||
            ic >= static_cast<unsigned int>(m_positions.size()))
            continue;

        const QVector3D &a = m_positions.at(ia);
        const QVector3D &b = m_positions.at(ib);
        const QVector3D &c = m_positions.at(ic);
        QVector3D n = QVector3D::crossProduct(b - a, c - a);
        if (!n.isNull())
            n.normalize();

        m_normals[ia] += n;
        m_normals[ib] += n;
        m_normals[ic] += n;
    }

    for (QVector3D &n : m_normals) {
        if (!n.isNull())
            n.normalize();
        else
            n = QVector3D(0.0f, 1.0f, 0.0f);
    }

    m_uploaded = false;
}

void Mesh::restoreOriginalNormals()
{
    if (!m_originalNormals.isEmpty() && m_originalNormals.size() == m_positions.size()) {
        m_normals = m_originalNormals;
    } else if (m_normals.isEmpty()) {
        computeSmoothNormals();
    }
    m_uploaded = false;
}

void Mesh::upload(QOpenGLFunctions_4_1_Core *gl)
{
    if (!gl || !isValid())
        return;

    if (m_normals.size() != m_positions.size())
        computeSmoothNormals();

    if (!m_vao.isCreated())
        m_vao.create();
    QOpenGLVertexArrayObject::Binder vaoBinder(&m_vao);

    if (!m_vbo.isCreated())
        m_vbo.create();
    m_vbo.bind();
    m_vbo.setUsagePattern(QOpenGLBuffer::StaticDraw);

    struct Vertex
    {
        QVector3D position;
        QVector3D normal;
    };

    QVector<Vertex> vertexData;
    vertexData.resize(m_positions.size());
    for (int i = 0; i < m_positions.size(); ++i) {
        vertexData[i].position = m_positions.at(i);
        vertexData[i].normal = (i < m_normals.size()) ? m_normals.at(i) : QVector3D(0, 1, 0);
    }

    m_vbo.allocate(vertexData.constData(), vertexData.size() * sizeof(Vertex));

    if (!m_ebo.isCreated())
        m_ebo.create();
    m_ebo.bind();
    m_ebo.setUsagePattern(QOpenGLBuffer::StaticDraw);
    m_ebo.allocate(m_indices.constData(), m_indices.size() * sizeof(unsigned int));

    gl->glEnableVertexAttribArray(0);
    gl->glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<void *>(offsetof(Vertex, position)));
    gl->glEnableVertexAttribArray(1);
    gl->glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<void *>(offsetof(Vertex, normal)));

    m_uploaded = true;
}

void Mesh::draw(QOpenGLFunctions_4_1_Core *gl) const
{
    if (!m_uploaded || !gl)
        return;

    QOpenGLVertexArrayObject::Binder vaoBinder(const_cast<QOpenGLVertexArrayObject *>(&m_vao));
    gl->glDrawElements(GL_TRIANGLES, m_indices.size(), GL_UNSIGNED_INT, nullptr);
}

quint64 Mesh::triangleCount() const
{
    return static_cast<quint64>(m_indices.size()) / 3;
}
