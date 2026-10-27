#pragma once

#include <CamerasPlugin/Core/CameraViewComponent.h>

using plTopDownViewComponentManager = plComponentManager<class plTopDownViewComponent, plBlockStorageType::Compact>;

/// \brief A camera high above a target at a fixed pitch: top-down, isometric and strategy games.
///
/// The camera looks at the target (plus any pan) from Zoom Distances[current zoom] away, at Pitch below the
/// horizon and Yaw around it. Zoom, rotate-step and pan come from input actions; Edge Scroll pans when the mouse is
/// near the screen edge. Ground Follow raises the focus point to the ground under the target.
class PL_CAMERAS_DLL plTopDownViewComponent : public plCameraViewComponent
{
  PL_DECLARE_COMPONENT_TYPE(plTopDownViewComponent, plCameraViewComponent, plTopDownViewComponentManager);

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
  virtual plBitflags<plCameraViewKind> GetViewKind() const override { return plCameraViewKind::TopDown; }
  virtual void UpdateView(plCameraViewContext& ref_context, const plCameraPose& previous, plCameraPose& out_pose) override;
  virtual plGameObjectHandle GetSubjectObject() const override { return m_hTarget; }
  virtual bool GetSubjectPoint(plVec3& out_vPosition) const override;

  //////////////////////////////////////////////////////////////////////////
  // plTopDownViewComponent

public:
  plTopDownViewComponent();
  ~plTopDownViewComponent();

  void SetTargetReference(const char* szReference); // [ property ]

  plAngle m_Pitch = plAngle::MakeFromDegree(55.0f);       // [ property ]
  plAngle m_Yaw = plAngle::MakeFromDegree(45.0f);         // [ property ]
  plAngle m_RotateStep = plAngle::MakeFromDegree(90.0f);  // [ property ]
  plString m_sRotateLeftAction;                           // [ property ]
  plString m_sRotateRightAction;                          // [ property ]

  plDynamicArray<float> m_ZoomDistances; // [ property ]
  plUInt32 m_uiStartZoom = 1;            // [ property ]
  plString m_sZoomInAction;              // [ property ]
  plString m_sZoomOutAction;             // [ property ]

  plString m_sPanAction;        // [ property ]
  float m_fPanSpeed = 12.0f;    // [ property ]
  float m_fMaxPan = 30.0f;      // [ property ]
  bool m_bEdgeScroll = false;   // [ property ]

  plTime m_FollowLag = plTime::MakeFromSeconds(0.2); // [ property ]
  bool m_bGroundFollow = false;                      // [ property ]

  void ZoomIn();                 // [ scriptable ]
  void ZoomOut();                // [ scriptable ]
  void RotateBySteps(float fSteps); // [ scriptable ]
  void ResetPan();               // [ scriptable ]

private:
  const char* DummyGetter() const { return nullptr; }
  void ReadInput(plCameraViewContext& ref_context);
  bool Pressed(const plInputComponent* pInput, const plString& sAction, bool& ref_bWasDown) const;
  float GroundHeightAt(const plVec3& vPosition) const;

  plGameObjectHandle m_hTarget;

  plUInt32 m_uiZoom = 1;
  float m_fCurDistance = 0.0f;
  plAngle m_TargetYaw;
  plAngle m_CurYaw;
  plVec3 m_vPan = plVec3::MakeZero();
  plVec3 m_vSmoothedFocus = plVec3::MakeZero();
  bool m_bHasFocus = false;

  bool m_bZoomInDown = false;
  bool m_bZoomOutDown = false;
  bool m_bRotLeftDown = false;
  bool m_bRotRightDown = false;
};
