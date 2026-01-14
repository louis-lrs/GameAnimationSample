// Fill out your copyright notice in the Description page of Project Settings.

#include "GeCharacterMovementReplication.h"
#include "GeCharacterMovementComponent.h"
#include "GameFramework/Character.h"

// Implementation of FSavedMove_GeCharacter
void FSavedMove_GeCharacter::Clear()
{
	Super::Clear();
	SavedAccumulatedJumpTime = 0.f;
	SavedCapsuleStage = 0;
	StartCapsuleStage = 0;
	EndCapsuleStage = 0;
}

void FSavedMove_GeCharacter::SetMoveFor(ACharacter* Character, float InDeltaTime, FVector const& NewAccel, FNetworkPredictionData_Client_Character& ClientData)
{
	Super::SetMoveFor(Character, InDeltaTime, NewAccel, ClientData);
	
	if (UGeCharacterMovementComponent* GeMovement = Cast<UGeCharacterMovementComponent>(Character->GetCharacterMovement()))
	{
		SavedAccumulatedJumpTime = GeMovement->GetAccumulatedJumpTime();
		SavedCapsuleStage = static_cast<uint8>(GeMovement->GetCurrentCapsuleStage());
		
		// Record the start capsule stage (before this move is simulated)
		StartCapsuleStage = SavedCapsuleStage;
	}
}

void FSavedMove_GeCharacter::PostUpdate(ACharacter* Character, EPostUpdateMode PostUpdateMode)
{
	Super::PostUpdate(Character, PostUpdateMode);
	
	// Record the end capsule stage (after this move is simulated)
	if (UGeCharacterMovementComponent* GeMovement = Cast<UGeCharacterMovementComponent>(Character->GetCharacterMovement()))
	{
		EndCapsuleStage = static_cast<uint8>(GeMovement->GetCurrentCapsuleStage());
	}
}

bool FSavedMove_GeCharacter::CanCombineWith(const FSavedMovePtr& NewMove, ACharacter* Character, float MaxDelta) const
{
	if (const FSavedMove_GeCharacter* GeNewMove = static_cast<const FSavedMove_GeCharacter*>(NewMove.Get()))
	{
		// Don't combine if capsule stage is different
		if (SavedCapsuleStage != GeNewMove->SavedCapsuleStage)
		{
			return false;
		}
	}
	
	return Super::CanCombineWith(NewMove, Character, MaxDelta);
}

void FSavedMove_GeCharacter::PrepMoveFor(ACharacter* Character)
{
	Super::PrepMoveFor(Character);
	
	if (UGeCharacterMovementComponent* GeMovement = Cast<UGeCharacterMovementComponent>(Character->GetCharacterMovement()))
	{
		// Restore AccumulatedJumpTime from saved move (core fix for rubber-banding)
		GeMovement->SetAccumulatedJumpTime(SavedAccumulatedJumpTime);
		
		// Optionally restore capsule stage to ensure state alignment before simulation
		GeMovement->SetCurrentCapsuleStage(static_cast<EJumpCapsuleStage>(SavedCapsuleStage));
	}
}

// Implementation of FNetworkPredictionData_Client_GeCharacter
FSavedMovePtr FNetworkPredictionData_Client_GeCharacter::AllocateNewMove()
{
	return FSavedMovePtr(new FSavedMove_GeCharacter());
}

// Implementation of FNetworkPredictionData_Server_GeCharacter
float FNetworkPredictionData_Server_GeCharacter::GetServerAccumulatedJumpTime(float ClientAccumulatedJumpTime, float ServerAccumulatedJumpTime) const
{
	// Calculate the real AccumulatedJumpTime by comparing client and server values
	// Client's AccumulatedJumpTime already includes this frame's DeltaTime (calculated after UpdateDynamicCapsule)
	// Server hasn't processed this frame yet, so we need to subtract DeltaTime from client time
	// This way, when server adds DeltaTime in UpdateDynamicCapsule, it will match client's final value
	
	const float AccumulatedJumpTime = ClientAccumulatedJumpTime;
	
	// Use client's time (before this frame) to ensure synchronization
	// Server will add DeltaTime later, matching client's final state
	return AccumulatedJumpTime;
}

// Implementation of FGeCharacterNetworkMoveData
void FGeCharacterNetworkMoveData::ClientFillNetworkMoveData(const FSavedMove_Character& ClientMove, ENetworkMoveType MoveType)
{
	Super::ClientFillNetworkMoveData(ClientMove, MoveType);
	
	// Extract SavedAccumulatedJumpTime from custom SavedMove
	if (const FSavedMove_GeCharacter* GeClientMove = static_cast<const FSavedMove_GeCharacter*>(&ClientMove))
	{
		SavedAccumulatedJumpTime = GeClientMove->SavedAccumulatedJumpTime;
	}
}

bool FGeCharacterNetworkMoveData::Serialize(UCharacterMovementComponent& CharacterMovement, FArchive& Ar, UPackageMap* PackageMap, ENetworkMoveType MoveType)
{
	// Serialize base class data first
	bool bResult = Super::Serialize(CharacterMovement, Ar, PackageMap, MoveType);
	
	// Serialize custom data
	Ar << SavedAccumulatedJumpTime;
	
	return bResult;
}
