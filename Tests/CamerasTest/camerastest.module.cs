using PlasmaBuild.Core.Configuration;
using PlasmaBuild.Core.Rules;

public class CamerasTestModule : ModuleRules
{
    public CamerasTestModule(BuildContext context) : base(context)
    {
        PlasmaPackageSdk.ConfigurePackageModule(this, context, "Tests/CamerasTest", "CamerasTestPCH.h", "BUILDSYSTEM_BUILDING_CAMERASTEST_LIB");
        PlasmaPackageSdk.AddPackagePluginImport(this, context, "Source/CamerasPlugin", "plCamerasPlugin");
    }
}
