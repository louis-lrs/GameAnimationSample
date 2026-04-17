// Copyright Cinematic QTE System. All Rights Reserved.

#include "QTETaskBase.h"
#include "CinematicQTEModule.h"
#include "QTEDataAsset.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"

UQTETaskBase::UQTETaskBase()
{
}

void UQTETaskBase::StartQTE(UWorld* InWorld, APlayerController* InPC, UQTEDataAsset* InDataAsset,
	float InOverrideDuration /*= -1.f*/)
{
	if (!ensureAlwaysMsgf(TaskState == EQTETaskState::Idle,
		TEXT("QTETask[%s] StartQTE called on non-Idle task (state=%d). Caller must Finish/Cancel previous task first."),
		*GetName(), (int32)TaskState))
	{
		return;
	}

	if (!ensureAlwaysMsgf(IsValid(InDataAsset),
		TEXT("QTETask[%s] StartQTE: DataAsset is null."), *GetName()))
	{
		return;
	}

	FString Reason;
	if (!InDataAsset->IsValidConfig(Reason))
	{
		UE_LOG(LogCinematicQTE, Error, TEXT("QTETask[%s] StartQTE: invalid config - %s"),
			*GetName(), *Reason);
		return;
	}

	WorldRef = InWorld;
	PCRef = InPC;
	DataAsset = InDataAsset;
	// 优先使用外部覆盖时长（Sequencer B 模式），否则走 DataAsset.Duration
	TotalDuration = (InOverrideDuration > 0.f) ? InOverrideDuration : InDataAsset->Duration;
	RemainingTime = TotalDuration;
	CurrentProgress = 0.f;
	ElapsedRealTime = 0.f;
	ResultMeta = FQTEResultMeta();
	LastBroadcastProgress = -1.f;
	LastBroadcastRatio = -1.f;
	TaskState = EQTETaskState::Running;

	UE_LOG(LogCinematicQTE, Log, TEXT("QTETask[%s] Started. Asset=%s Duration=%.2f (Override=%.2f)"),
		*GetName(), *InDataAsset->GetName(), TotalDuration, InOverrideDuration);

	OnStartQTE();

	// 立即广播一次初始值
	BroadcastProgress(CurrentProgress);
	BroadcastRemaining(GetRemainingRatio());
}

void UQTETaskBase::TickQTE(float DeltaTime)
{
	if (TaskState != EQTETaskState::Running)
	{
		return;
	}

	// 处理强制结果（调试）
	if (ForcedResult.IsSet())
	{
		const EQTEResult R = ForcedResult.GetValue();
		ForcedResult.Reset();
		FinishQTE(R);
		return;
	}

	ElapsedRealTime += DeltaTime;
	RemainingTime = FMath::Max(0.f, TotalDuration - ElapsedRealTime);

	OnTickQTE(DeltaTime);

	BroadcastRemaining(GetRemainingRatio());

	// 若仍在 Running 且超时
	if (TaskState == EQTETaskState::Running && RemainingTime <= 0.f)
	{
		// 派生类可能已在 OnTickQTE 中提前结束；这里作为兜底
		FinishQTE(EQTEResult::Timeout);
	}
}

void UQTETaskBase::HandleInput(const FInputActionValue& Value)
{
	if (TaskState != EQTETaskState::Running)
	{
		return;
	}
	OnHandleInput(Value);
}

void UQTETaskBase::FinishQTE(EQTEResult Result)
{
	if (TaskState == EQTETaskState::Finished)
	{
		return;
	}

	TaskState = EQTETaskState::Finished;
	LastResult = Result;

	// 填充元数据
	ResultMeta.ElapsedTime = ElapsedRealTime;
	ResultMeta.FinalProgress = CurrentProgress;

	UE_LOG(LogCinematicQTE, Log, TEXT("QTETask[%s] Finished. Result=%d Elapsed=%.2fs Progress=%.2f"),
		*GetName(), (int32)Result, ElapsedRealTime, CurrentProgress);

	OnFinishQTE(Result);

	// 最终广播
	BroadcastProgress(CurrentProgress);
	OnQTEFinished.Broadcast(Result, DataAsset, ResultMeta);
}

void UQTETaskBase::CancelQTE()
{
	if (TaskState == EQTETaskState::Running)
	{
		FinishQTE(EQTEResult::Cancelled);
	}
}

float UQTETaskBase::GetRemainingRatio() const
{
	return (TotalDuration > 0.f) ? FMath::Clamp(RemainingTime / TotalDuration, 0.f, 1.f) : 0.f;
}

void UQTETaskBase::BroadcastProgress(float NewProgress)
{
	NewProgress = FMath::Clamp(NewProgress, 0.f, 1.f);
	CurrentProgress = NewProgress;
	if (LastBroadcastProgress < 0.f || FMath::Abs(NewProgress - LastBroadcastProgress) > BroadcastEpsilon
		|| FMath::IsNearlyEqual(NewProgress, 0.f) || FMath::IsNearlyEqual(NewProgress, 1.f))
	{
		LastBroadcastProgress = NewProgress;
		OnProgressChanged.Broadcast(NewProgress);
	}
}

void UQTETaskBase::BroadcastRemaining(float NewRatio)
{
	NewRatio = FMath::Clamp(NewRatio, 0.f, 1.f);
	if (LastBroadcastRatio < 0.f || FMath::Abs(NewRatio - LastBroadcastRatio) > BroadcastEpsilon
		|| FMath::IsNearlyZero(NewRatio))
	{
		LastBroadcastRatio = NewRatio;
		OnRemainingTimeChanged.Broadcast(NewRatio);
	}
}
