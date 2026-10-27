#pragma once

#include <CamerasPlugin/Core/CameraTypes.h>
#include <Core/World/Component.h>
#include <Core/World/World.h>

class plCameraViewComponent;
struct plMsgExtractRenderData;
struct plMsgUpdateLocalBounds;

using plCameraVolumeComponentManager = plComponentManager<class plCameraVolumeComponent, plBlockStorageType::Compact>;

/// \brief A box or sphere that shows a camera view while the player is inside it.
///
/// Every camera director tests where its player's subject is against the volume each frame. Entering requests the
/// view at the volume's priority with Blend In; leaving releases it with Blend Out.
class PL_CAMERAS_DLL plCameraVolumeComponent : public plComponent
{
  PL_DECLARE_COMPONENT_TYPE(plCameraVolumeComponent, plComponent, plCameraVolumeComponentManager);

  //////////////////////////////////////////////////////////////////////////
  // plComponent

public:
  virtual void SerializeComponent(plWorldWriter& inout_stream) const override;
  virtual void DeserializeComponent(plWorldReader& inout_stream) override;

protected:
  virtual void OnActivated() override;
  virtual void OnDeactivated() override;

  //////////////////////////////////////////////////////////////////////////
  // plCameraVolumeComponent

public:
  plCameraVolumeComponent();
  ~plCameraVolumeComponent();

  plEnum<plCameraVolumeShape> m_Shape;         // [ property ]
  plVec3 m_vHalfExtents = plVec3(2.0f);        // [ property ]
  float m_fRadius = 2.0f;                      // [ property ]
  plInt32 m_iPriority = 20;                    // [ property ]
  plTime m_BlendIn = plTime::MakeFromSeconds(0.6);  // [ property ]
  plTime m_BlendOut = plTime::MakeFromSeconds(0.6); // [ property ]
  plEnum<plCameraBlendCurve> m_BlendCurve;     // [ property ]

  /// The object with the view to show.
  void SetViewReference(const char* szReference); // [ property ]

  plCameraViewComponent* GetView() const;

  /// \brief Whether a world-space point is inside the volume.
  bool Contains(const plVec3& vGlobalPosition) const;

private:
  const char* DummyGetter() const { return nullptr; }
  void OnUpdateLocalBounds(plMsgUpdateLocalBounds& msg) const;

  plGameObjectHandle m_hViewObject;
};
