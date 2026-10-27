#include <CamerasPlugin/CamerasPCH.h>

#include <CamerasPlugin/Core/CameraViewComponent.h>
#include <CamerasPlugin/Core/CameraWorldModule.h>
#include <CamerasPlugin/Volumes/CameraVolumeComponent.h>
#include <Core/Messages/UpdateLocalBoundsMessage.h>
#include <Core/WorldSerializer/WorldReader.h>
#include <Core/WorldSerializer/WorldWriter.h>

// clang-format off
PL_BEGIN_COMPONENT_TYPE(plCameraVolumeComponent, 1, plComponentMode::Static)
{
  PL_BEGIN_PROPERTIES
  {
    PL_ACCESSOR_PROPERTY("View", DummyGetter, SetViewReference)->AddAttributes(new plGameObjectReferenceAttribute()),
    PL_ENUM_MEMBER_PROPERTY("Shape", plCameraVolumeShape, m_Shape),
    PL_MEMBER_PROPERTY("HalfExtents", m_vHalfExtents)->AddAttributes(new plDefaultValueAttribute(plVec3(2.0f)), new plClampValueAttribute(plVec3(0.01f), plVariant())),
    PL_MEMBER_PROPERTY("Radius", m_fRadius)->AddAttributes(new plDefaultValueAttribute(2.0f), new plClampValueAttribute(0.01f, plVariant())),
    PL_MEMBER_PROPERTY("Priority", m_iPriority)->AddAttributes(new plDefaultValueAttribute(20)),
    PL_MEMBER_PROPERTY("BlendIn", m_BlendIn)->AddAttributes(new plDefaultValueAttribute(plTime::MakeFromSeconds(0.6)), new plClampValueAttribute(plTime::MakeZero(), plVariant())),
    PL_MEMBER_PROPERTY("BlendOut", m_BlendOut)->AddAttributes(new plDefaultValueAttribute(plTime::MakeFromSeconds(0.6)), new plClampValueAttribute(plTime::MakeZero(), plVariant())),
    PL_ENUM_MEMBER_PROPERTY("BlendCurve", plCameraBlendCurve, m_BlendCurve),
  }
  PL_END_PROPERTIES;
  PL_BEGIN_MESSAGEHANDLERS
  {
    PL_MESSAGE_HANDLER(plMsgUpdateLocalBounds, OnUpdateLocalBounds),
  }
  PL_END_MESSAGEHANDLERS;
  PL_BEGIN_ATTRIBUTES
  {
    new plCategoryAttribute("Cameras"),
    new plBoxVisualizerAttribute("HalfExtents", 2.0f, plColor::Gold),
    new plSphereVisualizerAttribute("Radius", plColor::Gold),
    new plBoxManipulatorAttribute("HalfExtents", 2.0f, true),
    new plSphereManipulatorAttribute("Radius"),
  }
  PL_END_ATTRIBUTES;
}
PL_END_COMPONENT_TYPE;
// clang-format on

plCameraVolumeComponent::plCameraVolumeComponent() = default;
plCameraVolumeComponent::~plCameraVolumeComponent() = default;

void plCameraVolumeComponent::SerializeComponent(plWorldWriter& inout_stream) const
{
  SUPER::SerializeComponent(inout_stream);
  auto& s = inout_stream.GetStream();

  inout_stream.WriteGameObjectHandle(m_hViewObject);
  s << m_Shape;
  s << m_vHalfExtents;
  s << m_fRadius;
  s << m_iPriority;
  s << m_BlendIn;
  s << m_BlendOut;
  s << m_BlendCurve;
}

void plCameraVolumeComponent::DeserializeComponent(plWorldReader& inout_stream)
{
  SUPER::DeserializeComponent(inout_stream);
  auto& s = inout_stream.GetStream();

  m_hViewObject = inout_stream.ReadGameObjectHandle();
  s >> m_Shape;
  s >> m_vHalfExtents;
  s >> m_fRadius;
  s >> m_iPriority;
  s >> m_BlendIn;
  s >> m_BlendOut;
  s >> m_BlendCurve;
}

void plCameraVolumeComponent::OnActivated()
{
  SUPER::OnActivated();
  GetWorld()->GetOrCreateModule<plCameraWorldModule>()->RegisterVolume(this);
  GetOwner()->UpdateLocalBounds();
}

void plCameraVolumeComponent::OnDeactivated()
{
  if (plCameraWorldModule* pModule = GetWorld()->GetModule<plCameraWorldModule>())
    pModule->UnregisterVolume(this);

  SUPER::OnDeactivated();
}

void plCameraVolumeComponent::SetViewReference(const char* szReference)
{
  m_hViewObject = plCameraReference::Resolve(this, szReference, "View");
}

plCameraViewComponent* plCameraVolumeComponent::GetView() const
{
  plGameObject* pObject = nullptr;
  plCameraViewComponent* pView = nullptr;

  if (const_cast<plWorld*>(GetWorld())->TryGetObject(m_hViewObject, pObject))
    pObject->TryGetComponentOfBaseType(pView);

  return pView;
}

bool plCameraVolumeComponent::Contains(const plVec3& vGlobalPosition) const
{
  const plTransform t = GetOwner()->GetGlobalTransform();
  const plVec3 vLocal = t.GetInverse().TransformPosition(vGlobalPosition);

  if (m_Shape == plCameraVolumeShape::Sphere)
    return vLocal.GetLengthSquared() <= m_fRadius * m_fRadius;

  return plMath::Abs(vLocal.x) <= m_vHalfExtents.x && plMath::Abs(vLocal.y) <= m_vHalfExtents.y && plMath::Abs(vLocal.z) <= m_vHalfExtents.z;
}

void plCameraVolumeComponent::OnUpdateLocalBounds(plMsgUpdateLocalBounds& msg) const
{
  if (m_Shape == plCameraVolumeShape::Sphere)
    msg.AddBounds(plBoundingSphere::MakeFromCenterAndRadius(plVec3::MakeZero(), m_fRadius), plInvalidSpatialDataCategory);
  else
    msg.AddBounds(plBoundingBox::MakeFromMinMax(-m_vHalfExtents, m_vHalfExtents), plInvalidSpatialDataCategory);
}
