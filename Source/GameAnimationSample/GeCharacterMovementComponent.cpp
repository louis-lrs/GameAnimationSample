// Fill out your copyright notice in the Description page of Project Settings.


#include "GeCharacterMovementComponent.h"
#include "GeCharacterMovementReplication.h"

#include "EnhancedLog.h"
#include "GameAnimationSample.h"
#include "KismetTraceUtils.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/ScopedMovementUpdate.h"
#include "EngineLogs.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/WorldSettings.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetMathLibrary.h"
#include "Net/UnrealNetwork.h"
#include "Net/Core/PushModel/PushModel.h"

static TAutoConsoleVariable<int32> CVarAnimSkillMovement_DebugDynamicCapsule(TEXT("a.AnimSkill.Movement.DebugDynamicCapsule"),0,TEXT("0: Disable, 1: Autonomous, 2: Client, 3: DedicatedServer, 4: Simulated Proxy, 5: All"));
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

// Helper function to check if debug should be enabled based on CVAR value and role
static bool ShouldEnableDebugForRole(int32 DebugMode, const AActor* Actor)
{
	if (DebugMode <= 0 || !Actor)
	{
		return false;
	}
	
	// Check client ID filter
	const auto ClientID = CVarAnimSkillMovement_DebugClientID.GetValueOnAnyThread();
	if (ClientID > 0)
	{
		if (UE::GetPlayInEditorID() != ClientID)
		{
			return false;
		}
	}
	
	const UWorld* World = Actor->GetWorld();
	if (!World)
	{
		return false;
	}
	
	const ENetRole LocalRole = Actor->GetLocalRole();
	const ENetMode NetMode = World->GetNetMode();
	
	// Filter by role based on debug mode
	if (DebugMode == 1)
	{
		// Autonomous/Authority only
		return (LocalRole == ROLE_AutonomousProxy && NetMode == NM_Client)
			|| (LocalRole == ROLE_Authority && NetMode == NM_Standalone);
	}
	else if (DebugMode == 2)
	{
		// Client/Standalone only
		return NetMode == NM_Client || NetMode == NM_Standalone;
	}
	else if (DebugMode == 3)
	{
		// DedicatedServer only
		return NetMode == NM_DedicatedServer;
	}
	else if (DebugMode == 4)
	{
		// SimulatedProxy only
		return LocalRole == ROLE_SimulatedProxy;
	}
	
	// Mode 5 or any other value: enable for all
	return true;
}

// Sets default values for this component's properties
UGeCharacterMovementComponent::UGeCharacterMovementComponent()
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = true;
	
	// Set custom network move data container for syncing AccumulatedJumpTime
	SetNetworkMoveDataContainer(GeNetworkMoveDataContainer);
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
	
	if (!HasValidData())
	{
		return;
	}
	
	// Debug movement info
	const int32 theDebugMovement = CVarAnimSkillMovement_DebugMovement.GetValueOnAnyThread();
	if (ShouldEnableDebugForRole(theDebugMovement, CharacterOwner))
	{
		DisplayDebugForGame(DeltaTime);
	}
	
	// Debug dynamic capsule info
	const int32 theDebugDynamicCapsule = CVarAnimSkillMovement_DebugDynamicCapsule.GetValueOnAnyThread();
	if (ShouldEnableDebugForRole(theDebugDynamicCapsule, CharacterOwner))
	{
		const FString DebugInfo = GetDynamicCapsuleDebugInfo();
		// Use unique key by appending suffix to avoid conflict with DisplayDebugForGame
		const FString theObjectHash = FString::Printf(TEXT("%u_DynamicCapsule"), GetTypeHash(FObjectKey{this}));
		UKismetSystemLibrary::PrintString(this, DebugInfo, true, false, FLinearColor::White, 0.f, FName(*theObjectHash));
	}
	
	// if (CharacterOwner->GetLocalRole() == ROLE_SimulatedProxy)
	// {
	// 	InterpMeshOffset(DeltaTime);
	// }
}

void UGeCharacterMovementComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	
	// ServerCapsuleStage replication settings:
	// - COND_SkipOwner: Don't replicate to owning client (they predict it)
	// - REPNOTIFY_OnChanged: Trigger OnRep only when value actually changes
	// - bIsPushBased = true: Support PushModel for efficient replication
	FDoRepLifetimeParams CapsuleStageParams;
	CapsuleStageParams.Condition = COND_SkipOwner;
	CapsuleStageParams.RepNotifyCondition = REPNOTIFY_OnChanged;
	CapsuleStageParams.bIsPushBased = true;
	DOREPLIFETIME_WITH_PARAMS_FAST(UGeCharacterMovementComponent, ServerCapsuleStage, CapsuleStageParams);
}

FNetworkPredictionData_Client* UGeCharacterMovementComponent::GetPredictionData_Client() const
{
	if (ClientPredictionData == nullptr)
	{
		UGeCharacterMovementComponent* MutableThis = const_cast<UGeCharacterMovementComponent*>(this);
		MutableThis->ClientPredictionData = new FNetworkPredictionData_Client_GeCharacter(*this);
	}
	
	return ClientPredictionData;
}

FNetworkPredictionData_Server* UGeCharacterMovementComponent::GetPredictionData_Server() const
{
	if (ServerPredictionData == nullptr)
	{
		UGeCharacterMovementComponent* MutableThis = const_cast<UGeCharacterMovementComponent*>(this);
		MutableThis->ServerPredictionData = new FNetworkPredictionData_Server_GeCharacter(*this);
	}
	
	return ServerPredictionData;
}

bool UGeCharacterMovementComponent::CanDelaySendingMove(const FSavedMovePtr& NewMovePtr)
{
	// Call parent implementation first
	if (!Super::CanDelaySendingMove(NewMovePtr))
	{
		return false;
	}
	
	// Don't delay moves that change capsule stage over the course of the move
	if (const FSavedMove_GeCharacter* GeNewMove = static_cast<const FSavedMove_GeCharacter*>(NewMovePtr.Get()))
	{
		if (GeNewMove->StartCapsuleStage != GeNewMove->EndCapsuleStage)
		{
			return false;
		}
	}
	
	return true;
}

void UGeCharacterMovementComponent::ServerMove_PerformMovement(const FCharacterNetworkMoveData& MoveData)
{
	if (!HasValidData() || !IsActive())
	{
		return;
	}

	const float ClientTimeStamp = MoveData.TimeStamp;

	FVector ClientAccel = MoveData.Acceleration;

	// Convert the move's acceleration to worldspace if necessary
	if (MovementBaseUtility::IsDynamicBase(MoveData.MovementBase))
	{
		MovementBaseUtility::TransformDirectionToWorld(MoveData.MovementBase, MoveData.MovementBaseBoneName, MoveData.Acceleration, ClientAccel);
	}

	const uint8 ClientMoveFlags = MoveData.CompressedMoveFlags;
	const FRotator ClientControlRotation = MoveData.ControlRotation;

	FNetworkPredictionData_Server_Character* ServerData = GetPredictionData_Server_Character();
	check(ServerData);

	if (!VerifyClientTimeStamp(ClientTimeStamp, *ServerData))
	{
		const float ServerTimeStamp = ServerData->CurrentClientTimeStamp;
		// This is more severe if the timestamp has a large discrepancy and hasn't been recently reset.
		static constexpr float NetServerMoveTimestampExpiredWarningThreshold = 1.0f;
		if (ServerTimeStamp > 1.0f && FMath::Abs(ServerTimeStamp - ClientTimeStamp) > NetServerMoveTimestampExpiredWarningThreshold)
		{
			UE_LOG(LogNetPlayerMovement, Warning, TEXT("ServerMove: TimeStamp expired: %f, CurrentTimeStamp: %f, Character: %s"), ClientTimeStamp, ServerTimeStamp, *GetNameSafe(CharacterOwner));
		}
		else
		{
			UE_LOG(LogNetPlayerMovement, Log, TEXT("ServerMove: TimeStamp expired: %f, CurrentTimeStamp: %f, Character: %s"), ClientTimeStamp, ServerTimeStamp, *GetNameSafe(CharacterOwner));
		}
		return;
	}

	bool bServerReadyForClient = true;
	APlayerController* PC = Cast<APlayerController>(CharacterOwner->GetController());
	if (PC)
	{
		bServerReadyForClient = PC->NotifyServerReceivedClientData(CharacterOwner, ClientTimeStamp);
		if (!bServerReadyForClient)
		{
			ClientAccel = FVector::ZeroVector;
		}
	}

	const UWorld* MyWorld = GetWorld();
	const float DeltaTime = ServerData->GetServerMoveDeltaTime(ClientTimeStamp, CharacterOwner->GetActorTimeDilation(*MyWorld));

	// Defer all mesh child updates until all movement completes.
	FScopedMovementUpdate ScopedMeshUpdate(CharacterOwner->GetMesh(), EScopedUpdate::DeferredUpdates);

	if (DeltaTime > 0.f)
	{
		ServerData->CurrentClientTimeStamp = ClientTimeStamp;
		ServerData->ServerAccumulatedClientTimeStamp += DeltaTime;
		ServerData->ServerTimeStamp = MyWorld->GetTimeSeconds();
		ServerData->ServerTimeStampLastServerMove = ServerData->ServerTimeStamp;

		if (AController* CharacterController = Cast<AController>(CharacterOwner->GetController()))
		{
			CharacterController->SetControlRotation(ClientControlRotation);
		}

		if (!bServerReadyForClient)
		{
			return;
		}

		// Perform actual movement
		if ((MyWorld->GetWorldSettings()->GetPauserPlayerState() == nullptr))
		{
			if (const FGeCharacterNetworkMoveData* GeMoveData = static_cast<const FGeCharacterNetworkMoveData*>(&MoveData))
			{
				OnApplyJumpTimeData(*GeMoveData);
			}
			
			FScopedMovementUpdate ScopedCapsuleUpdate(bEnableScopedMovementUpdates ? UpdatedComponent : nullptr, EScopedUpdate::DeferredUpdates);
			if (PC)
			{
				PC->UpdateRotation(DeltaTime);
			}

			MoveAutonomous(ClientTimeStamp, DeltaTime, ClientMoveFlags, ClientAccel);
		}

		UE_CLOG(CharacterOwner && UpdatedComponent, LogNetPlayerMovement, VeryVerbose, TEXT("ServerMove Time %f Acceleration %s Velocity %s Position %s Rotation %s DeltaTime %f Mode %s MovementBase %s.%s (Dynamic:%d)"),
			ClientTimeStamp, *ClientAccel.ToString(), *Velocity.ToString(), *UpdatedComponent->GetComponentLocation().ToString(), *UpdatedComponent->GetComponentRotation().ToCompactString(), DeltaTime, *GetMovementName(),
			*GetNameSafe(GetMovementBase()), *CharacterOwner->GetBasedMovement().BoneName.ToString(), MovementBaseUtility::IsDynamicBase(GetMovementBase()) ? 1 : 0);
	}

	// Validate move only after old and first dual portion, after all moves are completed.
	if (MoveData.NetworkMoveType == FCharacterNetworkMoveData::ENetworkMoveType::NewMove)
	{
		ServerMoveHandleClientError(ClientTimeStamp, DeltaTime, ClientAccel, MoveData.Location, MoveData.MovementBase, MoveData.MovementBaseBoneName, MoveData.MovementMode);
	}
}

void UGeCharacterMovementComponent::OnApplyJumpTimeData(const FGeCharacterNetworkMoveData& GeMoveData)
{
	// 从 FFloat16 直接获取浮点值
	const float ReceivedActualJumpApexTime = GeMoveData.SavedActualJumpApexTime.GetFloat();
	const float OldActualJumpApexTime = ActualJumpApexTime;
	if (ReceivedActualJumpApexTime > UE_KINDA_SMALL_NUMBER)
	{
		if (ActualJumpApexTime <= UE_KINDA_SMALL_NUMBER || 
			FMath::Abs(ActualJumpApexTime - ReceivedActualJumpApexTime) > UE_KINDA_SMALL_NUMBER)
		{
			SetActualJumpApexTime(ReceivedActualJumpApexTime);
			UE_LOG_GATED(GDisplayLogCapsule, LogGeCharacterMovement, Warning, this,
						 TEXT("[DynamicCapsule] ServerMove: Applied client ActualJumpApexTime: %.4f -> %.4f, Diff=%.4f"), 
						 OldActualJumpApexTime, ActualJumpApexTime, ActualJumpApexTime - OldActualJumpApexTime);
		}
	}

	if (FNetworkPredictionData_Server_GeCharacter* GeServerData = static_cast<FNetworkPredictionData_Server_GeCharacter*>(GetPredictionData_Server()))
	{
		const float ReceivedAccumulatedJumpTime = GeMoveData.SavedAccumulatedJumpTime.GetFloat();
		const float OldAccumulatedJumpTime = AccumulatedJumpTime;
		const float RealAccumulatedJumpTime = GeServerData->GetServerAccumulatedJumpTime(
			ReceivedAccumulatedJumpTime, 
			AccumulatedJumpTime);
		SetAccumulatedJumpTime(RealAccumulatedJumpTime);
		
		const float TimeDiff = FMath::Abs(RealAccumulatedJumpTime - ReceivedAccumulatedJumpTime);
		if (TimeDiff > 0.01f)
		{
			UE_LOG_GATED(GDisplayLogCapsule, LogGeCharacterMovement, Warning, this,
				TEXT("[DynamicCapsule] ServerMove: AccumulatedJumpTime Sync: Client=%.4f, ServerOld=%.4f, ServerNew=%.4f, Diff=%.4f, CurrentStage=%s"),
				ReceivedAccumulatedJumpTime, OldAccumulatedJumpTime, RealAccumulatedJumpTime, TimeDiff,
				*UEnum::GetValueAsString(CurrentCapsuleStage));
		}
	}
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
	const float TimeDiff = ActualJumpApexTime - ExpectedJumpApexTime;
	UE_LOG_GATED(GDisplayLogCapsule, LogGeCharacterMovement, Warning, this,
		TEXT("[DynamicCapsule] %hs ExpectedJumpApexTime=%.4f, ActualJumpApexTime=%.4f, Diff=%.4f"),
		__FUNCTION__, ExpectedJumpApexTime, ActualJumpApexTime, TimeDiff);
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
	// Simulated proxy receives capsule stage from server
	// Apply the stage change, which will handle mesh offset interpolation
	if (CharacterOwner->GetLocalRole() == ROLE_SimulatedProxy)
	{
		UE_LOG_GATED(GDisplayLogCapsule, LogGeCharacterMovement, Log, this, TEXT("[DynamicCapsule] %hs Received ServerCapsuleStage=%s"), 
			__FUNCTION__, *UEnum::GetValueAsString(ServerCapsuleStage));
		SetCapsuleStage(ServerCapsuleStage);
	}
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
			UE_LOG_GATED(GDisplayLogCapsule, LogGeCharacterMovement, Verbose, this, 
				TEXT("[DynamicCapsule] %hs Invalid ExpectedJumpApexTime=%.4f, returning FullSize"), __FUNCTION__, ExpectedJumpApexTime);
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
		UE_LOG_GATED(GDisplayLogCapsule, LogGeCharacterMovement, Verbose, this, 
			TEXT("[DynamicCapsule] %hs Ascending: Progress=%.4f (Accumulated=%.4f, ExpectedJumpApex=%.4f), Vel=%s, Target=%s, MaxReached=%s"),
			__FUNCTION__, Progress, AccumulatedJumpTime, ExpectedJumpApexTime, *Velocity.ToCompactString(), *UEnum::GetValueAsString(TargetStage), *UEnum::GetValueAsString(MaxReachedStage));
	}
	else
	{
		// Don't enter a shrunk stage if we didn't reach it during ascent.
		if (MaxReachedStage == EJumpCapsuleStage::FullSize)
		{
			UE_LOG_GATED(GDisplayLogCapsule, LogGeCharacterMovement, Verbose, this,
				TEXT("[DynamicCapsule] %hs MaxReachedStage=FullSize, returning FullSize"), __FUNCTION__);
			return EJumpCapsuleStage::FullSize;
		}
		
		// Safety check: prevent division by zero
		if (ActualJumpApexTime <= UE_KINDA_SMALL_NUMBER)
		{
			UE_LOG_GATED(GDisplayLogCapsule, LogGeCharacterMovement, Verbose, this,
				TEXT("[DynamicCapsule] %hs Invalid ActualJumpApexTime=%.4f, returning FullSize"), __FUNCTION__, ActualJumpApexTime);
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
		UE_LOG_GATED(GDisplayLogCapsule, LogGeCharacterMovement, Verbose, this, 
			TEXT("[DynamicCapsule] %hs Descending: FallingProgress=%.4f (Accumulated=%.4f, ActualJumpApex=%.4f), Vel=%s, Target=%s, MaxReached=%s"), 
			__FUNCTION__, FallingProgress, AccumulatedJumpTime, ActualJumpApexTime, *Velocity.ToCompactString(), *UEnum::GetValueAsString(TargetStage), *UEnum::GetValueAsString(MaxReachedStage));
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

	/* ------------------------ Physics Probe ------------------------ */
	const float ScaledTotalZDelta = TotalZDelta * ComponentScale;
	const float ScaledHalfHeightAdjust = HalfHeightAdjust * ComponentScale;
	const float ScaledNewHalfHeight = NewHalfHeight * ComponentScale;
	const float ScaledOldHalfHeight = OldHalfHeight * ComponentScale;

	const FVector PawnLocation = UpdatedComponent->GetComponentLocation();
	const FQuat PawnRotation = UpdatedComponent->GetComponentQuat();
	FVector ProposedLocation = PawnLocation + FVector(0.f, 0.f, ScaledTotalZDelta);
	
	UE_LOG_GATED(GDisplayLogCapsule, LogGeCharacterMovement, Log, this, 
		TEXT("[DynamicCapsule] %hs Pending: %s -> %s, OldHalfHeight=%.2f, NewHalfHeight=%.2f, AccumulatedJumpTime=%.4f"), __FUNCTION__,
		*UEnum::GetValueAsString(CurrentCapsuleStage), *UEnum::GetValueAsString(NewCapsuleStage), OldHalfHeight, NewHalfHeight, AccumulatedJumpTime);
	
    const bool bIsSimulatedProxy = (CharacterOwner->GetLocalRole() == ROLE_SimulatedProxy);
    if (!bIsSimulatedProxy)
    {
	    UE_LOG_GATED(GDisplayLogCapsule, LogGeCharacterMovement, Verbose, this,
		    TEXT("[DynamicCapsule] %hs CollisionCheck: PawnLocation=%s, ProposedLocation=%s, ScaledTotalZDelta=%.4f"),
		    __FUNCTION__, *PawnLocation.ToCompactString(), *ProposedLocation.ToCompactString(), ScaledTotalZDelta);
	    
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
		    	UE_LOG_GATED(GDisplayLogCapsule, LogGeCharacterMovement, Error, this, TEXT("[DynamicCapsule] %hs Floor Hit! Adjusted ProposedLocation=%s, HitLocation=%s, BottomDropDist=%.4f"), 
			    	__FUNCTION__, *ProposedLocation.ToCompactString(), *FloorHit.Location.ToCompactString(), BottomDropDist);
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
			    UE_LOG_GATED(GDisplayLogCapsule, LogGeCharacterMovement, Error, this, TEXT("[DynamicCapsule] %hs CeilingHit Hit! Adjusted ProposedLocation=%s, HitLocation=%s, TopLiftDist=%.4f"), 
				    __FUNCTION__, *ProposedLocation.ToCompactString(), *CeilingHit.Location.ToCompactString(), TopLiftDist);
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
		    UE_LOG_GATED(GDisplayLogCapsule, LogGeCharacterMovement, Warning, this, TEXT("[DynamicCapsule] %hs FAILED: Encroached (Ceilling/Floor) at ProposedLocation=%s"), __FUNCTION__, *ProposedLocation.ToCompactString());
		    return false;
	    }
	    
	    UE_LOG_GATED(GDisplayLogCapsule, LogGeCharacterMovement, Verbose, this, TEXT("[DynamicCapsule] %hs CollisionCheck PASSED: ProposedLocation=%s"), __FUNCTION__, *ProposedLocation.ToCompactString());
    }

	/* ------------------------ Commit Changes ------------------------ */
	CurrentCapsuleStage = NewCapsuleStage;
	if (CharacterOwner->HasAuthority())
	{
		ServerCapsuleStage = NewCapsuleStage;
		
		// Support PushModel: Mark property dirty to trigger replication
		MARK_PROPERTY_DIRTY_FROM_NAME(UGeCharacterMovementComponent, ServerCapsuleStage, this);
		
		// Force network update to prevent replication delay causing visual desync
		CharacterOwner->ForceNetUpdate();
		
		UE_LOG_GATED(GDisplayLogCapsule, LogGeCharacterMovement, Log, this, TEXT("[DynamicCapsule] %hs SUCCEED (Server): %s, AccumulatedJumpTime=%.4f"), 
			__FUNCTION__, *UEnum::GetValueAsString(NewCapsuleStage), AccumulatedJumpTime);
	}
	else
	{
		UE_LOG_GATED(GDisplayLogCapsule, LogGeCharacterMovement, Log, this, TEXT("[DynamicCapsule] %hs SUCCEED (Client): %s, AccumulatedJumpTime=%.4f"), 
			__FUNCTION__, *UEnum::GetValueAsString(NewCapsuleStage), AccumulatedJumpTime);
	}

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
		const FVector MoveDelta = ProposedLocation - PawnLocation;
		const bool bMoveResult = UpdatedComponent->MoveComponent(MoveDelta, PawnRotation, false, nullptr, EMoveComponentFlags::MOVECOMP_NoFlags, ETeleportType::TeleportPhysics);
		
		if (!bMoveResult)
		{
			UE_LOG_GATED(GDisplayLogCapsule, LogGeCharacterMovement, Error, this, TEXT("[DynamicCapsule] %hs MoveComponent FAILED: MoveDelta=%s, ProposedLocation=%s, PawnLocation=%s"),
				__FUNCTION__, *MoveDelta.ToCompactString(), *ProposedLocation.ToCompactString(), *PawnLocation.ToCompactString());
		}
		
		// Force update mesh transform in case not propagated because of IsDeferringMovementUpdates
		UpdatedComponent->UpdateChildTransforms();
		
		const FVector PostMoveLocation = UpdatedComponent->GetComponentLocation();
		if (!PostMoveLocation.Equals(ProposedLocation, 1.0f))
		{
			UE_LOG_GATED(GDisplayLogCapsule, LogGeCharacterMovement, Error, this, TEXT("[DynamicCapsule] %hs Location Mismatch: Proposed=%s, Actual=%s, Delta=%s"),
				__FUNCTION__, *ProposedLocation.ToCompactString(), *PostMoveLocation.ToCompactString(), *(PostMoveLocation - ProposedLocation).ToCompactString());
		}
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
	
	// For simulated proxies, capsule position is controlled by server replication
	// WorldMoveDelta should be 0 or minimal, so we directly calculate mesh offset
	float CompensatedMeshZ;
	if (bIsSimulatedProxy)
	{
		// Simulated proxy: capsule position is synced from server, no world movement
		// Directly calculate mesh offset based on capsule size change
		CompensatedMeshZ = PreActionMeshRelZ;
	}
	else
	{
		// Authority/Autonomous: compensate for world movement
		CompensatedMeshZ = PreActionMeshRelZ - WorldMoveDelta;
	}
	
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
	UE_LOG_GATED(GDisplayLogCapsule, LogGeCharacterMovement, Verbose, this, TEXT("[DynamicCapsule] %hs SUCCEED: %s, MeshLoc=%s, IdealMeshZ=%.2f, CompensatedMeshZ=%.2f"),
		__FUNCTION__, *UEnum::GetValueAsString(NewCapsuleStage), *CharacterMesh->GetRelativeLocation().ToCompactString(), IdealMeshZ, CompensatedMeshZ);
	
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
	UE_LOG_GATED(GDisplayLogCapsule, LogGeCharacterMovement, Verbose, this, TEXT("[DynamicCapsule] %hs MeshLoc=%s, NewMeshZ=%.2f"),
		__FUNCTION__, *CharacterMesh->GetRelativeLocation().ToCompactString(), NewMeshZ);
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

	// Get actual frame delta time instead of potentially combined move delta
	const float ActualDeltaTime = GetWorld() ? GetWorld()->GetDeltaSeconds() : DeltaSeconds;
	
	ON_SCOPE_EXIT 
	{ 
		InterpMeshOffset(ActualDeltaTime); 
	};
	
	const bool bIsSimulatedProxy = (CharacterOwner->GetLocalRole() == ROLE_SimulatedProxy);
	
	// Simulated proxies only handle mesh offset interpolation
	// Capsule stage changes are handled via OnRep_ServerCapsuleStage
	if (bIsSimulatedProxy)
	{
		return;
	}
	
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
			if (!FMath::IsNearlyEqual(CurrentHalfHeight, ExpectedCapsuleHalfHeight, 0.01f))
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
		const float OldAccumulatedJumpTime = AccumulatedJumpTime;
		AccumulatedJumpTime += ActualDeltaTime;
		const EJumpCapsuleStage DesiredStage = CalculateDesiredStage();
		const bool bSetStageResult = SetCapsuleStage(DesiredStage);
		
		UE_LOG_GATED(GDisplayLogCapsule, LogGeCharacterMovement, Verbose, this,
				TEXT("[DynamicCapsule] %hs AccumulatedJumpTime=%.4f->%.4f, ActualDeltaTime=%.4f (ParamDeltaTime=%.4f)"),
				__FUNCTION__, OldAccumulatedJumpTime, AccumulatedJumpTime, ActualDeltaTime, DeltaSeconds);
		
		if (!bSetStageResult && DesiredStage != CurrentCapsuleStage)
		{
			UE_LOG_GATED(GDisplayLogCapsule, LogGeCharacterMovement, Warning, this,
				TEXT("[DynamicCapsule] %hs SetCapsuleStage FAILED: DesiredStage=%s, CurrentStage=%s, AccumulatedJumpTime=%.4f->%.4f, Location=%s"),
				__FUNCTION__, *UEnum::GetValueAsString(DesiredStage), *UEnum::GetValueAsString(CurrentCapsuleStage),
				OldAccumulatedJumpTime, AccumulatedJumpTime, *UpdatedComponent->GetComponentLocation().ToCompactString());
		}
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
	
	// Only reset if active or has pending restore, otherwise already interrupted
	if (!bIsDynamicCapsuleActive && !bPendingCapsuleRestore)
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
		CharacterOwner->Crouch(false);
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
	
	// Reset capsule stage to FullSize before starting new jump
	// This ensures clean state even if previous jump was interrupted
	if (CurrentCapsuleStage != EJumpCapsuleStage::FullSize)
	{
		UE_LOG_GATED(GDisplayLogCapsule, LogGeCharacterMovement, Warning, this, TEXT("[DynamicCapsule] %hs Resetting from %s to FullSize"), 
			__FUNCTION__, *UEnum::GetValueAsString(CurrentCapsuleStage));
		CurrentCapsuleStage = EJumpCapsuleStage::FullSize;
	}
	
	UE_LOG_GATED(GDisplayLogCapsule, LogGeCharacterMovement, Warning, this, TEXT("[DynamicCapsule] %hs"), __FUNCTION__);
	
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
		const float ActualInitialVelocityZ = Velocity.Z;
		const float InitialVelocityZ = (ActualInitialVelocityZ > UE_KINDA_SMALL_NUMBER)  ? ActualInitialVelocityZ : JumpZVelocity;
		ExpectedJumpApexTime = FMath::Abs(InitialVelocityZ / theGravityZ);
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
		// If bRestoreCapsule is false, clear TargetMeshZOffset even if early returning
		if (!bRestoreCapsule)
		{
			TargetMeshZOffset.Reset();
		}
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
		// Always clear TargetMeshZOffset when bRestoreCapsule is false
		TargetMeshZOffset.Reset();
		UE_LOG_GATED(GDisplayLogCapsule, LogGeCharacterMovement, Log, this, TEXT("[DynamicCapsule] %hs Interrupted and cleared runtime data"), __FUNCTION__);
	}
}

FString UGeCharacterMovementComponent::GetDynamicCapsuleDebugInfo() const
{
	// Early return if no valid data
	if (!HasValidData())
	{
		return FString();
	}
	
	// Pre-allocate string to avoid multiple reallocations
	FString DebugInfo;
	DebugInfo.Reserve(1024);
	
	// Get runtime environment info
	const int32 FrameNumber = GFrameNumber % 1000;
	const ENetRole LocalRole = CharacterOwner->GetLocalRole();
	const ENetRole RemoteRole = CharacterOwner->GetRemoteRole();
	const FString NetModeStr = GetNetMode() == NM_Standalone ? TEXT("Standalone") :
	                           GetNetMode() == NM_Client ? TEXT("Client") :
	                           GetNetMode() == NM_DedicatedServer ? TEXT("DedicatedServer") :
	                           GetNetMode() == NM_ListenServer ? TEXT("ListenServer") : TEXT("Unknown");
	
	// Build debug info using Appendf for better performance
	DebugInfo.Appendf(TEXT("=== Dynamic Capsule Debug Info ===\n"));
	DebugInfo.Appendf(TEXT("Frame: %d | NetMode: %s | LocalRole: %s | RemoteRole: %s\n"), 
		FrameNumber, *NetModeStr, *UEnum::GetValueAsString(LocalRole), *UEnum::GetValueAsString(RemoteRole));
	DebugInfo.Appendf(TEXT("Enabled: %s | Active: %s | Pending Restore: %s\n"), 
		bEnableDynamicCapsule ? TEXT("Yes") : TEXT("No"),
		bIsDynamicCapsuleActive ? TEXT("Yes") : TEXT("No"),
		bPendingCapsuleRestore ? TEXT("Yes") : TEXT("No"));
	
	DebugInfo.Appendf(TEXT("\n--- Stage Info ---\n"));
	DebugInfo.Appendf(TEXT("Current: %s | Max Reached: %s | Server: %s\n"), 
		*UEnum::GetValueAsString(CurrentCapsuleStage),
		*UEnum::GetValueAsString(MaxReachedStage),
		*UEnum::GetValueAsString(ServerCapsuleStage));
	
	DebugInfo.Appendf(TEXT("\n--- Timing Info ---\n"));
	DebugInfo.Appendf(TEXT("Accumulated: %.4f | Expected Apex: %.4f | Actual Apex: %.4f\n"), 
		AccumulatedJumpTime, ExpectedJumpApexTime, ActualJumpApexTime);
	
	DebugInfo.Appendf(TEXT("\n--- Capsule Info ---\n"));
	UCapsuleComponent* Capsule = CharacterOwner->GetCapsuleComponent();
	if (Capsule != nullptr)
	{
		const float CurrentHalfHeight = Capsule->GetUnscaledCapsuleHalfHeight();
		const float DefaultHalfHeight = GetDefaultCapsuleHalfHeight();
		const float Ratio = DefaultHalfHeight > 0.f ? (CurrentHalfHeight / DefaultHalfHeight * 100.f) : 0.f;
		DebugInfo.Appendf(TEXT("Current: %.2f | Default: %.2f | Expected: %.2f | Ratio: %.2f%%\n"), 
			CurrentHalfHeight, DefaultHalfHeight, ExpectedCapsuleHalfHeight, Ratio);
	}
	
	DebugInfo.Appendf(TEXT("\n--- Mesh Offset Info ---\n"));
	if (TargetMeshZOffset.IsSet())
	{
		const float DefaultMeshZ = GetDefaultMeshZ();
		const float TargetZ = TargetMeshZOffset.GetValue();
		if (CharacterOwner->GetMesh())
		{
			const float CurrentMeshZ = CharacterOwner->GetMesh()->GetRelativeLocation().Z;
			DebugInfo.Appendf(TEXT("Target: %.2f | Default: %.2f | Current: %.2f | Delta: %.2f\n"), 
				TargetZ, DefaultMeshZ, CurrentMeshZ, TargetZ - CurrentMeshZ);
		}
		else
		{
			DebugInfo.Appendf(TEXT("Target: %.2f | Default: %.2f\n"), TargetZ, DefaultMeshZ);
		}
	}
	else
	{
		DebugInfo.Append(TEXT("No Target Mesh Offset\n"));
	}
	
	DebugInfo.Appendf(TEXT("\n--- Movement Info ---\n"));
	DebugInfo.Appendf(TEXT("Mode: %s | Custom: %d | Falling: %s | Root Motion: %s\n"), 
		*UEnum::GetValueAsString(MovementMode),
		CustomMovementMode,
		IsFalling() ? TEXT("Yes") : TEXT("No"),
		HasRootMotionSources() ? TEXT("Yes") : TEXT("No"));
	
	DebugInfo.Appendf(TEXT("\n--- Configuration ---\n"));
	DebugInfo.Appendf(TEXT("Stage1: %s"), bEnableStage1 ? TEXT("Yes") : TEXT("No"));
	if (bEnableStage1)
	{
		DebugInfo.Appendf(TEXT(" (T:%.2f R:%.2f O:%.2f)"), 
			Stage1Config.Threshold, Stage1Config.ShrinkRatio, Stage1Config.CapsuleOffset);
	}
	DebugInfo.Append(TEXT("\n"));
	DebugInfo.Appendf(TEXT("Stage2: %s"), bEnableStage2 ? TEXT("Yes") : TEXT("No"));
	if (bEnableStage2)
	{
		DebugInfo.Appendf(TEXT(" (T:%.2f R:%.2f O:%.2f)"), 
			Stage2Config.Threshold, Stage2Config.ShrinkRatio, Stage2Config.CapsuleOffset);
	}
	DebugInfo.Appendf(TEXT("\nInterp Speed: %.2f\n"), InterpMeshSpeed);
	
	DebugInfo.Append(TEXT("========================\n"));
	
	return DebugInfo;
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
	
	// If tag is in skip restore container, use clear data only mode (1.2)
	if (MovementModeTagsSkipRestore.HasTag(MovementTag))
	{
		return false;
	}
	
	// Default behavior: restore capsule (1.1)
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

