// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/CharacterMovementComponent.h"

// Forward declaration
class UGeCharacterMovementComponent;

// Custom SavedMove class to sync AccumulatedJumpTime
class FSavedMove_GeCharacter : public FSavedMove_Character
{
public:
	typedef FSavedMove_Character Super;

	FFloat16 SavedActualJumpApexTime;  // 使用 FFloat16 压缩存储时间
	FFloat16 SavedAccumulatedJumpTime; // 使用 FFloat16 压缩存储时间
	uint8 SavedCapsuleStage;
	uint8 StartCapsuleStage;
	uint8 EndCapsuleStage;

	FSavedMove_GeCharacter()
		: SavedActualJumpApexTime(0.f)
		, SavedAccumulatedJumpTime(0.f)
		, SavedCapsuleStage(0)
		, StartCapsuleStage(0)
		, EndCapsuleStage(0)
	{
	}

	virtual void Clear() override;
	virtual void SetMoveFor(ACharacter* Character, float InDeltaTime, FVector const& NewAccel, class FNetworkPredictionData_Client_Character& ClientData) override;
	virtual void PostUpdate(ACharacter* Character, EPostUpdateMode PostUpdateMode) override;
	virtual bool CanCombineWith(const FSavedMovePtr& NewMove, ACharacter* Character, float MaxDelta) const override;
	virtual void CombineWith(const FSavedMove_Character* OldMove, ACharacter* InCharacter, APlayerController* PC, const FVector& OldStartLocation) override;
	virtual void PrepMoveFor(ACharacter* Character) override;
};

// Custom NetworkPredictionData class to allocate custom SavedMove
class FNetworkPredictionData_Client_GeCharacter : public FNetworkPredictionData_Client_Character
{
public:
	typedef FNetworkPredictionData_Client_Character Super;

	FNetworkPredictionData_Client_GeCharacter(const UCharacterMovementComponent& ClientMovement)
		: FNetworkPredictionData_Client_Character(ClientMovement)
	{
	}

	virtual FSavedMovePtr AllocateNewMove() override;
};

// Custom NetworkPredictionData class for server-side prediction
class FNetworkPredictionData_Server_GeCharacter : public FNetworkPredictionData_Server_Character
{
public:
	typedef FNetworkPredictionData_Server_Character Super;

	FNetworkPredictionData_Server_GeCharacter(const UCharacterMovementComponent& ServerMovement)
		: FNetworkPredictionData_Server_Character(ServerMovement)
	{
	}

	float GetServerAccumulatedJumpTime(float ClientAccumulatedJumpTime, float ServerAccumulatedJumpTime) const;
};

// Custom NetworkMoveData class to sync AccumulatedJumpTime to server
struct FGeCharacterNetworkMoveData : public FCharacterNetworkMoveData
{
	typedef FCharacterNetworkMoveData Super;
	
	FFloat16 SavedActualJumpApexTime;  // 使用 FFloat16 压缩存储时间
	FFloat16 SavedAccumulatedJumpTime; // 使用 FFloat16 压缩存储时间

	FGeCharacterNetworkMoveData()
		: SavedActualJumpApexTime(0)
		, SavedAccumulatedJumpTime(0)
	{
	}

	virtual void ClientFillNetworkMoveData(const FSavedMove_Character& ClientMove, ENetworkMoveType MoveType) override;
	virtual bool Serialize(UCharacterMovementComponent& CharacterMovement, FArchive& Ar, UPackageMap* PackageMap, ENetworkMoveType MoveType) override;
};

// Custom NetworkMoveDataContainer class to allocate custom NetworkMoveData
struct FGeCharacterNetworkMoveDataContainer : public FCharacterNetworkMoveDataContainer
{
	typedef FCharacterNetworkMoveDataContainer Super;
	
	FGeCharacterNetworkMoveDataContainer()
	{
		NewMoveData = &MoveData[0];
		PendingMoveData = &MoveData[1];
		OldMoveData = &MoveData[2];
	}

	virtual ~FGeCharacterNetworkMoveDataContainer() override {}

private:
	FGeCharacterNetworkMoveData MoveData[3];
};
