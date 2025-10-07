#include "GridGizmo.h"

#include <QVector3D>

GridGizmo::GridGizmo()
    : m_gridVbo(QOpenGLBuffer::VertexBuffer)
    , m_axesVbo(QOpenGLBuffer::VertexBuffer)
{
}

GridGizmo::~GridGizmo()
{
    if (m_gridVbo.isCreated())
        m_gridVbo.destroy();
    if (m_axesVbo.isCreated())
        m_axesVbo.destroy();
    if (m_gridVao.isCreated())
        m_gridVao.destroy();
    if (m_axesVao.isCreated())
        m_axesVao.destroy();
}

void GridGizmo::initialize(QOpenGLFunctions_4_1_Core *gl)
{
    if (m_initialized || !gl)
        return;

    createGridGeometry(gl);
    createAxesGeometry(gl);
    m_initialized = true;
}

void GridGizmo::createGridGeometry(QOpenGLFunctions_4_1_Core *gl)
{
    if (!m_gridVao.isCreated())
        m_gridVao.create();
    QOpenGLVertexArrayObject::Binder vaoBinder(&m_gridVao);

    if (!m_gridVbo.isCreated())
        m_gridVbo.create();
    m_gridVbo.bind();
    m_gridVbo.setUsagePattern(QOpenGLBuffer::StaticDraw);

    const int divisions = 20;
    const float spacing = 10.0f;
    const float halfSize = divisions * spacing * 0.5f;
    QVector<QVector3D> vertices;
    vertices.reserve((divisions + 1) * 4);
    for (int i = 0; i <= divisions; ++i) {
        float offset = -halfSize + i * spacing;
        vertices.append(QVector3D(offset, 0.0f, -halfSize));
        vertices.append(QVector3D(offset, 0.0f, halfSize));
        vertices.append(QVector3D(-halfSize, 0.0f, offset));
        vertices.append(QVector3D(halfSize, 0.0f, offset));
    }
    m_gridVertexCount = vertices.size();
    m_gridVbo.allocate(vertices.constData(), vertices.size() * sizeof(QVector3D));

    gl->glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(QVector3D), nullptr);
    gl->glEnableVertexAttribArray(0);
}

void GridGizmo::createAxesGeometry(QOpenGLFunctions_4_1_Core *gl)
{
    if (!m_axesVao.isCreated())
        m_axesVao.create();
    QOpenGLVertexArrayObject::Binder vaoBinder(&m_axesVao);

    if (!m_axesVbo.isCreated())
        m_axesVbo.create();
    m_axesVbo.bind();
    m_axesVbo.setUsagePattern(QOpenGLBuffer::StaticDraw);

    const float axisLength = 100.0f;
    QVector<QVector3D> vertices = {
        QVector3D(0.0f, 0.0f, 0.0f), QVector3D(axisLength, 0.0f, 0.0f), // X
        QVector3D(0.0f, 0.0f, 0.0f), QVector3D(0.0f, axisLength, 0.0f), // Y
        QVector3D(0.0f, 0.0f, 0.0f), QVector3D(0.0f, 0.0f, axisLength)  // Z
    };

    m_axesVertexCount = vertices.size();
    m_axesVbo.allocate(vertices.constData(), vertices.size() * sizeof(QVector3D));

    gl->glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(QVector3D), nullptr);
    gl->glEnableVertexAttribArray(0);
}

void GridGizmo::drawGrid(QOpenGLFunctions_4_1_Core *gl)
{
    if (!m_initialized || !gl)
        return;
    QOpenGLVertexArrayObject::Binder vaoBinder(&m_gridVao);
    gl->glDrawArrays(GL_LINES, 0, m_gridVertexCount);
}

void GridGizmo::drawAxes(QOpenGLFunctions_4_1_Core *gl, const std::function<void(int axis)> &configureDraw)
{
    if (!m_initialized || !gl)
        return;
    QOpenGLVertexArrayObject::Binder vaoBinder(&m_axesVao);
    for (int axis = 0; axis < 3; ++axis) {
        if (configureDraw)
            configureDraw(axis);
        gl->glDrawArrays(GL_LINES, axis * 2, 2);
    }
}
