// Copyright Cinematic QTE System. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "CinematicQTETypes.h"
#include "QTEWidgetBase.generated.h"

class UQTETaskBase;
class UQTEDataAsset;

/**
 * QTE UI Widget 抽象基类。
 * 子类（含蓝图）通过 BP 事件接收进度/剩余时间更新。
 */
UCLASS(Abstract, Blueprintable)
class CINEMATICQTE_API UQTEWidgetBase : public UUserWidget
{
	GENERATED_BODY()

public:
	/**
	 * 由 Subsystem 在 AddToViewport 前调用；
	 * 绑定 Task 的委托，同步初始参数。
	 */
	virtual void InitializeFromTask(UQTETaskBase* InTask, UQTEDataAsset* InDataAsset);

	/** QTE 结束时调用，播放成功/失败反馈动画 */
	virtual void OnQTEFinished(EQTEResult Result);

	// ====== Blueprint Events ======

	UFUNCTION(BlueprintImplementableEvent, Category = "QTE", meta = (DisplayName = "On QTE Started"))
	void BP_OnQTEStarted(UQTEDataAsset* InDataAsset);

	UFUNCTION(BlueprintImplementableEvent, Category = "QTE", meta = (DisplayName = "On Progress Changed"))
	void BP_OnProgressChanged(float Progress);

	UFUNCTION(BlueprintImplementableEvent, Category = "QTE", meta = (DisplayName = "On Remaining Time Changed"))
	void BP_OnRemainingTimeChanged(float RemainingRatio);

	UFUNCTION(BlueprintImplementableEvent, Category = "QTE", meta = (DisplayName = "On Perfect Window Info"))
	void BP_OnPerfectWindowInfo(bool bInHasPerfectWindow, float InWindowStart, float InWindowEnd);

	UFUNCTION(BlueprintImplementableEvent, Category = "QTE", meta = (DisplayName = "On QTE Finished"))
	void BP_OnQTEFinished(EQTEResult Result);

protected:
	UFUNCTION()
	void HandleProgressChanged(float NewProgress);

	UFUNCTION()
	void HandleRemainingChanged(float NewRatio);

	UPROPERTY(BlueprintReadOnly, Transient, Category = "QTE")
	TObjectPtr<UQTEDataAsset> BoundDataAsset = nullptr;

	UPROPERTY(BlueprintReadOnly, Transient, Category = "QTE")
	TObjectPtr<UQTETaskBase> BoundTask = nullptr;

	/** 是否存在完美窗口（非 Tap 类型或未启用时为 false） */
	UPROPERTY(BlueprintReadOnly, Transient, Category = "QTE|Perfect Window")
	bool bHasPerfectWindow = false;

	/** 完美窗口起点（相对 Duration 的比例，0~1） */
	UPROPERTY(BlueprintReadOnly, Transient, Category = "QTE|Perfect Window")
	float PerfectWindowStart = 0.f;

	/** 完美窗口终点（相对 Duration 的比例，0~1） */
	UPROPERTY(BlueprintReadOnly, Transient, Category = "QTE|Perfect Window")
	float PerfectWindowEnd = 0.f;
};
