#include <CamerasPlugin/CamerasPCH.h>

#include <CamerasPlugin/Core/CameraModifier.h>
#include <Foundation/IO/Stream.h>
#include <Foundation/Serialization/ReflectionSerializer.h>

// clang-format off
PL_BEGIN_DYNAMIC_REFLECTED_TYPE(plCameraModifier, 1, plRTTINoAllocator)
{
  PL_BEGIN_PROPERTIES
  {
    PL_MEMBER_PROPERTY("Name", m_sName),
    PL_MEMBER_PROPERTY("Enabled", m_bEnabled)->AddAttributes(new plDefaultValueAttribute(true)),
    PL_MEMBER_PROPERTY("Weight", m_fWeight)->AddAttributes(new plDefaultValueAttribute(1.0f), new plClampValueAttribute(0.0f, 1.0f)),
  }
  PL_END_PROPERTIES;
}
PL_END_DYNAMIC_REFLECTED_TYPE;
// clang-format on

plCameraProjection plCameraModifierContext::GetProjection(const plCameraPose& pose) const
{
  return plCameraProjection::Make(pose.m_fFieldOfView, m_fAspectRatio, m_bFovIsHorizontal);
}

plCameraModifier::plCameraModifier()
{
  m_Views = plCameraViewKind::Default;
}

plCameraModifier::~plCameraModifier() = default;

void plCameraModifier::GetDebugText(plStringBuilder& out_sText) const
{
  out_sText = m_fDebugAmount > 0.005f ? "active" : "-";
}

float plCameraModifier::GetEffectiveWeight(const plCameraModifierContext& context) const
{
  if (!m_bEnabled || !m_Views.IsAnySet(context.m_ViewKind))
    return 0.0f;

  if (context.m_bStereo && IsRotationalEffect() && !m_bAllowInXR)
    return 0.0f;

  return plMath::Clamp(m_fWeight, 0.0f, 1.0f);
}

//////////////////////////////////////////////////////////////////////////

void plCameraModifierStack::Prepare(plArrayPtr<plCameraModifier* const> modifiers, plCameraModifierContext& ref_context)
{
  for (plCameraModifier* pModifier : modifiers)
  {
    if (pModifier != nullptr && pModifier->m_bEnabled)
      pModifier->Prepare(ref_context);
  }
}

void plCameraModifierStack::Apply(plArrayPtr<plCameraModifier* const> modifiers, plCameraModifierStage::Enum stage, plCameraModifierContext& ref_context, plCameraPose& ref_pose)
{
  for (plCameraModifier* pModifier : modifiers)
  {
    if (pModifier == nullptr || pModifier->GetStage() != stage)
      continue;

    const float fWeight = pModifier->GetEffectiveWeight(ref_context);

    // disabled modifiers still run with weight zero, so their state settles instead of resuming mid-effect
    pModifier->Apply(ref_context, ref_pose, fWeight);
  }
}

void plCameraModifierStack::SendImpulse(plArrayPtr<plCameraModifier* const> modifiers, const plMsgCameraImpulse& msg)
{
  for (plCameraModifier* pModifier : modifiers)
  {
    if (pModifier != nullptr && pModifier->m_bEnabled)
      pModifier->OnImpulse(msg);
  }
}

void plCameraModifierStack::Reset(plArrayPtr<plCameraModifier* const> modifiers)
{
  for (plCameraModifier* pModifier : modifiers)
  {
    if (pModifier != nullptr)
      pModifier->Reset();
  }
}

plCameraModifier* plCameraModifierStack::FindByName(plArrayPtr<plCameraModifier* const> modifiers, plStringView sName)
{
  for (plCameraModifier* pModifier : modifiers)
  {
    if (pModifier != nullptr && pModifier->m_sName == sName)
      return pModifier;
  }

  return nullptr;
}

void plCameraModifierStack::Write(plStreamWriter& inout_stream, plArrayPtr<plCameraModifier* const> modifiers)
{
  plUInt32 uiCount = 0;
  for (plCameraModifier* pModifier : modifiers)
  {
    if (pModifier != nullptr)
      ++uiCount;
  }

  inout_stream << uiCount;

  for (plCameraModifier* pModifier : modifiers)
  {
    if (pModifier != nullptr)
      plReflectionSerializer::WriteObjectToBinary(inout_stream, pModifier->GetDynamicRTTI(), pModifier);
  }
}

void plCameraModifierStack::Read(plStreamReader& inout_stream, plDynamicArray<plCameraModifier*>& ref_modifiers)
{
  DeleteAll(ref_modifiers);

  plUInt32 uiCount = 0;
  inout_stream >> uiCount;

  for (plUInt32 i = 0; i < uiCount; ++i)
  {
    const plRTTI* pType = nullptr;
    void* pObject = plReflectionSerializer::ReadObjectFromBinary(inout_stream, pType);

    if (pObject == nullptr)
      continue;

    if (pType == nullptr || !pType->IsDerivedFrom<plCameraModifier>())
    {
      // a type from a newer or missing plugin: drop it rather than mistake it for a modifier
      if (pType != nullptr)
        pType->GetAllocator()->Deallocate(pObject);

      continue;
    }

    ref_modifiers.PushBack(static_cast<plCameraModifier*>(pObject));
  }
}

void plCameraModifierStack::DeleteAll(plDynamicArray<plCameraModifier*>& ref_modifiers)
{
  for (plCameraModifier* pModifier : ref_modifiers)
  {
    if (pModifier != nullptr)
      pModifier->GetDynamicRTTI()->GetAllocator()->Deallocate(pModifier);
  }

  ref_modifiers.Clear();
}
