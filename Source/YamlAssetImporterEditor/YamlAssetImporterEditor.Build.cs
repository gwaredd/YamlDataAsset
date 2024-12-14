using UnrealBuildTool;

public class YamlAssetImporterEditor : ModuleRules
{
    public YamlAssetImporterEditor( ReadOnlyTargetRules Target ) : base( Target )
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        if( Target.Configuration != UnrealTargetConfiguration.Shipping )
        {
            OptimizeCode = CodeOptimization.Never; // for debugging
        }

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
