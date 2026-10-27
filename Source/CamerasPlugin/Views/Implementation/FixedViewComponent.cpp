#include <CamerasPlugin/CamerasPCH.h>

#include <CamerasPlugin/Views/FixedViewComponent.h>
#include <Core/WorldSerializer/WorldReader.h>
#include <Core/WorldSerializer/WorldWriter.h>

// clang-format off
PL_BEGIN_COMPONENT_TYPE(plFixedViewComponent, 1, plComponentMode::Dynamic)
{
  PL_BEGIN_PROPERTIES
  {
    PL_ACCESSOR_PROPERTY("Target", DummyGetter, SetTargetReference)->AddAttributes(new plGameObjectReferenceAttribute()),
    PL_MEMBER_PROPERTY("TargetOffset", m_vTargetOffset)->AddAttributes(new plDefaultValueAttribute(plVec3(0, 0, 1.0f))),
    PL_MEMBER_PROPERTY("TrackingSmoothing", m_TrackingSmoothing)->AddAttributes(new plDefaultValueAttribute(plTime::MakeFromSeconds(0.15)), new plClampValueAttribute(plTime::MakeZero(), plTime::MakeFromSeconds(2.0))),
    PL_MEMBER_PROPERTY("FrameTarget", m_bFrameTarget),
    PL_MEMBER_PROPERTY("FrameSize", m_fFrameSize)->AddAttributes(new plDefaultValueAttribute(3.0f), new plClampValueAttribute(0.1f, 100.0f)),
  }
  PL_END_PROPERTIES;
}
PL_END_COMPONENT_TYPE;
// clang-format on

plFixedViewComponent::plFixedViewComponent() = default;
plFixedViewComponent::~plFixedViewComponent() = default;

void plFixedViewComponent::SerializeComponent(plWorldWriter& inout_stream) const
{
  SUPER::SerializeComponent(inout_stream);
  auto& s = inout_stream.GetStream();

  inout_stream.WriteGameObjectHandle(m_hTarget);
  s << m_vTargetOffset;
  s << m_TrackingSmoothing;
  s << m_bFrameTarget;
  s << m_fFrameSize;
}

void plFixedViewComponent::DeserializeComponent(plWorldReader& inout_stream)
{
  SUPER::DeserializeComponent(inout_stream);
  auto& s = inout_stream.GetStream();

  m_hTarget = inout_stream.ReadGameObjectHandle();
  s >> m_vTargetOffset;
  s >> m_TrackingSmoothing;
  s >> m_bFrameTarget;
  s >> m_fFrameSize;
}

void plFixedViewComponent::SetTargetReference(const char* szReference)
{
  m_hTarget = plCameraReference::Resolve(this, szReference, "Target");
}

bool plFixedViewComponent::GetSubjectPoint(plVec3& out_vPosition) const
{
  const plGameObject* pTarget = nullptr;
  if (!GetWorld()->TryGetObject(m_hTarget, pTarget))
    return false;

  out_vPosition = pTarget->GetGlobalPosition() + m_vTargetOffset;
  return true;
}

void plFixedViewComponent::OnShownByDirector(const plCameraPose& current)
{
  m_bHasSmoothed = false;
}

void plFixedViewComponent::UpdateView(plCameraViewContext& ref_context, const plCameraPose& previous, plCameraPose& out_pose)
{
  out_pose.m_vPosition = GetOwner()->GetGlobalPosition();
  out_pose.m_qRotation = GetOwner()->GetGlobalRotation();
  out_pose.m_fFieldOfView = m_fFieldOfView;
  out_pose.m_fFocusDistance = 0.0f;

  plVec3 vTarget;
  if (GetSubjectPoint(vTarget))
  {
    const plQuat qLook = plCameraPose::MakeLookRotation(vTarget - out_pose.m_vPosition);

    if (!m_bHasSmoothed || !m_TrackingSmoothing.IsPositive())
    {
      m_qSmoothed = qLook;
      m_bHasSmoothed = true;
    }
    else
    {
      const float fFollow = 1.0f - plMath::Exp(-ref_context.m_fDeltaSeconds / m_TrackingSmoothing.AsFloatInSeconds());
      m_qSmoothed = plQuat::MakeSlerp(m_qSmoothed, qLook, fFollow);
    }

    out_pose.m_qRotation = m_qSmoothed;

    const float fDistance = (vTarget - out_pose.m_vPosition).GetLength();
    out_pose.m_fFocusDistance = fDistance;

    if (m_bFrameTarget && fDistance > 0.01f)
    {
      const float fFov = 2.0f * plMath::ATan(m_fFrameSize * 0.5f / fDistance).GetDegree();
      out_pose.m_fFieldOfView = plMath::Clamp(fFov, 5.0f, m_fFieldOfView > 0.0f ? m_fFieldOfView : 90.0f);
    }
  }

  m_LastPose = out_pose;
}
