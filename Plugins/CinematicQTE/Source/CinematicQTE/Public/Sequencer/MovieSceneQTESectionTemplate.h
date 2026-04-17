// Copyright Cinematic QTE System. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Evaluation/MovieSceneEvalTemplate.h"
#include "CinematicQTETypes.h"
#include "MovieSceneQTESectionTemplate.generated.h"

class UMovieSceneQTESection;
class UQTEDataAsset;

/**
 * QTE Section 的评估模板。
 * Player 越过 Section 起始帧时调用 Subsystem StartQTE。
 * Range 模式（bUseRange=true，来源于 Section.bUseSectionRangeAsDuration）下，
 * Player 越过 Section 结束帧时若 QTE 仍在运行则以 Timeout 结束。
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

	/** Section 结束帧（仅 bUseRange=true 时有效） */
	UPROPERTY()
	FFrameNumber EndFrame;

	UPROPERTY()
	EQTEConflictPolicy ConflictPolicy = EQTEConflictPolicy::Ignore;

	/**
	 * Section 长度语义，直接来源于 Section.bUseSectionRangeAsDuration：
	 *   - false → Key 模式：使用 DataAsset.Duration，Section 长度忽略
	 *   - true  → Range 模式：使用 Section 长度作为运行时长
	 */
	UPROPERTY()
	bool bUseRange = false;

	/** Section 唯一签名，作为运行时 QTE 的 Owner 标识 */
	UPROPERTY()
	FGuid SectionSignature;

	virtual UScriptStruct& GetScriptStructImpl() const override
	{
		return *StaticStruct();
	}
	virtual void Evaluate(const FMovieSceneEvaluationOperand& Operand,
	                      const FMovieSceneContext& Context,
	                      const FPersistentEvaluationData& PersistentData,
	                      FMovieSceneExecutionTokens& ExecutionTokens) const override;
};
