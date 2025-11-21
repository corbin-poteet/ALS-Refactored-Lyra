using UnrealBuildTool;

public class ALSLyra : ModuleRules
{
	public ALSLyra(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_7;
		
		CppCompileWarningSettings.NonInlinedGenCppWarningLevel = WarningLevel.Warning;
		
		PublicDependencyModuleNames.AddRange(new[]
		{
			"Core", "CoreUObject", "Engine", "AIModule", "ALS", "LyraGame"
		});
		
		PrivateDependencyModuleNames.AddRange(new[]
		{
			"EnhancedInput", "ALSCamera", "GameplayAbilities",
		});
	}
}