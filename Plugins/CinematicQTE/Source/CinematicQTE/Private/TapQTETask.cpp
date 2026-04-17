// Copyright Cinematic QTE System. All Rights Reserved.

#include "TapQTETask.h"
#include "TapQTEDataAsset.h"
#include "CinematicQTEModule.h"
#include "InputActionValue.h"

UTapQTETask::UTapQTETask()
{
}

void UTapQTETask::OnStartQTE()
{
	TapData = Cast<UTapQTEDataAsset>(DataAsset);
	if (!ensureAlwaysMsgf(TapData != nullptr,
		TEXT("TapQTETask requires UTapQTEDataAsset, got %s. Check DataAsset.TaskClass."),
		DataAsset ? *DataAsset->GetClass()->GetName() : TEXT("null")))
	{
		FinishQTE(EQTEResult::Cancelled);
		return;
	}
	bInputHandled = false;
}

void UTapQTETask::OnHandleInput(const FInputActionValue& Value)
{
	if (!TapData || bInputHandled || TaskState != EQTETaskState::Running)
	{
		return;
	}
	bInputHandled = true;

	const float PressTime = ElapsedRealTime;
	const float Ratio = (TotalDuration > 0.f) ? (PressTime / TotalDuration) : 0.f;
	ResultMeta.PressCount = 1;
	ResultMeta.PressTimingRatio = Ratio;

	bool bSuccess = false;
	if (TapData->bUsePerfectWindow)
	{
		bSuccess = (Ratio >= TapData->PerfectWindowStart && Ratio <= TapData->PerfectWindowEnd);
	}
	else
	{
		bSuccess = true; // 整段有效
	}

	// 按下即进度满（视觉反馈）
	BroadcastProgress(1.f);
	FinishQTE(bSuccess ? EQTEResult::Success : EQTEResult::Failure);
}
