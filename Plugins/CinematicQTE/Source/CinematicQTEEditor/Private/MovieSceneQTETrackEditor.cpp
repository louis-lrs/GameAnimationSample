// Copyright Cinematic QTE System. All Rights Reserved.

#include "MovieSceneQTETrackEditor.h"
#include "Sequencer/MovieSceneQTETrack.h"
#include "Sequencer/MovieSceneQTESection.h"
#include "QTESectionInterface.h"
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

TSharedRef<ISequencerSection> FMovieSceneQTETrackEditor::MakeSectionInterface(UMovieSceneSection& SectionObject,
	UMovieSceneTrack& Track, FGuid ObjectBinding)
{
	return MakeShared<FQTESection>(SectionObject, GetSequencer());
}

void FMovieSceneQTETrackEditor::BuildTrackContextMenu(FMenuBuilder& MenuBuilder, UMovieSceneTrack* Track)
{
	MenuBuilder.BeginSection("QTE", LOCTEXT("QTESectionHeader", "QTE"));
	{
		MenuBuilder.AddMenuEntry(
			LOCTEXT("AddQTESection", "Add QTE Section"),
			LOCTEXT("AddQTESectionTooltip",
				"Add a QTE section at the current playhead. "
				"Default runtime duration is driven by DataAsset.Duration; "
				"toggle 'Use Section Range As Duration' on the section to drive it by Section length instead."),
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

	// Section 默认长度 0.25s，便于在时间轴上可见且便于拖拽；
	// 运行时长语义由 Section.bUseSectionRangeAsDuration 决定，与视觉长度解耦。
	const FFrameNumber CurrentFrame = LocalSequencer->GetLocalTime().Time.FrameNumber;
	const FFrameRate TickResolution = MovieScene->GetTickResolution();
	const FFrameNumber Length = (0.25 * TickResolution).FrameNumber;

	UMovieSceneQTESection* NewSection = Cast<UMovieSceneQTESection>(QTETrack->CreateNewSection());
	if (!NewSection)
	{
		return;
	}

	NewSection->Modify();
	NewSection->SetRange(TRange<FFrameNumber>(CurrentFrame, CurrentFrame + Length));

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