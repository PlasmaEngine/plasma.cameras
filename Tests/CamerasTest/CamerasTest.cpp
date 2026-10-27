#include <CamerasTest/CamerasTestPCH.h>

#include <CamerasPlugin/Core/CameraDirectorComponent.h>
#include <CamerasPlugin/Core/CameraMessages.h>
#include <CamerasPlugin/Modifiers/EffectModifiers.h>
#include <CamerasPlugin/Core/CameraTypes.h>
#include <CamerasPlugin/Modifiers/FramingSolver.h>
#include <CamerasPlugin/Views/FixedViewComponent.h>
#include <CamerasPlugin/Volumes/CameraVolumeComponent.h>
#include <Core/World/World.h>
#include <Foundation/Configuration/Startup.h>
#include <Foundation/Math/Random.h>
#include <RendererCore/Components/CameraComponent.h>

#include <cstdio>

// Headless checks for Gameplay Cameras: the framing maths and the camera director's request rules.
// Prints one line per check and returns the number of failures.

namespace
{
  plUInt32 s_uiFailures = 0;
  plUInt32 s_uiChecks = 0;

  void Check(bool bOk, const char* szWhat, double fValue = 0.0)
  {
    ++s_uiChecks;

    if (!bOk)
      ++s_uiFailures;

    std::printf("%s  %s (%.6f)\n", bOk ? "PASS" : "FAIL", szWhat, fValue);
  }

  plVec3 RandomVec(plRandom& rng, float fMin, float fMax)
  {
    return plVec3(rng.FloatMinMax(fMin, fMax), rng.FloatMinMax(fMin, fMax), rng.FloatMinMax(fMin, fMax));
  }

  void TestBlendCurves()
  {
    bool bOk = true;
    for (plUInt32 c = plCameraBlendCurve::Linear; c <= plCameraBlendCurve::EaseInOut; ++c)
    {
      const auto curve = static_cast<plCameraBlendCurve::Enum>(c);
      bOk &= plMath::IsEqual(plCameraBlendCurve::Evaluate(curve, 0.0f), 0.0f, 0.0001f);
      bOk &= plMath::IsEqual(plCameraBlendCurve::Evaluate(curve, 1.0f), 1.0f, 0.0001f);

      float fPrev = 0.0f;
      for (float t = 0.0f; t <= 1.0f; t += 0.05f)
      {
        const float v = plCameraBlendCurve::Evaluate(curve, t);
        bOk &= v >= fPrev - 0.0001f;
        fPrev = v;
      }
    }

    Check(bOk, "blend curves start at 0, end at 1 and never go back");
    Check(plCameraBlendCurve::Evaluate(plCameraBlendCurve::Cut, 0.0f) == 1.0f, "cut jumps straight to the target");
  }

  void TestAnchors()
  {
    const plVec2 ul = plCameraScreenAnchor::GetPoint(plCameraScreenAnchor::UpperLeft);
    const plVec2 lr = plCameraScreenAnchor::GetPoint(plCameraScreenAnchor::LowerRight);
    const plVec2 mirrored = plCameraScreenAnchor::GetPoint(plCameraScreenAnchor::UpperLeft, -1.0f);

    Check(ul.IsEqual(plVec2(-1.0f / 3.0f, 1.0f / 3.0f), 0.0001f), "upper left is the (-1/3, +1/3) thirds point");
    Check(lr.IsEqual(plVec2(1.0f / 3.0f, -1.0f / 3.0f), 0.0001f), "lower right is the (+1/3, -1/3) thirds point");
    Check(mirrored.IsEqual(plVec2(1.0f / 3.0f, 1.0f / 3.0f), 0.0001f), "mirroring flips left and right only");
  }

  void TestProjection(plRandom& rng)
  {
    float fWorst = 0.0f;

    for (plUInt32 i = 0; i < 500; ++i)
    {
      plCameraPose pose;
      pose.m_vPosition = RandomVec(rng, -50, 50);
      pose.m_qRotation = plCameraPose::MakeRotation(plAngle::MakeFromDegree(rng.FloatMinMax(-180, 180)), plAngle::MakeFromDegree(rng.FloatMinMax(-80, 80)));
      pose.m_fFieldOfView = rng.FloatMinMax(30, 100);

      const plCameraProjection proj = plCameraProjection::Make(pose.m_fFieldOfView, 16.0f / 9.0f, false);
      const plVec2 vNdc(rng.FloatMinMax(-0.9f, 0.9f), rng.FloatMinMax(-0.9f, 0.9f));
      const plVec3 vWorld = pose.m_vPosition + (pose.m_qRotation * proj.GetViewDirection(vNdc)) * rng.FloatMinMax(1, 100);

      plVec2 vBack;
      if (!proj.Project(pose, vWorld, vBack))
        fWorst = 1.0f;
      else
        fWorst = plMath::Max(fWorst, (vBack - vNdc).GetLength());
    }

    Check(fWorst < 0.0005f, "projection round trip (worst NDC error)", fWorst);
  }

  void TestAimAt(plRandom& rng)
  {
    float fWorst = 0.0f;

    for (plUInt32 i = 0; i < 1000; ++i)
    {
      const plVec3 vCamera = RandomVec(rng, -20, 20);
      plVec3 vSubject = vCamera + RandomVec(rng, -10, 10);

      if ((vSubject - vCamera).GetLength() < 1.0f)
        vSubject += plVec3(3, 0, 0);

      // Keep the subject within 45 degrees of the horizon. Beyond that, an anchor far off-center needs the camera
      // pitched past vertical, where no level-horizon aim exists; gameplay cameras never look there.
      const plVec3 vDelta = vSubject - vCamera;
      if (plMath::Abs(vDelta.z) > vDelta.GetAsVec2().GetLength())
        vSubject.z = vCamera.z + plMath::Sign(vDelta.z) * vDelta.GetAsVec2().GetLength();

      const plVec2 vAnchor(rng.FloatMinMax(-0.6f, 0.6f), rng.FloatMinMax(-0.6f, 0.6f));
      const plCameraProjection proj = plCameraProjection::Make(65.0f, 16.0f / 9.0f, false);

      plCameraPose pose;
      pose.m_vPosition = vCamera;
      pose.m_qRotation = plCameraFramingSolver::AimAt(vCamera, vSubject, vAnchor, proj);

      plVec2 vNdc;
      const float fErr = proj.Project(pose, vSubject, vNdc) ? (vNdc - vAnchor).GetLength() : 1.0f;
      fWorst = plMath::Max(fWorst, fErr);
    }

    Check(fWorst < 0.0005f, "aim puts the subject on its anchor (worst NDC error)", fWorst);
  }

  /// \a uiMinPercent: how many random cases must be framed exactly. Level layouts (both subjects at one screen height)
  /// cannot be met when the target is far above or below the player at short range without putting the camera
  /// outside its elevation limits, so they get a lower bar; those cases relax instead (checked separately).
  void TestFrameTwo(plRandom& rng, plCameraScreenAnchor::Enum playerAnchor, plCameraScreenAnchor::Enum targetAnchor, const char* szLayout, plUInt32 uiMinPercent)
  {
    const plCameraProjection proj = plCameraProjection::Make(65.0f, 16.0f / 9.0f, false);
    const plUInt32 uiCases = 300;

    plUInt32 uiExact = 0;
    plUInt32 uiReported = 0;
    float fWorst = 0.0f;
    float fWorstReported = 0.0f;
    float fWorstPin = 0.0f;
    float fWorstElevation = 0.0f;
    float fSum = 0.0f;

    for (plUInt32 i = 0; i < uiCases; ++i)
    {
      const plVec3 vPlayer(0, 0, 1.4f);
      const float fYaw = rng.FloatMinMax(-60, 60);
      const float fDist = rng.FloatMinMax(4, 15);
      const plVec3 vTarget = vPlayer + plVec3(plMath::Cos(plAngle::MakeFromDegree(fYaw)) * fDist, plMath::Sin(plAngle::MakeFromDegree(fYaw)) * fDist, rng.FloatMinMax(-0.5f, 3.0f));
      const plVec3 vPreferred = vPlayer + plVec3(-5, rng.FloatMinMax(-1, 1), 1.5f);

      const plVec2 vA = plCameraScreenAnchor::GetPoint(playerAnchor);
      const plVec2 vB = plCameraScreenAnchor::GetPoint(targetAnchor);

      const auto result = plCameraFramingSolver::FrameTwo(vPlayer, vTarget, vA, vB, proj, vPreferred, 2.5f, 12.0f);

      plCameraPose pose;
      pose.m_vPosition = result.m_vPosition;
      pose.m_qRotation = result.m_qRotation;

      plVec2 vNdcA, vNdcB;
      const float fErrA = proj.Project(pose, vPlayer, vNdcA) ? (vNdcA - vA).GetLength() : 1.0f;
      const float fErrB = proj.Project(pose, vTarget, vNdcB) ? (vNdcB - vB).GetLength() : 1.0f;

      fWorstPin = plMath::Max(fWorstPin, fErrA);
      fWorst = plMath::Max(fWorst, fErrB);
      fSum += fErrB;

      const plVec3 vUp = result.m_vPosition - vPlayer;
      const float fElevation = plMath::ATan2(vUp.z, vUp.GetAsVec2().GetLength()).GetDegree();
      fWorstElevation = plMath::Max(fWorstElevation, plMath::Max(-25.0f - fElevation, fElevation - 70.0f));

      if (fErrB < 0.001f)
        ++uiExact;

      if (result.m_bExact)
      {
        ++uiReported;
        fWorstReported = plMath::Max(fWorstReported, fErrB);
      }
    }

    plStringBuilder s;
    s.SetFormat("two subjects, {}: {} of {} within 0.001 NDC (0.5 px at 1080p), mean error", szLayout, uiExact, uiCases);
    Check(uiExact * 100 >= uiCases * uiMinPercent, s, fSum / uiCases);

    s.SetFormat("two subjects, {}: the player stays pinned in every case (worst NDC error)", szLayout);
    Check(fWorstPin < 0.001f, s, fWorstPin);

    s.SetFormat("two subjects, {}: the camera stays between -25 and 70 degrees elevation (worst excess, degrees)", szLayout);
    Check(fWorstElevation <= 0.01f, s, fWorstElevation);

    s.SetFormat("two subjects, {}: cases the solver calls exact ({}) really are (worst target error)", szLayout, uiReported);
    Check(fWorstReported < 0.01f, s, fWorstReported);

    s.SetFormat("two subjects, {}: worst relaxed target error (informational)", szLayout);
    Check(true, s, fWorst);
  }

  void TestMirrorSymmetry()
  {
    const plCameraProjection proj = plCameraProjection::Make(65.0f, 16.0f / 9.0f, false);
    const plVec3 vPlayer(0, 0, 1.4f);
    const plVec3 vTarget(8, 3, 2.0f);
    const plVec3 vTargetMirrored(8, -3, 2.0f);

    const plVec2 vA = plCameraScreenAnchor::GetPoint(plCameraScreenAnchor::LowerLeft);
    const plVec2 vB = plCameraScreenAnchor::GetPoint(plCameraScreenAnchor::UpperRight);
    const plVec2 vAm = plCameraScreenAnchor::GetPoint(plCameraScreenAnchor::LowerLeft, -1.0f);
    const plVec2 vBm = plCameraScreenAnchor::GetPoint(plCameraScreenAnchor::UpperRight, -1.0f);

    const auto r1 = plCameraFramingSolver::FrameTwo(vPlayer, vTarget, vA, vB, proj, plVec3(-5, 0, 3), 2.5f, 12.0f);
    const auto r2 = plCameraFramingSolver::FrameTwo(vPlayer, vTargetMirrored, vAm, vBm, proj, plVec3(-5, 0, 3), 2.5f, 12.0f);

    const plVec3 vReflected(r2.m_vPosition.x, -r2.m_vPosition.y, r2.m_vPosition.z);
    Check((vReflected - r1.m_vPosition).GetLength() < 0.05f, "a mirrored layout of a mirrored scene gives the mirrored camera (distance)",
      (vReflected - r1.m_vPosition).GetLength());
  }

  void TestZones()
  {
    bool bOk = true;
    bOk &= plCameraFramingSolver::ZoneCorrection(0.05f, 0.1f, 0.5f, 0.5f) == 0.0f;
    bOk &= plMath::IsEqual(plCameraFramingSolver::ZoneCorrection(0.3f, 0.1f, 0.5f, 0.5f), 0.1f, 0.0001f);
    bOk &= plMath::IsEqual(plCameraFramingSolver::ZoneCorrection(0.7f, 0.1f, 0.5f, 0.0f), 0.2f, 0.0001f);
    bOk &= plMath::IsEqual(plCameraFramingSolver::ZoneCorrection(-0.7f, 0.1f, 0.5f, 1.0f), -0.6f, 0.0001f);
    Check(bOk, "dead zone holds still, soft zone follows by the factor, beyond it corrects fully");
  }

  //////////////////////////////////////////////////////////////////////////
  // Director rules, in a real world

  struct DirectorScene
  {
    plWorld* m_pWorld = nullptr;
    plCameraDirectorComponent* m_pDirector = nullptr;
    plFixedViewComponent* m_pViews[4] = {};
  };

  plFixedViewComponent* AddView(plWorld& world, const plVec3& vPosition, const char* szName)
  {
    plGameObjectDesc desc;
    desc.m_LocalPosition = vPosition;
    desc.m_sName.Assign(szName);
    desc.m_bDynamic = true;

    plGameObject* pObject = nullptr;
    world.CreateObject(desc, pObject);

    plFixedViewComponent* pView = nullptr;
    plFixedViewComponent::CreateComponent(pObject, pView);
    return pView;
  }

  void Step(plWorld& world, plUInt32 uiFrames)
  {
    for (plUInt32 i = 0; i < uiFrames; ++i)
      world.Update();
  }

  plVec3 CameraPosition(DirectorScene& scene)
  {
    return scene.m_pDirector->GetOwner()->GetGlobalPosition();
  }

  void TestDirector()
  {
    plWorldDesc desc("CamerasTest");
    plWorld world(desc);
    PL_LOCK(world.GetWriteMarker());

    world.GetClock().SetFixedTimeStep(plTime::MakeFromSeconds(1.0 / 60.0));
    world.SetWorldSimulationEnabled(true);

    DirectorScene scene;
    scene.m_pWorld = &world;
    scene.m_pViews[0] = AddView(world, plVec3(0, 0, -5), "Player");
    scene.m_pViews[1] = AddView(world, plVec3(10, 0, 0), "A");
    scene.m_pViews[2] = AddView(world, plVec3(0, 10, 0), "B");
    scene.m_pViews[3] = AddView(world, plVec3(0, 0, 10), "C");

    plGameObjectDesc cameraDesc;
    cameraDesc.m_sName.Assign("Camera");
    cameraDesc.m_bDynamic = true;
    plGameObject* pCamera = nullptr;
    world.CreateObject(cameraDesc, pCamera);
    plCameraDirectorComponent::CreateComponent(pCamera, scene.m_pDirector);
    scene.m_pDirector->m_BlendTime = plTime::MakeFromSeconds(0.5);
    scene.m_pDirector->m_BlendCurve = plCameraBlendCurve::Linear;
    scene.m_pDirector->SetPlayerView(scene.m_pViews[0]);

    Step(world, 5);
    Check(CameraPosition(scene).IsEqual(plVec3(0, 0, -5), 0.001f), "with no requests the player's view is shown", (CameraPosition(scene) - plVec3(0, 0, -5)).GetLength());

    // a higher priority request takes over after its blend
    const plUInt32 uiA = scene.m_pDirector->RequestView(scene.m_pViews[1], 20, plTime(), plTime::MakeFromSeconds(-1.0), plCameraBlendCurve::Linear, plCameraRequestSource::Script);
    Step(world, 40);
    Check(CameraPosition(scene).IsEqual(plVec3(10, 0, 0), 0.001f), "priority 20 beats the player's 10", (CameraPosition(scene) - plVec3(10, 0, 0)).GetLength());

    // equal priority: the newest wins
    scene.m_pDirector->RequestView(scene.m_pViews[2], 20, plTime(), plTime::MakeZero(), plCameraBlendCurve::Cut, plCameraRequestSource::Script);
    Step(world, 2);
    Check(CameraPosition(scene).IsEqual(plVec3(0, 10, 0), 0.001f), "a tie goes to the newest request", (CameraPosition(scene) - plVec3(0, 10, 0)).GetLength());

    // a lower priority request does not take over
    scene.m_pDirector->RequestView(scene.m_pViews[3], 5, plTime(), plTime::MakeZero(), plCameraBlendCurve::Cut, plCameraRequestSource::Script);
    Step(world, 2);
    Check(CameraPosition(scene).IsEqual(plVec3(0, 10, 0), 0.001f), "priority 5 does not beat 20", (CameraPosition(scene) - plVec3(0, 10, 0)).GetLength());

    // interrupting a blend continues from where the camera is, without a jump
    scene.m_pDirector->ReleaseView(scene.m_pViews[2]->GetHandle());
    Step(world, 10); // blending from B back to A
    const plVec3 vBefore = CameraPosition(scene);
    scene.m_pDirector->RequestView(scene.m_pViews[3], 50, plTime(), plTime::MakeFromSeconds(0.5), plCameraBlendCurve::Linear, plCameraRequestSource::Script);
    Step(world, 1);
    const float fJump = (CameraPosition(scene) - vBefore).GetLength();
    Check(fJump < 1.0f, "an interrupted blend continues from the current camera (largest one-frame step, m)", fJump);
    Step(world, 40);
    Check(CameraPosition(scene).IsEqual(plVec3(0, 0, 10), 0.001f), "the interrupting request is shown after its blend", (CameraPosition(scene) - plVec3(0, 0, 10)).GetLength());

    // timed requests expire on their own
    scene.m_pDirector->ReleaseView(scene.m_pViews[3]->GetHandle());
    scene.m_pDirector->ReleaseRequest(uiA);
    scene.m_pDirector->RequestView(scene.m_pViews[1], 30, plTime::MakeFromSeconds(0.5), plTime::MakeZero(), plCameraBlendCurve::Cut, plCameraRequestSource::Script);
    Step(world, 10);
    const bool bShown = CameraPosition(scene).IsEqual(plVec3(10, 0, 0), 0.001f);
    Step(world, 60);
    const plVec3 vAfter = CameraPosition(scene);
    const bool bGone = vAfter.IsEqual(plVec3(0, 0, -5), 0.001f);
    Check(bShown && bGone, "a timed request shows, then expires back to the player's view");

    // a camera volume requests its view while the player's subject is inside
    plGameObjectDesc volumeDesc;
    volumeDesc.m_LocalPosition = plVec3(50, 0, 0);
    plGameObject* pVolumeObject = nullptr;
    world.CreateObject(volumeDesc, pVolumeObject);
    plCameraVolumeComponent* pVolume = nullptr;
    plCameraVolumeComponent::CreateComponent(pVolumeObject, pVolume);
    pVolume->m_vHalfExtents = plVec3(2.0f);
    pVolume->m_BlendIn = plTime::MakeZero();
    pVolume->m_BlendOut = plTime::MakeZero();
    pVolume->SetViewReference("C");

    Check(pVolume->Contains(plVec3(51, 1, -1)) && !pVolume->Contains(plVec3(53, 0, 0)), "volume containment");
  }

  void TestModifiers()
  {
    plCameraModifierContext ctx;
    ctx.m_fDeltaSeconds = 1.0f / 60.0f;
    ctx.m_ViewKind = plCameraViewKind::FirstPerson;

    plCameraPose base;
    base.m_fFieldOfView = 70.0f;

    // shake: trauma squared, decaying at TraumaDecay per second
    {
      plCameraShakeModifier shake;
      shake.m_bUseShakeVolumes = false;

      plMsgCameraImpulse msg;
      msg.m_Type = plCameraImpulseType::Shake;
      msg.m_fStrength = 1.0f;
      shake.OnImpulse(msg);

      for (plUInt32 i = 0; i < 30; ++i)
      {
        plCameraPose pose = base;
        shake.Apply(ctx, pose, 1.0f);
      }

      Check(plMath::IsEqual(shake.GetTrauma(), 1.0f - 0.9f * 0.5f, 0.01f), "shake trauma decays by Trauma Decay per second (after 0.5 s)", shake.GetTrauma());
    }

    // recoil: a kick lifts the aim, the spring brings it back
    {
      plCameraRecoilModifier recoil;

      plMsgCameraImpulse msg;
      msg.m_Type = plCameraImpulseType::Recoil;
      msg.m_Pitch = plAngle::MakeFromDegree(3.0f);
      recoil.OnImpulse(msg);

      float fPeak = 0.0f;
      plAngle yaw, pitch;

      for (plUInt32 i = 0; i < 120; ++i)
      {
        plCameraPose pose = base;
        recoil.Apply(ctx, pose, 1.0f);
        pose.GetYawPitch(yaw, pitch);
        fPeak = plMath::Max(fPeak, pitch.GetDegree());
      }

      Check(fPeak > 2.4f && fPeak < 3.6f && plMath::Abs(pitch.GetDegree()) < 0.05f, "a 3 degree recoil peaks within 20% of 3 degrees and recovers within 2 s (peak)", fPeak);
    }

    // zoom: Fov Scale applied once fully blended in
    {
      plCameraZoomModifier zoom;
      zoom.SetZoomed(true);

      plCameraPose pose;
      for (plUInt32 i = 0; i < 120; ++i)
      {
        pose = base;
        zoom.Apply(ctx, pose, 1.0f);
      }

      Check(plMath::IsEqual(pose.m_fFieldOfView, 70.0f * 0.6f, 0.05f), "zoom scales the field of view by Fov Scale", pose.m_fFieldOfView);
    }

    // head bob: nothing while the subject stands still
    {
      plCameraHeadBobModifier bob;
      plCameraPose pose = base;
      bob.Apply(ctx, pose, 1.0f);
      Check(pose.m_vPosition.IsZero(0.0001f), "head bob stays still while the subject does");
    }
  }

  void TestSequenceYield()
  {
    plWorldDesc desc("CamerasYieldTest");
    plWorld world(desc);
    PL_LOCK(world.GetWriteMarker());

    world.GetClock().SetFixedTimeStep(plTime::MakeFromSeconds(1.0 / 60.0));
    world.SetWorldSimulationEnabled(true);

    plFixedViewComponent* pPlayerView = AddView(world, plVec3(0, 0, 0), "Player");

    // the director's camera
    plGameObjectDesc cameraDesc;
    cameraDesc.m_sName.Assign("Camera");
    cameraDesc.m_bDynamic = true;
    plGameObject* pCameraObject = nullptr;
    world.CreateObject(cameraDesc, pCameraObject);

    plCameraComponent* pOwnCamera = nullptr;
    plCameraComponent::CreateComponent(pCameraObject, pOwnCamera);
    pOwnCamera->SetUsageHint(plCameraUsageHint::MainView);

    plCameraDirectorComponent* pDirector = nullptr;
    plCameraDirectorComponent::CreateComponent(pCameraObject, pDirector);
    pDirector->SetPlayerView(pPlayerView);
    pDirector->m_BlendTime = plTime::MakeFromSeconds(0.5);
    pDirector->m_BlendCurve = plCameraBlendCurve::Linear;

    // a sequence's cut camera, idle for now
    plGameObjectDesc cutDesc;
    cutDesc.m_sName.Assign("CutCamera");
    cutDesc.m_LocalPosition = plVec3(20, 0, 0);
    plGameObject* pCutObject = nullptr;
    world.CreateObject(cutDesc, pCutObject);

    plCameraComponent* pCutCamera = nullptr;
    plCameraComponent::CreateComponent(pCutObject, pCutCamera);
    pCutCamera->SetUsageHint(plCameraUsageHint::None);

    Step(world, 5);

    // the sequence cuts to its camera, as the sequencer does: by promoting it to Main View
    pCutCamera->SetUsageHint(plCameraUsageHint::MainView);
    Step(world, 2);
    Check(pOwnCamera->GetUsageHint() == plCameraUsageHint::None, "the director steps aside while a sequence camera holds the Main View");

    // the sequence ends and restores its camera
    pCutCamera->SetUsageHint(plCameraUsageHint::None);
    Step(world, 1);
    Check(pOwnCamera->GetUsageHint() == plCameraUsageHint::MainView, "the director takes the Main View back when the sequence ends");

    const float fStart = pCameraObject->GetGlobalPosition().x;
    Step(world, 60);
    const float fEnd = pCameraObject->GetGlobalPosition().x;
    Check(fStart > 15.0f && plMath::Abs(fEnd) < 0.001f, "it blends back from the cut camera's pose to the player's view (x at start of blend)", fStart);
  }
} // namespace

int main(int argc, const char** argv)
{
  plStartup::StartupCoreSystems();

  {
    plRandom rng;
    rng.Initialize(42);

    TestBlendCurves();
    TestAnchors();
    TestProjection(rng);
    TestAimAt(rng);
    TestZones();
    TestFrameTwo(rng, plCameraScreenAnchor::LowerLeft, plCameraScreenAnchor::UpperRight, "thirds diagonal", 90);
    TestFrameTwo(rng, plCameraScreenAnchor::LowerLeft, plCameraScreenAnchor::MiddleRight, "over the shoulder", 90);
    TestFrameTwo(rng, plCameraScreenAnchor::MiddleLeft, plCameraScreenAnchor::MiddleRight, "thirds horizontal", 80);
    TestMirrorSymmetry();
    TestModifiers();
    TestDirector();
    TestSequenceYield();
  }

  std::printf("\n%u checks, %u failed\n", s_uiChecks, s_uiFailures);

  plStartup::ShutdownCoreSystems();
  return static_cast<int>(s_uiFailures);
}
