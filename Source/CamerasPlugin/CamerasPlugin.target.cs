using PlasmaBuild.Core.Configuration;
using PlasmaBuild.Core.Rules;

public class CamerasPluginTarget : TargetRules
{
    public CamerasPluginTarget(BuildContext context) : base(context)
    {
        Type = TargetType.SharedLibrary;
        OutputName = "plCamerasPlugin";
        OutputDirectory = PlasmaPackageSdk.PackageBinaryDirectory(context);
        UsePCHFiles = true;
        UseUnityBuild = true;
        UseAdaptiveUnityBuild = true;
        UseIncrementalLinking = true;
        ExtraModules.Add("CamerasPluginModule");
    }
}
