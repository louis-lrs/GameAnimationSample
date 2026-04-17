// Copyright Cinematic QTE System. All Rights Reserved.

#include "CinematicQTEEditorModule.h"
#include "MovieSceneQTETrackEditor.h"
#include "ISequencerModule.h"
#include "Modules/ModuleManager.h"

#define LOCTEXT_NAMESPACE "FCinematicQTEEditorModule"

void FCinematicQTEEditorModule::StartupModule()
{
	ISequencerModule& SequencerModule = FModuleManager::LoadModuleChecked<ISequencerModule>("Sequencer");
	TrackEditorDelegateHandle = SequencerModule.RegisterTrackEditor(
		FOnCreateTrackEditor::CreateStatic(&FMovieSceneQTETrackEditor::CreateTrackEditor));
}

void FCinematicQTEEditorModule::ShutdownModule()
{
	if (FModuleManager::Get().IsModuleLoaded("Sequencer"))
	{
		ISequencerModule& SequencerModule = FModuleManager::GetModuleChecked<ISequencerModule>("Sequencer");
		SequencerModule.UnRegisterTrackEditor(TrackEditorDelegateHandle);
	}
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FCinematicQTEEditorModule, CinematicQTEEditor);
