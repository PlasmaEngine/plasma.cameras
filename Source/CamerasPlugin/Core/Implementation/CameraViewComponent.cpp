#include <CamerasPlugin/CamerasPCH.h>

#include <CamerasPlugin/Core/CameraDebug.h>
#include <CamerasPlugin/Core/CameraViewComponent.h>
#include <CamerasPlugin/Core/CameraWorldModule.h>
#include <Core/WorldSerializer/WorldReader.h>
#include <Core/WorldSerializer/WorldWriter.h>
#include <GameEngine/Gameplay/InputComponent.h>
#include <RendererCore/Components/CameraComponent.h>
#include <RendererCore/Pipeline/RenderData.h>
#include <RendererCore/RenderWorld/RenderWorld.h>

// clang-format off
PL_BEGIN_ABSTRACT_COMPONENT_TYPE(plCameraViewComponent, 1)
{
  PL_BEGIN_PROPERTIES
  {
    PL_MEMBER_PROPERTY("FieldOfView", m_fFieldOfView)->AddAttributes(new plDefaultValueAttribute(0.0f), new plClampValueAttribute(0.0f, 170.0f), new plGroupAttribute("View")),
    PL_MEMBER_PROPERTY("Priority", m_iPriority)->AddAttributes(new plDefaultValueAttribute(50), new plGroupAttribute("View")),
    PL_MEMBER_PROPERTY("BlendTime", m_BlendTime)->AddAttributes(new plGroupAttribute("View"), new plClampValueAttribute(plTime::MakeZero(), plVariant())),
    PL_ENUM_MEMBER_PROPERTY("BlendCurve", plCameraBlendCurve, m_BlendCurve)->AddAttributes(new plGroupAttribute("View")),
    PL_ARRAY_MEMBER_PROPERTY("Modifiers", m_Modifiers)->AddFlags(plPropertyFlags::PointerOwner)->AddAttributes(new plGroupAttribute("Modifiers")),
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
  }
  PL_END_ATTRIBUTES;
}
PL_END_ABSTRACT_COMPONENT_TYPE;
// clang-format on

plCameraViewComponent::plCameraViewComponent() = default;

plCameraViewComponent::~plCameraViewComponent()
{
  plCameraModifierStack::DeleteAll(m_Modifiers);
}

void plCameraViewComponent::SerializeComponent(plWorldWriter& inout_stream) const
{
  SUPER::SerializeComponent(inout_stream);
  auto& s = inout_stream.GetStream();

  s << m_fFieldOfView;
  s << m_iPriority;
  s << m_BlendTime;
  s << m_BlendCurve;
  plCameraModifierStack::Write(s, m_Modifiers);
}

void plCameraViewComponent::DeserializeComponent(plWorldReader& inout_stream)
{
  SUPER::DeserializeComponent(inout_stream);

  if (inout_stream.GetComponentTypeVersion(GetDynamicRTTI()) < GetFirstVersionWithViewData())
    return;

  auto& s = inout_stream.GetStream();

  s >> m_fFieldOfView;
  s >> m_iPriority;
  s >> m_BlendTime;
  s >> m_BlendCurve;
  plCameraModifierStack::Read(s, m_Modifiers);
}

void plCameraViewComponent::OnActivated()
{
  SUPER::OnActivated();

  m_SubjectTracker.Reset();
  m_LastPose.m_vPosition = GetOwner()->GetGlobalPosition();
  m_LastPose.m_qRotation = GetOwner()->GetGlobalRotation();
  m_LastPose.m_fFieldOfView = m_fFieldOfView;

  GetWorld()->GetOrCreateModule<plCameraWorldModule>()->RegisterView(this);
}

void plCameraViewComponent::OnDeactivated()
{
  if (plCameraWorldModule* pModule = GetWorld()->GetModule<plCameraWorldModule>())
    pModule->UnregisterView(this);

  SUPER::OnDeactivated();
}

bool plCameraViewComponent::GetSubjectPoint(plVec3& out_vPosition) const
{
  const plGameObject* pSubject = nullptr;
  if (!GetWorld()->TryGetObject(GetSubjectObject(), pSubject))
    return false;

  out_vPosition = pSubject->GetGlobalPosition();
  return true;
}

void plCameraViewComponent::DrawRig() const
{
  plCameraDebug::DrawPoseFrustum(GetWorld(), m_LastPose, plColor::CornflowerBlue);
}

const plInputComponent* plCameraViewComponent::FindInput() const
{
  const plGameObject* pSubject = nullptr;
  GetWorld()->TryGetObject(GetSubjectObject(), pSubject);

  return plFindCameraInput(pSubject, GetOwner());
}

const plCameraSubjectInfo& plCameraViewComponent::TrackSubject(float fDeltaSeconds)
{
  plVec3 vSubject;
  if (GetSubjectPoint(vSubject))
  {
    return m_SubjectTracker.Update(GetWorld(), GetSubjectObject(), vSubject, fDeltaSeconds);
  }

  m_SubjectTracker.Reset();
  return m_SubjectTracker.GetInfo();
}

void plCameraViewComponent::FillModifierContext(plCameraModifierContext& out_context, float fDeltaSeconds, plCameraDirectorComponent* pDirector, const plCameraPlayerSettings& settings) const
{
  out_context.m_pWorld = const_cast<plWorld*>(GetWorld());
  out_context.m_fDeltaSeconds = fDeltaSeconds;
  out_context.m_pDirector = pDirector;
  out_context.m_pView = const_cast<plCameraViewComponent*>(this);
  out_context.m_ViewKind = GetViewKind();
  out_context.m_pInput = FindInput();
  out_context.m_Subject = m_SubjectTracker.GetInfo();
  out_context.m_PlayerSettings = settings;
}

void plCameraViewComponent::UpdateStandalone(float fDeltaSeconds)
{
  plCameraWorldModule* pModule = GetWorld()->GetOrCreateModule<plCameraWorldModule>();
  const plCameraPlayerSettings settings = plCameraPlayerSettings::Get();

  plCameraComponent* pCamera = nullptr;
  GetOwner()->TryGetComponentOfBaseType(pCamera);

  TrackSubject(fDeltaSeconds);

  plCameraViewContext viewContext;
  viewContext.m_pWorld = GetWorld();
  viewContext.m_fDeltaSeconds = fDeltaSeconds;
  viewContext.m_PlayerSettings = settings;
  viewContext.m_bStereo = pCamera != nullptr && pCamera->GetCameraMode() == plCameraMode::Stereo;

  if (m_fStandaloneCameraFov <= 0.0f)
    m_fStandaloneCameraFov = pCamera != nullptr ? pCamera->GetFieldOfView() : 70.0f;

  const plCameraPose previous = m_LastPose;
  plCameraPose pose = previous;
  UpdateView(viewContext, previous, pose);

  if (pose.m_fFieldOfView <= 0.0f)
    pose.m_fFieldOfView = m_fStandaloneCameraFov;

  plCameraModifierContext context;
  FillModifierContext(context, fDeltaSeconds, nullptr, settings);
  pModule->FillDisplayInfo(context, pCamera);
  context.m_bStereo = viewContext.m_bStereo;
  context.m_bPlayerSteering = viewContext.m_bPlayerSteering;

  plCameraModifierStack::Prepare(m_Modifiers, context);
  plCameraModifierStack::Apply(m_Modifiers, plCameraModifierStage::Framing, context, pose);
  plCameraModifierStack::Apply(m_Modifiers, plCameraModifierStage::Effects, context, pose);

  if (context.m_bCommitLook)
  {
    plAngle yaw = context.m_CommitYaw, pitch = context.m_CommitPitch;

    if (!context.m_bCommitExplicit)
      pose.GetYawPitch(yaw, pitch);

    SetLookRotation(yaw, pitch);
  }

  m_LastPose = pose;
  plCameraWorldModule::WritePoseToCamera(GetOwner(), pCamera, pose, settings, false);
}

void plCameraViewComponent::OnMsgExtractRenderData(plMsgExtractRenderData& msg) const
{
  if (msg.m_OverrideCategory != plDefaultRenderDataCategories::Selection)
    return;

  // every view of the editor extracts the selection, so draw once per frame
  const plUInt16 uiFrame = static_cast<plUInt16>(plRenderWorld::GetFrameCounter());
  if (m_uiDrawnFrame == uiFrame)
    return;

  m_uiDrawnFrame = uiFrame;
  DrawRig();
}

//////////////////////////////////////////////////////////////////////////

plGameObjectHandle plCameraReference::Resolve(const plComponent* pComponent, const char* szReference, const char* szProperty)
{
  if (szReference == nullptr || plStringUtils::IsNullOrEmpty(szReference))
    return plGameObjectHandle();

  auto resolver = pComponent->GetWorld()->GetGameObjectReferenceResolver();

  if (resolver.IsValid())
    return resolver(szReference, pComponent->GetHandle(), szProperty);

  // at runtime there is no resolver, references are plain object paths
  plGameObject* pObject = const_cast<plWorld*>(pComponent->GetWorld())->SearchForObject(szReference, const_cast<plGameObject*>(pComponent->GetOwner()));
  return pObject != nullptr ? pObject->GetHandle() : plGameObjectHandle();
}
