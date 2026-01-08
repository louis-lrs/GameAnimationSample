// Fill out your copyright notice in the Description page of Project Settings.


#include "GeCharacterMovementComponent.h"

#include "EnhancedLog.h"
#include "GameAnimationSample.h"
#include "KismetTraceUtils.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/Character.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetMathLibrary.h"
#include "Net/UnrealNetwork.h"
#include "Net/Core/PushModel/PushModel.h"

static TAutoConsoleVariable<int32> CVarAnimSkillMovement_DebugMovement(TEXT("a.AnimSkill.Movement.DebugMovement"),0,TEXT("0: Disable, 1: Autonomous, 2: Client, 3: DedicatedServer, 4: Simulated Proxy, 5: All"));
static TAutoConsoleVariable<int32> CVarAnimSkillMovement_DebugClientID(TEXT("a.AnimSkill.Movement.DebugClientID"),-1,TEXT(""));
static TAutoConsoleVariable<int32> CVarAnimSkillMovement_DebugStanceCollision(TEXT("a.AnimSkill.Movement.DebugStanceCollision"),0,TEXT("0: Disable, 1: Enable"));

static bool GDisplayLogCapsule = false;

namespace GeCharacterMovementCVars
{
	static bool bEnableDynamicCapusle = true;
	FAutoConsoleVariableRef CVarEnableDynamicCapusle(
		TEXT("a.AnimSkill.Movement.EnableDynamicCapusle"),
		bEnableDynamicCapusle,
		TEXT(""));
    	
	FAutoConsoleVariableRef CVarEnableLogCapsule(
		TEXT("a.AnimSkill.Movement.EnableLogCapsule"),
		GDisplayLogCapsule,
		TEXT(""));
}

// Sets default values for this component's properties
UGeCharacterMovementComponent::UGeCharacterMovementComponent()
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = true;
}

// Called when the game starts
void UGeCharacterMovementComponent::BeginPlay()
{
	Super::BeginPlay();

	if (!HasValidData())
	{
		return;
	}
	CharacterOwner->OnReachedJumpApex.AddUniqueDynamic(this, &ThisClass::OnReachedJumpApex);
	CharacterOwner->LandedDelegate.AddUniqueDynamic(this, &ThisClass::OnLandedCallback);
}

// Called every frame
void UGeCharacterMovementComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	
	const int32 theDebugMovement = CVarAnimSkillMovement_DebugMovement.GetValueOnAnyThread();
	if (theDebugMovement > 0)
	{
		const auto ClientID = CVarAnimSkillMovement_DebugClientID.GetValueOnAnyThread();
		if (ClientID > 0)
		{
			if (UE::GetPlayInEditorID() != ClientID)
			{
				return;
			}
		}

		bool bEnableDebug = true;
		if (theDebugMovement == 1)
		{
			bEnableDebug = (CharacterOwner->GetLocalRole() == ROLE_AutonomousProxy && IsNetMode(NM_Client))
				|| (CharacterOwner->GetLocalRole() == ROLE_Authority && IsNetMode(NM_Standalone));
		}
		else if (theDebugMovement == 2)
		{
			bEnableDebug = IsNetMode(NM_Client) || IsNetMode(NM_Standalone);
		}
		else if (theDebugMovement == 3)
		{
			bEnableDebug = IsNetMode(NM_DedicatedServer);
		}
		else if (theDebugMovement == 4)
		{
			bEnableDebug = CharacterOwner->GetLocalRole() == ROLE_SimulatedProxy;
		}
			
		if (!bEnableDebug)
		{
			return;
		}

		DisplayDebugForGame(DeltaTime);
	}
	
	// if (CharacterOwner->GetLocalRole() == ROLE_SimulatedProxy)
	// {
	// 	InterpMeshOffset(DeltaTime);
	// }
}

void UGeCharacterMovementComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	
	// Define replicated shared params
	// FDoRepLifetimeParams SharedParamsSimulatedOnly{ COND_SimulatedOnly, REPNOTIFY_OnChanged, true };
	// DOREPLIFETIME_WITH_PARAMS_FAST(UGeCharacterMovementComponent, ServerCapsuleStage, SharedParamsSimulatedOnly);
	
	DOREPLIFETIME_CONDITION(UGeCharacterMovementComponent, ServerCapsuleStage, COND_SkipOwner);
}

void UGeCharacterMovementComponent::OnMovementModeChanged(EMovementMode PreviousMovementMode, uint8 PreviousCustomMode)
{
	Super::OnMovementModeChanged(PreviousMovementMode, PreviousCustomMode);
	
	// If is not falling movement mode, interrupt with mode determined by target movement mode
	if (MovementMode != MOVE_Falling)
	{
		bNotifyApex = false;
		if (bIsDynamicCapsuleActive)
		{
			const bool bRestoreCapsule = ShouldRestoreCapsuleOnMovementModeChange(MovementMode, CustomMovementMode);
			InterruptDynamicCapsule(bRestoreCapsule);	
		}
	}
}

void UGeCharacterMovementComponent::AdjustFloorHeight()
{
	Super::AdjustFloorHeight();
}

bool UGeCharacterMovementComponent::DoJump(bool bReplayingMoves)
{
	bool bResult = Super::DoJump(bReplayingMoves);
	if (bResult)
	{
		bNotifyApex = true;
		OnDynamicCapsuleBegin();
	}
	return bResult;
}

void UGeCharacterMovementComponent::ProcessLanded(const FHitResult& Hit, float remainingTime, int32 Iterations)
{
	Super::ProcessLanded(Hit, remainingTime, Iterations);
	
	OnDynamicCapsuleEnd();
}

void UGeCharacterMovementComponent::OnMovementUpdated(float DeltaSeconds, const FVector& OldLocation, const FVector& OldVelocity)
{
	Super::OnMovementUpdated(DeltaSeconds, OldLocation, OldVelocity);
	
	UpdateDynamicCapsule(DeltaSeconds);
}

void UGeCharacterMovementComponent::OnReachedJumpApex()
{	
	ActualJumpApexTime = AccumulatedJumpTime;
	UE_LOG_GATED(GDisplayLogCapsule, LogGeCharacterMovement, Log, this,
	             TEXT("[DynamicCapsule] %hs ExpectedJumpApexTime=%.4f, ActualJumpApexTime=%.4f"), __FUNCTION__,
	             ExpectedJumpApexTime, ActualJumpApexTime);
}

void UGeCharacterMovementComponent::OnLandedCallback(const FHitResult& Hit)
{
}

#pragma region DynamicCapsule

float UGeCharacterMovementComponent::GetDefaultMeshZ() const
{
	constexpr float DefaultValue = -90.f;
	
	if (!HasValidData())
	{
		return DefaultValue;
	}
	
	// Retrieve the default relative Z location from the CDO's Mesh component
	if (ACharacter* DefaultCharacter = CharacterOwner->GetClass()->GetDefaultObject<ACharacter>())
	{
		if (USkeletalMeshComponent* DefaultMesh = DefaultCharacter->GetMesh())
		{
			return DefaultMesh->GetRelativeLocation().Z;
		}
	}
	
	return DefaultValue;
}

float UGeCharacterMovementComponent::GetDefaultCapsuleHalfHeight() const
{
	constexpr float DefaultValue = 90.f;
	
	if (!HasValidData())
	{
		return DefaultValue;
	}
	
	// Retrieve the default unscaled half-height from the CDO's Capsule component
	if (ACharacter* DefaultCharacter = CharacterOwner->GetClass()->GetDefaultObject<ACharacter>())
	{
		if (UCapsuleComponent* DefaultCapsule = DefaultCharacter->GetCapsuleComponent())
		{
			return DefaultCapsule->GetUnscaledCapsuleHalfHeight();
		}
	}
	return DefaultValue;
}

void UGeCharacterMovementComponent::OnRep_ServerCapsuleStage()
{
	SetCapsuleStage(ServerCapsuleStage);
}

FJumpStageConfig UGeCharacterMovementComponent::GetStageParams(EJumpCapsuleStage Stage) const
{
	// Return default "FullSize" config if no specific stage match
	FJumpStageConfig Config(0.f, 1.f, 0.f);

	switch (Stage)
	{
	case EJumpCapsuleStage::Stage1:
		Config = Stage1Config;
		break;
	case EJumpCapsuleStage::Stage2:
		Config = Stage2Config;
		break;
	case EJumpCapsuleStage::FullSize:
	default:
		break;
	}
	return Config;
}

EJumpCapsuleStage UGeCharacterMovementComponent::CalculateDesiredStage()
{
	EJumpCapsuleStage TargetStage = EJumpCapsuleStage::FullSize;
	if (Velocity.Z > 0.f)
	{
		// Safety check: prevent division by zero
		if (ExpectedJumpApexTime <= UE_KINDA_SMALL_NUMBER)
		{
			return EJumpCapsuleStage::FullSize;
		}
		
		const float Progress = AccumulatedJumpTime / ExpectedJumpApexTime;
		if (bEnableStage2 && Progress >= Stage2Config.Threshold)
		{
			TargetStage = EJumpCapsuleStage::Stage2;
		}
		else if (bEnableStage1 && Progress >= Stage1Config.Threshold)
		{
			TargetStage = EJumpCapsuleStage::Stage1;
		}
		
		// Track max stage
		if (TargetStage > MaxReachedStage)
		{
			MaxReachedStage = TargetStage;
		}
		UE_LOG_GATED(GDisplayLogCapsule, LogGeCharacterMovement, Verbose, this, TEXT("[DynamicCapsule] %hs Progress=%.2f, Target=%s, MaxReached=%s"), 
			__FUNCTION__, Progress, *UEnum::GetValueAsString(TargetStage), *UEnum::GetValueAsString(MaxReachedStage));
	}
	else
	{
		// Don't enter a shrunk stage if we didn't reach it during ascent.
		if (MaxReachedStage == EJumpCapsuleStage::FullSize)
		{
			return EJumpCapsuleStage::FullSize;
		}
		
		// Safety check: prevent division by zero
		if (ActualJumpApexTime <= UE_KINDA_SMALL_NUMBER)
		{
			return EJumpCapsuleStage::FullSize;
		}
		
		// Calculate falling thresholds symmetrically
		const float SymmetricStage2 = FMath::Max(1.f - Stage2Config.Threshold, 0.f);
		const float SymmetricStage1 = FMath::Max(1.f - Stage1Config.Threshold, 0.f);
		
		const float FallingProgress = FMath::Max((AccumulatedJumpTime - ActualJumpApexTime) / ActualJumpApexTime, 0.f);
		if (bEnableStage2 && FallingProgress < SymmetricStage2)
		{
			TargetStage = EJumpCapsuleStage::Stage2;
		}
		else if (bEnableStage1 && FallingProgress < SymmetricStage1)
		{
			TargetStage = EJumpCapsuleStage::Stage1;
		}
		
		// Clamp max stage (Don't enter Stage if we never reached it)
		if (TargetStage > MaxReachedStage)
		{
			TargetStage = MaxReachedStage;
		}
		UE_LOG_GATED(GDisplayLogCapsule, LogGeCharacterMovement, Verbose, this, TEXT("[DynamicCapsule] %hs FallingProgress=%.2f, Target=%s, MaxReached=%s"), 
			__FUNCTION__, FallingProgress, *UEnum::GetValueAsString(TargetStage), *UEnum::GetValueAsString(MaxReachedStage));
	}
	return TargetStage;
}

bool UGeCharacterMovementComponent::SetCapsuleStage(EJumpCapsuleStage NewCapsuleStage)
{
	if (!HasValidData())
	{
		return false;
	}
	
	if (CurrentCapsuleStage == NewCapsuleStage)
	{
		return true;
	}

	const UWorld* MyWorld = GetWorld();
	if (MyWorld == nullptr) { return false; }

	UCapsuleComponent* Capsule = CharacterOwner->GetCapsuleComponent();
	if (Capsule == nullptr) { return false; }
	
	USkeletalMeshComponent* CharacterMesh = CharacterOwner->GetMesh();
	if (CharacterMesh == nullptr) { return false; }
	
	/* ------------------------ Setup Parameters ------------------------ */
	const FJumpStageConfig TargetConfig = GetStageParams(NewCapsuleStage);
	const FJumpStageConfig CurrentConfig = GetStageParams(CurrentCapsuleStage);
	
	// Calculate the delta to adjust the mesh visual position
	const float DefaultHalfHeight = GetDefaultCapsuleHalfHeight();
	const float NewHalfHeight = DefaultHalfHeight * TargetConfig.ShrinkRatio;
	const float OldHalfHeight = Capsule->GetUnscaledCapsuleHalfHeight();
	const float ComponentScale = Capsule->GetShapeScale();
	
	// Precision check to avoid unnecessary physics updates
	if (FMath::IsNearlyZero(NewHalfHeight) || FMath::IsNearlyZero(ComponentScale))
	{
		return false;
	}
	
	// Keep Top Fixed. If shrinking (Old > New), we move UP (+) by the difference.
	const float HalfHeightAdjust = OldHalfHeight - NewHalfHeight;
	// Transitioning from CurrentCapsuleStage to NewCapsuleStage.
	const float ManualOffsetDelta = TargetConfig.CapsuleOffset - CurrentConfig.CapsuleOffset;
	// Add OffsetDelta. Positive moves the whole capsule UP.
	const float TotalZDelta = HalfHeightAdjust + ManualOffsetDelta;
	
	// Precision check to avoid unnecessary physics updates
	if (FMath::IsNearlyZero(HalfHeightAdjust) && FMath::IsNearlyZero(TotalZDelta))
	{
		CurrentCapsuleStage = NewCapsuleStage;
		return true;
	}

#if 0
	
	/* ------------------------ Physics Probe ------------------------ */
	const bool bIsMovingDown = TotalZDelta < 0.f; 
	const float ScaledTotalZDelta = TotalZDelta * ComponentScale;
	const float ScaledHalfHeightAdjust = HalfHeightAdjust * ComponentScale;
	
	const FVector PawnLocation = UpdatedComponent->GetComponentLocation();
	const FQuat PawnRotation = UpdatedComponent->GetComponentQuat();
	FVector ProposedLocation = PawnLocation + FVector(0.f, 0.f, ScaledTotalZDelta);
	
	const bool bIsSimulatedProxy = (CharacterOwner->GetLocalRole() == ROLE_SimulatedProxy);
	if (!bIsSimulatedProxy && bIsMovingDown)
	{
		// Compensate for the difference between current capsule size and target capsule size
		constexpr float SweepInflation = UE_KINDA_SMALL_NUMBER * 10.f;
		const float AbsMoveDownAmount = -ScaledTotalZDelta;
		
		// Use current capsule for sweeping downwards
		const ECollisionChannel CollisionChannel = UpdatedComponent->GetCollisionObjectType();
		const FCollisionShape CurrentCapsuleShape = Capsule->GetCollisionShape();
		FCollisionQueryParams CapsuleParams(SCENE_QUERY_STAT(DynamicCapsuleTrace), false, CharacterOwner);
		FCollisionResponseParams ResponseParam;
		InitCollisionParams(CapsuleParams, ResponseParam);
		
		FHitResult FloorHit;
		FVector Start = PawnLocation - FVector(0.f, 0.f, AbsMoveDownAmount);
		FVector End = Start - FVector(0.f, 0.f, AbsMoveDownAmount + SweepInflation);
		const bool bHitFloor = MyWorld->SweepSingleByChannel(FloorHit, Start, End, FQuat::Identity, CollisionChannel, CurrentCapsuleShape, CapsuleParams, ResponseParam);
#if ENABLE_DRAW_DEBUG
		if (CVarAnimSkillMovement_DebugStanceCollision.GetValueOnAnyThread() > 0)
		{
			DrawDebugCapsuleTraceSingle(MyWorld, Start, End, CurrentCapsuleShape.GetCapsuleRadius(), CurrentCapsuleShape.GetCapsuleHalfHeight(), 
				EDrawDebugTrace::Type::ForDuration, bHitFloor, FloorHit,FLinearColor::Green, FLinearColor::Red, 3.f);
		}
#endif
		if (bHitFloor)
		{
			ProposedLocation.Z = FloorHit.Location.Z + AbsMoveDownAmount + MAX_FLOOR_DIST;
			UE_LOG_GATED(GDisplayLogCapsule, LogGeCharacterMovement, Log, this, TEXT("[DynamicCapsule] %hs Adjusted ProposedLocation=%s"), __FUNCTION__, *ProposedLocation.ToCompactString());
		}
		
		// Use target capsule for proposed location blocking test
		const FCollisionShape TargetCapsuleShape = GetPawnCapsuleCollisionShape(SHRINK_HeightCustom, -SweepInflation + ScaledHalfHeightAdjust); // Shrink by negative amount, so actually grow it.
		const bool bEncroached = MyWorld->OverlapBlockingTestByChannel(ProposedLocation, PawnRotation, CollisionChannel, TargetCapsuleShape, CapsuleParams, ResponseParam);
#if ENABLE_DRAW_DEBUG
		if (CVarAnimSkillMovement_DebugStanceCollision.GetValueOnAnyThread() > 0)
		{
			const FTransform ActorTransform = GetActorTransform();
			const FVector LocalOffset = ActorTransform.TransformVectorNoScale(FVector(0, 50, 0));
			const FVector FinalLocation = ProposedLocation + LocalOffset;
			FHitResult theHitResult;
			theHitResult.bBlockingHit = bEncroached;
			theHitResult.Location = FinalLocation;
			DrawDebugCapsuleTraceSingle(MyWorld, FinalLocation, FinalLocation, TargetCapsuleShape.GetCapsuleRadius(), TargetCapsuleShape.GetCapsuleHalfHeight()
				 , EDrawDebugTrace::ForDuration, bEncroached, theHitResult, FLinearColor::Blue, FLinearColor::Red, 3.f);
		}
#endif
		if (bEncroached)
		{
			UE_LOG_GATED(GDisplayLogCapsule, LogGeCharacterMovement, Warning, this, TEXT("[DynamicCapsule] %hs Failed: Encroached at ProposedLocation=%s"), __FUNCTION__, *ProposedLocation.ToCompactString());
			return false;
		}
	}

	/* ------------------------ Commit Changes ------------------------ */
	CurrentCapsuleStage = NewCapsuleStage;
	if (CharacterOwner->HasAuthority())
	{
		ServerCapsuleStage = NewCapsuleStage;
		// If Push Model is enabled, simple assignment won't trigger replication!
		// MARK_PROPERTY_DIRTY_FROM_NAME(UGeCharacterMovementComponent, ServerCapsuleStage, this);
	}
	UE_LOG_GATED(GDisplayLogCapsule, LogGeCharacterMovement, Log, this, TEXT("[DynamicCapsule] %hs Succeed: %s"), __FUNCTION__, *UEnum::GetValueAsString(NewCapsuleStage));

	// 1. Record state BEFORE changes for Visual Compensation
	const float PreActionCapsuleZ = UpdatedComponent->GetComponentLocation().Z;
	const float PreActionMeshRelZ = CharacterMesh->GetRelativeLocation().Z;
	
	if (bIsSimulatedProxy)
	{
		// 2.1 Apply Size Change
		Capsule->SetCapsuleHalfHeight(NewHalfHeight, true);
		bShrinkProxyCapsule = true;
	}
	else if (!bIsSimulatedProxy)
	{
		// 2.2 Apply Size Change
		Capsule->SetCapsuleHalfHeight(NewHalfHeight, true);
	
		// 3. Move Capsule
		UpdatedComponent->MoveComponent(ProposedLocation - PawnLocation, PawnRotation, false, nullptr, EMoveComponentFlags::MOVECOMP_NoFlags, ETeleportType::TeleportPhysics);
	}
	
	bForceNextFloorCheck = true;
	AdjustProxyCapsuleSize();
	
	// Record expected capsule half height for detecting external modifications
	ExpectedCapsuleHalfHeight = NewHalfHeight;

	// 4. Record state AFTER changes for Visual Compensation
	const float PostActionCapsuleZ = UpdatedComponent->GetComponentLocation().Z;
	const float WorldMoveDelta = PostActionCapsuleZ - PreActionCapsuleZ;
	
	// 5. Visual Compensation
	const float DefaultMeshZ = GetDefaultMeshZ();
	const float TotalShrinkAmount = DefaultHalfHeight - NewHalfHeight;
	const float TotalCenterShiftUp = TotalShrinkAmount + TargetOffset;
	const float IdealMeshZ = DefaultMeshZ - TotalCenterShiftUp;

	FVector MeshRelativeLocation = CharacterMesh->GetRelativeLocation();
	float CompensatedMeshZ = PreActionMeshRelZ - WorldMoveDelta;
	
	if (FMath::IsNearlyEqual(CompensatedMeshZ, IdealMeshZ, 0.1f))
	{
		MeshRelativeLocation.Z = IdealMeshZ;
		CharacterMesh->SetRelativeLocation(MeshRelativeLocation);
		TargetMeshZOffset.Reset();
	}
	else
	{
		MeshRelativeLocation.Z = CompensatedMeshZ;
		CharacterMesh->SetRelativeLocation(MeshRelativeLocation);
		TargetMeshZOffset = IdealMeshZ;
	}
	UE_LOG_GATED(GDisplayLogCapsule, LogGeCharacterMovement, Log, this, TEXT("[DynamicCapsule] %hs MeshLoc=%s"), __FUNCTION__, *CharacterMesh->GetRelativeLocation().ToCompactString());
	
#else

	/* ------------------------ Physics Probe ------------------------ */
	const float ScaledTotalZDelta = TotalZDelta * ComponentScale;
	const float ScaledHalfHeightAdjust = HalfHeightAdjust * ComponentScale;
	const float ScaledNewHalfHeight = NewHalfHeight * ComponentScale;
	const float ScaledOldHalfHeight = OldHalfHeight * ComponentScale;

	const FVector PawnLocation = UpdatedComponent->GetComponentLocation();
	const FQuat PawnRotation = UpdatedComponent->GetComponentQuat();
	FVector ProposedLocation = PawnLocation + FVector(0.f, 0.f, ScaledTotalZDelta);
    
    const bool bIsSimulatedProxy = (CharacterOwner->GetLocalRole() == ROLE_SimulatedProxy);
    if (!bIsSimulatedProxy)
    {
	    constexpr float SweepInflation = UE_KINDA_SMALL_NUMBER * 10.f; // Preventing precision error

    	// Use current capsule for Sweeping
    	const ECollisionChannel CollisionChannel = UpdatedComponent->GetCollisionObjectType();
    	const FCollisionShape CurrentCapsuleShape = Capsule->GetCollisionShape();
    	const FCollisionShape TargetCapsuleShape = FCollisionShape::MakeCapsule(CurrentCapsuleShape.GetCapsuleRadius(), NewHalfHeight + SweepInflation);
    	
	    FCollisionQueryParams CapsuleParams(SCENE_QUERY_STAT(DynamicCapsuleTrace), false, CharacterOwner);
	    FCollisionResponseParams ResponseParam;
	    InitCollisionParams(CapsuleParams, ResponseParam);
    	
    	// Check for floor collision when moving capsule downwards
	    const float CurrentBottomZ = PawnLocation.Z - ScaledOldHalfHeight;
	    const float ProposedBottomZ = ProposedLocation.Z - ScaledNewHalfHeight;
	    const float BottomDropDist = CurrentBottomZ - ProposedBottomZ;
	    if (BottomDropDist > KINDA_SMALL_NUMBER)
	    {
		    FHitResult FloorHit;
		    FVector Start = PawnLocation;
		    FVector End = Start - FVector(0.f, 0.f, BottomDropDist + SweepInflation);
		    const bool bHitResult = MyWorld->SweepSingleByChannel(FloorHit, Start, End, FQuat::Identity, CollisionChannel, CurrentCapsuleShape, CapsuleParams, ResponseParam);

#if ENABLE_DRAW_DEBUG
		    if (CVarAnimSkillMovement_DebugStanceCollision.GetValueOnAnyThread() > 0)
		    {
			    DrawDebugCapsuleTraceSingle(MyWorld, Start, End, CurrentCapsuleShape.GetCapsuleRadius(), CurrentCapsuleShape.GetCapsuleHalfHeight(),
			    	EDrawDebugTrace::Type::ForDuration, bHitResult, FloorHit, FLinearColor::Green, FLinearColor::Red, 3.f);
		    }
#endif

		    if (bHitResult)
		    {
		    	// Adjust proposed location to avoid floor collision
			    ProposedLocation.Z = FloorHit.Location.Z - ScaledHalfHeightAdjust + MAX_FLOOR_DIST;
		    	UE_LOG_GATED(GDisplayLogCapsule, LogGeCharacterMovement, Error, this, TEXT("[DynamicCapsule] %hs Floor Hit! Adjusted ProposedLocation=%s"), __FUNCTION__, *ProposedLocation.ToCompactString());
		    }
	    }

    	// Check for ceiling collision when moving capsule upwards
	    const float CurrentToZ = PawnLocation.Z + ScaledOldHalfHeight;
	    const float ProposedTopZ = ProposedLocation.Z + ScaledNewHalfHeight;
	    const float TopLiftDist = ProposedTopZ - CurrentToZ;
	    if (TopLiftDist > KINDA_SMALL_NUMBER)
	    {
		    FHitResult CeilingHit;
		    FVector Start = PawnLocation;
		    FVector End = Start + FVector(0.f, 0.f, TopLiftDist + SweepInflation);
		    const bool bHitResult = MyWorld->SweepSingleByChannel(CeilingHit, Start, End, FQuat::Identity, CollisionChannel, CurrentCapsuleShape, CapsuleParams, ResponseParam);

#if ENABLE_DRAW_DEBUG
		    if (CVarAnimSkillMovement_DebugStanceCollision.GetValueOnAnyThread() > 0)
		    {
			    DrawDebugCapsuleTraceSingle(MyWorld, Start, End, CurrentCapsuleShape.GetCapsuleRadius(), CurrentCapsuleShape.GetCapsuleHalfHeight(),
			    	EDrawDebugTrace::Type::ForDuration, bHitResult, CeilingHit, FLinearColor::Green, FLinearColor::Red, 3.f);
		    }
#endif

		    if (bHitResult)
		    {
		    	// Adjust proposed location to avoid ceiling collision
			    ProposedLocation.Z = CeilingHit.Location.Z + ScaledHalfHeightAdjust - MAX_FLOOR_DIST;
			    UE_LOG_GATED(GDisplayLogCapsule, LogGeCharacterMovement, Log, this, TEXT("[DynamicCapsule] %hs CeilingHit Hit! Adjusted ProposedLocation=%s"), __FUNCTION__, *ProposedLocation.ToCompactString());
		    }
	    }

    	// Use target capsule for proposed location blocking test
	    FCollisionQueryParams VerifyParams(SCENE_QUERY_STAT(DynamicCapsuleVerify), false, CharacterOwner);
	    FCollisionResponseParams VerifyResponse;
	    InitCollisionParams(VerifyParams, VerifyResponse);
	    const bool bEncroached = MyWorld->OverlapBlockingTestByChannel(ProposedLocation, PawnRotation, CollisionChannel, TargetCapsuleShape, VerifyParams, VerifyResponse);

#if ENABLE_DRAW_DEBUG
	    if (CVarAnimSkillMovement_DebugStanceCollision.GetValueOnAnyThread() > 0)
	    {
		    const FTransform ActorTransform = GetActorTransform();
		    const FVector LocalOffset = ActorTransform.TransformVectorNoScale(FVector(0, 50, 0));
	    	const FVector FinalLocation = ProposedLocation + LocalOffset;
		    FHitResult theHitResult;
		    theHitResult.bBlockingHit = bEncroached;
		    theHitResult.Location = FinalLocation;
	    	DrawDebugCapsuleTraceSingle(MyWorld, FinalLocation, FinalLocation, TargetCapsuleShape.GetCapsuleRadius(), TargetCapsuleShape.GetCapsuleHalfHeight()
				 , EDrawDebugTrace::ForDuration, bEncroached, theHitResult, FLinearColor::Blue, FLinearColor::Red, 3.f);
	    }
#endif

	    if (bEncroached)
	    {
	    	// Do not perform capsule change if encorached eventually
		    UE_LOG_GATED(GDisplayLogCapsule, LogGeCharacterMovement, Warning, this, TEXT("[DynamicCapsule] %hs Failed: Encroached (Ceilling/Floor) at ProposedLocation=%s"), __FUNCTION__, *ProposedLocation.ToCompactString());
		    return false;
	    }
    }

	/* ------------------------ Commit Changes ------------------------ */
	CurrentCapsuleStage = NewCapsuleStage;
	if (CharacterOwner->HasAuthority())
	{
		ServerCapsuleStage = NewCapsuleStage;
		// If Push Model is enabled, simple assignment won't trigger replication!
		// MARK_PROPERTY_DIRTY_FROM_NAME(UGeCharacterMovementComponent, ServerCapsuleStage, this);
	}
	UE_LOG_GATED(GDisplayLogCapsule, LogGeCharacterMovement, Log, this, TEXT("[DynamicCapsule] %hs Succeed: %s"), __FUNCTION__, *UEnum::GetValueAsString(NewCapsuleStage));

	// 1. Record state BEFORE changes for Visual Compensation
	const float PreActionCapsuleZ = UpdatedComponent->GetComponentLocation().Z;
	const float PreActionMeshRelZ = CharacterMesh->GetRelativeLocation().Z;
	
	if (bIsSimulatedProxy)
	{
		// 2.1 Apply Size Change
		Capsule->SetCapsuleHalfHeight(NewHalfHeight, true);
		bShrinkProxyCapsule = true;
	}
	else if (!bIsSimulatedProxy)
	{
		// 2.2 Apply Size Change
		Capsule->SetCapsuleHalfHeight(NewHalfHeight, true);
	
		// 3. Move Capsule
		UpdatedComponent->MoveComponent(ProposedLocation - PawnLocation, PawnRotation, false, nullptr, EMoveComponentFlags::MOVECOMP_NoFlags, ETeleportType::TeleportPhysics);
	}
	
	bForceNextFloorCheck = true;
	AdjustProxyCapsuleSize();
	
	// Record expected capsule half height for detecting external modifications
	ExpectedCapsuleHalfHeight = NewHalfHeight;

	// 4. Record state AFTER changes for Visual Compensation
	const float PostActionCapsuleZ = UpdatedComponent->GetComponentLocation().Z;
	const float WorldMoveDelta = PostActionCapsuleZ - PreActionCapsuleZ;
	
	// 5. Visual Compensation
	const float DefaultMeshZ = GetDefaultMeshZ();
	const float TotalShrinkAmount = DefaultHalfHeight - NewHalfHeight;
	const float TotalCenterShiftUp = TotalShrinkAmount + TargetConfig.CapsuleOffset;
	const float IdealMeshZ = DefaultMeshZ - TotalCenterShiftUp;

	FVector MeshRelativeLocation = CharacterMesh->GetRelativeLocation();
	float CompensatedMeshZ = PreActionMeshRelZ - WorldMoveDelta;
	
	if (FMath::IsNearlyEqual(CompensatedMeshZ, IdealMeshZ, 0.1f))
	{
		MeshRelativeLocation.Z = IdealMeshZ;
		CharacterMesh->SetRelativeLocation(MeshRelativeLocation);
		TargetMeshZOffset.Reset();
	}
	else
	{
		MeshRelativeLocation.Z = CompensatedMeshZ;
		CharacterMesh->SetRelativeLocation(MeshRelativeLocation);
		TargetMeshZOffset = IdealMeshZ;
	}
	UE_LOG_GATED(GDisplayLogCapsule, LogGeCharacterMovement, Verbose, this, TEXT("[DynamicCapsule] %hs MeshLoc=%s"),
		__FUNCTION__, *CharacterMesh->GetRelativeLocation().ToCompactString());
	
#endif
	
	if (FNetworkPredictionData_Client_Character* ClientData = GetPredictionData_Client_Character())
	{
		// Don't smooth this change in mesh position
		// if (CharacterOwner->GetLocalRole() == ROLE_SimulatedProxy || (IsNetMode(NM_ListenServer) && CharacterOwner->GetRemoteRole() == ROLE_AutonomousProxy))
		// {
		// 	ClientData->MeshTranslationOffset += ScaledHalfHeightAdjust * -GetGravityDirection();
		// 	ClientData->OriginalMeshTranslationOffset = ClientData->MeshTranslationOffset;
		// }
		
		// Don't combine this change
		if (CharacterOwner->IsLocallyControlled())
		{
			if (FSavedMove_Character* const PendingMove = ClientData->PendingMove.Get())
			{
				PendingMove->bForceNoCombine = true;
			}
		}
	}
	return true;
}

void UGeCharacterMovementComponent::InterpMeshOffset(float DeltaTime)
{
	if (!HasValidData())
	{
		return;
	}
	
	if (!TargetMeshZOffset.IsSet())
	{
		return;
	}
	
	USkeletalMeshComponent* CharacterMesh = CharacterOwner->GetMesh();
	if (CharacterMesh == nullptr) { return; }

	FVector MeshRelativeLocation = CharacterMesh->GetRelativeLocation();
	const float CurrentZ = MeshRelativeLocation.Z;
	const float DesiredZ = TargetMeshZOffset.GetValue();
	
	float NewMeshZ = FMath::FInterpTo(CurrentZ, DesiredZ, DeltaTime, InterpMeshSpeed);
	if (FMath::IsNearlyEqual(NewMeshZ, DesiredZ, 0.1f))
	{
		NewMeshZ = DesiredZ;
		TargetMeshZOffset.Reset();
	}
	MeshRelativeLocation.Z = NewMeshZ;
	CharacterMesh->SetRelativeLocation(MeshRelativeLocation);
	UE_LOG_GATED(GDisplayLogCapsule, LogGeCharacterMovement, Verbose, this, TEXT("[DynamicCapsule] %hs MeshLoc=%s"),
		__FUNCTION__, *CharacterMesh->GetRelativeLocation().ToCompactString());
}

void UGeCharacterMovementComponent::UpdateDynamicCapsule(float DeltaSeconds)
{
	if (!(bEnableDynamicCapsule && GeCharacterMovementCVars::bEnableDynamicCapusle))
	{
		return;
	}
	
	if (!HasValidData())
	{
		return;
	}

	ON_SCOPE_EXIT 
	{ 
		InterpMeshOffset(DeltaSeconds); 
	};
	
	// Handle pending capsule restoration
	if (bPendingCapsuleRestore)
	{
		const bool bRestoreResult = SetCapsuleStage(EJumpCapsuleStage::FullSize);
		if (bRestoreResult)
		{
			bPendingCapsuleRestore = false;
			UE_LOG_GATED(GDisplayLogCapsule, LogGeCharacterMovement, Log, this, TEXT("[DynamicCapsule] %hs Successfully restored capsule"), __FUNCTION__);
		}
		return;
	}
	
	if (!bIsDynamicCapsuleActive)
	{
		return;
	}
	
	// Check for external capsule modifications
	if (ExpectedCapsuleHalfHeight > UE_KINDA_SMALL_NUMBER)
	{
		UCapsuleComponent* Capsule = CharacterOwner->GetCapsuleComponent();
		if (Capsule != nullptr)
		{
			const float CurrentHalfHeight = Capsule->GetUnscaledCapsuleHalfHeight();
			if (!FMath::IsNearlyEqual(CurrentHalfHeight, ExpectedCapsuleHalfHeight, 1.f))
			{
				// External modification detected, use clear-only mode
				InterruptDynamicCapsule(false);
				return;
			}
		}
	}
	
	// Handle root motion: use restore mode
	if (HasRootMotionSources())
	{
		InterruptDynamicCapsule(true);
		return;
	}
	
	if (IsFalling())
	{
		AccumulatedJumpTime += DeltaSeconds;
		SetCapsuleStage(CalculateDesiredStage());
	}
	else
	{
		ResetDynamicCapsule();
	}
}

void UGeCharacterMovementComponent::ResetDynamicCapsule()
{
	if (!(bEnableDynamicCapsule && GeCharacterMovementCVars::bEnableDynamicCapusle))
	{
		return;
	}
	
	if (!HasValidData())
	{
		return;
	}
	
	const bool bResetResult = SetCapsuleStage(EJumpCapsuleStage::FullSize);
	if (bResetResult)
	{
		SetDynamicCapsuleActive(false);
		return;
	}
	
	if (!bResetResult && CharacterOwner->IsLocallyControlled() && IsMovingOnGround())
	{
		const bool bIsCrouching = IsCrouching();
		if (!bIsCrouching && CanCrouchInCurrentState())
		{
			Crouch(false);
		}
		// UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(CharacterOwner.Get(), GetDefault<US1GameTagSettings>()->InputCrouchTag, FGameplayEventData());	
	}
	
	if (IsCrouching())
	{
		SetDynamicCapsuleActive(false);
	}
}

void UGeCharacterMovementComponent::ClearDynamicCapsuleState()
{
	AccumulatedJumpTime = 0.f;
	ActualJumpApexTime = 0.f;
	ExpectedJumpApexTime = 0.f;
    
	// Reset stage trackers
	MaxReachedStage = EJumpCapsuleStage::FullSize;
	
	// Clear pending restore flag and expected capsule size
	bPendingCapsuleRestore = false;
	ExpectedCapsuleHalfHeight = 0.f;
}

void UGeCharacterMovementComponent::OnDynamicCapsuleBegin()
{
	if (!(bEnableDynamicCapsule && GeCharacterMovementCVars::bEnableDynamicCapusle))
	{
		return;
	}
	
	if (!HasValidData())
	{
		return;
	}
	
	UE_LOG_GATED(GDisplayLogCapsule, LogGeCharacterMovement, Warning, this, TEXT("[DynamicCapsule] %hs"), __FUNCTION__);
	ensureMsgf(CurrentCapsuleStage == EJumpCapsuleStage::FullSize, TEXT("Unexpect CapsuleStage=%s"), *UEnum::GetValueAsString(CurrentCapsuleStage));
	
	SetDynamicCapsuleActive(true);
	
	// Initialize expected capsule half height
	UCapsuleComponent* Capsule = CharacterOwner->GetCapsuleComponent();
	if (Capsule != nullptr)
	{
		ExpectedCapsuleHalfHeight = Capsule->GetUnscaledCapsuleHalfHeight();
	}

	const float theGravityZ = GetGravityZ();
	if (theGravityZ < 0.f)
	{
		ExpectedJumpApexTime = FMath::Abs(JumpZVelocity / theGravityZ);			
	}
	else
	{
		ExpectedJumpApexTime = 0.f;
	}
}

void UGeCharacterMovementComponent::OnDynamicCapsuleEnd()
{
	if (!(bEnableDynamicCapsule && GeCharacterMovementCVars::bEnableDynamicCapusle))
	{
		return;
	}
	
	if (!HasValidData())
	{
		return;
	}
	
	ResetDynamicCapsule();
	UE_LOG_GATED(GDisplayLogCapsule, LogGeCharacterMovement, Warning, this, TEXT("[DynamicCapsule] %hs"), __FUNCTION__);
}

void UGeCharacterMovementComponent::SetDynamicCapsuleActive(bool bActive)
{
	if (bIsDynamicCapsuleActive == bActive)
	{
		return;
	}
	bIsDynamicCapsuleActive = bActive;
	UE_LOG_GATED(GDisplayLogCapsule, LogGeCharacterMovement, Log, this, TEXT("[DynamicCapsule] %hs bActive=%d"), __FUNCTION__, bActive);
	
	ClearDynamicCapsuleState();
}

void UGeCharacterMovementComponent::InterruptDynamicCapsule(bool bRestoreCapsule)
{
	if (!bIsDynamicCapsuleActive && !bPendingCapsuleRestore)
	{
		return;
	}
	
	// Disable dynamic capsule adjustment
	SetDynamicCapsuleActive(false);
	
	if (bRestoreCapsule)
	{
		// Attempt to restore capsule immediately
		const bool bRestoreResult = SetCapsuleStage(EJumpCapsuleStage::FullSize);
		if (bRestoreResult)
		{
			// Successfully restored, clear the pending flag
			bPendingCapsuleRestore = false;
			UE_LOG_GATED(GDisplayLogCapsule, LogGeCharacterMovement, Log, this, TEXT("[DynamicCapsule] %hs Interrupted and restored capsule immediately"), __FUNCTION__);
		}
		else
		{
			// Failed to restore, set flag to try every frame
			bPendingCapsuleRestore = true;
			UE_LOG_GATED(GDisplayLogCapsule, LogGeCharacterMovement, Warning, this, TEXT("[DynamicCapsule] %hs Interrupted but failed to restore capsule, will retry every frame"), __FUNCTION__);
		}
	}
	else
	{
		// Clear pending restore flag
		bPendingCapsuleRestore = false;
		// Clear mesh offset target to stop interpolation in clear-only mode
		TargetMeshZOffset.Reset();
		UE_LOG_GATED(GDisplayLogCapsule, LogGeCharacterMovement, Log, this, TEXT("[DynamicCapsule] %hs Interrupted and cleared runtime data"), __FUNCTION__);
	}
}

FGameplayTag UGeCharacterMovementComponent::GetMovementModeTag(EMovementMode InMovementMode, uint8 InCustomMode) const
{
	// Default implementation: convert movement mode to GameplayTag
	// Override this function to provide custom mapping
	
	FString TagName;
	
	switch (InMovementMode)
	{
	case MOVE_None:
		TagName = TEXT("MovementMode.None");
		break;
	case MOVE_Walking:
		TagName = TEXT("MovementMode.Walking");
		break;
	case MOVE_NavWalking:
		TagName = TEXT("MovementMode.NavWalking");
		break;
	case MOVE_Falling:
		TagName = TEXT("MovementMode.Falling");
		break;
	case MOVE_Swimming:
		TagName = TEXT("MovementMode.Swimming");
		break;
	case MOVE_Flying:
		TagName = TEXT("MovementMode.Flying");
		break;
	case MOVE_Custom:
		// For custom modes, you may want to use CustomMode value or override this function
		TagName = FString::Printf(TEXT("MovementMode.Custom.%d"), InCustomMode);
		break;
	case MOVE_MAX:
	default:
		TagName = TEXT("MovementMode.Unknown");
		break;
	}
	
	return FGameplayTag::RequestGameplayTag(FName(*TagName), false);
}

bool UGeCharacterMovementComponent::ShouldRestoreCapsuleOnMovementModeChange(EMovementMode NewMovementMode, uint8 NewCustomMode) const
{
	// Get the GameplayTag for the new movement mode
	const FGameplayTag MovementTag = GetMovementModeTag(NewMovementMode, NewCustomMode);
	
	// Check if the tag is in the restore capsule container (1.1)
	if (MovementModeTagsRestoreCapsule.HasTag(MovementTag))
	{
		return true;
	}
	
	// Check if the tag is in the clear data container (1.2)
	if (MovementModeTagsClearData.HasTag(MovementTag))
	{
		return false;
	}
	
	// Default behavior: restore capsule if not configured
	return true;
}

#pragma endregion

void UGeCharacterMovementComponent::DisplayDebugForGame(float DeltaTime, bool bPrintToScreen, bool bPrintToLog)
{
	if (!HasValidData())
	{
		return;
	}
	
	USkeletalMeshComponent* CharacterMesh = CharacterOwner->GetMesh();
	const FString theLocalRole = UEnum::GetValueAsString(GetOwnerRole());
	const FString theObjectHash = FString::Printf(TEXT("%u"), GetTypeHash(FObjectKey{this}));
	
	const FTransform ActorTransform = GetActorTransform();
	const FVector LocalVelocity = UKismetMathLibrary::InverseTransformDirection(ActorTransform, GetCurrentVelocity());
	const FVector LocalAcceleration = UKismetMathLibrary::InverseTransformDirection(ActorTransform, GetCurrentAcceleration());
	const FString theDebugString = FString::Printf(
		TEXT("[%s] [Frame:%d] MaxSpeed=%.2f, Speed2D=%.2f, Speed=%.2f, Acc=%.2f, Loc(%s), Rot(%s), MeshRot:%s, MeshLoc:%s, LocalVel:%s, LocalAcc:%s")
		, *theLocalRole
		, GFrameNumber % 1000
		, GetMaxSpeed()
		, GetCurrentVelocity().Size2D()
		, GetCurrentVelocity().Size()
		, GetCurrentAcceleration().Size()
		, *ActorTransform.GetLocation().ToCompactString()
		, *ActorTransform.GetRotation().Rotator().ToCompactString()
		, *CharacterMesh->GetComponentRotation().ToCompactString()
		, *CharacterMesh->GetComponentLocation().ToCompactString()
		, *LocalVelocity.ToCompactString()
		, *LocalAcceleration.ToCompactString());

	UKismetSystemLibrary::PrintString(this, theDebugString, bPrintToScreen, bPrintToLog, FLinearColor::White, 0.f, FName(*theObjectHash));

	const FVector  theCollisionLoc = UpdatedComponent->GetComponentLocation();
	const FRotator theCollisionRot = UpdatedComponent->GetComponentRotation();
	const FVector  theCollisionBaseLoc = GetActorFeetLocation();
	const FRotator theControlRotation = CharacterOwner->GetControlRotation();
	const FVector theControlDirection = theControlRotation.Vector().GetSafeNormal();
	const FVector theVelDirection = GetCurrentVelocity();
	const FVector theAccDirection = GetCurrentAcceleration();
	const FVector theInputDirection = GetLastInputVector();
	const FVector ZAxisBias = FVector(0.f, 0.f, 10.f);

	// Get the collision data
	float theRadius, theHeight;
	CharacterOwner->GetSimpleCollisionCylinder(theRadius, theHeight);

	// Define draw parameter
	const float theScaleLength = theRadius * 3.f;
	constexpr float theArrowSize = 50.f;
	constexpr float theThickness = 3.f;

	// Draw character capsule collision
	UKismetSystemLibrary::DrawDebugCapsule(this, theCollisionLoc, theHeight, theRadius, theCollisionRot, FColor::Black, 0.f, 0.5f);

	// Draw looking rotation cone
	UKismetSystemLibrary::DrawDebugConeInDegrees(this, CharacterOwner->GetPawnViewLocation(), theControlDirection, 50.f, 30.f, 30.f, 8, FColor::White, 0.f, 0.5f);

	// Draw desired character rotation direction
	FRotator theDesiredRotation(0.f, theControlRotation.Yaw, 0.f);
	UKismetSystemLibrary::DrawDebugArrow(this, theCollisionBaseLoc, theCollisionBaseLoc + theDesiredRotation.Vector() * theScaleLength, theArrowSize, FColor::Red, 0.f, theThickness);

	// Draw character rotation
	UKismetSystemLibrary::DrawDebugArrow(this, theCollisionBaseLoc, theCollisionBaseLoc + CharacterOwner->GetActorForwardVector() * theScaleLength, theArrowSize, FColor::Blue, 0.f, theThickness);

	// Draw character velocity
	if (!theVelDirection.IsNearlyZero())
	{
		const float Modifier = GetMaxSpeed() > 0.f ? theVelDirection.Size() / GetMaxSpeed() : 1.f;
		UKismetSystemLibrary::DrawDebugArrow(this, theCollisionBaseLoc,
			theCollisionBaseLoc + theVelDirection.GetSafeNormal() * theScaleLength * Modifier, theArrowSize, FLinearColor::Green, 0.f, theThickness);
	}

	// Draw character acceleration
	if (!theAccDirection.IsNearlyZero())
	{
		const float Modifier = GetMaxAcceleration() > 0.f ? theAccDirection.Size() / GetMaxAcceleration() : 1.f;
		UKismetSystemLibrary::DrawDebugArrow(this, theCollisionBaseLoc + ZAxisBias,
			theCollisionBaseLoc + ZAxisBias + theAccDirection.GetSafeNormal() * theScaleLength * Modifier, theArrowSize, FLinearColor::Yellow, 0.f, theThickness);
	}

	// Draw input vector
	if (!theInputDirection.IsNearlyZero())
	{
		UKismetSystemLibrary::DrawDebugArrow(this, theCollisionBaseLoc + ZAxisBias * 2,
			theCollisionBaseLoc + ZAxisBias * 2 + theInputDirection.GetSafeNormal() * theScaleLength, theArrowSize, FLinearColor::Gray, 0.f, theThickness);
	}

	// Draw last location and current location
	// UKismetSystemLibrary::DrawDebugArrow(this, GetLastUpdateLocation(), UpdatedComponent->GetComponentLocation(), 1.f, FLinearColor::Yellow, float(theDebugValue), 1.5f);

	// Draw current location
	UKismetSystemLibrary::DrawDebugPoint(this, UpdatedComponent->GetComponentLocation(), 5.f, FLinearColor::Yellow, 2.f);
}

