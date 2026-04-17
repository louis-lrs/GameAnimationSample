// Copyright Cinematic QTE System. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "CinematicQTETypes.h"
#include "QTETaskBase.generated.h"

class UQTEDataAsset;
class UWorld;
class APlayerController;
class UInputAction;
class UEnhancedInputComponent;
struct FInputActionValue;

/**
 * QTE 任务抽象基类。
 * 管理单个 QTE 的生命周期、剩余时间、进度广播。
 * 派生类需实现 OnStartQTE/OnTickQTE/OnHandleInput/OnFinishQTE。
 */
UCLASS(Abstract, BlueprintType, Blueprintable)
class CINEMATICQTE_API UQTETaskBase : public UObject
{
	GENERATED_BODY()

public:
	UQTETaskBase();

	// ====== Delegates ======

	UPROPERTY(BlueprintAssignable, Category = "QTE|Events")
	FOnQTEProgressChanged OnProgressChanged;

	UPROPERTY(BlueprintAssignable, Category = "QTE|Events")
	FOnQTERemainingTimeChanged OnRemainingTimeChanged;

	UPROPERTY(BlueprintAssignable, Category = "QTE|Events")
	FOnQTEFinished OnQTEFinished;

	// ====== Lifecycle ======

	/** 开始 QTE */
	virtual void StartQTE(UWorld* InWorld, APlayerController* InPC, UQTEDataAsset* InDataAsset);

	/** 每帧更新（由 Subsystem 驱动） */
	virtual void TickQTE(float DeltaTime);

	/** 输入事件回调（由 Subsystem 路由过来） */
	virtual void HandleInput(const FInputActionValue& Value);

	/** 结束 QTE */
	virtual void FinishQTE(EQTEResult Result);

	/** 外部取消 */
	virtual void CancelQTE();

	// ====== Accessors ======

	UFUNCTION(BlueprintPure, Category = "QTE")
	EQTETaskState GetTaskState() const { return TaskState; }

	UFUNCTION(BlueprintPure, Category = "QTE")
	float GetCurrentProgress() const { return CurrentProgress; }

	UFUNCTION(BlueprintPure, Category = "QTE")
	float GetRemainingTime() const { return RemainingTime; }

	UFUNCTION(BlueprintPure, Category = "QTE")
	float GetRemainingRatio() const;

	UFUNCTION(BlueprintPure, Category = "QTE")
	UQTEDataAsset* GetDataAsset() const { return DataAsset; }

	UFUNCTION(BlueprintPure, Category = "QTE")
	const FQTEResultMeta& GetResultMeta() const { return ResultMeta; }

	/** 调试用：强制指定下一次结束时的结果 */
	void SetForcedResult(EQTEResult InForcedResult) { ForcedResult = InForcedResult; }

protected:
	// ====== 派生类实现 ======

	virtual void OnStartQTE() {}
	virtual void OnTickQTE(float DeltaTime) {}
	virtual void OnHandleInput(const FInputActionValue& Value) {}
	virtual void OnFinishQTE(EQTEResult Result) {}

	/** 广播进度（带节流） */
	void BroadcastProgress(float NewProgress);

	/** 广播剩余时间比例（带节流） */
	void BroadcastRemaining(float NewRatio);

protected:
	UPROPERTY(Transient)
	TObjectPtr<UQTEDataAsset> DataAsset = nullptr;

	UPROPERTY(Transient)
	TWeakObjectPtr<UWorld> WorldRef;

	UPROPERTY(Transient)
	TWeakObjectPtr<APlayerController> PCRef;

	UPROPERTY(Transient)
	EQTETaskState TaskState = EQTETaskState::Idle;

	UPROPERTY(Transient)
	float CurrentProgress = 0.f;

	UPROPERTY(Transient)
	float RemainingTime = 0.f;

	UPROPERTY(Transient)
	float TotalDuration = 0.f;

	UPROPERTY(Transient)
	float ElapsedRealTime = 0.f;

	UPROPERTY(Transient)
	FQTEResultMeta ResultMeta;

	/** 上次广播的进度值，用于节流 */
	float LastBroadcastProgress = -1.f;

	/** 上次广播的剩余时间比例 */
	float LastBroadcastRatio = -1.f;

	/** 广播最小间隔（比例差） */
	static constexpr float BroadcastEpsilon = 0.005f;

	/** 调试强制结果 */
	TOptional<EQTEResult> ForcedResult;
};
