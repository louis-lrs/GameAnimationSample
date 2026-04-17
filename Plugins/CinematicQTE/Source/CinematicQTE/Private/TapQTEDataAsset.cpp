// Copyright Cinematic QTE System. All Rights Reserved.

#include "TapQTEDataAsset.h"
#include "TapQTETask.h"

UTapQTEDataAsset::UTapQTEDataAsset()
{
	QTEType = EQTEType::Tap;
	Duration = 1.5f;
	TaskClass = UTapQTETask::StaticClass();
}

bool UTapQTEDataAsset::IsValidConfig(FString& OutReason) const
{
	if (!Super::IsValidConfig(OutReason))
	{
		return false;
	}
	if (bUsePerfectWindow)
	{
		if (PerfectWindowEnd <= PerfectWindowStart)
		{
			OutReason = TEXT("PerfectWindowEnd must be greater than PerfectWindowStart.");
			return false;
		}
	}
	return true;
}
