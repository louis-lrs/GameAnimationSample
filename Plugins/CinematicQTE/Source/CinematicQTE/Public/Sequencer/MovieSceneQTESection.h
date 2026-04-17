// Copyright Cinematic QTE System. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MovieSceneSection.h"
#include "MovieSceneQTESection.generated.h"

class UQTEDataAsset;

/**
 * Sequencer 中承载 QTE 配置的 Section。
 * Section 的起始帧即为 QTE 触发时刻。
 */
UCLASS(MinimalAPI)
class UMovieSceneQTESection : public UMovieSceneSection
{
	GENERATED_BODY()

public:
	UMovieSceneQTESection(const FObjectInitializer& ObjectInitializer);

	/** 该 Section 对应的 QTE 配置 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "QTE")
	TObjectPtr<UQTEDataAsset> QTEDataAsset = nullptr;

	/** 冲突策略 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "QTE")
	uint8 ConflictPolicy = 0; // EQTEConflictPolicy::Ignore
};
