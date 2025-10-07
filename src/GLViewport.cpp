#include "GLViewport.h"

#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFileInfo>
#include <QGuiApplication>
#include <QImage>
#include <QKeyEvent>
#include <QMimeData>
#include <QMouseEvent>
#include <QVector2D>
#include <QVector>
#include <QUrl>
#include <QWheelEvent>
#include <QtMath>

namespace
{
constexpr float kOrbitSpeed = 0.35f;
constexpr float kPanSpeed = 0.2f;
constexpr float kDollySpeed = 0.5f;
constexpr float kFlySpeed = 150.0f;
constexpr float kGamma = 2.2f;
} // namespace

GLViewport::GLViewport(QWidget *parent)
    : QOpenGLWidget(parent)
    , m_bboxVbo(QOpenGLBuffer::VertexBuffer)
{
    setFocusPolicy(Qt::StrongFocus);
    setAcceptDrops(true);
    updateLightDirection();
    m_updateTimer.setInterval(16);
    connect(&m_updateTimer, &QTimer::timeout, this, [this]() {
        if (!m_elapsedTimer.isValid())
            m_elapsedTimer.start();
        const float deltaSeconds = m_elapsedTimer.restart() / 1000.0f;
        handleFlyMode(deltaSeconds);
        update();
    });
    m_updateTimer.start();
    m_fpsTimer.start();
}

GLViewport::~GLViewport()
{
    makeCurrent();
    m_bboxVbo.destroy();
    m_bboxVao.destroy();
    m_phongProgram.removeAllShaders();
    m_colorProgram.removeAllShaders();
    doneCurrent();
}

void GLViewport::initializeGL()
{
    initializeOpenGLFunctions();
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_MULTISAMPLE);
    glClearColor(0.1f, 0.12f, 0.15f, 1.0f);

    const char *phongVertex = R"(
        #version 410 core
        layout(location = 0) in vec3 aPosition;
        layout(location = 1) in vec3 aNormal;
        uniform mat4 uModel;
        uniform mat4 uView;
        uniform mat4 uProjection;
        uniform mat3 uNormalMatrix;
        out vec3 vNormal;
        out vec3 vWorldPos;
        void main() {
            vec4 worldPos = uModel * vec4(aPosition, 1.0);
            vWorldPos = worldPos.xyz;
            vNormal = uNormalMatrix * aNormal;
            gl_Position = uProjection * uView * worldPos;
        }
    )";

    const char *phongFragment = R"(
        #version 410 core
        in vec3 vNormal;
        in vec3 vWorldPos;
        uniform vec3 uLightDirection;
        uniform vec3 uCameraPos;
        uniform vec3 uBaseColor;
        uniform int uUseFaceNormals;
        uniform float uGamma;
        out vec4 fragColor;
        void main() {
            vec3 normal = normalize(vNormal);
            if (uUseFaceNormals == 1) {
                vec3 d1 = dFdx(vWorldPos);
                vec3 d2 = dFdy(vWorldPos);
                normal = normalize(cross(d1, d2));
            }
            vec3 lightDir = normalize(-uLightDirection);
            float diff = max(dot(normal, lightDir), 0.0);
            vec3 viewDir = normalize(uCameraPos - vWorldPos);
            vec3 reflectDir = reflect(-lightDir, normal);
            float spec = pow(max(dot(viewDir, reflectDir), 0.0), 32.0);
            vec3 color = uBaseColor * (0.15 + diff) + vec3(0.4) * spec;
            color = pow(max(color, vec3(0.0)), vec3(1.0 / max(uGamma, 0.0001)));
            fragColor = vec4(color, 1.0);
        }
    )";

    const char *colorVertex = R"(
        #version 410 core
        layout(location = 0) in vec3 aPosition;
        uniform mat4 uMvp;
        void main() {
            gl_Position = uMvp * vec4(aPosition, 1.0);
        }
    )";

    const char *colorFragment = R"(
        #version 410 core
        uniform vec3 uColor;
        out vec4 fragColor;
        void main() {
            fragColor = vec4(uColor, 1.0);
        }
    )";

    if (!m_phongProgram.addShaderFromSourceCode(QOpenGLShader::Vertex, phongVertex) ||
        !m_phongProgram.addShaderFromSourceCode(QOpenGLShader::Fragment, phongFragment) ||
        !m_phongProgram.link()) {
        emit loadFailed(tr("Failed to compile Phong shader: %1").arg(m_phongProgram.log()));
    }

    if (!m_colorProgram.addShaderFromSourceCode(QOpenGLShader::Vertex, colorVertex) ||
        !m_colorProgram.addShaderFromSourceCode(QOpenGLShader::Fragment, colorFragment) ||
        !m_colorProgram.link()) {
        emit loadFailed(tr("Failed to compile color shader: %1").arg(m_colorProgram.log()));
    }

    m_grid.initialize(this);
    m_bboxVao.create();
    m_bboxVbo.create();

    m_camera.setPerspective(45.0f, static_cast<float>(width()) / qMax(1.0f, static_cast<float>(height())), 1.0f, 10000.0f);
    emit cameraDistanceChanged(m_camera.distance());
}

void GLViewport::resizeGL(int w, int h)
{
    m_camera.resize(w, h);
}

void GLViewport::paintGL()
{
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    if (m_backfaceCulling)
        glEnable(GL_CULL_FACE);
    else
        glDisable(GL_CULL_FACE);

    QMatrix4x4 model;
    model.translate(m_translation);
    model.rotate(m_rotation.x(), 1.0f, 0.0f, 0.0f);
    model.rotate(m_rotation.y(), 0.0f, 1.0f, 0.0f);
    model.rotate(m_rotation.z(), 0.0f, 0.0f, 1.0f);
    model.scale(m_scale);

    const QMatrix4x4 view = m_camera.viewMatrix();
    const QMatrix4x4 projection = m_camera.projectionMatrix();

    if (m_mesh.isValid() && m_phongProgram.isLinked()) {
        if (m_shadingMode == ShadingMode::Shaded || m_shadingMode == ShadingMode::ShadedWireframe) {
            m_phongProgram.bind();
            updateCameraUniforms(m_phongProgram, model, view, projection);
            m_phongProgram.setUniformValue("uLightDirection", m_lightDirection.normalized());
            m_phongProgram.setUniformValue("uCameraPos", m_camera.position());
            m_phongProgram.setUniformValue("uBaseColor", QVector3D(0.7f, 0.72f, 0.75f));
            m_phongProgram.setUniformValue("uUseFaceNormals", m_faceNormals ? 1 : 0);
            m_phongProgram.setUniformValue("uGamma", kGamma);
            m_mesh.draw(this);
            m_phongProgram.release();
        }

        if (m_shadingMode == ShadingMode::Wireframe || m_shadingMode == ShadingMode::ShadedWireframe) {
            glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
            glDisable(GL_CULL_FACE);
            m_colorProgram.bind();
            m_colorProgram.setUniformValue("uMvp", projection * view * model);
            m_colorProgram.setUniformValue("uColor", QVector3D(0.05f, 0.9f, 0.9f));
            m_mesh.draw(this);
            m_colorProgram.release();
            glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
            if (m_backfaceCulling)
                glEnable(GL_CULL_FACE);
        }

        if (m_shadingMode == ShadingMode::Shaded && m_backfaceCulling)
            glEnable(GL_CULL_FACE);

        if (m_bboxVertexCount > 0 && m_colorProgram.isLinked()) {
            m_colorProgram.bind();
            drawBoundingBox(projection * view * model, QVector3D(0.85f, 0.35f, 0.1f));
            m_colorProgram.release();
        }
    }

    if (m_gridVisible && m_colorProgram.isLinked()) {
        m_colorProgram.bind();
        QMatrix4x4 gridModel;
        gridModel.setToIdentity();
        m_colorProgram.setUniformValue("uMvp", projection * view * gridModel);
        m_colorProgram.setUniformValue("uColor", QVector3D(0.3f, 0.3f, 0.3f));
        m_grid.drawGrid(this);
        m_colorProgram.release();
    }

    if (m_axesVisible && m_colorProgram.isLinked()) {
        m_colorProgram.bind();
        m_colorProgram.setUniformValue("uMvp", projection * view);
        m_grid.drawAxes(this, [this](int axis) {
            switch (axis) {
            case 0:
                m_colorProgram.setUniformValue("uColor", QVector3D(0.9f, 0.2f, 0.2f));
                break;
            case 1:
                m_colorProgram.setUniformValue("uColor", QVector3D(0.2f, 0.9f, 0.2f));
                break;
            default:
                m_colorProgram.setUniformValue("uColor", QVector3D(0.2f, 0.4f, 0.9f));
                break;
            }
        });
        m_colorProgram.release();
    }

    updateFps();
}

bool GLViewport::loadMesh(const QString &path, QString *errorMessage)
{
    MeshBuffer buffer = m_loader.load(path, errorMessage);
    if (buffer.positions.isEmpty() || buffer.indices.isEmpty()) {
        if (errorMessage && errorMessage->isEmpty())
            *errorMessage = tr("No geometry found in %1").arg(path);
        return false;
    }

    m_mesh.clear();
    m_mesh.setData(buffer.positions, buffer.normals, buffer.indices, buffer.hasNormals);
    if (m_recomputeNormals || !buffer.hasNormals)
        m_mesh.computeSmoothNormals();
    else
        m_mesh.restoreOriginalNormals();

    makeCurrent();
    m_mesh.upload(this);
    updateBoundingBoxBuffer();
    doneCurrent();

    updateStatistics(path);

    const QVector3D center = (m_mesh.minBounds() + m_mesh.maxBounds()) * 0.5f;
    const float radius = m_mesh.size().length() * 0.5f;
    m_camera.focus(center, qMax(radius, 1.0f));
    emit cameraDistanceChanged(m_camera.distance());

    resetModelTransform();
    update();
    return true;
}

bool GLViewport::saveScreenshot(const QString &path)
{
    const QImage image = grabFramebuffer();
    if (image.isNull())
        return false;
    return image.save(path, "PNG");
}

void GLViewport::setGridVisible(bool visible)
{
    if (m_gridVisible == visible)
        return;
    m_gridVisible = visible;
    update();
}

void GLViewport::setAxesVisible(bool visible)
{
    if (m_axesVisible == visible)
        return;
    m_axesVisible = visible;
    update();
}

void GLViewport::setBackfaceCullingEnabled(bool enabled)
{
    if (m_backfaceCulling == enabled)
        return;
    m_backfaceCulling = enabled;
    update();
}

void GLViewport::setRecomputeNormals(bool enabled)
{
    if (m_recomputeNormals == enabled)
        return;
    m_recomputeNormals = enabled;
    if (m_mesh.isValid()) {
        if (enabled)
            m_mesh.computeSmoothNormals();
        else
            m_mesh.restoreOriginalNormals();
        makeCurrent();
        m_mesh.upload(this);
        doneCurrent();
        updateStatistics(m_loadedFilePath);
        update();
    }
}

void GLViewport::setFaceNormalsEnabled(bool enabled)
{
    if (m_faceNormals == enabled)
        return;
    m_faceNormals = enabled;
    update();
}

void GLViewport::setShadingMode(ShadingMode mode)
{
    if (m_shadingMode == mode)
        return;
    m_shadingMode = mode;
    update();
}

void GLViewport::setModelTransform(const QVector3D &translation, const QVector3D &rotation, const QVector3D &scale)
{
    m_translation = translation;
    m_rotation = rotation;
    m_scale = QVector3D(qMax(scale.x(), 0.0001f), qMax(scale.y(), 0.0001f), qMax(scale.z(), 0.0001f));
    syncTransformToUi();
    update();
}

void GLViewport::resetModelTransform()
{
    m_translation = QVector3D(0, 0, 0);
    m_rotation = QVector3D(0, 0, 0);
    m_scale = QVector3D(1, 1, 1);
    syncTransformToUi();
    update();
}

void GLViewport::mousePressEvent(QMouseEvent *event)
{
    m_lastMousePos = event->pos();
    if (event->button() == Qt::LeftButton)
        m_leftButton = true;
    if (event->button() == Qt::MiddleButton)
        m_middleButton = true;
    if (event->button() == Qt::RightButton)
        m_rightButton = true;
    setFocus();
}

void GLViewport::mouseMoveEvent(QMouseEvent *event)
{
    const QPoint delta = event->pos() - m_lastMousePos;
    if (m_leftButton) {
        m_camera.orbit(delta.x() * kOrbitSpeed, -delta.y() * kOrbitSpeed);
        emit cameraDistanceChanged(m_camera.distance());
    } else if (m_middleButton) {
        const float distanceFactor = m_camera.distance() * 0.01f;
        QVector2D panDelta(-delta.x() * kPanSpeed * distanceFactor, delta.y() * kPanSpeed * distanceFactor);
        m_camera.pan(panDelta);
    } else if (m_rightButton) {
        m_lightAzimuth += delta.x() * 0.5f;
        m_lightElevation = qBound(-89.0f, m_lightElevation - delta.y() * 0.5f, 89.0f);
        updateLightDirection();
    }
    m_lastMousePos = event->pos();
    update();
}

void GLViewport::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton)
        m_leftButton = false;
    if (event->button() == Qt::MiddleButton)
        m_middleButton = false;
    if (event->button() == Qt::RightButton)
        m_rightButton = false;
}

void GLViewport::wheelEvent(QWheelEvent *event)
{
    const float delta = static_cast<float>(event->angleDelta().y()) / 120.0f;
    m_camera.dolly(-delta * kDollySpeed * m_camera.distance());
    emit cameraDistanceChanged(m_camera.distance());
    update();
}

void GLViewport::keyPressEvent(QKeyEvent *event)
{
    if (event->isAutoRepeat())
        return;

    switch (event->key()) {
    case Qt::Key_W:
        m_moveForward = true;
        break;
    case Qt::Key_S:
        m_moveBackward = true;
        break;
    case Qt::Key_A:
        m_moveLeft = true;
        break;
    case Qt::Key_D:
        m_moveRight = true;
        break;
    case Qt::Key_Q:
        m_moveDown = true;
        break;
    case Qt::Key_E:
        m_moveUp = true;
        break;
    case Qt::Key_F:
        m_flyMode = !m_flyMode;
        break;
    default:
        break;
    }
}

void GLViewport::keyReleaseEvent(QKeyEvent *event)
{
    if (event->isAutoRepeat())
        return;

    switch (event->key()) {
    case Qt::Key_W:
        m_moveForward = false;
        break;
    case Qt::Key_S:
        m_moveBackward = false;
        break;
    case Qt::Key_A:
        m_moveLeft = false;
        break;
    case Qt::Key_D:
        m_moveRight = false;
        break;
    case Qt::Key_Q:
        m_moveDown = false;
        break;
    case Qt::Key_E:
        m_moveUp = false;
        break;
    default:
        break;
    }
}

void GLViewport::dragEnterEvent(QDragEnterEvent *event)
{
    if (event->mimeData()->hasUrls()) {
        const QList<QUrl> urls = event->mimeData()->urls();
        if (!urls.isEmpty()) {
            const QString path = urls.first().toLocalFile();
            if (path.endsWith(QLatin1String(".stl"), Qt::CaseInsensitive)) {
                event->acceptProposedAction();
                return;
            }
        }
    }
    event->ignore();
}

void GLViewport::dropEvent(QDropEvent *event)
{
    if (!event->mimeData()->hasUrls()) {
        event->ignore();
        return;
    }
    const QList<QUrl> urls = event->mimeData()->urls();
    if (urls.isEmpty()) {
        event->ignore();
        return;
    }
    const QString path = urls.first().toLocalFile();
    QString error;
    if (!loadMesh(path, &error) && !error.isEmpty())
        emit loadFailed(error);
    event->acceptProposedAction();
}

void GLViewport::updateCameraUniforms(QOpenGLShaderProgram &program, const QMatrix4x4 &modelMatrix, const QMatrix4x4 &view, const QMatrix4x4 &projection)
{
    program.setUniformValue("uModel", modelMatrix);
    program.setUniformValue("uView", view);
    program.setUniformValue("uProjection", projection);
    program.setUniformValue("uNormalMatrix", modelMatrix.normalMatrix());
}

void GLViewport::updateBoundingBoxBuffer()
{
    if (!m_mesh.isValid()) {
        m_bboxVertexCount = 0;
        return;
    }

    const QVector3D min = m_mesh.minBounds();
    const QVector3D max = m_mesh.maxBounds();

    QVector<QVector3D> vertices = {
        {min.x(), min.y(), min.z()}, {max.x(), min.y(), min.z()},
        {max.x(), min.y(), min.z()}, {max.x(), min.y(), max.z()},
        {max.x(), min.y(), max.z()}, {min.x(), min.y(), max.z()},
        {min.x(), min.y(), max.z()}, {min.x(), min.y(), min.z()},

        {min.x(), max.y(), min.z()}, {max.x(), max.y(), min.z()},
        {max.x(), max.y(), min.z()}, {max.x(), max.y(), max.z()},
        {max.x(), max.y(), max.z()}, {min.x(), max.y(), max.z()},
        {min.x(), max.y(), max.z()}, {min.x(), max.y(), min.z()},

        {min.x(), min.y(), min.z()}, {min.x(), max.y(), min.z()},
        {max.x(), min.y(), min.z()}, {max.x(), max.y(), min.z()},
        {max.x(), min.y(), max.z()}, {max.x(), max.y(), max.z()},
        {min.x(), min.y(), max.z()}, {min.x(), max.y(), max.z()}
    };

    m_bboxVertexCount = vertices.size();

    if (!m_bboxVao.isCreated())
        m_bboxVao.create();
    QOpenGLVertexArrayObject::Binder vaoBinder(&m_bboxVao);

    if (!m_bboxVbo.isCreated())
        m_bboxVbo.create();
    m_bboxVbo.bind();
    m_bboxVbo.setUsagePattern(QOpenGLBuffer::DynamicDraw);
    m_bboxVbo.allocate(vertices.constData(), vertices.size() * sizeof(QVector3D));

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(QVector3D), nullptr);
}

void GLViewport::drawBoundingBox(const QMatrix4x4 &mvp, const QVector3D &color)
{
    if (m_bboxVertexCount <= 0)
        return;
    QOpenGLVertexArrayObject::Binder vaoBinder(&m_bboxVao);
    m_colorProgram.setUniformValue("uMvp", mvp);
    m_colorProgram.setUniformValue("uColor", color);
    glDrawArrays(GL_LINES, 0, m_bboxVertexCount);
}

void GLViewport::updateFps()
{
    ++m_frameCounter;
    if (m_fpsTimer.elapsed() > 1000) {
        const float fps = static_cast<float>(m_frameCounter) * 1000.0f / m_fpsTimer.elapsed();
        emit fpsChanged(fps);
        m_frameCounter = 0;
        m_fpsTimer.restart();
    }
}

void GLViewport::updateStatistics(const QString &filePath)
{
    m_loadedFilePath = filePath;
    m_stats.fileName = QFileInfo(filePath).fileName();
    m_stats.triangleCount = m_mesh.triangleCount();
    m_stats.minBounds = m_mesh.minBounds();
    m_stats.maxBounds = m_mesh.maxBounds();
    m_stats.size = m_mesh.size();
    m_stats.hasNormals = m_mesh.hasSourceNormals() && !m_recomputeNormals;
    emit meshInfoChanged(m_stats);
}

void GLViewport::syncTransformToUi()
{
    emit transformChanged(m_translation, m_rotation, m_scale);
}

void GLViewport::handleFlyMode(float deltaSeconds)
{
    if (!m_flyMode)
        return;

    QVector3D direction;
    if (m_moveForward)
        direction.setZ(direction.z() + 1.0f);
    if (m_moveBackward)
        direction.setZ(direction.z() - 1.0f);
    if (m_moveLeft)
        direction.setX(direction.x() - 1.0f);
    if (m_moveRight)
        direction.setX(direction.x() + 1.0f);
    if (m_moveUp)
        direction.setY(direction.y() + 1.0f);
    if (m_moveDown)
        direction.setY(direction.y() - 1.0f);

    if (direction.isNull())
        return;

    const Qt::KeyboardModifiers mods = QGuiApplication::keyboardModifiers();
    float speed = kFlySpeed;
    if (mods & Qt::ShiftModifier)
        speed *= 2.5f;

    direction.normalize();
    m_camera.addFlyMovement(direction * speed * deltaSeconds);
    emit cameraDistanceChanged(m_camera.distance());
}

void GLViewport::updateLightDirection()
{
    const float azimuthRad = qDegreesToRadians(m_lightAzimuth);
    const float elevationRad = qDegreesToRadians(m_lightElevation);
    m_lightDirection.setX(qCos(elevationRad) * qCos(azimuthRad));
    m_lightDirection.setY(qSin(elevationRad));
    m_lightDirection.setZ(qCos(elevationRad) * qSin(azimuthRad));
    if (m_lightDirection.isNull())
        m_lightDirection = QVector3D(0.0f, -1.0f, 0.0f);
}
