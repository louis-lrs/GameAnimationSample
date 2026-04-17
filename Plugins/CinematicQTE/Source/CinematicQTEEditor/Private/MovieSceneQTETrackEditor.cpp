// Copyright Cinematic QTE System. All Rights Reserved.

#include "MovieSceneQTETrackEditor.h"
#include "Sequencer/MovieSceneQTETrack.h"
#include "Sequencer/MovieSceneQTESection.h"
#include "MovieScene.h"
#include "ISequencer.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "ScopedTransaction.h"
#include "Styling/AppStyle.h"

#define LOCTEXT_NAMESPACE "QTETrackEditor"

FMovieSceneQTETrackEditor::FMovieSceneQTETrackEditor(TSharedRef<ISequencer> InSequencer)
	: FMovieSceneTrackEditor(InSequencer)
{
}

TSharedRef<ISequencerTrackEditor> FMovieSceneQTETrackEditor::CreateTrackEditor(TSharedRef<ISequencer> InSequencer)
{
	return MakeShared<FMovieSceneQTETrackEditor>(InSequencer);
}

bool FMovieSceneQTETrackEditor::SupportsType(TSubclassOf<UMovieSceneTrack> TrackClass) const
{
	return TrackClass == UMovieSceneQTETrack::StaticClass();
}

void FMovieSceneQTETrackEditor::BuildAddTrackMenu(FMenuBuilder& MenuBuilder)
{
	MenuBuilder.AddMenuEntry(
		LOCTEXT("AddQTETrack", "QTE Track"),
		LOCTEXT("AddQTETrackTooltip", "Add a Cinematic QTE track for inserting quick-time events."),
		FSlateIcon(FAppStyle::GetAppStyleSetName(), "Sequencer.Tracks.Event"),
		FUIAction(FExecuteAction::CreateSP(this, &FMovieSceneQTETrackEditor::HandleAddQTETrack))
	);
}

const FSlateBrush* FMovieSceneQTETrackEditor::GetIconBrush() const
{
	return FAppStyle::GetBrush("Sequencer.Tracks.Event");
}

void FMovieSceneQTETrackEditor::HandleAddQTETrack()
{
	TSharedPtr<ISequencer> Sequencer = GetSequencer();
	UMovieScene* MovieScene = Sequencer.IsValid() ? Sequencer->GetFocusedMovieSceneSequence()->GetMovieScene() : nullptr;
	if (!MovieScene || MovieScene->IsReadOnly()) return;

	const FScopedTransaction Transaction(LOCTEXT("AddQTETrack_Transaction", "Add QTE Track"));
	MovieScene->Modify();

	UMovieSceneQTETrack* NewTrack = MovieScene->AddTrack<UMovieSceneQTETrack>();
	if (NewTrack)
	{
		Sequencer->NotifyMovieSceneDataChanged(EMovieSceneDataChangeType::MovieSceneStructureItemAdded);
	}
}

#undef LOCTEXT_NAMESPACE
