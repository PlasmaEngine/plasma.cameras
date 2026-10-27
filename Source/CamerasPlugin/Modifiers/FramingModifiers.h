#pragma once

#include <CamerasPlugin/Core/CameraModifier.h>
#include <Core/World/Declarations.h>

class plCameraPointOfInterestComponent;

/// \brief An extra subject for the Composition modifier: an object, where on screen it goes, and how much it matters.
struct PL_CAMERAS_DLL plCameraFramingSubject
{
  /// Object path or name, searched relative to the camera.
  plString m_sObject;
  plVec3 m_vOffset = plVec3::MakeZero();
  plEnum<plCameraScreenAnchor> m_Anchor = plCameraScreenAnchor::UpperRight;
  plVec2 m_vCustomAnchor = plVec2::MakeZero();
  float m_fWeight = 1.0f;

  /// Kept on screen even when the layout cannot be met exactly.
  bool m_bMustKeep = false;
};

PL_DECLARE_REFLECTABLE_TYPE(PL_CAMERAS_DLL, plCameraFramingSubject);

/// \brief Places subjects on screen: a soft center for the view's subject alone, rule-of-thirds layouts for several.
///
/// Alone, the subject sits at Solo Point: the camera does not turn while it stays inside the dead zone, turns with
/// Damping in the soft zone, and never lets it leave the soft zone. With a second subject (from Look At, Lock-On or
/// the Subjects list) the camera also moves: it is placed on the orbit around the subject where both land on their
/// anchors, within Min/Max Distance, then the arm is swept for collision.
class PL_CAMERAS_DLL plCameraCompositionModifier : public plCameraModifier
{
  PL_ADD_DYNAMIC_REFLECTION(plCameraCompositionModifier, plCameraModifier);
  PL_CAMERA_MODIFIER_COMMON_ACCESSORS();

public:
  plCameraCompositionModifier();

  plEnum<plCameraSoloPoint> m_SoloPoint;
  plVec2 m_vDeadZone = plVec2(0.12f, 0.10f);
  float m_fSoftZone = 0.55f;
  plTime m_Damping = plTime::MakeFromSeconds(0.35);

  plEnum<plCameraFramingLayout> m_Layout;
  plEnum<plCameraMirrorMode> m_Mirror;
  plDynamicArray<plCameraFramingSubject> m_Subjects;
  float m_fMinDistance = 2.5f;
  float m_fMaxDistance = 10.0f;

  /// Widen the field of view up to this many degrees when two subjects cannot be framed otherwise. Zero never widens.
  float m_fMaxFieldOfView = 0.0f;
  plUInt8 m_uiCollisionLayer = 0;

  virtual plCameraModifierStage::Enum GetStage() const override { return plCameraModifierStage::Framing; }
  virtual void Prepare(plCameraModifierContext& ref_context) override;
  virtual void Apply(plCameraModifierContext& ref_context, plCameraPose& ref_pose, float fWeight) override;
  virtual void Reset() override;
  virtual void GetDebugText(plStringBuilder& out_sText) const override;

private:
  plVec2 GetSoloAnchor(const plCameraModifierContext& context, const plCameraPose& pose);
  void ApplySolo(plCameraModifierContext& ref_context, plCameraPose& ref_pose, float fWeight, const plVec2& vAnchor);
  void ApplyPair(plCameraModifierContext& ref_context, plCameraPose& ref_pose, float fWeight, const plVec3& vTarget, const plVec2& vPlayerAnchor,
    const plVec2& vTargetAnchor, float fPairWeight, const plVec2& vSoloAnchor);
  bool GetConfiguredSubject(const plCameraModifierContext& context, plVec3& out_vPosition, plVec2& out_vAnchor, plVec2& out_vPlayerAnchor);
  float SweepFromSubject(const plCameraModifierContext& context, const plVec3& vFrom, const plVec3& vTo) const;

  plQuat m_qAim = plQuat::MakeIdentity();
  plVec3 m_vLastPosition = plVec3::MakeZero();
  bool m_bHasAim = false;
  float m_fLeadSide = 1.0f;
  float m_fSoloX = 0.0f;
  float m_fMirror = 1.0f;
  bool m_bMirrorDecided = false;
  plVec3 m_vSolved = plVec3::MakeZero();
  bool m_bHasSolved = false;
  float m_fLastError = 0.0f;
  const char* m_szState = "-";
};

/// \brief "Press F to look at": frames the best point of interest in range when Action is pressed.
///
/// Balanced framing hands the point to the Composition modifier as a second subject laid out by Layout, so the
/// camera moves to show the player and the point together. Aim framing (or no Composition in the stack) turns to
/// the point and zooms. It holds for Duration or while held, ends early on look input, and then keeps the new
/// direction or returns.
class PL_CAMERAS_DLL plCameraLookAtModifier : public plCameraModifier
{
  PL_ADD_DYNAMIC_REFLECTION(plCameraLookAtModifier, plCameraModifier);
  PL_CAMERA_MODIFIER_COMMON_ACCESSORS();

public:
  plCameraLookAtModifier();

  plString m_sAction = "LookAtInterest";
  plEnum<plCameraLookAtFraming> m_Framing;
  plEnum<plCameraFramingLayout> m_Layout;
  plEnum<plCameraMirrorMode> m_Mirror;
  plTime m_BlendIn = plTime::MakeFromSeconds(0.6);
  plEnum<plCameraLookAtHold> m_Hold;
  plTime m_Duration = plTime::MakeFromSeconds(3.0);
  float m_fZoomFieldOfView = 45.0f;
  plEnum<plCameraLookAtRelease> m_Release;
  bool m_bBreakOnInput = true;

  /// \brief Frames a specific point of interest now, without the key.
  void Trigger(plCameraPointOfInterestComponent* pPoint);

  /// \brief The point a key press would frame right now, for the game's prompt. Invalid when none is in range.
  plComponentHandle GetBestPointOfInterest() const { return m_hBest; }

  bool IsActive() const { return m_bActive; }

  virtual plCameraModifierStage::Enum GetStage() const override { return plCameraModifierStage::Framing; }
  virtual void Prepare(plCameraModifierContext& ref_context) override;
  virtual void Apply(plCameraModifierContext& ref_context, plCameraPose& ref_pose, float fWeight) override;
  virtual void Reset() override;
  virtual void GetDebugText(plStringBuilder& out_sText) const override;

private:
  void FindBestPoint(plCameraModifierContext& ref_context);
  void Start(plCameraModifierContext& ref_context, plComponentHandle hPoint);
  void End(plCameraModifierContext& ref_context);
  bool IsVisible(const plCameraModifierContext& context, const plVec3& vPoint) const;

  plComponentHandle m_hBest;
  plComponentHandle m_hTarget;
  plComponentHandle m_hPendingTrigger;
  plVec3 m_vTarget = plVec3::MakeZero();
  bool m_bActive = false;
  bool m_bWasDown = false;
  float m_fWeight = 0.0f;
  float m_fElapsed = 0.0f;
  float m_fHoldSeconds = 0.0f;
  float m_fMirror = 1.0f;
  float m_fZoom = 0.0f;
  plAngle m_SavedYaw;
  plAngle m_SavedPitch;
  plCameraPose m_LastPose;
  bool m_bHasLastPose = false;
};

/// \brief A slow, weak pull toward the nearest point of interest while the player is not steering. Never takes control.
class PL_CAMERAS_DLL plCameraLookHintModifier : public plCameraModifier
{
  PL_ADD_DYNAMIC_REFLECTION(plCameraLookHintModifier, plCameraModifier);
  PL_CAMERA_MODIFIER_COMMON_ACCESSORS();

public:
  plCameraLookHintModifier();

  plAngle m_Speed = plAngle::MakeFromDegree(8.0f);
  plTime m_IdleTime = plTime::MakeFromSeconds(1.5);

  virtual plCameraModifierStage::Enum GetStage() const override { return plCameraModifierStage::Framing; }
  virtual void Apply(plCameraModifierContext& ref_context, plCameraPose& ref_pose, float fWeight) override;
  virtual void Reset() override { m_fIdle = 0.0f; }
  virtual void GetDebugText(plStringBuilder& out_sText) const override;

private:
  float m_fIdle = 0.0f;
};

/// \brief Keeps the depth of field on what matters, pulling focus smoothly: the subject (or the Look At target while
/// one is framed), or whatever is under the crosshair.
class PL_CAMERAS_DLL plCameraFocusModifier : public plCameraModifier
{
  PL_ADD_DYNAMIC_REFLECTION(plCameraFocusModifier, plCameraModifier);
  PL_CAMERA_MODIFIER_COMMON_ACCESSORS();

public:
  plCameraFocusModifier();

  plEnum<plCameraFocusTarget> m_Target;
  plTime m_PullTime = plTime::MakeFromSeconds(0.4);
  float m_fMaxDistance = 200.0f;

  virtual plCameraModifierStage::Enum GetStage() const override { return plCameraModifierStage::Framing; }
  virtual void Apply(plCameraModifierContext& ref_context, plCameraPose& ref_pose, float fWeight) override;
  virtual void Reset() override { m_fDistance = 0.0f; }
  virtual void GetDebugText(plStringBuilder& out_sText) const override;

private:
  float m_fDistance = 0.0f;
  plString m_sFocusName;
};

/// \brief Locks onto a target (an object with Target Tag in front of the camera) and keeps it framed with the player,
/// through the Composition modifier. Action toggles it (or is held, with Toggle off); Switch Action picks the next.
class PL_CAMERAS_DLL plCameraLockOnModifier : public plCameraModifier
{
  PL_ADD_DYNAMIC_REFLECTION(plCameraLockOnModifier, plCameraModifier);
  PL_CAMERA_MODIFIER_COMMON_ACCESSORS();

public:
  plCameraLockOnModifier();

  plString m_sAction = "LockOn";
  plString m_sSwitchAction;
  bool m_bToggle = true;
  plString m_sTargetTag = "LockOnTarget";
  float m_fMaxDistance = 25.0f;
  plVec3 m_vTargetOffset = plVec3(0, 0, 1.2f);
  plEnum<plCameraFramingLayout> m_Layout = plCameraFramingLayout::OverTheShoulder;
  plEnum<plCameraMirrorMode> m_Mirror;
  plTime m_BlendIn = plTime::MakeFromSeconds(0.4);

  virtual plCameraModifierStage::Enum GetStage() const override { return plCameraModifierStage::Framing; }
  virtual void Prepare(plCameraModifierContext& ref_context) override;
  virtual void Apply(plCameraModifierContext& ref_context, plCameraPose& ref_pose, float fWeight) override {}
  virtual void Reset() override;
  virtual void GetDebugText(plStringBuilder& out_sText) const override;

  plGameObjectHandle GetTarget() const { return m_hTarget; }

private:
  plGameObjectHandle PickTarget(const plCameraModifierContext& context, plGameObjectHandle hAfter) const;

  plGameObjectHandle m_hTarget;
  bool m_bWanted = false;
  bool m_bWasDown = false;
  bool m_bSwitchWasDown = false;
  float m_fWeight = 0.0f;
  float m_fMirror = 1.0f;
  plCameraPose m_LastPose;
};
