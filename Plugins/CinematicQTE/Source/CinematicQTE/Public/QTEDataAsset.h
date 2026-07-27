// Copyright Cinematic QTE System. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "CinematicQTETypes.h"
#include "StructUtils/InstancedStruct.h"
#include "QTEDataAsset.generated.h"

class UInputAction;
class UQTEWidgetBase;
class UQTETaskBase;
class UCurveFloat;

/**
 * QTE 配置数据资产基类。
 * 策划在编辑器中通过派生资产（Mash/Tap/Custom）配置 QTE 参数。
 */
UCLASS(BlueprintType, Abstract, EditInlineNew)
class CINEMATICQTE_API UQTEDataAsset : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UQTEDataAsset();

	// ====== 基础信息 ======

	/** QTE 类型（只读，由具体子类设定） */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "QTE|Basic")
	EQTEType QTEType = EQTEType::Custom;

	/** QTE 持续时间（秒，真实时间，不受慢速影响） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "QTE|Basic", meta = (ClampMin = "0.1"))
	float Duration = 3.0f;

	/** 玩家输入所用的 InputAction */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "QTE|Basic")
	TObjectPtr<UInputAction> InputAction = nullptr;

	// ====== 动画速率控制 ======

	/** QTE 触发期间的过场动画播放速率 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "QTE|SlowMotion",
		meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float SlowMotionRate = 0.01f;

	/** 进入慢速的平滑过渡时长（秒） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "QTE|SlowMotion",
		meta = (ClampMin = "0.0"))
	float SlowDownBlendTime = 0.2f;

	/** 恢复正常速率的平滑过渡时长（秒） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "QTE|SlowMotion",
		meta = (ClampMin = "0.0"))
	float SpeedUpBlendTime = 0.3f;

	/** 可选的插值曲线；为空时使用线性插值 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "QTE|SlowMotion")
	TObjectPtr<UCurveFloat> BlendCurve = nullptr;

	// ====== UI ======

	/** QTE 对应的 UI Widget 类；为空则不创建 UI */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "QTE|UI")
	TSubclassOf<UQTEWidgetBase> WidgetClass;

	/** Viewport 层级（Z-Order） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "QTE|UI")
	int32 WidgetZOrder = 100;

	/** 提示文本 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "QTE|UI")
	FText DisplayText;

	/** 提示图标 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "QTE|UI")
	TSoftObjectPtr<UTexture2D> DisplayIcon;

	/** 结果反馈动画播放时长（结束后再移除 Widget） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "QTE|UI", meta = (ClampMin = "0.0"))
	float FeedbackDuration = 0.5f;

	// ====== 结果扩展事件（InstancedStruct，支持蓝图派生） ======

	/** 成功时执行的事件数据（可在派生项目中自定义结构） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "QTE|Events", meta = (BaseStruct = "/Script/CoreUObject.Object"))
	FInstancedStruct SuccessEvent;

	/** 失败时执行的事件数据 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "QTE|Events", meta = (BaseStruct = "/Script/CoreUObject.Object"))
	FInstancedStruct FailureEvent;

	// ====== 任务类配置 ======

	/** 对应的 Task 类；由具体子类设定，也允许策划在蓝图派生中指定 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "QTE|Advanced")
	TSubclassOf<UQTETaskBase> TaskClass;

	// ====== 校验 ======

	/**
	 * 运行时校验配置是否有效。
	 * @param OutReason 无效时返回具体原因
	 */
	UFUNCTION(BlueprintCallable, Category = "QTE")
	virtual bool IsValidConfig(FString& OutReason) const;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
};
