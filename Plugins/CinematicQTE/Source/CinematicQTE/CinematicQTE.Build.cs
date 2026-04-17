// Copyright Cinematic QTE System. All Rights Reserved.

using UnrealBuildTool;

public class CinematicQTE : ModuleRules
{
	public CinematicQTE(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"MovieScene",
			"MovieSceneTracks",
			"LevelSequence",
			"EnhancedInput",
			"UMG",
			"InputCore"
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"Slate",
			"SlateCore",
			"DeveloperSettings"
		});
	}
}
