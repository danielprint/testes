#pragma once

#include <QMatrix4x4>
#include <QVector2D>
#include <QVector3D>

class Camera
{
public:
    Camera();

    void setPerspective(float fovY, float aspect, float nearPlane, float farPlane);
    void resize(int width, int height);

    void orbit(float deltaYawDeg, float deltaPitchDeg);
    void pan(const QVector2D &delta);
    void dolly(float delta);

    void focus(const QVector3D &center, float radius);

    QMatrix4x4 viewMatrix() const;
    QMatrix4x4 projectionMatrix() const;

    QVector3D position() const;
    QVector3D target() const { return m_target; }
    float distance() const { return m_distance; }

    void setTarget(const QVector3D &target);
    void setDistance(float distance);

    void enableFlyMode(bool enabled) { m_flyMode = enabled; }
    bool flyMode() const { return m_flyMode; }

    void addFlyMovement(const QVector3D &movement);

private:
    void clampPitch();

    float m_yaw = -45.0f;
    float m_pitch = -30.0f;
    QVector3D m_target = QVector3D(0, 0, 0);
    float m_distance = 250.0f;

    float m_fovY = 45.0f;
    float m_aspect = 1.0f;
    float m_nearPlane = 1.0f;
    float m_farPlane = 5000.0f;

    bool m_flyMode = false;
};
