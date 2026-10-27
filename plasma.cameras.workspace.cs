// #pl-version 1

using PlasmaBuild.Core.Configuration;
using PlasmaBuild.Core.Rules;

public class PlasmaCamerasWorkspace : WorkspaceRules
{
    public PlasmaCamerasWorkspace(BuildContext context) : base(context)
    {
        TargetNames.Add("CamerasPlugin");
        TargetNames.Add("EditorPluginCameras");
        TargetNames.Add("CamerasTest");
    }
}
