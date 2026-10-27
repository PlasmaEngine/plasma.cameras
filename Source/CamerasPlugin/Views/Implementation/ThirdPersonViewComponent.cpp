#include <CamerasPlugin/CamerasPCH.h>

#include <CamerasPlugin/Core/CameraDebug.h>
#include <CamerasPlugin/Views/ThirdPersonViewComponent.h>
#include <Core/Interfaces/PhysicsWorldModule.h>
#include <Core/WorldSerializer/WorldReader.h>
#include <Core/WorldSerializer/WorldWriter.h>
#include <RendererCore/Debug/DebugRenderer.h>

// clang-format off
PL_BEGIN_COMPONENT_TYPE(plThirdPersonViewComponent, 2, plComponentMode::Dynamic)
{
  PL_BEGIN_PROPERTIES
  {
    PL_ACCESSOR_PROPERTY("TargetObject", GetTargetObject, SetTargetObject)->AddAttributes(new plDefaultValueAttribute(plStringView(".."))),
    PL_MEMBER_PROPERTY("TargetOffsetHigh", m_vTargetOffsetHigh),
    PL_MEMBER_PROPERTY("TargetOffsetLow", m_vTargetOffsetLow),
    PL_MEMBER_PROPERTY("MinDistance", m_fMinDistance)->AddAttributes(new plDefaultValueAttribute(0.25f), new plClampValueAttribute(0.05f, 10.0f)),
    PL_MEMBER_PROPERTY("MaxDistance", m_fMaxDistance)->AddAttributes(new plDefaultValueAttribute(3.0f), new plClampValueAttribute(0.5f, 100.0f)),
    PL_MEMBER_PROPERTY("MinUpRotation", m_MinUpRotation)->AddAttributes(new plDefaultValueAttribute(plAngle::MakeFromDegree(-70)), new plClampValueAttribute(plAngle::MakeFromDegree(-85), plAngle::MakeFromDegree(70))),
    PL_MEMBER_PROPERTY("MaxUpRotation", m_MaxUpRotation)->AddAttributes(new plDefaultValueAttribute(plAngle::MakeFromDegree(+80)), new plClampValueAttribute(plAngle::MakeFromDegree(-70), plAngle::MakeFromDegree(85))),
    PL_MEMBER_PROPERTY("CeilingHeight", m_fCeilingHeight)->AddAttributes(new plDefaultValueAttribute(4.0f)),
    PL_MEMBER_PROPERTY("FloorHeight", m_fFloorHeight)->AddAttributes(new plDefaultValueAttribute(0.0f)),
    PL_MEMBER_PROPERTY("CollisionLayer", m_uiCollisionLayer)->AddAttributes(new plDynamicEnumAttribute("PhysicsCollisionLayer")),
    PL_MEMBER_PROPERTY("SweepWidth", m_fSweepWidth)->AddAttributes(new plDefaultValueAttribute(0.2f), new plClampValueAttribute(0.05f, 2.0f)),
    PL_MEMBER_PROPERTY("ZoomInSpeed", m_fZoomInSpeed)->AddAttributes(new plDefaultValueAttribute(10.0f), new plClampValueAttribute(0.01f, 1000.0f)),
    PL_MEMBER_PROPERTY("ZoomOutSpeed", m_fZoomOutSpeed)->AddAttributes(new plDefaultValueAttribute(1.0f), new plClampValueAttribute(0.01f, 1000.0f)),
    PL_MEMBER_PROPERTY("HitDynamicShapes", m_bHitDynamicShapes),
    PL_MEMBER_PROPERTY("LookInput", m_LookInput)->AddAttributes(new plGroupAttribute("Control")),
    PL_ENUM_MEMBER_PROPERTY("YawMode", plCameraYawMode, m_YawMode)->AddAttributes(new plGroupAttribute("Control")),
    PL_MEMBER_PROPERTY("ShoulderOffset", m_fShoulderOffset)->AddAttributes(new plGroupAttribute("Control"), new plClampValueAttribute(-3.0f, 3.0f)),
    PL_MEMBER_PROPERTY("ShoulderSwapAction", m_sShoulderSwapAction)->AddAttributes(new plGroupAttribute("Control")),
    PL_MEMBER_PROPERTY("PositionLag", m_PositionLag)->AddAttributes(new plGroupAttribute("Control"), new plClampValueAttribute(plTime::MakeZero(), plTime::MakeFromSeconds(2.0))),
    PL_MEMBER_PROPERTY("AutoRecenter", m_bAutoRecenter)->AddAttributes(new plGroupAttribute("Control")),
    PL_MEMBER_PROPERTY("RecenterDelay", m_RecenterDelay)->AddAttributes(new plGroupAttribute("Control"), new plDefaultValueAttribute(plTime::MakeFromSeconds(2.0))),
    PL_MEMBER_PROPERTY("RecenterSpeed", m_RecenterSpeed)->AddAttributes(new plGroupAttribute("Control"), new plDefaultValueAttribute(plAngle::MakeFromDegree(90.0f))),
    PL_MEMBER_PROPERTY("LookAhead", m_fLookAhead)->AddAttributes(new plGroupAttribute("Control"), new plClampValueAttribute(0.0f, 10.0f)),
  }
  PL_END_PROPERTIES;
  PL_BEGIN_FUNCTIONS
  {
    PL_SCRIPT_FUNCTION_PROPERTY(RotateUp, In, "angle"),
    PL_SCRIPT_FUNCTION_PROPERTY(Orbit, In, "angle"),
    PL_SCRIPT_FUNCTION_PROPERTY(SwapShoulder),
  }
  PL_END_FUNCTIONS;
}
PL_END_COMPONENT_TYPE;
// clang-format on

plThirdPersonViewComponent::plThirdPersonViewComponent() = default;
plThirdPersonViewComponent::~plThirdPersonViewComponent() = default;

void plThirdPersonViewComponent::SerializeComponent(plWorldWriter& inout_stream) const
{
  SUPER::SerializeComponent(inout_stream);

  auto& s = inout_stream.GetStream();

  // version 1
  s << m_sTargetObject;
  s << m_vTargetOffsetHigh;
  s << m_vTargetOffsetLow;
  s << m_fMinDistance;
  s << m_fMaxDistance;
  s << m_MinUpRotation;
  s << m_MaxUpRotation;
  s << m_fCeilingHeight;
  s << m_fFloorHeight;
  s << m_uiCollisionLayer;
  s << m_fSweepWidth;
  s << m_fZoomInSpeed;
  s << m_fZoomOutSpeed;

  // version 2
  s << m_LookInput.m_sLookAction;
  s << m_LookInput.m_sTurnLeftAction;
  s << m_LookInput.m_sTurnRightAction;
  s << m_LookInput.m_sLookUpAction;
  s << m_LookInput.m_sLookDownAction;
  s << m_LookInput.m_fSensitivity;
  s << m_LookInput.m_bInvertY;
  s << m_YawMode;
  s << m_fShoulderOffset;
  s << m_sShoulderSwapAction;
  s << m_PositionLag;
  s << m_bAutoRecenter;
  s << m_RecenterDelay;
  s << m_RecenterSpeed;
  s << m_bHitDynamicShapes;
  s << m_fLookAhead;
}

void plThirdPersonViewComponent::DeserializeComponent(plWorldReader& inout_stream)
{
  SUPER::DeserializeComponent(inout_stream);
  const plUInt32 uiVersion = inout_stream.GetComponentTypeVersion(GetStaticRTTI());

  auto& s = inout_stream.GetStream();

  // version 1
  s >> m_sTargetObject;
  s >> m_vTargetOffsetHigh;
  s >> m_vTargetOffsetLow;
  s >> m_fMinDistance;
  s >> m_fMaxDistance;
  s >> m_MinUpRotation;
  s >> m_MaxUpRotation;
  s >> m_fCeilingHeight;
  s >> m_fFloorHeight;
  s >> m_uiCollisionLayer;
  s >> m_fSweepWidth;
  s >> m_fZoomInSpeed;
  s >> m_fZoomOutSpeed;

  if (uiVersion < 2)
    return;

  s >> m_LookInput.m_sLookAction;
  s >> m_LookInput.m_sTurnLeftAction;
  s >> m_LookInput.m_sTurnRightAction;
  s >> m_LookInput.m_sLookUpAction;
  s >> m_LookInput.m_sLookDownAction;
  s >> m_LookInput.m_fSensitivity;
  s >> m_LookInput.m_bInvertY;
  s >> m_YawMode;
  s >> m_fShoulderOffset;
  s >> m_sShoulderSwapAction;
  s >> m_PositionLag;
  s >> m_bAutoRecenter;
  s >> m_RecenterDelay;
  s >> m_RecenterSpeed;
  s >> m_bHitDynamicShapes;
  s >> m_fLookAhead;
}

void plThirdPersonViewComponent::OnSimulationStarted()
{
  SUPER::OnSimulationStarted();

  m_CurUpRotation = plMath::Lerp(m_MinUpRotation, m_MaxUpRotation, 0.5f);
  m_YawOffset = plAngle();
  m_fShoulderSide = 1.0f;
  m_fCurShoulder = m_fShoulderOffset;
  m_bHasLaggedPivot = false;
  m_fSinceSteered = 0.0f;

  SetTargetObject(m_sTargetObject);

  plGameObject* pTarget = nullptr;
  if (GetWorld()->TryGetObject(m_hTargetObject, pTarget))
  {
    m_FreeYaw = plMath::ATan2(pTarget->GetGlobalDirForwards().y, pTarget->GetGlobalDirForwards().x);
  }
}

void plThirdPersonViewComponent::SetTargetObject(const char* szTargetObject)
{
  m_sTargetObject = szTargetObject;
  m_hTargetObject.Invalidate();

  if (IsActiveAndSimulating() && !m_sTargetObject.IsEmpty())
  {
    if (plGameObject* pTargetObject = GetWorld()->SearchForObject(m_sTargetObject, GetOwner()))
    {
      m_hTargetObject = pTargetObject->GetHandle();
    }
  }
}

const char* plThirdPersonViewComponent::GetTargetObject() const
{
  return m_sTargetObject;
}

void plThirdPersonViewComponent::RotateUp(plAngle angle)
{
  m_RotateUp += angle;
}

void plThirdPersonViewComponent::Orbit(plAngle angle)
{
  m_YawOffset += angle;
  m_FreeYaw += angle;
  m_fSinceSteered = 0.0f;
}

void plThirdPersonViewComponent::SwapShoulder()
{
  m_fShoulderSide = -m_fShoulderSide;
}

bool plThirdPersonViewComponent::GetSubjectPoint(plVec3& out_vPosition) const
{
  const plGameObject* pTarget = nullptr;
  if (!GetWorld()->TryGetObject(m_hTargetObject, pTarget))
    return false;

  out_vPosition = pTarget->GetGlobalTransform().TransformPosition(plMath::Lerp(m_vTargetOffsetLow, m_vTargetOffsetHigh, 0.5f));
  return true;
}

void plThirdPersonViewComponent::SetLookRotation(plAngle yaw, plAngle pitch)
{
  // the camera looks down when the arm is raised
  m_CurUpRotation = plMath::Clamp(-pitch, m_MinUpRotation, m_MaxUpRotation);
  m_FreeYaw = yaw;

  const plGameObject* pTarget = nullptr;
  if (GetWorld()->TryGetObject(m_hTargetObject, pTarget))
  {
    const plVec3 vForward = pTarget->GetGlobalDirForwards();
    m_YawOffset = (yaw - plMath::ATan2(vForward.y, vForward.x)).GetNormalizedRange();

    if (m_YawOffset > plAngle::MakeFromDegree(180.0f))
      m_YawOffset -= plAngle::MakeFromDegree(360.0f);
  }
}

void plThirdPersonViewComponent::OnShownByDirector(const plCameraPose& current)
{
  if (m_YawMode == plCameraYawMode::Free)
  {
    plAngle yaw, pitch;
    current.GetYawPitch(yaw, pitch);
    m_FreeYaw = yaw;
  }
}

plQuat plThirdPersonViewComponent::GetArmRotation(const plTransform& target) const
{
  const plQuat qUp = plQuat::MakeFromAxisAndAngle(plVec3::MakeAxisY(), m_CurUpRotation);

  if (m_YawMode == plCameraYawMode::Free)
    return plQuat::MakeFromAxisAndAngle(plVec3::MakeAxisZ(), m_FreeYaw) * qUp;

  return target.m_qRotation * plQuat::MakeFromAxisAndAngle(plVec3::MakeAxisZ(), m_YawOffset) * qUp;
}

float plThirdPersonViewComponent::SweepArm(const plVec3& vFrom, const plVec3& vDir, float fDistance, bool& out_bBlocked) const
{
  out_bBlocked = false;

  const plPhysicsWorldModuleInterface* pPhysics = GetWorld()->GetModuleReadOnly<plPhysicsWorldModuleInterface>();
  if (pPhysics == nullptr)
    return fDistance;

  plPhysicsQueryParameters params;
  params.m_uiCollisionLayer = m_uiCollisionLayer;
  params.m_ShapeTypes = plPhysicsShapeType::Static;

  if (m_bHitDynamicShapes)
    params.m_ShapeTypes |= plPhysicsShapeType::Dynamic;

  plPhysicsCastResult result;
  if (!pPhysics->SweepTestSphere(result, m_fSweepWidth, vFrom, vDir, fDistance, params))
    return fDistance;

  out_bBlocked = true;
  return plMath::Max(m_fMinDistance, result.m_fDistance);
}

bool plThirdPersonViewComponent::ComputeRig(const plTransform& target, Rig& out_rig) const
{
  const plVec3 vOffsetCenter = plMath::Lerp(m_vTargetOffsetLow, m_vTargetOffsetHigh, 0.5f);
  plVec3 vOffset = vOffsetCenter;

  if (m_CurUpRotation.GetRadian() > 0)
  {
    const float fLerp = plMath::Unlerp(0.0f, m_MaxUpRotation.GetRadian(), m_CurUpRotation.GetRadian());
    vOffset = plMath::Lerp(vOffsetCenter, m_vTargetOffsetHigh, fLerp);
  }
  else if (m_MinUpRotation.GetRadian() < 0)
  {
    const float fLerp = plMath::Unlerp(0.0f, m_MinUpRotation.GetRadian(), m_CurUpRotation.GetRadian());
    vOffset = plMath::Lerp(vOffsetCenter, m_vTargetOffsetLow, fLerp);
  }

  const plQuat qArm = GetArmRotation(target);
  const plQuat qBase = m_YawMode == plCameraYawMode::Free ? plQuat::MakeFromAxisAndAngle(plVec3::MakeAxisZ(), m_FreeYaw) : target.m_qRotation;

  out_rig.m_vPivot = target.m_vPosition + qBase * vOffset;

  // shoulder: shift sideways in the arm's horizontal frame, so camera and aim point move together
  plVec3 vRight = qArm * plVec3(0, -1, 0);
  vRight.z = 0.0f;
  vRight.NormalizeIfNotZero(plVec3::MakeZero()).IgnoreResult();
  out_rig.m_vPivot += vRight * m_fCurShoulder;

  if (m_fLookAhead > 0.0f)
  {
    plVec3 vVelocity = GetSubjectInfo().m_vVelocity;
    vVelocity.z = 0.0f;
    const float fSpeed = vVelocity.GetLength();

    if (fSpeed > 0.1f)
      out_rig.m_vPivot += (vVelocity / fSpeed) * m_fLookAhead * plMath::Min(1.0f, fSpeed / 6.0f);
  }

  out_rig.m_vLookAt = out_rig.m_vPivot;
  out_rig.m_vArmDir = qArm * plVec3(-1, 0, 0);

  float fNewDistance = m_fMaxDistance;

  // keep the camera under the ceiling and above the floor
  if (m_CurUpRotation.GetRadian() > 0)
  {
    const plPlane p = plPlane::MakeFromNormalAndPoint(-plVec3::MakeAxisZ(), target.m_vPosition + plVec3::MakeAxisZ() * m_fCeilingHeight);

    float fHitDist;
    if (p.GetRayIntersection(out_rig.m_vPivot, out_rig.m_vArmDir, &fHitDist))
      fNewDistance = plMath::Min(fNewDistance, fHitDist);
  }
  else
  {
    const plPlane p = plPlane::MakeFromNormalAndPoint(plVec3::MakeAxisZ(), target.m_vPosition + plVec3::MakeAxisZ() * m_fFloorHeight);

    float fHitDist;
    if (p.GetRayIntersection(out_rig.m_vPivot, out_rig.m_vArmDir, &fHitDist))
      fNewDistance = plMath::Min(fNewDistance, fHitDist);
  }

  out_rig.m_fTargetDistance = SweepArm(out_rig.m_vPivot, out_rig.m_vArmDir, fNewDistance, out_rig.m_bBlocked);
  return true;
}

void plThirdPersonViewComponent::ApplyInput(plCameraViewContext& ref_context)
{
  const plInputComponent* pInput = FindInput();

  if (m_LookInput.HasAnyAction())
  {
    const float fScale = ref_context.m_PlayerSettings.m_fLookSensitivity * ref_context.m_fLookScale;
    plVec2 vLook = m_LookInput.Read(pInput, fScale);

    if (ref_context.m_PlayerSettings.m_bInvertY)
      vLook.y = -vLook.y;

    if (!vLook.IsZero(0.00001f))
    {
      ref_context.m_bPlayerSteering = true;
      m_fSinceSteered = 0.0f;
    }

    m_YawOffset += plAngle::MakeFromRadian(vLook.x);
    m_FreeYaw += plAngle::MakeFromRadian(vLook.x);
    m_RotateUp -= plAngle::MakeFromRadian(vLook.y);
  }

  if (!m_sShoulderSwapAction.IsEmpty())
  {
    const bool bDown = plReadCameraAction(pInput, m_sShoulderSwapAction) > 0.5f;

    if (bDown && !m_bSwapWasDown)
      SwapShoulder();

    m_bSwapWasDown = bDown;
  }
}

void plThirdPersonViewComponent::UpdateRecenter(plCameraViewContext& ref_context, bool bSteered)
{
  m_fSinceSteered = bSteered ? 0.0f : m_fSinceSteered + ref_context.m_fDeltaSeconds;

  if (!m_bAutoRecenter || m_YawMode != plCameraYawMode::FollowTarget || m_fSinceSteered < m_RecenterDelay.AsFloatInSeconds())
    return;

  // only while the target moves: a player standing still keeps the view they chose
  if (GetSubjectInfo().m_vVelocity.GetAsVec2().GetLength() < 0.5f)
    return;

  const float fStep = m_RecenterSpeed.GetRadian() * ref_context.m_fDeltaSeconds;
  const float fOffset = m_YawOffset.GetRadian();
  m_YawOffset = plAngle::MakeFromRadian(fOffset > 0.0f ? plMath::Max(0.0f, fOffset - fStep) : plMath::Min(0.0f, fOffset + fStep));
}

void plThirdPersonViewComponent::UpdateView(plCameraViewContext& ref_context, const plCameraPose& previous, plCameraPose& out_pose)
{
  out_pose = previous;

  plGameObject* pTarget = nullptr;
  if (m_hTargetObject.IsInvalidated() || !GetWorld()->TryGetObject(m_hTargetObject, pTarget))
    return;

  const float fDt = ref_context.m_fDeltaSeconds;

  ApplyInput(ref_context);
  UpdateRecenter(ref_context, ref_context.m_bPlayerSteering);

  m_CurUpRotation = plMath::Clamp(m_CurUpRotation + m_RotateUp, m_MinUpRotation, m_MaxUpRotation);
  m_RotateUp = plAngle();

  m_fCurShoulder = plMath::Lerp(m_fCurShoulder, m_fShoulderOffset * m_fShoulderSide, plMath::Min(1.0f, fDt * 8.0f));

  Rig rig;
  ComputeRig(pTarget->GetGlobalTransform(), rig);

  if (m_PositionLag.IsPositive() && m_bHasLaggedPivot)
  {
    const float fFollow = 1.0f - plMath::Exp(-fDt / m_PositionLag.AsFloatInSeconds());
    m_vLaggedPivot = plMath::Lerp(m_vLaggedPivot, rig.m_vPivot, fFollow);
    rig.m_vLookAt = m_vLaggedPivot;
  }
  else
  {
    m_vLaggedPivot = rig.m_vPivot;
  }

  m_bHasLaggedPivot = true;

  const float fSpeed = m_fCurDistance > rig.m_fTargetDistance ? m_fZoomInSpeed : m_fZoomOutSpeed;
  m_fCurDistance = plMath::Lerp(m_fCurDistance, rig.m_fTargetDistance, plMath::Min(1.0f, fDt * fSpeed));

  const plVec3 vLookFromPos = rig.m_vLookAt + m_fCurDistance * rig.m_vArmDir;
  out_pose = plCameraPose::MakeLookAt(vLookFromPos, rig.m_vLookAt, m_fFieldOfView);

  m_LastRig = rig;
  m_LastPose = out_pose;
}

void plThirdPersonViewComponent::DrawRig() const
{
  const plGameObject* pTarget = nullptr;

  if (!GetWorld()->TryGetObject(m_hTargetObject, pTarget))
  {
    // in the editor nothing has been resolved yet: find the target the way simulation will
    pTarget = const_cast<plWorld*>(GetWorld())->SearchForObject(m_sTargetObject, const_cast<plGameObject*>(GetOwner()));
  }

  if (pTarget == nullptr)
  {
    SUPER::DrawRig();
    return;
  }

  const plTransform target = pTarget->GetGlobalTransform();

  Rig rig;
  if (IsActiveAndSimulating())
  {
    rig = m_LastRig;
  }
  else
  {
    ComputeRig(target, rig);
  }

  const float fDistance = IsActiveAndSimulating() ? m_fCurDistance : rig.m_fTargetDistance;
  const plVec3 vCamera = rig.m_vLookAt + rig.m_vArmDir * fDistance;

  plHybridArray<plDebugRenderer::Line, 16> lines;
  lines.PushBack(plDebugRenderer::Line(target.TransformPosition(m_vTargetOffsetLow), target.TransformPosition(m_vTargetOffsetHigh)));
  plDebugRenderer::DrawLines(GetWorld(), lines, plColor(0.88f, 0.69f, 0.51f));

  lines.Clear();
  lines.PushBack(plDebugRenderer::Line(rig.m_vLookAt, vCamera));
  plDebugRenderer::DrawLines(GetWorld(), lines, plColor(0.43f, 0.66f, 1.0f));

  lines.Clear();
  lines.PushBack(plDebugRenderer::Line(vCamera, rig.m_vLookAt + rig.m_vArmDir * m_fMaxDistance));
  plDebugRenderer::DrawLines(GetWorld(), lines, plColor(0.43f, 0.66f, 1.0f, 0.3f));

  plDebugRenderer::DrawLineSphere(GetWorld(), plBoundingSphere::MakeFromCenterAndRadius(vCamera, m_fSweepWidth), rig.m_bBlocked ? plColor(0.91f, 0.69f, 0.29f) : plColor(0.43f, 0.66f, 1.0f));

  // ceiling and floor as squares around the target
  auto square = [&](float fHeight, const plColor& color)
  {
    const plVec3 c = target.m_vPosition + plVec3(0, 0, fHeight);
    const float r = m_fMaxDistance;
    plHybridArray<plDebugRenderer::Line, 4> sq;
    sq.PushBack(plDebugRenderer::Line(c + plVec3(-r, -r, 0), c + plVec3(r, -r, 0)));
    sq.PushBack(plDebugRenderer::Line(c + plVec3(r, -r, 0), c + plVec3(r, r, 0)));
    sq.PushBack(plDebugRenderer::Line(c + plVec3(r, r, 0), c + plVec3(-r, r, 0)));
    sq.PushBack(plDebugRenderer::Line(c + plVec3(-r, r, 0), c + plVec3(-r, -r, 0)));
    plDebugRenderer::DrawLines(GetWorld(), sq, color);
  };

  square(m_fCeilingHeight, plColor(0.54f, 0.54f, 0.58f, 0.6f));
  square(m_fFloorHeight, plColor(0.54f, 0.54f, 0.58f, 0.3f));

  plCameraDebug::DrawPoseFrustum(GetWorld(), plCameraPose::MakeLookAt(vCamera, rig.m_vLookAt, m_fFieldOfView), plColor(0.43f, 0.66f, 1.0f), 0.5f);
}
