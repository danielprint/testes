#pragma once

#include <QOpenGLBuffer>
#include <QOpenGLFunctions_4_1_Core>
#include <QOpenGLVertexArrayObject>
#include <functional>

class GridGizmo
{
public:
    GridGizmo();
    ~GridGizmo();

    void initialize(QOpenGLFunctions_4_1_Core *gl);
    void drawGrid(QOpenGLFunctions_4_1_Core *gl);
    void drawAxes(QOpenGLFunctions_4_1_Core *gl, const std::function<void(int axis)> &configureDraw);

private:
    void createGridGeometry(QOpenGLFunctions_4_1_Core *gl);
    void createAxesGeometry(QOpenGLFunctions_4_1_Core *gl);

    QOpenGLVertexArrayObject m_gridVao;
    QOpenGLBuffer m_gridVbo;
    int m_gridVertexCount = 0;

    QOpenGLVertexArrayObject m_axesVao;
    QOpenGLBuffer m_axesVbo;
    int m_axesVertexCount = 0;

    bool m_initialized = false;
};
