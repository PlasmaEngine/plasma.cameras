#include <CamerasPlugin/CamerasPCH.h>

#include <CamerasPlugin/Core/CameraDebug.h>
#include <CamerasPlugin/Core/CameraDirectorComponent.h>
#include <CamerasPlugin/Core/CameraMessages.h>
#include <CamerasPlugin/Core/CameraViewComponent.h>
#include <CamerasPlugin/Core/CameraWorldModule.h>
#include <CamerasPlugin/Modifiers/FramingModifiers.h>
#include <CamerasPlugin/Modifiers/FramingSolver.h>
#include <CamerasPlugin/Volumes/CameraPointOfInterestComponent.h>
#include <Core/Interfaces/PhysicsWorldModule.h>
#include <Core/World/SpatialSystem.h>
#include <Core/World/World.h>
#include <Foundation/Types/TagRegistry.h>

namespace
{
  plVariant ViewsDefault(plBitflags<plCameraViewKind> views)
  {
    return plVariant(static_cast<plInt64>(views.GetValue()));
  }

  float Follow(float fDeltaSeconds, plTime timeConstant)
  {
    if (!timeConstant.IsPositive())
      return 1.0f;

    return 1.0f - plMath::Exp(-fDeltaSeconds / timeConstant.AsFloatInSeconds());
  }

  float Smooth(float t)
  {
    t = plMath::Clamp(t, 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
  }

  /// The pose that frames for this modifier's purposes: the director's output if there is one, else the view's.
  const plCameraPose& GetCameraPose(const plCameraModifierContext& context)
  {
    if (context.m_pDirector != nullptr)
      return context.m_pDirector->GetOutputPose();

    return context.m_pView->GetLastPose();
  }

  /// The layout for a two-subject framing; Custom has no fixed anchors, so it falls back.
  plCameraFramingLayout::Enum TwoSubjectLayout(plEnum<plCameraFramingLayout> layout, plCameraFramingLayout::Enum fallback)
  {
    return layout == plCameraFramingLayout::Custom ? fallback : static_cast<plCameraFramingLayout::Enum>(layout.GetValue());
  }

  /// -1 when a layout should be flipped so its target side matches where the target is, else 1.
  float DecideMirror(plCameraMirrorMode::Enum mode, const plCameraPose& camera, const plVec3& vPlayer, const plVec3& vTarget, const plVec2& vPlayerAnchor,
    const plVec2& vTargetAnchor)
  {
    if (mode != plCameraMirrorMode::Auto)
      return 1.0f;

    const plVec3 vRight = -camera.GetLeft();
    const float fSide = (vTarget - vPlayer).Dot(vRight);
    const float fLayoutSide = vTargetAnchor.x - vPlayerAnchor.x;

    if (plMath::Abs(fLayoutSide) < 0.001f || plMath::Abs(fSide) < 0.001f)
      return 1.0f;

    return plMath::Sign(fSide) == plMath::Sign(fLayoutSide) ? 1.0f : -1.0f;
  }
} // namespace

// clang-format off
PL_BEGIN_STATIC_REFLECTED_TYPE(plCameraFramingSubject, plNoBase, 1, plRTTIDefaultAllocator<plCameraFramingSubject>)
{
  PL_BEGIN_PROPERTIES
  {
    PL_MEMBER_PROPERTY("Object", m_sObject),
    PL_MEMBER_PROPERTY("Offset", m_vOffset),
    PL_ENUM_MEMBER_PROPERTY("Anchor", plCameraScreenAnchor, m_Anchor)->AddAttributes(new plDefaultValueAttribute(plCameraScreenAnchor::UpperRight)),
    PL_MEMBER_PROPERTY("CustomAnchor", m_vCustomAnchor),
    PL_MEMBER_PROPERTY("Weight", m_fWeight)->AddAttributes(new plDefaultValueAttribute(1.0f), new plClampValueAttribute(0.0f, 1.0f)),
    PL_MEMBER_PROPERTY("MustKeep", m_bMustKeep),
  }
  PL_END_PROPERTIES;
}
PL_END_STATIC_REFLECTED_TYPE;

PL_BEGIN_DYNAMIC_REFLECTED_TYPE(plCameraCompositionModifier, 1, plRTTIDefaultAllocator<plCameraCompositionModifier>)
{
  PL_BEGIN_PROPERTIES
  {
    PL_BITFLAGS_ACCESSOR_PROPERTY("Views", plCameraViewKind, GetViewsProperty, SetViewsProperty)->AddAttributes(new plDefaultValueAttribute(ViewsDefault(plCameraViewKind::ThirdPerson | plCameraViewKind::Fixed | plCameraViewKind::Rail))),
    PL_ENUM_MEMBER_PROPERTY("SoloPoint", plCameraSoloPoint, m_SoloPoint),
    PL_MEMBER_PROPERTY("DeadZone", m_vDeadZone)->AddAttributes(new plDefaultValueAttribute(plVec2(0.12f, 0.10f)), new plClampValueAttribute(plVec2(0.0f), plVec2(0.9f))),
    PL_MEMBER_PROPERTY("SoftZone", m_fSoftZone)->AddAttributes(new plDefaultValueAttribute(0.55f), new plClampValueAttribute(0.05f, 0.95f)),
    PL_MEMBER_PROPERTY("Damping", m_Damping)->AddAttributes(new plDefaultValueAttribute(plTime::MakeFromSeconds(0.35))),
    PL_ENUM_MEMBER_PROPERTY("Layout", plCameraFramingLayout, m_Layout)->AddAttributes(new plGroupAttribute("Several Subjects")),
    PL_ENUM_MEMBER_PROPERTY("Mirror", plCameraMirrorMode, m_Mirror)->AddAttributes(new plGroupAttribute("Several Subjects")),
    PL_ARRAY_MEMBER_PROPERTY("Subjects", m_Subjects)->AddAttributes(new plGroupAttribute("Several Subjects")),
    PL_MEMBER_PROPERTY("MinDistance", m_fMinDistance)->AddAttributes(new plDefaultValueAttribute(2.5f), new plClampValueAttribute(0.1f, 1000.0f), new plGroupAttribute("Several Subjects")),
    PL_MEMBER_PROPERTY("MaxDistance", m_fMaxDistance)->AddAttributes(new plDefaultValueAttribute(10.0f), new plClampValueAttribute(0.1f, 1000.0f), new plGroupAttribute("Several Subjects")),
    PL_MEMBER_PROPERTY("MaxFieldOfView", m_fMaxFieldOfView)->AddAttributes(new plClampValueAttribute(0.0f, 170.0f), new plGroupAttribute("Several Subjects")),
    PL_MEMBER_PROPERTY("CollisionLayer", m_uiCollisionLayer)->AddAttributes(new plDynamicEnumAttribute("PhysicsCollisionLayer"), new plGroupAttribute("Several Subjects")),
  }
  PL_END_PROPERTIES;
}
PL_END_DYNAMIC_REFLECTED_TYPE;

PL_BEGIN_DYNAMIC_REFLECTED_TYPE(plCameraLookAtModifier, 1, plRTTIDefaultAllocator<plCameraLookAtModifier>)
{
  PL_BEGIN_PROPERTIES
  {
    PL_BITFLAGS_ACCESSOR_PROPERTY("Views", plCameraViewKind, GetViewsProperty, SetViewsProperty)->AddAttributes(new plDefaultValueAttribute(ViewsDefault(plCameraViewKind::FirstPerson | plCameraViewKind::ThirdPerson))),
    PL_MEMBER_PROPERTY("Action", m_sAction)->AddAttributes(new plDefaultValueAttribute("LookAtInterest")),
    PL_ENUM_MEMBER_PROPERTY("Framing", plCameraLookAtFraming, m_Framing),
    PL_ENUM_MEMBER_PROPERTY("Layout", plCameraFramingLayout, m_Layout),
    PL_ENUM_MEMBER_PROPERTY("Mirror", plCameraMirrorMode, m_Mirror),
    PL_MEMBER_PROPERTY("BlendIn", m_BlendIn)->AddAttributes(new plDefaultValueAttribute(plTime::MakeFromSeconds(0.6))),
    PL_ENUM_MEMBER_PROPERTY("Hold", plCameraLookAtHold, m_Hold),
    PL_MEMBER_PROPERTY("Duration", m_Duration)->AddAttributes(new plDefaultValueAttribute(plTime::MakeFromSeconds(3.0))),
    PL_MEMBER_PROPERTY("ZoomFieldOfView", m_fZoomFieldOfView)->AddAttributes(new plDefaultValueAttribute(45.0f), new plClampValueAttribute(1.0f, 170.0f)),
    PL_ENUM_MEMBER_PROPERTY("Release", plCameraLookAtRelease, m_Release),
    PL_MEMBER_PROPERTY("BreakOnInput", m_bBreakOnInput)->AddAttributes(new plDefaultValueAttribute(true)),
  }
  PL_END_PROPERTIES;
}
PL_END_DYNAMIC_REFLECTED_TYPE;

PL_BEGIN_DYNAMIC_REFLECTED_TYPE(plCameraLookHintModifier, 1, plRTTIDefaultAllocator<plCameraLookHintModifier>)
{
  PL_BEGIN_PROPERTIES
  {
    PL_BITFLAGS_ACCESSOR_PROPERTY("Views", plCameraViewKind, GetViewsProperty, SetViewsProperty)->AddAttributes(new plDefaultValueAttribute(ViewsDefault(plCameraViewKind::ThirdPerson))),
    PL_MEMBER_PROPERTY("Speed", m_Speed)->AddAttributes(new plDefaultValueAttribute(plAngle::MakeFromDegree(8.0f))),
    PL_MEMBER_PROPERTY("IdleTime", m_IdleTime)->AddAttributes(new plDefaultValueAttribute(plTime::MakeFromSeconds(1.5))),
  }
  PL_END_PROPERTIES;
}
PL_END_DYNAMIC_REFLECTED_TYPE;

PL_BEGIN_DYNAMIC_REFLECTED_TYPE(plCameraFocusModifier, 1, plRTTIDefaultAllocator<plCameraFocusModifier>)
{
  PL_BEGIN_PROPERTIES
  {
    PL_BITFLAGS_ACCESSOR_PROPERTY("Views", plCameraViewKind, GetViewsProperty, SetViewsProperty)->AddAttributes(new plDefaultValueAttribute(ViewsDefault(plCameraViewKind::Default))),
    PL_ENUM_MEMBER_PROPERTY("Target", plCameraFocusTarget, m_Target),
    PL_MEMBER_PROPERTY("PullTime", m_PullTime)->AddAttributes(new plDefaultValueAttribute(plTime::MakeFromSeconds(0.4))),
    PL_MEMBER_PROPERTY("MaxDistance", m_fMaxDistance)->AddAttributes(new plDefaultValueAttribute(200.0f), new plClampValueAttribute(1.0f, 100000.0f)),
  }
  PL_END_PROPERTIES;
}
PL_END_DYNAMIC_REFLECTED_TYPE;

PL_BEGIN_DYNAMIC_REFLECTED_TYPE(plCameraLockOnModifier, 1, plRTTIDefaultAllocator<plCameraLockOnModifier>)
{
  PL_BEGIN_PROPERTIES
  {
    PL_BITFLAGS_ACCESSOR_PROPERTY("Views", plCameraViewKind, GetViewsProperty, SetViewsProperty)->AddAttributes(new plDefaultValueAttribute(ViewsDefault(plCameraViewKind::ThirdPerson))),
    PL_MEMBER_PROPERTY("Action", m_sAction)->AddAttributes(new plDefaultValueAttribute("LockOn")),
    PL_MEMBER_PROPERTY("SwitchAction", m_sSwitchAction),
    PL_MEMBER_PROPERTY("Toggle", m_bToggle)->AddAttributes(new plDefaultValueAttribute(true)),
    PL_MEMBER_PROPERTY("TargetTag", m_sTargetTag)->AddAttributes(new plDefaultValueAttribute("LockOnTarget")),
    PL_MEMBER_PROPERTY("MaxDistance", m_fMaxDistance)->AddAttributes(new plDefaultValueAttribute(25.0f), new plClampValueAttribute(1.0f, 1000.0f)),
    PL_MEMBER_PROPERTY("TargetOffset", m_vTargetOffset)->AddAttributes(new plDefaultValueAttribute(plVec3(0, 0, 1.2f))),
    PL_ENUM_MEMBER_PROPERTY("Layout", plCameraFramingLayout, m_Layout)->AddAttributes(new plDefaultValueAttribute(plCameraFramingLayout::OverTheShoulder)),
    PL_ENUM_MEMBER_PROPERTY("Mirror", plCameraMirrorMode, m_Mirror),
    PL_MEMBER_PROPERTY("BlendIn", m_BlendIn)->AddAttributes(new plDefaultValueAttribute(plTime::MakeFromSeconds(0.4))),
  }
  PL_END_PROPERTIES;
}
PL_END_DYNAMIC_REFLECTED_TYPE;
// clang-format on

//////////////////////////////////////////////////////////////////////////
// Composition

plCameraCompositionModifier::plCameraCompositionModifier()
{
  m_Views = plCameraViewKind::ThirdPerson | plCameraViewKind::Fixed | plCameraViewKind::Rail;
}

void plCameraCompositionModifier::Reset()
{
  m_bHasAim = false;
  m_bHasSolved = false;
  m_bMirrorDecided = false;
  m_fSoloX = 0.0f;
}

void plCameraCompositionModifier::Prepare(plCameraModifierContext& ref_context)
{
  ref_context.m_bHasComposition = m_bEnabled && m_Views.IsAnySet(ref_context.m_ViewKind);
}

plVec2 plCameraCompositionModifier::GetSoloAnchor(const plCameraModifierContext& context, const plCameraPose& pose)
{
  float fTargetX = 0.0f;

  switch (m_SoloPoint)
  {
    case plCameraSoloPoint::Center:
      break;
    case plCameraSoloPoint::LeftThird:
      fTargetX = -1.0f / 3.0f;
      break;
    case plCameraSoloPoint::RightThird:
      fTargetX = 1.0f / 3.0f;
      break;
    case plCameraSoloPoint::LeadingRoom:
    {
      // the third behind the direction of travel; only switch sides on clear movement
      const float fScreenSpeed = context.m_Subject.m_vVelocity.Dot(-pose.GetLeft());
      if (plMath::Abs(fScreenSpeed) > 0.35f)
        m_fLeadSide = -plMath::Sign(fScreenSpeed);

      fTargetX = m_fLeadSide / 3.0f;
      break;
    }
  }

  m_fSoloX = plMath::Lerp(m_fSoloX, fTargetX, Follow(context.m_fDeltaSeconds, plTime::MakeFromSeconds(0.6)));
  return plVec2(m_fSoloX, 0.0f);
}

float plCameraCompositionModifier::SweepFromSubject(const plCameraModifierContext& context, const plVec3& vFrom, const plVec3& vTo) const
{
  plVec3 vDir = vTo - vFrom;
  const float fDistance = vDir.GetLength();

  if (fDistance < 0.01f)
    return fDistance;

  vDir /= fDistance;

  const plPhysicsWorldModuleInterface* pPhysics = context.m_pWorld->GetModuleReadOnly<plPhysicsWorldModuleInterface>();
  if (pPhysics == nullptr)
    return fDistance;

  plPhysicsQueryParameters params;
  params.m_uiCollisionLayer = m_uiCollisionLayer;
  params.m_ShapeTypes = plPhysicsShapeType::Static;

  plPhysicsCastResult result;
  if (pPhysics->SweepTestSphere(result, 0.2f, vFrom, vDir, fDistance, params))
    return plMath::Max(0.3f, result.m_fDistance);

  return fDistance;
}

bool plCameraCompositionModifier::GetConfiguredSubject(const plCameraModifierContext& context, plVec3& out_vPosition, plVec2& out_vAnchor, plVec2& out_vPlayerAnchor)
{
  for (const plCameraFramingSubject& subject : m_Subjects)
  {
    if (subject.m_sObject.IsEmpty() || subject.m_fWeight <= 0.0f)
      continue;

    plGameObject* pObject = context.m_pWorld->SearchForObject(subject.m_sObject, context.m_pView->GetOwner());
    if (pObject == nullptr)
      continue;

    out_vPosition = pObject->GetGlobalTransform().TransformPosition(subject.m_vOffset);

    plCameraScreenAnchor::Enum playerAnchor = plCameraScreenAnchor::Center;
    plCameraScreenAnchor::Enum targetAnchor = subject.m_Anchor;

    if (!plCameraFramingLayout::GetAnchors(m_Layout, playerAnchor, targetAnchor))
    {
      playerAnchor = plCameraScreenAnchor::Center;
      targetAnchor = subject.m_Anchor;
    }

    const plVec2 vPlayer = plCameraScreenAnchor::GetPoint(playerAnchor);
    const plVec2 vTarget = plCameraScreenAnchor::GetPoint(targetAnchor, 1.0f, subject.m_vCustomAnchor);

    if (!m_bMirrorDecided)
    {
      m_fMirror = DecideMirror(m_Mirror, GetCameraPose(context), context.m_Subject.m_vPosition, out_vPosition, vPlayer, vTarget);
      m_bMirrorDecided = true;
    }

    out_vPlayerAnchor = plVec2(vPlayer.x * m_fMirror, vPlayer.y);
    out_vAnchor = plVec2(vTarget.x * m_fMirror, vTarget.y);
    return true;
  }

  m_bMirrorDecided = false;
  return false;
}

void plCameraCompositionModifier::Apply(plCameraModifierContext& ref_context, plCameraPose& ref_pose, float fWeight)
{
  m_fDebugAmount = 0.0f;
  m_szState = "-";

  if (fWeight <= 0.0f || !ref_context.m_Subject.m_bValid)
  {
    m_bHasAim = false;
    return;
  }

  const plVec2 vSoloAnchor = GetSoloAnchor(ref_context, ref_pose);

  if (ref_context.m_Framing.m_bActive && ref_context.m_Framing.m_fWeight > 0.0f)
  {
    ApplyPair(ref_context, ref_pose, fWeight, ref_context.m_Framing.m_vTarget, ref_context.m_Framing.m_vPlayerAnchor, ref_context.m_Framing.m_vTargetAnchor,
      ref_context.m_Framing.m_fWeight, vSoloAnchor);
    return;
  }

  plVec3 vTarget;
  plVec2 vTargetAnchor, vPlayerAnchor;
  if (GetConfiguredSubject(ref_context, vTarget, vTargetAnchor, vPlayerAnchor))
  {
    ApplyPair(ref_context, ref_pose, fWeight, vTarget, vPlayerAnchor, vTargetAnchor, 1.0f, vSoloAnchor);
    return;
  }

  m_bHasSolved = false;
  ApplySolo(ref_context, ref_pose, fWeight, vSoloAnchor);
}

void plCameraCompositionModifier::ApplySolo(plCameraModifierContext& ref_context, plCameraPose& ref_pose, float fWeight, const plVec2& vAnchor)
{
  const plVec3 vSubject = ref_context.m_Subject.m_vPosition;
  const plCameraProjection projection = ref_context.GetProjection(ref_pose);

  const bool bPlayerView = ref_context.m_ViewKind.IsAnySet(plCameraViewKind::FirstPerson | plCameraViewKind::ThirdPerson);

  // A jump this large is a cut or a teleport: frame from scratch. While the player steers a view, the view's own aim
  // is what they asked for: adopt it, so turning the camera never drags the subject toward the soft zone's edge.
  if (!m_bHasAim || (ref_pose.m_vPosition - m_vLastPosition).GetLengthSquared() > 3.0f * 3.0f)
  {
    m_qAim = plCameraFramingSolver::AimAt(ref_pose.m_vPosition, vSubject, vAnchor, projection);
    m_bHasAim = true;
  }
  else if (bPlayerView && ref_context.m_bPlayerSteering)
  {
    m_qAim = ref_pose.m_qRotation;
  }

  plCameraPose aimed = ref_pose;
  aimed.m_qRotation = m_qAim;

  plVec2 vNdc;
  if (!projection.Project(aimed, vSubject, vNdc))
  {
    m_qAim = plCameraFramingSolver::AimAt(ref_pose.m_vPosition, vSubject, vAnchor, projection);
    vNdc = vAnchor;
  }

  // On views the player steers, framing only assists: not at all while being steered, and scaled by the player's
  // assist setting otherwise. The hard limit (the soft zone's edge) always holds.
  float fSoftFollow = Follow(ref_context.m_fDeltaSeconds, m_Damping);

  if (bPlayerView)
    fSoftFollow *= ref_context.m_bPlayerSteering ? 0.0f : ref_context.m_PlayerSettings.m_fAssist;

  const plVec2 vOffset = vNdc - vAnchor;
  const plVec2 vCorrection(plCameraFramingSolver::ZoneCorrection(vOffset.x, m_vDeadZone.x, m_fSoftZone, fSoftFollow),
    plCameraFramingSolver::ZoneCorrection(vOffset.y, m_vDeadZone.y, m_fSoftZone, fSoftFollow));

  if (!vCorrection.IsZero(0.00001f))
    m_qAim = plCameraFramingSolver::AimAt(ref_pose.m_vPosition, vSubject, vNdc - vCorrection, projection);

  m_vLastPosition = ref_pose.m_vPosition;
  ref_pose.m_qRotation = plQuat::MakeSlerp(ref_pose.m_qRotation, m_qAim, fWeight);

  const bool bOutsideDead = plMath::Abs(vOffset.x) > m_vDeadZone.x || plMath::Abs(vOffset.y) > m_vDeadZone.y;
  m_szState = bOutsideDead ? "soft zone" : "dead zone";
  m_fDebugAmount = bOutsideDead ? 1.0f : 0.3f;

  if (plCameraDebug::GetLevel() > 1)
    plCameraDebug::DrawFramingGuides(ref_context.m_pWorld, vAnchor, m_vDeadZone, m_fSoftZone, vNdc - vCorrection);
}

void plCameraCompositionModifier::ApplyPair(plCameraModifierContext& ref_context, plCameraPose& ref_pose, float fWeight, const plVec3& vTarget,
  const plVec2& vPlayerAnchor, const plVec2& vTargetAnchor, float fPairWeight, const plVec2& vSoloAnchor)
{
  const plVec3 vSubject = ref_context.m_Subject.m_vPosition;
  plCameraProjection projection = ref_context.GetProjection(ref_pose);

  plCameraFramingSolver::TwoSubjectResult solved = plCameraFramingSolver::FrameTwo(vSubject, vTarget, vPlayerAnchor, vTargetAnchor, projection,
    ref_pose.m_vPosition, m_fMinDistance, m_fMaxDistance);

  // optional fallback: a wider lens can fit what no position can
  float fFov = ref_pose.m_fFieldOfView;
  if (!solved.m_bExact && m_fMaxFieldOfView > ref_pose.m_fFieldOfView)
  {
    plCameraPose wide = ref_pose;
    wide.m_fFieldOfView = m_fMaxFieldOfView;
    const plCameraProjection wideProjection = ref_context.GetProjection(wide);
    const auto wider = plCameraFramingSolver::FrameTwo(vSubject, vTarget, vPlayerAnchor, vTargetAnchor, wideProjection, ref_pose.m_vPosition, m_fMinDistance, m_fMaxDistance);

    if (wider.m_fError < solved.m_fError)
    {
      solved = wider;
      projection = wideProjection;
      fFov = m_fMaxFieldOfView;
    }
  }

  // collision: pull in along the line from the subject, which keeps the subject pinned and lets the target relax
  plVec3 vSolvedPosition = solved.m_vPosition;
  const float fClear = SweepFromSubject(ref_context, vSubject, vSolvedPosition);
  const float fWanted = (vSolvedPosition - vSubject).GetLength();
  if (fClear < fWanted - 0.01f)
    vSolvedPosition = vSubject + (vSolvedPosition - vSubject) * (fClear / fWanted);

  // the solution can jump when a subject crosses the axis; ease the camera there instead
  if (!m_bHasSolved)
    m_vSolved = vSolvedPosition;
  else
    m_vSolved = plMath::Lerp(m_vSolved, vSolvedPosition, Follow(ref_context.m_fDeltaSeconds, plTime::MakeFromSeconds(0.25)));

  m_bHasSolved = true;

  const float w = plMath::Clamp(fPairWeight, 0.0f, 1.0f) * fWeight;
  ref_pose.m_vPosition = plMath::Lerp(ref_pose.m_vPosition, m_vSolved, w);
  ref_pose.m_fFieldOfView = plMath::Lerp(ref_pose.m_fFieldOfView, fFov, w);

  // slide the subject's screen point from its solo anchor to the layout anchor, so neither end of the blend pops
  const plVec2 vAnchor = plMath::Lerp(vSoloAnchor, vPlayerAnchor, w);
  ref_pose.m_qRotation = plCameraFramingSolver::AimAt(ref_pose.m_vPosition, vSubject, vAnchor, ref_context.GetProjection(ref_pose));

  m_qAim = ref_pose.m_qRotation;
  m_vLastPosition = ref_pose.m_vPosition;
  m_bHasAim = true;

  plVec2 vTargetNdc;
  m_fLastError = ref_context.GetProjection(ref_pose).Project(ref_pose, vTarget, vTargetNdc) ? (vTargetNdc - vTargetAnchor).GetLength() : 1.0f;
  m_szState = solved.m_bExact ? "balanced" : "balanced (relaxed)";
  m_fDebugAmount = w;

  if (plCameraDebug::GetLevel() > 1)
    plCameraDebug::DrawFramingGuides(ref_context.m_pWorld, vTargetAnchor, plVec2(0.02f), 0.05f, vTargetNdc);
}

void plCameraCompositionModifier::GetDebugText(plStringBuilder& out_sText) const
{
  if (m_bHasSolved)
    out_sText.SetFormat("{}, error {}", m_szState, plArgF(m_fLastError, 3));
  else
    out_sText = m_szState;
}

//////////////////////////////////////////////////////////////////////////
// Look At

plCameraLookAtModifier::plCameraLookAtModifier()
{
  m_Views = plCameraViewKind::FirstPerson | plCameraViewKind::ThirdPerson;
}

void plCameraLookAtModifier::Reset()
{
  m_bActive = false;
  m_fWeight = 0.0f;
  m_hTarget.Invalidate();
  m_hBest.Invalidate();
}

void plCameraLookAtModifier::Trigger(plCameraPointOfInterestComponent* pPoint)
{
  if (pPoint != nullptr)
    m_hPendingTrigger = pPoint->GetHandle();
}

bool plCameraLookAtModifier::IsVisible(const plCameraModifierContext& context, const plVec3& vPoint) const
{
  const plPhysicsWorldModuleInterface* pPhysics = context.m_pWorld->GetModuleReadOnly<plPhysicsWorldModuleInterface>();
  if (pPhysics == nullptr)
    return true;

  const plVec3 vFrom = GetCameraPose(context).m_vPosition;
  plVec3 vDir = vPoint - vFrom;
  const float fDistance = vDir.GetLength();

  if (fDistance < 0.5f)
    return true;

  vDir /= fDistance;

  plPhysicsQueryParameters params;
  params.m_ShapeTypes = plPhysicsShapeType::Static;

  plPhysicsCastResult result;
  return !pPhysics->Raycast(result, vFrom, vDir, fDistance - 0.5f, params);
}

void plCameraLookAtModifier::FindBestPoint(plCameraModifierContext& ref_context)
{
  const plCameraWorldModule* pModule = ref_context.m_pWorld->GetModuleReadOnly<plCameraWorldModule>();
  const plCameraPose& camera = GetCameraPose(ref_context);

  plComponentHandle hBest;
  plInt32 iBestPriority = plMath::MinValue<plInt32>();
  float fBestDistance = plMath::MaxValue<float>();

  if (pModule != nullptr && ref_context.m_Subject.m_bValid)
  {
    for (plComponentHandle hPoint : pModule->GetPointsOfInterest())
    {
      plCameraPointOfInterestComponent* pPoint = nullptr;
      if (!ref_context.m_pWorld->TryGetComponent(hPoint, pPoint) || !pPoint->IsActiveAndSimulating())
        continue;

      if (!pPoint->IsInRange(ref_context.m_Subject.m_vPosition))
        continue;

      const plVec3 vFrame = pPoint->GetFramePoint();
      const plVec3 vToPoint = (vFrame - camera.m_vPosition).GetNormalized();

      if (vToPoint.Dot(camera.GetForward()) < 0.2f)
        continue;

      if (pPoint->m_bRequireLineOfSight && !IsVisible(ref_context, vFrame))
        continue;

      if (pPoint->m_bAutoTrigger && !pPoint->HasAutoTriggered())
      {
        pPoint->MarkAutoTriggered();
        m_hPendingTrigger = hPoint;
      }

      const float fDistance = (pPoint->GetOwner()->GetGlobalPosition() - ref_context.m_Subject.m_vPosition).GetLength();
      if (pPoint->m_iPriority > iBestPriority || (pPoint->m_iPriority == iBestPriority && fDistance < fBestDistance))
      {
        hBest = hPoint;
        iBestPriority = pPoint->m_iPriority;
        fBestDistance = fDistance;
      }
    }
  }

  if (hBest == m_hBest)
    return;

  m_hBest = hBest;

  // tell the player's hierarchy, so game UI can show or hide its prompt
  plGameObject* pSubject = nullptr;
  if (!ref_context.m_pWorld->TryGetObject(ref_context.m_pView->GetSubjectObject(), pSubject))
    return;

  plMsgCameraPointOfInterestChanged msg;
  msg.m_hPointOfInterest = hBest;

  plCameraPointOfInterestComponent* pBest = nullptr;
  if (ref_context.m_pWorld->TryGetComponent(hBest, pBest))
    msg.m_sDisplayName = pBest->m_sDisplayName;

  pSubject->SendEventMessage(msg, ref_context.m_pView);
}

void plCameraLookAtModifier::Start(plCameraModifierContext& ref_context, plComponentHandle hPoint)
{
  plCameraPointOfInterestComponent* pPoint = nullptr;
  if (!ref_context.m_pWorld->TryGetComponent(hPoint, pPoint))
    return;

  const plCameraPose& camera = GetCameraPose(ref_context);

  m_bActive = true;
  m_hTarget = hPoint;
  m_vTarget = pPoint->GetFramePoint();
  m_fElapsed = 0.0f;
  m_fHoldSeconds = pPoint->m_HoldTime.IsPositive() ? pPoint->m_HoldTime.AsFloatInSeconds() : m_Duration.AsFloatInSeconds();
  m_fZoom = pPoint->m_fZoomFieldOfView > 0.0f ? pPoint->m_fZoomFieldOfView : m_fZoomFieldOfView;
  camera.GetYawPitch(m_SavedYaw, m_SavedPitch);

  plCameraScreenAnchor::Enum playerAnchor, targetAnchor;
  plCameraFramingLayout::GetAnchors(TwoSubjectLayout(m_Layout, plCameraFramingLayout::ThirdsDiagonal), playerAnchor,
    targetAnchor);

  m_fMirror = DecideMirror(m_Mirror, camera, ref_context.m_Subject.m_vPosition, m_vTarget, plCameraScreenAnchor::GetPoint(playerAnchor),
    plCameraScreenAnchor::GetPoint(targetAnchor));
}

void plCameraLookAtModifier::End(plCameraModifierContext& ref_context)
{
  m_bActive = false;

  if (m_Release == plCameraLookAtRelease::Return)
  {
    ref_context.m_bCommitLook = true;
    ref_context.m_bCommitExplicit = true;
    ref_context.m_CommitYaw = m_SavedYaw;
    ref_context.m_CommitPitch = m_SavedPitch;
  }
  else
  {
    ref_context.m_bCommitLook = true;
    ref_context.m_bCommitExplicit = false;
  }
}

void plCameraLookAtModifier::Prepare(plCameraModifierContext& ref_context)
{
  if (!m_Views.IsAnySet(ref_context.m_ViewKind))
    return;

  const float fDt = ref_context.m_fDeltaSeconds;

  if (!m_bActive)
    FindBestPoint(ref_context);

  const bool bDown = plReadCameraAction(ref_context.m_pInput, m_sAction) > 0.5f;
  const bool bPressed = bDown && !m_bWasDown;
  m_bWasDown = bDown;

  if (!m_bActive)
  {
    if (!m_hPendingTrigger.IsInvalidated())
      Start(ref_context, m_hPendingTrigger);
    else if (bPressed && !m_hBest.IsInvalidated())
      Start(ref_context, m_hBest);

    m_hPendingTrigger.Invalidate();
  }
  else
  {
    m_fElapsed += fDt;

    plCameraPointOfInterestComponent* pPoint = nullptr;
    const bool bGone = !ref_context.m_pWorld->TryGetComponent(m_hTarget, pPoint);

    if (!bGone)
      m_vTarget = pPoint->GetFramePoint();

    const float fBlendIn = m_BlendIn.AsFloatInSeconds();
    const bool bTimeUp = m_Hold == plCameraLookAtHold::ForDuration ? m_fElapsed >= fBlendIn + m_fHoldSeconds : (!bDown && m_fElapsed >= fBlendIn);
    const bool bBroken = m_bBreakOnInput && ref_context.m_bPlayerSteering && m_fElapsed > 0.2f;

    if (bGone || bTimeUp || bBroken)
      End(ref_context);
  }

  const float fBlendIn = plMath::Max(0.01f, m_BlendIn.AsFloatInSeconds());
  m_fWeight = m_bActive ? plMath::Min(1.0f, m_fWeight + fDt / fBlendIn) : plMath::Max(0.0f, m_fWeight - fDt / (fBlendIn * 0.8f));

  if (m_fWeight <= 0.0f)
    return;

  if (m_fWeight > 0.5f)
  {
    ref_context.m_bHasFocusPoint = true;
    ref_context.m_vFocusPoint = m_vTarget;
  }

  if (m_Framing == plCameraLookAtFraming::Balanced)
  {
    plCameraScreenAnchor::Enum playerAnchor, targetAnchor;
    plCameraFramingLayout::GetAnchors(TwoSubjectLayout(m_Layout, plCameraFramingLayout::ThirdsDiagonal), playerAnchor,
      targetAnchor);

    ref_context.m_Framing.m_bActive = true;
    ref_context.m_Framing.m_vTarget = m_vTarget;
    ref_context.m_Framing.m_vPlayerAnchor = plCameraScreenAnchor::GetPoint(playerAnchor, m_fMirror);
    ref_context.m_Framing.m_vTargetAnchor = plCameraScreenAnchor::GetPoint(targetAnchor, m_fMirror);
    ref_context.m_Framing.m_fWeight = Smooth(m_fWeight);
  }
}

void plCameraLookAtModifier::Apply(plCameraModifierContext& ref_context, plCameraPose& ref_pose, float fWeight)
{
  m_fDebugAmount = m_fWeight * fWeight;

  // balanced framing is done by the Composition modifier; without one, or for Aim, turn to the point here
  const bool bAimHere = m_Framing == plCameraLookAtFraming::Aim || !ref_context.m_bHasComposition;

  if (m_fWeight > 0.0f && bAimHere)
  {
    const float w = Smooth(m_fWeight) * fWeight;
    ref_pose.m_qRotation = plQuat::MakeSlerp(ref_pose.m_qRotation, plCameraPose::MakeLookRotation(m_vTarget - ref_pose.m_vPosition), w);
    ref_pose.m_fFieldOfView = plMath::Lerp(ref_pose.m_fFieldOfView, m_fZoom, w);
  }
}

void plCameraLookAtModifier::GetDebugText(plStringBuilder& out_sText) const
{
  if (m_bActive)
    out_sText.SetFormat("framing, weight {}{}", plArgF(m_fWeight, 2), m_fMirror < 0.0f ? ", mirrored" : "");
  else if (!m_hBest.IsInvalidated())
    out_sText = "point in range";
  else
    out_sText = "-";
}

//////////////////////////////////////////////////////////////////////////
// Look Hint

plCameraLookHintModifier::plCameraLookHintModifier()
{
  m_Views = plCameraViewKind::ThirdPerson;
}

void plCameraLookHintModifier::Apply(plCameraModifierContext& ref_context, plCameraPose& ref_pose, float fWeight)
{
  m_fDebugAmount = 0.0f;
  m_fIdle = ref_context.m_bPlayerSteering ? 0.0f : m_fIdle + ref_context.m_fDeltaSeconds;

  if (fWeight <= 0.0f || m_fIdle < m_IdleTime.AsFloatInSeconds() || ref_context.m_bCommitLook || ref_context.m_Framing.m_bActive || !ref_context.m_Subject.m_bValid)
    return;

  const plCameraWorldModule* pModule = ref_context.m_pWorld->GetModuleReadOnly<plCameraWorldModule>();
  if (pModule == nullptr)
    return;

  const plCameraPointOfInterestComponent* pNearest = nullptr;
  float fNearest = plMath::MaxValue<float>();

  for (plComponentHandle hPoint : pModule->GetPointsOfInterest())
  {
    const plCameraPointOfInterestComponent* pPoint = nullptr;
    if (!ref_context.m_pWorld->TryGetComponent(hPoint, pPoint) || !pPoint->IsInRange(ref_context.m_Subject.m_vPosition))
      continue;

    const float fDistance = (pPoint->GetOwner()->GetGlobalPosition() - ref_context.m_Subject.m_vPosition).GetLengthSquared();
    if (fDistance < fNearest)
    {
      fNearest = fDistance;
      pNearest = pPoint;
    }
  }

  if (pNearest == nullptr)
    return;

  plAngle yaw, pitch;
  ref_pose.GetYawPitch(yaw, pitch);

  const plVec3 vTo = pNearest->GetFramePoint() - ref_pose.m_vPosition;
  plAngle diff = plMath::ATan2(vTo.y, vTo.x) - yaw;
  diff.NormalizeRange();
  if (diff > plAngle::MakeFromDegree(180.0f))
    diff -= plAngle::MakeFromDegree(360.0f);

  const float fStep = m_Speed.GetRadian() * fWeight * ref_context.m_fDeltaSeconds;
  const plAngle step = plAngle::MakeFromRadian(plMath::Clamp(diff.GetRadian(), -fStep, fStep));

  // nudge the view itself, so the hint persists and the player takes over seamlessly
  ref_context.m_bCommitLook = true;
  ref_context.m_bCommitExplicit = true;
  ref_context.m_CommitYaw = yaw + step;
  ref_context.m_CommitPitch = pitch;

  ref_pose.m_qRotation = plCameraPose::MakeRotation(yaw + step, pitch);
  m_fDebugAmount = plMath::Min(1.0f, plMath::Abs(diff.GetRadian()));
}

void plCameraLookHintModifier::GetDebugText(plStringBuilder& out_sText) const
{
  out_sText = m_fDebugAmount > 0.0f ? "nudging" : "-";
}

//////////////////////////////////////////////////////////////////////////
// Focus

plCameraFocusModifier::plCameraFocusModifier() = default;

void plCameraFocusModifier::Apply(plCameraModifierContext& ref_context, plCameraPose& ref_pose, float fWeight)
{
  if (fWeight <= 0.0f)
    return;

  float fTarget = m_fMaxDistance;
  m_sFocusName = "far";

  if (ref_context.m_bHasFocusPoint)
  {
    fTarget = (ref_context.m_vFocusPoint - ref_pose.m_vPosition).GetLength();
    m_sFocusName = "look at target";
  }
  else if (m_Target == plCameraFocusTarget::Subject && ref_context.m_Subject.m_bValid)
  {
    fTarget = (ref_context.m_Subject.m_vPosition - ref_pose.m_vPosition).GetLength();
    m_sFocusName = "subject";
  }
  else if (m_Target == plCameraFocusTarget::Crosshair)
  {
    if (const plPhysicsWorldModuleInterface* pPhysics = ref_context.m_pWorld->GetModuleReadOnly<plPhysicsWorldModuleInterface>())
    {
      plPhysicsQueryParameters params;
      params.m_ShapeTypes = plPhysicsShapeType::Static | plPhysicsShapeType::Dynamic;

      plPhysicsCastResult result;
      if (pPhysics->Raycast(result, ref_pose.m_vPosition, ref_pose.GetForward(), m_fMaxDistance, params))
      {
        fTarget = result.m_fDistance;
        m_sFocusName = "crosshair";
      }
    }
  }

  m_fDistance = m_fDistance <= 0.0f ? fTarget : plMath::Lerp(m_fDistance, fTarget, Follow(ref_context.m_fDeltaSeconds, m_PullTime));
  m_fDebugAmount = fWeight;

  ref_pose.m_fFocusDistance = ref_pose.m_fFocusDistance > 0.0f ? plMath::Lerp(ref_pose.m_fFocusDistance, m_fDistance, fWeight) : m_fDistance;
}

void plCameraFocusModifier::GetDebugText(plStringBuilder& out_sText) const
{
  out_sText.SetFormat("{} m ({})", plArgF(m_fDistance, 1), m_sFocusName);
}

//////////////////////////////////////////////////////////////////////////
// Lock-On

plCameraLockOnModifier::plCameraLockOnModifier()
{
  m_Views = plCameraViewKind::ThirdPerson;
}

void plCameraLockOnModifier::Reset()
{
  m_hTarget.Invalidate();
  m_bWanted = false;
  m_fWeight = 0.0f;
}

plGameObjectHandle plCameraLockOnModifier::PickTarget(const plCameraModifierContext& context, plGameObjectHandle hAfter) const
{
  const plSpatialSystem* pSpatial = context.m_pWorld->GetSpatialSystem();
  if (pSpatial == nullptr || !context.m_Subject.m_bValid || m_sTargetTag.IsEmpty())
    return plGameObjectHandle();

  plTagSet tags;
  tags.Set(plTagRegistry::GetGlobalRegistry().RegisterTag(m_sTargetTag));

  plSpatialSystem::QueryParams params;
  params.m_uiCategoryBitmask = plDefaultSpatialDataCategories::RenderStatic.GetBitmask() | plDefaultSpatialDataCategories::RenderDynamic.GetBitmask();
  params.m_pIncludeTags = &tags;

  plHybridArray<plGameObject*, 32> candidates;
  pSpatial->FindObjectsInSphere(plBoundingSphere::MakeFromCenterAndRadius(context.m_Subject.m_vPosition, m_fMaxDistance), params, candidates);

  const plCameraPose& camera = GetCameraPose(context);
  plAngle cameraYaw, cameraPitch;
  camera.GetYawPitch(cameraYaw, cameraPitch);

  plAngle afterYaw;
  bool bHasAfter = false;
  plGameObject* pAfter = nullptr;
  if (context.m_pWorld->TryGetObject(hAfter, pAfter))
  {
    const plVec3 v = pAfter->GetGlobalPosition() - camera.m_vPosition;
    afterYaw = plMath::ATan2(v.y, v.x) - cameraYaw;
    afterYaw.NormalizeRange();
    bHasAfter = true;
  }

  plGameObjectHandle hBest;
  float fBest = plMath::MaxValue<float>();

  for (plGameObject* pCandidate : candidates)
  {
    if (pCandidate->GetHandle() == hAfter)
      continue;

    const plVec3 vTo = pCandidate->GetGlobalPosition() + m_vTargetOffset - camera.m_vPosition;
    if (vTo.GetNormalized().Dot(camera.GetForward()) < 0.3f)
      continue;

    plAngle yaw = plMath::ATan2(vTo.y, vTo.x) - cameraYaw;
    yaw.NormalizeRange();

    // without a current target: the one nearest the screen center; switching: the next one to the right
    float fScore = plMath::Min(yaw.GetRadian(), plMath::Pi<float>() * 2.0f - yaw.GetRadian());
    if (bHasAfter)
    {
      plAngle step = afterYaw - yaw;
      step.NormalizeRange();
      fScore = step.GetRadian();
    }

    if (fScore < fBest)
    {
      fBest = fScore;
      hBest = pCandidate->GetHandle();
    }
  }

  return hBest;
}

void plCameraLockOnModifier::Prepare(plCameraModifierContext& ref_context)
{
  if (!m_Views.IsAnySet(ref_context.m_ViewKind))
    return;

  const bool bDown = plReadCameraAction(ref_context.m_pInput, m_sAction) > 0.5f;
  const bool bPressed = bDown && !m_bWasDown;
  m_bWasDown = bDown;

  if (m_bToggle)
  {
    if (bPressed)
      m_bWanted = !m_bWanted;
  }
  else
  {
    m_bWanted = bDown;
  }

  const bool bSwitchDown = plReadCameraAction(ref_context.m_pInput, m_sSwitchAction) > 0.5f;
  const bool bSwitch = bSwitchDown && !m_bSwitchWasDown;
  m_bSwitchWasDown = bSwitchDown;

  plGameObject* pTarget = nullptr;
  bool bValid = ref_context.m_pWorld->TryGetObject(m_hTarget, pTarget);

  if (bValid && ref_context.m_Subject.m_bValid && (pTarget->GetGlobalPosition() - ref_context.m_Subject.m_vPosition).GetLength() > m_fMaxDistance * 1.2f)
    bValid = false;

  if (m_bWanted && (!bValid || bSwitch))
  {
    const plGameObjectHandle hNew = PickTarget(ref_context, bSwitch ? m_hTarget : plGameObjectHandle());

    if (!hNew.IsInvalidated() || !bValid)
    {
      m_hTarget = hNew;
      bValid = ref_context.m_pWorld->TryGetObject(m_hTarget, pTarget);

      if (bValid)
      {
        plCameraScreenAnchor::Enum playerAnchor, targetAnchor;
        plCameraFramingLayout::GetAnchors(TwoSubjectLayout(m_Layout, plCameraFramingLayout::OverTheShoulder), playerAnchor,
          targetAnchor);
        m_fMirror = DecideMirror(m_Mirror, GetCameraPose(ref_context), ref_context.m_Subject.m_vPosition, pTarget->GetGlobalPosition(),
          plCameraScreenAnchor::GetPoint(playerAnchor), plCameraScreenAnchor::GetPoint(targetAnchor));
      }
    }

    // nothing to lock on to
    if (!bValid)
      m_bWanted = false;
  }

  const bool bActive = m_bWanted && bValid;
  const float fBlendIn = plMath::Max(0.01f, m_BlendIn.AsFloatInSeconds());
  m_fWeight = bActive ? plMath::Min(1.0f, m_fWeight + ref_context.m_fDeltaSeconds / fBlendIn) : plMath::Max(0.0f, m_fWeight - ref_context.m_fDeltaSeconds / fBlendIn);
  m_fDebugAmount = m_fWeight;

  // Look At wins while it frames something
  if (m_fWeight <= 0.0f || !bValid || ref_context.m_Framing.m_bActive)
    return;

  plCameraScreenAnchor::Enum playerAnchor, targetAnchor;
  plCameraFramingLayout::GetAnchors(TwoSubjectLayout(m_Layout, plCameraFramingLayout::OverTheShoulder), playerAnchor, targetAnchor);

  ref_context.m_Framing.m_bActive = true;
  ref_context.m_Framing.m_vTarget = pTarget->GetGlobalPosition() + m_vTargetOffset;
  ref_context.m_Framing.m_vPlayerAnchor = plCameraScreenAnchor::GetPoint(playerAnchor, m_fMirror);
  ref_context.m_Framing.m_vTargetAnchor = plCameraScreenAnchor::GetPoint(targetAnchor, m_fMirror);
  ref_context.m_Framing.m_fWeight = Smooth(m_fWeight);
  ref_context.m_Framing.m_sName = pTarget->GetName();

  if (m_fWeight > 0.5f)
  {
    ref_context.m_bHasFocusPoint = true;
    ref_context.m_vFocusPoint = ref_context.m_Framing.m_vTarget;
  }
}

void plCameraLockOnModifier::GetDebugText(plStringBuilder& out_sText) const
{
  out_sText = m_fWeight > 0.0f ? "locked" : (m_bWanted ? "searching" : "-");
}
