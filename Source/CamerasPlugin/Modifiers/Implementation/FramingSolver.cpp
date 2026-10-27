#include <CamerasPlugin/CamerasPCH.h>

#include <CamerasPlugin/Modifiers/FramingSolver.h>

plQuat plCameraFramingSolver::AimAt(const plVec3& vCamera, const plVec3& vSubject, const plVec2& vAnchor, const plCameraProjection& projection)
{
  plVec3 u = vSubject - vCamera;
  if (u.NormalizeIfNotZero(plVec3::MakeAxisX()).Failed())
    return plQuat::MakeIdentity();

  // The anchor's ray in camera space (forward, left, up). Pitch then yaw must carry it onto u.
  const plVec3 c = projection.GetViewDirection(vAnchor);

  // After pitching by p the ray's height is c.x*sin(p) + c.z*cos(p) = R*sin(p + phi); solve for the subject's height.
  // Two pitches give that height; take the upright one (|p| < 90 degrees) nearest the horizon.
  const float R = plMath::Sqrt(c.x * c.x + c.z * c.z);
  const float fPhi = plMath::ATan2(c.z, c.x).GetRadian();
  const float fAsin = plMath::ASin(plMath::Clamp(u.z / plMath::Max(R, 0.0001f), -1.0f, 1.0f)).GetRadian();
  const float fMaxPitch = plAngle::MakeFromDegree(89.0f).GetRadian();

  auto wrap = [](float f)
  {
    while (f > plMath::Pi<float>())
      f -= 2.0f * plMath::Pi<float>();
    while (f < -plMath::Pi<float>())
      f += 2.0f * plMath::Pi<float>();
    return f;
  };

  float fPitch = wrap(fAsin - fPhi);
  const float fOther = wrap(plMath::Pi<float>() - fAsin - fPhi);

  if (plMath::Abs(fPitch) > fMaxPitch || (plMath::Abs(fOther) <= fMaxPitch && plMath::Abs(fOther) < plMath::Abs(fPitch)))
    fPitch = fOther;

  fPitch = plMath::Clamp(fPitch, -fMaxPitch, fMaxPitch);

  // the ray's horizontal direction after pitching, then the yaw that turns it onto the subject
  const float fX = c.x * plMath::Cos(plAngle::MakeFromRadian(fPitch)) - c.z * plMath::Sin(plAngle::MakeFromRadian(fPitch));
  float fYaw = plMath::ATan2(u.y, u.x).GetRadian() - plMath::ATan2(c.y, fX).GetRadian();

  // A few corrective steps absorb what the clamp and float error leave: each moves the subject's projection onto
  // the anchor, measured in angles through the projection.
  plCameraPose pose;
  pose.m_vPosition = vCamera;

  for (plUInt32 i = 0; i < 3; ++i)
  {
    pose.m_qRotation = plCameraPose::MakeRotation(plAngle::MakeFromRadian(fYaw), plAngle::MakeFromRadian(fPitch));

    plVec2 vNdc;
    if (!projection.Project(pose, vSubject, vNdc))
      break;

    const plVec2 vError = vNdc - vAnchor;
    if (vError.GetLengthSquared() < 1e-12f)
      break;

    fYaw -= plMath::ATan(vNdc.x * projection.m_fTanHalfX).GetRadian() - plMath::ATan(vAnchor.x * projection.m_fTanHalfX).GetRadian();
    fPitch = plMath::Clamp(fPitch + plMath::ATan(vNdc.y * projection.m_fTanHalfY).GetRadian() - plMath::ATan(vAnchor.y * projection.m_fTanHalfY).GetRadian(), -fMaxPitch, fMaxPitch);
  }

  return plCameraPose::MakeRotation(plAngle::MakeFromRadian(fYaw), plAngle::MakeFromRadian(fPitch));
}

namespace
{
  struct TorusFrame
  {
    plVec3 m_vA;
    plVec3 m_vAxis;  ///< unit A -> B
    plVec3 m_vE1;    ///< unit, perpendicular to the axis, toward the preferred position
    plVec3 m_vE2;
    float m_fDistance = 0.0f;   ///< |AC|
    float m_fAngleAtA = 0.0f;   ///< angle between the axis and AC

    plVec3 GetPosition(float fTheta) const
    {
      const plVec3 vRadial = m_vE1 * plMath::Cos(plAngle::MakeFromRadian(fTheta)) + m_vE2 * plMath::Sin(plAngle::MakeFromRadian(fTheta));
      return m_vA + (m_vAxis * plMath::Cos(plAngle::MakeFromRadian(m_fAngleAtA)) + vRadial * plMath::Sin(plAngle::MakeFromRadian(m_fAngleAtA))) * m_fDistance;
    }
  };

  /// Gameplay cameras sit between slightly below and well above their subject; outside this they look straight up or
  /// down at it, where no level-horizon framing exists.
  constexpr float s_fMinElevation = -0.436f; // -25 degrees
  constexpr float s_fMaxElevation = 1.222f;  //  70 degrees

  float ElevationPenalty(const plVec3& vCamera, const plVec3& vA)
  {
    const plVec3 v = vCamera - vA;
    const float fElevation = plMath::ATan2(v.z, v.GetAsVec2().GetLength()).GetRadian();

    if (fElevation < s_fMinElevation)
      return 10.0f + (s_fMinElevation - fElevation);

    if (fElevation > s_fMaxElevation)
      return 10.0f + (fElevation - s_fMaxElevation);

    return 0.0f;
  }

  float ErrorAt(const TorusFrame& frame, float fTheta, const plVec3& vA, const plVec3& vB, const plVec2& vAnchorA, const plVec2& vAnchorB,
    const plCameraProjection& projection, plCameraPose& out_pose)
  {
    out_pose.m_vPosition = frame.GetPosition(fTheta);
    out_pose.m_qRotation = plCameraFramingSolver::AimAt(out_pose.m_vPosition, vA, vAnchorA, projection);

    const float fPenalty = ElevationPenalty(out_pose.m_vPosition, vA);
    if (fPenalty > 0.0f)
      return fPenalty;

    plVec2 vNdc;
    if (!projection.Project(out_pose, vB, vNdc))
      return 100.0f;

    return (vNdc - vAnchorB).GetLength();
  }
} // namespace

plCameraFramingSolver::TwoSubjectResult plCameraFramingSolver::FrameTwo(const plVec3& vA, const plVec3& vB, const plVec2& vAnchorA, const plVec2& vAnchorB,
  const plCameraProjection& projection, const plVec3& vPreferred, float fMinDistance, float fMaxDistance)
{
  TwoSubjectResult result;

  const plVec3 vRayA = projection.GetViewDirection(vAnchorA);
  const plVec3 vRayB = projection.GetViewDirection(vAnchorB);
  const float fAlpha = plMath::ACos(plMath::Clamp(vRayA.Dot(vRayB), -1.0f, 1.0f)).GetRadian();

  plVec3 vAB = vB - vA;
  const float fD = vAB.GetLength();

  plVec3 vToPreferred = vPreferred - vA;
  const float fPreferred = plMath::Clamp(vToPreferred.GetLength(), fMinDistance, fMaxDistance);

  // degenerate: both subjects in one spot or both anchors in one spot; only A can be framed
  if (fD < 0.01f || fAlpha < 0.001f)
  {
    vToPreferred.NormalizeIfNotZero(plVec3(-1, 0, 0)).IgnoreResult();
    result.m_vPosition = vA + vToPreferred * fPreferred;
    result.m_qRotation = AimAt(result.m_vPosition, vA, vAnchorA, projection);
    return result;
  }

  TorusFrame frame;
  frame.m_vA = vA;
  frame.m_vAxis = vAB / fD;

  frame.m_vE1 = vToPreferred - frame.m_vAxis * vToPreferred.Dot(frame.m_vAxis);
  if (frame.m_vE1.NormalizeIfNotZero(plVec3::MakeZero()).Failed())
  {
    frame.m_vE1 = frame.m_vAxis.GetOrthogonalVector();
    frame.m_vE1.Normalize();
  }

  frame.m_vE2 = frame.m_vAxis.CrossRH(frame.m_vE1);

  // Law of sines in the triangle A, B, C with the angle alpha at the camera C:
  //   |AC| = |AB| * sin(beta) / sin(alpha), where beta is the angle at B.
  // Two betas give the preferred distance; take the one whose angle at A is closest to where the camera is now.
  const float fSinAlpha = plMath::Sin(plAngle::MakeFromRadian(fAlpha));
  const float fSinBeta = plMath::Clamp(fPreferred * fSinAlpha / fD, 0.0f, 1.0f);
  const float fBeta1 = plMath::ASin(fSinBeta).GetRadian();
  const float fBeta2 = plMath::Pi<float>() - fBeta1;
  const float fCurrentAngleAtA = plMath::ACos(plMath::Clamp(vToPreferred.GetNormalized().Dot(frame.m_vAxis), -1.0f, 1.0f)).GetRadian();

  float fBestBeta = -1.0f;
  float fBestDiff = plMath::MaxValue<float>();

  for (float fBeta : {fBeta1, fBeta2})
  {
    const float fAngleAtA = plMath::Pi<float>() - fAlpha - fBeta;
    if (fAngleAtA <= 0.001f)
      continue;

    const float fDiff = plMath::Abs(fAngleAtA - fCurrentAngleAtA);
    if (fDiff < fBestDiff)
    {
      fBestDiff = fDiff;
      fBestBeta = fBeta;
    }
  }

  if (fBestBeta < 0.0f)
  {
    // the anchors are further apart than any triangle allows: aim at A from the preferred spot
    vToPreferred.NormalizeIfNotZero(plVec3(-1, 0, 0)).IgnoreResult();
    result.m_vPosition = vA + vToPreferred * fPreferred;
    result.m_qRotation = AimAt(result.m_vPosition, vA, vAnchorA, projection);
    plVec2 vNdc;
    plCameraPose pose{result.m_vPosition, result.m_qRotation};
    result.m_fError = projection.Project(pose, vB, vNdc) ? (vNdc - vAnchorB).GetLength() : 100.0f;
    return result;
  }

  frame.m_fAngleAtA = plMath::Pi<float>() - fAlpha - fBestBeta;
  frame.m_fDistance = plMath::Clamp(fD * plMath::Sin(plAngle::MakeFromRadian(fBestBeta)) / fSinAlpha, fMinDistance, fMaxDistance);

  // The angle around the A-B axis: theta 0 is the preferred side. Coarse samples, then a golden-section refinement
  // around the best one, minimizing B's screen error plus a small pull toward theta 0 for continuity.
  const float fPull = 0.002f;
  auto cost = [&](float fTheta, plCameraPose& out_pose)
  { return ErrorAt(frame, fTheta, vA, vB, vAnchorA, vAnchorB, projection, out_pose) + fPull * fTheta * fTheta; };

  // the whole circle: a target on the 'wrong' side of its layout is framed from the far side of the orbit
  const float fRange = plMath::Pi<float>();
  const plUInt32 uiSamples = 37;
  const float fStep = 2.0f * fRange / (uiSamples - 1);

  plCameraPose pose;
  float fBestTheta = 0.0f;
  float fBestCost = plMath::MaxValue<float>();

  for (plUInt32 i = 0; i < uiSamples; ++i)
  {
    const float fTheta = -fRange + i * fStep;
    const float fCost = cost(fTheta, pose);

    if (fCost < fBestCost)
    {
      fBestCost = fCost;
      fBestTheta = fTheta;
    }
  }

  float lo = fBestTheta - fStep;
  float hi = fBestTheta + fStep;
  const float fGolden = 0.6180339887f;

  for (plUInt32 i = 0; i < 24; ++i)
  {
    const float m1 = hi - (hi - lo) * fGolden;
    const float m2 = lo + (hi - lo) * fGolden;

    if (cost(m1, pose) < cost(m2, pose))
      hi = m2;
    else
      lo = m1;
  }

  const float fTheta = 0.5f * (lo + hi);
  result.m_fError = ErrorAt(frame, fTheta, vA, vB, vAnchorA, vAnchorB, projection, pose);
  result.m_vPosition = pose.m_vPosition;
  result.m_qRotation = pose.m_qRotation;
  result.m_bExact = result.m_fError < 0.01f;

  if (ElevationPenalty(result.m_vPosition, vA) > 0.0f)
  {
    // every exact position is above or below the subject: keep the preferred spot, clamped to a usable elevation,
    // pin the subject and let the target fall where it may
    plVec3 vDir = vToPreferred;
    const float fHorizontal = plMath::Max(0.001f, vDir.GetAsVec2().GetLength());
    const float fElevation = plMath::Clamp(plMath::ATan2(vDir.z, fHorizontal).GetRadian(), s_fMinElevation, s_fMaxElevation);
    const plVec2 vFlat = vDir.GetAsVec2() / fHorizontal;
    vDir = plVec3(vFlat.x * plMath::Cos(plAngle::MakeFromRadian(fElevation)), vFlat.y * plMath::Cos(plAngle::MakeFromRadian(fElevation)),
      plMath::Sin(plAngle::MakeFromRadian(fElevation)));

    result.m_vPosition = vA + vDir * fPreferred;
    result.m_qRotation = AimAt(result.m_vPosition, vA, vAnchorA, projection);

    plCameraPose fallback;
    fallback.m_vPosition = result.m_vPosition;
    fallback.m_qRotation = result.m_qRotation;
    plVec2 vNdc;
    result.m_fError = projection.Project(fallback, vB, vNdc) ? (vNdc - vAnchorB).GetLength() : 1.0f;
    result.m_bExact = false;
  }

  return result;
}

float plCameraFramingSolver::ZoneCorrection(float fOffset, float fDeadZone, float fSoftZone, float fSoftFollow)
{
  const float a = plMath::Abs(fOffset);
  if (a <= fDeadZone)
    return 0.0f;

  const float fSoft = plMath::Max(fSoftZone, fDeadZone);
  const float fHard = a > fSoft ? a - fSoft : 0.0f;
  const float fRest = (plMath::Min(a, fSoft) - fDeadZone) * fSoftFollow;

  return plMath::Sign(fOffset) * (fHard + fRest);
}
