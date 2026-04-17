// Copyright Cinematic QTE System. All Rights Reserved.

using UnrealBuildTool;

public class CinematicQTETests : ModuleRules
{
	public CinematicQTETests(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"CinematicQTE",
			"LevelSequence",
			"MovieScene",
			"EnhancedInput"
		});
	}
}
