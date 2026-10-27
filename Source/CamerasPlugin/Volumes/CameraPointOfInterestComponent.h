#pragma once

#include <CamerasPlugin/CamerasDLL.h>
#include <Core/World/Component.h>
#include <Core/World/World.h>

struct plMsgExtractRenderData;

using plCameraPointOfInterestComponentManager = plComponentManager<class plCameraPointOfInterestComponent, plBlockStorageType::Compact>;

/// \brief Marks something worth looking at: the Look At modifier frames it when the player presses its key.
///
/// It is eligible while the player is within Activation Radius, inside the View Cone in front of it, and (if
/// required) has line of sight to it. Among eligible points the highest priority, then the nearest, is picked. The
/// game shows its own prompt ("F Look at Shrine of Ash") through plCameraLookAtModifier::GetBestPointOfInterest().
class PL_CAMERAS_DLL plCameraPointOfInterestComponent : public plComponent
{
  PL_DECLARE_COMPONENT_TYPE(plCameraPointOfInterestComponent, plComponent, plCameraPointOfInterestComponentManager);

  //////////////////////////////////////////////////////////////////////////
  // plComponent

public:
  virtual void SerializeComponent(plWorldWriter& inout_stream) const override;
  virtual void DeserializeComponent(plWorldReader& inout_stream) override;

protected:
  virtual void OnActivated() override;
  virtual void OnDeactivated() override;
  virtual void OnSimulationStarted() override;

  //////////////////////////////////////////////////////////////////////////
  // plCameraPointOfInterestComponent

public:
  plCameraPointOfInterestComponent();
  ~plCameraPointOfInterestComponent();

  /// Shown in the game's prompt.
  plString m_sDisplayName;                       // [ property ]
  float m_fActivationRadius = 15.0f;             // [ property ]

  /// Full angle around the object's +X in which the player must stand. 360 means any side.
  plAngle m_ViewCone = plAngle::MakeFromDegree(360.0f); // [ property ]
  bool m_bRequireLineOfSight = true;             // [ property ]
  plInt32 m_iPriority = 0;                       // [ property ]

  /// Local offset of the point the camera frames, e.g. the top of a statue.
  plVec3 m_vFrameOffset = plVec3::MakeZero();    // [ property ]

  /// Field of view for Aim framing. Zero uses the modifier's.
  float m_fZoomFieldOfView = 0.0f;               // [ property ]

  /// How long to hold. Zero uses the modifier's.
  plTime m_HoldTime;                             // [ property ]

  /// Frames itself once, without a key press, the first time the player comes into range. For scripted beats.
  bool m_bAutoTrigger = false;                   // [ property ]

  plVec3 GetFramePoint() const;

  /// \brief Range and cone test (no line of sight) for a player standing at \a vPlayer.
  bool IsInRange(const plVec3& vPlayer) const;

  bool HasAutoTriggered() const { return m_bAutoTriggered; }
  void MarkAutoTriggered() { m_bAutoTriggered = true; }

private:
  void OnMsgExtractRenderData(plMsgExtractRenderData& msg) const;

  bool m_bAutoTriggered = false;
  mutable plUInt16 m_uiDrawnFrame = 0xFFFF;
};
