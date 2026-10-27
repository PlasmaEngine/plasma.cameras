using PlasmaBuild.Core.Configuration;
using PlasmaBuild.Core.Rules;

/// Headless checks for the camera maths and the director's request rules. A console program without a window:
/// run it with the SDK's Binaries/<platform>_<config> directory on PATH.
public class CamerasTestTarget : TargetRules
{
    public CamerasTestTarget(BuildContext context) : base(context)
    {
        Type = TargetType.Executable;
        OutputName = "CamerasTest";
        OutputDirectory = PlasmaPackageSdk.PackageBinaryDirectory(context);
        UsePCHFiles = false;
        UseUnityBuild = false;
        TargetDependencies.Add("CamerasPlugin");
        ExtraModules.Add("CamerasTestModule");
    }
}
