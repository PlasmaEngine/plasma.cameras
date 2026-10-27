#pragma once

#include <CamerasPlugin/Core/CameraViewComponent.h>

using plFixedViewComponentManager = plComponentManager<class plFixedViewComponent, plBlockStorageType::Compact>;

/// \brief A camera that stays where it is placed: a security camera, a fixed-angle shot.
///
/// With a Target it turns to keep looking at it (smoothed by Tracking Smoothing). With Frame Target it also narrows
/// the field of view so a Frame Size meter tall area around the target fills the screen height, never wider than
/// Field Of View.
class PL_CAMERAS_DLL plFixedViewComponent : public plCameraViewComponent
{
  PL_DECLARE_COMPONENT_TYPE(plFixedViewComponent, plCameraViewComponent, plFixedViewComponentManager);

  //////////////////////////////////////////////////////////////////////////
  // plComponent

public:
  virtual void SerializeComponent(plWorldWriter& inout_stream) const override;
  virtual void DeserializeComponent(plWorldReader& inout_stream) override;

  //////////////////////////////////////////////////////////////////////////
  // plCameraViewComponent

public:
  virtual plBitflags<plCameraViewKind> GetViewKind() const override { return plCameraViewKind::Fixed; }
  virtual void UpdateView(plCameraViewContext& ref_context, const plCameraPose& previous, plCameraPose& out_pose) override;
  virtual void OnShownByDirector(const plCameraPose& current) override;
  virtual plGameObjectHandle GetSubjectObject() const override { return m_hTarget; }
  virtual bool GetSubjectPoint(plVec3& out_vPosition) const override;

  //////////////////////////////////////////////////////////////////////////
  // plFixedViewComponent

public:
  plFixedViewComponent();
  ~plFixedViewComponent();

  void SetTargetReference(const char* szReference); // [ property ]

  plVec3 m_vTargetOffset = plVec3(0, 0, 1.0f);                  // [ property ]
  plTime m_TrackingSmoothing = plTime::MakeFromSeconds(0.15);   // [ property ]
  bool m_bFrameTarget = false;                                  // [ property ]
  float m_fFrameSize = 3.0f;                                    // [ property ]

private:
  const char* DummyGetter() const { return nullptr; }

  plGameObjectHandle m_hTarget;
  plQuat m_qSmoothed = plQuat::MakeIdentity();
  bool m_bHasSmoothed = false;
};
