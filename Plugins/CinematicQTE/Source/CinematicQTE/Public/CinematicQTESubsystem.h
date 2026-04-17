// Copyright Cinematic QTE System. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Tickable.h"
#include "CinematicQTETypes.h"
#include "SequencePlayRateController.h"
#include "InputMappingContext.h"
#include "CinematicQTESubsystem.generated.h"

class UQTEDataAsset;
class UQTETaskBase;
class UQTEWidgetBase;
class ULevelSequencePlayer;
class APlayerController;
class UInputMappingContext;
class UEnhancedInputComponent;

/** QTE 全局完成委托（带 Subsystem 上下文） */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnGlobalQTEFinished, EQTEResult, Result, UQTEDataAsset*, DataAsset, FQTEResultMeta, Meta);

/**
 * 过场动画 QTE 子系统（WorldSubsystem）。
 * 统一负责 QTE 的生命周期、动画速率控制、UI/输入接入。
 *
 * 典型用法：
 *   UCinematicQTESubsystem::Get(World)->StartQTE(DataAsset, SequencePlayer);
 */
UCLASS()
class CINEMATICQTE_API UCinematicQTESubsystem : public UWorldSubsystem, public FTickableGameObject
{
	GENERATED_BODY()

public:
	// ====== USubsystem ======
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;

	// ====== FTickableGameObject ======
	virtual void Tick(float DeltaTime) override;
	virtual bool IsTickable() const override;
	virtual bool IsTickableWhenPaused() const override { return false; }
	virtual TStatId GetStatId() const override;

	// ====== Public API ======

	/** 便捷访问器 */
	UFUNCTION(BlueprintCallable, Category = "QTE", meta = (WorldContext = "WorldContext"))
	static UCinematicQTESubsystem* Get(const UObject* WorldContext);

	/**
	 * 启动一个 QTE。
	 * @param InDataAsset  QTE 配置
	 * @param InPlayer     关联的 Sequence Player（可空，为空则不做速率控制）
	 * @param InConflictPolicy 冲突策略；默认 Ignore
	 * @return 是否成功启动
	 */
	UFUNCTION(BlueprintCallable, Category = "QTE")
	bool StartQTE(UQTEDataAsset* InDataAsset, ULevelSequencePlayer* InPlayer,
		EQTEConflictPolicy InConflictPolicy = EQTEConflictPolicy::Ignore);

	/** 取消当前 QTE（带结果分类） */
	UFUNCTION(BlueprintCallable, Category = "QTE")
	void CancelCurrentQTE(EQTEResult Result = EQTEResult::Cancelled);

	UFUNCTION(BlueprintPure, Category = "QTE")
	bool IsQTEActive() const;

	UFUNCTION(BlueprintPure, Category = "QTE")
	UQTETaskBase* GetCurrentTask() const { return CurrentTask; }

	UFUNCTION(BlueprintPure, Category = "QTE")
	UQTEWidgetBase* GetCurrentWidget() const { return CurrentWidget; }

	/** 全局 QTE 结束事件（订阅此事件可接入剧情分支等） */
	UPROPERTY(BlueprintAssignable, Category = "QTE|Events")
	FOnGlobalQTEFinished OnGlobalQTEFinished;

	/** 获取速率控制器（调试用） */
	float GetCurrentPlayRate() const { return PlayRateController.GetCurrentRate(); }

private:
	void HandleTaskFinished(EQTEResult Result, UQTEDataAsset* InDataAsset, FQTEResultMeta Meta);

	void CreateAndShowWidget(UQTEDataAsset* InDataAsset);
	void DestroyWidgetWithFeedback(float DelayTime);

	void BindQTEInput(APlayerController* PC, UQTEDataAsset* InDataAsset);
	void UnbindQTEInput();

	APlayerController* ResolvePlayerController() const;

	void CheckForcedDebugResult();

private:
	UPROPERTY(Transient)
	TObjectPtr<UQTETaskBase> CurrentTask = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UQTEWidgetBase> CurrentWidget = nullptr;

	UPROPERTY(Transient)
	TWeakObjectPtr<ULevelSequencePlayer> CurrentSequencePlayer;

	/** 动态 InputMappingContext */
	UPROPERTY(Transient)
	TObjectPtr<UInputMappingContext> DynamicMappingContext = nullptr;

	/** 绑定到 EnhancedInputComponent 的 BindingHandle */
	TArray<uint32> InputBindingHandles;

	UPROPERTY(Transient)
	TWeakObjectPtr<UEnhancedInputComponent> BoundInputComponent;

	/** 速率控制器 */
	FSequencePlayRateController PlayRateController;

	/** 待处理的 QTE 队列（Queue 策略使用） */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UQTEDataAsset>> QueuedAssets;

	/** Widget 移除计时器 */
	FTimerHandle WidgetRemovalTimer;

	/** 缓存最近一次 Widget 反馈时长 */
	float PendingFeedbackDuration = 0.f;
};
