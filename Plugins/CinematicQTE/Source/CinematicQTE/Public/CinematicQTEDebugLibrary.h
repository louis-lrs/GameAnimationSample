// Copyright Cinematic QTE System. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CinematicQTETypes.h"
#include "CinematicQTEDebugLibrary.generated.h"

class ULevelSequencePlayer;
class UQTEDataAsset;

/**
 * 供蓝图与控制台使用的 QTE 便捷函数库。
 */
UCLASS()
class CINEMATICQTE_API UCinematicQTEDebugLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** 手动启动一个 QTE（便捷蓝图节点） */
	UFUNCTION(BlueprintCallable, Category = "QTE|Debug", meta = (WorldContext = "WorldContext"))
	static bool StartQTE(const UObject* WorldContext, UQTEDataAsset* DataAsset, ULevelSequencePlayer* SequencePlayer,
		EQTEConflictPolicy Policy = EQTEConflictPolicy::Ignore);

	/** 取消当前 QTE */
	UFUNCTION(BlueprintCallable, Category = "QTE|Debug", meta = (WorldContext = "WorldContext"))
	static void CancelCurrentQTE(const UObject* WorldContext, EQTEResult Result = EQTEResult::Cancelled);

	/** 查询当前是否有 QTE 在进行 */
	UFUNCTION(BlueprintPure, Category = "QTE|Debug", meta = (WorldContext = "WorldContext"))
	static bool IsQTEActive(const UObject* WorldContext);

	/** 获取当前 Sequence PlayRate（调试用） */
	UFUNCTION(BlueprintPure, Category = "QTE|Debug", meta = (WorldContext = "WorldContext"))
	static float GetCurrentPlayRate(const UObject* WorldContext);
};
