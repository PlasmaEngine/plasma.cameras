#include <CamerasPlugin/CamerasPCH.h>

#include <CamerasPlugin/Core/CameraTypes.h>

// clang-format off
PL_BEGIN_STATIC_REFLECTED_ENUM(plCameraBlendCurve, 1)
  PL_ENUM_CONSTANTS(plCameraBlendCurve::Cut, plCameraBlendCurve::Linear, plCameraBlendCurve::EaseIn, plCameraBlendCurve::EaseOut, plCameraBlendCurve::EaseInOut)
PL_END_STATIC_REFLECTED_ENUM;

PL_BEGIN_STATIC_REFLECTED_BITFLAGS(plCameraViewKind, 1)
  PL_BITFLAGS_CONSTANTS(plCameraViewKind::FirstPerson, plCameraViewKind::ThirdPerson, plCameraViewKind::Fixed, plCameraViewKind::Rail, plCameraViewKind::TopDown, plCameraViewKind::Other)
PL_END_STATIC_REFLECTED_BITFLAGS;

PL_BEGIN_STATIC_REFLECTED_ENUM(plCameraRequestSource, 1)
  PL_ENUM_CONSTANTS(plCameraRequestSource::Player, plCameraRequestSource::Volume, plCameraRequestSource::Script, plCameraRequestSource::Sequence)
PL_END_STATIC_REFLECTED_ENUM;

PL_BEGIN_STATIC_REFLECTED_ENUM(plCameraScreenAnchor, 1)
  PL_ENUM_CONSTANTS(plCameraScreenAnchor::UpperLeft, plCameraScreenAnchor::UpperCenter, plCameraScreenAnchor::UpperRight)
  PL_ENUM_CONSTANTS(plCameraScreenAnchor::MiddleLeft, plCameraScreenAnchor::Center, plCameraScreenAnchor::MiddleRight)
  PL_ENUM_CONSTANTS(plCameraScreenAnchor::LowerLeft, plCameraScreenAnchor::LowerCenter, plCameraScreenAnchor::LowerRight, plCameraScreenAnchor::Custom)
PL_END_STATIC_REFLECTED_ENUM;

PL_BEGIN_STATIC_REFLECTED_ENUM(plCameraSoloPoint, 1)
  PL_ENUM_CONSTANTS(plCameraSoloPoint::Center, plCameraSoloPoint::LeftThird, plCameraSoloPoint::RightThird, plCameraSoloPoint::LeadingRoom)
PL_END_STATIC_REFLECTED_ENUM;

PL_BEGIN_STATIC_REFLECTED_ENUM(plCameraFramingLayout, 1)
  PL_ENUM_CONSTANTS(plCameraFramingLayout::ThirdsDiagonal, plCameraFramingLayout::ThirdsHorizontal, plCameraFramingLayout::OverTheShoulder, plCameraFramingLayout::Custom)
PL_END_STATIC_REFLECTED_ENUM;

PL_BEGIN_STATIC_REFLECTED_ENUM(plCameraMirrorMode, 1)
  PL_ENUM_CONSTANTS(plCameraMirrorMode::Auto, plCameraMirrorMode::Off)
PL_END_STATIC_REFLECTED_ENUM;

PL_BEGIN_STATIC_REFLECTED_ENUM(plCameraLookAtFraming, 1)
  PL_ENUM_CONSTANTS(plCameraLookAtFraming::Balanced, plCameraLookAtFraming::Aim)
PL_END_STATIC_REFLECTED_ENUM;

PL_BEGIN_STATIC_REFLECTED_ENUM(plCameraLookAtHold, 1)
  PL_ENUM_CONSTANTS(plCameraLookAtHold::ForDuration, plCameraLookAtHold::WhileHeld)
PL_END_STATIC_REFLECTED_ENUM;

PL_BEGIN_STATIC_REFLECTED_ENUM(plCameraLookAtRelease, 1)
  PL_ENUM_CONSTANTS(plCameraLookAtRelease::KeepDirection, plCameraLookAtRelease::Return)
PL_END_STATIC_REFLECTED_ENUM;

PL_BEGIN_STATIC_REFLECTED_ENUM(plCameraFocusTarget, 1)
  PL_ENUM_CONSTANTS(plCameraFocusTarget::Subject, plCameraFocusTarget::Crosshair)
PL_END_STATIC_REFLECTED_ENUM;

PL_BEGIN_STATIC_REFLECTED_ENUM(plCameraImpulseType, 1)
  PL_ENUM_CONSTANTS(plCameraImpulseType::Shake, plCameraImpulseType::Recoil, plCameraImpulseType::Landing)
PL_END_STATIC_REFLECTED_ENUM;

PL_BEGIN_STATIC_REFLECTED_ENUM(plCameraYawMode, 1)
  PL_ENUM_CONSTANTS(plCameraYawMode::FollowTarget, plCameraYawMode::Free)
PL_END_STATIC_REFLECTED_ENUM;

PL_BEGIN_STATIC_REFLECTED_ENUM(plCameraRailMode, 1)
  PL_ENUM_CONSTANTS(plCameraRailMode::ClosestToTarget, plCameraRailMode::Scripted)
PL_END_STATIC_REFLECTED_ENUM;

PL_BEGIN_STATIC_REFLECTED_ENUM(plCameraVolumeShape, 1)
  PL_ENUM_CONSTANTS(plCameraVolumeShape::Box, plCameraVolumeShape::Sphere)
PL_END_STATIC_REFLECTED_ENUM;
// clang-format on

float plCameraBlendCurve::Evaluate(Enum curve, float fT)
{
  const float t = plMath::Clamp(fT, 0.0f, 1.0f);

  switch (curve)
  {
    case Cut:
      return 1.0f;
    case Linear:
      return t;
    case EaseIn:
      return t * t;
    case EaseOut:
      return 1.0f - (1.0f - t) * (1.0f - t);
    case EaseInOut:
      return t * t * (3.0f - 2.0f * t);
  }

  return t;
}

const char* plCameraRequestSource::GetName(Enum source)
{
  switch (source)
  {
    case Player:
      return "Player";
    case Volume:
      return "Volume";
    case Script:
      return "Script";
    case Sequence:
      return "Sequence";
  }

  return "?";
}

plVec2 plCameraScreenAnchor::GetPoint(Enum anchor, float fMirror, const plVec2& vCustom)
{
  if (anchor == Custom)
    return plVec2(vCustom.x * fMirror, vCustom.y);

  const plInt32 iColumn = static_cast<plInt32>(anchor) % 3;
  const plInt32 iRow = static_cast<plInt32>(anchor) / 3;

  return plVec2((iColumn - 1) / 3.0f * fMirror, (1 - iRow) / 3.0f);
}

bool plCameraFramingLayout::GetAnchors(Enum layout, plCameraScreenAnchor::Enum& out_player, plCameraScreenAnchor::Enum& out_target)
{
  switch (layout)
  {
    case ThirdsDiagonal:
      out_player = plCameraScreenAnchor::LowerLeft;
      out_target = plCameraScreenAnchor::UpperRight;
      return true;

    case ThirdsHorizontal:
      out_player = plCameraScreenAnchor::MiddleLeft;
      out_target = plCameraScreenAnchor::MiddleRight;
      return true;

    case OverTheShoulder:
      out_player = plCameraScreenAnchor::LowerLeft;
      out_target = plCameraScreenAnchor::MiddleRight;
      return true;

    case Custom:
      break;
  }

  return false;
}
