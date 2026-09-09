/* Copyright (C) 2021 Hugo ATTAL - All Rights Reserved
* This plugin is downloadable from the UE4 Marketplace
*/

#include "NodeFactoryPatch.h"

#include "AnimationGraphFactory.h"
#include "BlueprintConnectionDrawingPolicy.h"
#include "ConnectionDrawingPolicy.h"
#include "EdGraphSchema_K2.h"
#include "EdGraphUtilities.h"
#include "ENConnectionDrawingPolicy.h"

FConnectionDrawingPolicy* FNodeFactoryPatch::CreateConnectionPolicy_Hook(const UEdGraphSchema* Schema, int32 InBackLayerID, int32 InFrontLayerID, float ZoomFactor, const FSlateRect& InClippingRect, FSlateWindowElementList& InDrawElements, UEdGraph* InGraphObj)
{
	const TSharedPtr<FENConnectionDrawingPolicyFactory> ENConnectionFactory = MakeShareable(new FENConnectionDrawingPolicyFactory);

	FConnectionDrawingPolicy* ConnectionDrawingPolicy = ENConnectionFactory->CreateConnectionPolicy(Schema, InBackLayerID, InFrontLayerID, ZoomFactor, InClippingRect, InDrawElements, InGraphObj);

	if (!ConnectionDrawingPolicy)
	{
		ConnectionDrawingPolicy = Schema->CreateConnectionDrawingPolicy(InBackLayerID, InFrontLayerID, ZoomFactor, InClippingRect, InDrawElements, InGraphObj);
	}

	if (!ConnectionDrawingPolicy)
	{
		const TSharedPtr<FAnimationGraphPinConnectionFactory> AnimationGraphFactory = MakeShareable(new FAnimationGraphPinConnectionFactory);
		ConnectionDrawingPolicy = AnimationGraphFactory->CreateConnectionPolicy(Schema, InBackLayerID, InFrontLayerID, ZoomFactor, InClippingRect, InDrawElements, InGraphObj);
	}

	if (!ConnectionDrawingPolicy)
	{
		if (Schema->IsA(UEdGraphSchema_K2::StaticClass()))
		{
			ConnectionDrawingPolicy = new FKismetConnectionDrawingPolicy(InBackLayerID, InFrontLayerID, ZoomFactor, InClippingRect, InDrawElements, InGraphObj);
		}
		else
		{
			ConnectionDrawingPolicy = new FConnectionDrawingPolicy(InBackLayerID, InFrontLayerID, ZoomFactor, InClippingRect, InDrawElements);
		}
	}

	return ConnectionDrawingPolicy;
}
