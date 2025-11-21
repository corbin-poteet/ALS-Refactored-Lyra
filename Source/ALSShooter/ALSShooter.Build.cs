using UnrealBuildTool;

public class ALSShooter : ModuleRules
{
	public ALSShooter(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_7;
		
		CppCompileWarningSettings.NonInlinedGenCppWarningLevel = WarningLevel.Warning;
		
		PublicDependencyModuleNames.AddRange(new[]
		{
			"Core", "CoreUObject", "Engine", "AIModule", "ALSLyra"
		});
		
		PrivateDependencyModuleNames.AddRange(new[]
		{
			"EnhancedInput", "ALSCamera", "GameplayAbilities",
		});
	}
}