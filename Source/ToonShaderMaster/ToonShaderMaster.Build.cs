using UnrealBuildTool;

public class ToonShaderMaster : ModuleRules
{
	public ToonShaderMaster(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine"
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"Slate",
			"SlateCore",
			"InputCore",
			"UnrealEd",
			"AssetTools",
			"AssetRegistry",
			"PropertyEditor",
			"ToolMenus"
		});
	}
}
