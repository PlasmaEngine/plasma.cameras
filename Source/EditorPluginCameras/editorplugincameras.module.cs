using PlasmaBuild.Core.Configuration;
using PlasmaBuild.Core.Rules;

public class EditorPluginCamerasModule : ModuleRules
{
    public EditorPluginCamerasModule(BuildContext context) : base(context)
    {
        PlasmaPackageSdk.ConfigurePackageModule(this, context, "Source/EditorPluginCameras", "EditorPluginCamerasPCH.h",
            "BUILDSYSTEM_BUILDING_EDITORPLUGINCAMERAS_LIB");

        PlasmaPackageSdk.ConfigureEditorSdkConsumer(this, context);
        PlasmaPackageSdk.AddPackagePluginImport(this, context, "Source/CamerasPlugin", "plCamerasPlugin");

        // the package's icon folder is registered with the engine-drawn editor UI
        PlasmaPackageSdk.AddImportLibrary(this, context, "plEditorUi");
    }
}
