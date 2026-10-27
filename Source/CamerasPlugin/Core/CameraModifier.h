#pragma once

#include <CamerasPlugin/Core/CameraInput.h>
#include <CamerasPlugin/Core/CameraPose.h>
#include <Foundation/Reflection/Reflection.h>
#include <Foundation/Types/ArrayPtr.h>

class plCameraDirectorComponent;
class plCameraViewComponent;
class plInputComponent;
class plStreamReader;
class plStreamWriter;
class plWorld;
struct plMsgCameraImpulse;

/// \brief A second subject that Look At or Lock-On asks the Composition modifier to frame together with the player.
struct plCameraFramingRequest
{
  bool m_bActive = false;
  plVec3 m_vTarget = plVec3::MakeZero();
  plVec2 m_vPlayerAnchor = plVec2::MakeZero();
  plVec2 m_vTargetAnchor = plVec2::MakeZero();

  /// 0-1, ramps the framing in and out so neither end pops.
  float m_fWeight = 0.0f;
  plString m_sName;
};

/// \brief Everything a modifier can read about the frame, plus the few values modifiers hand to each other.
struct PL_CAMERAS_DLL plCameraModifierContext
{
  plWorld* m_pWorld = nullptr;
  float m_fDeltaSeconds = 0.0f;

  /// Null when a view runs on its own, without a director.
  plCameraDirectorComponent* m_pDirector = nullptr;
  plCameraViewComponent* m_pView = nullptr;
  plBitflags<plCameraViewKind> m_ViewKind;

  const plInputComponent* m_pInput = nullptr;
  plCameraSubjectInfo m_Subject;
  plCameraPlayerSettings m_PlayerSettings;

  float m_fAspectRatio = 16.0f / 9.0f;
  bool m_bFovIsHorizontal = false;
  bool m_bStereo = false;

  /// Whether the player turned the camera this frame. Framing yields on those axes.
  bool m_bPlayerSteering = false;

  // ---- exchanged between modifiers ----

  /// Filled by Look At and Lock-On in Prepare, consumed by Composition in Apply.
  plCameraFramingRequest m_Framing;

  /// Set by Composition in Prepare, so Look At knows whether to aim by itself.
  bool m_bHasComposition = false;

  /// Set by Look At while it frames something, read by Focus.
  bool m_bHasFocusPoint = false;
  plVec3 m_vFocusPoint = plVec3::MakeZero();
  plString m_sFocusName;

  /// Multiplies the views' look input on the next frame. Zoom lowers it.
  float m_fLookScale = 1.0f;

  /// Set when a modifier wants the view to adopt the camera's final direction this frame (Look At with Keep Direction).
  bool m_bCommitLook = false;

  /// With m_bCommitLook: adopt this direction instead of the camera's (Look At with Return, Look Hint).
  bool m_bCommitExplicit = false;
  plAngle m_CommitYaw;
  plAngle m_CommitPitch;

  /// \brief The projection for a pose, using this frame's aspect ratio and the camera's field-of-view axis.
  plCameraProjection GetProjection(const plCameraPose& pose) const;
};

/// \brief One entry of a modifier stack: something that adjusts the camera pose after the view computed it.
///
/// Modifiers live inline in the Modifiers list of a camera director (applied to every view) or of a single view.
/// Within a stage they run in list order, view-local ones first.
class PL_CAMERAS_DLL plCameraModifier : public plReflectedClass
{
  PL_ADD_DYNAMIC_REFLECTION(plCameraModifier, plReflectedClass);

public:
  plCameraModifier();
  virtual ~plCameraModifier();

  /// Lets scripts address the modifier (SetModifierWeight). Optional.
  plString m_sName;
  bool m_bEnabled = true;
  float m_fWeight = 1.0f;

  /// The kinds of view this modifier applies to. Reflected by each modifier with its own default.
  plBitflags<plCameraViewKind> m_Views;

  /// Rotational effects make people sick in a headset, so they are off in stereo unless this is set.
  bool m_bAllowInXR = false;

  virtual plCameraModifierStage::Enum GetStage() const = 0;

  /// \brief Called on every modifier of the stack before any Apply, to exchange information (see plCameraModifierContext).
  virtual void Prepare(plCameraModifierContext& ref_context) {}

  /// \brief Adjusts the pose. \a fWeight already folds in m_fWeight and the view scope.
  virtual void Apply(plCameraModifierContext& ref_context, plCameraPose& ref_pose, float fWeight) = 0;

  virtual void OnImpulse(const plMsgCameraImpulse& msg) {}

  /// \brief Drops runtime state, e.g. when the camera cuts.
  virtual void Reset() {}

  /// \brief Whether this modifier only rotates or shakes the camera, and should therefore sit out in XR.
  virtual bool IsRotationalEffect() const { return false; }

  /// \brief One line for the Cameras.Debug overlay.
  virtual void GetDebugText(plStringBuilder& out_sText) const;

  /// \brief The weight this frame: zero when disabled, out of scope, or a rotational effect in XR.
  float GetEffectiveWeight(const plCameraModifierContext& context) const;

  /// How much the modifier did last frame (0-1), for the debug overlay.
  float m_fDebugAmount = 0.0f;
};

/// \brief Declares the accessors through which each modifier type reflects the base's Views and AllowInXR.
///
/// Reflection can only bind members of the reflected type itself, and each modifier type wants its own default for
/// Views (Head Bob only makes sense in first person), so every modifier reflects these through its own accessors.
#define PL_CAMERA_MODIFIER_COMMON_ACCESSORS()                                                    \
public:                                                                                          \
  plBitflags<plCameraViewKind> GetViewsProperty() const { return m_Views; }                      \
  void SetViewsProperty(plBitflags<plCameraViewKind> views) { m_Views = views; }                 \
  bool GetAllowInXRProperty() const { return m_bAllowInXR; }                                     \
  void SetAllowInXRProperty(bool bAllow) { m_bAllowInXR = bAllow; }

/// \brief Helpers for the modifier lists held by directors and views.
struct PL_CAMERAS_DLL plCameraModifierStack
{
  static void Prepare(plArrayPtr<plCameraModifier* const> modifiers, plCameraModifierContext& ref_context);
  static void Apply(plArrayPtr<plCameraModifier* const> modifiers, plCameraModifierStage::Enum stage, plCameraModifierContext& ref_context, plCameraPose& ref_pose);
  static void SendImpulse(plArrayPtr<plCameraModifier* const> modifiers, const plMsgCameraImpulse& msg);
  static void Reset(plArrayPtr<plCameraModifier* const> modifiers);
  static plCameraModifier* FindByName(plArrayPtr<plCameraModifier* const> modifiers, plStringView sName);

  static void Write(plStreamWriter& inout_stream, plArrayPtr<plCameraModifier* const> modifiers);
  static void Read(plStreamReader& inout_stream, plDynamicArray<plCameraModifier*>& ref_modifiers);
  static void DeleteAll(plDynamicArray<plCameraModifier*>& ref_modifiers);
};
