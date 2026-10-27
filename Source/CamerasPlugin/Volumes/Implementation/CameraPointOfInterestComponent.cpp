#include <CamerasPlugin/CamerasPCH.h>

#include <CamerasPlugin/Core/CameraWorldModule.h>
#include <CamerasPlugin/Volumes/CameraPointOfInterestComponent.h>
#include <Core/WorldSerializer/WorldReader.h>
#include <Core/WorldSerializer/WorldWriter.h>
#include <RendererCore/Debug/DebugRenderer.h>
#include <RendererCore/Pipeline/RenderData.h>
#include <RendererCore/RenderWorld/RenderWorld.h>

// clang-format off
PL_BEGIN_COMPONENT_TYPE(plCameraPointOfInterestComponent, 1, plComponentMode::Static)
{
  PL_BEGIN_PROPERTIES
  {
    PL_MEMBER_PROPERTY("DisplayName", m_sDisplayName),
    PL_MEMBER_PROPERTY("ActivationRadius", m_fActivationRadius)->AddAttributes(new plDefaultValueAttribute(15.0f), new plClampValueAttribute(0.1f, plVariant())),
    PL_MEMBER_PROPERTY("ViewCone", m_ViewCone)->AddAttributes(new plDefaultValueAttribute(plAngle::MakeFromDegree(360.0f)), new plClampValueAttribute(plAngle::MakeFromDegree(1.0f), plAngle::MakeFromDegree(360.0f))),
    PL_MEMBER_PROPERTY("RequireLineOfSight", m_bRequireLineOfSight)->AddAttributes(new plDefaultValueAttribute(true)),
    PL_MEMBER_PROPERTY("Priority", m_iPriority),
    PL_MEMBER_PROPERTY("FrameOffset", m_vFrameOffset),
    PL_MEMBER_PROPERTY("ZoomFieldOfView", m_fZoomFieldOfView)->AddAttributes(new plClampValueAttribute(0.0f, 170.0f)),
    PL_MEMBER_PROPERTY("HoldTime", m_HoldTime)->AddAttributes(new plClampValueAttribute(plTime::MakeZero(), plVariant())),
    PL_MEMBER_PROPERTY("AutoTrigger", m_bAutoTrigger),
  }
  PL_END_PROPERTIES;
  PL_BEGIN_MESSAGEHANDLERS
  {
    PL_MESSAGE_HANDLER(plMsgExtractRenderData, OnMsgExtractRenderData),
  }
  PL_END_MESSAGEHANDLERS;
  PL_BEGIN_ATTRIBUTES
  {
    new plCategoryAttribute("Cameras"),
    new plSphereVisualizerAttribute("ActivationRadius", plColor::Gold),
    new plSphereManipulatorAttribute("ActivationRadius"),
  }
  PL_END_ATTRIBUTES;
}
PL_END_COMPONENT_TYPE;
// clang-format on

plCameraPointOfInterestComponent::plCameraPointOfInterestComponent() = default;
plCameraPointOfInterestComponent::~plCameraPointOfInterestComponent() = default;

void plCameraPointOfInterestComponent::SerializeComponent(plWorldWriter& inout_stream) const
{
  SUPER::SerializeComponent(inout_stream);
  auto& s = inout_stream.GetStream();

  s << m_sDisplayName;
  s << m_fActivationRadius;
  s << m_ViewCone;
  s << m_bRequireLineOfSight;
  s << m_iPriority;
  s << m_vFrameOffset;
  s << m_fZoomFieldOfView;
  s << m_HoldTime;
  s << m_bAutoTrigger;
}

void plCameraPointOfInterestComponent::DeserializeComponent(plWorldReader& inout_stream)
{
  SUPER::DeserializeComponent(inout_stream);
  auto& s = inout_stream.GetStream();

  s >> m_sDisplayName;
  s >> m_fActivationRadius;
  s >> m_ViewCone;
  s >> m_bRequireLineOfSight;
  s >> m_iPriority;
  s >> m_vFrameOffset;
  s >> m_fZoomFieldOfView;
  s >> m_HoldTime;
  s >> m_bAutoTrigger;
}

void plCameraPointOfInterestComponent::OnActivated()
{
  SUPER::OnActivated();
  GetWorld()->GetOrCreateModule<plCameraWorldModule>()->RegisterPointOfInterest(this);
}

void plCameraPointOfInterestComponent::OnDeactivated()
{
  if (plCameraWorldModule* pModule = GetWorld()->GetModule<plCameraWorldModule>())
    pModule->UnregisterPointOfInterest(this);

  SUPER::OnDeactivated();
}

void plCameraPointOfInterestComponent::OnSimulationStarted()
{
  SUPER::OnSimulationStarted();
  m_bAutoTriggered = false;
}

plVec3 plCameraPointOfInterestComponent::GetFramePoint() const
{
  return GetOwner()->GetGlobalTransform().TransformPosition(m_vFrameOffset);
}

bool plCameraPointOfInterestComponent::IsInRange(const plVec3& vPlayer) const
{
  const plVec3 vToPlayer = vPlayer - GetOwner()->GetGlobalPosition();
  const float fDistance = vToPlayer.GetLength();

  if (fDistance > m_fActivationRadius)
    return false;

  if (m_ViewCone.GetDegree() >= 359.0f || fDistance < 0.001f)
    return true;

  const plVec3 vForward = GetOwner()->GetGlobalDirForwards();
  return vForward.GetAngleBetween(vToPlayer / fDistance) <= m_ViewCone * 0.5f;
}

void plCameraPointOfInterestComponent::OnMsgExtractRenderData(plMsgExtractRenderData& msg) const
{
  if (msg.m_OverrideCategory != plDefaultRenderDataCategories::Selection)
    return;

  const plUInt16 uiFrame = static_cast<plUInt16>(plRenderWorld::GetFrameCounter());
  if (m_uiDrawnFrame == uiFrame)
    return;

  m_uiDrawnFrame = uiFrame;

  const plVec3 vOrigin = GetOwner()->GetGlobalPosition();
  const plVec3 vFrame = GetFramePoint();

  plHybridArray<plDebugRenderer::Line, 16> lines;
  lines.PushBack(plDebugRenderer::Line(vOrigin, vFrame));

  // the view cone's edges, on the ground plane
  if (m_ViewCone.GetDegree() < 359.0f)
  {
    const plVec3 vForward = GetOwner()->GetGlobalDirForwards();
    const plQuat qLeft = plQuat::MakeFromAxisAndAngle(plVec3::MakeAxisZ(), m_ViewCone * 0.5f);
    const plQuat qRight = plQuat::MakeFromAxisAndAngle(plVec3::MakeAxisZ(), -m_ViewCone * 0.5f);
    lines.PushBack(plDebugRenderer::Line(vOrigin, vOrigin + (qLeft * vForward) * m_fActivationRadius));
    lines.PushBack(plDebugRenderer::Line(vOrigin, vOrigin + (qRight * vForward) * m_fActivationRadius));
  }

  plDebugRenderer::DrawLines(GetWorld(), lines, plColor::Gold);
  plDebugRenderer::DrawCross(GetWorld(), vFrame, 0.3f, plColor::Gold);
}
