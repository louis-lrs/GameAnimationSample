// Copyright Cinematic QTE System. All Rights Reserved.

#include "CinematicQTEDebugLibrary.h"
#include "CinematicQTESubsystem.h"

bool UCinematicQTEDebugLibrary::StartQTE(const UObject* WorldContext, UQTEDataAsset* DataAsset,
	ULevelSequencePlayer* SequencePlayer, EQTEConflictPolicy Policy /*= EQTEConflictPolicy::Ignore*/)
{
	if (UCinematicQTESubsystem* Sub = UCinematicQTESubsystem::Get(WorldContext))
	{
		return Sub->StartQTE(DataAsset, SequencePlayer, Policy);
	}
	return false;
}

void UCinematicQTEDebugLibrary::CancelCurrentQTE(const UObject* WorldContext, EQTEResult Result /*= EQTEResult::Cancelled*/)
{
	if (UCinematicQTESubsystem* Sub = UCinematicQTESubsystem::Get(WorldContext))
	{
		Sub->CancelCurrentQTE(Result);
	}
}

bool UCinematicQTEDebugLibrary::IsQTEActive(const UObject* WorldContext)
{
	if (UCinematicQTESubsystem* Sub = UCinematicQTESubsystem::Get(WorldContext))
	{
		return Sub->IsQTEActive();
	}
	return false;
}

float UCinematicQTEDebugLibrary::GetCurrentPlayRate(const UObject* WorldContext)
{
	if (UCinematicQTESubsystem* Sub = UCinematicQTESubsystem::Get(WorldContext))
	{
		return Sub->GetCurrentPlayRate();
	}
	return 1.f;
}
