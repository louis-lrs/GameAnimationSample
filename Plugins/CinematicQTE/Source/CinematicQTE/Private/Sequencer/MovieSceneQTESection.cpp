// Copyright Cinematic QTE System. All Rights Reserved.

#include "Sequencer/MovieSceneQTESection.h"

UMovieSceneQTESection::UMovieSceneQTESection(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	EvalOptions.EnableAndSetCompletionMode(EMovieSceneCompletionMode::RestoreState);
}
