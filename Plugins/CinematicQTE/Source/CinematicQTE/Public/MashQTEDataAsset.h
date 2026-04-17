// Copyright Cinematic QTE System. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "QTEDataAsset.h"
#include "MashQTEDataAsset.generated.h"

/**
 * 连点累积进度型 QTE 配置。
 * 玩家需在 Duration 内连续按键累积进度至 1.0 即成功。
 */
UCLASS(BlueprintType)
class CINEMATICQTE_API UMashQTEDataAsset : public UQTEDataAsset
{
	GENERATED_BODY()

public:
	UMashQTEDataAsset();

	/** 达成成功所需的按压次数 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "QTE|Mash", meta = (ClampMin = "1"))
	int32 RequiredPressCount = 10;

	/**
	 * 单次按压贡献的进度（0.0~1.0）。
	 * 为 0 时自动取 1.0 / RequiredPressCount。
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "QTE|Mash", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float ProgressPerPress = 0.f;

	/** 每秒自动衰减值（0 表示不衰减） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "QTE|Mash", meta = (ClampMin = "0.0"))
	float ProgressDecayRate = 0.f;

	/** 两次有效按压之间的最小时间间隔（防按键宏） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "QTE|Mash", meta = (ClampMin = "0.0"))
	float MinPressInterval = 0.05f;

	/** 获取单次贡献值（若 ProgressPerPress 为 0 则返回 1/Count） */
	UFUNCTION(BlueprintCallable, Category = "QTE|Mash")
	float GetEffectiveProgressPerPress() const;

	virtual bool IsValidConfig(FString& OutReason) const override;
};
