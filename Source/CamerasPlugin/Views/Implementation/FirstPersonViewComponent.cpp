#include <CamerasPlugin/CamerasPCH.h>

#include <CamerasPlugin/Views/FirstPersonViewComponent.h>
#include <Core/WorldSerializer/WorldReader.h>
#include <Core/WorldSerializer/WorldWriter.h>

// clang-format off
PL_BEGIN_COMPONENT_TYPE(plFirstPersonViewComponent, 1, plComponentMode::Dynamic)
{
  PL_BEGIN_PROPERTIES
  {
    PL_ACCESSOR_PROPERTY("EyeObject", DummyGetter, SetEyeObjectReference)->AddAttributes(new plGameObjectReferenceAttribute()),
    PL_MEMBER_PROPERTY("EyeOffset", m_vEyeOffset),
    PL_MEMBER_PROPERTY("EyeHeightSmoothing", m_EyeHeightSmoothing)->AddAttributes(new plDefaultValueAttribute(plTime::MakeFromSeconds(0.12)), new plClampValueAttribute(plTime::MakeZero(), plTime::MakeFromSeconds(2.0))),
    PL_MEMBER_PROPERTY("MaxPitch", m_MaxPitch)->AddAttributes(new plDefaultValueAttribute(plAngle::MakeFromDegree(85.0f)), new plClampValueAttribute(plAngle::MakeFromDegree(0.0f), plAngle::MakeFromDegree(89.0f))),
    PL_MEMBER_PROPERTY("YawFromEyeObject", m_bYawFromEyeObject)->AddAttributes(new plDefaultValueAttribute(true)),
    PL_MEMBER_PROPERTY("LookInput", m_LookInput)->AddAttributes(new plGroupAttribute("Control")),
    PL_MEMBER_PROPERTY("NearPlane", m_fNearPlane)->AddAttributes(new plClampValueAttribute(0.0f, 1.0f)),
  }
  PL_END_PROPERTIES;
  PL_BEGIN_FUNCTIONS
  {
    PL_SCRIPT_FUNCTION_PROPERTY(SetVerticalRotation, In, "Radians"),
    PL_SCRIPT_FUNCTION_PROPERTY(ChangeVerticalRotation, In, "Radians"),
  }
  PL_END_FUNCTIONS;
}
PL_END_COMPONENT_TYPE;
// clang-format on

plFirstPersonViewComponent::plFirstPersonViewComponent() = default;
plFirstPersonViewComponent::~plFirstPersonViewComponent() = default;

void plFirstPersonViewComponent::SerializeComponent(plWorldWriter& inout_stream) const
{
  SUPER::SerializeComponent(inout_stream);
  auto& s = inout_stream.GetStream();

  inout_stream.WriteGameObjectHandle(m_hEyeObject);
  s << m_vEyeOffset;
  s << m_EyeHeightSmoothing;
  s << m_MaxPitch;
  s << m_bYawFromEyeObject;
  s << m_LookInput.m_sLookAction;
  s << m_LookInput.m_sTurnLeftAction;
  s << m_LookInput.m_sTurnRightAction;
  s << m_LookInput.m_sLookUpAction;
  s << m_LookInput.m_sLookDownAction;
  s << m_LookInput.m_fSensitivity;
  s << m_LookInput.m_bInvertY;
  s << m_fNearPlane;
}

void plFirstPersonViewComponent::DeserializeComponent(plWorldReader& inout_stream)
{
  SUPER::DeserializeComponent(inout_stream);
  auto& s = inout_stream.GetStream();

  m_hEyeObject = inout_stream.ReadGameObjectHandle();
  s >> m_vEyeOffset;
  s >> m_EyeHeightSmoothing;
  s >> m_MaxPitch;
  s >> m_bYawFromEyeObject;
  s >> m_LookInput.m_sLookAction;
  s >> m_LookInput.m_sTurnLeftAction;
  s >> m_LookInput.m_sTurnRightAction;
  s >> m_LookInput.m_sLookUpAction;
  s >> m_LookInput.m_sLookDownAction;
  s >> m_LookInput.m_fSensitivity;
  s >> m_LookInput.m_bInvertY;
  s >> m_fNearPlane;
}

void plFirstPersonViewComponent::OnSimulationStarted()
{
  SUPER::OnSimulationStarted();

  m_Pitch = plAngle();
  m_bHasHeight = false;

  if (const plGameObject* pEye = GetEyeObject())
  {
    const plVec3 vForward = pEye->GetGlobalDirForwards();
    m_OwnYaw = plMath::ATan2(vForward.y, vForward.x);
  }
}

void plFirstPersonViewComponent::SetEyeObjectReference(const char* szReference)
{
  m_hEyeObject = plCameraReference::Resolve(this, szReference, "EyeObject");
}

const plGameObject* plFirstPersonViewComponent::GetEyeObject() const
{
  const plGameObject* pEye = nullptr;
  if (GetWorld()->TryGetObject(m_hEyeObject, pEye))
    return pEye;

  return GetOwner()->GetParent();
}

plGameObjectHandle plFirstPersonViewComponent::GetSubjectObject() const
{
  // the character is the eye object's topmost ancestor: that is where the controller (and its ground state) lives
  const plGameObject* pObject = GetEyeObject();
  if (pObject == nullptr)
    return plGameObjectHandle();

  while (pObject->GetParent() != nullptr)
    pObject = pObject->GetParent();

  return pObject->GetHandle();
}

bool plFirstPersonViewComponent::GetSubjectPoint(plVec3& out_vPosition) const
{
  const plGameObject* pEye = GetEyeObject();
  if (pEye == nullptr)
    return false;

  out_vPosition = pEye->GetGlobalTransform().TransformPosition(m_vEyeOffset);
  return true;
}

void plFirstPersonViewComponent::SetLookRotation(plAngle yaw, plAngle pitch)
{
  m_Pitch = plMath::Clamp(pitch, -m_MaxPitch, m_MaxPitch);
  m_OwnYaw = yaw;
}

void plFirstPersonViewComponent::SetVerticalRotation(float fRadians)
{
  m_Pitch = plMath::Clamp(plAngle::MakeFromRadian(fRadians), -m_MaxPitch, m_MaxPitch);
}

void plFirstPersonViewComponent::ChangeVerticalRotation(float fRadians)
{
  SetVerticalRotation(m_Pitch.GetRadian() + fRadians);
}

void plFirstPersonViewComponent::UpdateView(plCameraViewContext& ref_context, const plCameraPose& previous, plCameraPose& out_pose)
{
  out_pose = previous;

  const plGameObject* pEye = GetEyeObject();
  if (pEye == nullptr)
    return;

  if (m_LookInput.HasAnyAction())
  {
    const float fScale = ref_context.m_PlayerSettings.m_fLookSensitivity * ref_context.m_fLookScale;
    plVec2 vLook = m_LookInput.Read(FindInput(), fScale);

    if (ref_context.m_PlayerSettings.m_bInvertY)
      vLook.y = -vLook.y;

    if (!vLook.IsZero(0.00001f))
      ref_context.m_bPlayerSteering = true;

    m_OwnYaw += plAngle::MakeFromRadian(vLook.x);
    m_Pitch = plMath::Clamp(m_Pitch + plAngle::MakeFromRadian(vLook.y), -m_MaxPitch, m_MaxPitch);
  }

  const plVec3 vEye = pEye->GetGlobalTransform().TransformPosition(m_vEyeOffset);

  // smooth only the height, so crouching eases while walking stays exact
  if (!m_bHasHeight || !m_EyeHeightSmoothing.IsPositive())
  {
    m_fSmoothedHeight = vEye.z;
    m_bHasHeight = true;
  }
  else
  {
    const float fFollow = 1.0f - plMath::Exp(-ref_context.m_fDeltaSeconds / m_EyeHeightSmoothing.AsFloatInSeconds());
    m_fSmoothedHeight = plMath::Lerp(m_fSmoothedHeight, vEye.z, fFollow);
  }

  plAngle yaw = m_OwnYaw;
  if (m_bYawFromEyeObject)
  {
    const plVec3 vForward = pEye->GetGlobalDirForwards();
    yaw = plMath::ATan2(vForward.y, vForward.x);
    m_OwnYaw = yaw;
  }

  out_pose.m_vPosition = plVec3(vEye.x, vEye.y, m_fSmoothedHeight);
  out_pose.m_qRotation = plCameraPose::MakeRotation(yaw, ref_context.m_bStereo ? plAngle() : m_Pitch);
  out_pose.m_fFieldOfView = m_fFieldOfView;
  out_pose.m_fNearPlane = m_fNearPlane;

  m_LastPose = out_pose;
}
