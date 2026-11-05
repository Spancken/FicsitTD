using UnrealBuildTool;

public class FicsitTD : ModuleRules
{
	public FicsitTD(ReadOnlyTargetRules Target) : base(Target)
	{
		CppStandard = CppStandardVersion.Cpp20;
		DefaultBuildSettings = BuildSettingsVersion.Latest;
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		bLegacyPublicIncludePaths = false;

		PublicDependencyModuleNames.AddRange(new[] {
			"Core", "CoreUObject", "Engine",
			"AIModule", "GameplayTasks", "NavigationSystem", "AssetRegistry",
			"FactoryGame", "SML", "UMG", "Slate", "SlateCore", "Json", "RenderCore", "RHI", "AbstractInstance", "AkAudio", "WwiseFileHandler", "WwiseResourceLoader", "WwiseSoundEngine", "Projects", "InputCore"
		});
	}
}
