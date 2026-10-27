#pragma once

#include <CamerasPlugin/Core/CameraModifier.h>
#include <Core/World/Component.h>
#include <Core/World/World.h>

class plCameraDirectorComponent;
struct plMsgExtractRenderData;

/// \brief What a view gets to work with each frame.
struct PL_CAMERAS_DLL plCameraViewContext
{
  plWorld* m_pWorld = nullptr;
  float m_fDeltaSeconds = 0.0f;

  /// Null when the view runs on its own.
  plCameraDirectorComponent* m_pDirector = nullptr;

  plCameraPlayerSettings m_PlayerSettings;

  /// Multiplies look input; zoom modifiers lower it.
  float m_fLookScale = 1.0f;
  bool m_bStereo = false;

  /// Set by the view when the player turned it this frame.
  bool m_bPlayerSteering = false;
};

/// \brief Base class of every camera view: a component that computes where a camera should be, without moving anything.
///
/// A camera director picks one view at a time (by request priority), blends between them and writes the result to
/// its camera. A view in a world without any director drives its own object instead, which is how a single camera
/// (and every scene made before directors existed) works.
class PL_CAMERAS_DLL plCameraViewComponent : public plComponent
{
  PL_DECLARE_ABSTRACT_COMPONENT_TYPE(plCameraViewComponent, plComponent);

  //////////////////////////////////////////////////////////////////////////
  // plComponent

public:
  virtual void SerializeComponent(plWorldWriter& inout_stream) const override;
  virtual void DeserializeComponent(plWorldReader& inout_stream) override;

protected:
  virtual void OnActivated() override;
  virtual void OnDeactivated() override;

  //////////////////////////////////////////////////////////////////////////
  // plCameraViewComponent

public:
  plCameraViewComponent();
  ~plCameraViewComponent();

  /// Degrees. Zero keeps the camera's own field of view (as it was when play started).
  float m_fFieldOfView = 0.0f; // [ property ]

  /// The priority this view is shown at when a script activates it with priority 0 (ActivateView).
  plInt32 m_iPriority = 50; // [ property ]

  /// Blend used when this view becomes active. Zero uses the director's default.
  plTime m_BlendTime;                       // [ property ]
  plEnum<plCameraBlendCurve> m_BlendCurve; // [ property ]

  /// Modifiers that only apply while this view is active. They run before the director's own.
  plDynamicArray<plCameraModifier*> m_Modifiers; // [ property ]

  virtual plBitflags<plCameraViewKind> GetViewKind() const = 0;

  /// \brief Computes the pose for this frame. \a previous is the camera's pose last frame, for views that track it.
  virtual void UpdateView(plCameraViewContext& ref_context, const plCameraPose& previous, plCameraPose& out_pose) = 0;

  /// \brief Called when a director starts showing this view, with the camera's current pose.
  virtual void OnShownByDirector(const plCameraPose& current) {}

  /// \brief The object the view looks at or follows (the player, usually). May be invalid.
  virtual plGameObjectHandle GetSubjectObject() const { return plGameObjectHandle(); }

  /// \brief Where on the subject the camera aims, in world space. False when there is no subject.
  virtual bool GetSubjectPoint(plVec3& out_vPosition) const;

  /// \brief Adopts a direction for the view's own look state, so a framing modifier can hand over without a snap.
  virtual void SetLookRotation(plAngle yaw, plAngle pitch) {}

  /// \brief Draws the view's rig. Used for Cameras.Debug 2 and when the object is selected in the editor.
  virtual void DrawRig() const;

  /// \brief The view's most recent pose.
  const plCameraPose& GetLastPose() const { return m_LastPose; }

  /// \brief Runs the view without a director: computes the pose, applies its own modifiers and moves its object.
  void UpdateStandalone(float fDeltaSeconds);

  /// \brief Tracks the subject (velocity, ground state). Called by whoever runs the view, once per frame.
  const plCameraSubjectInfo& TrackSubject(float fDeltaSeconds);

  const plCameraSubjectInfo& GetSubjectInfo() const { return m_SubjectTracker.GetInfo(); }

  /// \brief The input component steering this view, if any: on the subject or its parents, else on this object or its parents.
  const plInputComponent* FindInput() const;

  /// \brief Builds the modifier context for this view.
  void FillModifierContext(plCameraModifierContext& out_context, float fDeltaSeconds, plCameraDirectorComponent* pDirector, const plCameraPlayerSettings& settings) const;

protected:
  /// \brief The first version of the derived type whose saved data starts with the view data written by this base.
  ///
  /// Views that existed before the base class (Third Person View, version 1) return the version that introduced it,
  /// so older scenes load without reading data that is not there.
  virtual plUInt32 GetFirstVersionWithViewData() const { return 1; }

  void OnMsgExtractRenderData(plMsgExtractRenderData& msg) const;

  plCameraPose m_LastPose;
  plCameraSubjectTracker m_SubjectTracker;
  mutable plUInt16 m_uiDrawnFrame = 0xFFFF;

  /// The camera's own field of view, read once, for views that keep it (Field Of View zero) without a director.
  float m_fStandaloneCameraFov = 0.0f;
};

/// \brief Helpers for view properties that reference other objects.
struct PL_CAMERAS_DLL plCameraReference
{
  /// \brief Resolves an object reference property the way components do, relative to \a pComponent.
  static plGameObjectHandle Resolve(const plComponent* pComponent, const char* szReference, const char* szProperty);
};
