#include <CamerasPlugin/CamerasPCH.h>

#include <CamerasPlugin/Core/CameraDebug.h>
#include <CamerasPlugin/Views/RailViewComponent.h>
#include <Core/WorldSerializer/WorldReader.h>
#include <Core/WorldSerializer/WorldWriter.h>
#include <RendererCore/Components/SplineComponent.h>
#include <RendererCore/Debug/DebugRenderer.h>

// clang-format off
PL_BEGIN_COMPONENT_TYPE(plRailViewComponent, 1, plComponentMode::Dynamic)
{
  PL_BEGIN_PROPERTIES
  {
    PL_ACCESSOR_PROPERTY("Spline", DummyGetter, SetSplineReference)->AddAttributes(new plGameObjectReferenceAttribute()),
    PL_ACCESSOR_PROPERTY("Target", DummyGetter, SetTargetReference)->AddAttributes(new plGameObjectReferenceAttribute()),
    PL_MEMBER_PROPERTY("TargetOffset", m_vTargetOffset)->AddAttributes(new plDefaultValueAttribute(plVec3(0, 0, 1.0f))),
    PL_ENUM_MEMBER_PROPERTY("Mode", plCameraRailMode, m_Mode),
    PL_MEMBER_PROPERTY("KeyOffset", m_fKeyOffset),
    PL_MEMBER_PROPERTY("RailSmoothing", m_RailSmoothing)->AddAttributes(new plDefaultValueAttribute(plTime::MakeFromSeconds(0.3)), new plClampValueAttribute(plTime::MakeZero(), plTime::MakeFromSeconds(5.0))),
  }
  PL_END_PROPERTIES;
  PL_BEGIN_FUNCTIONS
  {
    PL_SCRIPT_FUNCTION_PROPERTY(SetRailKey, In, "Key"),
  }
  PL_END_FUNCTIONS;
}
PL_END_COMPONENT_TYPE;
// clang-format on

plRailViewComponent::plRailViewComponent() = default;
plRailViewComponent::~plRailViewComponent() = default;

void plRailViewComponent::SerializeComponent(plWorldWriter& inout_stream) const
{
  SUPER::SerializeComponent(inout_stream);
  auto& s = inout_stream.GetStream();

  inout_stream.WriteGameObjectHandle(m_hSpline);
  inout_stream.WriteGameObjectHandle(m_hTarget);
  s << m_vTargetOffset;
  s << m_Mode;
  s << m_fKeyOffset;
  s << m_RailSmoothing;
}

void plRailViewComponent::DeserializeComponent(plWorldReader& inout_stream)
{
  SUPER::DeserializeComponent(inout_stream);
  auto& s = inout_stream.GetStream();

  m_hSpline = inout_stream.ReadGameObjectHandle();
  m_hTarget = inout_stream.ReadGameObjectHandle();
  s >> m_vTargetOffset;
  s >> m_Mode;
  s >> m_fKeyOffset;
  s >> m_RailSmoothing;
}

void plRailViewComponent::SetSplineReference(const char* szReference)
{
  m_hSpline = plCameraReference::Resolve(this, szReference, "Spline");
}

void plRailViewComponent::SetTargetReference(const char* szReference)
{
  m_hTarget = plCameraReference::Resolve(this, szReference, "Target");
}

void plRailViewComponent::SetRailKey(float fKey)
{
  m_fScriptKey = fKey;
}

const plSplineComponent* plRailViewComponent::GetSpline() const
{
  const plGameObject* pObject = nullptr;
  if (!GetWorld()->TryGetObject(m_hSpline, pObject))
    pObject = GetOwner();

  const plSplineComponent* pSpline = nullptr;
  pObject->TryGetComponentOfBaseType(pSpline);
  return pSpline;
}

bool plRailViewComponent::GetSubjectPoint(plVec3& out_vPosition) const
{
  const plGameObject* pTarget = nullptr;
  if (!GetWorld()->TryGetObject(m_hTarget, pTarget))
    return false;

  out_vPosition = pTarget->GetGlobalPosition() + m_vTargetOffset;
  return true;
}

void plRailViewComponent::OnShownByDirector(const plCameraPose& current)
{
  m_bHasKey = false;
}

void plRailViewComponent::UpdateView(plCameraViewContext& ref_context, const plCameraPose& previous, plCameraPose& out_pose)
{
  out_pose = previous;

  const plSplineComponent* pSpline = GetSpline();
  if (pSpline == nullptr)
    return;

  plVec3 vTarget = plVec3::MakeZero();
  const bool bHasTarget = GetSubjectPoint(vTarget);

  float fKey = m_fScriptKey;
  if (m_Mode == plCameraRailMode::ClosestToTarget && bHasTarget)
  {
    float fDistanceToSpline = 0.0f;
    fKey = pSpline->FindKeyClosestToPoint(vTarget, fDistanceToSpline, plSplineComponentSpace::Global);
  }

  fKey += m_fKeyOffset;

  if (!m_bHasKey || !m_RailSmoothing.IsPositive())
  {
    m_fSmoothedKey = fKey;
    m_bHasKey = true;
  }
  else
  {
    const float fFollow = 1.0f - plMath::Exp(-ref_context.m_fDeltaSeconds / m_RailSmoothing.AsFloatInSeconds());
    m_fSmoothedKey = plMath::Lerp(m_fSmoothedKey, fKey, fFollow);
  }

  out_pose.m_vPosition = pSpline->GetPositionAtKey(m_fSmoothedKey, plSplineComponentSpace::Global);
  out_pose.m_fFieldOfView = m_fFieldOfView;

  if (bHasTarget)
  {
    out_pose.m_qRotation = plCameraPose::MakeLookRotation(vTarget - out_pose.m_vPosition);
    out_pose.m_fFocusDistance = (vTarget - out_pose.m_vPosition).GetLength();
  }
  else
  {
    out_pose.m_qRotation = plCameraPose::MakeLookRotation(pSpline->GetForwardDirAtKey(m_fSmoothedKey, plSplineComponentSpace::Global));
  }

  m_LastPose = out_pose;
}

void plRailViewComponent::DrawRig() const
{
  SUPER::DrawRig();

  plVec3 vTarget;
  if (GetSubjectPoint(vTarget))
  {
    plHybridArray<plDebugRenderer::Line, 1> lines;
    lines.PushBack(plDebugRenderer::Line(m_LastPose.m_vPosition, vTarget));
    plDebugRenderer::DrawLines(GetWorld(), lines, plColor(0.39f, 0.75f, 0.73f, 0.5f));
  }
}
