#include <CamerasPlugin/CamerasPCH.h>

#include <CamerasPlugin/Core/CameraPose.h>
#include <Foundation/Math/Mat3.h>

void plCameraPose::GetYawPitch(plAngle& out_yaw, plAngle& out_pitch) const
{
  const plVec3 vForward = GetForward();

  out_yaw = plAngle::MakeFromRadian(plMath::ATan2(vForward.y, vForward.x).GetRadian());
  out_pitch = plMath::ASin(plMath::Clamp(vForward.z, -1.0f, 1.0f));
}

plQuat plCameraPose::MakeRotation(plAngle yaw, plAngle pitch, plAngle roll)
{
  // Positive rotation around +Y tips +X down, so looking up is a negative angle around Y.
  const plQuat qYaw = plQuat::MakeFromAxisAndAngle(plVec3::MakeAxisZ(), yaw);
  const plQuat qPitch = plQuat::MakeFromAxisAndAngle(plVec3::MakeAxisY(), -pitch);
  const plQuat qRoll = plQuat::MakeFromAxisAndAngle(plVec3::MakeAxisX(), roll);

  return qYaw * qPitch * qRoll;
}

plQuat plCameraPose::MakeLookRotation(const plVec3& vDirection)
{
  plVec3 vDir = vDirection;

  if (vDir.NormalizeIfNotZero(plVec3::MakeAxisX()).Failed())
    return plQuat::MakeIdentity();

  const plAngle yaw = plMath::ATan2(vDir.y, vDir.x);
  const plAngle pitch = plMath::ASin(plMath::Clamp(vDir.z, -1.0f, 1.0f));

  return MakeRotation(yaw, pitch);
}

plCameraPose plCameraPose::MakeLookAt(const plVec3& vPosition, const plVec3& vTarget, float fFieldOfView)
{
  plCameraPose pose;
  pose.m_vPosition = vPosition;
  pose.m_qRotation = MakeLookRotation(vTarget - vPosition);
  pose.m_fFieldOfView = fFieldOfView;
  return pose;
}

plCameraPose plCameraPose::Blend(const plCameraPose& a, const plCameraPose& b, float fT)
{
  if (fT <= 0.0f)
    return a;

  if (fT >= 1.0f)
    return b;

  plCameraPose result;
  result.m_vPosition = plMath::Lerp(a.m_vPosition, b.m_vPosition, fT);
  result.m_qRotation = plQuat::MakeSlerp(a.m_qRotation, b.m_qRotation, fT);
  result.m_fFieldOfView = plMath::Lerp(a.m_fFieldOfView, b.m_fFieldOfView, fT);

  // A pose without focus contributes the other pose's distance, so focus never blends through zero.
  const float fFocusA = a.m_fFocusDistance > 0.0f ? a.m_fFocusDistance : b.m_fFocusDistance;
  const float fFocusB = b.m_fFocusDistance > 0.0f ? b.m_fFocusDistance : a.m_fFocusDistance;
  result.m_fFocusDistance = plMath::Lerp(fFocusA, fFocusB, fT);

  // the near plane switches at the halfway point: interpolating it would clip whichever view needs it close
  result.m_fNearPlane = fT < 0.5f ? a.m_fNearPlane : b.m_fNearPlane;

  return result;
}

plCameraProjection plCameraProjection::Make(float fFovDegrees, float fAspectRatio, bool bFovIsHorizontal)
{
  const float fTanHalf = plMath::Tan(plAngle::MakeFromDegree(plMath::Clamp(fFovDegrees, 1.0f, 179.0f) * 0.5f));
  const float fAspect = plMath::Max(fAspectRatio, 0.01f);

  plCameraProjection proj;

  if (bFovIsHorizontal)
  {
    proj.m_fTanHalfX = fTanHalf;
    proj.m_fTanHalfY = fTanHalf / fAspect;
  }
  else
  {
    proj.m_fTanHalfY = fTanHalf;
    proj.m_fTanHalfX = fTanHalf * fAspect;
  }

  return proj;
}

bool plCameraProjection::Project(const plCameraPose& pose, const plVec3& vWorldPoint, plVec2& out_vNdc) const
{
  const plVec3 vLocal = pose.m_qRotation.GetInverse() * (vWorldPoint - pose.m_vPosition);

  if (vLocal.x < 0.01f)
    return false;

  // camera space: +X forward, +Y left, +Z up; screen +X is right
  out_vNdc.x = (-vLocal.y / vLocal.x) / m_fTanHalfX;
  out_vNdc.y = (vLocal.z / vLocal.x) / m_fTanHalfY;
  return true;
}

plVec3 plCameraProjection::GetViewDirection(const plVec2& vNdc) const
{
  plVec3 vDir(1.0f, -vNdc.x * m_fTanHalfX, vNdc.y * m_fTanHalfY);
  vDir.Normalize();
  return vDir;
}
