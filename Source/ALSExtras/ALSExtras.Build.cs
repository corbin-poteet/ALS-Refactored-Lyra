using UnrealBuildTool;

public class ALSExtras : ModuleRules
{
	public ALSExtras(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_3;

		bEnableNonInlinedGenCppWarnings = true;
		// UnsafeTypeCastWarningLevel = WarningLevel.Warning;

		PublicDependencyModuleNames.AddRange(new[]
		{
			"Core", "CoreUObject", "Engine", "AIModule", "ALS", "LyraGame", "AIModule", "GameplayAbilities", "ModularGameplay", "ModularGameplayActors"
		});

		PrivateDependencyModuleNames.AddRange(new[]
		{
			"EnhancedInput", "ALSCamera"
		});
	}
}