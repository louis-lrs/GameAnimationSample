/* Copyright (C) 2021 Hugo ATTAL - All Rights Reserved
* This plugin is downloadable from the UE4 Marketplace
*/

#pragma once

#include "CoreMinimal.h"
#include "ENConnectionDrawingPolicy.h"
#include "EdGraph/RigVMEdGraphConnectionDrawingPolicy.h"

// Keep engine RigVM visit / fade / reroute wire state, draw with Electronic Nodes paths.
// Always used for RigVM graphs so PIE teardown cannot reach the unguarded engine policy.
class FENRigVMConnectionDrawingPolicy : public FRigVMEdGraphConnectionDrawingPolicy
{
public:
	FENRigVMConnectionDrawingPolicy(
		int32 InBackLayerID,
		int32 InFrontLayerID,
		float ZoomFactor,
		const FSlateRect& InClippingRect,
		FSlateWindowElementList& InDrawElements,
		UEdGraph* InGraphObj,
		bool bInUseElectronicNodesPaths);

	virtual void DrawConnection(int32 LayerId, const FVector2f& Start, const FVector2f& End, const FConnectionParams& Params) override;
	virtual void DetermineWiringStyle(UEdGraphPin* OutputPin, UEdGraphPin* InputPin, FConnectionParams& Params) override;

	~FENRigVMConnectionDrawingPolicy();

private:
	static bool CanSafelyQueryDebuggedHost(const UEdGraphPin* Pin);

	bool bUseElectronicNodesPaths = false;
	FENConnectionDrawingPolicy* ConnectionDrawingPolicy = nullptr;
};
