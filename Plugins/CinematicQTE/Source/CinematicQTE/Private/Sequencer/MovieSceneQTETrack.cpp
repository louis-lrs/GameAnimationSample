// Copyright Cinematic QTE System. All Rights Reserved.

#include "Sequencer/MovieSceneQTETrack.h"
#include "Sequencer/MovieSceneQTESection.h"
#include "Sequencer/MovieSceneQTESectionTemplate.h"

UMovieSceneQTETrack::UMovieSceneQTETrack(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
#if WITH_EDITORONLY_DATA
	TrackTint = FColor(210, 60, 60, 200);
#endif
}

bool UMovieSceneQTETrack::SupportsType(TSubclassOf<UMovieSceneSection> SectionClass) const
{
	return SectionClass == UMovieSceneQTESection::StaticClass();
}

UMovieSceneSection* UMovieSceneQTETrack::CreateNewSection()
{
	return NewObject<UMovieSceneQTESection>(this, NAME_None, RF_Transactional);
}

void UMovieSceneQTETrack::AddSection(UMovieSceneSection& Section)
{
	Sections.Add(&Section);
}

void UMovieSceneQTETrack::RemoveSection(UMovieSceneSection& Section)
{
	Sections.Remove(&Section);
}

void UMovieSceneQTETrack::RemoveSectionAt(int32 SectionIndex)
{
	if (Sections.IsValidIndex(SectionIndex))
	{
		Sections.RemoveAt(SectionIndex);
	}
}

bool UMovieSceneQTETrack::IsEmpty() const
{
	return Sections.Num() == 0;
}

bool UMovieSceneQTETrack::HasSection(const UMovieSceneSection& Section) const
{
	return Sections.Contains(&Section);
}

const TArray<UMovieSceneSection*>& UMovieSceneQTETrack::GetAllSections() const
{
	return reinterpret_cast<const TArray<UMovieSceneSection*>&>(Sections);
}

FMovieSceneEvalTemplatePtr UMovieSceneQTETrack::CreateTemplateForSection(const UMovieSceneSection& InSection) const
{
	if (const UMovieSceneQTESection* QTESection = Cast<UMovieSceneQTESection>(&InSection))
	{
		return FMovieSceneQTESectionTemplate(*QTESection);
	}
	return FMovieSceneEvalTemplatePtr();
}

#if WITH_EDITORONLY_DATA
FText UMovieSceneQTETrack::GetDisplayName() const
{
	return NSLOCTEXT("CinematicQTE", "QTETrackDisplayName", "QTE Track");
}
#endif
