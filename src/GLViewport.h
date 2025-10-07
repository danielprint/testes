#pragma once

#include "Camera.h"
#include "GridGizmo.h"
#include "Mesh.h"
#include "MeshLoader.h"
#include "MeshStatistics.h"

#include <QElapsedTimer>
#include <QMatrix4x4>
#include <QOpenGLBuffer>
#include <QOpenGLShaderProgram>
#include <QOpenGLVertexArrayObject>
#include <QOpenGLWidget>
#include <QPointer>
#include <QTimer>

class GLViewport : public QOpenGLWidget, protected QOpenGLFunctions_4_1_Core
{
    Q_OBJECT

public:
    enum class ShadingMode
    {
        Shaded = 0,
        Wireframe,
        ShadedWireframe
    };

    explicit GLViewport(QWidget *parent = nullptr);
    ~GLViewport() override;

    bool loadMesh(const QString &path, QString *errorMessage);
    bool saveScreenshot(const QString &path);

    void setGridVisible(bool visible);
    void setAxesVisible(bool visible);
    void setBackfaceCullingEnabled(bool enabled);
    void setRecomputeNormals(bool enabled);
    void setFaceNormalsEnabled(bool enabled);
    void setShadingMode(ShadingMode mode);

    void setModelTransform(const QVector3D &translation, const QVector3D &rotation, const QVector3D &scale);
    void resetModelTransform();

signals:
    void meshInfoChanged(const MeshStatistics &stats);
    void loadFailed(const QString &message);
    void cameraDistanceChanged(float distance);
    void fpsChanged(float fps);
    void transformChanged(const QVector3D &translation, const QVector3D &rotation, const QVector3D &scale);

protected:
    void initializeGL() override;
    void resizeGL(int w, int h) override;
    void paintGL() override;

    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void keyReleaseEvent(QKeyEvent *event) override;
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dropEvent(QDropEvent *event) override;

private:
    void updateCameraUniforms(QOpenGLShaderProgram &program, const QMatrix4x4 &modelMatrix, const QMatrix4x4 &view, const QMatrix4x4 &projection);
    void updateBoundingBoxBuffer();
    void drawBoundingBox(const QMatrix4x4 &mvp, const QVector3D &color);
    void updateFps();
    void updateStatistics(const QString &filePath);
    void syncTransformToUi();
    void handleFlyMode(float deltaSeconds);
    void updateLightDirection();

    MeshLoader m_loader;
    Mesh m_mesh;
    MeshStatistics m_stats;
    QString m_loadedFilePath;

    Camera m_camera;
    GridGizmo m_grid;

    bool m_gridVisible = true;
    bool m_axesVisible = true;
    bool m_backfaceCulling = false;
    bool m_recomputeNormals = false;
    bool m_faceNormals = false;
    ShadingMode m_shadingMode = ShadingMode::Shaded;

    QVector3D m_translation = QVector3D(0, 0, 0);
    QVector3D m_rotation = QVector3D(0, 0, 0);
    QVector3D m_scale = QVector3D(1, 1, 1);

    QOpenGLShaderProgram m_phongProgram;
    QOpenGLShaderProgram m_colorProgram;

    QOpenGLBuffer m_bboxVbo;
    QOpenGLVertexArrayObject m_bboxVao;
    int m_bboxVertexCount = 0;

    QTimer m_updateTimer;
    QElapsedTimer m_elapsedTimer;
    QElapsedTimer m_fpsTimer;
    int m_frameCounter = 0;

    QPoint m_lastMousePos;
    bool m_leftButton = false;
    bool m_middleButton = false;
    bool m_rightButton = false;

    bool m_flyMode = false;
    QVector3D m_flyVelocity;
    bool m_moveForward = false;
    bool m_moveBackward = false;
    bool m_moveLeft = false;
    bool m_moveRight = false;
    bool m_moveUp = false;
    bool m_moveDown = false;

    QVector3D m_lightDirection = QVector3D(-0.4f, -1.0f, -0.6f);
    float m_lightAzimuth = -45.0f;
    float m_lightElevation = -35.0f;
};
