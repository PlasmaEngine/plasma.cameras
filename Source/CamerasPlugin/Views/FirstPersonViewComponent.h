#pragma once

#include <CamerasPlugin/Core/CameraViewComponent.h>

using plFirstPersonViewComponentManager = plComponentManager<class plFirstPersonViewComponent, plBlockStorageType::Compact>;

/// \brief A camera at a character's eyes.
///
/// The position follows Eye Object (usually the character controller's head object), with height changes such as
/// crouching smoothed over Eye Height Smoothing. Yaw comes from the eye object, so a character controller that turns
/// itself also turns the view; pitch comes from look input or script and is clamped to Max Pitch.
/// In a headset only the position is used: the headset owns the rotation.
class PL_CAMERAS_DLL plFirstPersonViewComponent : public plCameraViewComponent
{
  PL_DECLARE_COMPONENT_TYPE(plFirstPersonViewComponent, plCameraViewComponent, plFirstPersonViewComponentManager);

  //////////////////////////////////////////////////////////////////////////
  // plComponent

public:
  virtual void SerializeComponent(plWorldWriter& inout_stream) const override;
  virtual void DeserializeComponent(plWorldReader& inout_stream) override;

protected:
  virtual void OnSimulationStarted() override;

  //////////////////////////////////////////////////////////////////////////
  // plCameraViewComponent

public:
  virtual plBitflags<plCameraViewKind> GetViewKind() const override { return plCameraViewKind::FirstPerson; }
  virtual void UpdateView(plCameraViewContext& ref_context, const plCameraPose& previous, plCameraPose& out_pose) override;
  virtual plGameObjectHandle GetSubjectObject() const override;
  virtual bool GetSubjectPoint(plVec3& out_vPosition) const override;
  virtual void SetLookRotation(plAngle yaw, plAngle pitch) override;

  //////////////////////////////////////////////////////////////////////////
  // plFirstPersonViewComponent

public:
  plFirstPersonViewComponent();
  ~plFirstPersonViewComponent();

  /// The object at the eyes. Empty uses this object's parent.
  void SetEyeObjectReference(const char* szReference); // [ property ]

  plVec3 m_vEyeOffset = plVec3::MakeZero();                       // [ property ]
  plTime m_EyeHeightSmoothing = plTime::MakeFromSeconds(0.12);    // [ property ]
  plAngle m_MaxPitch = plAngle::MakeFromDegree(85.0f);            // [ property ]

  /// Yaw from the eye object (the character turns the view). Off: look input turns the view by itself.
  bool m_bYawFromEyeObject = true; // [ property ]

  plCameraLookInput m_LookInput; // [ property ]

  /// Near clip distance while this view is active. Zero leaves the camera's own.
  float m_fNearPlane = 0.0f; // [ property ]

  /// \brief Sets the pitch, in radians. Same as Head Bone's.
  void SetVerticalRotation(float fRadians); // [ scriptable ]

  /// \brief Adds to the pitch, in radians. Same as Head Bone's.
  void ChangeVerticalRotation(float fRadians); // [ scriptable ]

private:
  const char* DummyGetter() const { return nullptr; }
  const plGameObject* GetEyeObject() const;

  plGameObjectHandle m_hEyeObject;

  plAngle m_Pitch;
  plAngle m_OwnYaw;
  float m_fSmoothedHeight = 0.0f;
  bool m_bHasHeight = false;
};
