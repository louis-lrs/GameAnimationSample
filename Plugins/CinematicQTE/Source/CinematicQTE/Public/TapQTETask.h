// Copyright Cinematic QTE System. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "QTETaskBase.h"
#include "TapQTETask.generated.h"

class UTapQTEDataAsset;

/**
 * 单点时机触发型 QTE 任务。
 */
UCLASS(BlueprintType)
class CINEMATICQTE_API UTapQTETask : public UQTETaskBase
{
	GENERATED_BODY()

public:
	UTapQTETask();

protected:
	virtual void OnStartQTE() override;
	virtual void OnHandleInput(const FInputActionValue& Value) override;

private:
	UPROPERTY(Transient)
	TObjectPtr<UTapQTEDataAsset> TapData = nullptr;

	bool bInputHandled = false;
};
