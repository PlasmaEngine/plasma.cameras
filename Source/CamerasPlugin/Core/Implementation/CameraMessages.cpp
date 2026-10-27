#include <CamerasPlugin/CamerasPCH.h>

#include <CamerasPlugin/Core/CameraMessages.h>

// clang-format off
PL_IMPLEMENT_MESSAGE_TYPE(plMsgCameraImpulse);
PL_BEGIN_DYNAMIC_REFLECTED_TYPE(plMsgCameraImpulse, 1, plRTTIDefaultAllocator<plMsgCameraImpulse>)
{
  PL_BEGIN_PROPERTIES
  {
    PL_ENUM_MEMBER_PROPERTY("Type", plCameraImpulseType, m_Type),
    PL_MEMBER_PROPERTY("Strength", m_fStrength),
    PL_MEMBER_PROPERTY("Position", m_vPosition),
    PL_MEMBER_PROPERTY("Radius", m_fRadius),
    PL_MEMBER_PROPERTY("Pitch", m_Pitch),
    PL_MEMBER_PROPERTY("Yaw", m_Yaw),
  }
  PL_END_PROPERTIES;
}
PL_END_DYNAMIC_REFLECTED_TYPE;

PL_IMPLEMENT_MESSAGE_TYPE(plMsgCameraRequestView);
PL_BEGIN_DYNAMIC_REFLECTED_TYPE(plMsgCameraRequestView, 1, plRTTIDefaultAllocator<plMsgCameraRequestView>)
{
  PL_BEGIN_PROPERTIES
  {
    PL_MEMBER_PROPERTY("Priority", m_iPriority),
    PL_MEMBER_PROPERTY("Duration", m_Duration),
    PL_MEMBER_PROPERTY("BlendTime", m_BlendTime),
    PL_ENUM_MEMBER_PROPERTY("BlendCurve", plCameraBlendCurve, m_BlendCurve),
    PL_ENUM_MEMBER_PROPERTY("Source", plCameraRequestSource, m_Source),
  }
  PL_END_PROPERTIES;
}
PL_END_DYNAMIC_REFLECTED_TYPE;

PL_IMPLEMENT_MESSAGE_TYPE(plMsgCameraPointOfInterestChanged);
PL_BEGIN_DYNAMIC_REFLECTED_TYPE(plMsgCameraPointOfInterestChanged, 1, plRTTIDefaultAllocator<plMsgCameraPointOfInterestChanged>)
{
  PL_BEGIN_PROPERTIES
  {
    PL_MEMBER_PROPERTY("DisplayName", m_sDisplayName),
  }
  PL_END_PROPERTIES;
}
PL_END_DYNAMIC_REFLECTED_TYPE;

PL_IMPLEMENT_MESSAGE_TYPE(plMsgCameraReleaseView);
PL_BEGIN_DYNAMIC_REFLECTED_TYPE(plMsgCameraReleaseView, 1, plRTTIDefaultAllocator<plMsgCameraReleaseView>)
{
  PL_BEGIN_PROPERTIES
  {
    PL_MEMBER_PROPERTY("RequestId", m_uiRequestId),
    PL_MEMBER_PROPERTY("BlendTime", m_BlendTime),
  }
  PL_END_PROPERTIES;
}
PL_END_DYNAMIC_REFLECTED_TYPE;
// clang-format on
