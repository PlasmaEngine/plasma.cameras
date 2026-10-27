#pragma once

#include <CamerasPlugin/Core/CameraInput.h>
#include <CamerasPlugin/Core/CameraPose.h>
#include <Core/World/WorldModule.h>

class plCameraComponent;
class plCameraDirectorComponent;
class plCameraViewComponent;
class plCameraVolumeComponent;
class plCameraPointOfInterestComponent;
struct plCameraModifierContext;
struct plMsgCameraImpulse;

/// \brief Runs the gameplay cameras of a world once per frame, after everything has moved.
///
/// Views, directors, camera volumes and points of interest register here when they activate. Each frame every
/// director picks, blends and modifies its view. When a world has no director at all, each view drives its own
/// object instead (standalone mode).
class PL_CAMERAS_DLL plCameraWorldModule final : public plWorldModule
{
  PL_DECLARE_WORLD_MODULE();
  PL_ADD_DYNAMIC_REFLECTION(plCameraWorldModule, plWorldModule);

public:
  plCameraWorldModule(plWorld* pWorld);
  ~plCameraWorldModule();

  virtual void Initialize() override;

  void RegisterView(plCameraViewComponent* pView);
  void UnregisterView(plCameraViewComponent* pView);
  void RegisterDirector(plCameraDirectorComponent* pDirector);
  void UnregisterDirector(plCameraDirectorComponent* pDirector);
  void RegisterVolume(plCameraVolumeComponent* pVolume);
  void UnregisterVolume(plCameraVolumeComponent* pVolume);
  void RegisterPointOfInterest(plCameraPointOfInterestComponent* pPoi);
  void UnregisterPointOfInterest(plCameraPointOfInterestComponent* pPoi);

  const plDynamicArray<plComponentHandle>& GetVolumes() const { return m_Volumes; }
  const plDynamicArray<plComponentHandle>& GetPointsOfInterest() const { return m_PointsOfInterest; }
  const plDynamicArray<plComponentHandle>& GetDirectors() const { return m_Directors; }

  /// \brief The director for a player, or nullptr.
  plCameraDirectorComponent* FindDirector(plUInt8 uiPlayer) const;

  /// \brief Sends an impulse to every director.
  void PlayImpulse(const plMsgCameraImpulse& msg);

  /// \brief Fills the aspect ratio and field-of-view axis of the view the camera renders to.
  void FillDisplayInfo(plCameraModifierContext& ref_context, const plCameraComponent* pCamera) const;

  /// \brief Moves \a pOwner to the pose and sets the camera's field of view (plus the player's offset) and, if asked, focus.
  ///
  /// In stereo only the position and yaw are written: the headset owns the rest.
  static void WritePoseToCamera(plGameObject* pOwner, plCameraComponent* pCamera, const plCameraPose& pose, const plCameraPlayerSettings& settings, bool bWriteFocus);

private:
  void Update(const plWorldModule::UpdateContext& context);

  plDynamicArray<plComponentHandle> m_Views;
  plDynamicArray<plComponentHandle> m_Directors;
  plDynamicArray<plComponentHandle> m_Volumes;
  plDynamicArray<plComponentHandle> m_PointsOfInterest;
};
