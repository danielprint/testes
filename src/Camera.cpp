#include "Camera.h"

#include <QtMath>

Camera::Camera() = default;

void Camera::setPerspective(float fovY, float aspect, float nearPlane, float farPlane)
{
    m_fovY = fovY;
    m_aspect = aspect;
    m_nearPlane = nearPlane;
    m_farPlane = farPlane;
}

void Camera::resize(int width, int height)
{
    if (height == 0)
        height = 1;
    m_aspect = static_cast<float>(width) / static_cast<float>(height);
}

void Camera::orbit(float deltaYawDeg, float deltaPitchDeg)
{
    m_yaw += deltaYawDeg;
    m_pitch += deltaPitchDeg;
    clampPitch();
}

void Camera::pan(const QVector2D &delta)
{
    QVector3D forward = (target() - position());
    if (!forward.isNull())
        forward.normalize();
    QVector3D right = QVector3D::crossProduct(forward, QVector3D(0, 1, 0));
    if (!right.isNull())
        right.normalize();
    QVector3D up = QVector3D::crossProduct(right, forward);
    if (!up.isNull())
        up.normalize();

    m_target += (-right * delta.x() + up * delta.y());
}

void Camera::dolly(float delta)
{
    m_distance = qMax(1.0f, m_distance + delta);
}

void Camera::focus(const QVector3D &center, float radius)
{
    m_target = center;
    m_distance = qMax(radius * 2.5f, 1.0f);
}

QMatrix4x4 Camera::viewMatrix() const
{
    QMatrix4x4 view;
    QVector3D pos = position();
    view.lookAt(pos, m_target, QVector3D(0, 1, 0));
    return view;
}

QMatrix4x4 Camera::projectionMatrix() const
{
    QMatrix4x4 projection;
    projection.perspective(m_fovY, m_aspect, m_nearPlane, m_farPlane);
    return projection;
}

QVector3D Camera::position() const
{
    const float yawRad = qDegreesToRadians(m_yaw);
    const float pitchRad = qDegreesToRadians(m_pitch);
    QVector3D dir;
    dir.setX(qCos(pitchRad) * qCos(yawRad));
    dir.setY(qSin(pitchRad));
    dir.setZ(qCos(pitchRad) * qSin(yawRad));
    return m_target - dir.normalized() * m_distance;
}

void Camera::setTarget(const QVector3D &target)
{
    m_target = target;
}

void Camera::setDistance(float distance)
{
    m_distance = qMax(1.0f, distance);
}

void Camera::addFlyMovement(const QVector3D &movement)
{
    QVector3D pos = position();
    QVector3D forward = (m_target - pos);
    if (!forward.isNull())
        forward.normalize();
    QVector3D right = QVector3D::crossProduct(forward, QVector3D(0, 1, 0));
    if (!right.isNull())
        right.normalize();
    QVector3D up = QVector3D::crossProduct(right, forward);
    if (!up.isNull())
        up.normalize();

    QVector3D offset = right * movement.x() + up * movement.y() + forward * movement.z();
    m_target += offset;
}

void Camera::clampPitch()
{
    const float limit = 89.0f;
    if (m_pitch > limit)
        m_pitch = limit;
    if (m_pitch < -limit)
        m_pitch = -limit;
}
