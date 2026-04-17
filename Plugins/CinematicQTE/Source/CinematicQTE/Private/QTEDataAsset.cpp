// Copyright Cinematic QTE System. All Rights Reserved.

#include "QTEDataAsset.h"
#include "CinematicQTEModule.h"
#include "InputAction.h"

UQTEDataAsset::UQTEDataAsset()
{
}

bool UQTEDataAsset::IsValidConfig(FString& OutReason) const
{
	if (Duration <= 0.f)
	{
		OutReason = TEXT("Duration must be greater than 0.");
		return false;
	}
	if (InputAction == nullptr)
	{
		OutReason = TEXT("InputAction is not set.");
		return false;
	}
	if (SlowMotionRate < 0.f || SlowMotionRate > 1.f)
	{
		OutReason = TEXT("SlowMotionRate must be within [0, 1].");
		return false;
	}
	OutReason.Reset();
	return true;
}

#if WITH_EDITOR
void UQTEDataAsset::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	FString Reason;
	if (!IsValidConfig(Reason))
	{
		UE_LOG(LogCinematicQTE, Warning, TEXT("QTEDataAsset[%s] config invalid: %s"),
			*GetName(), *Reason);
	}
}
#endif
