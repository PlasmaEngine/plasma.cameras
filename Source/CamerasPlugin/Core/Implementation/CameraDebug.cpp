#include <CamerasPlugin/CamerasPCH.h>

#include <CamerasPlugin/Core/CameraDebug.h>
#include <Foundation/Configuration/CVar.h>
#include <RendererCore/Debug/DebugRenderer.h>
#include <RendererCore/Pipeline/View.h>
#include <RendererCore/RenderWorld/RenderWorld.h>

plCVarInt cvar_CamerasDebug("Cameras.Debug", 0, plCVarFlags::Default,
  "1 prints each camera director's request stack, blend and modifiers in the game view. 2 also draws the view rigs and the framing guides.");

plInt32 plCameraDebug::GetLevel()
{
  return cvar_CamerasDebug;
}

void plCameraDebug::DrawPoseFrustum(const plWorld* pWorld, const plCameraPose& pose, const plColor& color, float fLength)
{
  const plVec3 vForward = pose.GetForward();
  const plVec3 vLeft = pose.GetLeft();
  const plVec3 vUp = pose.GetUp();

  // a view that keeps the camera's own field of view has none of its own yet; draw a typical one
  const float fFov = pose.m_fFieldOfView > 0.0f ? pose.m_fFieldOfView : 60.0f;
  const float fHalf = fLength * plMath::Tan(plAngle::MakeFromDegree(fFov * 0.5f));
  const plVec3 vCenter = pose.m_vPosition + vForward * fLength;
  const plVec3 vWide = vLeft * fHalf * (16.0f / 9.0f);
  const plVec3 vTall = vUp * fHalf;

  const plVec3 corners[4] = {vCenter + vWide + vTall, vCenter - vWide + vTall, vCenter - vWide - vTall, vCenter + vWide - vTall};

  plHybridArray<plDebugRenderer::Line, 8> lines;

  for (plUInt32 i = 0; i < 4; ++i)
  {
    lines.PushBack(plDebugRenderer::Line(pose.m_vPosition, corners[i]));
    lines.PushBack(plDebugRenderer::Line(corners[i], corners[(i + 1) % 4]));
  }

  plDebugRenderer::DrawLines(pWorld, lines, color);
}

void plCameraDebug::DrawText(const plWorld* pWorld, plStringView sText, const plColor& color)
{
  plDebugRenderer::DrawInfoText(pWorld, plDebugTextPlacement::TopLeft, "Cameras", plFmt("{}", sText), color);
}

void plCameraDebug::DrawFramingGuides(const plWorld* pWorld, const plVec2& vAnchor, const plVec2& vDeadZone, float fSoftZone, const plVec2& vSubject)
{
  const plView* pView = plRenderWorld::GetViewByUsageHint(plCameraUsageHint::MainView, plCameraUsageHint::EditorView, pWorld);
  if (pView == nullptr)
    return;

  const plRectFloat viewport = pView->GetViewport();
  const float w = viewport.width;
  const float h = viewport.height;

  // NDC (+Y up) to pixels (+Y down)
  auto toPixel = [&](float x, float y)
  { return plVec3((x * 0.5f + 0.5f) * w, (0.5f - y * 0.5f) * h, 0.0f); };

  plHybridArray<plDebugRenderer::Line, 32> thirds;
  thirds.PushBack(plDebugRenderer::Line(toPixel(-1.0f / 3.0f, 1), toPixel(-1.0f / 3.0f, -1)));
  thirds.PushBack(plDebugRenderer::Line(toPixel(1.0f / 3.0f, 1), toPixel(1.0f / 3.0f, -1)));
  thirds.PushBack(plDebugRenderer::Line(toPixel(-1, 1.0f / 3.0f), toPixel(1, 1.0f / 3.0f)));
  thirds.PushBack(plDebugRenderer::Line(toPixel(-1, -1.0f / 3.0f), toPixel(1, -1.0f / 3.0f)));
  plDebugRenderer::Draw2DLines(pWorld, thirds, plColor(1, 1, 1, 0.2f));

  auto rect = [&](const plVec2& vHalf, plDynamicArray<plDebugRenderer::Line>& out_lines)
  {
    const plVec3 a = toPixel(vAnchor.x - vHalf.x, vAnchor.y + vHalf.y);
    const plVec3 b = toPixel(vAnchor.x + vHalf.x, vAnchor.y + vHalf.y);
    const plVec3 c = toPixel(vAnchor.x + vHalf.x, vAnchor.y - vHalf.y);
    const plVec3 d = toPixel(vAnchor.x - vHalf.x, vAnchor.y - vHalf.y);
    out_lines.PushBack(plDebugRenderer::Line(a, b));
    out_lines.PushBack(plDebugRenderer::Line(b, c));
    out_lines.PushBack(plDebugRenderer::Line(c, d));
    out_lines.PushBack(plDebugRenderer::Line(d, a));
  };

  plHybridArray<plDebugRenderer::Line, 8> soft;
  rect(plVec2(fSoftZone), soft);
  plDebugRenderer::Draw2DLines(pWorld, soft, plColor(0.43f, 0.66f, 1.0f, 0.6f));

  plHybridArray<plDebugRenderer::Line, 8> dead;
  rect(vDeadZone, dead);
  plDebugRenderer::Draw2DLines(pWorld, dead, plColor(1, 1, 1, 0.6f));

  const plVec3 vPixel = toPixel(vSubject.x, vSubject.y);
  plHybridArray<plDebugRenderer::Line, 4> subject;
  subject.PushBack(plDebugRenderer::Line(vPixel - plVec3(8, 0, 0), vPixel + plVec3(8, 0, 0)));
  subject.PushBack(plDebugRenderer::Line(vPixel - plVec3(0, 8, 0), vPixel + plVec3(0, 8, 0)));
  plDebugRenderer::Draw2DLines(pWorld, subject, plColor(0.56f, 0.84f, 0.58f));
}
