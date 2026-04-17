// Copyright Cinematic QTE System. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MovieSceneTrack.h"
#include "MovieSceneQTETrack.generated.h"

/**
 * Sequencer QTE Track：管理一组 QTE Section。
 */
UCLASS(MinimalAPI)
class UMovieSceneQTETrack : public UMovieSceneTrack
{
	GENERATED_BODY()

public:
	UMovieSceneQTETrack(const FObjectInitializer& ObjectInitializer);

	// ====== UMovieSceneTrack ======
	virtual bool SupportsType(TSubclassOf<UMovieSceneSection> SectionClass) const override;
	virtual UMovieSceneSection* CreateNewSection() override;
	virtual void AddSection(UMovieSceneSection& Section) override;
	virtual void RemoveSection(UMovieSceneSection& Section) override;
	virtual void RemoveSectionAt(int32 SectionIndex) override;
	virtual bool IsEmpty() const override;
	virtual bool HasSection(const UMovieSceneSection& Section) const override;
	virtual const TArray<UMovieSceneSection*>& GetAllSections() const override;
	virtual FMovieSceneEvalTemplatePtr CreateTemplateForSection(const UMovieSceneSection& InSection) const override;

#if WITH_EDITORONLY_DATA
	virtual FText GetDefaultDisplayName() const override;
#endif

private:
	UPROPERTY()
	TArray<TObjectPtr<UMovieSceneSection>> Sections;
};
