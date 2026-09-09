/* Copyright (C) 2021 Hugo ATTAL - All Rights Reserved
* This plugin is downloadable from the UE4 Marketplace
*/

#include "ENRigVMConnectionDrawingPolicy.h"

#include "EdGraph/EdGraphPin.h"
#include "Engine/World.h"
#include "RigVMBlueprintUtils.h"
#include "RigVMHost.h"

FENRigVMConnectionDrawingPolicy::FENRigVMConnectionDrawingPolicy(
	int32 InBackLayerID,
	int32 InFrontLayerID,
	float ZoomFactor,
	const FSlateRect& InClippingRect,
	FSlateWindowElementList& InDrawElements,
	UEdGraph* InGraphObj,
	bool bInUseElectronicNodesPaths)
	: FRigVMEdGraphConnectionDrawingPolicy(InBackLayerID, InFrontLayerID, ZoomFactor, InClippingRect, InDrawElements, InGraphObj)
	, bUseElectronicNodesPaths(bInUseElectronicNodesPaths)
{
	if (bUseElectronicNodesPaths)
	{
		ConnectionDrawingPolicy = new FENConnectionDrawingPolicy(
			InBackLayerID, InFrontLayerID, ZoomFactor, InClippingRect, InDrawElements, InGraphObj);
	}
}

void FENRigVMConnectionDrawingPolicy::DrawConnection(
	int32 LayerId,
	const FVector2f& Start,
	const FVector2f& End,
	const FConnectionParams& Params)
{
	if (bUseElectronicNodesPaths && ConnectionDrawingPolicy)
	{
		ConnectionDrawingPolicy->SetMousePosition(LocalMousePosition);
		ConnectionDrawingPolicy->DrawConnection(LayerId, Start, End, Params);
		SplineOverlapResult = ConnectionDrawingPolicy->SplineOverlapResult;
		return;
	}

	FRigVMEdGraphConnectionDrawingPolicy::DrawConnection(LayerId, Start, End, Params);
}

void FENRigVMConnectionDrawingPolicy::DetermineWiringStyle(
	UEdGraphPin* OutputPin,
	UEdGraphPin* InputPin,
	FConnectionParams& Params)
{
	// Engine policy calls GetVM() / GetRigVMExtendedExecuteContext() on the debugged host.
	// During PIE teardown that host can be BeginDestroy'd with a nulled context (check crash).
	if (CanSafelyQueryDebuggedHost(OutputPin) && CanSafelyQueryDebuggedHost(InputPin))
	{
		FRigVMEdGraphConnectionDrawingPolicy::DetermineWiringStyle(OutputPin, InputPin, Params);
		return;
	}

	FKismetConnectionDrawingPolicy::DetermineWiringStyle(OutputPin, InputPin, Params);
}

FENRigVMConnectionDrawingPolicy::~FENRigVMConnectionDrawingPolicy()
{
	delete ConnectionDrawingPolicy;
	ConnectionDrawingPolicy = nullptr;
}

bool FENRigVMConnectionDrawingPolicy::CanSafelyQueryDebuggedHost(const UEdGraphPin* Pin)
{
	if (Pin == nullptr)
	{
		return true;
	}

	const UEdGraphNode* Node = Pin->GetOwningNode();
	if (Node == nullptr)
	{
		return true;
	}

	const FRigVMEditorAssetInterfacePtr RigBlueprint = FRigVMBlueprintUtils::FindAssetForNode(Node);
	if (RigBlueprint.GetObject() == nullptr)
	{
		return true;
	}

	UObject* DebuggedObject = RigBlueprint->GetObjectBeingDebugged(true);
	if (DebuggedObject == nullptr)
	{
		return true;
	}

	if (!IsValid(DebuggedObject) || DebuggedObject->HasAnyFlags(RF_BeginDestroyed | RF_FinishDestroyed))
	{
		return false;
	}

	URigVMHost* Host = Cast<URigVMHost>(DebuggedObject);
	if (Host == nullptr)
	{
		return true;
	}

	if (URigVMHost::IsGarbageOrDestroyed(Host))
	{
		return false;
	}

	if (const UWorld* World = Host->GetWorld())
	{
		if (World->bIsTearingDown)
		{
			return false;
		}
	}

	return true;
}
