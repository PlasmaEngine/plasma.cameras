#include <CamerasPlugin/CamerasPCH.h>

#include <CamerasPlugin/Core/CameraDebug.h>
#include <CamerasPlugin/Core/CameraDirectorComponent.h>
#include <CamerasPlugin/Core/CameraMessages.h>
#include <CamerasPlugin/Core/CameraViewComponent.h>
#include <CamerasPlugin/Core/CameraWorldModule.h>
#include <CamerasPlugin/Modifiers/EffectModifiers.h>
#include <CamerasPlugin/Modifiers/FramingModifiers.h>
#include <CamerasPlugin/Volumes/CameraPointOfInterestComponent.h>
#include <CamerasPlugin/Volumes/CameraVolumeComponent.h>
#include <Core/WorldSerializer/WorldReader.h>
#include <Core/WorldSerializer/WorldWriter.h>
#include <Foundation/Utilities/Stats.h>
#include <RendererCore/Components/CameraComponent.h>

// clang-format off
PL_BEGIN_COMPONENT_TYPE(plCameraDirectorComponent, 1, plComponentMode::Dynamic)
{
  PL_BEGIN_PROPERTIES
  {
    PL_MEMBER_PROPERTY("Player", m_uiPlayer)->AddAttributes(new plClampValueAttribute(0, 3)),
    PL_ACCESSOR_PROPERTY("DefaultView", DummyGetter, SetDefaultViewReference)->AddAttributes(new plGameObjectReferenceAttribute()),
    PL_MEMBER_PROPERTY("BlendTime", m_BlendTime)->AddAttributes(new plDefaultValueAttribute(plTime::MakeFromSeconds(0.6)), new plClampValueAttribute(plTime::MakeZero(), plVariant())),
    PL_ENUM_MEMBER_PROPERTY("BlendCurve", plCameraBlendCurve, m_BlendCurve),
    PL_MEMBER_PROPERTY("WriteFocus", m_bWriteFocus)->AddAttributes(new plDefaultValueAttribute(true)),
    PL_ARRAY_MEMBER_PROPERTY("Modifiers", m_Modifiers)->AddFlags(plPropertyFlags::PointerOwner)->AddAttributes(new plGroupAttribute("Modifiers")),
  }
  PL_END_PROPERTIES;
  PL_BEGIN_MESSAGEHANDLERS
  {
    PL_MESSAGE_HANDLER(plMsgCameraRequestView, OnMsgRequestView),
    PL_MESSAGE_HANDLER(plMsgCameraReleaseView, OnMsgReleaseView),
    PL_MESSAGE_HANDLER(plMsgCameraImpulse, OnMsgImpulse),
  }
  PL_END_MESSAGEHANDLERS;
  PL_BEGIN_FUNCTIONS
  {
    PL_SCRIPT_FUNCTION_PROPERTY(ActivateView, In, "ViewObject", In, "Priority", In, "Duration", In, "BlendTime"),
    PL_SCRIPT_FUNCTION_PROPERTY(ReleaseViewByName, In, "ViewObject", In, "BlendTime"),
    PL_SCRIPT_FUNCTION_PROPERTY(ReleaseRequestById, In, "RequestId"),
    PL_SCRIPT_FUNCTION_PROPERTY(SetPlayerViewByName, In, "ViewObject"),
    PL_SCRIPT_FUNCTION_PROPERTY(AddTrauma, In, "Trauma"),
    PL_SCRIPT_FUNCTION_PROPERTY(AddRecoil, In, "PitchDegrees", In, "YawDegrees"),
    PL_SCRIPT_FUNCTION_PROPERTY(SetModifierEnabled, In, "Name", In, "Enabled"),
    PL_SCRIPT_FUNCTION_PROPERTY(SetModifierWeight, In, "Name", In, "Weight"),
    PL_SCRIPT_FUNCTION_PROPERTY(IsBlending),
    PL_SCRIPT_FUNCTION_PROPERTY(LookAtObject, In, "Object"),
    PL_SCRIPT_FUNCTION_PROPERTY(SetZoomed, In, "Zoomed"),
    PL_SCRIPT_FUNCTION_PROPERTY(SetLean, In, "Lean"),
  }
  PL_END_FUNCTIONS;
  PL_BEGIN_ATTRIBUTES
  {
    new plCategoryAttribute("Cameras"),
  }
  PL_END_ATTRIBUTES;
}
PL_END_COMPONENT_TYPE;
// clang-format on

plCameraDirectorComponent::plCameraDirectorComponent() = default;

plCameraDirectorComponent::~plCameraDirectorComponent()
{
  plCameraModifierStack::DeleteAll(m_Modifiers);
}

void plCameraDirectorComponent::SerializeComponent(plWorldWriter& inout_stream) const
{
  SUPER::SerializeComponent(inout_stream);
  auto& s = inout_stream.GetStream();

  s << m_uiPlayer;
  inout_stream.WriteGameObjectHandle(m_hDefaultViewObject);
  s << m_BlendTime;
  s << m_BlendCurve;
  s << m_bWriteFocus;
  plCameraModifierStack::Write(s, m_Modifiers);
}

void plCameraDirectorComponent::DeserializeComponent(plWorldReader& inout_stream)
{
  SUPER::DeserializeComponent(inout_stream);
  auto& s = inout_stream.GetStream();

  s >> m_uiPlayer;
  m_hDefaultViewObject = inout_stream.ReadGameObjectHandle();
  s >> m_BlendTime;
  s >> m_BlendCurve;
  s >> m_bWriteFocus;
  plCameraModifierStack::Read(s, m_Modifiers);
}

void plCameraDirectorComponent::OnActivated()
{
  SUPER::OnActivated();
  GetWorld()->GetOrCreateModule<plCameraWorldModule>()->RegisterDirector(this);
}

void plCameraDirectorComponent::OnDeactivated()
{
  if (plCameraWorldModule* pModule = GetWorld()->GetModule<plCameraWorldModule>())
    pModule->UnregisterDirector(this);

  SUPER::OnDeactivated();
}

void plCameraDirectorComponent::OnSimulationStarted()
{
  SUPER::OnSimulationStarted();

  m_Requests.Clear();
  m_uiActiveRequestId = plInvalidIndex;
  m_hActiveView.Invalidate();
  m_fBlendT = 1.0f;
  m_fTime = 0.0;
  m_fCameraFov = 0.0f;
  m_bScannedMainViews = false;
  m_IgnoredMainViews.Clear();
  m_hYieldCamera.Invalidate();
  m_bYielding = false;

  m_Output.m_vPosition = GetOwner()->GetGlobalPosition();
  m_Output.m_qRotation = GetOwner()->GetGlobalRotation();
  m_bHasOutput = false;

  plCameraModifierStack::Reset(m_Modifiers);
}

void plCameraDirectorComponent::SetDefaultViewReference(const char* szReference)
{
  auto resolver = GetWorld()->GetGameObjectReferenceResolver();

  if (!resolver.IsValid())
    return;

  m_hDefaultViewObject = resolver(szReference, GetHandle(), "DefaultView");
  m_hPlayerView.Invalidate();
}

//////////////////////////////////////////////////////////////////////////

plUInt32 plCameraDirectorComponent::RequestView(plCameraViewComponent* pView, plInt32 iPriority, plTime duration, plTime blendTime, plCameraBlendCurve::Enum curve, plCameraRequestSource::Enum source)
{
  if (pView == nullptr)
    return 0;

  Request& req = m_Requests.ExpandAndGetRef();
  req.m_uiId = m_uiNextRequestId++;
  req.m_hView = pView->GetHandle();
  req.m_iPriority = iPriority;
  req.m_fEndTime = duration.IsPositive() ? m_fTime + duration.GetSeconds() : 0.0;
  req.m_BlendTime = blendTime;
  req.m_BlendCurve = curve;
  req.m_Source = source;

  return req.m_uiId;
}

void plCameraDirectorComponent::ReleaseRequest(plUInt32 uiRequestId, plTime blendTime)
{
  for (plUInt32 i = 0; i < m_Requests.GetCount(); ++i)
  {
    if (m_Requests[i].m_uiId != uiRequestId)
      continue;

    if (uiRequestId == m_uiActiveRequestId)
      m_PendingBlendTime = blendTime;

    m_Requests.RemoveAtAndCopy(i);
    return;
  }
}

void plCameraDirectorComponent::ReleaseView(plComponentHandle hView, plTime blendTime)
{
  for (plUInt32 i = m_Requests.GetCount(); i > 0; --i)
  {
    if (m_Requests[i - 1].m_hView == hView)
      ReleaseRequest(m_Requests[i - 1].m_uiId, blendTime);
  }
}

void plCameraDirectorComponent::SetPlayerView(plCameraViewComponent* pView)
{
  m_hPlayerView = pView != nullptr ? pView->GetHandle() : plComponentHandle();
}

plCameraViewComponent* plCameraDirectorComponent::GetActiveView() const
{
  plCameraViewComponent* pView = nullptr;
  const_cast<plWorld*>(GetWorld())->TryGetComponent(m_hActiveView, pView);
  return pView;
}

plCameraViewComponent* plCameraDirectorComponent::GetPlayerView() const
{
  plCameraViewComponent* pView = nullptr;
  const_cast<plWorld*>(GetWorld())->TryGetComponent(m_hPlayerView, pView);
  return pView;
}

void plCameraDirectorComponent::PlayImpulse(const plMsgCameraImpulse& msg)
{
  if (plCameraViewComponent* pView = GetActiveView())
    plCameraModifierStack::SendImpulse(pView->m_Modifiers, msg);

  plCameraModifierStack::SendImpulse(m_Modifiers, msg);
}

//////////////////////////////////////////////////////////////////////////

plCameraViewComponent* plCameraDirectorComponent::ResolvePlayerView()
{
  if (plCameraViewComponent* pView = GetPlayerView())
    return pView;

  plCameraViewComponent* pView = nullptr;

  plGameObject* pDefault = nullptr;
  if (GetWorld()->TryGetObject(m_hDefaultViewObject, pDefault))
    pDefault->TryGetComponentOfBaseType(pView);

  if (pView == nullptr)
    GetOwner()->TryGetComponentOfBaseType(pView);

  if (pView == nullptr)
  {
    for (auto it = GetOwner()->GetChildren(); it.IsValid() && pView == nullptr; ++it)
      it->TryGetComponentOfBaseType(pView);
  }

  SetPlayerView(pView);
  return pView;
}

void plCameraDirectorComponent::UpdateVolumes()
{
  plCameraWorldModule* pModule = GetWorld()->GetModule<plCameraWorldModule>();
  if (pModule == nullptr)
    return;

  // Volumes test where the player's subject is; without one, where the camera is.
  plVec3 vProbe = GetOwner()->GetGlobalPosition();
  if (const plCameraViewComponent* pPlayerView = GetPlayerView())
    pPlayerView->GetSubjectPoint(vProbe);

  for (plComponentHandle hVolume : pModule->GetVolumes())
  {
    plCameraVolumeComponent* pVolume = nullptr;
    if (!GetWorld()->TryGetComponent(hVolume, pVolume) || !pVolume->IsActiveAndSimulating())
      continue;

    const bool bInside = pVolume->Contains(vProbe);

    plUInt32 uiExisting = plInvalidIndex;
    for (plUInt32 i = 0; i < m_Requests.GetCount(); ++i)
    {
      if (m_Requests[i].m_hVolume == hVolume)
        uiExisting = i;
    }

    if (bInside && uiExisting == plInvalidIndex)
    {
      plCameraViewComponent* pView = pVolume->GetView();
      const plUInt32 uiId = RequestView(pView, pVolume->m_iPriority, plTime(), pVolume->m_BlendIn, pVolume->m_BlendCurve, plCameraRequestSource::Volume);

      if (uiId != 0)
        m_Requests.PeekBack().m_hVolume = hVolume;
    }
    else if (!bInside && uiExisting != plInvalidIndex)
    {
      ReleaseRequest(m_Requests[uiExisting].m_uiId, pVolume->m_BlendOut);
    }
  }
}

void plCameraDirectorComponent::PruneRequests()
{
  for (plUInt32 i = m_Requests.GetCount(); i > 0; --i)
  {
    const Request& req = m_Requests[i - 1];

    plCameraViewComponent* pView = nullptr;
    const bool bViewGone = !GetWorld()->TryGetComponent(req.m_hView, pView) || !pView->IsActiveAndInitialized();
    const bool bExpired = req.m_fEndTime > 0.0 && m_fTime >= req.m_fEndTime;

    if (bViewGone || bExpired)
      ReleaseRequest(req.m_uiId);
  }
}

const plCameraDirectorComponent::Request* plCameraDirectorComponent::SelectTopRequest() const
{
  const Request* pTop = nullptr;

  for (const Request& req : m_Requests)
  {
    // the newest wins a tie, and ids only grow
    if (pTop == nullptr || req.m_iPriority > pTop->m_iPriority || (req.m_iPriority == pTop->m_iPriority && req.m_uiId > pTop->m_uiId))
      pTop = &req;
  }

  return pTop;
}

plTime plCameraDirectorComponent::GetBlendTime(const Request& request, const plCameraViewComponent* pView) const
{
  if (!m_PendingBlendTime.IsNegative())
    return m_PendingBlendTime;

  if (!request.m_BlendTime.IsNegative())
    return request.m_BlendTime;

  if (pView != nullptr && pView->m_BlendTime.IsPositive())
    return pView->m_BlendTime;

  return m_BlendTime;
}

plCameraBlendCurve::Enum plCameraDirectorComponent::GetBlendCurve(const Request& request, const plCameraViewComponent* pView) const
{
  if (!request.m_BlendTime.IsNegative())
    return request.m_BlendCurve;

  if (pView != nullptr && pView->m_BlendTime.IsPositive())
    return pView->m_BlendCurve;

  return m_BlendCurve;
}

void plCameraDirectorComponent::StartBlend(const Request& request, plCameraViewComponent* pView)
{
  m_uiActiveRequestId = request.m_uiId;
  m_hActiveView = pView->GetHandle();

  m_BlendDuration = GetBlendTime(request, pView);
  m_ActiveCurve = GetBlendCurve(request, pView);
  m_PendingBlendTime = plTime::MakeFromSeconds(-1.0);

  // the very first view cuts in; afterwards everything blends from where the camera is now
  const bool bCut = !m_bHasOutput || m_ActiveCurve == plCameraBlendCurve::Cut || !m_BlendDuration.IsPositive();
  m_fBlendT = bCut ? 1.0f : 0.0f;
  m_BlendFrom = m_Output;

  pView->OnShownByDirector(m_Output);
}

//////////////////////////////////////////////////////////////////////////

void plCameraDirectorComponent::UpdateDirector(float fDeltaSeconds)
{
  m_fTime += fDeltaSeconds;

  plCameraViewComponent* pPlayerView = ResolvePlayerView();

  UpdateVolumes();
  PruneRequests();

  // the player's own view is an implicit, permanent request at priority 10
  Request playerRequest;
  playerRequest.m_uiId = 0;
  playerRequest.m_iPriority = 10;
  playerRequest.m_Source = plCameraRequestSource::Player;
  if (pPlayerView != nullptr)
    playerRequest.m_hView = pPlayerView->GetHandle();

  const Request* pTop = SelectTopRequest();
  if (pTop == nullptr || pTop->m_iPriority < playerRequest.m_iPriority)
    pTop = &playerRequest;

  plCameraViewComponent* pView = nullptr;
  if (!GetWorld()->TryGetComponent(pTop->m_hView, pView))
    return;

  if (pTop->m_uiId != m_uiActiveRequestId || pTop->m_hView != m_hActiveView)
    StartBlend(*pTop, pView);

  plCameraComponent* pCamera = nullptr;
  GetOwner()->TryGetComponentOfBaseType(pCamera);

  if (UpdateYield(pCamera))
    return;

  if (m_fCameraFov <= 0.0f)
    m_fCameraFov = pCamera != nullptr ? pCamera->GetFieldOfView() : 70.0f;

  const plCameraPlayerSettings settings = plCameraPlayerSettings::Get();
  plCameraWorldModule* pModule = GetWorld()->GetOrCreateModule<plCameraWorldModule>();

  pView->TrackSubject(fDeltaSeconds);

  plCameraViewContext viewContext;
  viewContext.m_pWorld = GetWorld();
  viewContext.m_fDeltaSeconds = fDeltaSeconds;
  viewContext.m_pDirector = this;
  viewContext.m_PlayerSettings = settings;
  viewContext.m_fLookScale = m_fLookScale;
  viewContext.m_bStereo = pCamera != nullptr && pCamera->GetCameraMode() == plCameraMode::Stereo;

  const plCameraPose previous = m_bHasOutput ? m_Output : pView->GetLastPose();
  plCameraPose viewPose = previous;
  pView->UpdateView(viewContext, previous, viewPose);

  if (viewPose.m_fFieldOfView <= 0.0f)
    viewPose.m_fFieldOfView = m_fCameraFov;

  if (m_fBlendT < 1.0f)
  {
    m_fBlendT = plMath::Min(1.0f, m_fBlendT + fDeltaSeconds / plMath::Max(0.001f, static_cast<float>(m_BlendDuration.GetSeconds())));
  }

  plCameraPose pose = plCameraPose::Blend(m_BlendFrom, viewPose, plCameraBlendCurve::Evaluate(m_ActiveCurve, m_fBlendT));

  plCameraModifierContext context;
  pView->FillModifierContext(context, fDeltaSeconds, this, settings);
  pModule->FillDisplayInfo(context, pCamera);
  context.m_bPlayerSteering = viewContext.m_bPlayerSteering;

  plCameraModifierStack::Prepare(pView->m_Modifiers, context);
  plCameraModifierStack::Prepare(m_Modifiers, context);

  plCameraModifierStack::Apply(pView->m_Modifiers, plCameraModifierStage::Framing, context, pose);
  plCameraModifierStack::Apply(m_Modifiers, plCameraModifierStage::Framing, context, pose);
  plCameraModifierStack::Apply(pView->m_Modifiers, plCameraModifierStage::Effects, context, pose);
  plCameraModifierStack::Apply(m_Modifiers, plCameraModifierStage::Effects, context, pose);

  if (context.m_bCommitLook)
  {
    plAngle yaw = context.m_CommitYaw, pitch = context.m_CommitPitch;

    if (!context.m_bCommitExplicit)
      pose.GetYawPitch(yaw, pitch);

    pView->SetLookRotation(yaw, pitch);
  }

  m_fLookScale = context.m_fLookScale;
  m_Output = pose;
  m_bHasOutput = true;

  plCameraWorldModule::WritePoseToCamera(GetOwner(), pCamera, pose, settings, m_bWriteFocus);

  PublishStats();

  if (plCameraDebug::GetLevel() > 0)
    DrawDebug(context);
}

//////////////////////////////////////////////////////////////////////////

bool plCameraDirectorComponent::UpdateYield(plCameraComponent* pOwnCamera)
{
  const plCameraComponentManager* pManager = GetWorld()->GetComponentManager<plCameraComponentManager>();
  if (pOwnCamera == nullptr || pManager == nullptr)
    return false;

  // cameras that were already Main View when play started are a scene setup, not a sequence: never yield to them
  if (!m_bScannedMainViews)
  {
    m_bScannedMainViews = true;

    for (auto it = pManager->GetComponents(); it.IsValid(); ++it)
    {
      if (&*it != pOwnCamera && it->IsActive() && it->GetUsageHint() == plCameraUsageHint::MainView)
        m_IgnoredMainViews.PushBack(it->GetHandle());
    }
  }

  // a sequence promotes its cut camera to Main View; the newest such camera is the one being shown
  const plCameraComponent* pForeign = nullptr;
  for (auto it = pManager->GetComponents(); it.IsValid(); ++it)
  {
    if (&*it != pOwnCamera && it->IsActive() && it->GetUsageHint() == plCameraUsageHint::MainView && !m_IgnoredMainViews.Contains(it->GetHandle()))
      pForeign = &*it;
  }

  if (pForeign != nullptr)
  {
    if (!m_bYielding)
    {
      // step aside so the game state picks the cut camera
      pOwnCamera->SetUsageHint(plCameraUsageHint::None);
      m_bYielding = true;
    }

    m_hYieldCamera = pForeign->GetHandle();

    // remember where the cut is, so the way back blends from it
    m_Output.m_vPosition = pForeign->GetOwner()->GetGlobalPosition();
    m_Output.m_qRotation = pForeign->GetOwner()->GetGlobalRotation();
    m_Output.m_fFieldOfView = pForeign->GetFieldOfView();
    m_bHasOutput = true;
    return true;
  }

  if (m_bYielding)
  {
    pOwnCamera->SetUsageHint(plCameraUsageHint::MainView);
    m_bYielding = false;
    m_hYieldCamera.Invalidate();

    // blend from the cut's last pose into whatever view is due now
    m_BlendFrom = m_Output;
    m_BlendDuration = m_BlendTime;
    m_ActiveCurve = m_BlendCurve;
    m_fBlendT = m_BlendTime.IsPositive() ? 0.0f : 1.0f;
  }

  return false;
}

void plCameraDirectorComponent::DrawDebug(const plCameraModifierContext& context) const
{
  plStringBuilder sText;
  sText.SetFormat("Camera Director · player {}\n", m_uiPlayer);

  auto describe = [&](const Request& req, bool bActive)
  {
    const plCameraViewComponent* pView = nullptr;
    GetWorld()->TryGetComponent(req.m_hView, pView);

    plStringBuilder sName = pView != nullptr ? pView->GetOwner()->GetName() : plStringView("(gone)");
    if (sName.IsEmpty())
      sName = "(unnamed)";

    plStringBuilder sBlend;
    if (bActive && m_fBlendT < 1.0f)
      sBlend.SetFormat("  blend {}", plArgF(m_fBlendT, 2));

    sText.AppendFormat("{} {} [{}] p{}{}\n", bActive ? ">" : " ", sName, plCameraRequestSource::GetName(req.m_Source), req.m_iPriority, sBlend);
  };

  Request playerRequest;
  playerRequest.m_iPriority = 10;
  playerRequest.m_Source = plCameraRequestSource::Player;
  playerRequest.m_hView = m_hPlayerView;
  describe(playerRequest, m_uiActiveRequestId == 0);

  for (const Request& req : m_Requests)
    describe(req, req.m_uiId == m_uiActiveRequestId);

  auto listModifiers = [&](const plDynamicArray<plCameraModifier*>& modifiers, const char* szOwner)
  {
    plStringBuilder sLine;
    for (const plCameraModifier* pModifier : modifiers)
    {
      if (pModifier == nullptr)
        continue;

      pModifier->GetDebugText(sLine);
      sText.AppendFormat("  {} {} ({}): {}\n", pModifier->m_bEnabled ? " " : "x", pModifier->GetDynamicRTTI()->GetTypeName(), szOwner, sLine);
    }
  };

  if (const plCameraViewComponent* pView = GetActiveView())
    listModifiers(pView->m_Modifiers, "view");

  listModifiers(m_Modifiers, "director");

  plAngle yaw, pitch;
  m_Output.GetYawPitch(yaw, pitch);
  sText.AppendFormat("fov {}  yaw {}  pitch {}  aspect {}", plArgF(m_Output.m_fFieldOfView, 1), plArgF(yaw.GetDegree(), 0), plArgF(pitch.GetDegree(), 0), plArgF(context.m_fAspectRatio, 2));

  plCameraDebug::DrawText(GetWorld(), sText, plColor::White);

  if (plCameraDebug::GetLevel() > 1)
  {
    if (const plCameraViewComponent* pView = GetActiveView())
      pView->DrawRig();
  }
}

void plCameraDirectorComponent::PublishStats() const
{
  plStringBuilder sKey, sValue;

  sKey.SetFormat("Cameras/Player{}/View", m_uiPlayer);
  const plCameraViewComponent* pView = GetActiveView();
  sValue = pView != nullptr ? pView->GetOwner()->GetName() : "";
  plStats::SetStat(sKey, sValue.GetData());

  sKey.SetFormat("Cameras/Player{}/Blend", m_uiPlayer);
  plStats::SetStat(sKey, m_fBlendT);

  sKey.SetFormat("Cameras/Player{}/FieldOfView", m_uiPlayer);
  plStats::SetStat(sKey, m_Output.m_fFieldOfView);
}

//////////////////////////////////////////////////////////////////////////

namespace
{
  plCameraViewComponent* FindViewByName(plWorld* pWorld, plGameObject* pRelativeTo, const char* szViewObject)
  {
    plGameObject* pObject = pWorld->SearchForObject(szViewObject, pRelativeTo);

    plCameraViewComponent* pView = nullptr;
    if (pObject != nullptr)
      pObject->TryGetComponentOfBaseType(pView);

    return pView;
  }
} // namespace

plUInt32 plCameraDirectorComponent::ActivateView(const char* szViewObject, plInt32 iPriority, float fDurationSeconds, float fBlendSeconds)
{
  plCameraViewComponent* pView = FindViewByName(GetWorld(), GetOwner(), szViewObject);

  if (pView == nullptr)
  {
    plLog::Warning("Camera Director: no camera view at '{}'.", szViewObject);
    return 0;
  }

  // priority 0 uses the view's own Priority
  const plInt32 iEffective = iPriority != 0 ? iPriority : pView->m_iPriority;
  return RequestView(pView, iEffective, plTime::MakeFromSeconds(fDurationSeconds), plTime::MakeFromSeconds(fBlendSeconds), m_BlendCurve, plCameraRequestSource::Script);
}

void plCameraDirectorComponent::ReleaseViewByName(const char* szViewObject, float fBlendSeconds)
{
  if (plCameraViewComponent* pView = FindViewByName(GetWorld(), GetOwner(), szViewObject))
    ReleaseView(pView->GetHandle(), plTime::MakeFromSeconds(fBlendSeconds));
}

void plCameraDirectorComponent::ReleaseRequestById(plUInt32 uiRequestId)
{
  ReleaseRequest(uiRequestId);
}

void plCameraDirectorComponent::SetPlayerViewByName(const char* szViewObject)
{
  SetPlayerView(FindViewByName(GetWorld(), GetOwner(), szViewObject));
}

void plCameraDirectorComponent::AddTrauma(float fTrauma)
{
  plMsgCameraImpulse msg;
  msg.m_Type = plCameraImpulseType::Shake;
  msg.m_fStrength = fTrauma;
  PlayImpulse(msg);
}

void plCameraDirectorComponent::AddRecoil(float fPitchDegrees, float fYawDegrees)
{
  plMsgCameraImpulse msg;
  msg.m_Type = plCameraImpulseType::Recoil;
  msg.m_Pitch = plAngle::MakeFromDegree(fPitchDegrees);
  msg.m_Yaw = plAngle::MakeFromDegree(fYawDegrees);
  PlayImpulse(msg);
}

void plCameraDirectorComponent::SetModifierEnabled(const char* szName, bool bEnabled)
{
  plHybridArray<plCameraModifier*, 16> modifiers;
  GetAllModifiers(modifiers);

  if (plCameraModifier* pModifier = plCameraModifierStack::FindByName(modifiers, szName))
    pModifier->m_bEnabled = bEnabled;
}

void plCameraDirectorComponent::SetModifierWeight(const char* szName, float fWeight)
{
  plHybridArray<plCameraModifier*, 16> modifiers;
  GetAllModifiers(modifiers);

  if (plCameraModifier* pModifier = plCameraModifierStack::FindByName(modifiers, szName))
    pModifier->m_fWeight = plMath::Clamp(fWeight, 0.0f, 1.0f);
}

void plCameraDirectorComponent::GetAllModifiers(plDynamicArray<plCameraModifier*>& out_modifiers) const
{
  out_modifiers.Clear();

  if (const plCameraViewComponent* pView = GetActiveView())
  {
    for (plCameraModifier* pModifier : pView->m_Modifiers)
    {
      if (pModifier != nullptr)
        out_modifiers.PushBack(pModifier);
    }
  }

  for (plCameraModifier* pModifier : m_Modifiers)
  {
    if (pModifier != nullptr)
      out_modifiers.PushBack(pModifier);
  }
}

void plCameraDirectorComponent::LookAtObject(const char* szObject)
{
  plGameObject* pObject = GetWorld()->SearchForObject(szObject, GetOwner());

  plCameraPointOfInterestComponent* pPoint = nullptr;
  if (pObject == nullptr || !pObject->TryGetComponentOfBaseType(pPoint))
  {
    plLog::Warning("Camera Director: no point of interest at '{}'.", szObject);
    return;
  }

  ForEachModifier<plCameraLookAtModifier>([&](plCameraLookAtModifier* pModifier) { pModifier->Trigger(pPoint); });
}

void plCameraDirectorComponent::SetZoomed(bool bZoomed)
{
  ForEachModifier<plCameraZoomModifier>([&](plCameraZoomModifier* pModifier) { pModifier->SetZoomed(bZoomed); });
}

void plCameraDirectorComponent::SetLean(float fLean)
{
  ForEachModifier<plCameraLeanModifier>([&](plCameraLeanModifier* pModifier) { pModifier->SetLean(fLean); });
}

plComponentHandle plCameraDirectorComponent::GetBestPointOfInterest() const
{
  plComponentHandle hBest;

  ForEachModifier<plCameraLookAtModifier>([&](plCameraLookAtModifier* pModifier)
    {
      if (hBest.IsInvalidated())
        hBest = pModifier->GetBestPointOfInterest();
    });

  return hBest;
}

void plCameraDirectorComponent::OnMsgRequestView(plMsgCameraRequestView& msg)
{
  plGameObject* pObject = nullptr;
  plCameraViewComponent* pView = nullptr;

  if (GetWorld()->TryGetObject(msg.m_hViewObject, pObject))
    pObject->TryGetComponentOfBaseType(pView);

  if (pView == nullptr)
    return;

  msg.m_uiRequestId = RequestView(pView, msg.m_iPriority, msg.m_Duration, msg.m_BlendTime, msg.m_BlendCurve, msg.m_Source);
}

void plCameraDirectorComponent::OnMsgReleaseView(plMsgCameraReleaseView& msg)
{
  if (msg.m_uiRequestId != 0)
  {
    ReleaseRequest(msg.m_uiRequestId, msg.m_BlendTime);
    return;
  }

  plGameObject* pObject = nullptr;
  plCameraViewComponent* pView = nullptr;

  if (GetWorld()->TryGetObject(msg.m_hViewObject, pObject) && pObject->TryGetComponentOfBaseType(pView))
    ReleaseView(pView->GetHandle(), msg.m_BlendTime);
}

void plCameraDirectorComponent::OnMsgImpulse(plMsgCameraImpulse& msg)
{
  PlayImpulse(msg);
}
