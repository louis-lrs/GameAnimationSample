// Copyright Cinematic QTE System. All Rights Reserved.

#include "QTEWidgetBase.h"
#include "QTETaskBase.h"
#include "QTEDataAsset.h"

void UQTEWidgetBase::InitializeFromTask(UQTETaskBase* InTask, UQTEDataAsset* InDataAsset)
{
	BoundTask = InTask;
	BoundDataAsset = InDataAsset;

	if (InTask)
	{
		InTask->OnProgressChanged.AddDynamic(this, &UQTEWidgetBase::HandleProgressChanged);
		InTask->OnRemainingTimeChanged.AddDynamic(this, &UQTEWidgetBase::HandleRemainingChanged);
	}

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
