using System.IO;
using UnrealBuildTool;

public class YamlAssetImporterEditor : ModuleRules
{
    public YamlAssetImporterEditor( ReadOnlyTargetRules Target ) : base( Target )
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        //Type = ModuleType.CPlusPlus;

        if( Target.Configuration != UnrealTargetConfiguration.Shipping )
        {
            OptimizeCode = CodeOptimization.Never; // for debugging
        }

        bEnableExceptions = true;

        PublicDefinitions.AddRange( new string[] {
            "YAML_CPP_API=YAMLASSETIMPORTEREDITOR_API", // replace the source ExportHeader with our ExportHeader
        });

        PrivateIncludePaths.Add( Path.Combine( PluginDirectory, "Source/YamlAssetImporterEditor/ThirdParty" ) );

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
