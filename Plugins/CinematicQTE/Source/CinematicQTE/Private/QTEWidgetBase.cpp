// Copyright Cinematic QTE System. All Rights Reserved.

#include "QTEWidgetBase.h"
#include "QTETaskBase.h"
#include "QTEDataAsset.h"
#include "TapQTEDataAsset.h"

void UQTEWidgetBase::InitializeFromTask(UQTETaskBase* InTask, UQTEDataAsset* InDataAsset)
{
	BoundTask = InTask;
	BoundDataAsset = InDataAsset;

	if (InTask)
	{
		InTask->OnProgressChanged.AddDynamic(this, &UQTEWidgetBase::HandleProgressChanged);
		InTask->OnRemainingTimeChanged.AddDynamic(this, &UQTEWidgetBase::HandleRemainingChanged);
	}

	// 从 DataAsset 提取完美窗口信息（仅 TapQTEDataAsset 具备）
	bHasPerfectWindow = false;
	PerfectWindowStart = 0.f;
	PerfectWindowEnd = 0.f;
	if (const UTapQTEDataAsset* TapData = Cast<UTapQTEDataAsset>(InDataAsset))
	{
		if (TapData->bUsePerfectWindow)
		{
			bHasPerfectWindow = true;
			PerfectWindowStart = TapData->PerfectWindowStart;
			PerfectWindowEnd = TapData->PerfectWindowEnd;
		}
	}

	BP_OnPerfectWindowInfo(bHasPerfectWindow, PerfectWindowStart, PerfectWindowEnd);
	BP_OnQTEStarted(InDataAsset);
}

void UQTEWidgetBase::OnQTEFinished(EQTEResult Result)
{
	// 解绑，避免残留
	if (BoundTask)
	{
		BoundTask->OnProgressChanged.RemoveDynamic(this, &UQTEWidgetBase::HandleProgressChanged);
		BoundTask->OnRemainingTimeChanged.RemoveDynamic(this, &UQTEWidgetBase::HandleRemainingChanged);
	}
	BP_OnQTEFinished(Result);
}

void UQTEWidgetBase::HandleProgressChanged(float NewProgress)
{
	BP_OnProgressChanged(NewProgress);
}

void UQTEWidgetBase::HandleRemainingChanged(float NewRatio)
{
	BP_OnRemainingTimeChanged(NewRatio);
}
