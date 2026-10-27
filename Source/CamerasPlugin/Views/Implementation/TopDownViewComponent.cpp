#include <CamerasPlugin/CamerasPCH.h>

#include <CamerasPlugin/Views/TopDownViewComponent.h>
#include <Core/Input/InputManager.h>
#include <Core/Interfaces/PhysicsWorldModule.h>
#include <Core/WorldSerializer/WorldReader.h>
#include <Core/WorldSerializer/WorldWriter.h>
#include <GameEngine/Gameplay/InputComponent.h>

// clang-format off
PL_BEGIN_COMPONENT_TYPE(plTopDownViewComponent, 1, plComponentMode::Dynamic)
{
  PL_BEGIN_PROPERTIES
  {
    PL_ACCESSOR_PROPERTY("Target", DummyGetter, SetTargetReference)->AddAttributes(new plGameObjectReferenceAttribute()),
    PL_MEMBER_PROPERTY("Pitch", m_Pitch)->AddAttributes(new plDefaultValueAttribute(plAngle::MakeFromDegree(55.0f)), new plClampValueAttribute(plAngle::MakeFromDegree(5.0f), plAngle::MakeFromDegree(89.0f))),
    PL_MEMBER_PROPERTY("Yaw", m_Yaw)->AddAttributes(new plDefaultValueAttribute(plAngle::MakeFromDegree(45.0f))),
    PL_MEMBER_PROPERTY("RotateStep", m_RotateStep)->AddAttributes(new plDefaultValueAttribute(plAngle::MakeFromDegree(90.0f)), new plGroupAttribute("Control")),
    PL_MEMBER_PROPERTY("RotateLeftAction", m_sRotateLeftAction)->AddAttributes(new plGroupAttribute("Control")),
    PL_MEMBER_PROPERTY("RotateRightAction", m_sRotateRightAction)->AddAttributes(new plGroupAttribute("Control")),
    PL_ARRAY_MEMBER_PROPERTY("ZoomDistances", m_ZoomDistances)->AddAttributes(new plGroupAttribute("Zoom")),
    PL_MEMBER_PROPERTY("StartZoom", m_uiStartZoom)->AddAttributes(new plDefaultValueAttribute(1), new plGroupAttribute("Zoom")),
    PL_MEMBER_PROPERTY("ZoomInAction", m_sZoomInAction)->AddAttributes(new plGroupAttribute("Zoom")),
    PL_MEMBER_PROPERTY("ZoomOutAction", m_sZoomOutAction)->AddAttributes(new plGroupAttribute("Zoom")),
    PL_MEMBER_PROPERTY("PanAction", m_sPanAction)->AddAttributes(new plGroupAttribute("Pan")),
    PL_MEMBER_PROPERTY("PanSpeed", m_fPanSpeed)->AddAttributes(new plDefaultValueAttribute(12.0f), new plClampValueAttribute(0.0f, 500.0f), new plGroupAttribute("Pan")),
    PL_MEMBER_PROPERTY("MaxPan", m_fMaxPan)->AddAttributes(new plDefaultValueAttribute(30.0f), new plClampValueAttribute(0.0f, 10000.0f), new plGroupAttribute("Pan")),
    PL_MEMBER_PROPERTY("EdgeScroll", m_bEdgeScroll)->AddAttributes(new plGroupAttribute("Pan")),
    PL_MEMBER_PROPERTY("FollowLag", m_FollowLag)->AddAttributes(new plDefaultValueAttribute(plTime::MakeFromSeconds(0.2)), new plClampValueAttribute(plTime::MakeZero(), plTime::MakeFromSeconds(3.0))),
    PL_MEMBER_PROPERTY("GroundFollow", m_bGroundFollow),
  }
  PL_END_PROPERTIES;
  PL_BEGIN_FUNCTIONS
  {
    PL_SCRIPT_FUNCTION_PROPERTY(ZoomIn),
    PL_SCRIPT_FUNCTION_PROPERTY(ZoomOut),
    PL_SCRIPT_FUNCTION_PROPERTY(RotateBySteps, In, "Steps"),
    PL_SCRIPT_FUNCTION_PROPERTY(ResetPan),
  }
  PL_END_FUNCTIONS;
}
PL_END_COMPONENT_TYPE;
// clang-format on

plTopDownViewComponent::plTopDownViewComponent()
{
  m_ZoomDistances.PushBack(8.0f);
  m_ZoomDistances.PushBack(14.0f);
  m_ZoomDistances.PushBack(22.0f);
}

plTopDownViewComponent::~plTopDownViewComponent() = default;

void plTopDownViewComponent::SerializeComponent(plWorldWriter& inout_stream) const
{
  SUPER::SerializeComponent(inout_stream);
  auto& s = inout_stream.GetStream();

  inout_stream.WriteGameObjectHandle(m_hTarget);
  s << m_Pitch;
  s << m_Yaw;
  s << m_RotateStep;
  s << m_sRotateLeftAction;
  s << m_sRotateRightAction;
  s.WriteArray(m_ZoomDistances).IgnoreResult();
  s << m_uiStartZoom;
  s << m_sZoomInAction;
  s << m_sZoomOutAction;
  s << m_sPanAction;
  s << m_fPanSpeed;
  s << m_fMaxPan;
  s << m_bEdgeScroll;
  s << m_FollowLag;
  s << m_bGroundFollow;
}

void plTopDownViewComponent::DeserializeComponent(plWorldReader& inout_stream)
{
  SUPER::DeserializeComponent(inout_stream);
  auto& s = inout_stream.GetStream();

  m_hTarget = inout_stream.ReadGameObjectHandle();
  s >> m_Pitch;
  s >> m_Yaw;
  s >> m_RotateStep;
  s >> m_sRotateLeftAction;
  s >> m_sRotateRightAction;
  s.ReadArray(m_ZoomDistances).IgnoreResult();
  s >> m_uiStartZoom;
  s >> m_sZoomInAction;
  s >> m_sZoomOutAction;
  s >> m_sPanAction;
  s >> m_fPanSpeed;
  s >> m_fMaxPan;
  s >> m_bEdgeScroll;
  s >> m_FollowLag;
  s >> m_bGroundFollow;
}

void plTopDownViewComponent::OnSimulationStarted()
{
  SUPER::OnSimulationStarted();

  if (m_ZoomDistances.IsEmpty())
    m_ZoomDistances.PushBack(14.0f);

  m_uiZoom = plMath::Min(m_uiStartZoom, m_ZoomDistances.GetCount() - 1);
  m_fCurDistance = m_ZoomDistances[m_uiZoom];
  m_TargetYaw = m_Yaw;
  m_CurYaw = m_Yaw;
  m_vPan.SetZero();
  m_bHasFocus = false;
}

void plTopDownViewComponent::SetTargetReference(const char* szReference)
{
  m_hTarget = plCameraReference::Resolve(this, szReference, "Target");
}

bool plTopDownViewComponent::GetSubjectPoint(plVec3& out_vPosition) const
{
  const plGameObject* pTarget = nullptr;
  if (!GetWorld()->TryGetObject(m_hTarget, pTarget))
    return false;

  out_vPosition = pTarget->GetGlobalPosition();
  return true;
}

void plTopDownViewComponent::ZoomIn()
{
  if (m_uiZoom > 0)
    --m_uiZoom;
}

void plTopDownViewComponent::ZoomOut()
{
  if (m_uiZoom + 1 < m_ZoomDistances.GetCount())
    ++m_uiZoom;
}

void plTopDownViewComponent::RotateBySteps(float fSteps)
{
  m_TargetYaw += m_RotateStep * fSteps;
}

void plTopDownViewComponent::ResetPan()
{
  m_vPan.SetZero();
}

bool plTopDownViewComponent::Pressed(const plInputComponent* pInput, const plString& sAction, bool& ref_bWasDown) const
{
  const bool bDown = plReadCameraAction(pInput, sAction) > 0.5f;
  const bool bPressed = bDown && !ref_bWasDown;
  ref_bWasDown = bDown;
  return bPressed;
}

void plTopDownViewComponent::ReadInput(plCameraViewContext& ref_context)
{
  const plInputComponent* pInput = FindInput();

  if (Pressed(pInput, m_sZoomInAction, m_bZoomInDown))
    ZoomIn();

  if (Pressed(pInput, m_sZoomOutAction, m_bZoomOutDown))
    ZoomOut();

  if (Pressed(pInput, m_sRotateLeftAction, m_bRotLeftDown))
    RotateBySteps(1.0f);

  if (Pressed(pInput, m_sRotateRightAction, m_bRotRightDown))
    RotateBySteps(-1.0f);

  plVec2 vPan = plVec2::MakeZero();

  if (pInput != nullptr && !m_sPanAction.IsEmpty())
    vPan = pInput->GetActionVector(m_sPanAction).GetAsVec2();

  if (m_bEdgeScroll)
  {
    float fMouseX = 0.5f, fMouseY = 0.5f;
    plInputManager::GetInputSlotState(plInputSlot_MousePositionX, &fMouseX);
    plInputManager::GetInputSlotState(plInputSlot_MousePositionY, &fMouseY);

    const float fEdge = 0.02f;
    if (fMouseX < fEdge)
      vPan.x -= 1.0f;
    else if (fMouseX > 1.0f - fEdge)
      vPan.x += 1.0f;

    if (fMouseY < fEdge)
      vPan.y += 1.0f;
    else if (fMouseY > 1.0f - fEdge)
      vPan.y -= 1.0f;
  }

  if (vPan.IsZero(0.0001f))
    return;

  ref_context.m_bPlayerSteering = true;

  // pan relative to the camera's heading: +y away from the camera, +x to its right
  const plVec3 vForward(plMath::Cos(m_CurYaw), plMath::Sin(m_CurYaw), 0.0f);
  const plVec3 vRight(vForward.y, -vForward.x, 0.0f);
  const float fScale = m_fPanSpeed * ref_context.m_fDeltaSeconds * (m_fCurDistance / 14.0f);

  m_vPan += (vForward * vPan.y + vRight * vPan.x) * fScale;

  const float fLength = m_vPan.GetLength();
  if (fLength > m_fMaxPan)
    m_vPan *= m_fMaxPan / fLength;
}

float plTopDownViewComponent::GroundHeightAt(const plVec3& vPosition) const
{
  const plPhysicsWorldModuleInterface* pPhysics = GetWorld()->GetModuleReadOnly<plPhysicsWorldModuleInterface>();
  if (pPhysics == nullptr)
    return vPosition.z;

  plPhysicsQueryParameters params;
  params.m_ShapeTypes = plPhysicsShapeType::Static;

  plPhysicsCastResult result;
  if (pPhysics->Raycast(result, vPosition + plVec3(0, 0, 50.0f), plVec3(0, 0, -1), 100.0f, params))
    return result.m_vPosition.z;

  return vPosition.z;
}

void plTopDownViewComponent::UpdateView(plCameraViewContext& ref_context, const plCameraPose& previous, plCameraPose& out_pose)
{
  out_pose = previous;

  plVec3 vFocus;
  if (!GetSubjectPoint(vFocus))
    vFocus = GetOwner()->GetGlobalPosition();

  ReadInput(ref_context);

  const float fDt = ref_context.m_fDeltaSeconds;
  const float fEase = plMath::Min(1.0f, fDt * 6.0f);

  vFocus += m_vPan;
  if (m_bGroundFollow)
    vFocus.z = GroundHeightAt(vFocus);

  if (!m_bHasFocus || !m_FollowLag.IsPositive())
  {
    m_vSmoothedFocus = vFocus;
    m_bHasFocus = true;
  }
  else
  {
    m_vSmoothedFocus = plMath::Lerp(m_vSmoothedFocus, vFocus, 1.0f - plMath::Exp(-fDt / m_FollowLag.AsFloatInSeconds()));
  }

  if (!m_ZoomDistances.IsEmpty())
  {
    m_uiZoom = plMath::Min(m_uiZoom, m_ZoomDistances.GetCount() - 1);
    m_fCurDistance = plMath::Lerp(m_fCurDistance, m_ZoomDistances[m_uiZoom], fEase);
  }

  m_CurYaw = plMath::Lerp(m_CurYaw, m_TargetYaw, fEase);

  const plVec3 vBack(-plMath::Cos(m_CurYaw) * plMath::Cos(m_Pitch), -plMath::Sin(m_CurYaw) * plMath::Cos(m_Pitch), plMath::Sin(m_Pitch));
  out_pose = plCameraPose::MakeLookAt(m_vSmoothedFocus + vBack * m_fCurDistance, m_vSmoothedFocus, m_fFieldOfView);
  out_pose.m_fFocusDistance = m_fCurDistance;

  m_LastPose = out_pose;
}
