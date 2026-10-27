#pragma once

#include <CamerasPlugin/Core/CameraViewComponent.h>

using plRailViewComponentManager = plComponentManager<class plRailViewComponent, plBlockStorageType::Compact>;

/// \brief A camera that rides along a spline (a dolly track) and looks at a target.
///
/// In Closest To Target mode the camera sits at the point of the spline nearest the target, shifted by Key Offset.
/// In Scripted mode a script moves it with SetRailKey. Movement along the spline is smoothed by Rail Smoothing.
/// Edit the spline with the Spline Edit Mode.
class PL_CAMERAS_DLL plRailViewComponent : public plCameraViewComponent
{
  PL_DECLARE_COMPONENT_TYPE(plRailViewComponent, plCameraViewComponent, plRailViewComponentManager);

  //////////////////////////////////////////////////////////////////////////
  // plComponent

public:
  virtual void SerializeComponent(plWorldWriter& inout_stream) const override;
  virtual void DeserializeComponent(plWorldReader& inout_stream) override;

  //////////////////////////////////////////////////////////////////////////
  // plCameraViewComponent

public:
  virtual plBitflags<plCameraViewKind> GetViewKind() const override { return plCameraViewKind::Rail; }
  virtual void UpdateView(plCameraViewContext& ref_context, const plCameraPose& previous, plCameraPose& out_pose) override;
  virtual void OnShownByDirector(const plCameraPose& current) override;
  virtual plGameObjectHandle GetSubjectObject() const override { return m_hTarget; }
  virtual bool GetSubjectPoint(plVec3& out_vPosition) const override;
  virtual void DrawRig() const override;

  //////////////////////////////////////////////////////////////////////////
  // plRailViewComponent

public:
  plRailViewComponent();
  ~plRailViewComponent();

  /// The object with the plSplineComponent. Empty uses this object.
  void SetSplineReference(const char* szReference); // [ property ]
  void SetTargetReference(const char* szReference); // [ property ]

  plVec3 m_vTargetOffset = plVec3(0, 0, 1.0f);              // [ property ]
  plEnum<plCameraRailMode> m_Mode;                          // [ property ]
  float m_fKeyOffset = 0.0f;                                // [ property ]
  plTime m_RailSmoothing = plTime::MakeFromSeconds(0.3);    // [ property ]

  /// \brief Moves the camera to a spline key (Scripted mode).
  void SetRailKey(float fKey); // [ scriptable ]

private:
  const char* DummyGetter() const { return nullptr; }
  const class plSplineComponent* GetSpline() const;

  plGameObjectHandle m_hSpline;
  plGameObjectHandle m_hTarget;
  float m_fScriptKey = 0.0f;
  float m_fSmoothedKey = 0.0f;
  bool m_bHasKey = false;
};
