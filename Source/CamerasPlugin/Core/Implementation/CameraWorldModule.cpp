#include <CamerasPlugin/CamerasPCH.h>

#include <CamerasPlugin/Core/CameraDirectorComponent.h>
#include <CamerasPlugin/Core/CameraMessages.h>
#include <CamerasPlugin/Core/CameraModifier.h>
#include <CamerasPlugin/Core/CameraViewComponent.h>
#include <CamerasPlugin/Core/CameraWorldModule.h>
#include <CamerasPlugin/Volumes/CameraPointOfInterestComponent.h>
#include <CamerasPlugin/Volumes/CameraVolumeComponent.h>
#include <Core/World/World.h>
#include <RendererCore/Components/CameraComponent.h>
#include <RendererCore/Pipeline/View.h>
#include <RendererCore/RenderWorld/RenderWorld.h>

// clang-format off
PL_IMPLEMENT_WORLD_MODULE(plCameraWorldModule);
PL_BEGIN_DYNAMIC_REFLECTED_TYPE(plCameraWorldModule, 1, plRTTINoAllocator)
PL_END_DYNAMIC_REFLECTED_TYPE;
// clang-format on

plCameraWorldModule::plCameraWorldModule(plWorld* pWorld)
  : plWorldModule(pWorld)
{
}

plCameraWorldModule::~plCameraWorldModule() = default;

void plCameraWorldModule::Initialize()
{
  SUPER::Initialize();

  // After transforms are final, so cameras follow where their subjects ended up this frame.
  auto desc = PL_CREATE_MODULE_UPDATE_FUNCTION_DESC(plCameraWorldModule::Update, this);
  desc.m_Phase = UpdateFunctionDesc::Phase::PostTransform;
  desc.m_bOnlyUpdateWhenSimulating = true;
  desc.m_fPriority = -100.0f;

  RegisterUpdateFunction(desc);
}

namespace
{
  void AddHandle(plDynamicArray<plComponentHandle>& ref_handles, plComponentHandle hComponent)
  {
    if (!ref_handles.Contains(hComponent))
      ref_handles.PushBack(hComponent);
  }
} // namespace

void plCameraWorldModule::RegisterView(plCameraViewComponent* pView)
{
  AddHandle(m_Views, pView->GetHandle());
}

void plCameraWorldModule::UnregisterView(plCameraViewComponent* pView)
{
  m_Views.RemoveAndSwap(pView->GetHandle());
}

void plCameraWorldModule::RegisterDirector(plCameraDirectorComponent* pDirector)
{
  AddHandle(m_Directors, pDirector->GetHandle());
}

void plCameraWorldModule::UnregisterDirector(plCameraDirectorComponent* pDirector)
{
  m_Directors.RemoveAndCopy(pDirector->GetHandle());
}

void plCameraWorldModule::RegisterVolume(plCameraVolumeComponent* pVolume)
{
  AddHandle(m_Volumes, pVolume->GetHandle());
}

void plCameraWorldModule::UnregisterVolume(plCameraVolumeComponent* pVolume)
{
  m_Volumes.RemoveAndSwap(pVolume->GetHandle());
}

void plCameraWorldModule::RegisterPointOfInterest(plCameraPointOfInterestComponent* pPoi)
{
  AddHandle(m_PointsOfInterest, pPoi->GetHandle());
}

void plCameraWorldModule::UnregisterPointOfInterest(plCameraPointOfInterestComponent* pPoi)
{
  m_PointsOfInterest.RemoveAndSwap(pPoi->GetHandle());
}

plCameraDirectorComponent* plCameraWorldModule::FindDirector(plUInt8 uiPlayer) const
{
  for (plComponentHandle hDirector : m_Directors)
  {
    plCameraDirectorComponent* pDirector = nullptr;
    if (const_cast<plWorld*>(GetWorld())->TryGetComponent(hDirector, pDirector) && pDirector->m_uiPlayer == uiPlayer)
      return pDirector;
  }

  return nullptr;
}

void plCameraWorldModule::PlayImpulse(const plMsgCameraImpulse& msg)
{
  for (plComponentHandle hDirector : m_Directors)
  {
    plCameraDirectorComponent* pDirector = nullptr;
    if (GetWorld()->TryGetComponent(hDirector, pDirector))
      pDirector->PlayImpulse(msg);
  }
}

void plCameraWorldModule::FillDisplayInfo(plCameraModifierContext& ref_context, const plCameraComponent* pCamera) const
{
  ref_context.m_fAspectRatio = 16.0f / 9.0f;
  ref_context.m_bFovIsHorizontal = false;
  ref_context.m_bStereo = false;

  if (pCamera != nullptr)
  {
    ref_context.m_bFovIsHorizontal = pCamera->GetCameraMode() == plCameraMode::PerspectiveFixedFovX;
    ref_context.m_bStereo = pCamera->GetCameraMode() == plCameraMode::Stereo;
  }

  if (const plView* pView = plRenderWorld::GetViewByUsageHint(plCameraUsageHint::MainView, plCameraUsageHint::EditorView, GetWorld()))
  {
    const plRectFloat viewport = pView->GetViewport();

    if (viewport.height > 0.0f)
      ref_context.m_fAspectRatio = viewport.width / viewport.height;
  }
}

void plCameraWorldModule::WritePoseToCamera(plGameObject* pOwner, plCameraComponent* pCamera, const plCameraPose& pose, const plCameraPlayerSettings& settings, bool bWriteFocus)
{
  const bool bStereo = pCamera != nullptr && pCamera->GetCameraMode() == plCameraMode::Stereo;

  if (bStereo)
  {
    plAngle yaw, pitch;
    pose.GetYawPitch(yaw, pitch);
    pOwner->SetGlobalTransform(plTransform(pose.m_vPosition, plCameraPose::MakeRotation(yaw, plAngle())));
    return;
  }

  pOwner->SetGlobalTransform(plTransform(pose.m_vPosition, pose.m_qRotation));

  if (pCamera == nullptr)
    return;

  const float fFov = plMath::Clamp(pose.m_fFieldOfView + settings.m_fFieldOfViewOffset, 1.0f, 170.0f);
  if (!plMath::IsEqual(pCamera->GetFieldOfView(), fFov, 0.001f))
    pCamera->SetFieldOfView(fFov);

  if (pose.m_fNearPlane > 0.0f && !plMath::IsEqual(pCamera->GetNearPlane(), pose.m_fNearPlane, 0.0001f))
    pCamera->SetNearPlane(pose.m_fNearPlane);

  if (bWriteFocus && pose.m_fFocusDistance > 0.0f)
  {
    if (pCamera->GetFocusMode() != plCameraFocusMode::Manual)
      pCamera->SetFocusMode(plCameraFocusMode::Manual);

    if (!plMath::IsEqual(pCamera->GetFocusDistance(), pose.m_fFocusDistance, 0.001f))
      pCamera->SetFocusDistance(pose.m_fFocusDistance);
  }
}

void plCameraWorldModule::Update(const plWorldModule::UpdateContext& context)
{
  const float fDeltaSeconds = static_cast<float>(GetWorld()->GetClock().GetTimeDiff().GetSeconds());

  if (!m_Directors.IsEmpty())
  {
    for (plComponentHandle hDirector : m_Directors)
    {
      plCameraDirectorComponent* pDirector = nullptr;
      if (GetWorld()->TryGetComponent(hDirector, pDirector) && pDirector->IsActiveAndSimulating())
        pDirector->UpdateDirector(fDeltaSeconds);
    }

    return;
  }

  // standalone: without a director, every view moves its own object
  for (plComponentHandle hView : m_Views)
  {
    plCameraViewComponent* pView = nullptr;
    if (GetWorld()->TryGetComponent(hView, pView) && pView->IsActiveAndSimulating())
      pView->UpdateStandalone(fDeltaSeconds);
  }
}
