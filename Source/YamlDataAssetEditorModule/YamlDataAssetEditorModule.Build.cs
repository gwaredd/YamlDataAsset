using System.IO;
using UnrealBuildTool;

public class YamlDataAssetEditorModule : ModuleRules
{
    public YamlDataAssetEditorModule( ReadOnlyTargetRules Target ) : base( Target )
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        // if( Target.Configuration != UnrealTargetConfiguration.Shipping )
        // {
        //     OptimizeCode = CodeOptimization.Never; // for debugging
        // }

        bEnableExceptions = true;

        PublicDefinitions.AddRange( new string[] {
            "YAML_CPP_API=YAMLDATAASSETEDITORMODULE_API", // replace the source ExportHeader with our ExportHeader
        });

        PrivateIncludePaths.Add( Path.Combine( PluginDirectory, "Source/YamlDataAssetEditorModule/ThirdParty" ) );

        PublicDependencyModuleNames.AddRange( new string[] {
            "Core",
            "CoreUObject",
            "Engine",
            "InputCore",
            "Slate",
            "SlateCore",
            "UnrealEd",
        } );
    }
}
