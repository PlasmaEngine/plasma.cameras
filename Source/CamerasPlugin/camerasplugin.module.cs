using PlasmaBuild.Core.Configuration;
using PlasmaBuild.Core.Rules;

public class CamerasPluginModule : ModuleRules
{
    public CamerasPluginModule(BuildContext context) : base(context)
    {
        // Physics queries go through Core's plPhysicsWorldModuleInterface, so no physics package is linked.
        PlasmaPackageSdk.ConfigurePackageModule(this, context, "Source/CamerasPlugin",
            "CamerasPCH.h", "BUILDSYSTEM_BUILDING_CAMERASPLUGIN_LIB");
    }
}
