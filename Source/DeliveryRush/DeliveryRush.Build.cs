using UnrealBuildTool;

public class DeliveryRush : ModuleRules
{
    public DeliveryRush(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        CppStandard = CppStandardVersion.Cpp20;
        PublicDependencyModuleNames.AddRange(new string[] {
            "Core", "CoreUObject", "Engine", "InputCore", "ProceduralMeshComponent"
        });
        PrivateDependencyModuleNames.Add("RHI");
        if (Target.bBuildEditor)
            PrivateDependencyModuleNames.AddRange(new string[] { "MeshDescription", "StaticMeshDescription", "AssetRegistry" });
    }
}
