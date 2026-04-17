// Copyright Cinematic QTE System. All Rights Reserved.

using UnrealBuildTool;

public class CinematicQTEEditor : ModuleRules
{
	public CinematicQTEEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"UnrealEd",
			"Sequencer",
			"MovieScene",
			"MovieSceneTools",
			"MovieSceneTracks",
			"LevelSequence",
			"CinematicQTE",
			"InputCore",
			"Slate",
			"SlateCore",
			"EditorFramework",
			"ToolMenus"
		});
	}
}
