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
        // Modifier translation units have private helpers with matching names.
        UseUnityBuild = false;
        UseAdaptiveUnityBuild = false;
        UseIncrementalLinking = true;
        ExtraModules.Add("CamerasPluginModule");
    }
}
