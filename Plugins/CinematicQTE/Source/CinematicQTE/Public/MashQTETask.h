// Copyright Cinematic QTE System. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "QTETaskBase.h"
#include "MashQTETask.generated.h"

class UMashQTEDataAsset;

/**
 * 连点累积进度型 QTE 任务。
 */
UCLASS(BlueprintType)
class CINEMATICQTE_API UMashQTETask : public UQTETaskBase
{
	GENERATED_BODY()

public:
	UMashQTETask();

protected:
	virtual void OnStartQTE() override;
	virtual void OnTickQTE(float DeltaTime) override;
	virtual void OnHandleInput(const FInputActionValue& Value) override;

private:
	UPROPERTY(Transient)
	TObjectPtr<UMashQTEDataAsset> MashData = nullptr;

	/** 上次按压的时间戳（ElapsedRealTime） */
	float LastPressTime = -1.f;
};
