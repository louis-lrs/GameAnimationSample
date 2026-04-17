// Copyright Cinematic QTE System. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MovieSceneSection.h"
#include "CinematicQTETypes.h"
#include "MovieSceneQTESection.generated.h"

class UQTEDataAsset;

/**
 * Sequencer 中承载 QTE 配置的 Section。
 *
 * 两种模式由 bUseSectionRangeAsDuration 字段显式切换：
 *   - false（默认，AnimNotify 风格）：运行时长完全由 DataAsset.Duration 决定，
 *     Section 在时间轴上的宽度仅作视觉占位，不影响运行时。
 *   - true（AnimNotifyState 风格）：Section 长度即 QTE 运行时长，
 *     播放头越过终点时若 QTE 仍在运行按 Timeout 结束。
 *
 * Section 本身不使用 UE 原生的 IsLocked 机制来区分模式——IsLocked 会连带禁用拖动位置
 * 和 Details 面板编辑，不符合策划的交互预期。
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

	/** 冲突策略：触发时若已有 QTE 运行，如何处置 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "QTE")
	EQTEConflictPolicy ConflictPolicy = EQTEConflictPolicy::Ignore;

	/**
	 * 是否以 Section 自身长度作为 QTE 运行时长。
	 *   false（默认）：使用 DataAsset.Duration；
	 *   true         ：使用 Section 长度，播放头越过终点时若 QTE 仍在运行按 Timeout 结束。
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "QTE")
	bool bUseSectionRangeAsDuration = false;
};
