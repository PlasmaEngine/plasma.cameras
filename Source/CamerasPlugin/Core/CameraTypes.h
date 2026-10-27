#pragma once

#include <CamerasPlugin/CamerasDLL.h>
#include <Foundation/Reflection/Reflection.h>
#include <Foundation/Types/Bitflags.h>

/// \brief How a blend between two camera poses is shaped over its duration.
struct PL_CAMERAS_DLL plCameraBlendCurve
{
  using StorageType = plUInt8;

  enum Enum : StorageType
  {
    Cut,       ///< Jumps straight to the new pose.
    Linear,    ///< Constant speed.
    EaseIn,    ///< Starts slow.
    EaseOut,   ///< Ends slow.
    EaseInOut, ///< Starts and ends slow.

    Default = EaseInOut
  };

  /// \brief Maps a linear blend fraction (0..1) through the curve.
  static float Evaluate(Enum curve, float fT);
};

PL_DECLARE_REFLECTABLE_TYPE(PL_CAMERAS_DLL, plCameraBlendCurve);

/// \brief The kind of a camera view, used to scope modifiers to some views only.
PL_DECLARE_FLAGS_WITH_DEFAULT(plUInt32, plCameraViewKind, 0xFFFFFFFF,
  FirstPerson,
  ThirdPerson,
  Fixed,
  Rail,
  TopDown,
  Other);

PL_DECLARE_REFLECTABLE_TYPE(PL_CAMERAS_DLL, plCameraViewKind);

/// \brief Which pass of the modifier stack a modifier runs in.
///
/// Framing modifiers aim the camera and run first. Effect modifiers add motion on top of the framed pose.
/// Keeping them in fixed stages means a composition modifier never sees shake as subject drift and cancels it.
struct PL_CAMERAS_DLL plCameraModifierStage
{
  using StorageType = plUInt8;

  enum Enum : StorageType
  {
    Framing,
    Effects,

    Default = Effects
  };
};

/// \brief Where a camera view request came from. Only used for debugging and for releasing requests by source.
struct PL_CAMERAS_DLL plCameraRequestSource
{
  using StorageType = plUInt8;

  enum Enum : StorageType
  {
    Player,
    Volume,
    Script,
    Sequence,

    Default = Script
  };

  static const char* GetName(Enum source);
};

PL_DECLARE_REFLECTABLE_TYPE(PL_CAMERAS_DLL, plCameraRequestSource);

/// \brief One of the nine rule-of-thirds points on screen, or a custom point.
struct PL_CAMERAS_DLL plCameraScreenAnchor
{
  using StorageType = plUInt8;

  enum Enum : StorageType
  {
    UpperLeft,
    UpperCenter,
    UpperRight,
    MiddleLeft,
    Center,
    MiddleRight,
    LowerLeft,
    LowerCenter,
    LowerRight,
    Custom,

    Default = Center
  };

  /// \brief The anchor in normalized device coordinates (-1..1, +Y up). Thirds sit at +-1/3.
  ///
  /// \a fMirror of -1 flips the anchor left to right. \a vCustom is returned for Custom.
  static plVec2 GetPoint(Enum anchor, float fMirror = 1.0f, const plVec2& vCustom = plVec2::MakeZero());
};

PL_DECLARE_REFLECTABLE_TYPE(PL_CAMERAS_DLL, plCameraScreenAnchor);

/// \brief Where a single subject sits on screen when nothing else is being framed.
struct PL_CAMERAS_DLL plCameraSoloPoint
{
  using StorageType = plUInt8;

  enum Enum : StorageType
  {
    Center,
    LeftThird,
    RightThird,
    LeadingRoom, ///< The third behind the subject's direction of travel, so it has room to move into.

    Default = Center
  };
};

PL_DECLARE_REFLECTABLE_TYPE(PL_CAMERAS_DLL, plCameraSoloPoint);

/// \brief Ready-made screen layouts for framing two subjects (the player and a target).
struct PL_CAMERAS_DLL plCameraFramingLayout
{
  using StorageType = plUInt8;

  enum Enum : StorageType
  {
    ThirdsDiagonal,   ///< Player lower left, target upper right.
    ThirdsHorizontal, ///< Player middle left, target middle right.
    OverTheShoulder,  ///< Player lower left, target center right.
    Custom,           ///< Uses each subject's own anchor.

    Default = ThirdsDiagonal
  };

  /// \brief The player's and the target's anchors for a layout. Returns false for Custom.
  static bool GetAnchors(Enum layout, plCameraScreenAnchor::Enum& out_player, plCameraScreenAnchor::Enum& out_target);
};

PL_DECLARE_REFLECTABLE_TYPE(PL_CAMERAS_DLL, plCameraFramingLayout);

/// \brief Whether a two-subject layout may be flipped left to right.
struct PL_CAMERAS_DLL plCameraMirrorMode
{
  using StorageType = plUInt8;

  enum Enum : StorageType
  {
    Auto, ///< Flip when the target is on the other side of the player, which needs less camera travel.
    Off,

    Default = Auto
  };
};

PL_DECLARE_REFLECTABLE_TYPE(PL_CAMERAS_DLL, plCameraMirrorMode);

/// \brief How the Look At modifier frames its target.
struct PL_CAMERAS_DLL plCameraLookAtFraming
{
  using StorageType = plUInt8;

  enum Enum : StorageType
  {
    Balanced, ///< Adds the target as a second subject, so the camera moves to frame the player and the target together.
    Aim,      ///< Turns the camera to the target, optionally zooming. The player is not kept on screen.

    Default = Balanced
  };
};

PL_DECLARE_REFLECTABLE_TYPE(PL_CAMERAS_DLL, plCameraLookAtFraming);

/// \brief When the Look At modifier lets go.
struct PL_CAMERAS_DLL plCameraLookAtHold
{
  using StorageType = plUInt8;

  enum Enum : StorageType
  {
    ForDuration,
    WhileHeld,

    Default = ForDuration
  };
};

PL_DECLARE_REFLECTABLE_TYPE(PL_CAMERAS_DLL, plCameraLookAtHold);

/// \brief What happens to the camera's direction after a look at ends.
struct PL_CAMERAS_DLL plCameraLookAtRelease
{
  using StorageType = plUInt8;

  enum Enum : StorageType
  {
    KeepDirection, ///< The view adopts the direction the camera ended up in.
    Return,        ///< The view goes back to where it was looking before.

    Default = KeepDirection
  };
};

PL_DECLARE_REFLECTABLE_TYPE(PL_CAMERAS_DLL, plCameraLookAtRelease);

/// \brief What the Focus modifier keeps sharp.
struct PL_CAMERAS_DLL plCameraFocusTarget
{
  using StorageType = plUInt8;

  enum Enum : StorageType
  {
    Subject,   ///< The view's subject, or the Look At target while one is framed.
    Crosshair, ///< Whatever is under the screen center.

    Default = Subject
  };
};

PL_DECLARE_REFLECTABLE_TYPE(PL_CAMERAS_DLL, plCameraFocusTarget);

/// \brief The kind of a one-shot camera impulse.
struct PL_CAMERAS_DLL plCameraImpulseType
{
  using StorageType = plUInt8;

  enum Enum : StorageType
  {
    Shake,   ///< Adds trauma to Shake modifiers, scaled by distance from the impulse position.
    Recoil,  ///< Kicks Recoil modifiers.
    Landing, ///< Dips Landing modifiers.

    Default = Shake
  };
};

PL_DECLARE_REFLECTABLE_TYPE(PL_CAMERAS_DLL, plCameraImpulseType);

/// \brief How the third-person view's yaw is driven.
struct PL_CAMERAS_DLL plCameraYawMode
{
  using StorageType = plUInt8;

  enum Enum : StorageType
  {
    FollowTarget, ///< Stays behind the target; look input adds an offset.
    Free,         ///< Orbits independently of the target's rotation.

    Default = FollowTarget
  };
};

PL_DECLARE_REFLECTABLE_TYPE(PL_CAMERAS_DLL, plCameraYawMode);

/// \brief How the rail view picks its position along the spline.
struct PL_CAMERAS_DLL plCameraRailMode
{
  using StorageType = plUInt8;

  enum Enum : StorageType
  {
    ClosestToTarget, ///< The point on the spline closest to the target.
    Scripted,        ///< A key set from script with SetRailKey().

    Default = ClosestToTarget
  };
};

PL_DECLARE_REFLECTABLE_TYPE(PL_CAMERAS_DLL, plCameraRailMode);

/// \brief The shape of a camera volume.
struct PL_CAMERAS_DLL plCameraVolumeShape
{
  using StorageType = plUInt8;

  enum Enum : StorageType
  {
    Box,
    Sphere,

    Default = Box
  };
};

PL_DECLARE_REFLECTABLE_TYPE(PL_CAMERAS_DLL, plCameraVolumeShape);
