#pragma once

#include <CamerasPlugin/Core/CameraTypes.h>
#include <Core/Messages/EventMessage.h>
#include <Core/World/Declarations.h>
#include <Foundation/Time/Time.h>

/// \brief A one-shot camera event: an explosion, a weapon kick, a hard landing.
///
/// Send it to a camera director's object, or use plCameraWorldModule::PlayImpulse to reach every director. Each
/// director passes it to its modifiers; a Shake modifier reacts to Shake impulses, a Recoil modifier to Recoil
/// impulses, and so on.
struct PL_CAMERAS_DLL plMsgCameraImpulse : public plMessage
{
  PL_DECLARE_MESSAGE_TYPE(plMsgCameraImpulse, plMessage);

  plEnum<plCameraImpulseType> m_Type;

  /// For Shake: trauma to add (0-1) at the impulse position. For Landing: the fall speed in m/s.
  float m_fStrength = 0.5f;

  /// For Shake: where it happened. Ignored when m_fRadius is zero.
  plVec3 m_vPosition = plVec3::MakeZero();

  /// For Shake: the strength falls off to zero at this distance from m_vPosition. Zero means no falloff.
  float m_fRadius = 0.0f;

  /// For Recoil: the kick.
  plAngle m_Pitch;
  plAngle m_Yaw;
};

/// \brief Asks a camera director to show a view.
///
/// The highest priority request wins; between equal priorities the newest wins. Volumes, scripts and sequences
/// all go through this.
struct PL_CAMERAS_DLL plMsgCameraRequestView : public plMessage
{
  PL_DECLARE_MESSAGE_TYPE(plMsgCameraRequestView, plMessage);

  /// The object with the plCameraViewComponent to show.
  plGameObjectHandle m_hViewObject;
  plInt32 m_iPriority = 50;

  /// How long the request lasts. Zero lasts until it is released.
  plTime m_Duration;

  /// Negative uses the view's or the director's default.
  plTime m_BlendTime = plTime::MakeFromSeconds(-1.0);
  plEnum<plCameraBlendCurve> m_BlendCurve;
  plEnum<plCameraRequestSource> m_Source = plCameraRequestSource::Script;

  /// Set by the director: the id to release the request with.
  plUInt32 m_uiRequestId = 0;
};

/// \brief Sent up the subject's hierarchy (the player) whenever the point of interest a Look At would frame changes.
///
/// Game UI listens for it to show or hide its "F Look at ..." prompt. An invalid handle means nothing is in range.
struct PL_CAMERAS_DLL plMsgCameraPointOfInterestChanged : public plEventMessage
{
  PL_DECLARE_MESSAGE_TYPE(plMsgCameraPointOfInterestChanged, plEventMessage);

  plComponentHandle m_hPointOfInterest;
  plString m_sDisplayName;
};

/// \brief Drops a view request again.
///
/// Releases the request with m_uiRequestId if it is set, otherwise every request for m_hViewObject.
struct PL_CAMERAS_DLL plMsgCameraReleaseView : public plMessage
{
  PL_DECLARE_MESSAGE_TYPE(plMsgCameraReleaseView, plMessage);

  plGameObjectHandle m_hViewObject;
  plUInt32 m_uiRequestId = 0;

  /// Negative uses the director's default blend back.
  plTime m_BlendTime = plTime::MakeFromSeconds(-1.0);
};
