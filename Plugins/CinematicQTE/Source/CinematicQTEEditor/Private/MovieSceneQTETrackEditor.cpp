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

void FMovieSceneQTETrackEditor::BuildTrackContextMenu(FMenuBuilder& MenuBuilder, UMovieSceneTrack* Track)
{
	MenuBuilder.BeginSection("QTE", LOCTEXT("QTESectionHeader", "QTE"));
	{
		MenuBuilder.AddMenuEntry(
			LOCTEXT("AddQTESection", "Add QTE Section"),
			LOCTEXT("AddQTESectionTooltip", "Add a new QTE section at the current playback time."),
			FSlateIcon(FAppStyle::GetAppStyleSetName(), "Sequencer.Tracks.Event"),
			FUIAction(FExecuteAction::CreateSP(this, &FMovieSceneQTETrackEditor::HandleAddQTESection, Track))
		);
	}
	MenuBuilder.EndSection();
}

void FMovieSceneQTETrackEditor::HandleAddQTESection(UMovieSceneTrack* Track)
{
	UMovieSceneQTETrack* QTETrack = Cast<UMovieSceneQTETrack>(Track);
	TSharedPtr<ISequencer> LocalSequencer = GetSequencer();
	if (!QTETrack || !LocalSequencer.IsValid())
	{
		return;
	}

	UMovieScene* MovieScene = QTETrack->GetTypedOuter<UMovieScene>();
	if (!MovieScene || MovieScene->IsReadOnly())
	{
		return;
	}

	const FScopedTransaction Transaction(LOCTEXT("AddQTESection_Transaction", "Add QTE Section"));
	QTETrack->Modify();

	UMovieSceneSection* NewSection = QTETrack->CreateNewSection();
	if (!NewSection)
	{
		return;
	}

	// 以当前播放光标位置作为 Section 起点，默认长度 1 秒
	const FFrameNumber CurrentFrame = LocalSequencer->GetLocalTime().Time.FrameNumber;
	const FFrameRate TickResolution = MovieScene->GetTickResolution();
	const FFrameNumber DefaultLength = (1.0 * TickResolution).FrameNumber;
	NewSection->SetRange(TRange<FFrameNumber>(CurrentFrame, CurrentFrame + DefaultLength));

	QTETrack->AddSection(*NewSection);
	LocalSequencer->NotifyMovieSceneDataChanged(EMovieSceneDataChangeType::MovieSceneStructureItemAdded);
}

void FMovieSceneQTETrackEditor::HandleAddQTETrack()
{
	TSharedPtr<ISequencer> LocalSequencer = GetSequencer();
	UMovieScene* MovieScene = LocalSequencer.IsValid() ? LocalSequencer->GetFocusedMovieSceneSequence()->GetMovieScene() : nullptr;
	if (!MovieScene || MovieScene->IsReadOnly()) return;

	const FScopedTransaction Transaction(LOCTEXT("AddQTETrack_Transaction", "Add QTE Track"));
	MovieScene->Modify();

	UMovieSceneQTETrack* NewTrack = MovieScene->AddTrack<UMovieSceneQTETrack>();
	if (NewTrack)
	{
		LocalSequencer->NotifyMovieSceneDataChanged(EMovieSceneDataChangeType::MovieSceneStructureItemAdded);
	}
}

#undef LOCTEXT_NAMESPACE
