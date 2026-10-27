#pragma once

#include <CamerasPlugin/Core/CameraModifier.h>

/// \brief Trauma-driven shake: smooth noise on yaw, pitch, roll and (optionally) position, scaled by trauma squared.
///
/// Trauma (0-1) comes from Shake impulses (explosions, AddTrauma) and from camera shake volumes at the camera, and
/// decays by Trauma Decay per second. Squaring it keeps small amounts subtle and big ones violent.
class PL_CAMERAS_DLL plCameraShakeModifier : public plCameraModifier
{
  PL_ADD_DYNAMIC_REFLECTION(plCameraShakeModifier, plCameraModifier);
  PL_CAMERA_MODIFIER_COMMON_ACCESSORS();

public:
  plCameraShakeModifier();

  plAngle m_MaxYaw = plAngle::MakeFromDegree(4.0f);
  plAngle m_MaxPitch = plAngle::MakeFromDegree(4.0f);
  plAngle m_MaxRoll = plAngle::MakeFromDegree(6.0f);
  float m_fMaxOffset = 0.0f;
  float m_fFrequency = 9.0f;
  float m_fTraumaDecay = 0.9f;
  bool m_bUseShakeVolumes = true;

  virtual plCameraModifierStage::Enum GetStage() const override { return plCameraModifierStage::Effects; }
  virtual void Apply(plCameraModifierContext& ref_context, plCameraPose& ref_pose, float fWeight) override;
  virtual void OnImpulse(const plMsgCameraImpulse& msg) override;
  virtual void Reset() override;
  virtual bool IsRotationalEffect() const override { return true; }
  virtual void GetDebugText(plStringBuilder& out_sText) const override;

  float GetTrauma() const { return m_fTrauma; }

private:
  float m_fTrauma = 0.0f;
  float m_fVolumeTrauma = 0.0f;
  float m_fTime = 0.0f;
  plVec3 m_vLastPosition = plVec3::MakeZero();
};

/// \brief Widens the field of view as the subject speeds up past Speed Threshold, up to Max Kick at Full Speed.
class PL_CAMERAS_DLL plCameraFovKickModifier : public plCameraModifier
{
  PL_ADD_DYNAMIC_REFLECTION(plCameraFovKickModifier, plCameraModifier);
  PL_CAMERA_MODIFIER_COMMON_ACCESSORS();

public:
  plCameraFovKickModifier();

  float m_fSpeedThreshold = 4.5f;
  float m_fFullSpeed = 7.0f;
  float m_fMaxKick = 10.0f;
  plTime m_InTime = plTime::MakeFromSeconds(0.25);
  plTime m_OutTime = plTime::MakeFromSeconds(0.4);

  virtual plCameraModifierStage::Enum GetStage() const override { return plCameraModifierStage::Effects; }
  virtual void Apply(plCameraModifierContext& ref_context, plCameraPose& ref_pose, float fWeight) override;
  virtual void Reset() override { m_fCurrent = 0.0f; }
  virtual void GetDebugText(plStringBuilder& out_sText) const override;

private:
  float m_fCurrent = 0.0f;
};

/// \brief Zooms while Action is held (or SetZoomed is on): narrows the field of view, pulls a third-person camera in
/// toward its subject and slows look input.
class PL_CAMERAS_DLL plCameraZoomModifier : public plCameraModifier
{
  PL_ADD_DYNAMIC_REFLECTION(plCameraZoomModifier, plCameraModifier);
  PL_CAMERA_MODIFIER_COMMON_ACCESSORS();

public:
  plCameraZoomModifier();

  plString m_sAction = "Aim";
  float m_fFovScale = 0.6f;
  float m_fArmPull = 0.4f;
  float m_fLookScale = 0.5f;
  plTime m_BlendTime = plTime::MakeFromSeconds(0.15);

  void SetZoomed(bool bZoomed) { m_bScriptZoom = bZoomed; }

  virtual plCameraModifierStage::Enum GetStage() const override { return plCameraModifierStage::Effects; }
  virtual void Apply(plCameraModifierContext& ref_context, plCameraPose& ref_pose, float fWeight) override;
  virtual void Reset() override { m_fAmount = 0.0f; }
  virtual void GetDebugText(plStringBuilder& out_sText) const override;

private:
  bool m_bScriptZoom = false;
  float m_fAmount = 0.0f;
};

/// \brief Weapon kick: Recoil impulses push the aim up (and randomly sideways); a damped spring brings it back.
class PL_CAMERAS_DLL plCameraRecoilModifier : public plCameraModifier
{
  PL_ADD_DYNAMIC_REFLECTION(plCameraRecoilModifier, plCameraModifier);
  PL_CAMERA_MODIFIER_COMMON_ACCESSORS();

public:
  plCameraRecoilModifier();

  float m_fStiffness = 140.0f;
  float m_fDamping = 18.0f;
  plAngle m_RandomYaw = plAngle::MakeFromDegree(1.2f);

  virtual plCameraModifierStage::Enum GetStage() const override { return plCameraModifierStage::Effects; }
  virtual void Apply(plCameraModifierContext& ref_context, plCameraPose& ref_pose, float fWeight) override;
  virtual void OnImpulse(const plMsgCameraImpulse& msg) override;
  virtual void Reset() override;
  virtual bool IsRotationalEffect() const override { return true; }
  virtual void GetDebugText(plStringBuilder& out_sText) const override;

private:
  float m_fPitch = 0.0f;
  float m_fPitchVelocity = 0.0f;
  float m_fYaw = 0.0f;
  float m_fYawVelocity = 0.0f;
  plUInt32 m_uiSeed = 1;
};

/// \brief Figure-eight walking bob and a small nod, scaled by the subject's speed. Off while airborne.
class PL_CAMERAS_DLL plCameraHeadBobModifier : public plCameraModifier
{
  PL_ADD_DYNAMIC_REFLECTION(plCameraHeadBobModifier, plCameraModifier);
  PL_CAMERA_MODIFIER_COMMON_ACCESSORS();

public:
  plCameraHeadBobModifier();

  float m_fVerticalAmplitude = 0.04f;
  float m_fLateralAmplitude = 0.03f;
  float m_fStepsPerMeter = 0.6f;
  plAngle m_PitchNod = plAngle::MakeFromDegree(0.4f);
  float m_fFullSpeed = 5.0f;

  virtual plCameraModifierStage::Enum GetStage() const override { return plCameraModifierStage::Effects; }
  virtual void Apply(plCameraModifierContext& ref_context, plCameraPose& ref_pose, float fWeight) override;
  virtual void Reset() override;
  virtual bool IsRotationalEffect() const override { return true; }
  virtual void GetDebugText(plStringBuilder& out_sText) const override;

private:
  float m_fPhase = 0.0f;
  float m_fAmount = 0.0f;
};

/// \brief Leans the camera sideways with roll while Lean Left / Lean Right are held (or SetLean), never through walls.
/// Dutch Angle adds a constant roll on top.
class PL_CAMERAS_DLL plCameraLeanModifier : public plCameraModifier
{
  PL_ADD_DYNAMIC_REFLECTION(plCameraLeanModifier, plCameraModifier);
  PL_CAMERA_MODIFIER_COMMON_ACCESSORS();

public:
  plCameraLeanModifier();

  plString m_sLeanLeftAction = "LeanLeft";
  plString m_sLeanRightAction = "LeanRight";
  plAngle m_MaxRoll = plAngle::MakeFromDegree(12.0f);
  float m_fSideOffset = 0.35f;
  plTime m_BlendTime = plTime::MakeFromSeconds(0.15);
  plAngle m_DutchAngle;
  plUInt8 m_uiCollisionLayer = 0;

  /// \brief -1 leans fully left, 1 fully right, added to the actions.
  void SetLean(float fLean) { m_fScriptLean = plMath::Clamp(fLean, -1.0f, 1.0f); }

  virtual plCameraModifierStage::Enum GetStage() const override { return plCameraModifierStage::Effects; }
  virtual void Apply(plCameraModifierContext& ref_context, plCameraPose& ref_pose, float fWeight) override;
  virtual void Reset() override { m_fLean = 0.0f; }
  virtual bool IsRotationalEffect() const override { return true; }
  virtual void GetDebugText(plStringBuilder& out_sText) const override;

private:
  float m_fScriptLean = 0.0f;
  float m_fLean = 0.0f;
};

/// \brief Dips the camera when the subject lands (or on a Landing impulse), springing back.
class PL_CAMERAS_DLL plCameraLandingModifier : public plCameraModifier
{
  PL_ADD_DYNAMIC_REFLECTION(plCameraLandingModifier, plCameraModifier);
  PL_CAMERA_MODIFIER_COMMON_ACCESSORS();

public:
  plCameraLandingModifier();

  float m_fMinSpeed = 3.0f;
  float m_fDipPerSpeed = 0.03f;
  float m_fMaxDip = 0.35f;
  plAngle m_PitchPerMeter = plAngle::MakeFromDegree(12.0f);
  float m_fStiffness = 120.0f;
  float m_fDamping = 14.0f;

  virtual plCameraModifierStage::Enum GetStage() const override { return plCameraModifierStage::Effects; }
  virtual void Apply(plCameraModifierContext& ref_context, plCameraPose& ref_pose, float fWeight) override;
  virtual void OnImpulse(const plMsgCameraImpulse& msg) override;
  virtual void Reset() override;
  virtual bool IsRotationalEffect() const override { return true; }
  virtual void GetDebugText(plStringBuilder& out_sText) const override;

private:
  void Kick(float fFallSpeed);

  float m_fOffset = 0.0f;
  float m_fVelocity = 0.0f;
};

/// \brief Critically damped smoothing of position and rotation, for views without lag of their own.
class PL_CAMERAS_DLL plCameraDampingModifier : public plCameraModifier
{
  PL_ADD_DYNAMIC_REFLECTION(plCameraDampingModifier, plCameraModifier);
  PL_CAMERA_MODIFIER_COMMON_ACCESSORS();

public:
  plCameraDampingModifier();

  plTime m_PositionHalfLife = plTime::MakeFromSeconds(0.08);
  plTime m_RotationHalfLife = plTime::MakeFromSeconds(0.05);

  virtual plCameraModifierStage::Enum GetStage() const override { return plCameraModifierStage::Effects; }
  virtual void Apply(plCameraModifierContext& ref_context, plCameraPose& ref_pose, float fWeight) override;
  virtual void Reset() override { m_bHasState = false; }
  virtual void GetDebugText(plStringBuilder& out_sText) const override;

private:
  plVec3 m_vPosition = plVec3::MakeZero();
  plQuat m_qRotation = plQuat::MakeIdentity();
  bool m_bHasState = false;
};
