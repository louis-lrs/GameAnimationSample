// Fill out your copyright notice in the Description page of Project Settings.

#include "GeCharacterMovementReplication.h"
#include "GeCharacterMovementComponent.h"
#include "GameFramework/Character.h"

// Implementation of FSavedMove_GeCharacter
void FSavedMove_GeCharacter::Clear()
{
	Super::Clear();
	SavedActualJumpApexTime = 0.0f;
	SavedAccumulatedJumpTime = 0.0f;
	SavedCapsuleStage = 0;
	StartCapsuleStage = 0;
	EndCapsuleStage = 0;
}

void FSavedMove_GeCharacter::SetMoveFor(ACharacter* Character, float InDeltaTime, FVector const& NewAccel, FNetworkPredictionData_Client_Character& ClientData)
{
	Super::SetMoveFor(Character, InDeltaTime, NewAccel, ClientData);
	
	if (UGeCharacterMovementComponent* GeMovement = Cast<UGeCharacterMovementComponent>(Character->GetCharacterMovement()))
	{
		// 使用 FFloat16 直接存储浮点时间值
		SavedActualJumpApexTime = GeMovement->GetActualJumpApexTime();
		SavedAccumulatedJumpTime = GeMovement->GetAccumulatedJumpTime();
		SavedCapsuleStage = static_cast<uint8>(GeMovement->GetCurrentCapsuleStage());
		StartCapsuleStage = SavedCapsuleStage;
	}
}

void FSavedMove_GeCharacter::PostUpdate(ACharacter* Character, EPostUpdateMode PostUpdateMode)
{
	Super::PostUpdate(Character, PostUpdateMode);
	
	if (UGeCharacterMovementComponent* GeMovement = Cast<UGeCharacterMovementComponent>(Character->GetCharacterMovement()))
	{
		EndCapsuleStage = static_cast<uint8>(GeMovement->GetCurrentCapsuleStage());
	}
}

bool FSavedMove_GeCharacter::CanCombineWith(const FSavedMovePtr& NewMove, ACharacter* Character, float MaxDelta) const
{
	if (const FSavedMove_GeCharacter* GeNewMove = static_cast<const FSavedMove_GeCharacter*>(NewMove.Get()))
	{
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
		// 从 FFloat16 直接转换回 float
		GeMovement->SetActualJumpApexTime(SavedActualJumpApexTime.GetFloat());
		GeMovement->SetAccumulatedJumpTime(SavedAccumulatedJumpTime.GetFloat());
		GeMovement->SetCurrentCapsuleStage(static_cast<EJumpCapsuleStage>(SavedCapsuleStage));
	}
}

FSavedMovePtr FNetworkPredictionData_Client_GeCharacter::AllocateNewMove()
{
	return FSavedMovePtr(new FSavedMove_GeCharacter());
}

float FNetworkPredictionData_Server_GeCharacter::GetServerAccumulatedJumpTime(float ClientAccumulatedJumpTime, float ServerAccumulatedJumpTime) const
{
	return ClientAccumulatedJumpTime;
}

void FGeCharacterNetworkMoveData::ClientFillNetworkMoveData(const FSavedMove_Character& ClientMove, ENetworkMoveType MoveType)
{
	Super::ClientFillNetworkMoveData(ClientMove, MoveType);
	
	if (const FSavedMove_GeCharacter* GeClientMove = static_cast<const FSavedMove_GeCharacter*>(&ClientMove))
	{
		// 复制 FFloat16 压缩值
		SavedActualJumpApexTime = GeClientMove->SavedActualJumpApexTime;
		SavedAccumulatedJumpTime = GeClientMove->SavedAccumulatedJumpTime;
	}
}

bool FGeCharacterNetworkMoveData::Serialize(UCharacterMovementComponent& CharacterMovement, FArchive& Ar, UPackageMap* PackageMap, ENetworkMoveType MoveType)
{
	bool bResult = Super::Serialize(CharacterMovement, Ar, PackageMap, MoveType);
	Ar << SavedActualJumpApexTime;
	Ar << SavedAccumulatedJumpTime;
	return bResult;
}
