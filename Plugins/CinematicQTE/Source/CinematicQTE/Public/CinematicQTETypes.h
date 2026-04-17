// Copyright Cinematic QTE System. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "CinematicQTETypes.generated.h"

class UQTEDataAsset;

/** QTE 类型 */
UENUM(BlueprintType)
enum class EQTEType : uint8
{
	/** 连点累积进度型 */
	Mash		UMETA(DisplayName = "Mash (Rapid Press)"),
	/** 单次时机点击型 */
	Tap			UMETA(DisplayName = "Tap (Single Press)"),
	/** 自定义扩展类型 */
	Custom		UMETA(DisplayName = "Custom")
};

/** QTE 结果 */
UENUM(BlueprintType)
enum class EQTEResult : uint8
{
	/** 未结束 */
	None		UMETA(DisplayName = "None"),
	/** 成功 */
	Success		UMETA(DisplayName = "Success"),
	/** 失败（错误按键或进度不达标） */
	Failure		UMETA(DisplayName = "Failure"),
	/** 超时未操作 */
	Timeout		UMETA(DisplayName = "Timeout"),
	/** 外部取消（切关、强停等） */
	Cancelled	UMETA(DisplayName = "Cancelled")
};

/** QTE 运行状态 */
UENUM(BlueprintType)
enum class EQTETaskState : uint8
{
	Idle		UMETA(DisplayName = "Idle"),
	Running		UMETA(DisplayName = "Running"),
	Finished	UMETA(DisplayName = "Finished")
};

/** 新 QTE 到来时，如果当前已有 QTE 在进行的处理策略 */
UENUM(BlueprintType)
enum class EQTEConflictPolicy : uint8
{
	/** 忽略新触发 */
	Ignore		UMETA(DisplayName = "Ignore New"),
	/** 放入队列等待 */
	Queue		UMETA(DisplayName = "Queue"),
	/** 取消当前并替换为新的 */
	Replace		UMETA(DisplayName = "Replace Current")
};

/** QTE 结束时的结果元数据 */
USTRUCT(BlueprintType)
struct CINEMATICQTE_API FQTEResultMeta
{
	GENERATED_BODY()

	/** 实际消耗时间（秒，真实时间，不受慢速影响） */
	UPROPERTY(BlueprintReadOnly, Category = "QTE|Meta")
	float ElapsedTime = 0.f;

	/** 实际有效按压次数（Mash 模式使用） */
	UPROPERTY(BlueprintReadOnly, Category = "QTE|Meta")
	int32 PressCount = 0;

	/** 最终进度（0.0 ~ 1.0） */
	UPROPERTY(BlueprintReadOnly, Category = "QTE|Meta")
	float FinalProgress = 0.f;

	/** 按下时的剩余时间比例（Tap 模式使用） */
	UPROPERTY(BlueprintReadOnly, Category = "QTE|Meta")
	float PressTimingRatio = 0.f;
};

/** 通用多播委托 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnQTEProgressChanged, float, NewProgress);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnQTERemainingTimeChanged, float, RemainingRatio);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnQTEFinished, EQTEResult, Result, UQTEDataAsset*, DataAsset, FQTEResultMeta, Meta);
