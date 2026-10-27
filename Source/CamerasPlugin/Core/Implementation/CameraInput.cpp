#include <CamerasPlugin/CamerasPCH.h>

#include <CamerasPlugin/Core/CameraInput.h>
#include <Core/World/World.h>
#include <GameEngine/Configuration/InputBindings.h>
#include <GameEngine/GameApplication/GameApplication.h>
#include <GameEngine/Gameplay/InputComponent.h>

// clang-format off
PL_BEGIN_STATIC_REFLECTED_TYPE(plCameraLookInput, plNoBase, 1, plRTTIDefaultAllocator<plCameraLookInput>)
{
  PL_BEGIN_PROPERTIES
  {
    PL_MEMBER_PROPERTY("LookAction", m_sLookAction),
    PL_MEMBER_PROPERTY("TurnLeftAction", m_sTurnLeftAction),
    PL_MEMBER_PROPERTY("TurnRightAction", m_sTurnRightAction),
    PL_MEMBER_PROPERTY("LookUpAction", m_sLookUpAction),
    PL_MEMBER_PROPERTY("LookDownAction", m_sLookDownAction),
    PL_MEMBER_PROPERTY("Sensitivity", m_fSensitivity)->AddAttributes(new plDefaultValueAttribute(1.0f), new plClampValueAttribute(0.0f, 100.0f)),
    PL_MEMBER_PROPERTY("InvertY", m_bInvertY),
  }
  PL_END_PROPERTIES;
}
PL_END_STATIC_REFLECTED_TYPE;
// clang-format on

bool plCameraLookInput::HasAnyAction() const
{
  return !m_sLookAction.IsEmpty() || !m_sTurnLeftAction.IsEmpty() || !m_sTurnRightAction.IsEmpty() || !m_sLookUpAction.IsEmpty() ||
         !m_sLookDownAction.IsEmpty();
}

plVec2 plCameraLookInput::Read(const plInputComponent* pInput, float fScale) const
{
  if (pInput == nullptr)
    return plVec2::MakeZero();

  plVec2 vDelta = plVec2::MakeZero();

  if (!m_sLookAction.IsEmpty())
  {
    const plVec3 vLook = pInput->GetActionVector(m_sLookAction);

    // the vector action turns right with +x
    vDelta.x -= vLook.x;
    vDelta.y += vLook.y;
  }

  vDelta.x += plReadCameraAction(pInput, m_sTurnLeftAction) - plReadCameraAction(pInput, m_sTurnRightAction);
  vDelta.y += plReadCameraAction(pInput, m_sLookUpAction) - plReadCameraAction(pInput, m_sLookDownAction);

  if (m_bInvertY)
    vDelta.y = -vDelta.y;

  return vDelta * (m_fSensitivity * fScale);
}

//////////////////////////////////////////////////////////////////////////

namespace
{
  plInputBindings* GetBindings()
  {
    plGameApplication* pApp = plGameApplication::GetGameApplicationInstance();
    return pApp != nullptr ? &pApp->GetInputBindings() : nullptr;
  }
} // namespace

plCameraPlayerSettings plCameraPlayerSettings::Get()
{
  plCameraPlayerSettings settings;

  const plInputBindings* pBindings = GetBindings();
  if (pBindings == nullptr)
    return settings;

  settings.m_fLookSensitivity = plMath::Max(0.0f, pBindings->GetValue("Camera.LookSensitivity", 1.0f));
  settings.m_bInvertY = pBindings->GetValue("Camera.InvertY", 0.0f) > 0.5f;
  settings.m_fFieldOfViewOffset = pBindings->GetValue("Camera.FieldOfViewOffset", 0.0f);

  const bool bMouse = pBindings->GetActiveScheme() == plInputScheme::KeyboardMouse;
  settings.m_fAssist = plMath::Clamp(bMouse ? pBindings->GetValue("Camera.MouseAssist", 0.0f) : pBindings->GetValue("Camera.GamepadAssist", 1.0f), 0.0f, 1.0f);

  return settings;
}

void plCameraPlayerSettings::SetLookSensitivity(float fValue)
{
  if (plInputBindings* pBindings = GetBindings())
    pBindings->SetValue("Camera.LookSensitivity", fValue);
}

void plCameraPlayerSettings::SetInvertY(bool bInvert)
{
  if (plInputBindings* pBindings = GetBindings())
    pBindings->SetValue("Camera.InvertY", bInvert ? 1.0f : 0.0f);
}

void plCameraPlayerSettings::SetFieldOfViewOffset(float fDegrees)
{
  if (plInputBindings* pBindings = GetBindings())
    pBindings->SetValue("Camera.FieldOfViewOffset", fDegrees);
}

//////////////////////////////////////////////////////////////////////////

namespace
{
  const plInputComponent* FindInputUpwards(const plGameObject* pObject)
  {
    for (const plGameObject* pCur = pObject; pCur != nullptr; pCur = pCur->GetParent())
    {
      const plInputComponent* pInput = nullptr;
      if (pCur->TryGetComponentOfBaseType(pInput) && pInput->IsActiveAndInitialized())
        return pInput;
    }

    return nullptr;
  }
} // namespace

const plInputComponent* plFindCameraInput(const plGameObject* pFirst, const plGameObject* pSecond)
{
  if (const plInputComponent* pInput = FindInputUpwards(pFirst))
    return pInput;

  return FindInputUpwards(pSecond);
}

float plReadCameraAction(const plInputComponent* pInput, plStringView sAction)
{
  if (pInput == nullptr || sAction.IsEmpty())
    return 0.0f;

  plStringBuilder sTmp;
  return pInput->GetCurrentInputState(sAction.GetData(sTmp));
}

//////////////////////////////////////////////////////////////////////////

void plCameraSubjectTracker::Reset()
{
  m_Info = {};
  m_bHasLast = false;
  m_bWasInAir = false;
  m_fLastVerticalSpeed = 0.0f;
}

const plCameraSubjectInfo& plCameraSubjectTracker::Update(const plWorld* pWorld, plGameObjectHandle hSubject, const plVec3& vPosition, float fDeltaSeconds)
{
  m_Info.m_bValid = true;
  m_Info.m_vPosition = vPosition;
  m_Info.m_fLandingSpeed = 0.0f;

  if (m_bHasLast && fDeltaSeconds > 0.0001f)
  {
    m_Info.m_vVelocity = (vPosition - m_vLastPosition) / fDeltaSeconds;
  }

  m_vLastPosition = vPosition;
  m_bHasLast = true;

  const plGameObject* pSubject = nullptr;
  if (pWorld == nullptr || !pWorld->TryGetObject(hSubject, pSubject))
  {
    m_Info.m_bOnGround = true;
    m_Info.m_bInAir = false;
    m_Info.m_bCrouching = false;
    return m_Info;
  }

  bool bValue = false;
  m_Info.m_bOnGround = CallBoolFunction(pSubject, "IsStandingOnGround", bValue) ? bValue : true;
  m_Info.m_bInAir = CallBoolFunction(pSubject, "IsInAir", bValue) ? bValue : !m_Info.m_bOnGround;
  m_Info.m_bCrouching = CallBoolFunction(pSubject, "IsCrouching", bValue) && bValue;

  // the landing frame: report how fast it was falling just before
  if (m_bWasInAir && !m_Info.m_bInAir)
  {
    m_Info.m_fLandingSpeed = plMath::Min(0.0f, m_fLastVerticalSpeed);
  }

  m_bWasInAir = m_Info.m_bInAir;
  m_fLastVerticalSpeed = m_Info.m_vVelocity.z;
  return m_Info;
}

bool plCameraSubjectTracker::CallBoolFunction(const plGameObject* pObject, const char* szFunction, bool& out_bResult)
{
  for (const plComponent* pComponent : pObject->GetComponents())
  {
    for (const plRTTI* pType = pComponent->GetDynamicRTTI(); pType != nullptr; pType = pType->GetParentType())
    {
      for (const plAbstractFunctionProperty* pFunc : pType->GetFunctions())
      {
        if (pFunc->GetArgumentCount() != 0 || !plStringUtils::IsEqual(pFunc->GetPropertyName(), szFunction))
          continue;

        plVariant result;
        pFunc->Execute(const_cast<plComponent*>(pComponent), plArrayPtr<plVariant>(), result);

        if (!result.CanConvertTo<bool>())
          return false;

        out_bResult = result.ConvertTo<bool>();
        return true;
      }
    }
  }

  return false;
}
