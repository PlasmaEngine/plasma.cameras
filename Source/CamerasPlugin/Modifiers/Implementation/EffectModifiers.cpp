#include <CamerasPlugin/CamerasPCH.h>

#include <CamerasPlugin/Core/CameraMessages.h>
#include <CamerasPlugin/Modifiers/EffectModifiers.h>
#include <CamerasPlugin/Volumes/CameraShakeVolumeComponent.h>
#include <Core/Interfaces/PhysicsWorldModule.h>
#include <Core/World/World.h>

namespace
{
  plVariant ViewsDefault(plBitflags<plCameraViewKind> views)
  {
    return plVariant(static_cast<plInt64>(views.GetValue()));
  }

  /// Fraction to move toward a target this frame, for an exponential approach with the given time constant.
  float Follow(float fDeltaSeconds, plTime timeConstant)
  {
    if (!timeConstant.IsPositive())
      return 1.0f;

    return 1.0f - plMath::Exp(-fDeltaSeconds / timeConstant.AsFloatInSeconds());
  }

  /// Smooth noise in roughly -1..1: three incommensurate sines, phase-shifted per channel.
  float SmoothNoise(float fTime, float fSeed)
  {
    return 0.5f * plMath::Sin(plAngle::MakeFromRadian(fTime * 1.7f + fSeed)) + 0.3f * plMath::Sin(plAngle::MakeFromRadian(fTime * 3.1f + fSeed * 2.3f)) +
           0.2f * plMath::Sin(plAngle::MakeFromRadian(fTime * 5.9f + fSeed * 0.7f));
  }

  /// Rotates a pose around its own axes.
  void RotateLocal(plCameraPose& ref_pose, plAngle yaw, plAngle pitch, plAngle roll)
  {
    ref_pose.m_qRotation = ref_pose.m_qRotation * plCameraPose::MakeRotation(yaw, pitch, roll);
  }
} // namespace

// clang-format off
PL_BEGIN_DYNAMIC_REFLECTED_TYPE(plCameraShakeModifier, 1, plRTTIDefaultAllocator<plCameraShakeModifier>)
{
  PL_BEGIN_PROPERTIES
  {
    PL_BITFLAGS_ACCESSOR_PROPERTY("Views", plCameraViewKind, GetViewsProperty, SetViewsProperty)->AddAttributes(new plDefaultValueAttribute(ViewsDefault(plCameraViewKind::Default))),
    PL_ACCESSOR_PROPERTY("AllowInXR", GetAllowInXRProperty, SetAllowInXRProperty),
    PL_MEMBER_PROPERTY("MaxYaw", m_MaxYaw)->AddAttributes(new plDefaultValueAttribute(plAngle::MakeFromDegree(4.0f))),
    PL_MEMBER_PROPERTY("MaxPitch", m_MaxPitch)->AddAttributes(new plDefaultValueAttribute(plAngle::MakeFromDegree(4.0f))),
    PL_MEMBER_PROPERTY("MaxRoll", m_MaxRoll)->AddAttributes(new plDefaultValueAttribute(plAngle::MakeFromDegree(6.0f))),
    PL_MEMBER_PROPERTY("MaxOffset", m_fMaxOffset)->AddAttributes(new plClampValueAttribute(0.0f, 2.0f)),
    PL_MEMBER_PROPERTY("Frequency", m_fFrequency)->AddAttributes(new plDefaultValueAttribute(9.0f), new plClampValueAttribute(0.1f, 60.0f)),
    PL_MEMBER_PROPERTY("TraumaDecay", m_fTraumaDecay)->AddAttributes(new plDefaultValueAttribute(0.9f), new plClampValueAttribute(0.0f, 10.0f)),
    PL_MEMBER_PROPERTY("UseShakeVolumes", m_bUseShakeVolumes)->AddAttributes(new plDefaultValueAttribute(true)),
  }
  PL_END_PROPERTIES;
}
PL_END_DYNAMIC_REFLECTED_TYPE;

PL_BEGIN_DYNAMIC_REFLECTED_TYPE(plCameraFovKickModifier, 1, plRTTIDefaultAllocator<plCameraFovKickModifier>)
{
  PL_BEGIN_PROPERTIES
  {
    PL_BITFLAGS_ACCESSOR_PROPERTY("Views", plCameraViewKind, GetViewsProperty, SetViewsProperty)->AddAttributes(new plDefaultValueAttribute(ViewsDefault(plCameraViewKind::FirstPerson | plCameraViewKind::ThirdPerson))),
    PL_MEMBER_PROPERTY("SpeedThreshold", m_fSpeedThreshold)->AddAttributes(new plDefaultValueAttribute(4.5f), new plClampValueAttribute(0.0f, 100.0f)),
    PL_MEMBER_PROPERTY("FullSpeed", m_fFullSpeed)->AddAttributes(new plDefaultValueAttribute(7.0f), new plClampValueAttribute(0.0f, 100.0f)),
    PL_MEMBER_PROPERTY("MaxKick", m_fMaxKick)->AddAttributes(new plDefaultValueAttribute(10.0f), new plClampValueAttribute(-60.0f, 60.0f)),
    PL_MEMBER_PROPERTY("InTime", m_InTime)->AddAttributes(new plDefaultValueAttribute(plTime::MakeFromSeconds(0.25))),
    PL_MEMBER_PROPERTY("OutTime", m_OutTime)->AddAttributes(new plDefaultValueAttribute(plTime::MakeFromSeconds(0.4))),
  }
  PL_END_PROPERTIES;
}
PL_END_DYNAMIC_REFLECTED_TYPE;

PL_BEGIN_DYNAMIC_REFLECTED_TYPE(plCameraZoomModifier, 1, plRTTIDefaultAllocator<plCameraZoomModifier>)
{
  PL_BEGIN_PROPERTIES
  {
    PL_BITFLAGS_ACCESSOR_PROPERTY("Views", plCameraViewKind, GetViewsProperty, SetViewsProperty)->AddAttributes(new plDefaultValueAttribute(ViewsDefault(plCameraViewKind::Default))),
    PL_MEMBER_PROPERTY("Action", m_sAction)->AddAttributes(new plDefaultValueAttribute("Aim")),
    PL_MEMBER_PROPERTY("FovScale", m_fFovScale)->AddAttributes(new plDefaultValueAttribute(0.6f), new plClampValueAttribute(0.05f, 2.0f)),
    PL_MEMBER_PROPERTY("ArmPull", m_fArmPull)->AddAttributes(new plDefaultValueAttribute(0.4f), new plClampValueAttribute(0.0f, 0.95f)),
    PL_MEMBER_PROPERTY("LookScale", m_fLookScale)->AddAttributes(new plDefaultValueAttribute(0.5f), new plClampValueAttribute(0.0f, 2.0f)),
    PL_MEMBER_PROPERTY("BlendTime", m_BlendTime)->AddAttributes(new plDefaultValueAttribute(plTime::MakeFromSeconds(0.15))),
  }
  PL_END_PROPERTIES;
}
PL_END_DYNAMIC_REFLECTED_TYPE;

PL_BEGIN_DYNAMIC_REFLECTED_TYPE(plCameraRecoilModifier, 1, plRTTIDefaultAllocator<plCameraRecoilModifier>)
{
  PL_BEGIN_PROPERTIES
  {
    PL_BITFLAGS_ACCESSOR_PROPERTY("Views", plCameraViewKind, GetViewsProperty, SetViewsProperty)->AddAttributes(new plDefaultValueAttribute(ViewsDefault(plCameraViewKind::FirstPerson | plCameraViewKind::ThirdPerson))),
    PL_ACCESSOR_PROPERTY("AllowInXR", GetAllowInXRProperty, SetAllowInXRProperty),
    PL_MEMBER_PROPERTY("Stiffness", m_fStiffness)->AddAttributes(new plDefaultValueAttribute(140.0f), new plClampValueAttribute(1.0f, 2000.0f)),
    PL_MEMBER_PROPERTY("Damping", m_fDamping)->AddAttributes(new plDefaultValueAttribute(18.0f), new plClampValueAttribute(0.0f, 200.0f)),
    PL_MEMBER_PROPERTY("RandomYaw", m_RandomYaw)->AddAttributes(new plDefaultValueAttribute(plAngle::MakeFromDegree(1.2f))),
  }
  PL_END_PROPERTIES;
}
PL_END_DYNAMIC_REFLECTED_TYPE;

PL_BEGIN_DYNAMIC_REFLECTED_TYPE(plCameraHeadBobModifier, 1, plRTTIDefaultAllocator<plCameraHeadBobModifier>)
{
  PL_BEGIN_PROPERTIES
  {
    PL_BITFLAGS_ACCESSOR_PROPERTY("Views", plCameraViewKind, GetViewsProperty, SetViewsProperty)->AddAttributes(new plDefaultValueAttribute(ViewsDefault(plCameraViewKind::FirstPerson))),
    PL_ACCESSOR_PROPERTY("AllowInXR", GetAllowInXRProperty, SetAllowInXRProperty),
    PL_MEMBER_PROPERTY("VerticalAmplitude", m_fVerticalAmplitude)->AddAttributes(new plDefaultValueAttribute(0.04f), new plClampValueAttribute(0.0f, 0.5f)),
    PL_MEMBER_PROPERTY("LateralAmplitude", m_fLateralAmplitude)->AddAttributes(new plDefaultValueAttribute(0.03f), new plClampValueAttribute(0.0f, 0.5f)),
    PL_MEMBER_PROPERTY("StepsPerMeter", m_fStepsPerMeter)->AddAttributes(new plDefaultValueAttribute(0.6f), new plClampValueAttribute(0.05f, 5.0f)),
    PL_MEMBER_PROPERTY("PitchNod", m_PitchNod)->AddAttributes(new plDefaultValueAttribute(plAngle::MakeFromDegree(0.4f))),
    PL_MEMBER_PROPERTY("FullSpeed", m_fFullSpeed)->AddAttributes(new plDefaultValueAttribute(5.0f), new plClampValueAttribute(0.1f, 100.0f)),
  }
  PL_END_PROPERTIES;
}
PL_END_DYNAMIC_REFLECTED_TYPE;

PL_BEGIN_DYNAMIC_REFLECTED_TYPE(plCameraLeanModifier, 1, plRTTIDefaultAllocator<plCameraLeanModifier>)
{
  PL_BEGIN_PROPERTIES
  {
    PL_BITFLAGS_ACCESSOR_PROPERTY("Views", plCameraViewKind, GetViewsProperty, SetViewsProperty)->AddAttributes(new plDefaultValueAttribute(ViewsDefault(plCameraViewKind::FirstPerson))),
    PL_ACCESSOR_PROPERTY("AllowInXR", GetAllowInXRProperty, SetAllowInXRProperty),
    PL_MEMBER_PROPERTY("LeanLeftAction", m_sLeanLeftAction)->AddAttributes(new plDefaultValueAttribute("LeanLeft")),
    PL_MEMBER_PROPERTY("LeanRightAction", m_sLeanRightAction)->AddAttributes(new plDefaultValueAttribute("LeanRight")),
    PL_MEMBER_PROPERTY("MaxRoll", m_MaxRoll)->AddAttributes(new plDefaultValueAttribute(plAngle::MakeFromDegree(12.0f))),
    PL_MEMBER_PROPERTY("SideOffset", m_fSideOffset)->AddAttributes(new plDefaultValueAttribute(0.35f), new plClampValueAttribute(0.0f, 2.0f)),
    PL_MEMBER_PROPERTY("BlendTime", m_BlendTime)->AddAttributes(new plDefaultValueAttribute(plTime::MakeFromSeconds(0.15))),
    PL_MEMBER_PROPERTY("DutchAngle", m_DutchAngle),
    PL_MEMBER_PROPERTY("CollisionLayer", m_uiCollisionLayer)->AddAttributes(new plDynamicEnumAttribute("PhysicsCollisionLayer")),
  }
  PL_END_PROPERTIES;
}
PL_END_DYNAMIC_REFLECTED_TYPE;

PL_BEGIN_DYNAMIC_REFLECTED_TYPE(plCameraLandingModifier, 1, plRTTIDefaultAllocator<plCameraLandingModifier>)
{
  PL_BEGIN_PROPERTIES
  {
    PL_BITFLAGS_ACCESSOR_PROPERTY("Views", plCameraViewKind, GetViewsProperty, SetViewsProperty)->AddAttributes(new plDefaultValueAttribute(ViewsDefault(plCameraViewKind::FirstPerson | plCameraViewKind::ThirdPerson))),
    PL_ACCESSOR_PROPERTY("AllowInXR", GetAllowInXRProperty, SetAllowInXRProperty),
    PL_MEMBER_PROPERTY("MinSpeed", m_fMinSpeed)->AddAttributes(new plDefaultValueAttribute(3.0f), new plClampValueAttribute(0.0f, 100.0f)),
    PL_MEMBER_PROPERTY("DipPerSpeed", m_fDipPerSpeed)->AddAttributes(new plDefaultValueAttribute(0.03f), new plClampValueAttribute(0.0f, 1.0f)),
    PL_MEMBER_PROPERTY("MaxDip", m_fMaxDip)->AddAttributes(new plDefaultValueAttribute(0.35f), new plClampValueAttribute(0.0f, 2.0f)),
    PL_MEMBER_PROPERTY("PitchPerMeter", m_PitchPerMeter)->AddAttributes(new plDefaultValueAttribute(plAngle::MakeFromDegree(12.0f))),
    PL_MEMBER_PROPERTY("Stiffness", m_fStiffness)->AddAttributes(new plDefaultValueAttribute(120.0f), new plClampValueAttribute(1.0f, 2000.0f)),
    PL_MEMBER_PROPERTY("Damping", m_fDamping)->AddAttributes(new plDefaultValueAttribute(14.0f), new plClampValueAttribute(0.0f, 200.0f)),
  }
  PL_END_PROPERTIES;
}
PL_END_DYNAMIC_REFLECTED_TYPE;

PL_BEGIN_DYNAMIC_REFLECTED_TYPE(plCameraDampingModifier, 1, plRTTIDefaultAllocator<plCameraDampingModifier>)
{
  PL_BEGIN_PROPERTIES
  {
    PL_BITFLAGS_ACCESSOR_PROPERTY("Views", plCameraViewKind, GetViewsProperty, SetViewsProperty)->AddAttributes(new plDefaultValueAttribute(ViewsDefault(plCameraViewKind::Default))),
    PL_MEMBER_PROPERTY("PositionHalfLife", m_PositionHalfLife)->AddAttributes(new plDefaultValueAttribute(plTime::MakeFromSeconds(0.08))),
    PL_MEMBER_PROPERTY("RotationHalfLife", m_RotationHalfLife)->AddAttributes(new plDefaultValueAttribute(plTime::MakeFromSeconds(0.05))),
  }
  PL_END_PROPERTIES;
}
PL_END_DYNAMIC_REFLECTED_TYPE;
// clang-format on

//////////////////////////////////////////////////////////////////////////

plCameraShakeModifier::plCameraShakeModifier() = default;

void plCameraShakeModifier::Reset()
{
  m_fTrauma = 0.0f;
  m_fVolumeTrauma = 0.0f;
}

void plCameraShakeModifier::OnImpulse(const plMsgCameraImpulse& msg)
{
  if (msg.m_Type != plCameraImpulseType::Shake)
    return;

  float fStrength = msg.m_fStrength;

  if (msg.m_fRadius > 0.0f)
  {
    const float fDistance = (m_vLastPosition - msg.m_vPosition).GetLength();
    fStrength *= plMath::Max(0.0f, 1.0f - fDistance / msg.m_fRadius);
  }

  m_fTrauma = plMath::Clamp(m_fTrauma + fStrength, 0.0f, 1.0f);
}

void plCameraShakeModifier::Apply(plCameraModifierContext& ref_context, plCameraPose& ref_pose, float fWeight)
{
  m_vLastPosition = ref_pose.m_vPosition;
  m_fTime += ref_context.m_fDeltaSeconds;
  m_fTrauma = plMath::Max(0.0f, m_fTrauma - m_fTraumaDecay * ref_context.m_fDeltaSeconds);
  m_fVolumeTrauma = m_bUseShakeVolumes ? plCameraShakeVolumeComponent::SampleStrength(ref_context.m_pWorld, ref_pose.m_vPosition) : 0.0f;

  const float fTrauma = plMath::Min(1.0f, plMath::Max(m_fTrauma, m_fVolumeTrauma));
  const float k = fTrauma * fTrauma * fWeight;
  m_fDebugAmount = k;

  if (k <= 0.0f)
    return;

  const float t = m_fTime * m_fFrequency;
  RotateLocal(ref_pose, m_MaxYaw * (SmoothNoise(t, 1.0f) * k), m_MaxPitch * (SmoothNoise(t, 7.0f) * k), m_MaxRoll * (SmoothNoise(t, 13.0f) * k));

  if (m_fMaxOffset > 0.0f)
  {
    ref_pose.m_vPosition += (ref_pose.GetLeft() * SmoothNoise(t, 19.0f) + ref_pose.GetUp() * SmoothNoise(t, 23.0f)) * (m_fMaxOffset * k);
  }
}

void plCameraShakeModifier::GetDebugText(plStringBuilder& out_sText) const
{
  out_sText.SetFormat("trauma {}{}", plArgF(plMath::Max(m_fTrauma, m_fVolumeTrauma), 2), m_fVolumeTrauma > m_fTrauma ? " (volume)" : "");
}

//////////////////////////////////////////////////////////////////////////

plCameraFovKickModifier::plCameraFovKickModifier()
{
  m_Views = plCameraViewKind::FirstPerson | plCameraViewKind::ThirdPerson;
}

void plCameraFovKickModifier::Apply(plCameraModifierContext& ref_context, plCameraPose& ref_pose, float fWeight)
{
  const float fSpeed = ref_context.m_Subject.m_vVelocity.GetAsVec2().GetLength();
  const float fRange = plMath::Max(0.01f, m_fFullSpeed - m_fSpeedThreshold);
  const float fTarget = plMath::Clamp((fSpeed - m_fSpeedThreshold) / fRange, 0.0f, 1.0f) * m_fMaxKick * fWeight;

  m_fCurrent = plMath::Lerp(m_fCurrent, fTarget, Follow(ref_context.m_fDeltaSeconds, fTarget > m_fCurrent ? m_InTime : m_OutTime));
  m_fDebugAmount = m_fMaxKick != 0.0f ? plMath::Abs(m_fCurrent / m_fMaxKick) : 0.0f;

  ref_pose.m_fFieldOfView += m_fCurrent;
}

void plCameraFovKickModifier::GetDebugText(plStringBuilder& out_sText) const
{
  out_sText.SetFormat("+{} deg", plArgF(m_fCurrent, 1));
}

//////////////////////////////////////////////////////////////////////////

plCameraZoomModifier::plCameraZoomModifier() = default;

void plCameraZoomModifier::Apply(plCameraModifierContext& ref_context, plCameraPose& ref_pose, float fWeight)
{
  const bool bHeld = m_bScriptZoom || plReadCameraAction(ref_context.m_pInput, m_sAction) > 0.5f;
  m_fAmount = plMath::Lerp(m_fAmount, bHeld ? 1.0f : 0.0f, Follow(ref_context.m_fDeltaSeconds, m_BlendTime));

  const float w = m_fAmount * fWeight;
  m_fDebugAmount = w;

  if (w <= 0.0001f)
    return;

  ref_pose.m_fFieldOfView *= plMath::Lerp(1.0f, m_fFovScale, w);
  ref_context.m_fLookScale *= plMath::Lerp(1.0f, m_fLookScale, w);

  if (ref_context.m_ViewKind.IsSet(plCameraViewKind::ThirdPerson) && ref_context.m_Subject.m_bValid)
  {
    ref_pose.m_vPosition = plMath::Lerp(ref_pose.m_vPosition, ref_context.m_Subject.m_vPosition, m_fArmPull * w);
  }
}

void plCameraZoomModifier::GetDebugText(plStringBuilder& out_sText) const
{
  out_sText.SetFormat("{}", plArgF(m_fAmount, 2));
}

//////////////////////////////////////////////////////////////////////////

plCameraRecoilModifier::plCameraRecoilModifier()
{
  m_Views = plCameraViewKind::FirstPerson | plCameraViewKind::ThirdPerson;
}

void plCameraRecoilModifier::Reset()
{
  m_fPitch = m_fPitchVelocity = m_fYaw = m_fYawVelocity = 0.0f;
}

void plCameraRecoilModifier::OnImpulse(const plMsgCameraImpulse& msg)
{
  if (msg.m_Type != plCameraImpulseType::Recoil)
    return;

  // a cheap deterministic random in -1..1 for the sideways part
  m_uiSeed = m_uiSeed * 1664525u + 1013904223u;
  const float fRandom = static_cast<float>(m_uiSeed >> 8) / static_cast<float>(1u << 24) * 2.0f - 1.0f;

  // A velocity kick whose spring response peaks at roughly the requested angle. The factor is measured for the
  // default stiffness and damping (CamerasTest checks it); a much stiffer or softer spring peaks lower or higher.
  const float fKickScale = plMath::Sqrt(m_fStiffness) * 2.4f;
  m_fPitchVelocity += msg.m_Pitch.GetRadian() * fKickScale;
  m_fYawVelocity += (msg.m_Yaw.GetRadian() + m_RandomYaw.GetRadian() * fRandom) * fKickScale;
}

void plCameraRecoilModifier::Apply(plCameraModifierContext& ref_context, plCameraPose& ref_pose, float fWeight)
{
  // semi-implicit Euler, sub-stepped so stiff springs stay stable at low frame rates
  const float fDt = ref_context.m_fDeltaSeconds;
  const plUInt32 uiSteps = plMath::Clamp(static_cast<plUInt32>(fDt / 0.004f) + 1, 1u, 16u);
  const float h = fDt / uiSteps;

  for (plUInt32 i = 0; i < uiSteps; ++i)
  {
    m_fPitchVelocity += (-m_fPitch * m_fStiffness - m_fPitchVelocity * m_fDamping) * h;
    m_fPitch += m_fPitchVelocity * h;
    m_fYawVelocity += (-m_fYaw * m_fStiffness - m_fYawVelocity * m_fDamping) * h;
    m_fYaw += m_fYawVelocity * h;
  }

  m_fDebugAmount = plMath::Min(1.0f, plMath::Abs(m_fPitch) / plAngle::MakeFromDegree(4.0f).GetRadian()) * fWeight;

  RotateLocal(ref_pose, plAngle::MakeFromRadian(m_fYaw * fWeight), plAngle::MakeFromRadian(m_fPitch * fWeight), plAngle());
}

void plCameraRecoilModifier::GetDebugText(plStringBuilder& out_sText) const
{
  out_sText.SetFormat("{} deg pitch", plArgF(plAngle::MakeFromRadian(m_fPitch).GetDegree(), 1));
}

//////////////////////////////////////////////////////////////////////////

plCameraHeadBobModifier::plCameraHeadBobModifier()
{
  m_Views = plCameraViewKind::FirstPerson;
}

void plCameraHeadBobModifier::Reset()
{
  m_fPhase = 0.0f;
  m_fAmount = 0.0f;
}

void plCameraHeadBobModifier::Apply(plCameraModifierContext& ref_context, plCameraPose& ref_pose, float fWeight)
{
  const plCameraSubjectInfo& subject = ref_context.m_Subject;
  const float fSpeed = subject.m_vVelocity.GetAsVec2().GetLength();
  const float fTarget = subject.m_bInAir ? 0.0f : plMath::Clamp(fSpeed / m_fFullSpeed, 0.0f, 1.0f);

  m_fAmount = plMath::Lerp(m_fAmount, fTarget, plMath::Min(1.0f, ref_context.m_fDeltaSeconds * 8.0f));
  m_fPhase += ref_context.m_fDeltaSeconds * fSpeed * m_fStepsPerMeter * plMath::Pi<float>();

  const float a = m_fAmount * fWeight;
  m_fDebugAmount = a;

  if (a <= 0.0001f)
    return;

  // one step per half period: the height dips at every footfall, the sideways sway completes over two steps
  const float fStep = plMath::Sin(plAngle::MakeFromRadian(m_fPhase));
  const float fSway = plMath::Sin(plAngle::MakeFromRadian(m_fPhase * 0.5f));

  ref_pose.m_vPosition += plVec3(0, 0, (plMath::Abs(fStep) - 0.5f) * m_fVerticalAmplitude * a);
  ref_pose.m_vPosition -= ref_pose.GetLeft() * (fSway * m_fLateralAmplitude * a);
  RotateLocal(ref_pose, plAngle(), m_PitchNod * (-plMath::Abs(fStep) * a), plAngle());
}

void plCameraHeadBobModifier::GetDebugText(plStringBuilder& out_sText) const
{
  out_sText.SetFormat("{}", plArgF(m_fAmount, 2));
}

//////////////////////////////////////////////////////////////////////////

plCameraLeanModifier::plCameraLeanModifier()
{
  m_Views = plCameraViewKind::FirstPerson;
}

void plCameraLeanModifier::Apply(plCameraModifierContext& ref_context, plCameraPose& ref_pose, float fWeight)
{
  const float fInput = plReadCameraAction(ref_context.m_pInput, m_sLeanRightAction) - plReadCameraAction(ref_context.m_pInput, m_sLeanLeftAction);
  const float fTarget = plMath::Clamp(fInput + m_fScriptLean, -1.0f, 1.0f);

  m_fLean = plMath::Lerp(m_fLean, fTarget, Follow(ref_context.m_fDeltaSeconds, m_BlendTime));

  const float fLean = m_fLean * fWeight;
  m_fDebugAmount = plMath::Abs(fLean);

  // roll right leans the head right; never lean into a wall
  if (plMath::Abs(fLean) > 0.0001f && m_fSideOffset > 0.0f)
  {
    plVec3 vSide = -ref_pose.GetLeft();
    vSide.z = 0.0f;
    vSide.NormalizeIfNotZero(plVec3::MakeZero()).IgnoreResult();
    const plVec3 vDir = vSide * plMath::Sign(fLean);

    float fDistance = plMath::Abs(fLean) * m_fSideOffset;

    if (const plPhysicsWorldModuleInterface* pPhysics = ref_context.m_pWorld->GetModuleReadOnly<plPhysicsWorldModuleInterface>())
    {
      plPhysicsQueryParameters params;
      params.m_uiCollisionLayer = m_uiCollisionLayer;
      params.m_ShapeTypes = plPhysicsShapeType::Static | plPhysicsShapeType::Dynamic;

      plPhysicsCastResult result;
      if (pPhysics->SweepTestSphere(result, 0.15f, ref_pose.m_vPosition, vDir, fDistance, params))
        fDistance = plMath::Max(0.0f, result.m_fDistance);
    }

    ref_pose.m_vPosition += vDir * fDistance;
  }

  RotateLocal(ref_pose, plAngle(), plAngle(), m_MaxRoll * fLean + m_DutchAngle * fWeight);
}

void plCameraLeanModifier::GetDebugText(plStringBuilder& out_sText) const
{
  out_sText.SetFormat("{} deg roll", plArgF((m_MaxRoll * m_fLean + m_DutchAngle).GetDegree(), 1));
}

//////////////////////////////////////////////////////////////////////////

plCameraLandingModifier::plCameraLandingModifier()
{
  m_Views = plCameraViewKind::FirstPerson | plCameraViewKind::ThirdPerson;
}

void plCameraLandingModifier::Reset()
{
  m_fOffset = 0.0f;
  m_fVelocity = 0.0f;
}

void plCameraLandingModifier::Kick(float fFallSpeed)
{
  const float fSpeed = plMath::Abs(fFallSpeed);
  if (fSpeed < m_fMinSpeed)
    return;

  const float fDip = plMath::Min(m_fMaxDip, (fSpeed - m_fMinSpeed) * m_fDipPerSpeed + 0.02f);

  // a downward velocity whose spring response bottoms out near the wanted dip (same factor as Recoil's kick)
  m_fVelocity -= fDip * plMath::Sqrt(m_fStiffness) * 2.4f;
}

void plCameraLandingModifier::OnImpulse(const plMsgCameraImpulse& msg)
{
  if (msg.m_Type == plCameraImpulseType::Landing)
    Kick(msg.m_fStrength);
}

void plCameraLandingModifier::Apply(plCameraModifierContext& ref_context, plCameraPose& ref_pose, float fWeight)
{
  if (ref_context.m_Subject.m_fLandingSpeed < 0.0f)
    Kick(ref_context.m_Subject.m_fLandingSpeed);

  const float fDt = ref_context.m_fDeltaSeconds;
  const plUInt32 uiSteps = plMath::Clamp(static_cast<plUInt32>(fDt / 0.004f) + 1, 1u, 16u);
  const float h = fDt / uiSteps;

  for (plUInt32 i = 0; i < uiSteps; ++i)
  {
    m_fVelocity += (-m_fOffset * m_fStiffness - m_fVelocity * m_fDamping) * h;
    m_fOffset += m_fVelocity * h;
  }

  m_fDebugAmount = plMath::Min(1.0f, plMath::Abs(m_fOffset) / plMath::Max(0.01f, m_fMaxDip)) * fWeight;

  ref_pose.m_vPosition.z += m_fOffset * fWeight;
  RotateLocal(ref_pose, plAngle(), m_PitchPerMeter * (m_fOffset * fWeight), plAngle());
}

void plCameraLandingModifier::GetDebugText(plStringBuilder& out_sText) const
{
  out_sText.SetFormat("{} m", plArgF(m_fOffset, 2));
}

//////////////////////////////////////////////////////////////////////////

plCameraDampingModifier::plCameraDampingModifier() = default;

void plCameraDampingModifier::Apply(plCameraModifierContext& ref_context, plCameraPose& ref_pose, float fWeight)
{
  // a jump this far is a cut or a teleport: follow it instead of sweeping across the level
  if (!m_bHasState || (ref_pose.m_vPosition - m_vPosition).GetLengthSquared() > 25.0f * 25.0f)
  {
    m_vPosition = ref_pose.m_vPosition;
    m_qRotation = ref_pose.m_qRotation;
    m_bHasState = true;
  }

  const float fDt = ref_context.m_fDeltaSeconds;
  const float fPos = m_PositionHalfLife.IsPositive() ? 1.0f - plMath::Pow(0.5f, fDt / m_PositionHalfLife.AsFloatInSeconds()) : 1.0f;
  const float fRot = m_RotationHalfLife.IsPositive() ? 1.0f - plMath::Pow(0.5f, fDt / m_RotationHalfLife.AsFloatInSeconds()) : 1.0f;

  m_vPosition = plMath::Lerp(m_vPosition, ref_pose.m_vPosition, fPos);
  m_qRotation = plQuat::MakeSlerp(m_qRotation, ref_pose.m_qRotation, fRot);

  m_fDebugAmount = fWeight;

  ref_pose.m_vPosition = plMath::Lerp(ref_pose.m_vPosition, m_vPosition, fWeight);
  ref_pose.m_qRotation = plQuat::MakeSlerp(ref_pose.m_qRotation, m_qRotation, fWeight);
}

void plCameraDampingModifier::GetDebugText(plStringBuilder& out_sText) const
{
  out_sText = m_bHasState ? "on" : "-";
}
