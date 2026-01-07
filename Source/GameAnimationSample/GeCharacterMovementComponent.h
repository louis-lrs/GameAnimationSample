// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GeCharacterMovementComponent.generated.h"

// Enum to define the discrete stages of the capsule size during a jump
UENUM(BlueprintType)
enum class EJumpCapsuleStage : uint8
{
	FullSize = 0,   // Standing/Landed
	Stage1 = 1,		// Just Jumped/About to Land
	Stage2 = 2,		// About to Reach JumpApex
};

USTRUCT(BlueprintType)
struct FJumpStageConfig
{
	GENERATED_BODY()

	// Jump Progress Threshold [0.0 - 1.0] (Based on Time to Apex)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0.0", ClampMax="1.0"))
	float Threshold = 0.0f;

	// CapsuleHalfHeight ratio (Relative to default CapsuleHalfHeight)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0.0", UIMin="0.0"))
	float ShrinkRatio = 1.0f;

	// Additional Z offset (positive = move capsule up)
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float CapsuleOffset = 0.f;
	
	FJumpStageConfig() {}
	FJumpStageConfig(float InThreshold, float InRatio, float InOffset) 
		: Threshold(InThreshold), ShrinkRatio(InRatio), CapsuleOffset(InOffset) {}
};

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class GAMEANIMATIONSAMPLE_API UGeCharacterMovementComponent : public UCharacterMovementComponent
{
	GENERATED_BODY()

public:
	// Sets default values for this component's properties
	UGeCharacterMovementComponent();

protected:
	// Called when the game starts
	virtual void BeginPlay() override;

public:
	// Called every frame
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	
	// Override to setup replication rules
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	
	virtual void OnMovementModeChanged(EMovementMode PreviousMovementMode, uint8 PreviousCustomMode) override;
	virtual void AdjustFloorHeight() override;
	virtual bool DoJump(bool bReplayingMoves) override;
	virtual void ProcessLanded(const FHitResult& Hit, float remainingTime, int32 Iterations) override;
	virtual void OnMovementUpdated(float DeltaSeconds, const FVector& OldLocation, const FVector& OldVelocity) override;
	
	UFUNCTION()
	virtual void OnReachedJumpApex();
	
	UFUNCTION()
	virtual void OnLandedCallback(const FHitResult& Hit);

	virtual void DisplayDebugForGame(float DeltaTime, bool bPrintToScreen = true, bool bPrintToLog = false);
	FVector GetCurrentVelocity() const { return Velocity; }
	
#pragma region DynamicCapsule

public:
	
	/* ------------ Dynamic Capsule Configuration ------------ */
	
	UPROPERTY(EditAnywhere, Category="Character Movement: DynamicCapsule")
	bool bEnableDynamicCapsule = false;
	
	// Toggle for Stage 1 (Intermediate Shrink)
	UPROPERTY(EditAnywhere, Category="Character Movement: DynamicCapsule", meta=(EditCondition="bEnableDynamicCapsule", EditConditionHides))
	bool bEnableStage1 = true;
	
	UPROPERTY(EditAnywhere, Category="Character Movement: DynamicCapsule", meta=(EditCondition="bEnableDynamicCapsule && bEnableStage1", EditConditionHides))
	FJumpStageConfig Stage1Config = FJumpStageConfig(0.2f, 0.8f, 0.f);
	
	// Toggle for Stage 2 (Maximum Shrink)
    UPROPERTY(EditAnywhere, Category="Character Movement: DynamicCapsule", meta=(EditCondition="bEnableDynamicCapsule", EditConditionHides))
    bool bEnableStage2 = true;
	
	UPROPERTY(EditAnywhere, Category="Character Movement: DynamicCapsule", meta=(EditCondition="bEnableDynamicCapsule && bEnableStage2", EditConditionHides))
	FJumpStageConfig Stage2Config = FJumpStageConfig(0.5f, 0.6f, 0.f);
	
	UPROPERTY(EditAnywhere, Category="Character Movement: DynamicCapsule", meta=(ClampMin="0.0", UIMin="0.0", EditCondition="bEnableDynamicCapsule", EditConditionHides))
	float InterpMeshSpeed = 20.f;

protected:
	/* ------------ Runtime State ------------ */
	
	// Current active stage of the capsule
	EJumpCapsuleStage CurrentCapsuleStage = EJumpCapsuleStage::FullSize;

	// Tracks the "deepest" shrink stage reached during the current jump's ascent.
	// Used to prevent shrinking during descent if we hit a ceiling early.
	EJumpCapsuleStage MaxReachedStage = EJumpCapsuleStage::FullSize;
	
	// Target Z offset for the mesh to visually compensate for capsule resizing
	TOptional<float> TargetMeshZOffset;
	
	// Accumulated time since the jump started (Driven by CMC DeltaSeconds)
	float AccumulatedJumpTime = 0.f;
	
	// Actual time to reach the jump apex
	float ActualJumpApexTime = 0.f;

	// Estimated time to reach the jump apex
	float ExpectedJumpApexTime = 0.f;
	
	// Tracks whether dynamic capsule logic is currently allowed to run.
	bool bIsDynamicCapsuleActive = false;
	
public:
	/* ------------ Functions ------------ */
	
	// Helper to get params for a specific stage
	FJumpStageConfig GetStageParams(EJumpCapsuleStage Stage) const;
	
	// Calculates which stage we should be in based on physics state
	EJumpCapsuleStage CalculateDesiredStage();

	// Applies the physical capsule change and calculates visual offset
	bool SetCapsuleStage(EJumpCapsuleStage NewCapsuleStage);
	
	// Smoothens the mesh position every frame
	void InterpMeshOffset(float DeltaTime);

	// Abstracted function for dynamic capsule logic
	void UpdateDynamicCapsule(float DeltaSeconds);
	
	// Force reset dynamic capsule
	void ResetDynamicCapsule();
	
	// Clear dynamic capsule status
	void ClearDynamicCapsuleState();
	
	void OnDynamicCapsuleBegin();
	void OnDynamicCapsuleEnd();
	
	void SetDynamicCapsuleActive(bool bActive);
	bool IsDynamicCapsuleActive() const { return bIsDynamicCapsuleActive; }
	
	virtual float GetDefaultMeshZ() const;
	virtual float GetDefaultCapsuleHalfHeight() const;
	
	/* ------------ Replication ------------ */
	
public:
	// Variable to sync stage to simulated proxies
	UPROPERTY(ReplicatedUsing = OnRep_ServerCapsuleStage)
	EJumpCapsuleStage ServerCapsuleStage;
	
	// Function triggered when variable updates on clients
	UFUNCTION()
	void OnRep_ServerCapsuleStage();
	
#pragma endregion 
};
