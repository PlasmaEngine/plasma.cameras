#pragma once

#include <CamerasPlugin/Core/CameraTypes.h>
#include <Core/World/Declarations.h>
#include <Foundation/Math/Vec2.h>
#include <Foundation/Strings/String.h>

class plInputComponent;
class plGameObject;
class plWorld;

/// \brief Which input actions turn a view, read through a plInputComponent.
///
/// Either one vector action (x turns right, y looks up) or four directional actions. Both are read and added, so a
/// project can bind a stick to the vector action and keep its existing look buttons. Values are treated as
/// per-frame deltas in radians, scaled by Sensitivity and the player's look settings.
struct PL_CAMERAS_DLL plCameraLookInput
{
  plString m_sLookAction;
  plString m_sTurnLeftAction;
  plString m_sTurnRightAction;
  plString m_sLookUpAction;
  plString m_sLookDownAction;
  float m_fSensitivity = 1.0f;
  bool m_bInvertY = false;

  bool HasAnyAction() const;

  /// \brief The yaw (x, positive turns left) and pitch (y, positive looks up) to add this frame.
  plVec2 Read(const plInputComponent* pInput, float fScale) const;
};

PL_DECLARE_REFLECTABLE_TYPE(PL_CAMERAS_DLL, plCameraLookInput);

/// \brief The player's own look settings, stored with their input bindings (InputBindings.ddl) so an options menu can
/// change them.
///
/// Keys: Camera.LookSensitivity (multiplier, default 1), Camera.InvertY (0/1), Camera.FieldOfViewOffset (degrees,
/// default 0), Camera.MouseAssist and Camera.GamepadAssist (0-1, how strongly framing may turn a camera the player
/// is steering; defaults 0 and 1).
struct PL_CAMERAS_DLL plCameraPlayerSettings
{
  float m_fLookSensitivity = 1.0f;
  bool m_bInvertY = false;
  float m_fFieldOfViewOffset = 0.0f;

  /// Assist for the input scheme the player is using right now.
  float m_fAssist = 1.0f;

  /// \brief Reads the settings of the running game application, or the defaults when there is none.
  static plCameraPlayerSettings Get();

  static void SetLookSensitivity(float fValue);
  static void SetInvertY(bool bInvert);
  static void SetFieldOfViewOffset(float fDegrees);
};

/// \brief Finds the plInputComponent that drives a camera: on \a pFirst, then its ancestors, then on \a pSecond and its
/// ancestors. Returns nullptr when neither has one.
PL_CAMERAS_DLL const plInputComponent* plFindCameraInput(const plGameObject* pFirst, const plGameObject* pSecond);

/// \brief Reads an action's value from an input component, or 0 when the name is empty or there is no component.
PL_CAMERAS_DLL float plReadCameraAction(const plInputComponent* pInput, plStringView sAction);

/// \brief What camera code knows about the thing a view looks at: where it is, how it moves, whether it is grounded.
///
/// Grounded, airborne and crouching are read from whatever component on the subject exposes the script functions
/// IsStandingOnGround, IsInAir and IsCrouching (the character controllers do), so no physics package is linked.
struct PL_CAMERAS_DLL plCameraSubjectInfo
{
  plVec3 m_vPosition = plVec3::MakeZero();
  plVec3 m_vVelocity = plVec3::MakeZero();
  bool m_bValid = false;
  bool m_bOnGround = true;
  bool m_bInAir = false;
  bool m_bCrouching = false;

  /// Vertical speed in the frame the subject landed, negative when falling. Zero on every other frame.
  float m_fLandingSpeed = 0.0f;
};

/// \brief Tracks a subject object from frame to frame to derive its velocity, ground state and landings.
class PL_CAMERAS_DLL plCameraSubjectTracker
{
public:
  void Reset();
  const plCameraSubjectInfo& Update(const plWorld* pWorld, plGameObjectHandle hSubject, const plVec3& vPosition, float fDeltaSeconds);
  const plCameraSubjectInfo& GetInfo() const { return m_Info; }

private:
  static bool CallBoolFunction(const plGameObject* pObject, const char* szFunction, bool& out_bResult);

  plCameraSubjectInfo m_Info;
  plVec3 m_vLastPosition = plVec3::MakeZero();
  float m_fLastVerticalSpeed = 0.0f;
  bool m_bHasLast = false;
  bool m_bWasInAir = false;
};
