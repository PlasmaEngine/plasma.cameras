#pragma once

#include <CamerasPlugin/Core/CameraViewComponent.h>

using plThirdPersonViewComponentManager = plComponentManager<class plThirdPersonViewComponent, plBlockStorageType::Compact>;

/// \brief A camera on an arm behind a target, pulled in when the arm would pass through geometry.
///
/// The arm hangs from a point between Target Offset Low and High (picked by how far the camera is pitched), pitches
/// within Min/Max Up Rotation and is kept below Ceiling Height and above Floor Height. A sphere sweep along the arm
/// pulls the camera in at Zoom In Speed and lets it back out at Zoom Out Speed.
///
/// With the default settings it behaves exactly as it did before the Gameplay Cameras package: it follows the
/// target's yaw and only pitches through RotateUp. Look input, shoulder offset, lag, recentering, dynamic-shape
/// sweeps and look-ahead are opt-in.
class PL_CAMERAS_DLL plThirdPersonViewComponent : public plCameraViewComponent
{
  PL_DECLARE_COMPONENT_TYPE(plThirdPersonViewComponent, plCameraViewComponent, plThirdPersonViewComponentManager);

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
  virtual plBitflags<plCameraViewKind> GetViewKind() const override { return plCameraViewKind::ThirdPerson; }
  virtual void UpdateView(plCameraViewContext& ref_context, const plCameraPose& previous, plCameraPose& out_pose) override;
  virtual void OnShownByDirector(const plCameraPose& current) override;
  virtual plGameObjectHandle GetSubjectObject() const override { return m_hTargetObject; }
  virtual bool GetSubjectPoint(plVec3& out_vPosition) const override;
  virtual void SetLookRotation(plAngle yaw, plAngle pitch) override;
  virtual void DrawRig() const override;

protected:
  virtual plUInt32 GetFirstVersionWithViewData() const override { return 2; }

  //////////////////////////////////////////////////////////////////////////
  // plThirdPersonViewComponent

public:
  plThirdPersonViewComponent();
  ~plThirdPersonViewComponent();

  void SetTargetObject(const char* szTargetObject); // [ property ]
  const char* GetTargetObject() const;              // [ property ]

  void RotateUp(plAngle angle); // [ scriptable ]

  /// \brief Turns the camera around the target. Positive turns left.
  void Orbit(plAngle angle); // [ scriptable ]

  /// \brief Moves the camera to the other shoulder.
  void SwapShoulder(); // [ scriptable ]

  plVec3 m_vTargetOffsetHigh = plVec3::MakeZero();       // [ property ]
  plVec3 m_vTargetOffsetLow = plVec3::MakeZero();        // [ property ]
  float m_fMinDistance = 0.25f;                           // [ property ]
  float m_fMaxDistance = 3.0f;                            // [ property ]
  plAngle m_MinUpRotation = plAngle::MakeFromDegree(-70); // [ property ]
  plAngle m_MaxUpRotation = plAngle::MakeFromDegree(+80); // [ property ]
  float m_fCeilingHeight = 4.0f;                          // [ property ]
  float m_fFloorHeight = 0.0f;                            // [ property ]
  plUInt8 m_uiCollisionLayer = 0;                         // [ property ]
  float m_fSweepWidth = 0.2f;                             // [ property ]
  float m_fZoomInSpeed = 10.0f;                           // [ property ]
  float m_fZoomOutSpeed = 1.0f;                           // [ property ]

  // ---- version 2 ----

  plCameraLookInput m_LookInput;                          // [ property ]
  plEnum<plCameraYawMode> m_YawMode;                      // [ property ]

  /// Sideways offset of camera and aim point, positive to the right. Swap Shoulder flips the side.
  float m_fShoulderOffset = 0.0f; // [ property ]
  plString m_sShoulderSwapAction; // [ property ]

  /// How far behind the target the aim point trails. Zero follows exactly.
  plTime m_PositionLag; // [ property ]

  /// Swings the camera back behind the target after the player stops steering it (Follow Target mode).
  bool m_bAutoRecenter = false;                                   // [ property ]
  plTime m_RecenterDelay = plTime::MakeFromSeconds(2.0);          // [ property ]
  plAngle m_RecenterSpeed = plAngle::MakeFromDegree(90.0f);       // [ property ]

  /// Whether the arm is also pulled in by dynamic bodies. Characters never block it.
  bool m_bHitDynamicShapes = false; // [ property ]

  /// How far ahead of a moving target the camera aims, in meters at full running speed.
  float m_fLookAhead = 0.0f; // [ property ]

private:
  struct Rig
  {
    plVec3 m_vPivot;
    plVec3 m_vLookAt;
    plVec3 m_vArmDir;
    float m_fTargetDistance = 0.0f;
    bool m_bBlocked = false;
  };

  bool ComputeRig(const plTransform& target, Rig& out_rig) const;
  float SweepArm(const plVec3& vFrom, const plVec3& vDir, float fDistance, bool& out_bBlocked) const;
  plQuat GetArmRotation(const plTransform& target) const;
  void ApplyInput(plCameraViewContext& ref_context);
  void UpdateRecenter(plCameraViewContext& ref_context, bool bSteered);

  // properties
  plString m_sTargetObject;

  // runtime state
  plGameObjectHandle m_hTargetObject;
  plAngle m_RotateUp;
  plAngle m_CurUpRotation;
  plAngle m_YawOffset;   ///< relative to the target in Follow Target mode
  plAngle m_FreeYaw;     ///< absolute in Free mode
  float m_fCurDistance = 1.0f;
  float m_fShoulderSide = 1.0f;
  float m_fCurShoulder = 0.0f;
  bool m_bSwapWasDown = false;
  plVec3 m_vLaggedPivot = plVec3::MakeZero();
  bool m_bHasLaggedPivot = false;
  float m_fSinceSteered = 0.0f;
  Rig m_LastRig;
};
