#pragma once

#include <CamerasPlugin/Core/CameraPose.h>

/// \brief The maths of balanced framing: where to put and how to turn a camera so subjects land on screen points.
///
/// Works in normalized device coordinates (-1..1, +Y up); rule-of-thirds points sit at +-1/3. All rotations are
/// roll-free (the horizon stays level).
struct PL_CAMERAS_DLL plCameraFramingSolver
{
  /// \brief The roll-free rotation that puts \a vSubject exactly on \a vAnchor, seen from \a vCamera.
  static plQuat AimAt(const plVec3& vCamera, const plVec3& vSubject, const plVec2& vAnchor, const plCameraProjection& projection);

  struct TwoSubjectResult
  {
    plVec3 m_vPosition = plVec3::MakeZero();
    plQuat m_qRotation = plQuat::MakeIdentity();

    /// How far the second subject ended up from its anchor, in NDC units. Zero means it was framed exactly.
    float m_fError = 0.0f;

    /// False when no camera position within the distance limits can frame both; the result is the closest one.
    bool m_bExact = false;
  };

  /// \brief Frames \a vA on \a vAnchorA and \a vB on \a vAnchorB.
  ///
  /// Every camera position that sees A and B the right angle apart lies on one surface around them (a spindle
  /// torus with A and B on its axis). The solver keeps A pinned exactly, takes the distance to A closest to the
  /// distance from \a vPreferred, and searches the angle around the A-B axis for the roll-free position nearest
  /// \a vPreferred where B lands on its anchor. The distance to A stays within [\a fMinDistance, \a fMaxDistance].
  static TwoSubjectResult FrameTwo(const plVec3& vA, const plVec3& vB, const plVec2& vAnchorA, const plVec2& vAnchorB, const plCameraProjection& projection,
    const plVec3& vPreferred, float fMinDistance, float fMaxDistance);

  /// \brief Applies dead-zone / soft-zone framing to one axis.
  ///
  /// \a fOffset is the subject's distance from its anchor. Returns the correction to apply this frame: nothing inside
  /// the dead zone, everything beyond the soft zone, and \a fSoftFollow of the rest.
  static float ZoneCorrection(float fOffset, float fDeadZone, float fSoftZone, float fSoftFollow);
};
