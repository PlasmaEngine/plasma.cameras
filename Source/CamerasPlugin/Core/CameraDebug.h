#pragma once

#include <CamerasPlugin/Core/CameraPose.h>
#include <Foundation/Math/Color.h>

class plWorld;

/// \brief The Cameras.Debug cvar and the drawing it does.
///
/// Cameras.Debug 1 prints each director's request stack, blend and modifier contributions in the game view.
/// Cameras.Debug 2 also draws the views' rigs (arm, sweep, frustums) and the framing guides.
struct PL_CAMERAS_DLL plCameraDebug
{
  static plInt32 GetLevel();

  static void DrawPoseFrustum(const plWorld* pWorld, const plCameraPose& pose, const plColor& color, float fLength = 1.0f);
  static void DrawText(const plWorld* pWorld, plStringView sText, const plColor& color = plColor::White);

  /// \brief Draws the rule-of-thirds grid and a framing zone (dead zone, soft zone) around a screen anchor, in screen space.
  static void DrawFramingGuides(const plWorld* pWorld, const plVec2& vAnchor, const plVec2& vDeadZone, float fSoftZone, const plVec2& vSubject);
};
