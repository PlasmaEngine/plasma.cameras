#include <EditorPluginCameras/EditorPluginCamerasPCH.h>

#include <CamerasPlugin/Modifiers/FramingModifiers.h>
#include <CamerasPlugin/Views/FixedViewComponent.h>
#include <CamerasPlugin/Views/ThirdPersonViewComponent.h>
#include <CamerasPlugin/Volumes/CameraVolumeComponent.h>
#include <EditorUi/Widgets.h>
#include <Foundation/Configuration/Plugin.h>

// The editor side of Gameplay Cameras: the package's icons and hiding properties that do nothing in the current mode.
// Visualizers come from the runtime plugin itself (selected views draw their rigs).

namespace
{
  /// The package's icon folder (":/TypeIcons/plCameraDirectorComponent.svg" is "EditorIcons/TypeIcons/...").
  /// The plugin binary lives in <package>/Bin/<configuration>.
  plString s_sIconFolder;

  void AddPackageIconFolder()
  {
    plStringBuilder sPluginFile, sCopiedFile;
    plPlugin::GetPluginPaths("plEditorPluginCameras", sPluginFile, sCopiedFile, 0);

    plStringBuilder sFolder = sPluginFile.GetFileDirectory();
    sFolder.AppendPath("..", "..", "EditorIcons");
    sFolder.MakeCleanPath();

    s_sIconFolder = sFolder;
    plEditorUi::AddIconFolder(s_sIconFolder);
  }

  void RemovePackageIconFolder()
  {
    if (s_sIconFolder.IsEmpty())
      return;

    plEditorUi::RemoveIconFolder(s_sIconFolder);
    s_sIconFolder.Clear();
  }

  void Show(plPropertyMetaStateEvent& e, const char* szProperty, bool bVisible)
  {
    (*e.m_pPropertyStates)[szProperty].m_Visibility = bVisible ? plPropertyUiState::Default : plPropertyUiState::Invisible;
  }

  plInt64 GetInt(plPropertyMetaStateEvent& e, const char* szProperty)
  {
    return e.m_pObject->GetTypeAccessor().GetValue(szProperty).ConvertTo<plInt64>();
  }

  bool GetBool(plPropertyMetaStateEvent& e, const char* szProperty)
  {
    return e.m_pObject->GetTypeAccessor().GetValue(szProperty).ConvertTo<bool>();
  }

  void CamerasPropertyMetaState(plPropertyMetaStateEvent& e)
  {
    const plRTTI* pType = e.m_pObject->GetTypeAccessor().GetType();

    if (pType == plGetStaticRTTI<plCameraVolumeComponent>())
    {
      const bool bSphere = GetInt(e, "Shape") == plCameraVolumeShape::Sphere;
      Show(e, "Radius", bSphere);
      Show(e, "HalfExtents", !bSphere);
    }
    else if (pType == plGetStaticRTTI<plThirdPersonViewComponent>())
    {
      const bool bFollow = GetInt(e, "YawMode") == plCameraYawMode::FollowTarget;
      const bool bRecenter = bFollow && GetBool(e, "AutoRecenter");
      Show(e, "AutoRecenter", bFollow);
      Show(e, "RecenterDelay", bRecenter);
      Show(e, "RecenterSpeed", bRecenter);
    }
    else if (pType == plGetStaticRTTI<plFixedViewComponent>())
    {
      Show(e, "FrameSize", GetBool(e, "FrameTarget"));
    }
    else if (pType == plGetStaticRTTI<plCameraLookAtModifier>())
    {
      const bool bAim = GetInt(e, "Framing") == plCameraLookAtFraming::Aim;
      Show(e, "Layout", !bAim);
      Show(e, "Mirror", !bAim);
      Show(e, "ZoomFieldOfView", bAim);
      Show(e, "Duration", GetInt(e, "Hold") == plCameraLookAtHold::ForDuration);
    }
    else if (pType == plGetStaticRTTI<plCameraLockOnModifier>())
    {
      Show(e, "Mirror", GetInt(e, "Layout") != plCameraFramingLayout::Custom);
    }
  }

  void OnLoadPlugin()
  {
    AddPackageIconFolder();
    plPropertyMetaState::GetSingleton()->m_Events.AddEventHandler(CamerasPropertyMetaState);
  }

  void OnUnloadPlugin()
  {
    plPropertyMetaState::GetSingleton()->m_Events.RemoveEventHandler(CamerasPropertyMetaState);
    RemovePackageIconFolder();
  }
} // namespace

PL_PLUGIN_DEPENDENCY(plCamerasPlugin);

PL_PLUGIN_ON_LOADED()
{
  OnLoadPlugin();
}

PL_PLUGIN_ON_UNLOADED()
{
  OnUnloadPlugin();
}
