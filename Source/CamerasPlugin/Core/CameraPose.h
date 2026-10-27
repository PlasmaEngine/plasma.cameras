#pragma once

#include <CamerasPlugin/Core/CameraTypes.h>
#include <Foundation/Math/Quat.h>
#include <Foundation/Math/Vec2.h>
#include <Foundation/Math/Vec3.h>

/// \brief Everything a camera view decides about the camera for one frame.
///
/// Uses the engine's camera convention: the camera looks along its local +X, +Y is left and +Z is up.
struct PL_CAMERAS_DLL plCameraPose
{
  plVec3 m_vPosition = plVec3::MakeZero();
  plQuat m_qRotation = plQuat::MakeIdentity();

  /// Degrees, in whatever axis the camera component's mode fixes (vertical by default).
  float m_fFieldOfView = 70.0f;

  /// Distance to keep in focus. Zero or less leaves the camera's own focus settings alone.
  float m_fFocusDistance = 0.0f;

  /// Near clip distance. Zero or less leaves the camera's own setting alone.
  float m_fNearPlane = 0.0f;

  plVec3 GetForward() const { return m_qRotation * plVec3::MakeAxisX(); }
  plVec3 GetLeft() const { return m_qRotation * plVec3::MakeAxisY(); }
  plVec3 GetUp() const { return m_qRotation * plVec3::MakeAxisZ(); }

  /// \brief Yaw (around +Z) and pitch (positive looks up) of the forward direction.
  void GetYawPitch(plAngle& out_yaw, plAngle& out_pitch) const;

  /// \brief Builds a rotation from yaw (around +Z), pitch (positive looks up) and roll (positive tilts the horizon clockwise).
  static plQuat MakeRotation(plAngle yaw, plAngle pitch, plAngle roll = plAngle());

  /// \brief A rotation that looks along \a vDirection with the horizon level.
  static plQuat MakeLookRotation(const plVec3& vDirection);

  /// \brief A pose at \a vPosition looking at \a vTarget.
  static plCameraPose MakeLookAt(const plVec3& vPosition, const plVec3& vTarget, float fFieldOfView);

  /// \brief Blends two poses: positions and focus linearly, rotation along the shortest arc, field of view linearly.
  static plCameraPose Blend(const plCameraPose& a, const plCameraPose& b, float fT);
};

/// \brief Projects world points into a camera's normalized device coordinates (-1..1, +Y up) and back into directions.
///
/// The framing modifiers work in these coordinates: rule-of-thirds points sit at +-1/3.
struct PL_CAMERAS_DLL plCameraProjection
{
  float m_fTanHalfX = 1.0f;
  float m_fTanHalfY = 0.5625f;

  /// \brief Projection for a perspective camera. \a bFovIsHorizontal says which axis \a fFovDegrees fixes.
  static plCameraProjection Make(float fFovDegrees, float fAspectRatio, bool bFovIsHorizontal);

  /// \brief The point's screen position, or false when it is behind the camera.
  bool Project(const plCameraPose& pose, const plVec3& vWorldPoint, plVec2& out_vNdc) const;

  /// \brief The camera-space direction (forward, left, up) through a screen position.
  plVec3 GetViewDirection(const plVec2& vNdc) const;
};
