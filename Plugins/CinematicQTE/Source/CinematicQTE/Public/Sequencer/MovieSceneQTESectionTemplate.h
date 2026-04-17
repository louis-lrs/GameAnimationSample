// Copyright Cinematic QTE System. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Evaluation/MovieSceneEvalTemplate.h"
#include "MovieSceneQTESectionTemplate.generated.h"

class UMovieSceneQTESection;
class UQTEDataAsset;

/**
 * QTE Section 的评估模板。
 * 在 Player 越过 Section 起始帧时调用 Subsystem StartQTE。
 */
USTRUCT()
struct FMovieSceneQTESectionTemplate : public FMovieSceneEvalTemplate
{
	GENERATED_BODY()

	FMovieSceneQTESectionTemplate() = default;
	FMovieSceneQTESectionTemplate(const UMovieSceneQTESection& InSection);

	UPROPERTY()
	TObjectPtr<UQTEDataAsset> QTEDataAsset = nullptr;

	UPROPERTY()
	FFrameNumber StartFrame;

	UPROPERTY()
	uint8 ConflictPolicy = 0;

	virtual UScriptStruct& GetScriptStructImpl() const override
	{
		return *StaticStruct();
	}
	virtual void Evaluate(const FMovieSceneEvaluationOperand& Operand,
	                      const FMovieSceneContext& Context,
	                      const FPersistentEvaluationData& PersistentData,
	                      FMovieSceneExecutionTokens& ExecutionTokens) const override;
};
