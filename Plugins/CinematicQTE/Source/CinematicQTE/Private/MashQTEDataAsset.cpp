// Copyright Cinematic QTE System. All Rights Reserved.

#include "MashQTEDataAsset.h"
#include "MashQTETask.h"

UMashQTEDataAsset::UMashQTEDataAsset()
{
	QTEType = EQTEType::Mash;
	// 默认指向 Mash 任务类（若该类已注册）
	TaskClass = UMashQTETask::StaticClass();
}

float UMashQTEDataAsset::GetEffectiveProgressPerPress() const
{
	if (ProgressPerPress > 0.f)
	{
		return ProgressPerPress;
	}
	return (RequiredPressCount > 0) ? (1.f / static_cast<float>(RequiredPressCount)) : 1.f;
}

bool UMashQTEDataAsset::IsValidConfig(FString& OutReason) const
{
	if (!Super::IsValidConfig(OutReason))
	{
		return false;
	}
	if (RequiredPressCount <= 0)
	{
		OutReason = TEXT("RequiredPressCount must be > 0.");
		return false;
	}
	return true;
}
