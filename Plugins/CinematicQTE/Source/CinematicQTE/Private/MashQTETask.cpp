// Copyright Cinematic QTE System. All Rights Reserved.

#include "MashQTETask.h"
#include "MashQTEDataAsset.h"
#include "CinematicQTEModule.h"
#include "InputActionValue.h"

UMashQTETask::UMashQTETask()
{
}

void UMashQTETask::OnStartQTE()
{
	MashData = Cast<UMashQTEDataAsset>(DataAsset);
	if (!MashData)
	{
		UE_LOG(LogCinematicQTE, Error, TEXT("MashQTETask requires UMashQTEDataAsset, got %s"),
			DataAsset ? *DataAsset->GetClass()->GetName() : TEXT("null"));
		FinishQTE(EQTEResult::Cancelled);
		return;
	}
	LastPressTime = -1.f;
	ResultMeta.PressCount = 0;
}

void UMashQTETask::OnTickQTE(float DeltaTime)
{
	if (!MashData)
	{
		return;
	}

	// 进度衰减
	if (MashData->ProgressDecayRate > 0.f && CurrentProgress > 0.f)
	{
		const float NewProgress = FMath::Max(0.f, CurrentProgress - MashData->ProgressDecayRate * DeltaTime);
		BroadcastProgress(NewProgress);
	}
}

void UMashQTETask::OnHandleInput(const FInputActionValue& Value)
{
	if (!MashData || TaskState != EQTETaskState::Running)
	{
		return;
	}

	// 防连发：检查两次按压间隔
	if (LastPressTime >= 0.f)
	{
		const float Interval = ElapsedRealTime - LastPressTime;
		if (Interval < MashData->MinPressInterval)
		{
			UE_LOG(LogCinematicQTE, Verbose, TEXT("MashQTE: press ignored (interval=%.3f < min=%.3f)"),
				Interval, MashData->MinPressInterval);
			return;
		}
	}
	LastPressTime = ElapsedRealTime;

	++ResultMeta.PressCount;

	const float Delta = MashData->GetEffectiveProgressPerPress();
	const float NewProgress = FMath::Clamp(CurrentProgress + Delta, 0.f, 1.f);
	BroadcastProgress(NewProgress);

	if (NewProgress >= 1.f)
	{
		FinishQTE(EQTEResult::Success);
	}
}
