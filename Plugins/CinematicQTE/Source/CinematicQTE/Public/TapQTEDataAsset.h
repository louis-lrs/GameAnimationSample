// Copyright Cinematic QTE System. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "QTEDataAsset.h"
#include "TapQTEDataAsset.generated.h"

/**
 * 单点时机触发型 QTE 配置。
 * 玩家需在完美窗口内按键即判定成功。
 */
UCLASS(BlueprintType)
class CINEMATICQTE_API UTapQTEDataAsset : public UQTEDataAsset
{
	GENERATED_BODY()

public:
	UTapQTEDataAsset();

	/** 是否启用"完美窗口"判定；关闭后整段 Duration 都算成功 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "QTE|Tap")
	bool bUsePerfectWindow = true;

	/** 完美窗口起点（相对 Duration 的比例，0~1） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "QTE|Tap",
		meta = (ClampMin = "0.0", ClampMax = "1.0", EditCondition = "bUsePerfectWindow"))
	float PerfectWindowStart = 0.4f;

	/** 完美窗口终点（相对 Duration 的比例，0~1） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "QTE|Tap",
		meta = (ClampMin = "0.0", ClampMax = "1.0", EditCondition = "bUsePerfectWindow"))
	float PerfectWindowEnd = 0.6f;

	virtual bool IsValidConfig(FString& OutReason) const override;
};
